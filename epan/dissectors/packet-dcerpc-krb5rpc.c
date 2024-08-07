/* packet-dcerpc-krb5rpc.c
 *
 * Routines for dcerpc DCE/KRB5 interface
 * Copyright 2002, Jaime Fournier <Jaime.Fournier@hush.com>
 * This information is based off the released idl files from opengroup.
 * ftp://ftp.opengroup.org/pub/dce122/dce/src/security.tar.gz security/idl/krb5rpc.idl
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"


#include <epan/packet.h>
#include <epan/asn1.h>
#include "packet-dcerpc.h"
#include "packet-kerberos.h"

void proto_register_krb5rpc (void);
void proto_reg_handoff_krb5rpc (void);

static int proto_krb5rpc;

static int ett_krb5rpc;


static e_guid_t uuid_krb5rpc =
  { 0x8f73de50, 0x768c, 0x11ca, {0xbf, 0xfc, 0x08, 0x00, 0x1e, 0x03, 0x94,
                                 0x31}
};
static uint16_t ver_krb5rpc = 1;
static int hf_krb5rpc_opnum;
static int hf_krb5rpc_sendto_kdc_rqst_keysize;
static int hf_krb5rpc_sendto_kdc_rqst_spare1;
static int hf_krb5rpc_sendto_kdc_resp_len;
static int hf_krb5rpc_sendto_kdc_resp_max;
static int hf_krb5rpc_sendto_kdc_resp_spare1;
static int hf_krb5rpc_sendto_kdc_resp_keysize;
/* static int hf_krb5rpc_sendto_kdc_resp_st; */
static int hf_krb5rpc_krb5;
static int ett_krb5rpc_krb5;

static int
krb5rpc_dissect_sendto_kdc_rqst (tvbuff_t * tvb, int offset,
                                 packet_info * pinfo, proto_tree * tree,
                                 dcerpc_info *di, uint8_t *drep)
{
  uint32_t keysize, spare1, remain;
  proto_item *item;
  tvbuff_t *krb5_tvb;
  proto_tree *subtree;


  /*
   *        [in]        handle_t        h,
   *        [in]        unsigned32      len,
   *        [in, size_is(len)]
   *        byte            message[],
   *        [in]        unsigned32      out_buf_len,
   */

  offset =
    dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                       hf_krb5rpc_sendto_kdc_rqst_keysize, &keysize);
  offset =
    dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                       hf_krb5rpc_sendto_kdc_rqst_spare1, &spare1);
  item = proto_tree_add_item (tree, hf_krb5rpc_krb5, tvb, offset, -1, ENC_NA);
  subtree = proto_item_add_subtree (item, ett_krb5rpc_krb5);

  remain = tvb_captured_length_remaining(tvb, offset);
  krb5_tvb = tvb_new_subset_length(tvb, offset, remain);
  offset = dissect_kerberos_main (krb5_tvb, pinfo, subtree, true, NULL);


  return offset;
}


static int
krb5rpc_dissect_sendto_kdc_resp (tvbuff_t * tvb, int offset,
                                 packet_info * pinfo, proto_tree * tree,
                                 dcerpc_info *di, uint8_t *drep)
{
  uint32_t resp_len, maxsize, spare1, keysize, remain;
  proto_item *item;
  tvbuff_t *krb5_tvb;
  proto_tree *subtree;


  /*
   *
   *        [out]       unsigned32      *resp_len,
   *        [out, length_is(*resp_len), size_is(out_buf_len)]
   *        byte            out_buf[],
   *        [out]       error_status_t  *st unsigned long
   *
   */

  offset =
    dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                       hf_krb5rpc_sendto_kdc_resp_len, &resp_len);
  offset =
    dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                       hf_krb5rpc_sendto_kdc_resp_max, &maxsize);
  offset =
    dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                       hf_krb5rpc_sendto_kdc_resp_spare1, &spare1);
  offset =
    dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                       hf_krb5rpc_sendto_kdc_resp_keysize, &keysize);


  item = proto_tree_add_item (tree, hf_krb5rpc_krb5, tvb, offset, -1, ENC_NA);
  subtree = proto_item_add_subtree (item, ett_krb5rpc_krb5);
  remain = tvb_captured_length_remaining(tvb, offset);
  krb5_tvb = tvb_new_subset_length(tvb, offset, remain);

  offset = dissect_kerberos_main (krb5_tvb, pinfo, subtree, true, NULL);
  offset += 16; /* no idea what this is, probably just extended encrypted text. */

  return offset;
}


static const dcerpc_sub_dissector krb5rpc_dissectors[] = {
  {0, "rsec_krb5rpc_sendto_kdc", krb5rpc_dissect_sendto_kdc_rqst,
   krb5rpc_dissect_sendto_kdc_resp},
  {0, NULL, NULL, NULL},
};


void
proto_register_krb5rpc (void)
{
  static hf_register_info hf[] = {
    {&hf_krb5rpc_opnum,
     {"Opnum", "krb5rpc.opnum", FT_UINT16, BASE_DEC, NULL, 0x0,
      NULL, HFILL }},
    {&hf_krb5rpc_sendto_kdc_rqst_keysize,
     {"Request keysize",
      "krb5rpc.sendto_kdc_rqst_keysize", FT_UINT32, BASE_DEC, NULL, 0x0,
      NULL, HFILL}},
    {&hf_krb5rpc_sendto_kdc_rqst_spare1,
     {"Request spare1",
      "krb5rpc.sendto_kdc_rqst_spare1", FT_UINT32, BASE_DEC, NULL, 0x0, NULL,
      HFILL}},
    {&hf_krb5rpc_sendto_kdc_resp_len,
     {"Response length", "krb5rpc.sendto_kdc_resp_len",
      FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL}},
    {&hf_krb5rpc_sendto_kdc_resp_max,
     {"Response max", "krb5rpc.sendto_kdc_resp_max",
      FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL}},
    {&hf_krb5rpc_sendto_kdc_resp_spare1,
     {"Response spare1",
      "krb5rpc.sendto_kdc_resp_spare1", FT_UINT32, BASE_DEC, NULL, 0x0, NULL,
      HFILL}},
    {&hf_krb5rpc_sendto_kdc_resp_keysize,
     {"Response key size",
      "krb5rpc.sendto_kdc_resp_keysize", FT_UINT32, BASE_DEC, NULL, 0x0,
      NULL, HFILL}},
#if 0
    {&hf_krb5rpc_sendto_kdc_resp_st,
     {"Response st", "krb5rpc.sendto_kdc_resp_st",
      FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL}},
#endif
    {&hf_krb5rpc_krb5,
     {"krb5", "krb5rpc.krb5", FT_BYTES, BASE_NONE, NULL, 0x0,
      "krb5 blob", HFILL}},

  };

  static int *ett[] = {
    &ett_krb5rpc,
    &ett_krb5rpc_krb5,
  };
  proto_krb5rpc =
    proto_register_protocol ("DCE/RPC Kerberos V", "KRB5RPC", "krb5rpc");
  proto_register_field_array (proto_krb5rpc, hf, array_length (hf));
  proto_register_subtree_array (ett, array_length (ett));
}

void
proto_reg_handoff_krb5rpc (void)
{
  /* Register the protocol as dcerpc */
  dcerpc_init_uuid (proto_krb5rpc, ett_krb5rpc, &uuid_krb5rpc, ver_krb5rpc,
                    krb5rpc_dissectors, hf_krb5rpc_opnum);
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local Variables:
 * c-basic-offset: 2
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=2 tabstop=8 expandtab:
 * :indentSize=2:tabSize=8:noTabs=true:
 */
