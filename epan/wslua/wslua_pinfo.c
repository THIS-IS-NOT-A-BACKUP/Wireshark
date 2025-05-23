/*
 * wslua_pinfo.c
 *
 * Wireshark's interface to the Lua Programming Language
 *
 * (c) 2006, Luis E. Garcia Ontanon <luis@ontanon.org>
 * (c) 2008, Balint Reczey <balint.reczey@ericsson.com>
 * (c) 2011, Stig Bjorlykke <stig@bjorlykke.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "config.h"

#include "wslua_pinfo_common.h"

#include <epan/wmem_scopes.h>
#include <epan/conversation.h>
#include <string.h>


/* WSLUA_MODULE Pinfo Obtaining Packet Information */


/*
 * Track pointers to wireshark's structures.
 * see comment on wslua_tvb.c
 */

static GPtrArray* outstanding_Pinfo;
static GPtrArray* outstanding_PrivateTable;

CLEAR_OUTSTANDING(Pinfo,expired, true)
CLEAR_OUTSTANDING(PrivateTable,expired, true)

Pinfo* push_Pinfo(lua_State* L, packet_info* ws_pinfo) {
    Pinfo pinfo = NULL;
    if (ws_pinfo) {
        pinfo = (Pinfo)g_malloc(sizeof(struct _wslua_pinfo));
        pinfo->ws_pinfo = ws_pinfo;
        pinfo->expired = false;
        g_ptr_array_add(outstanding_Pinfo,pinfo);
    }
    return pushPinfo(L,pinfo);
}

#define PUSH_PRIVATE_TABLE(L,c) {g_ptr_array_add(outstanding_PrivateTable,c);pushPrivateTable(L,c);}


WSLUA_CLASS_DEFINE(PrivateTable,FAIL_ON_NULL_OR_EXPIRED("PrivateTable"));
/* PrivateTable represents the pinfo->private_table. */

WSLUA_METAMETHOD PrivateTable__tostring(lua_State* L) {
    /* Gets debugging type information about the private table. */
    PrivateTable priv = toPrivateTable(L,1);
    GString *key_string;
    GList *keys, *key;

    if (!priv) return 0;

    key_string = g_string_new ("");
    keys = g_hash_table_get_keys (priv->table);
    key = g_list_first (keys);
    while (key) {
        key_string = g_string_append (key_string, (const char *)key->data);
        key = g_list_next (key);
        if (key) {
            key_string = g_string_append_c (key_string, ',');
        }
    }

    lua_pushstring(L,key_string->str);

    g_string_free (key_string, TRUE);
    g_list_free (keys);

    WSLUA_RETURN(1); /* A string with all keys in the table, mostly for debugging. */
}

static int PrivateTable__index(lua_State* L) {
    /* Gets the text of a specific entry. */
    PrivateTable priv = checkPrivateTable(L,1);
    const char* name = luaL_checkstring(L,2);
    const char* string;

    string = (const char *)(g_hash_table_lookup (priv->table, name));

    if (string) {
        lua_pushstring(L, string);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int PrivateTable__newindex(lua_State* L) {
    /* Sets the text of a specific entry. */
    PrivateTable priv = checkPrivateTable(L,1);
    const char* name = luaL_checkstring(L,2);
    const char* string = NULL;

    if (lua_isstring(L,3)) {
        /* This also catches numbers, which is converted to string */
        string = luaL_checkstring(L,3);
    } else if (lua_isboolean(L,3)) {
        /* We support boolean by setting a empty string if true and NULL if false */
        string = lua_toboolean(L,3) ? "" : NULL;
    } else if (!lua_isnil(L,3)) {
        luaL_error(L,"unsupported type: %s", lua_typename(L,3));
        return 0;
    }

    if (string) {
      g_hash_table_replace (priv->table, (void *) g_strdup(name), (void *) g_strdup(string));
    } else {
      g_hash_table_remove (priv->table, (const void *) name);
    }

    return 1;
}

/* Gets registered as metamethod automatically by WSLUA_REGISTER_CLASS/META */
static int PrivateTable__gc(lua_State* L) {
    PrivateTable priv = toPrivateTable(L,1);

    if (!priv) return 0;

    if (!priv->expired) {
        priv->expired = true;
    } else {
        if (priv->is_allocated) {
            g_hash_table_destroy (priv->table);
        }
        g_free(priv);
    }

    return 0;
}

WSLUA_META PrivateTable_meta[] = {
    WSLUA_CLASS_MTREG(PrivateTable,index),
    WSLUA_CLASS_MTREG(PrivateTable,newindex),
    WSLUA_CLASS_MTREG(PrivateTable,tostring),
    { NULL, NULL }
};

int PrivateTable_register(lua_State* L) {
    WSLUA_REGISTER_META(PrivateTable);
    return 0;
}


WSLUA_CLASS_DEFINE(Pinfo,FAIL_ON_NULL_OR_EXPIRED("Pinfo"));
/* Packet information. */

static int Pinfo__tostring(lua_State *L) { lua_pushstring(L,"a Pinfo"); return 1; }

#define PINFO_ADDRESS_GETTER(name) \
    WSLUA_ATTRIBUTE_GET(Pinfo,name, { \
      Address addr = g_new(address,1); \
      copy_address(addr, &(obj->ws_pinfo->name)); \
      pushAddress(L,addr); \
    })

/*
 * Addresses within the Pinfo structure are only valid for a single packet, so
 * allocate memory from the pinfo pool.
 */
#define PINFO_ADDRESS_SETTER(name) \
    WSLUA_ATTRIBUTE_SET(Pinfo,name, { \
      const address* from = checkAddress(L,-1); \
      copy_address_wmem(obj->ws_pinfo->pool, &(obj->ws_pinfo->name), from); \
    })

#define PINFO_NAMED_BOOLEAN_GETTER(name,member) \
    WSLUA_ATTRIBUTE_NAMED_BOOLEAN_GETTER(Pinfo,name,ws_pinfo->member)

#define PINFO_NAMED_BOOLEAN_SETTER(name,member) \
    WSLUA_ATTRIBUTE_NAMED_BOOLEAN_SETTER(Pinfo,name,ws_pinfo->member)

#define PINFO_INTEGER_GETTER(name) \
    WSLUA_ATTRIBUTE_NAMED_INTEGER_GETTER(Pinfo,name,ws_pinfo->name)

#define PINFO_NAMED_INTEGER_GETTER(name,member) \
    WSLUA_ATTRIBUTE_NAMED_INTEGER_GETTER(Pinfo,name,ws_pinfo->member)

#define PINFO_NUMBER_SETTER(name,cast) \
    WSLUA_ATTRIBUTE_NAMED_INTEGER_SETTER(Pinfo,name,ws_pinfo->name,cast)

#define PINFO_NAMED_INTEGER_SETTER(name,member,cast) \
    WSLUA_ATTRIBUTE_NAMED_INTEGER_SETTER(Pinfo,name,ws_pinfo->member,cast)

static double
lua_nstime_to_sec(const nstime_t *nstime)
{
    return (((double)nstime->secs) + (((double)nstime->nsecs) / 1000000000.0));
}

static double
lua_delta_prev_captured_sec(const Pinfo pinfo, const frame_data *fd)
{
    nstime_t del;

    /*
     * XXX - there's no way to report "there *is* no delta time,
     * because either 1) this frame has no time stamp, 2) the
     * previous frame doesn't exist, or 2) it exists but *it*
     * has no time stamp.
     */
    frame_delta_time_prev_captured(pinfo->ws_pinfo->epan, fd, &del);
    return lua_nstime_to_sec(&del);
}

static double
lua_delta_prev_displayed_sec(const Pinfo pinfo, const frame_data *fd)
{
    nstime_t del;

    /*
     * XXX - there's no way to report "there *is* no delta time,
     * because either 1) this frame has no time stamp, 2) the
     * previous frame doesn't exist, or 2) it exists but *it*
     * has no time stamp.
     */
    frame_delta_time_prev_displayed(pinfo->ws_pinfo->epan, fd, &del);
    return lua_nstime_to_sec(&del);
}


/* WSLUA_ATTRIBUTE Pinfo_visited RO Whether this packet has been already visited. */
PINFO_NAMED_BOOLEAN_GETTER(visited,fd->visited);

/* WSLUA_ATTRIBUTE Pinfo_number RO The number of this packet in the current file. */
PINFO_NAMED_INTEGER_GETTER(number,num);

/* WSLUA_ATTRIBUTE Pinfo_len  RO The length of the frame. */
PINFO_NAMED_INTEGER_GETTER(len,fd->pkt_len);

/* WSLUA_ATTRIBUTE Pinfo_caplen RO The captured length of the frame. */
PINFO_NAMED_INTEGER_GETTER(caplen,fd->cap_len);

/* WSLUA_ATTRIBUTE Pinfo_abs_ts RO When the packet was captured. */
WSLUA_ATTRIBUTE_BLOCK_NUMBER_GETTER(Pinfo,abs_ts,lua_nstime_to_sec(&obj->ws_pinfo->abs_ts));

/* WSLUA_ATTRIBUTE Pinfo_rel_ts RO Number of seconds passed since beginning of capture. */
WSLUA_ATTRIBUTE_BLOCK_NUMBER_GETTER(Pinfo,rel_ts,lua_nstime_to_sec(&obj->ws_pinfo->rel_ts));

/* WSLUA_ATTRIBUTE Pinfo_delta_ts RO Number of seconds passed since the last captured packet. */
WSLUA_ATTRIBUTE_BLOCK_NUMBER_GETTER(Pinfo,delta_ts,lua_delta_prev_captured_sec(obj, obj->ws_pinfo->fd));

/* WSLUA_ATTRIBUTE Pinfo_delta_dis_ts RO Number of seconds passed since the last displayed packet. */
WSLUA_ATTRIBUTE_BLOCK_NUMBER_GETTER(Pinfo,delta_dis_ts,lua_delta_prev_displayed_sec(obj, obj->ws_pinfo->fd));

/* WSLUA_ATTRIBUTE Pinfo_curr_proto RO Which Protocol are we dissecting. */
WSLUA_ATTRIBUTE_NAMED_STRING_GETTER(Pinfo,curr_proto,ws_pinfo->current_proto);

/* WSLUA_ATTRIBUTE Pinfo_can_desegment RW Set if this segment could be desegmented. */
PINFO_INTEGER_GETTER(can_desegment);
PINFO_NUMBER_SETTER(can_desegment,uint16_t);

/* WSLUA_ATTRIBUTE Pinfo_saved_can_desegment RO Value of can_desegment before the current dissector was called.
   Supplied so that proxy protocols like SOCKS can restore it to whatever the previous dissector (e.g. TCP) set it,
   so that the dissectors they call are desegmented via the previous dissector.
   @since 4.3.1 */
PINFO_INTEGER_GETTER(saved_can_desegment);

/* WSLUA_ATTRIBUTE Pinfo_desegment_len RW Estimated number of additional bytes required for completing the PDU. */
PINFO_INTEGER_GETTER(desegment_len);
PINFO_NUMBER_SETTER(desegment_len,uint32_t);

/* WSLUA_ATTRIBUTE Pinfo_desegment_offset RW Offset in the tvbuff at which the dissector will continue processing when next called. */
PINFO_INTEGER_GETTER(desegment_offset);
PINFO_NUMBER_SETTER(desegment_offset,int);

/* WSLUA_ATTRIBUTE Pinfo_fragmented RO If the protocol is only a fragment. */
PINFO_NAMED_BOOLEAN_GETTER(fragmented,fragmented);

/* WSLUA_ATTRIBUTE Pinfo_in_error_pkt RW If we're inside an error packet. */
PINFO_NAMED_BOOLEAN_GETTER(in_error_pkt,flags.in_error_pkt);
PINFO_NAMED_BOOLEAN_SETTER(in_error_pkt,flags.in_error_pkt);

/* WSLUA_ATTRIBUTE Pinfo_match_uint RO Matched uint for calling subdissector from table. */
PINFO_INTEGER_GETTER(match_uint);

/* WSLUA_ATTRIBUTE Pinfo_match_string RO Matched string for calling subdissector from table. */
WSLUA_ATTRIBUTE_NAMED_STRING_GETTER(Pinfo,match_string,ws_pinfo->match_string);

/* WSLUA_ATTRIBUTE Pinfo_port_type RW Type of Port of .src_port and .dst_port. */
PINFO_NAMED_INTEGER_GETTER(port_type,ptype);
PINFO_NAMED_INTEGER_SETTER(port_type,ptype,uint8_t);

/* WSLUA_ATTRIBUTE Pinfo_src_port RW Source Port of this Packet. */
PINFO_NAMED_INTEGER_GETTER(src_port,srcport);
PINFO_NAMED_INTEGER_SETTER(src_port,srcport,uint32_t);

/* WSLUA_ATTRIBUTE Pinfo_dst_port RW Destination Port of this Packet. */
PINFO_NAMED_INTEGER_GETTER(dst_port,destport);
PINFO_NAMED_INTEGER_SETTER(dst_port,destport,uint32_t);

/* WSLUA_ATTRIBUTE Pinfo_dl_src RW Data Link Source Address of this Packet. */
PINFO_ADDRESS_GETTER(dl_src);
PINFO_ADDRESS_SETTER(dl_src);

/* WSLUA_ATTRIBUTE Pinfo_dl_dst RW Data Link Destination Address of this Packet. */
PINFO_ADDRESS_GETTER(dl_dst);
PINFO_ADDRESS_SETTER(dl_dst);

/* WSLUA_ATTRIBUTE Pinfo_net_src RW Network Layer Source Address of this Packet. */
PINFO_ADDRESS_GETTER(net_src);
PINFO_ADDRESS_SETTER(net_src);

/* WSLUA_ATTRIBUTE Pinfo_net_dst RW Network Layer Destination Address of this Packet. */
PINFO_ADDRESS_GETTER(net_dst);
PINFO_ADDRESS_SETTER(net_dst);

/* WSLUA_ATTRIBUTE Pinfo_src RW Source Address of this Packet. */
PINFO_ADDRESS_GETTER(src);
PINFO_ADDRESS_SETTER(src);

/* WSLUA_ATTRIBUTE Pinfo_dst RW Destination Address of this Packet. */
PINFO_ADDRESS_GETTER(dst);
PINFO_ADDRESS_SETTER(dst);

/* WSLUA_ATTRIBUTE Pinfo_p2p_dir RW Direction of this Packet. (incoming / outgoing) */
PINFO_INTEGER_GETTER(p2p_dir);
PINFO_NUMBER_SETTER(p2p_dir,int);

/* WSLUA_ATTRIBUTE Pinfo_match RO Port/Data we are matching. */
static int Pinfo_get_match(lua_State *L) {
    Pinfo pinfo = checkPinfo(L,1);

    if (pinfo->ws_pinfo->match_string) {
        lua_pushstring(L,pinfo->ws_pinfo->match_string);
    } else {
        lua_pushinteger(L,(lua_Integer)(pinfo->ws_pinfo->match_uint));
    }

    return 1;
}

/* WSLUA_ATTRIBUTE Pinfo_columns RO Access to the packet list columns. */
/* WSLUA_ATTRIBUTE Pinfo_cols RO Access to the packet list columns (equivalent to pinfo.columns). */
static int Pinfo_get_columns(lua_State *L) {
    Columns cols = NULL;
    Pinfo pinfo = checkPinfo(L,1);
    const char* colname = luaL_optstring(L,2,NULL);

    cols = (Columns)g_malloc(sizeof(struct _wslua_cols));
    cols->cinfo = pinfo->ws_pinfo->cinfo;
    cols->expired = false;

    if (!colname) {
        Push_Columns(L,cols);
    } else {
        lua_settop(L,0);
        Push_Columns(L,cols);
        lua_pushstring(L,colname);
        return get_Columns_index(L);
    }
    return 1;
}

/* WSLUA_ATTRIBUTE Pinfo_private RO Access to the private table entries. */
static int Pinfo_get_private(lua_State *L) {
    PrivateTable priv = NULL;
    Pinfo pinfo = checkPinfo(L,1);
    const char* privname = luaL_optstring(L,2,NULL);
    bool is_allocated = false;

    if (!pinfo->ws_pinfo->private_table) {
        pinfo->ws_pinfo->private_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        is_allocated = true;
    }

    priv = (PrivateTable)g_malloc(sizeof(struct _wslua_private_table));
    priv->table = pinfo->ws_pinfo->private_table;
    priv->is_allocated = is_allocated;
    priv->expired = false;

    if (!privname) {
        PUSH_PRIVATE_TABLE(L,priv);
    } else {
        lua_settop(L,0);
        PUSH_PRIVATE_TABLE(L,priv);
        lua_pushstring(L,privname);
        return PrivateTable__index(L);
    }
    return 1;
}

/* WSLUA_ATTRIBUTE Pinfo_hi RW Higher Address of this Packet. */
static int Pinfo_get_hi(lua_State *L) {
    Pinfo pinfo = checkPinfo(L,1);
    Address addr;

    addr = (Address)g_malloc(sizeof(address));
    if (cmp_address(&(pinfo->ws_pinfo->src), &(pinfo->ws_pinfo->dst) ) >= 0) {
        copy_address(addr, &(pinfo->ws_pinfo->src));
    } else {
        copy_address(addr, &(pinfo->ws_pinfo->dst));
    }

    pushAddress(L,addr);
    return 1;
}

/* WSLUA_ATTRIBUTE Pinfo_lo RO Lower Address of this Packet. */
static int Pinfo_get_lo(lua_State *L) {
    Pinfo pinfo = checkPinfo(L,1);
    Address addr;

    addr = (Address)g_malloc(sizeof(address));
    if (cmp_address(&(pinfo->ws_pinfo->src), &(pinfo->ws_pinfo->dst) ) < 0) {
        copy_address(addr, &(pinfo->ws_pinfo->src));
    } else {
        copy_address(addr, &(pinfo->ws_pinfo->dst));
    }

    pushAddress(L,addr);
    return 1;
}

/* WSLUA_ATTRIBUTE Pinfo_conversation RW
   On read, returns a <<lua_class_Conversation,``Conversation``>> object (equivalent to ``Conversation.find_from_pinfo(pinfo, 0, True)``)

   On write, sets the <<lua_class_Dissector,``Dissector``>> for the current conversation (shortcut for ``pinfo.conversation.dissector = dissector``). Accepts either a <<lua_class_Dissector,``Dissector``>> object or a <<lua_class_Proto,``Proto``>> object with an assigned dissector */
static int Pinfo_set_conversation(lua_State *L) {
    Pinfo pinfo = checkPinfo(L,1);
    Proto proto = checkProto(L,2);
    conversation_t  *conversation;


    if (!proto->handle) {
        luaL_error(L,"Proto %s has no registered dissector", proto->name? proto->name:"<UNKNOWN>");
        return 0;
    }

    conversation = find_or_create_conversation(pinfo->ws_pinfo);
    conversation_set_dissector(conversation,proto->handle);

    return 0;
}

static int Pinfo_get_conversation(lua_State *L) {
    Pinfo pinfo = checkPinfo(L,1);
    Conversation conv = find_or_create_conversation(pinfo->ws_pinfo);
    pushConversation(L, conv);
    WSLUA_RETURN(1);
}


/* Gets registered as metamethod automatically by WSLUA_REGISTER_CLASS/META */
static int Pinfo__gc(lua_State* L) {
    Pinfo pinfo = toPinfo(L,1);

    if (!pinfo) return 0;

    if (!pinfo->expired)
        pinfo->expired = true;
    else
        g_free(pinfo);

    return 0;

}

/* This table is ultimately registered as a sub-table of the class' metatable,
 * and if __index/__newindex is invoked then it calls the appropriate function
 * from this table for getting/setting the members.
 */
WSLUA_ATTRIBUTES Pinfo_attributes[] = {
    WSLUA_ATTRIBUTE_ROREG(Pinfo,number),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,len),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,caplen),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,abs_ts),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,rel_ts),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,delta_ts),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,delta_dis_ts),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,visited),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,src),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,dst),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,lo),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,hi),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,dl_src),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,dl_dst),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,net_src),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,net_dst),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,port_type),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,src_port),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,dst_port),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,match),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,curr_proto),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,columns),
    { "cols", Pinfo_get_columns, NULL },
    WSLUA_ATTRIBUTE_RWREG(Pinfo,can_desegment),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,saved_can_desegment),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,desegment_len),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,desegment_offset),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,private),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,fragmented),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,in_error_pkt),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,match_uint),
    WSLUA_ATTRIBUTE_ROREG(Pinfo,match_string),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,conversation),
    WSLUA_ATTRIBUTE_RWREG(Pinfo,p2p_dir),
    { NULL, NULL, NULL }
};

WSLUA_META Pinfo_meta[] = {
    WSLUA_CLASS_MTREG(Pinfo,tostring),
    { NULL, NULL }
};

int Pinfo_register(lua_State* L) {
    WSLUA_REGISTER_META_WITH_ATTRS(Pinfo);
    if (outstanding_Pinfo != NULL) {
        g_ptr_array_unref(outstanding_Pinfo);
    }
    outstanding_Pinfo = g_ptr_array_new();
    if (outstanding_PrivateTable != NULL) {
        g_ptr_array_unref(outstanding_PrivateTable);
    }
    outstanding_PrivateTable = g_ptr_array_new();
    return 0;
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
