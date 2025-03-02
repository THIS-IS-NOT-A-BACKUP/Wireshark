/* packet-mndp.c
 * Routines for the disassembly of the Mikrotik Neighbor Discovery Protocol
 *
 * Copyright 2011 Joerg Mayer (see AUTHORS file)
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
  http://wiki.mikrotik.com/wiki/Manual:IP/Neighbor_discovery
  TODO:
  - Find out about first 4 bytes (are the first 2 simply part of the sequence number?)
  - Find out about additional TLVs
  - Find out about unpack values
 */

#include "config.h"

#include <epan/packet.h>
void proto_register_mndp(void);
void proto_reg_handoff_mndp(void);

static dissector_handle_t mndp_handle;

/* protocol handles */
static int proto_mndp;

/* ett handles */
static int ett_mndp;
static int ett_mndp_tlv_header;

/* hf elements */
/* tlv generic */
static int hf_mndp_tlv_type;
static int hf_mndp_tlv_length;
static int hf_mndp_tlv_data;
/* tunnel header */
static int hf_mndp_header_unknown;
static int hf_mndp_header_seqno;
/* tlvs */
static int hf_mndp_mac;
static int hf_mndp_softwareid;
static int hf_mndp_version;
static int hf_mndp_identity;
static int hf_mndp_uptime;
static int hf_mndp_platform;
static int hf_mndp_board;
static int hf_mndp_unpack;
static int hf_mndp_ipv6address;
static int hf_mndp_interfacename;
static int hf_mndp_ipv4address;

#define PROTO_SHORT_NAME "MNDP"
#define PROTO_LONG_NAME "Mikrotik Neighbor Discovery Protocol"

#define PORT_MNDP	5678 /* Not IANA registered */

/* ============= copy/paste/modify from value_string.[hc] ============== */
typedef struct _ext_value_string {
	uint32_t value;
	const char    *strptr;
	int* hf_element;
	int (*specialfunction)(tvbuff_t *, packet_info *, proto_tree *, uint32_t,
			       uint32_t, const struct _ext_value_string *);
	const struct _ext_value_string *evs;
} ext_value_string;


static const char*
match_strextval_idx(uint32_t val, const ext_value_string *vs, int *idx) {
	int i = 0;

	if(vs) {
		while (vs[i].strptr) {
			if (vs[i].value == val) {
				if (idx)
					*idx = i;
				return vs[i].strptr;
			}
			i++;
		}
	}

	if (idx)
		*idx = -1;
	return NULL;
}

static const char*
extval_to_str_idx(wmem_allocator_t *pool, uint32_t val, const ext_value_string *vs, int *idx, const char *fmt) {
	const char *ret;

	if (!fmt)
		fmt="Unknown";

	ret = match_strextval_idx(val, vs, idx);
	if (ret != NULL)
		return ret;

	return wmem_strdup_printf(pool, fmt, val);
}
/* ============= end copy/paste/modify  ============== */

/* Forward decls needed by mndp_tunnel_tlv_vals et al */
static int dissect_tlv(tvbuff_t *tvb, packet_info *pinfo, proto_tree *mndp_tree,
	uint32_t offset, uint32_t length, const ext_value_string *value_array);

static const ext_value_string mndp_body_tlv_vals[] = {
	{  1, "MAC-Address",	&hf_mndp_mac,		NULL, NULL },
	{  5, "Identity",	&hf_mndp_identity,	NULL, NULL },
	{  7, "Version",	&hf_mndp_version,	NULL, NULL },
	{  8, "Platform",	&hf_mndp_platform,	NULL, NULL },
	{ 10, "Uptime",		&hf_mndp_uptime,	NULL, (ext_value_string *)true },
	{ 11, "Software-ID",	&hf_mndp_softwareid,	NULL, NULL },
	{ 12, "Board",		&hf_mndp_board,		NULL, NULL },
	{ 14, "Unpack",		&hf_mndp_unpack,	NULL, NULL },
	{ 15, "IPv6-Address",	&hf_mndp_ipv6address,	NULL, NULL },
	{ 16, "Interface name", &hf_mndp_interfacename, NULL, NULL },
	{ 17, "IPv4-Address",	&hf_mndp_ipv4address,	NULL, NULL },

	{ 0, NULL, NULL, NULL, NULL }
};

static const value_string mndp_unpack_vals[] = {
	/* none|simple|uncompressed-headers|uncompressed-all */
	{ 1,	"None" },
	{ 0,	NULL }
};

static int
dissect_tlv(tvbuff_t *tvb, packet_info *pinfo, proto_tree *mndp_tree,
	uint32_t offset, uint32_t length _U_, const ext_value_string *value_array)
{
	uint32_t    tlv_type;
	uint32_t    tlv_length;
	proto_item *tlv_tree;
	proto_item *type_item;
	int         type_index;
	uint32_t    tlv_end;
	unsigned    encoding_info;

	tlv_type = tvb_get_ntohs(tvb, offset);
	tlv_length = tvb_get_ntohs(tvb, offset + 2);
	tlv_tree = proto_tree_add_subtree_format(mndp_tree, tvb,
		offset, tlv_length+4, ett_mndp_tlv_header, NULL,
		"T %d, L %d: %s",
		tlv_type,
		tlv_length,
		extval_to_str_idx(pinfo->pool, tlv_type, value_array, NULL, "Unknown"));

	type_item = proto_tree_add_item(tlv_tree, hf_mndp_tlv_type,
		tvb, offset, 2, ENC_BIG_ENDIAN);
	proto_item_append_text(type_item, " = %s",
		extval_to_str_idx(pinfo->pool, tlv_type, value_array,
			&type_index, "Unknown"));
	offset += 2;
	proto_tree_add_item(tlv_tree, hf_mndp_tlv_length,
		tvb, offset, 2, ENC_BIG_ENDIAN);
	offset += 2;

	if (tlv_length == 0)
		return offset;

	tlv_end = offset + tlv_length;

	/* Make hf_ handling independent of specialfunction */
	/* FIXME: Properly handle encoding info */
	if ( type_index != -1
		 && !value_array[type_index].specialfunction
		 && value_array[type_index].evs != NULL
	) {
		encoding_info = value_array[type_index].evs ? true : false;
	} else {
		encoding_info = false;
	}
	if ( type_index != -1 && value_array[type_index].hf_element) {
		proto_tree_add_item(tlv_tree,
			*(value_array[type_index].hf_element),
			tvb, offset, tlv_length, encoding_info);
	} else {
		proto_tree_add_item(tlv_tree, hf_mndp_tlv_data,
			tvb, offset, tlv_length, ENC_NA);
	}
	if ( type_index != -1 && value_array[type_index].specialfunction ) {
		uint32_t newoffset;

		while (offset < tlv_end) {
			newoffset = value_array[type_index].specialfunction (
				tvb, pinfo, tlv_tree, offset, tlv_length,
				value_array[type_index].evs);
			DISSECTOR_ASSERT(newoffset > offset);
			offset = newoffset;
		}
	}
	return tlv_end;
}

static int
dissect_mndp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	proto_item *ti;
	proto_tree *mndp_tree;
	uint32_t    offset = 0;
	uint32_t    packet_length;

	col_set_str(pinfo->cinfo, COL_PROTOCOL, PROTO_SHORT_NAME);

	packet_length = tvb_reported_length(tvb);

	/* Header dissection */
	ti = proto_tree_add_item(tree, proto_mndp, tvb, offset, -1,
				 ENC_NA);
	mndp_tree = proto_item_add_subtree(ti, ett_mndp);

	proto_tree_add_item(mndp_tree, hf_mndp_header_unknown, tvb, offset, 2,
			    ENC_NA);
	offset += 2;
	proto_tree_add_item(mndp_tree, hf_mndp_header_seqno, tvb, offset, 2,
			    ENC_BIG_ENDIAN);
	offset += 2;

	while (offset < packet_length) {
		offset = dissect_tlv(tvb, pinfo, mndp_tree,
				     offset, 0, mndp_body_tlv_vals);
	}

	return offset;
}

static bool
test_mndp(tvbuff_t *tvb)
{
	/* Observed captures of MNDP always seem to have port 5678 as both
	 * the source and destination port, and have a broadcast destination IP
	 * and destination MAC address (if we have those layers.)
	 * The TLVs are also transmitted in increasing type order.
	 * TLV type 1 (MAC-Address) appears to be mandatory (and thus first),
	 * and always has length 6.
	 * We could also step through all the TLVs to see if the types and
	 * lengths are reasonable.
	 * Any of these could be used to strengthen the heuristic further.
	 */
	int offset = 0;
	int type, length;
	/* Minimum of 8 bytes, 4 Bytes header + 1 TLV-header */
	if ( tvb_captured_length(tvb) < 8) {
		return false;
	}
	offset += 4;
	type = tvb_get_uint16(tvb, offset, ENC_BIG_ENDIAN);
	if (type != 1) {
		return false;
	}
	offset += 2;
	length = tvb_get_uint16(tvb, offset, ENC_BIG_ENDIAN);
	if (length != 6) {
		return false;
	}
	offset += 2;
	/* Length does *not* include the type and length fields. */
	if (tvb_reported_length_remaining(tvb, offset) < length) {
		return false;
	}
	offset += length;
	/* If there's more data left, it should be another TLV. */
	if (tvb_reported_length_remaining(tvb, offset) > 0 &&
	    tvb_reported_length_remaining(tvb, offset) < 4) {
		return false;
	}
	return true;
}

static bool
dissect_mndp_heur(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
	if ( !test_mndp(tvb) ) {
		return false;
	}
	dissect_mndp(tvb, pinfo, tree);
	return true;
}

static int
dissect_mndp_static(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
	if ( !test_mndp(tvb) ) {
		return 0;
	}
	return dissect_mndp(tvb, pinfo, tree);
}

void
proto_register_mndp(void)
{
	static hf_register_info hf[] = {

	/* TLV fields */
		{ &hf_mndp_tlv_type,
		{ "TlvType",	"mndp.tlv.type", FT_UINT16, BASE_DEC, NULL,
			0x0, NULL, HFILL }},

		{ &hf_mndp_tlv_length,
		{ "TlvLength",	"mndp.tlv.length", FT_UINT16, BASE_DEC, NULL,
			0x0, NULL, HFILL }},

		{ &hf_mndp_tlv_data,
		{ "TlvData",   "mndp.tlv.data", FT_BYTES, BASE_NONE, NULL,
			0x0, NULL, HFILL }},

	/* MNDP tunnel header */
		{ &hf_mndp_header_unknown,
		{ "Header Unknown",	"mndp.header.unknown", FT_BYTES, BASE_NONE, NULL,
			0x0, NULL, HFILL }},

		{ &hf_mndp_header_seqno,
		{ "SeqNo",	"mndp.header.seqno", FT_UINT16, BASE_DEC, NULL,
			0x0, NULL, HFILL }},

	/* MNDP tunnel data */
		{ &hf_mndp_mac,
		{ "MAC-Address",	"mndp.mac", FT_ETHER, BASE_NONE, NULL,
			0x0, NULL, HFILL }},

		{ &hf_mndp_softwareid,
		{ "Software-ID", "mndp.softwareid", FT_STRING, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_version,
		{ "Version", "mndp.version", FT_STRING, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_identity,
		{ "Identity", "mndp.identity", FT_STRING, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_uptime,
		{ "Uptime", "mndp.uptime", FT_RELATIVE_TIME, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_platform,
		{ "Platform", "mndp.platform", FT_STRING, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_board,
		{ "Board", "mndp.board", FT_STRING, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_unpack,
		{ "Unpack", "mndp.unpack", FT_UINT8, BASE_DEC, VALS(mndp_unpack_vals),
				0x0, NULL, HFILL }},

		{ &hf_mndp_ipv6address,
		{ "IPv6-Address", "mndp.ipv6address", FT_IPv6, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_interfacename,
		{ "Interface name", "mndp.interfacename", FT_STRING, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

		{ &hf_mndp_ipv4address,
		{ "IPv4-Address", "mndp.ipv4address", FT_IPv4, BASE_NONE, NULL,
				0x0, NULL, HFILL }},

	};
	static int *ett[] = {
		&ett_mndp,
		&ett_mndp_tlv_header,
	};

	proto_mndp = proto_register_protocol(PROTO_LONG_NAME, PROTO_SHORT_NAME, "mndp");
	proto_register_field_array(proto_mndp, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	mndp_handle = register_dissector("mndp", dissect_mndp_static, proto_mndp);
}

void
proto_reg_handoff_mndp(void)
{
	dissector_add_uint_with_preference("udp.port", PORT_MNDP, mndp_handle);
	heur_dissector_add("udp", dissect_mndp_heur, "MNDP over UDP", "mndp_udp", proto_mndp, HEURISTIC_DISABLE);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
