/* packet-mgcp.c
 * Routines for mgcp packet disassembly
 * RFC 2705
 * RFC 3435 (obsoletes 2705): Media Gateway Control Protocol (MGCP) Version 1.0
 * RFC 3660: Basic MGCP Packages
 * RFC 3661: MGCP Return Code Usage
 * NCS 1.0: PacketCable Network-Based Call Signaling Protocol Specification,
 *          PKT-SP-EC-MGCP-I09-040113, January 13, 2004, Cable Television
 *          Laboratories, Inc., http://www.PacketCable.com/
 * NCS 1.5: PKT-SP-NCS1.5-I04-120412, April 12, 2012 Cable Television
 *          Laboratories, Inc., http://www.PacketCable.com/
 * www.iana.org/assignments/mgcp-localconnectionoptions
 *
 * Copyright (c) 2000 by Ed Warnicke <hagbard@physics.rutgers.edu>
 * Copyright (c) 2004 by Thomas Anders <thomas.anders [AT] blue-cable.de>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1999 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define WS_LOG_DOMAIN "packet-mgcp"

#include "config.h"
#include <wireshark.h>

#include <stdlib.h>

#include <epan/packet.h>
#include <epan/exceptions.h>
#include <epan/prefs.h>
#include <epan/conversation.h>
#include <epan/tap.h>
#include <epan/strutil.h>
#include <epan/rtd_table.h>
#include <epan/expert.h>
#include "packet-media-type.h"
#include "packet-mgcp.h"
#include "packet-sdp.h"

#include <wsutil/strtoi.h>

#define TCP_PORT_MGCP_GATEWAY 2427
#define UDP_PORT_MGCP_GATEWAY 2427
#define TCP_PORT_MGCP_CALLAGENT 2727
#define UDP_PORT_MGCP_CALLAGENT 2727


/* Define the mgcp proto */
static int proto_mgcp;

/* Define many many headers for mgcp */
static int hf_mgcp_req;
static int hf_mgcp_req_verb;
static int hf_mgcp_req_endpoint;
static int hf_mgcp_req_frame;
static int hf_mgcp_rsp;
static int hf_mgcp_rsp_frame;
static int hf_mgcp_time;
static int hf_mgcp_transid;
static int hf_mgcp_version;
static int hf_mgcp_rsp_rspcode;
static int hf_mgcp_rsp_rspstring;
static int hf_mgcp_params;
static int hf_mgcp_param_rspack;
static int hf_mgcp_param_bearerinfo;
static int hf_mgcp_param_callid;
static int hf_mgcp_param_connectionid;
static int hf_mgcp_param_secondconnectionid;
static int hf_mgcp_param_notifiedentity;
static int hf_mgcp_param_requestid;
static int hf_mgcp_param_localconnoptions;
static int hf_mgcp_param_localconnoptions_p;
static int hf_mgcp_param_localconnoptions_a;
static int hf_mgcp_param_localconnoptions_s;
static int hf_mgcp_param_localconnoptions_e;
static int hf_mgcp_param_localconnoptions_scrtp;
static int hf_mgcp_param_localconnoptions_scrtcp;
static int hf_mgcp_param_localconnoptions_b;
static int hf_mgcp_param_localconnoptions_esccd;
static int hf_mgcp_param_localconnoptions_escci;
static int hf_mgcp_param_localconnoptions_dqgi;
static int hf_mgcp_param_localconnoptions_dqrd;
static int hf_mgcp_param_localconnoptions_dqri;
static int hf_mgcp_param_localconnoptions_dqrr;
static int hf_mgcp_param_localconnoptions_k;
static int hf_mgcp_param_localconnoptions_gc;
static int hf_mgcp_param_localconnoptions_fmtp;
static int hf_mgcp_param_localconnoptions_nt;
static int hf_mgcp_param_localconnoptions_ofmtp;
static int hf_mgcp_param_localconnoptions_r;
static int hf_mgcp_param_localconnoptions_t;
static int hf_mgcp_param_localconnoptions_rcnf;
static int hf_mgcp_param_localconnoptions_rdir;
static int hf_mgcp_param_localconnoptions_rsh;
static int hf_mgcp_param_localconnoptions_mp;
static int hf_mgcp_param_localconnoptions_fxr;
static int hf_mgcp_param_localvoicemetrics;
static int hf_mgcp_param_remotevoicemetrics;
static int hf_mgcp_param_voicemetrics_nlr;
static int hf_mgcp_param_voicemetrics_jdr;
static int hf_mgcp_param_voicemetrics_bld;
static int hf_mgcp_param_voicemetrics_gld;
static int hf_mgcp_param_voicemetrics_bd;
static int hf_mgcp_param_voicemetrics_gd;
static int hf_mgcp_param_voicemetrics_rtd;
static int hf_mgcp_param_voicemetrics_esd;
static int hf_mgcp_param_voicemetrics_sl;
static int hf_mgcp_param_voicemetrics_nl;
static int hf_mgcp_param_voicemetrics_rerl;
static int hf_mgcp_param_voicemetrics_gmn;
static int hf_mgcp_param_voicemetrics_nsr;
static int hf_mgcp_param_voicemetrics_xsr;
static int hf_mgcp_param_voicemetrics_mlq;
static int hf_mgcp_param_voicemetrics_mcq;
static int hf_mgcp_param_voicemetrics_plc;
static int hf_mgcp_param_voicemetrics_jba;
static int hf_mgcp_param_voicemetrics_jbr;
static int hf_mgcp_param_voicemetrics_jbn;
static int hf_mgcp_param_voicemetrics_jbm;
static int hf_mgcp_param_voicemetrics_jbs;
static int hf_mgcp_param_voicemetrics_iaj;
static int hf_mgcp_param_connectionmode;
static int hf_mgcp_param_reqevents;
static int hf_mgcp_param_restartmethod;
static int hf_mgcp_param_restartdelay;
static int hf_mgcp_param_signalreq;
static int hf_mgcp_param_digitmap;
static int hf_mgcp_param_observedevent;
static int hf_mgcp_param_connectionparam;
static int hf_mgcp_param_connectionparam_ps;
static int hf_mgcp_param_connectionparam_os;
static int hf_mgcp_param_connectionparam_pr;
static int hf_mgcp_param_connectionparam_or;
static int hf_mgcp_param_connectionparam_pl;
static int hf_mgcp_param_connectionparam_ji;
static int hf_mgcp_param_connectionparam_la;
static int hf_mgcp_param_connectionparam_pcrps;
static int hf_mgcp_param_connectionparam_pcros;
static int hf_mgcp_param_connectionparam_pcrpl;
static int hf_mgcp_param_connectionparam_pcrji;
static int hf_mgcp_param_connectionparam_x;
static int hf_mgcp_param_reasoncode;
static int hf_mgcp_param_eventstates;
static int hf_mgcp_param_specificendpoint;
static int hf_mgcp_param_secondendpointid;
static int hf_mgcp_param_reqinfo;
static int hf_mgcp_param_quarantinehandling;
static int hf_mgcp_param_detectedevents;
static int hf_mgcp_param_capabilities;
static int hf_mgcp_param_maxmgcpdatagram;
static int hf_mgcp_param_packagelist;
static int hf_mgcp_param_extension;
static int hf_mgcp_param_extension_critical;
static int hf_mgcp_param_resourceid;
static int hf_mgcp_param_invalid;
static int hf_mgcp_messagecount;
static int hf_mgcp_dup;
static int hf_mgcp_req_dup;
static int hf_mgcp_req_dup_frame;
static int hf_mgcp_rsp_dup;
static int hf_mgcp_rsp_dup_frame;
static int hf_mgcp_param_x_osmux;
static int hf_mgcp_unknown_parameter;
static int hf_mgcp_malformed_parameter;

static expert_field ei_mgcp_rsp_rspcode_invalid;

static const value_string mgcp_return_code_vals[] = {
	{000, "Response Acknowledgement"},
	{100, "The transaction is currently being executed.  An actual completion message will follow on later."},
	{101, "The transaction has been queued for execution.  An actual completion message will follow later."},
	{200, "The requested transaction was executed normally."},
	{250, "The connection was deleted."},
	{400, "The transaction could not be executed, due to a transient error."},
	{401, "The phone is already off hook"},
	{402, "The phone is already on hook"},
	{403, "The transaction could not be executed, because the endpoint does not have sufficient resources at this time"},
	{404, "Insufficient bandwidth at this time"},
	{405, "The transaction could not be executed, because the endpoint is \"restarting\"."},
	{406, "Transaction time-out.  The transaction did not complete in a reasonable period of time and has been aborted."},
	{407, "Transaction aborted.  The transaction was aborted by some external action, e.g., a ModifyConnection command aborted by a DeleteConnection command."},
	{409, "The transaction could not be executed because of internal overload."},
	{410, "No endpoint available.  A valid \"any of\" wildcard was used, however there was no endpoint available to satisfy the request."},
	{500, "The transaction could not be executed, because the endpoint is unknown."},
	{501, "The transaction could not be executed, because the endpoint is not ready."},
	{502, "The transaction could not be executed, because the endpoint does not have sufficient resources"},
	{503, "\"All of\" wildcard too complicated."},
	{504, "Unknown or unsupported command."},
	{505, "Unsupported RemoteConnectionDescriptor."},
	{506, "Unable to satisfy both LocalConnectionOptions and RemoteConnectionDescriptor."},
	{507, "Unsupported functionality."},
	{508, "Unknown or unsupported quarantine handling."},
	{509, "Error in RemoteConnectionDescriptor."},
	{510, "The transaction could not be executed, because a protocol error was detected."},
	{511, "The transaction could not be executed, because the command contained an unrecognized extension."},
	{512, "The transaction could not be executed, because the gateway is not equipped to detect one of the requested events."},
	{513, "The transaction could not be executed, because the gateway is not equipped to generate one of the requested signals."},
	{514, "The transaction could not be executed, because the gateway cannot send the specified announcement."},
	{515, "The transaction refers to an incorrect connection-id (may have been already deleted)"},
	{516, "The transaction refers to an unknown call-id."},
	{517, "Unsupported or invalid mode."},
	{518, "Unsupported or unknown package."},
	{519, "Endpoint does not have a digit map."},
	{520, "The transaction could not be executed, because the endpoint is 'restarting'."},
	{521, "Endpoint redirected to another Call Agent."},
	{522, "No such event or signal."},
	{523, "Unknown action or illegal combination of actions"},
	{524, "Internal inconsistency in LocalConnectionOptions"},
	{525, "Unknown extension in LocalConnectionOptions"},
	{526, "Insufficient bandwidth"},
	{527, "Missing RemoteConnectionDescriptor"},
	{528, "Incompatible protocol version"},
	{529, "Internal hardware failure"},
	{530, "CAS signaling protocol error."},
	{531, "failure of a grouping of trunks (e.g. facility failure)."},
	{532, "Unsupported value(s) in LocalConnectionOptions."},
	{533, "Response too large."},
	{534, "Codec negotiation failure."},
	{535, "Packetization period not supported"},
	{536, "Unknown or unsupported RestartMethod"},
	{537, "Unknown or unsupported digit map extension"},
	{538, "Event/signal parameter error (e.g., missing, erroneous, unsupported, unknown, etc.)"},
	{539, "Invalid or unsupported command parameter."},
	{540, "Per endpoint connection limit exceeded."},
	{541, "Invalid or unsupported LocalConnectionOptions"},
	{0,   NULL }
};
static value_string_ext mgcp_return_code_vals_ext = VALUE_STRING_EXT_INIT(mgcp_return_code_vals);

/* TODO: add/use when tested/have capture to test with */
/*
static const value_string mgcp_reason_code_vals[] = {
	{0,   "Endpoint state is normal"},
	{900, "Endpoint malfunctioning."},
	{901, "Endpoint taken out-of-service."},
	{902, "Loss of lower layer connectivity (e.g., downstream sync)."},
	{903, "QoS resource reservation was lost."},
	{904, "Manual intervention."},
	{905, "Facility failure (e.g., DS-0 failure)."},
	{0,   NULL }
};
*/


/*
 * Define the trees for mgcp
 * We need one for MGCP itself, one for the MGCP paramters and one
 * for each of the dissected parameters
 */
static int ett_mgcp;
static int ett_mgcp_param;
static int ett_mgcp_param_connectionparam;
static int ett_mgcp_param_localconnectionoptions;
static int ett_mgcp_param_localvoicemetrics;
static int ett_mgcp_param_remotevoicemetrics;

/*
 * Define the tap for mgcp
 */
static int mgcp_tap;

/*
 * Here are the global variables associated with
 * the various user definable characteristics of the dissection
 *
 * MGCP has two kinds of "agents", gateways and callagents.  Callagents
 * control gateways in a master/slave sort of arrangement.  Since gateways
 * and callagents have different well known ports and could both
 * operate under either udp or tcp we have rather a lot of port info to
 * specify.
 *
 * global_mgcp_raw_text determines whether we are going to display
 * the raw text of the mgcp message, much like the HTTP dissector does.
 *
 */
static unsigned global_mgcp_gateway_tcp_port   = TCP_PORT_MGCP_GATEWAY;
static unsigned global_mgcp_gateway_udp_port   = UDP_PORT_MGCP_GATEWAY;
static unsigned global_mgcp_callagent_tcp_port = TCP_PORT_MGCP_CALLAGENT;
static unsigned global_mgcp_callagent_udp_port = UDP_PORT_MGCP_CALLAGENT;
static bool global_mgcp_raw_text;
static bool global_mgcp_message_count;

/* Some basic utility functions that are specific to this dissector */
static bool is_mgcp_verb(tvbuff_t *tvb, int offset, int maxlength, const char **verb_name);
static bool is_mgcp_rspcode(tvbuff_t *tvb, int offset, int maxlength);

/*
 * The various functions that either dissect some
 * subpart of MGCP.  These aren't really proto dissectors but they
 * are written in the same style.
 */
static void dissect_mgcp_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
				 proto_tree *mgcp_tree, proto_tree *ti);
static void dissect_mgcp_firstline(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, mgcp_info_t* mi);
static void dissect_mgcp_params(tvbuff_t *tvb, packet_info* pinfo, proto_tree *tree, mgcp_info_t* mi);
static void dissect_mgcp_connectionparams(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb,
					  int offset, int param_type_len,
					  int param_val_len);
static void dissect_mgcp_localconnectionoptions(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb,
						int offset, int param_type_len,
						int param_val_len);
static void dissect_mgcp_localvoicemetrics(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb,
						int offset, int param_type_len,
						int param_val_len);
static void dissect_mgcp_remotevoicemetrics(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb,
						int offset, int param_type_len,
						int param_val_len);

static void mgcp_raw_text_add(tvbuff_t *tvb, proto_tree *tree);

#define NUM_TIMESTATS 11

static const value_string mgcp_message_type[] = {
	{  0, "Overall"},
	{  1, "EPCF   "},
	{  2, "CRCX   "},
	{  3, "MDCX   "},
	{  4, "DLCX   "},
	{  5, "RQNT   "},
	{  6, "NTFY   "},
	{  7, "AUEP   "},
	{  8, "AUCX   "},
	{  9, "RSIP   "},
	{  0, NULL}
};

static tap_packet_status
mgcpstat_packet(void *pms, packet_info *pinfo, epan_dissect_t *edt _U_, const void *pmi, tap_flags_t flags _U_)
{
	rtd_data_t* rtd_data = (rtd_data_t*)pms;
	rtd_stat_table* ms = &rtd_data->stat_table;
	const mgcp_info_t *mi = (const mgcp_info_t *)pmi;
	nstime_t delta;
	tap_packet_status ret = TAP_PACKET_DONT_REDRAW;

	switch (mi->mgcp_type) {

	case MGCP_REQUEST:
		if (mi->is_duplicate) {
			/* Duplicate is ignored */
			ms->time_stats[0].req_dup_num++;
		}
		else {
			ms->time_stats[0].open_req_num++;
		}
		break;

	case MGCP_RESPONSE:
		if (mi->is_duplicate) {
			/* Duplicate is ignored */
			ms->time_stats[0].rsp_dup_num++;
		}
		else if (!mi->request_available) {
			/* no request was seen */
			ms->time_stats[0].disc_rsp_num++;
		}
		else {
			ms->time_stats[0].open_req_num--;
			/* calculate time delta between request and response */
			nstime_delta(&delta, &pinfo->abs_ts, &mi->req_time);

			time_stat_update(&(ms->time_stats[0].rtd[0]), &delta, pinfo);

			if (g_ascii_strncasecmp(mi->code, "EPCF", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[1]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "CRCX", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[2]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "MDCX", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[3]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "DLCX", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[4]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "RQNT", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[5]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "NTFY", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[6]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "AUEP", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[7]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "AUCX", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[8]), &delta, pinfo);
			}
			else if (g_ascii_strncasecmp(mi->code, "RSIP", 4) == 0 ) {
				time_stat_update(&(ms->time_stats[0].rtd[9]), &delta, pinfo);
			}
			else {
				time_stat_update(&(ms->time_stats[0].rtd[10]), &delta, pinfo);
			}

			ret = TAP_PACKET_REDRAW;
		}
		break;

	default:
		break;
	}

	return ret;
}


/*
 * Some functions which should be moved to a library
 * as I think that people may find them of general usefulness.
 */
static int tvb_find_null_line(tvbuff_t* tvb, int offset, int len, int* next_offset);
static int tvb_find_dot_line(tvbuff_t* tvb, int offset, int len, int* next_offset);

static dissector_handle_t sdp_handle;
static dissector_handle_t mgcp_handle;
extern void
dissect_asciitpkt(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
		  dissector_handle_t subdissector_handle);
extern uint16_t is_asciitpkt(tvbuff_t *tvb);

/*
 * Init Hash table stuff
 */

typedef struct _mgcp_call_info_key
{
	uint32_t transid;
	conversation_t *conversation;
} mgcp_call_info_key;

static wmem_map_t *mgcp_calls;

/* Compare 2 keys */
static int mgcp_call_equal(const void *k1, const void *k2)
{
	const mgcp_call_info_key* key1 = (const mgcp_call_info_key*) k1;
	const mgcp_call_info_key* key2 = (const mgcp_call_info_key*) k2;

	return (key1->transid == key2->transid &&
	        key1->conversation == key2->conversation);
}

/* Calculate a hash key */
static unsigned mgcp_call_hash(const void *k)
{
	const mgcp_call_info_key* key = (const mgcp_call_info_key*) k;

	return key->transid  + key->conversation->conv_index;
}


/************************************************************************
 * dissect_mgcp - The dissector for the Media Gateway Control Protocol
 ************************************************************************/
static int dissect_mgcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
	int sectionlen;
	uint32_t num_messages;
	int tvb_sectionend, tvb_sectionbegin, tvb_len;
	proto_tree *mgcp_tree = NULL;
	proto_item *ti = NULL, *tii;
	const char *verb_name = "";

	/* Initialize variables */
	tvb_sectionend = 0;
	tvb_sectionbegin = tvb_sectionend;
	tvb_len = tvb_reported_length(tvb);
	num_messages = 0;

	/*
	 * Check to see whether we're really dealing with MGCP by looking
	 * for a valid MGCP verb or response code.  This isn't infallible,
	 * but it's cheap and it's better than nothing.
	 */
	if (!is_mgcp_verb(tvb, 0, tvb_len, &verb_name) && !is_mgcp_rspcode(tvb, 0, tvb_len))
		return 0;

	/*
	 * Set the columns now, so that they'll be set correctly if we throw
	 * an exception.  We can set them later as well....
	 */
	col_set_str(pinfo->cinfo, COL_PROTOCOL, "MGCP");
	col_clear(pinfo->cinfo, COL_INFO);

	/*
	 * Loop through however many mgcp messages may be stuck in
	 * this packet using piggybacking
	 */
	do
	{
		num_messages++;

		/* Create our mgcp subtree */
		ti = proto_tree_add_item(tree, proto_mgcp, tvb, 0, -1, ENC_NA);
		mgcp_tree = proto_item_add_subtree(ti, ett_mgcp);

		sectionlen = tvb_find_dot_line(tvb, tvb_sectionbegin, -1, &tvb_sectionend);
		if (sectionlen != -1)
		{
			dissect_mgcp_message(tvb_new_subset_length_caplen(tvb, tvb_sectionbegin,
						sectionlen, sectionlen),
					pinfo, tree, mgcp_tree, ti);
			tvb_sectionbegin = tvb_sectionend;
		}
		else
		{
			break;
		}
	} while (tvb_sectionend < tvb_len);

	tii = proto_tree_add_uint(mgcp_tree, hf_mgcp_messagecount, tvb,
			0 , 0 , num_messages);
	proto_item_set_hidden(tii);

	/*
	 * Add our column information after dissecting SDP
	 * in order to prevent the column info changing to reflect the SDP
	 * (when showing message count)
	 */
	tvb_sectionbegin = 0;
	if (global_mgcp_message_count == true )
	{
		if (num_messages > 1)
		{
			col_add_fstr(pinfo->cinfo, COL_PROTOCOL, "MGCP (%i messages)", num_messages);
		}
		else
		{
			col_add_fstr(pinfo->cinfo, COL_PROTOCOL, "MGCP (%i message)", num_messages);
		}
	}

	sectionlen = tvb_find_line_end(tvb, tvb_sectionbegin, -1,
			&tvb_sectionend, false);
	col_prepend_fstr(pinfo->cinfo, COL_INFO, "%s",
			tvb_format_text(pinfo->pool, tvb, tvb_sectionbegin, sectionlen));

	return tvb_len;
}

/************************************************************************
 * dissect_tpkt_mgcp - The dissector for the ASCII TPKT Media Gateway Control Protocol
 ************************************************************************/
static int dissect_tpkt_mgcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data _U_)
{
	uint16_t ascii_tpkt;
	int     offset = 0;

	/* Check whether this looks like a ASCII TPKT-encapsulated
	 *  MGCP packet.
	 */
	ascii_tpkt = is_asciitpkt(tvb);

	if (ascii_tpkt != 1 )
	{
		/*
		 * It's not a ASCII TPKT packet
		 * in MGCP
		 */
		offset = dissect_mgcp(tvb, pinfo, tree, NULL);
	}
	else
	{
		/*
		 * Dissect ASCII TPKT header
		 */
		dissect_asciitpkt(tvb, pinfo, tree, mgcp_handle);
		offset = tvb_reported_length(tvb);
	}

	return offset;
}

/* Dissect an individual MGCP message */
static void dissect_mgcp_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
				 proto_tree *mgcp_tree, proto_tree *ti)
{
	/* Declare variables */
	int sectionlen;
	int tvb_sectionend, tvb_sectionbegin, tvb_len;
	tvbuff_t *next_tvb;
	const char *verb_name = "";
	mgcp_info_t* mi = wmem_new0(pinfo->pool, mgcp_info_t);
	sdp_setup_info_t setup_info = { .hf_id = 0, .hf_type = SDP_TRACE_ID_HF_TYPE_UINT32 };
	media_content_info_t content_info = { MEDIA_CONTAINER_SIP_DATA, NULL, NULL, &setup_info };

	mi->mgcp_type = MGCP_OTHERS;

	/* Initialize variables */
	tvb_len = tvb_reported_length(tvb);

	/*
	 * Check to see whether we're really dealing with MGCP by looking
	 * for a valid MGCP verb or response code.  This isn't infallible,
	 * but it's cheap and it's better than nothing.
	 */
	if (is_mgcp_verb(tvb, 0, tvb_len, &verb_name) || is_mgcp_rspcode(tvb, 0, tvb_len))
	{
		/* dissect first line */
		tvb_sectionbegin = 0;
		tvb_sectionend = tvb_sectionbegin;
		sectionlen = tvb_find_line_end(tvb, 0, -1, &tvb_sectionend, false);
		if (sectionlen > 0)
		{
			dissect_mgcp_firstline(tvb_new_subset_length_caplen(tvb, tvb_sectionbegin,
			                       sectionlen, sectionlen), pinfo,
			                       mgcp_tree, mi);
		}
		tvb_sectionbegin = tvb_sectionend;

		/* Dissect params */
		if (tvb_sectionbegin < tvb_len)
		{
			sectionlen = tvb_find_null_line(tvb, tvb_sectionbegin, -1,
			                                &tvb_sectionend);
			if (sectionlen > 0)
			{
				dissect_mgcp_params(tvb_new_subset_length_caplen(tvb, tvb_sectionbegin, sectionlen, sectionlen),
				                                   pinfo, mgcp_tree, mi);
			}
		}

		/* Set the mgcp payload length correctly so we don't include any
		   encapsulated SDP */
		sectionlen = tvb_sectionend;
		proto_item_set_len(ti, sectionlen);

		/* Display the raw text of the mgcp message if desired */

		/* Do we want to display the raw text of our MGCP packet? */
		if (global_mgcp_raw_text)
		{
			mgcp_raw_text_add(tvb, mgcp_tree);
		}

		/* Dissect sdp payload */
		if (tvb_sectionend < tvb_len)
		{
			setup_info.is_osmux = mi->is_osmux;
			next_tvb = tvb_new_subset_remaining(tvb, tvb_sectionend);
			call_dissector_with_data(sdp_handle, next_tvb, pinfo, tree, &content_info);
		}
	}
}


/*
 * Add the raw text of the message to the dissect tree if appropriate
 * preferences are specified.
 */
static void mgcp_raw_text_add(tvbuff_t *tvb, proto_tree *tree)
{
	int tvb_linebegin, tvb_lineend, linelen;

	tvb_linebegin = 0;

	do
	{
		tvb_find_line_end(tvb, tvb_linebegin, -1, &tvb_lineend, false);
		linelen = tvb_lineend - tvb_linebegin;
		proto_tree_add_format_text(tree, tvb, tvb_linebegin, linelen);
		tvb_linebegin = tvb_lineend;
	} while (tvb_offset_exists(tvb, tvb_lineend));
}

/*
 * is_mgcp_verb - A function for determining whether there is a
 *                MGCP verb at offset in tvb
 *
 * Parameter:
 * tvb - The tvbuff in which we are looking for an MGCP verb
 * offset - The offset in tvb at which we are looking for a MGCP verb
 * maxlength - The maximum distance from offset we may look for the
 *             characters that make up a MGCP verb.
 * verb_name - The name for the verb code found (output)
 *
 * Return: true if there is an MGCP verb at offset in tvb, otherwise false
 */
static bool is_mgcp_verb(tvbuff_t *tvb, int offset, int maxlength, const char **verb_name)
{
	bool returnvalue = false;
	char word[5];

	/* This function is used for checking if a packet is actually an
	   mgcp packet. Make sure that we do not throw an exception
	   during such a check. If we did throw an exeption, we could
	   not refuse the packet and give other dissectors the chance to
	   look at it. */
	if (tvb_captured_length_remaining(tvb, offset) < (int)sizeof(word))
		return false;

	/* Read the string into 'word' and see if it looks like the start of a verb */
	if ((maxlength >= 4) && tvb_get_raw_bytes_as_string(tvb, offset, word, sizeof word))
	{
		if (((g_ascii_strncasecmp(word, "EPCF", 4) == 0) && (*verb_name = "EndpointConfiguration")) ||
		    ((g_ascii_strncasecmp(word, "CRCX", 4) == 0) && (*verb_name = "CreateConnection")) ||
		    ((g_ascii_strncasecmp(word, "MDCX", 4) == 0) && (*verb_name = "ModifyConnection")) ||
		    ((g_ascii_strncasecmp(word, "DLCX", 4) == 0) && (*verb_name = "DeleteConnection")) ||
		    ((g_ascii_strncasecmp(word, "RQNT", 4) == 0) && (*verb_name = "NotificationRequest")) ||
		    ((g_ascii_strncasecmp(word, "NTFY", 4) == 0) && (*verb_name = "Notify")) ||
		    ((g_ascii_strncasecmp(word, "AUEP", 4) == 0) && (*verb_name = "AuditEndpoint")) ||
		    ((g_ascii_strncasecmp(word, "AUCX", 4) == 0) && (*verb_name = "AuditConnection")) ||
		    ((g_ascii_strncasecmp(word, "RSIP", 4) == 0) && (*verb_name = "RestartInProgress")) ||
		    ((g_ascii_strncasecmp(word, "MESG", 4) == 0) && (*verb_name = "Message")) ||
		    (word[0] == 'X' && g_ascii_isalpha(word[1]) && g_ascii_isalpha(word[2]) &&
		                       g_ascii_isalpha(word[3]) && (*verb_name = "*Experimental*")))
		{
			returnvalue = true;
		}
	}

	/* May be whitespace after verb code - anything else is an error.. */
	if (returnvalue && maxlength >= 5)
	{
		char next = tvb_get_uint8(tvb, 4);
		if ((next != ' ') && (next != '\t'))
		{
			returnvalue = false;
		}
	}

	return returnvalue;
}

/*
 * is_mgcp_rspcode - A function for determining whether something which
 *                   looks roughly like a MGCP response code (3-digit number)
 *                   is at 'offset' in tvb
 *
 * Parameters:
 * tvb - The tvbuff in which we are looking for an MGCP response code
 * offset - The offset in tvb at which we are looking for a MGCP response code
 * maxlength - The maximum distance from offset we may look for the
 *             characters that make up a MGCP response code.
 *
 * Return: true if there is an MGCP response code at offset in tvb,
 *         otherwise false
 */
static bool is_mgcp_rspcode(tvbuff_t *tvb, int offset, int maxlength)
{
	bool returnvalue = false;
	char word[4];

	/* see the comment in is_mgcp_verb() */
	if (tvb_captured_length_remaining(tvb, offset) < (int)sizeof(word))
		return false;

	/* Do 1st 3 characters look like digits? */
	if (maxlength >= 3)
	{
		tvb_get_raw_bytes_as_string(tvb, offset, word, sizeof word);
		if (g_ascii_isdigit(word[0]) && g_ascii_isdigit(word[1]) && g_ascii_isdigit(word[2]))
		{
			returnvalue = true;
		}
	}

	/* Maybe some white space after the 3rd digit - anything else is an error */
	if (returnvalue && maxlength >= 4)
	{
		char next = tvb_get_uint8(tvb, 3);
		if ((next != ' ') && (next != '\t'))
		{
			returnvalue = false;
		}
	}

	return returnvalue;
}

/*
 * tvb_parse_param - Parse the MGCP param into a type and a value.
 *
 * Parameters:
 * tvb - The tvbuff containing the MGCP param we are to parse.
 * offset - The offset in tvb at which we will begin looking for a
 *          MGCP parameter to parse.
 * len - The maximum distance from offset in tvb that we can look for
 *       an MGCP parameter to parse.
 * hf - The place to write a pointer to the integer representing the
 *      header field associated with the MGCP parameter parsed.
 *
 * Returns: The offset in tvb where the value of the MGCP parameter
 *          begins.
 */
static int tvb_parse_param(tvbuff_t* tvb, packet_info* pinfo, int offset, int len, int** hf, mgcp_info_t* mi)
{
	int returnvalue = -1, tvb_current_offset, ext_off;
	uint8_t tempchar, plus_minus;
	char **buf;

	tvb_current_offset = offset;
	*hf = NULL;
	buf = NULL;

	if (len > 0)
	{
		tempchar = (uint8_t)g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset));

		switch (tempchar)
		{
			case 'K':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_rspack;
				break;
			case 'B':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_bearerinfo;
				break;
			case 'C':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_callid;
				break;
			case 'I':
				tvb_current_offset++;
				if (len > (tvb_current_offset - offset) &&
				   (tempchar = tvb_get_uint8(tvb, tvb_current_offset)) == ':')
				{
					*hf = &hf_mgcp_param_connectionid;
					tvb_current_offset--;
				}
				else
					if (tempchar == '2')
				{
					*hf = &hf_mgcp_param_secondconnectionid;
				}
				break;
			case 'N':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_notifiedentity;
				break;
			case 'X':
				/* Move past 'X' */
				tvb_current_offset++;

				/* X: is RequestIdentifier */
				if (len > (tvb_current_offset - offset) &&
				   (tempchar = tvb_get_uint8(tvb, tvb_current_offset)) == ':')
				{
					*hf = &hf_mgcp_param_requestid;
					tvb_current_offset--;
				}
				/* XRM/MCR */
				else
				if (len > (tvb_current_offset - offset) &&
				   ((uint8_t)g_ascii_toupper(tvb_get_uint8(tvb,tvb_current_offset))) == 'R')
				{
					/* Move past 'R' */
					tvb_current_offset += 3;
					if (len > (tvb_current_offset - offset) &&
						((uint8_t)g_ascii_toupper(tvb_get_uint8(tvb,tvb_current_offset))) == 'R')
					{
						*hf = &hf_mgcp_param_remotevoicemetrics;
					}
					else
					if (len > (tvb_current_offset - offset) &&
					   ((uint8_t)g_ascii_toupper(tvb_get_uint8(tvb,tvb_current_offset))) == 'L')
					{
						*hf = &hf_mgcp_param_localvoicemetrics;
					}
					tvb_current_offset -= 4;
				}

				/* X+...: or X-....: are vendor extension parameters */
				else
				if (len > (tvb_current_offset - offset) &&
				    ((plus_minus = tvb_get_uint8(tvb, tvb_current_offset)) == '-' ||
				     (plus_minus == '+')))
				{
					/* Move past + or - */
					tvb_current_offset++;

					/* Keep going, through possible vendor param name */
					/* We have a mempbrk; perhaps an equivalent of strspn
					 * for tvbs would be useful.
					 */
					for (ext_off = 0; len > (ext_off + tvb_current_offset-offset); ext_off++) {
						tempchar = tvb_get_uint8(tvb, tvb_current_offset + ext_off);
						if (!g_ascii_isalpha(tempchar) && !g_ascii_isdigit(tempchar)) break;
					}

					if (tempchar == ':')
					{
						/* Looks like a valid vendor param name */
						ws_debug("MGCP Extension: %s", tvb_get_string_enc(pinfo->pool, tvb, tvb_current_offset, ext_off, ENC_ASCII));
						switch (plus_minus)
						{
							case '+':
								*hf = &hf_mgcp_param_extension_critical;
								break;
							case '-':
								if (tvb_strncaseeql(tvb, tvb_current_offset, "OSMUX", ext_off) == 0) {
									*hf = &hf_mgcp_param_x_osmux;
								} else {
									*hf = &hf_mgcp_param_extension;
								}
								break;
						}
						/* -1: Final generic path below expects us to point to char before the ':'. */
						tvb_current_offset += ext_off - 1;
					}
				}
				break;
			case 'L':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_localconnoptions;
				break;
			case 'M':
				tvb_current_offset++;
				if (len > (tvb_current_offset - offset) &&
				   (tempchar = (uint8_t)g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset))) == ':')
				{
					*hf = &hf_mgcp_param_connectionmode;
					tvb_current_offset--;
				}
				else
				if (tempchar == 'D')
				{
					*hf = &hf_mgcp_param_maxmgcpdatagram;
				}
				break;
			case 'R':
				tvb_current_offset++;
				if (len > (tvb_current_offset - offset) &&
				    (tempchar = (uint8_t)g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset))) == ':')
				{
					*hf = &hf_mgcp_param_reqevents;
					tvb_current_offset--;
				}
				else
				if ( tempchar == 'M')
				{
					*hf = &hf_mgcp_param_restartmethod;
				}
				else
				if (tempchar == 'D')
				{
					*hf = &hf_mgcp_param_restartdelay;
				}
				break;
			case 'S':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_signalreq;
				buf = &(mi->signalReq);
				break;
			case 'D':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					if (len > (tvb_current_offset + 5 - offset) &&
						(g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset + 1) == 'Q')) &&
						(                tvb_get_uint8(tvb, tvb_current_offset + 2) == '-' ) &&
						(g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset + 3) == 'R')) &&
						(g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset + 4) == 'I')) &&
						(                tvb_get_uint8(tvb, tvb_current_offset + 5) == ':' )
					) {
						tvb_current_offset+=4;
						*hf = &hf_mgcp_param_resourceid;
						break;
					}

					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_digitmap;
				mi->hasDigitMap = true;
				break;
			case 'O':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_observedevent;
				buf = &(mi->observedEvents);
				break;
			case 'P':
				tvb_current_offset++;
				if (len > (tvb_current_offset - offset) &&
				    (tempchar = (uint8_t)g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset))) == ':')
				{
					*hf = &hf_mgcp_param_connectionparam;
					tvb_current_offset--;
				}
				else
				if ( tempchar == 'L')
				{
					*hf = &hf_mgcp_param_packagelist;
				}
				break;
			case 'E':
				tvb_current_offset++;
				if (len > (tvb_current_offset - offset) &&
				    (tempchar = (uint8_t)g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset))) == ':')
				{
					*hf = &hf_mgcp_param_reasoncode;
					tvb_current_offset--;
				}
				else
				if ( tempchar == 'S')
				{
					*hf = &hf_mgcp_param_eventstates;
				}
				break;
			case 'Z':
				tvb_current_offset++;
				if (len > (tvb_current_offset - offset) &&
				    (tempchar = (uint8_t)g_ascii_toupper(tvb_get_uint8(tvb, tvb_current_offset))) == ':')
				{
					*hf = &hf_mgcp_param_specificendpoint;
					tvb_current_offset--;
				}
				else
				if (tempchar == '2')
				{
					*hf = &hf_mgcp_param_secondendpointid;
				}
				break;
			case 'F':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_reqinfo;
				break;
			case 'Q':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_quarantinehandling;
				break;
			case 'T':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_detectedevents;
				break;
			case 'A':
				if (tvb_get_uint8(tvb, tvb_current_offset+1) != ':')
				{
					*hf = &hf_mgcp_param_invalid;
					break;
				}
				*hf = &hf_mgcp_param_capabilities;
				break;

			default:
				*hf = &hf_mgcp_param_invalid;
				break;
		}

		/* Move to (hopefully) the colon */
		tvb_current_offset++;

		/* Add a recognised parameter type if we have one */
		if (*hf != NULL && len > (tvb_current_offset - offset) &&
		    tvb_get_uint8(tvb, tvb_current_offset) == ':')
		{
			tvb_current_offset++;
			tvb_current_offset = tvb_skip_wsp(tvb, tvb_current_offset, (len - tvb_current_offset + offset));
			returnvalue = tvb_current_offset;

			/* set the observedEvents or signalReq used in Voip Calls analysis */
			if (buf != NULL) {
				*buf = tvb_get_string_enc(pinfo->pool, tvb, tvb_current_offset, (len - tvb_current_offset + offset), ENC_ASCII);
			}
		}
	}
	else
	{
		/* Was an empty line */
		*hf = &hf_mgcp_param_invalid;
	}

	/* For these types, show the whole line */
	if ((*hf == &hf_mgcp_param_invalid) ||
	    (*hf == &hf_mgcp_param_extension) || (*hf == &hf_mgcp_param_extension_critical) ||
	    (*hf == &hf_mgcp_param_localvoicemetrics) || (*hf == &hf_mgcp_param_remotevoicemetrics))
	{
		returnvalue = offset;
	}

	return returnvalue;
}


/*
 * dissect_mgcp_firstline - Dissects the firstline of an MGCP message.
 *                          Adds the appropriate headers fields to
 *                          tree for the dissection of the first line
 *                          of an MGCP message.
 *
 * Parameters:
 * tvb - The tvb containing the first line of an MGCP message.  This
 *       tvb is presumed to ONLY contain the first line of the MGCP
 *       message.
 * pinfo - The packet info for the packet.  This is not really used
 *         by this function but is passed through so as to retain the
 *         style of a dissector.
 * tree - The tree from which to hang the structured information parsed
 *        from the first line of the MGCP message.
 */
static void dissect_mgcp_firstline(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, mgcp_info_t* mi)
{
	int tvb_current_offset, tvb_previous_offset, tvb_len, tvb_current_len;
	int tokennum, tokenlen;
	proto_item* hidden_item;
	char *transid = NULL;
	char *code = NULL;
	char *endpointId = NULL;
	mgcp_type_t mgcp_type = MGCP_OTHERS;
	conversation_t* conversation;
	mgcp_call_info_key mgcp_call_key;
	mgcp_call_info_key *new_mgcp_call_key = NULL;
	mgcp_call_t *mgcp_call = NULL;
	nstime_t delta;
	const char *verb_description = "";
	char code_with_verb[64] = "";  /* To fit "<4-letter-code> (<longest-verb>)" */
	proto_item* pi;

	static address null_address = ADDRESS_INIT_NONE;
	tvb_previous_offset = 0;
	tvb_len = tvb_reported_length(tvb);
	tvb_current_offset = tvb_previous_offset;
	mi->is_duplicate = false;
	mi->request_available = false;

	/* if (tree) */
	{
		tokennum = 0;

		do
		{
			tvb_current_len = tvb_reported_length_remaining(tvb, tvb_previous_offset);
			tvb_current_offset = tvb_find_uint8(tvb, tvb_previous_offset, tvb_current_len, ' ');
			if (tvb_current_offset == -1)
			{
				tvb_current_offset = tvb_len;
				tokenlen = tvb_current_len;
			}
			else
			{
				tokenlen = tvb_current_offset - tvb_previous_offset;
			}
			if (tokennum == 0)
			{
				if (tokenlen > 4)
				{
					/* XXX - exception */
					return;
				}

				code = tvb_format_text(pinfo->pool, tvb, tvb_previous_offset, tokenlen);
				(void) g_strlcpy(mi->code, code, 5);
				if (is_mgcp_verb(tvb, tvb_previous_offset, tvb_current_len, &verb_description))
				{
					mgcp_type = MGCP_REQUEST;
					if (verb_description != NULL)
					{
						/* Can show verb along with code if known */
						snprintf(code_with_verb, 64, "%s (%s)", code, verb_description);
					}

					proto_tree_add_string_format(tree, hf_mgcp_req_verb, tvb,
					                             tvb_previous_offset, tokenlen,
					                             code, "%s",
					                             strlen(code_with_verb) ? code_with_verb : code);
				}
				else
				if (is_mgcp_rspcode(tvb, tvb_previous_offset, tvb_current_len))
				{
					bool rspcode_valid;
					mgcp_type = MGCP_RESPONSE;
					rspcode_valid = ws_strtou32(code, NULL, &mi->rspcode);
					pi = proto_tree_add_uint(tree, hf_mgcp_rsp_rspcode, tvb,
					                    tvb_previous_offset, tokenlen, mi->rspcode);
					if (!rspcode_valid)
						expert_add_info(pinfo, pi, &ei_mgcp_rsp_rspcode_invalid);
				}
				else
				{
					break;
				}
			}
			if (tokennum == 1)
			{
				transid = tvb_format_text(pinfo->pool, tvb, tvb_previous_offset, tokenlen);
				/* XXX - what if this isn't a valid text string? */
				mi->transid = (uint32_t)strtoul(transid, NULL, 10);
				proto_tree_add_string(tree, hf_mgcp_transid, tvb,
				                      tvb_previous_offset, tokenlen, transid);
			}
			if (tokennum == 2)
			{
				if (mgcp_type == MGCP_REQUEST)
				{
					endpointId = tvb_format_text(pinfo->pool, tvb, tvb_previous_offset, tokenlen);
					mi->endpointId = wmem_strdup(pinfo->pool, endpointId);
					proto_tree_add_string(tree, hf_mgcp_req_endpoint, tvb,
					                      tvb_previous_offset, tokenlen, endpointId);
				}
				else
				if (mgcp_type == MGCP_RESPONSE)
				{
					if (tvb_current_offset < tvb_len)
					{
						tokenlen = tvb_find_line_end(tvb, tvb_previous_offset,
						                             -1, &tvb_current_offset, false);
					}
					else
					{
						tokenlen = tvb_current_len;
					}
					proto_tree_add_string(tree, hf_mgcp_rsp_rspstring, tvb,
					                      tvb_previous_offset, tokenlen,
					                      tvb_format_text(pinfo->pool, tvb, tvb_previous_offset,
					                      tokenlen));
					break;
				}
			}

			if ((tokennum == 3 && mgcp_type == MGCP_REQUEST))
			{
				if (tvb_current_offset < tvb_len )
				{
					tokenlen = tvb_find_line_end(tvb, tvb_previous_offset,
					                             -1, &tvb_current_offset, false);
				}
				else
				{
					tokenlen = tvb_current_len;
				}
				proto_tree_add_string(tree, hf_mgcp_version, tvb,
				                      tvb_previous_offset, tokenlen,
				                      tvb_format_text(pinfo->pool, tvb, tvb_previous_offset,
				                      tokenlen));
				break;
			}
			if (tvb_current_offset < tvb_len)
			{
				tvb_previous_offset = tvb_skip_wsp(tvb, tvb_current_offset,
				                                   tvb_current_len);
			}
			tokennum++;
		} while (tvb_current_offset < tvb_len && tvb_offset_exists(tvb, tvb_current_offset) && tvb_previous_offset < tvb_len && tokennum <= 3);

		switch (mgcp_type)
		{
			case MGCP_RESPONSE:
				hidden_item = proto_tree_add_boolean(tree, hf_mgcp_rsp, tvb, 0, 0, true);
				proto_item_set_hidden(hidden_item);
				/* Check for MGCP response.  A response must match a call that
				   we've seen, and the response must be sent to the same
				   port and address that the call came from, and must
				   come from the port to which the call was sent.

				   If the transport is connection-oriented (we check, for
				   now, only for "pinfo->ptype" of PT_TCP), we take
				   into account the address from which the call was sent
				   and the address to which the call was sent, because
				   the addresses of the two endpoints should be the same
				   for all calls and replies.

				   If the transport is connectionless, we don't worry
				   about the address to which the call was sent and from
				   which the reply was sent, because there's no
				   guarantee that the reply will come from the address
				   to which the call was sent. */
				if (pinfo->ptype == PT_TCP)
				{
					conversation = find_conversation_pinfo(pinfo, 0);
				}
				else
				{
					/* XXX - can we just use NO_ADDR_B?  Unfortunately,
					 * you currently still have to pass a non-null
					 * pointer for the second address argument even
					 * if you do that.
					 */
					conversation = find_conversation(pinfo->num, &null_address,
					                                 &pinfo->dst, conversation_pt_to_conversation_type(pinfo->ptype), pinfo->srcport,
					                                 pinfo->destport, 0);
				}
				if (conversation != NULL)
				{
					/* Look only for matching request, if
					   matching conversation is available. */
					mgcp_call_key.transid = mi->transid;
					mgcp_call_key.conversation = conversation;
					mgcp_call = (mgcp_call_t *)wmem_map_lookup(mgcp_calls, &mgcp_call_key);
					if (mgcp_call)
					{
						/* Indicate the frame to which this is a reply. */
						if (mgcp_call->req_num)
						{
							proto_item* item;
							mi->request_available = true;
							mgcp_call->responded = true;
							mi->req_num = mgcp_call->req_num;
							(void) g_strlcpy(mi->code, mgcp_call->code, 5);
							item = proto_tree_add_uint_format(tree, hf_mgcp_req_frame,
							                                  tvb, 0, 0, mgcp_call->req_num,
							                                  "This is a response to a request in frame %u",
							                                  mgcp_call->req_num);
							proto_item_set_generated(item);
							nstime_delta(&delta, &pinfo->abs_ts, &mgcp_call->req_time);
							item = proto_tree_add_time(tree, hf_mgcp_time, tvb, 0, 0, &delta);
							proto_item_set_generated(item);
						}

						if (mgcp_call->rsp_num == 0)
						{
							/* We have not yet seen a response to that call, so
							   this must be the first response; remember its
							   frame number. */
							mgcp_call->rsp_num = pinfo->num;
						}
						else
						{
							/* We have seen a response to this call - but was it
							   *this* response? (disregard provisional responses) */
							if ((mgcp_call->rsp_num != pinfo->num) &&
							    (mi->rspcode >= 200) &&
							    (mi->rspcode == mgcp_call->rspcode))
							{
								proto_item* item;

								/* No, so it's a duplicate response. Mark it as such. */
								mi->is_duplicate = true;
								col_append_fstr(pinfo->cinfo, COL_INFO,
										", Duplicate Response %u",
										mi->transid);

								item = proto_tree_add_uint(tree, hf_mgcp_dup, tvb, 0, 0, mi->transid);
								proto_item_set_hidden(item);
								item = proto_tree_add_uint(tree, hf_mgcp_rsp_dup,
										tvb, 0, 0, mi->transid);
								proto_item_set_generated(item);
								item = proto_tree_add_uint(tree, hf_mgcp_rsp_dup_frame,
										tvb, 0, 0, mgcp_call->rsp_num);
								proto_item_set_generated(item);
							}
						}
						/* Now store the response code (after comparison above) */
						mgcp_call->rspcode = mi->rspcode;
					}
				}
				break;
			case MGCP_REQUEST:
				hidden_item = proto_tree_add_boolean(tree, hf_mgcp_req, tvb, 0, 0, true);
				proto_item_set_hidden(hidden_item);
				/* Keep track of the address and port whence the call came,
				 * and the port to which the call is being sent, so that
				 * we can match up calls with replies.
				 *
				 * If the transport is connection-oriented (we check, for
				 * now, only for "pinfo->ptype" of PT_TCP), we take
				 * into account the address from which the call was sent
				 * and the address to which the call was sent, because
				 * the addresses of the two endpoints should be the same
				 * for all calls and replies.
				 *
				 * If the transport is connectionless, we don't worry
				 * about the address to which the call was sent and from
				 * which the reply was sent, because there's no
				 * guarantee that the reply will come from the address
				 * to which the call was sent.
				 */
				if (pinfo->ptype == PT_TCP)
				{
					conversation = find_conversation_pinfo(pinfo, 0);
				}
				else
				{
					/*
					 * XXX - can we just use NO_ADDR_B?  Unfortunately,
					 * you currently still have to pass a non-null
					 * pointer for the second address argument even
					 * if you do that.
					 */
					conversation = find_conversation(pinfo->num, &pinfo->src,
					                                 &null_address, conversation_pt_to_conversation_type(pinfo->ptype), pinfo->srcport,
					                                 pinfo->destport, 0);
				}
				if (conversation == NULL)
				{
					/* It's not part of any conversation - create a new one. */
					if (pinfo->ptype == PT_TCP)
					{
						conversation = conversation_new(pinfo->num, &pinfo->src,
						                                &pinfo->dst, CONVERSATION_TCP, pinfo->srcport,
						                                pinfo->destport, 0);
					}
					else
					{
						conversation = conversation_new(pinfo->num, &pinfo->src,
						                                &null_address, conversation_pt_to_conversation_type(pinfo->ptype), pinfo->srcport,
						                                pinfo->destport, 0);
					}
				}

				/* Prepare the key data */
				mgcp_call_key.transid = mi->transid;
				mgcp_call_key.conversation = conversation;

				/* Look up the request */
				mgcp_call = (mgcp_call_t *)wmem_map_lookup(mgcp_calls, &mgcp_call_key);
				if (mgcp_call != NULL)
				{
					/* We've seen a request with this TRANSID, with the same
					   source and destination, before - but was it
					   *this* request? */
					if (pinfo->num != mgcp_call->req_num)
					{
						/* No, so it's a duplicate request. Mark it as such. */
						mi->is_duplicate = true;
						mi->req_num = mgcp_call->req_num;
						col_append_fstr(pinfo->cinfo, COL_INFO,
						                ", Duplicate Request %u",
						                mi->transid);
						if (tree)
						{
							proto_item* item;
							item = proto_tree_add_uint(tree, hf_mgcp_dup, tvb, 0, 0, mi->transid);
							proto_item_set_hidden(item);
							item = proto_tree_add_uint(tree, hf_mgcp_req_dup, tvb, 0, 0, mi->transid);
							proto_item_set_generated(item);
							item = proto_tree_add_uint(tree, hf_mgcp_req_dup_frame, tvb, 0, 0, mi->req_num);
							proto_item_set_generated(item);
						}
					}
				}
				else
				{
					/* Prepare the value data.
					   "req_num" and "rsp_num" are frame numbers;
					   frame numbers are 1-origin, so we use 0
					   to mean "we don't yet know in which frame
					   the reply for this call appears". */
					new_mgcp_call_key    = (mgcp_call_info_key *)wmem_alloc(wmem_file_scope(), sizeof(*new_mgcp_call_key));
					*new_mgcp_call_key   = mgcp_call_key;
					mgcp_call            = (mgcp_call_t *)wmem_alloc(wmem_file_scope(), sizeof(*mgcp_call));
					mgcp_call->req_num   = pinfo->num;
					mgcp_call->rsp_num   = 0;
					mgcp_call->transid   = mi->transid;
					mgcp_call->responded = false;
					mgcp_call->req_time=pinfo->abs_ts;
					(void) g_strlcpy(mgcp_call->code, mi->code, 5);

					/* Store it */
					wmem_map_insert(mgcp_calls, new_mgcp_call_key, mgcp_call);
				}
				if (mgcp_call->rsp_num)
				{
					proto_item* item = proto_tree_add_uint_format(tree, hf_mgcp_rsp_frame,
					                                              tvb, 0, 0, mgcp_call->rsp_num,
					                                              "The response to this request is in frame %u",
					                                              mgcp_call->rsp_num);
					proto_item_set_generated(item);
				}
				break;
			default:
				break;
		}

		mi->mgcp_type = mgcp_type;
		if (mgcp_call)
		{
			mi->req_time.secs=mgcp_call->req_time.secs;
			mi->req_time.nsecs=mgcp_call->req_time.nsecs;
		}
	}

	tap_queue_packet(mgcp_tap, pinfo, mi);
}

/*
 * dissect_mgcp_params - Dissects the parameters of an MGCP message.
 *                       Adds the appropriate headers fields to
 *                       tree for the dissection of the parameters
 *                       of an MGCP message.
 *
 * Parameters:
 * tvb - The tvb containing the parameters of an MGCP message.  This
 *       tvb is presumed to ONLY contain the part of the MGCP
 *       message which contains the MGCP parameters.
 * tree - The tree from which to hang the structured information parsed
 *        from the parameters of the MGCP message.
 */
static void dissect_mgcp_params(tvbuff_t *tvb, packet_info* pinfo, proto_tree *tree, mgcp_info_t* mi)
{
	int linelen, tokenlen, *my_param;
	int tvb_lineend, tvb_linebegin, tvb_len, old_lineend;
	int tvb_tokenbegin;
	proto_tree *mgcp_param_ti, *mgcp_param_tree;

	tvb_len = tvb_reported_length(tvb);
	tvb_linebegin = 0;
	tvb_lineend = tvb_linebegin;

	mgcp_param_ti = proto_tree_add_item(tree, hf_mgcp_params, tvb,
			tvb_linebegin, tvb_len, ENC_NA);
	proto_item_set_text(mgcp_param_ti, "Parameters");
	mgcp_param_tree = proto_item_add_subtree(mgcp_param_ti, ett_mgcp_param);

	/* Parse the parameters */
	while (tvb_offset_exists(tvb, tvb_lineend))
	{
		old_lineend = tvb_lineend;
		linelen = tvb_find_line_end(tvb, tvb_linebegin, -1, &tvb_lineend, false);
		tvb_tokenbegin = tvb_parse_param(tvb, pinfo, tvb_linebegin, linelen, &my_param, mi);

		if (my_param)
		{
			tokenlen = tvb_find_line_end(tvb, tvb_tokenbegin, -1, &tvb_lineend, false);
			if (*my_param == hf_mgcp_param_connectionparam) {
				dissect_mgcp_connectionparams(mgcp_param_tree, pinfo, tvb, tvb_linebegin,
							      tvb_tokenbegin - tvb_linebegin, tokenlen);
			} else if (*my_param == hf_mgcp_param_localconnoptions) {
				dissect_mgcp_localconnectionoptions(mgcp_param_tree, pinfo, tvb, tvb_linebegin,
								    tvb_tokenbegin - tvb_linebegin, tokenlen);
			} else if (*my_param == hf_mgcp_param_localvoicemetrics) {
				dissect_mgcp_localvoicemetrics(mgcp_param_tree, pinfo, tvb, tvb_linebegin,
							       tvb_tokenbegin - tvb_linebegin, tokenlen);
			} else if (*my_param == hf_mgcp_param_remotevoicemetrics) {
				dissect_mgcp_remotevoicemetrics(mgcp_param_tree, pinfo, tvb, tvb_linebegin,
								tvb_tokenbegin - tvb_linebegin, tokenlen);
			} else if (*my_param == hf_mgcp_param_x_osmux) {
					proto_tree_add_string(mgcp_param_tree, *my_param, tvb,
							      tvb_linebegin, linelen,
							      tvb_format_text(pinfo->pool,
									      tvb, tvb_tokenbegin, tokenlen));
					/* Mark that Osmux is used, so that packet-sdp.c doesn't call
					 * srtp_add_address() and decodes it as RTP. */
					mi->is_osmux = true;
			} else {
				proto_tree_add_string(mgcp_param_tree, *my_param, tvb,
						      tvb_linebegin, linelen,
						      tvb_format_text(pinfo->pool,
								      tvb, tvb_tokenbegin, tokenlen));
			}
		}

		tvb_linebegin = tvb_lineend;
		/* Its a infinite loop if we didn't advance (or went backwards) */
		if (old_lineend >= tvb_lineend)
		{
			/* XXX - exception */
			break;
		}
	}
}

/* Dissect the connection params */
static void
dissect_mgcp_connectionparams(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb, int offset, int param_type_len, int param_val_len)
{
	proto_tree *tree;
	proto_item *item;

	char *tokenline;
	char **tokens;
	unsigned i;

	item = proto_tree_add_item(parent_tree, hf_mgcp_param_connectionparam, tvb, offset, param_type_len+param_val_len, ENC_ASCII);
	tree = proto_item_add_subtree(item, ett_mgcp_param_connectionparam);

	/* The P: line */
	offset += param_type_len; /* skip the P: */
	tokenline = tvb_get_string_enc(pinfo->pool, tvb, offset, param_val_len, ENC_ASCII);

	/* Split into type=value pairs separated by comma */
	tokens = wmem_strsplit(pinfo->pool, tokenline, ",", -1);

	for (i = 0; tokens[i] != NULL; i++)
	{
		char **typval;
		unsigned tokenlen;
		int hf_uint = 0;
		int hf_string = 0;

		tokenlen = (int)strlen(tokens[i]);
		typval = wmem_strsplit(pinfo->pool, tokens[i], "=", 2);
		if ((typval[0] != NULL) && (typval[1] != NULL))
		{
			if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PS"))
			{
				hf_uint = hf_mgcp_param_connectionparam_ps;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "OS"))
			{
				hf_uint = hf_mgcp_param_connectionparam_os;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PR"))
			{
				hf_uint = hf_mgcp_param_connectionparam_pr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "OR"))
			{
				hf_uint = hf_mgcp_param_connectionparam_or;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PL"))
			{
				hf_uint = hf_mgcp_param_connectionparam_pl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JI"))
			{
				hf_uint = hf_mgcp_param_connectionparam_ji;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "LA"))
			{
				hf_uint = hf_mgcp_param_connectionparam_la;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PC/RPS"))
			{
				hf_uint = hf_mgcp_param_connectionparam_pcrps;
			} else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PC/ROS"))
			{
				hf_uint = hf_mgcp_param_connectionparam_pcros;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PC/RPL"))
			{
				hf_uint = hf_mgcp_param_connectionparam_pcrpl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PC/RJI"))
			{
				hf_uint = hf_mgcp_param_connectionparam_pcrji;
			}
			else if (!g_ascii_strncasecmp(g_strstrip(typval[0]), "X-", 2))
			{
				hf_string = hf_mgcp_param_connectionparam_x;
			}

			if (hf_uint > 0)
			{
				proto_tree_add_uint(tree, hf_uint, tvb, offset, tokenlen, (uint32_t)strtoul(typval[1], NULL, 10));
			}
			else if (hf_string > 0)
			{
				proto_tree_add_string(tree, hf_string, tvb, offset, tokenlen, g_strstrip(typval[1]));
			}
			else
			{
				proto_tree_add_string(tree, hf_mgcp_unknown_parameter, tvb, offset, tokenlen, tokens[i]);
			}
		}
		else
		{
			proto_tree_add_string(tree, hf_mgcp_malformed_parameter, tvb, offset, tokenlen, tokens[i]);
		}
		offset += tokenlen + 1; /* 1 extra for the delimiter */
	}

}

/* Dissect the local connection option */
static void
dissect_mgcp_localconnectionoptions(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb, int offset, int param_type_len, int param_val_len)
{
	proto_tree *tree;
	proto_item *item;

	char *tokenline;
	char **tokens;
	unsigned i;

	item = proto_tree_add_item(parent_tree, hf_mgcp_param_localconnoptions, tvb, offset, param_type_len+param_val_len, ENC_ASCII);
	tree = proto_item_add_subtree(item, ett_mgcp_param_localconnectionoptions);

	/* The L: line */
	offset += param_type_len; /* skip the L: */
	tokenline = tvb_get_string_enc(pinfo->pool, tvb, offset, param_val_len, ENC_ASCII);

	/* Split into type=value pairs separated by comma */
	tokens = wmem_strsplit(pinfo->pool, tokenline, ",", -1);
	for (i = 0; tokens[i] != NULL; i++)
	{
		char **typval;
		unsigned tokenlen;
		int hf_uint;
		int hf_string;

		hf_uint = -1;
		hf_string = -1;

		tokenlen = (int)strlen(tokens[i]);
		typval = wmem_strsplit(pinfo->pool, tokens[i], ":", 2);
		if ((typval[0] != NULL) && (typval[1] != NULL))
		{
			if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "p"))
			{
				hf_uint = hf_mgcp_param_localconnoptions_p;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "a"))
			{
				hf_string = hf_mgcp_param_localconnoptions_a;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "s"))
			{
				hf_string = hf_mgcp_param_localconnoptions_s;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "e"))
			{
				hf_string = hf_mgcp_param_localconnoptions_e;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "sc-rtp"))
			{
				hf_string = hf_mgcp_param_localconnoptions_scrtp;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "sc-rtcp"))
			{
				hf_string = hf_mgcp_param_localconnoptions_scrtcp;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "b"))
			{
				hf_string = hf_mgcp_param_localconnoptions_b;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "es-ccd"))
			{
				hf_string = hf_mgcp_param_localconnoptions_esccd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "es-cci"))
			{
				hf_string = hf_mgcp_param_localconnoptions_escci;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "dq-gi"))
			{
				hf_string = hf_mgcp_param_localconnoptions_dqgi;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "dq-rd"))
			{
				hf_string = hf_mgcp_param_localconnoptions_dqrd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "dq-ri"))
			{
				hf_string = hf_mgcp_param_localconnoptions_dqri;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "dq-rr"))
			{
				hf_string = hf_mgcp_param_localconnoptions_dqrr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "k"))
			{
				hf_string = hf_mgcp_param_localconnoptions_k;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "gc"))
			{
				hf_uint = hf_mgcp_param_localconnoptions_gc;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "fmtp"))
			{
				hf_string = hf_mgcp_param_localconnoptions_fmtp;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "nt"))
			{
				hf_string = hf_mgcp_param_localconnoptions_nt;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "o-fmtp"))
			{
				hf_string = hf_mgcp_param_localconnoptions_ofmtp;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "r"))
			{
				hf_string = hf_mgcp_param_localconnoptions_r;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "t"))
			{
				hf_string = hf_mgcp_param_localconnoptions_t;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "r-cnf"))
			{
				hf_string = hf_mgcp_param_localconnoptions_rcnf;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "r-dir"))
			{
				hf_string = hf_mgcp_param_localconnoptions_rdir;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "r-sh"))
			{
				hf_string = hf_mgcp_param_localconnoptions_rsh;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "mp"))
			{
				hf_string = hf_mgcp_param_localconnoptions_mp;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "fxr/fx"))
			{
				hf_string = hf_mgcp_param_localconnoptions_fxr;
			}
			else
			{
				hf_uint = -1;
				hf_string = -1;
			}

			/* Add item */
			if (hf_uint > 0)
			{
				proto_tree_add_uint(tree, hf_uint, tvb, offset, tokenlen, (uint32_t)strtoul(typval[1], NULL, 10));
			}
			else if (hf_string > 0)
			{
				proto_tree_add_string(tree, hf_string, tvb, offset, tokenlen, g_strstrip(typval[1]));
			}
			else
			{
				proto_tree_add_string(tree, hf_mgcp_unknown_parameter, tvb, offset, tokenlen, tokens[i]);
			}
		}
	}
}

/* Dissect the Local Voice Metrics option */
static void
dissect_mgcp_localvoicemetrics(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb, int offset, int param_type_len, int param_val_len)
{
	proto_tree *tree = parent_tree;
	proto_item *item = NULL;

	char *tokenline = NULL;
	char **tokens = NULL;
	char **typval = NULL;
	unsigned i = 0;
	unsigned tokenlen = 0;
	int hf_string = -1;

	if (parent_tree)
	{
	item = proto_tree_add_item(parent_tree, hf_mgcp_param_localvoicemetrics, tvb, offset, param_type_len+param_val_len, ENC_ASCII);
		tree = proto_item_add_subtree(item, ett_mgcp_param_localvoicemetrics);
	}

	/* The XRM/LVM: line */
	offset += 9; /* skip the XRM/LVM: */
	tokenline = tvb_get_string_enc(pinfo->pool, tvb, offset, param_val_len - 9, ENC_ASCII);

	/* Split into type=value pairs separated by comma and WSP */
	tokens = wmem_strsplit(pinfo->pool, tokenline, ",", -1);
	for (i = 0; tokens[i] != NULL; i++)
	{

		tokenlen = (int)strlen(tokens[i]);
		typval = wmem_strsplit(pinfo->pool, tokens[i], "=", 2);
		if ((typval[0] != NULL) && (typval[1] != NULL))
		{
			if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "NLR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_nlr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JDR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jdr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "BLD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_bld;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "GLD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_gld;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "BD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_bd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "GD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_gd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "RTD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_rtd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "ESD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_esd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "SL"))
			{
				hf_string = hf_mgcp_param_voicemetrics_sl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "NL"))
			{
				hf_string = hf_mgcp_param_voicemetrics_nl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "RERL"))
			{
				hf_string = hf_mgcp_param_voicemetrics_rerl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "GMN"))
			{
				hf_string = hf_mgcp_param_voicemetrics_gmn;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "NSR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_nsr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "XSR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_xsr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "MLQ"))
			{
				hf_string = hf_mgcp_param_voicemetrics_mlq;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "MCQ"))
			{
				hf_string = hf_mgcp_param_voicemetrics_mcq;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PLC"))
			{
				hf_string = hf_mgcp_param_voicemetrics_plc;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBA"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jba;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBN"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbn;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBM"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbm;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBS"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbs;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "IAJ"))
			{
				hf_string = hf_mgcp_param_voicemetrics_iaj;
			}
			else
			{
				hf_string = -1;
			}

			/* Add item */
			if (tree)
			{
				if (hf_string > 0)
				{
					proto_tree_add_string(tree, hf_string, tvb, offset, tokenlen, g_strstrip(typval[1]));
				}
				else
				{
					proto_tree_add_string(tree, hf_mgcp_unknown_parameter, tvb, offset, tokenlen, tokens[i]);
				}
			}
		}
		else if (tree)
		{
			proto_tree_add_string(tree, hf_mgcp_malformed_parameter, tvb, offset, tokenlen, tokens[i]);
		}
		offset += tokenlen + 1; /* 1 extra for the delimiter */
	}
}

/* Dissect the Remote Voice Metrics option */
static void
dissect_mgcp_remotevoicemetrics(proto_tree *parent_tree, packet_info* pinfo, tvbuff_t *tvb, int offset, int param_type_len, int param_val_len)
{
	proto_tree *tree = parent_tree;
	proto_item *item = NULL;

	char *tokenline = NULL;
	char **tokens = NULL;
	char **typval = NULL;
	unsigned i = 0;
	unsigned tokenlen = 0;
	int hf_string = -1;

	if (parent_tree)
	{
	item = proto_tree_add_item(parent_tree, hf_mgcp_param_remotevoicemetrics, tvb, offset, param_type_len+param_val_len, ENC_ASCII);
		tree = proto_item_add_subtree(item, ett_mgcp_param_remotevoicemetrics);
	}

	/* The XRM/RVM: line */
	offset += 9; /* skip the XRM/RVM: */
	tokenline = tvb_get_string_enc(pinfo->pool, tvb, offset, param_val_len - 9, ENC_ASCII);

	/* Split into type=value pairs separated by comma and WSP */
	tokens = wmem_strsplit(pinfo->pool, tokenline, ",", -1);
	for (i = 0; tokens[i] != NULL; i++)
	{
		tokenlen = (int)strlen(tokens[i]);
		typval = wmem_strsplit(pinfo->pool, tokens[i], "=", 2);
		if ((typval[0] != NULL) && (typval[1] != NULL))
		{
			if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "NLR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_nlr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JDR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jdr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "BLD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_bld;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "GLD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_gld;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "BD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_bd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "GD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_gd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "RTD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_rtd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "ESD"))
			{
				hf_string = hf_mgcp_param_voicemetrics_esd;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "SL"))
			{
				hf_string = hf_mgcp_param_voicemetrics_sl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "NL"))
			{
				hf_string = hf_mgcp_param_voicemetrics_nl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "RERL"))
			{
				hf_string = hf_mgcp_param_voicemetrics_rerl;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "GMN"))
			{
				hf_string = hf_mgcp_param_voicemetrics_gmn;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "NSR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_nsr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "XSR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_xsr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "MLQ"))
			{
				hf_string = hf_mgcp_param_voicemetrics_mlq;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "MCQ"))
			{
				hf_string = hf_mgcp_param_voicemetrics_mcq;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "PLC"))
			{
				hf_string = hf_mgcp_param_voicemetrics_plc;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBA"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jba;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBR"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbr;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBN"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbn;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBM"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbm;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "JBS"))
			{
				hf_string = hf_mgcp_param_voicemetrics_jbs;
			}
			else if (!g_ascii_strcasecmp(g_strstrip(typval[0]), "IAJ"))
			{
				hf_string = hf_mgcp_param_voicemetrics_iaj;
			}
			else
			{
				hf_string = -1;
			}

			/* Add item */
			if (tree)
			{
				if (hf_string > 0)
				{
					proto_tree_add_string(tree, hf_string, tvb, offset, tokenlen, g_strstrip(typval[1]));
				}
				else
				{
					proto_tree_add_string(tree, hf_mgcp_unknown_parameter, tvb, offset, tokenlen, tokens[i]);
				}
			}
		}
		else if (tree)
		{
			proto_tree_add_string(tree, hf_mgcp_malformed_parameter, tvb, offset, tokenlen, tokens[i]);
		}
		offset += tokenlen + 1; /* 1 extra for the delimiter */
	}
}

/*
 * tvb_find_null_line - Returns the length from offset to the first null
 *                      line found (a null line is a line that begins
 *                      with a CR or LF.  The offset to the first character
 *                      after the null line is written into the int pointed
 *                      to by next_offset.
 *
 * Parameters:
 * tvb - The tvbuff in which we are looking for a null line.
 * offset - The offset in tvb at which we will begin looking for
 *          a null line.
 * len - The maximum distance from offset in tvb that we will look for
 *       a null line.  If it is -1 we will look to the end of the buffer.
 *
 * next_offset - The location to write the offset of first character
 *               FOLLOWING the null line.
 *
 * Returns: The length from offset to the first character BEFORE
 *          the null line..
 */
static int tvb_find_null_line(tvbuff_t* tvb, int offset, int len, int* next_offset)
{
	int tvb_lineend, tvb_current_len, tvb_linebegin, maxoffset;
	unsigned tempchar;

	tvb_linebegin = offset;
	tvb_lineend = tvb_linebegin;

	/* Simple setup to allow for the traditional -1 search to the end of the tvbuff */
	if (len != -1)
	{
		tvb_current_len = len;
	}
	else
	{
		tvb_current_len = tvb_reported_length_remaining(tvb, offset);
	}

	maxoffset = (tvb_current_len - 1) + offset;

	/* Loop around until we either find a line beginning with a carriage return
	   or newline character or until we hit the end of the tvbuff. */
	do
	{
		tvb_linebegin = tvb_lineend;
		tvb_current_len = tvb_reported_length_remaining(tvb, tvb_linebegin);
		tvb_find_line_end(tvb, tvb_linebegin, tvb_current_len, &tvb_lineend, false);
		tempchar = tvb_get_uint8(tvb, tvb_linebegin);
	} while (tempchar != '\r' && tempchar != '\n' && tvb_lineend <= maxoffset && tvb_offset_exists(tvb, tvb_lineend));


	*next_offset = tvb_lineend;

	if (tvb_lineend <= maxoffset)
	{
		tvb_current_len = tvb_linebegin - offset;
	}
	else
	{
		tvb_current_len = tvb_reported_length_remaining(tvb, offset);
	}

	return tvb_current_len;
}

/*
 * tvb_find_dot_line -  Returns the length from offset to the first line
 *                      containing only a dot (.) character.  A line
 *                      containing only a dot is used to indicate a
 *                      separation between multiple MGCP messages
 *                      piggybacked in the same UDP packet.
 *
 * Parameters:
 * tvb - The tvbuff in which we are looking for a dot line.
 * offset - The offset in tvb at which we will begin looking for
 *          a dot line.
 * len - The maximum distance from offset in tvb that we will look for
 *       a dot line.  If it is -1 we will look to the end of the buffer.
 *
 * next_offset - The location to write the offset of first character
 *               FOLLOWING the dot line.
 *
 * Returns: The length from offset to the first character BEFORE
 *          the dot line or -1 if the character at offset is a .
 *          followed by a newline or a carriage return.
 */
static int tvb_find_dot_line(tvbuff_t* tvb, int offset, int len, int* next_offset)
{
	int tvb_current_offset, tvb_current_len, maxoffset, tvb_len;
	uint8_t tempchar;
	tvb_current_len = len;
	tvb_len = tvb_reported_length(tvb);

	if (len == -1)
	{
		maxoffset = tvb_len - 1;
	}
	else
	{
		maxoffset = (len - 1) + offset;
	}
	tvb_current_offset = offset -1;

	do
	{
		tvb_current_offset = tvb_find_uint8(tvb, tvb_current_offset+1,
		                                     tvb_current_len, '.');
		tvb_current_len = maxoffset - tvb_current_offset + 1;

		/* If we didn't find a . then break out of the loop */
		if (tvb_current_offset == -1)
		{
			break;
		}

		/* Do we have and characters following the . ? */
		if (tvb_current_offset < maxoffset)
		{
			tempchar = tvb_get_uint8(tvb, tvb_current_offset+1);
			/* Are the characters that follow the dot a newline or carriage return ? */
			if (tempchar == '\r' || tempchar == '\n')
			{
				/* Do we have any characters that precede the . ? */
				if (tvb_current_offset == 0)
				{
					break;
				}
				else
				{
					tempchar = tvb_get_uint8(tvb, tvb_current_offset-1);

					/* Are the characters that precede the dot a newline or a
					   carriage return ? */
					if (tempchar == '\r' || tempchar == '\n')
					{
						break;
					}
				}
			}
		}
		else
		if (tvb_current_offset == maxoffset)
		{
			if (tvb_current_offset == 0)
			{
				break;
			}
			else
			{
				tempchar = tvb_get_uint8(tvb, tvb_current_offset-1);
				if (tempchar == '\r' || tempchar == '\n')
				{
					break;
				}
			}
		}
	} while (tvb_current_offset < maxoffset);


	/*
	 * So now we either have the tvb_current_offset of a . in a dot line
	 * or a tvb_current_offset of -1
	 */
	if (tvb_current_offset == -1)
	{
		tvb_current_offset = maxoffset +1;
		*next_offset = maxoffset + 1;
	}
	else
	{
		tvb_find_line_end(tvb, tvb_current_offset, tvb_current_len, next_offset, false);
	}

	if (tvb_current_offset == offset)
	{
		tvb_current_len = -1;
	}
	else
	{
		tvb_current_len = tvb_current_offset - offset;
	}

	return tvb_current_len;
}

/* Register all the bits needed with the filtering engine */

void proto_register_mgcp(void);
void proto_reg_handoff_mgcp(void);

void proto_register_mgcp(void)
{
	expert_module_t* expert_mgcp;

	static hf_register_info hf[] =
		{
			{ &hf_mgcp_req,
			  { "Request", "mgcp.req", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			    "True if MGCP request", HFILL }},
			{ &hf_mgcp_rsp,
			  { "Response", "mgcp.rsp", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
			    "true if MGCP response", HFILL }},
			{ &hf_mgcp_req_frame,
			  { "Request Frame", "mgcp.reqframe", FT_FRAMENUM, BASE_NONE, FRAMENUM_TYPE(FT_FRAMENUM_REQUEST), 0,
			    NULL, HFILL }},
			{ &hf_mgcp_rsp_frame,
			  { "Response Frame", "mgcp.rspframe", FT_FRAMENUM, BASE_NONE, FRAMENUM_TYPE(FT_FRAMENUM_RESPONSE), 0,
			    NULL, HFILL }},
			{ &hf_mgcp_time,
			  { "Time from request", "mgcp.time", FT_RELATIVE_TIME, BASE_NONE, NULL, 0,
			    "Timedelta between Request and Response", HFILL }},
			{ &hf_mgcp_req_verb,
			  { "Verb", "mgcp.req.verb", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Name of the verb", HFILL }},
			{ &hf_mgcp_req_endpoint,
			  { "Endpoint", "mgcp.req.endpoint", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Endpoint referenced by the message", HFILL }},
			{ &hf_mgcp_transid,
			  { "Transaction ID", "mgcp.transid", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Transaction ID of this message", HFILL }},
			{ &hf_mgcp_version,
			  { "Version", "mgcp.version", FT_STRING, BASE_NONE, NULL, 0x0,
			    "MGCP Version", HFILL }},
			{ &hf_mgcp_rsp_rspcode,
			  { "Response Code", "mgcp.rsp.rspcode", FT_UINT32, BASE_DEC|BASE_EXT_STRING, &mgcp_return_code_vals_ext, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_rsp_rspstring,
			  { "Response String", "mgcp.rsp.rspstring", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_params,
			  { "Parameters", "mgcp.params", FT_NONE, BASE_NONE, NULL, 0x0,
			    "MGCP parameters", HFILL }},
			{ &hf_mgcp_param_rspack,
			  { "ResponseAck (K)", "mgcp.param.rspack", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Response Ack", HFILL }},
			{ &hf_mgcp_param_bearerinfo,
			  { "BearerInformation (B)", "mgcp.param.bearerinfo", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Bearer Information", HFILL }},
			{ &hf_mgcp_param_callid,
			  { "CallId (C)", "mgcp.param.callid", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Call Id", HFILL }},
			{ &hf_mgcp_param_connectionid,
			  {"ConnectionIdentifier (I)", "mgcp.param.connectionid", FT_STRING, BASE_NONE, NULL, 0x0,
			   "Connection Identifier", HFILL }},
			{ &hf_mgcp_param_secondconnectionid,
			  { "SecondConnectionID (I2)", "mgcp.param.secondconnectionid", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Second Connection Identifier", HFILL }},
			{ &hf_mgcp_param_notifiedentity,
			  { "NotifiedEntity (N)", "mgcp.param.notifiedentity", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Notified Entity", HFILL }},
			{ &hf_mgcp_param_requestid,
			  { "RequestIdentifier (X)", "mgcp.param.requestid", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Request Identifier", HFILL }},
			{ &hf_mgcp_param_localconnoptions,
			  { "LocalConnectionOptions (L)", "mgcp.param.localconnectionoptions", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Local Connection Options", HFILL }},
			{ &hf_mgcp_param_localconnoptions_p,
			  { "Packetization period (p)", "mgcp.param.localconnectionoptions.p", FT_UINT32, BASE_DEC, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_a,
			  { "Codecs (a)", "mgcp.param.localconnectionoptions.a", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_s,
			  { "Silence Suppression (s)", "mgcp.param.localconnectionoptions.s", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_e,
			  { "Echo Cancellation (e)", "mgcp.param.localconnectionoptions.e", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_scrtp,
			  { "RTP ciphersuite (sc-rtp)", "mgcp.param.localconnectionoptions.scrtp", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_scrtcp,
			  { "RTCP ciphersuite (sc-rtcp)", "mgcp.param.localconnectionoptions.scrtcp", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_b,
			  { "Bandwidth (b)", "mgcp.param.localconnectionoptions.b", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_esccd,
			  { "Content Destination (es-ccd)", "mgcp.param.localconnectionoptions.esccd", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_escci,
			  { "Content Identifier (es-cci)", "mgcp.param.localconnectionoptions.escci", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_dqgi,
			  { "D-QoS GateID (dq-gi)", "mgcp.param.localconnectionoptions.dqgi", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_dqrd,
			  { "D-QoS Reserve Destination (dq-rd)", "mgcp.param.localconnectionoptions.dqrd", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_dqri,
			  { "D-QoS Resource ID (dq-ri)", "mgcp.param.localconnectionoptions.dqri", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_dqrr,
			  { "D-QoS Resource Reservation (dq-rr)", "mgcp.param.localconnectionoptions.dqrr", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_k,
			  { "Encryption Key (k)", "mgcp.param.localconnectionoptions.k", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_gc,
			  { "Gain Control (gc)", "mgcp.param.localconnectionoptions.gc", FT_UINT32, BASE_DEC, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_fmtp,
			  { "Media Format (fmtp)", "mgcp.param.localconnectionoptions.fmtp", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_nt,
			  { "Network Type (nt)", "mgcp.param.localconnectionoptions.nt", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_ofmtp,
			  { "Optional Media Format (o-fmtp)", "mgcp.param.localconnectionoptions.ofmtp", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_r,
			  { "Resource Reservation (r)", "mgcp.param.localconnectionoptions.r", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_t,
			  { "Type of Service (r)", "mgcp.param.localconnectionoptions.t", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_rcnf,
			  { "Reservation Confirmation (r-cnf)", "mgcp.param.localconnectionoptions.rcnf", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_rdir,
			  { "Reservation Direction (r-dir)", "mgcp.param.localconnectionoptions.rdir", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_rsh,
			  { "Resource Sharing (r-sh)", "mgcp.param.localconnectionoptions.rsh", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_mp,
			  { "Multiple Packetization period (mp)", "mgcp.param.localconnectionoptions.mp", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localconnoptions_fxr,
			  { "FXR (fxr/fx)", "mgcp.param.localconnectionoptions.fxr", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_localvoicemetrics,
			  { "LocalVoiceMetrics (XRM/LVM)", "mgcp.param.localvoicemetrics", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_remotevoicemetrics,
			  { "RemoteVoiceMetrics (XRM/RVM)", "mgcp.param.remotevoicemetrics", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_voicemetrics_nlr,
			  { "Network packet loss rate(NLR)", "mgcp.param.voicemetrics.nlr", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics NLR", HFILL }},
			{ &hf_mgcp_param_voicemetrics_jdr,
			  { "Jitter buffer discard rate(JDR)", "mgcp.param.voicemetrics.jdr", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics JDR", HFILL }},
			{ &hf_mgcp_param_voicemetrics_bld,
			  { "Burst loss density(BLD)", "mgcp.param.voicemetrics.bld", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics BLD", HFILL }},
			{ &hf_mgcp_param_voicemetrics_gld,
			  { "Gap loss density(GLD)", "mgcp.param.voicemetrics.gld", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics GLD", HFILL }},
			{ &hf_mgcp_param_voicemetrics_bd,
			  { "Burst duration(BD)", "mgcp.param.voicemetrics.bd", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics BD", HFILL }},
			{ &hf_mgcp_param_voicemetrics_gd,
			  { "Gap duration(GD)", "mgcp.param.voicemetrics.gd", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics GD", HFILL }},
			{ &hf_mgcp_param_voicemetrics_rtd,
			  { "Round trip network delay(RTD)", "mgcp.param.voicemetrics.rtd", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics RTD", HFILL }},
			{ &hf_mgcp_param_voicemetrics_esd,
			  { "End system delay(ESD)", "mgcp.param.voicemetrics.esd", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics ESD", HFILL }},
			{ &hf_mgcp_param_voicemetrics_sl,
			  { "Signal level(SL)", "mgcp.param.voicemetrics.sl", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics SL", HFILL }},
			{ &hf_mgcp_param_voicemetrics_nl,
			  { "Noise level(NL)", "mgcp.param.voicemetrics.nl", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metricsx NL", HFILL }},
			{ &hf_mgcp_param_voicemetrics_rerl,
			  { "Residual echo return loss(RERL)", "mgcp.param.voicemetrics.rerl", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics ERL", HFILL }},
			{ &hf_mgcp_param_voicemetrics_gmn,
			  { "Minimum gap threshold(GMN)", "mgcp.param.voicemetrics.gmn", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics GMN", HFILL }},
			{ &hf_mgcp_param_voicemetrics_nsr,
			  { "R factor(NSR)", "mgcp.param.voicemetrics.nsr", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics NSR", HFILL }},
			{ &hf_mgcp_param_voicemetrics_xsr,
			  { "External R factor(XSR)", "mgcp.param.voicemetrics.xsr", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics XSR", HFILL }},
			{ &hf_mgcp_param_voicemetrics_mlq,
			  { "Estimated MOS-LQ(MLQ)", "mgcp.param.voicemetrics.mlq", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics MLQ", HFILL }},
			{ &hf_mgcp_param_voicemetrics_mcq,
			  { "Estimated MOS-CQ(MCQ)", "mgcp.param.voicemetrics.mcq", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics MCQ", HFILL }},
			{ &hf_mgcp_param_voicemetrics_plc,
			  { "Packet loss concealment type(PLC)", "mgcp.param.voicemetrics.plc", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics PLC", HFILL }},
			{ &hf_mgcp_param_voicemetrics_jba,
			  { "Jitter Buffer Adaptive(JBA)", "mgcp.param.voicemetrics.jba", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics JBA", HFILL }},
			{ &hf_mgcp_param_voicemetrics_jbr,
			  { "Jitter Buffer Rate(JBR)", "mgcp.param.voicemetrics.jbr", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics JBR", HFILL }},
			{ &hf_mgcp_param_voicemetrics_jbn,
			  { "Nominal jitter buffer delay(JBN)", "mgcp.param.voicemetrics.jbn", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics JBN", HFILL }},
			{ &hf_mgcp_param_voicemetrics_jbm,
			  { "Maximum jitter buffer delay(JBM)", "mgcp.param.voicemetrics.jbm", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics JBM", HFILL }},
			{ &hf_mgcp_param_voicemetrics_jbs,
			  { "Absolute maximum jitter buffer delay(JBS)", "mgcp.param.voicemetrics.jbs", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics JBS", HFILL }},
			{ &hf_mgcp_param_voicemetrics_iaj,
			  { "Inter-arrival Jitter(IAJ)", "mgcp.param.voicemetrics.iaj", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Voice Metrics IAJ", HFILL }},
			{ &hf_mgcp_param_connectionmode,
			  { "ConnectionMode (M)", "mgcp.param.connectionmode", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Connection Mode", HFILL }},
			{ &hf_mgcp_param_reqevents,
			  { "RequestedEvents (R)", "mgcp.param.reqevents", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Requested Events", HFILL }},
			{ &hf_mgcp_param_signalreq,
			  { "SignalRequests (S)", "mgcp.param.signalreq", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Signal Request", HFILL }},
			{ &hf_mgcp_param_restartmethod,
			  { "RestartMethod (RM)", "mgcp.param.restartmethod", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Restart Method", HFILL }},
			{ &hf_mgcp_param_restartdelay,
			  { "RestartDelay (RD)", "mgcp.param.restartdelay", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Restart Delay", HFILL }},
			{ &hf_mgcp_param_digitmap,
			  { "DigitMap (D)", "mgcp.param.digitmap", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Digit Map", HFILL }},
			{ &hf_mgcp_param_observedevent,
			  { "ObservedEvents (O)", "mgcp.param.observedevents", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Observed Events", HFILL }},
			{ &hf_mgcp_param_connectionparam,
			  { "ConnectionParameters (P)", "mgcp.param.connectionparam", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Connection Parameters", HFILL }},
			{ &hf_mgcp_param_connectionparam_ps,
			  { "Packets sent (PS)", "mgcp.param.connectionparam.ps", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Packets sent (P:PS)", HFILL }},
			{ &hf_mgcp_param_connectionparam_os,
			  { "Octets sent (OS)", "mgcp.param.connectionparam.os", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Octets sent (P:OS)", HFILL }},
			{ &hf_mgcp_param_connectionparam_pr,
			  { "Packets received (PR)", "mgcp.param.connectionparam.pr", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Packets received (P:PR)", HFILL }},
			{ &hf_mgcp_param_connectionparam_or,
			  { "Octets received (OR)", "mgcp.param.connectionparam.or", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Octets received (P:OR)", HFILL }},
			{ &hf_mgcp_param_connectionparam_pl,
			  { "Packets lost (PL)", "mgcp.param.connectionparam.pl", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Packets lost (P:PL)", HFILL }},
			{ &hf_mgcp_param_connectionparam_ji,
			  { "Jitter (JI)", "mgcp.param.connectionparam.ji", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Average inter-packet arrival jitter in milliseconds (P:JI)", HFILL }},
			{ &hf_mgcp_param_connectionparam_la,
			  { "Latency (LA)", "mgcp.param.connectionparam.la", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Average latency in milliseconds (P:LA)", HFILL }},
			{ &hf_mgcp_param_connectionparam_pcrps,
			  { "Remote Packets sent (PC/RPS)", "mgcp.param.connectionparam.pcrps", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Remote Packets sent (P:PC/RPS)", HFILL }},
			{ &hf_mgcp_param_connectionparam_pcros,
			  { "Remote Octets sent (PC/ROS)", "mgcp.param.connectionparam.pcros", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Remote Octets sent (P:PC/ROS)", HFILL }},
			{ &hf_mgcp_param_connectionparam_pcrpl,
			  { "Remote Packets lost (PC/RPL)", "mgcp.param.connectionparam.pcrpl", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Remote Packets lost (P:PC/RPL)", HFILL }},
			{ &hf_mgcp_param_connectionparam_pcrji,
			  { "Remote Jitter (PC/RJI)", "mgcp.param.connectionparam.pcrji", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Remote Jitter (P:PC/RJI)", HFILL }},
			{ &hf_mgcp_param_connectionparam_x,
			  { "Vendor Extension", "mgcp.param.connectionparam.x", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Vendor Extension (P:X-*)", HFILL }},
			{ &hf_mgcp_param_reasoncode,
			  { "ReasonCode (E)", "mgcp.param.reasoncode", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Reason Code", HFILL }},
			{ &hf_mgcp_param_eventstates,
			  { "EventStates (ES)", "mgcp.param.eventstates", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Event States", HFILL }},
			{ &hf_mgcp_param_specificendpoint,
			  { "SpecificEndpointID (Z)", "mgcp.param.specificendpointid", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Specific Endpoint ID", HFILL }},
			{ &hf_mgcp_param_secondendpointid,
			  { "SecondEndpointID (Z2)", "mgcp.param.secondendpointid", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Second Endpoint ID", HFILL }},
			{ &hf_mgcp_param_reqinfo,
			  { "RequestedInfo (F)", "mgcp.param.reqinfo", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Requested Info", HFILL }},
			{ &hf_mgcp_param_quarantinehandling,
			  { "QuarantineHandling (Q)", "mgcp.param.quarantinehandling", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Quarantine Handling", HFILL }},
			{ &hf_mgcp_param_detectedevents,
			  { "DetectedEvents (T)", "mgcp.param.detectedevents", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Detected Events", HFILL }},
			{ &hf_mgcp_param_capabilities,
			  { "Capabilities (A)", "mgcp.param.capabilities", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_maxmgcpdatagram,
			  {"MaxMGCPDatagram (MD)", "mgcp.param.maxmgcpdatagram", FT_STRING, BASE_NONE, NULL, 0x0,
			   "Maximum MGCP Datagram size", HFILL }},
			{ &hf_mgcp_param_packagelist,
			  {"PackageList (PL)", "mgcp.param.packagelist", FT_STRING, BASE_NONE, NULL, 0x0,
			   "Package List", HFILL }},
			{ &hf_mgcp_param_extension,
			  { "Extension Parameter (non-critical)", "mgcp.param.extension", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_param_extension_critical,
			  { "Extension Parameter (critical)", "mgcp.param.extensioncritical", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Critical Extension Parameter", HFILL }},
			{ &hf_mgcp_param_resourceid,
			  { "ResourceIdentifier (DQ-RI)", "mgcp.param.resourceid", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Resource Identifier", HFILL }},
			{ &hf_mgcp_param_invalid,
			  { "Invalid Parameter", "mgcp.param.invalid", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_messagecount,
			  { "MGCP Message Count", "mgcp.messagecount", FT_UINT32, BASE_DEC, NULL, 0x0,
			    "Number of MGCP message in a packet", HFILL }},
			{ &hf_mgcp_dup,
			  { "Duplicate Message", "mgcp.dup", FT_UINT32, BASE_DEC, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_req_dup,
			  { "Duplicate Request", "mgcp.req.dup", FT_UINT32, BASE_DEC, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_req_dup_frame,
			  { "Original Request Frame", "mgcp.req.dup.frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			    "Frame containing original request", HFILL }},
			{ &hf_mgcp_rsp_dup,
			  { "Duplicate Response", "mgcp.rsp.dup", FT_UINT32, BASE_DEC, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_rsp_dup_frame,
			  { "Original Response Frame", "mgcp.rsp.dup.frame", FT_FRAMENUM, BASE_NONE, NULL, 0x0,
			    "Frame containing original response", HFILL }},
			{ &hf_mgcp_param_x_osmux,
			  { "X-Osmux", "mgcp.param.x_osmux", FT_STRING, BASE_NONE, NULL, 0x0,
			    "Osmux CID", HFILL }},
			{ &hf_mgcp_unknown_parameter,
			  { "Unknown parameter", "mgcp.unknown_parameter", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
			{ &hf_mgcp_malformed_parameter,
			  { "Malformed parameter", "mgcp.rsp.malformed_parameter", FT_STRING, BASE_NONE, NULL, 0x0,
			    NULL, HFILL }},
		};

	static int *ett[] =
		{
			&ett_mgcp,
			&ett_mgcp_param,
			&ett_mgcp_param_connectionparam,
			&ett_mgcp_param_localconnectionoptions,
			&ett_mgcp_param_localvoicemetrics,
			&ett_mgcp_param_remotevoicemetrics
		};

	static ei_register_info ei[] = {
		{ &ei_mgcp_rsp_rspcode_invalid, { "mgcp.rsp.rspcode.invalid", PI_MALFORMED, PI_ERROR,
		"RSP code must be a string containing an integer", EXPFILL }}
	};

	module_t *mgcp_module;

	/* Register protocol */
	proto_mgcp = proto_register_protocol("Media Gateway Control Protocol", "MGCP", "mgcp");
	proto_register_field_array(proto_mgcp, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));

	mgcp_calls = wmem_map_new_autoreset(wmem_epan_scope(), wmem_file_scope(), mgcp_call_hash, mgcp_call_equal);

	mgcp_handle = register_dissector("mgcp", dissect_mgcp, proto_mgcp);

	/* Register our configuration options */
	mgcp_module = prefs_register_protocol(proto_mgcp, proto_reg_handoff_mgcp);

	prefs_register_uint_preference(mgcp_module, "tcp.gateway_port",
				       "MGCP Gateway TCP Port",
				       "Set the UDP port for gateway messages "
				       "(if other than the default of 2427)",
				       10, &global_mgcp_gateway_tcp_port);

	prefs_register_uint_preference(mgcp_module, "udp.gateway_port",
				       "MGCP Gateway UDP Port",
				       "Set the TCP port for gateway messages "
				       "(if other than the default of 2427)",
				       10, &global_mgcp_gateway_udp_port);

	prefs_register_uint_preference(mgcp_module, "tcp.callagent_port",
				       "MGCP Callagent TCP Port",
				       "Set the TCP port for callagent messages "
				       "(if other than the default of 2727)",
				       10, &global_mgcp_callagent_tcp_port);

	prefs_register_uint_preference(mgcp_module, "udp.callagent_port",
				       "MGCP Callagent UDP Port",
				       "Set the UDP port for callagent messages "
				       "(if other than the default of 2727)",
				       10, &global_mgcp_callagent_udp_port);


	prefs_register_bool_preference(mgcp_module, "display_raw_text",
				       "Display raw text for MGCP message",
				       "Specifies that the raw text of the "
				       "MGCP message should be displayed "
				       "instead of (or in addition to) the "
				       "dissection tree",
				       &global_mgcp_raw_text);

	prefs_register_obsolete_preference(mgcp_module, "display_dissect_tree");

	prefs_register_bool_preference(mgcp_module, "display_mgcp_message_count",
				       "Display the number of MGCP messages",
				       "Display the number of MGCP messages "
				       "found in a packet in the protocol column.",
				       &global_mgcp_message_count);

	mgcp_tap = register_tap("mgcp");

	register_rtd_table(proto_mgcp, NULL, 1, NUM_TIMESTATS, mgcp_message_type, mgcpstat_packet, NULL);

	expert_mgcp = expert_register_protocol(proto_mgcp);
	expert_register_field_array(expert_mgcp, ei, array_length(ei));

}

/* The registration hand-off routine */
void proto_reg_handoff_mgcp(void)
{
	static bool mgcp_prefs_initialized = false;
	static dissector_handle_t mgcp_tpkt_handle;
	/*
	 * Variables to allow for proper deletion of dissector registration when
	 * the user changes port from the gui.
	 */
	static unsigned gateway_tcp_port;
	static unsigned gateway_udp_port;
	static unsigned callagent_tcp_port;
	static unsigned callagent_udp_port;

	if (!mgcp_prefs_initialized)
	{
		/* Get a handle for the SDP dissector. */
		sdp_handle = find_dissector_add_dependency("sdp", proto_mgcp);
		mgcp_tpkt_handle = create_dissector_handle(dissect_tpkt_mgcp, proto_mgcp);
		mgcp_prefs_initialized = true;
	}
	else
	{
		dissector_delete_uint("tcp.port", gateway_tcp_port,   mgcp_tpkt_handle);
		dissector_delete_uint("udp.port", gateway_udp_port,   mgcp_handle);
		dissector_delete_uint("tcp.port", callagent_tcp_port, mgcp_tpkt_handle);
		dissector_delete_uint("udp.port", callagent_udp_port, mgcp_handle);
	}

	/* Set our port number for future use */
	gateway_tcp_port = global_mgcp_gateway_tcp_port;
	gateway_udp_port = global_mgcp_gateway_udp_port;

	callagent_tcp_port = global_mgcp_callagent_tcp_port;
	callagent_udp_port = global_mgcp_callagent_udp_port;

	/* Names of port preferences too specific to add "auto" preference here */
	dissector_add_uint("tcp.port", global_mgcp_gateway_tcp_port,   mgcp_tpkt_handle);
	dissector_add_uint("udp.port", global_mgcp_gateway_udp_port,   mgcp_handle);
	dissector_add_uint("tcp.port", global_mgcp_callagent_tcp_port, mgcp_tpkt_handle);
	dissector_add_uint("udp.port", global_mgcp_callagent_udp_port, mgcp_handle);
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
