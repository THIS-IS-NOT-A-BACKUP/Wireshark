/*XXX
  There is a bug in asn2wrs that it can not yet handle tagged assignments such
  as EXTERNAL  ::=  [UNIVERSAL 8] IMPLICIT SEQUENCE {

  This bug is workedaround by some .cnf magic but this should be cleaned up
  once asn2wrs learns how to deal with tagged assignments
*/

/* packet-acse.c
 * Routines for ACSE packet dissection
 *   Ronnie Sahlberg 2005
 * dissect_acse() based original handwritten dissector by Sid
 *   Yuriy Sidelnikov <YSidelnikov@hotmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
#include <epan/exceptions.h>
#include <epan/expert.h>
#include <epan/oids.h>
#include <epan/asn1.h>
#include <wsutil/array.h>

#include "packet-ber.h"
#include "packet-acse.h"
#include "packet-ses.h"
#include "packet-pres.h"
#include "packet-x509if.h"

#define PNAME  "ISO 8650-1 OSI Association Control Service"
#define PSNAME "ACSE"
#define PFNAME "acse"

#define CLPNAME  "ISO 10035-1 OSI Connectionless Association Control Service"
#define CLPSNAME "CLACSE"
#define CLPFNAME "clacse"

#define ACSE_APDU_OID "2.2.1.0.1"

void proto_register_acse(void);
void proto_reg_handoff_acse(void);

/* Initialize the protocol and registered fields */
int proto_acse;
int proto_clacse;



#include "packet-acse-hf.c"
static int hf_acse_user_data;

/* Initialize the subtree pointers */
static int ett_acse;
#include "packet-acse-ett.c"

static expert_field ei_acse_dissector_not_available;
static expert_field ei_acse_malformed;
static expert_field ei_acse_invalid_oid;

static dissector_handle_t acse_handle;

/* indirect_reference, used to pick up the signalling so we know what
   kind of data is transferred in SES_DATA_TRANSFER_PDUs */
static uint32_t indir_ref=0;

#if NOT_NEEDED
/* to keep track of presentation context identifiers and protocol-oids */
typedef struct _acse_ctx_oid_t {
	/* XXX here we should keep track of ADDRESS/PORT as well */
	uint32_t ctx_id;
	char *oid;
} acse_ctx_oid_t;
static wmem_map_t *acse_ctx_oid_table;

static unsigned
acse_ctx_oid_hash(const void *k)
{
	acse_ctx_oid_t *aco=(acse_ctx_oid_t *)k;
	return aco->ctx_id;
}
/* XXX this one should be made ADDRESS/PORT aware */
static int
acse_ctx_oid_equal(const void *k1, const void *k2)
{
	acse_ctx_oid_t *aco1=(acse_ctx_oid_t *)k1;
	acse_ctx_oid_t *aco2=(acse_ctx_oid_t *)k2;
	return aco1->ctx_id==aco2->ctx_id;
}

static void
register_ctx_id_and_oid(packet_info *pinfo _U_, uint32_t idx, char *oid)
{
	acse_ctx_oid_t *aco, *tmpaco;
	aco=wmem_new(wmem_file_scope(), acse_ctx_oid_t);
	aco->ctx_id=idx;
	aco->oid=wmem_strdup(wmem_file_scope(), oid);

	/* if this ctx already exists, remove the old one first */
	tmpaco=(acse_ctx_oid_t *)wmem_map_lookup(acse_ctx_oid_table, aco);
	if (tmpaco) {
		wmem_map_remove(acse_ctx_oid_table, tmpaco);
	}
	wmem_map_insert(acse_ctx_oid_table, aco, aco);
}
static char *
find_oid_by_ctx_id(packet_info *pinfo _U_, uint32_t idx)
{
	acse_ctx_oid_t aco, *tmpaco;
	aco.ctx_id=idx;
	tmpaco=(acse_ctx_oid_t *)wmem_map_lookup(acse_ctx_oid_table, &aco);
	if (tmpaco) {
		return tmpaco->oid;
	}
	return NULL;
}

# endif /* NOT_NEEDED */

#include "packet-acse-fn.c"


/*
* Dissect ACSE PDUs inside a PPDU.
*/
static int
dissect_acse(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, void* data)
{
	int offset = 0;
	proto_item *item;
	proto_tree *tree;
	char *oid;
	struct SESSION_DATA_STRUCTURE* session;
	asn1_ctx_t asn1_ctx;
	asn1_ctx_init(&asn1_ctx, ASN1_ENC_BER, true, pinfo);

	/* do we have spdu type from the session dissector?  */
	if (data == NULL) {
		return 0;
	}

	/* first, try to check length   */
	/* do we have at least 2 bytes  */
	if (!tvb_bytes_exist(tvb, 0, 2)) {
		proto_tree_add_item(parent_tree, hf_acse_user_data, tvb, offset,
			tvb_reported_length_remaining(tvb,offset), ENC_NA);
		return 0;  /* no, it isn't a ACSE PDU */
	}

	session = ( (struct SESSION_DATA_STRUCTURE*)data);
	if (session->spdu_type == 0) {
		if (parent_tree) {
			REPORT_DISSECTOR_BUG("Wrong spdu type %x from session dissector.",session->spdu_type);
			return 0;
		}
	}

	asn1_ctx.private_data = session;
	/* save parent_tree so subdissectors can create new top nodes */
	asn1_ctx.subtree.top_tree = parent_tree;

	/*  ACSE has only AARQ,AARE,RLRQ,RLRE,ABRT type of pdu */
	/*  reject everything else                              */
	/*  data pdu is not ACSE pdu and has to go directly to app dissector */
	switch (session->spdu_type) {
	case SES_CONNECTION_REQUEST:		/*   AARQ   */
	case SES_CONNECTION_ACCEPT:		/*   AARE   */
	case SES_REFUSE:			/*   RLRE   */
	case SES_DISCONNECT:			/*   RLRQ   */
	case SES_FINISH:			/*   RLRE   */
	case SES_ABORT:				/*   ABRT   */
	case CLSES_UNIT_DATA:		/* AARQ Connectionless session */
		break;
	case SES_DATA_TRANSFER:
		oid=find_oid_by_pres_ctx_id(pinfo, indir_ref);
		if (oid) {
			if (strcmp(oid, ACSE_APDU_OID) == 0) {
				proto_tree_add_expert_format(parent_tree, pinfo, &ei_acse_invalid_oid, tvb, offset, -1,
				    "Invalid OID: %s", ACSE_APDU_OID);
			}
		 else {
			call_ber_oid_callback(oid, tvb, offset, pinfo, parent_tree, NULL);
		 }
		} else {
			proto_tree_add_expert(parent_tree, pinfo, &ei_acse_dissector_not_available,
									tvb, offset, -1);
		}
		return 0;
	default:
		return 0;
	}

	if (session->spdu_type == CLSES_UNIT_DATA) {
		/* create display subtree for the connectionless protocol */
		item = proto_tree_add_item(parent_tree, proto_clacse, tvb, 0, -1, ENC_NA);
		tree = proto_item_add_subtree(item, ett_acse);

		col_set_str(pinfo->cinfo, COL_PROTOCOL, "CL-ACSE");
		col_clear(pinfo->cinfo, COL_INFO);
	} else {
		/* create display subtree for the protocol */
		item = proto_tree_add_item(parent_tree, proto_acse, tvb, 0, -1, ENC_NA);
		tree = proto_item_add_subtree(item, ett_acse);

		col_set_str(pinfo->cinfo, COL_PROTOCOL, "ACSE");
		col_clear(pinfo->cinfo, COL_INFO);
	}

	/*  we can't make any additional checking here   */
	/*  postpone it before dissector will have more information */
	while (tvb_reported_length_remaining(tvb, offset) > 0) {
		int old_offset=offset;
		offset = dissect_acse_ACSE_apdu(false, tvb, offset, &asn1_ctx, tree, -1);
		if (offset == old_offset) {
			proto_tree_add_expert(tree, pinfo, &ei_acse_malformed, tvb, offset, -1);
			break;
		}
	}

	return tvb_captured_length(tvb);
}

/*--- proto_register_acse ----------------------------------------------*/
void proto_register_acse(void) {

  /* List of fields */
  static hf_register_info hf[] = {
    { &hf_acse_user_data,
      { "User data", "acse.user_data",
        FT_BYTES, BASE_NONE, NULL, 0,
        NULL, HFILL }},
#include "packet-acse-hfarr.c"
  };

  /* List of subtrees */
  static int *ett[] = {
    &ett_acse,
#include "packet-acse-ettarr.c"
  };

  static ei_register_info ei[] = {
     { &ei_acse_dissector_not_available, { "acse.dissector_not_available", PI_UNDECODED, PI_WARN, "Dissector is not available", EXPFILL }},
     { &ei_acse_malformed, { "acse.malformed", PI_MALFORMED, PI_ERROR, "Malformed packet", EXPFILL }},
     { &ei_acse_invalid_oid, { "acse.invalid_oid", PI_UNDECODED, PI_WARN, "Invalid OID", EXPFILL }},
  };

  expert_module_t* expert_acse;

  /* Register protocol */
  proto_acse = proto_register_protocol(PNAME, PSNAME, PFNAME);
  acse_handle = register_dissector("acse", dissect_acse, proto_acse);

  /* Register connectionless protocol */
  proto_clacse = proto_register_protocol(CLPNAME, CLPSNAME, CLPFNAME);


  /* Register fields and subtrees */
  proto_register_field_array(proto_acse, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));
  expert_acse = expert_register_protocol(proto_acse);
  expert_register_field_array(expert_acse, ei, array_length(ei));

#if NOT_NEEDED
  acse_ctx_oid_table = wmem_map_new_autoreset(wmem_epan_scope(), wmem_file_scope(), acse_ctx_oid_hash,
			acse_ctx_oid_equal);
#endif
}


/*--- proto_reg_handoff_acse -------------------------------------------*/
void proto_reg_handoff_acse(void) {
/*#include "packet-acse-dis-tab.c"*/
	oid_add_from_string("id-aCSE","2.2.3.1.1");
	register_ber_oid_dissector_handle(ACSE_APDU_OID, acse_handle, proto_acse, "id-as-acse");


}

