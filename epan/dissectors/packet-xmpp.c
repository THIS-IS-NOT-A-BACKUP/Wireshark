/* packet-xmpp.c
 * Wireshark's XMPP dissector.
 *
 * Copyright 2011, Mariusz Okroj <okrojmariusz[]gmail.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"


#include <epan/packet.h>
#include <epan/conversation.h>
#include <epan/prefs.h>
#include <epan/proto_data.h>
#include <epan/exceptions.h>

#include "packet-xmpp.h"
#include "packet-xmpp-core.h"

#define XMPP_PORT 5222

void proto_register_xmpp(void);
void proto_reg_handoff_xmpp(void);

int proto_xmpp;

static bool xmpp_desegment = true;

int hf_xmpp_xmlns;
int hf_xmpp_id;
int hf_xmpp_from;
int hf_xmpp_to;
int hf_xmpp_type;
int hf_xmpp_cdata;
int hf_xmpp_attribute;

int hf_xmpp_iq;

int hf_xmpp_query;
int hf_xmpp_query_node;

int hf_xmpp_query_item;
int hf_xmpp_query_item_jid;
int hf_xmpp_query_item_name;
int hf_xmpp_query_item_subscription;
int hf_xmpp_query_item_ask;
int hf_xmpp_query_item_group;
int hf_xmpp_query_item_node;
int hf_xmpp_query_item_approved;

int hf_xmpp_query_identity;
int hf_xmpp_query_identity_category;
int hf_xmpp_query_identity_type;
int hf_xmpp_query_identity_name;
static int hf_xmpp_query_identity_lang;

int hf_xmpp_query_feature;

int hf_xmpp_query_streamhost;
int hf_xmpp_query_streamhost_used;
int hf_xmpp_query_activate;
int hf_xmpp_query_udpsuccess;

int hf_xmpp_error;
int hf_xmpp_error_type;
int hf_xmpp_error_code;
int hf_xmpp_error_condition;
int hf_xmpp_error_text;

int hf_xmpp_iq_bind;
int hf_xmpp_iq_bind_jid;
int hf_xmpp_iq_bind_resource;

int hf_xmpp_services;
int hf_xmpp_channel;

int hf_xmpp_iq_session;
int hf_xmpp_stream;
int hf_xmpp_features;

int hf_xmpp_vcard;
int hf_xmpp_vcard_x_update;

int hf_xmpp_jingle;
int hf_xmpp_jingle_sid;
int hf_xmpp_jingle_initiator;
int hf_xmpp_jingle_responder;
int hf_xmpp_jingle_action;

int hf_xmpp_jingle_content;
int hf_xmpp_jingle_content_creator;
int hf_xmpp_jingle_content_name;
int hf_xmpp_jingle_content_disposition;
int hf_xmpp_jingle_content_senders;

int hf_xmpp_jingle_content_description;
int hf_xmpp_jingle_content_description_media;
int hf_xmpp_jingle_content_description_ssrc;

int hf_xmpp_jingle_cont_desc_payload;
int hf_xmpp_jingle_cont_desc_payload_id;
int hf_xmpp_jingle_cont_desc_payload_channels;
int hf_xmpp_jingle_cont_desc_payload_clockrate;
int hf_xmpp_jingle_cont_desc_payload_maxptime;
int hf_xmpp_jingle_cont_desc_payload_name;
int hf_xmpp_jingle_cont_desc_payload_ptime;

int hf_xmpp_jingle_cont_desc_payload_param;
int hf_xmpp_jingle_cont_desc_payload_param_value;
int hf_xmpp_jingle_cont_desc_payload_param_name;

int hf_xmpp_jingle_cont_desc_enc;
int hf_xmpp_jingle_cont_desc_enc_zrtp_hash;
int hf_xmpp_jingle_cont_desc_enc_crypto;

int hf_xmpp_jingle_cont_desc_rtp_hdr;
int hf_xmpp_jingle_cont_desc_bandwidth;

int hf_xmpp_jingle_cont_trans;
int hf_xmpp_jingle_cont_trans_pwd;
int hf_xmpp_jingle_cont_trans_ufrag;

int hf_xmpp_jingle_cont_trans_cand;
int hf_xmpp_jingle_cont_trans_rem_cand;
int hf_xmpp_jingle_cont_trans_activated;
int hf_xmpp_jingle_cont_trans_candidate_error;
int hf_xmpp_jingle_cont_trans_candidate_used;
int hf_xmpp_jingle_cont_trans_proxy_error;


int hf_xmpp_jingle_reason;
int hf_xmpp_jingle_reason_condition;
int hf_xmpp_jingle_reason_text;

int hf_xmpp_jingle_rtp_info;

int hf_xmpp_jingle_file_transfer_offer;
int hf_xmpp_jingle_file_transfer_request;
int hf_xmpp_jingle_file_transfer_received;
int hf_xmpp_jingle_file_transfer_abort;
int hf_xmpp_jingle_file_transfer_checksum;

int hf_xmpp_si;
int hf_xmpp_si_file;

int hf_xmpp_iq_feature_neg;
int hf_xmpp_x_data;
int hf_xmpp_x_data_field;
int hf_xmpp_x_data_field_value;
int hf_xmpp_x_data_instructions;
int hf_xmpp_muc_user_status;

int hf_xmpp_message;
int hf_xmpp_message_chatstate;

int hf_xmpp_message_thread;
int hf_xmpp_message_thread_parent;

int hf_xmpp_message_body;
int hf_xmpp_message_subject;

int hf_xmpp_ibb_open;
int hf_xmpp_ibb_close;
int hf_xmpp_ibb_data;

int hf_xmpp_delay;

int hf_xmpp_x_event;
int hf_xmpp_x_event_condition;

int hf_xmpp_presence;
int hf_xmpp_presence_show;
int hf_xmpp_presence_status;
int hf_xmpp_presence_caps;

int hf_xmpp_auth;
int hf_xmpp_failure;
int hf_xmpp_failure_text;
int hf_xmpp_starttls;
int hf_xmpp_proceed;
int hf_xmpp_xml_header_version;
int hf_xmpp_stream_end;

int hf_xmpp_muc_x;
int hf_xmpp_muc_user_x;
int hf_xmpp_muc_user_item;
int hf_xmpp_muc_user_invite;

int hf_xmpp_gtalk_session;
int hf_xmpp_gtalk_session_type;
int hf_xmpp_gtalk;
int hf_xmpp_gtalk_setting;
int hf_xmpp_gtalk_setting_element;
int hf_xmpp_gtalk_nosave_x;
int hf_xmpp_gtalk_mail_mailbox;
int hf_xmpp_gtalk_mail_new_mail;
int hf_xmpp_gtalk_transport_p2p;
int hf_xmpp_gtalk_mail_snippet;
int hf_xmpp_gtalk_status_status_list;

int hf_xmpp_conf_info;
int hf_xmpp_conf_info_sid;

int hf_xmpp_unknown;
int hf_xmpp_unknown_attr;

static int hf_xmpp_out;
static int hf_xmpp_in;
int hf_xmpp_response_in;
int hf_xmpp_response_to;
int hf_xmpp_jingle_session;
int hf_xmpp_ibb;

int hf_xmpp_ping;
int hf_xmpp_hashes;

int hf_xmpp_jitsi_inputevt;
int hf_xmpp_jitsi_inputevt_rmt_ctrl;

static int ett_xmpp;
int ett_xmpp_iq;
int ett_xmpp_query;
int ett_xmpp_query_item;
int ett_xmpp_query_identity;
static int ett_xmpp_query_feature;

int ett_xmpp_query_streamhost;
int ett_xmpp_query_streamhost_used;
int ett_xmpp_query_udpsuccess;

static int ett_xmpp_iq_error;
int ett_xmpp_iq_bind;
int ett_xmpp_iq_session;
int ett_xmpp_vcard;
int ett_xmpp_vcard_x_update;

int ett_xmpp_jingle;
int ett_xmpp_jingle_content;
int ett_xmpp_jingle_content_description;
int ett_xmpp_jingle_cont_desc_enc;
int ett_xmpp_jingle_cont_desc_enc_zrtp_hash;
int ett_xmpp_jingle_cont_desc_enc_crypto;
int ett_xmpp_jingle_cont_desc_rtp_hdr;
int ett_xmpp_jingle_cont_desc_bandwidth;
int ett_xmpp_jingle_cont_desc_payload;
int ett_xmpp_jingle_cont_desc_payload_param;
int ett_xmpp_jingle_cont_trans;
int ett_xmpp_jingle_cont_trans_cand;
int ett_xmpp_jingle_cont_trans_rem_cand;
int ett_xmpp_jingle_reason;
int ett_xmpp_jingle_rtp_info;

int ett_xmpp_jingle_file_transfer_offer;
int ett_xmpp_jingle_file_transfer_request;
int ett_xmpp_jingle_file_transfer_abort;
int ett_xmpp_jingle_file_transfer_received;
int ett_xmpp_jingle_file_transfer_checksum;
int ett_xmpp_jingle_file_transfer_file;

int ett_xmpp_services;
int ett_xmpp_services_relay;
int ett_xmpp_channel;

int ett_xmpp_si;
int ett_xmpp_si_file;
int ett_xmpp_si_file_range;

int ett_xmpp_iq_feature_neg;
int ett_xmpp_x_data;
int ett_xmpp_x_data_field;
int ett_xmpp_x_data_field_value;

int ett_xmpp_ibb_open;
int ett_xmpp_ibb_close;
int ett_xmpp_ibb_data;

int ett_xmpp_delay;

int ett_xmpp_x_event;

int ett_xmpp_message;
int ett_xmpp_message_thread;
int ett_xmpp_message_body;
int ett_xmpp_message_subject;

int ett_xmpp_presence;
int ett_xmpp_presence_status;
int ett_xmpp_presence_caps;

int ett_xmpp_auth;
static int ett_xmpp_challenge;
static int ett_xmpp_response;
static int ett_xmpp_success;
int ett_xmpp_failure;
int ett_xmpp_stream;
int ett_xmpp_features;
int ett_xmpp_features_mechanisms;
int ett_xmpp_starttls;
int ett_xmpp_proceed;

int ett_xmpp_muc_x;
int ett_xmpp_muc_hist;
int ett_xmpp_muc_user_x;
int ett_xmpp_muc_user_item;
int ett_xmpp_muc_user_invite;

int ett_xmpp_gtalk_session;
int ett_xmpp_gtalk_session_desc;
int ett_xmpp_gtalk_session_cand;
int ett_xmpp_gtalk_session_desc_payload;
int ett_xmpp_gtalk_session_reason;
int ett_xmpp_gtalk_jingleinfo_stun;
int ett_xmpp_gtalk_jingleinfo_server;
int ett_xmpp_gtalk_jingleinfo_relay;
int ett_xmpp_gtalk_jingleinfo_relay_serv;
int ett_xmpp_gtalk_setting;
int ett_xmpp_gtalk_nosave_x;
int ett_xmpp_gtalk_mail_mailbox;
int ett_xmpp_gtalk_mail_mail_info;
int ett_xmpp_gtalk_mail_senders;
int ett_xmpp_gtalk_mail_sender;
int ett_xmpp_gtalk_status_status_list;
int ett_xmpp_gtalk_transport_p2p;
int ett_xmpp_gtalk_transport_p2p_cand;

int ett_xmpp_conf_info;
int ett_xmpp_conf_desc;
int ett_xmpp_conf_state;
int ett_xmpp_conf_users;
int ett_xmpp_conf_user;
int ett_xmpp_conf_endpoint;
int ett_xmpp_conf_media;

int ett_xmpp_ping;
int ett_xmpp_hashes;
int ett_xmpp_hashes_hash;

int ett_xmpp_jitsi_inputevt;
int ett_xmpp_jitsi_inputevt_rmt_ctrl;

int ett_unknown[ETT_UNKNOWN_LEN];

static expert_field ei_xmpp_xml_disabled;
static expert_field ei_xmpp_packet_unknown;
expert_field ei_xmpp_starttls_missing;
expert_field ei_xmpp_response;
static expert_field ei_xmpp_challenge;
static expert_field ei_xmpp_success;
expert_field ei_xmpp_proceed_already_in_frame;
expert_field ei_xmpp_starttls_already_in_frame;
expert_field ei_xmpp_packet_without_response;
expert_field ei_xmpp_unknown_element;
expert_field ei_xmpp_field_unexpected_value;
expert_field ei_xmpp_unknown_attribute;
expert_field ei_xmpp_required_attribute;

static dissector_handle_t xmpp_handle;

static dissector_handle_t xml_handle;

static void
cleanup_xmpp(void *user_data) {

    xmpp_element_t *root = (xmpp_element_t*)user_data;

    xmpp_element_t_tree_free(root);
}

static int
dissect_xmpp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_) {

    xml_frame_t *xml_frame;
    xml_frame_t *xml_dissector_frame;
    bool         out_packet;

    conversation_t   *conversation;
    xmpp_conv_info_t *xmpp_info;

    proto_tree *xmpp_tree  = NULL;
    proto_item *xmpp_item  = NULL;
    proto_item *outin_item;

    xmpp_element_t *packet = NULL;

    int proto_xml = dissector_handle_get_protocol_index(xml_handle);

    bool whitespace_keepalive = ((tvb_reported_length(tvb) == 1) && tvb_get_uint8(tvb, 0) == ' ');

    /*check if desegment
     * now it checks that last char is '>',
     * TODO checks that first element in packet is closed*/
    int   indx;
    char last_char;

    if (xmpp_desegment && !whitespace_keepalive)
    {
        indx = tvb_reported_length(tvb) - 1;
        if (indx >= 0)
        {
            last_char = tvb_get_uint8(tvb, indx);

            while ((last_char <= ' ') && (indx - 1 >= 0))
            {
                indx--;
                last_char = tvb_get_uint8(tvb, indx);
            }

            if ((indx >= 0) && (last_char != '>'))
            {
                pinfo->desegment_len = DESEGMENT_ONE_MORE_SEGMENT;
                return tvb_captured_length(tvb);
            }
        }
    }

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "XMPP");
    col_clear(pinfo->cinfo, COL_INFO);

    /*if tree == NULL then xmpp_item and xmpp_tree will also NULL*/
    xmpp_item = proto_tree_add_item(tree, proto_xmpp, tvb, 0, -1, ENC_NA);
    xmpp_tree = proto_item_add_subtree(xmpp_item, ett_xmpp);

    if (whitespace_keepalive) {
        /* RFC 6120 section 4.6.1 */
        col_set_str(pinfo->cinfo, COL_INFO, "Whitespace Keepalive");
        return tvb_captured_length(tvb);
    }

    call_dissector_with_data(xml_handle, tvb, pinfo, xmpp_tree, NULL);

    /* If XML dissector is disabled, we can't do much */
    if (!proto_is_protocol_enabled(find_protocol_by_id(proto_xml)))
    {
        col_append_str(pinfo->cinfo, COL_INFO, "(XML dissector disabled, can't dissect XMPP)");
        expert_add_info(pinfo, xmpp_item, &ei_xmpp_xml_disabled);
        return tvb_captured_length(tvb);
    }

    /*if stream end occurs then return*/
    if(xmpp_stream_close(xmpp_tree,tvb, pinfo))
    {
        if(xmpp_tree)
            xmpp_proto_tree_hide_first_child(xmpp_tree);
        return tvb_captured_length(tvb);
    }

    xml_dissector_frame = (xml_frame_t *)p_get_proto_data(pinfo->pool, pinfo, proto_xml, 0);
    if(xml_dissector_frame == NULL)
        return tvb_captured_length(tvb);

    /*data from XML dissector*/
    xml_frame = xml_dissector_frame->first_child;

    if(!xml_frame)
        return tvb_captured_length(tvb);

    conversation = find_or_create_conversation(pinfo);
    xmpp_info = (xmpp_conv_info_t *)conversation_get_proto_data(conversation, proto_xmpp);

    if (!xmpp_info) {
        xmpp_info = wmem_new(wmem_file_scope(), xmpp_conv_info_t);
        xmpp_info->req_resp        = wmem_tree_new(wmem_file_scope());
        xmpp_info->jingle_sessions = wmem_tree_new(wmem_file_scope());
        xmpp_info->ibb_sessions    = wmem_tree_new(wmem_file_scope());
        xmpp_info->gtalk_sessions  = wmem_tree_new(wmem_file_scope());
        xmpp_info->ssl_start   = 0;
        conversation_add_proto_data(conversation, proto_xmpp, (void *) xmpp_info);
    }

    if (pinfo->match_uint == pinfo->destport)
        out_packet = true;
    else
        out_packet = false;

    while(xml_frame)
    {
        packet = xmpp_xml_frame_to_element_t(pinfo, xml_frame, NULL, tvb);
        DISSECTOR_ASSERT(packet);
        CLEANUP_PUSH(cleanup_xmpp, packet);

        if (strcmp(packet->name, "iq") == 0) {
            xmpp_iq_reqresp_track(pinfo, packet, xmpp_info);
            xmpp_jingle_session_track(pinfo, packet, xmpp_info);
            xmpp_gtalk_session_track(pinfo, packet, xmpp_info);
        }

        if (strcmp(packet->name, "iq") == 0 || strcmp(packet->name, "message") == 0) {
            xmpp_ibb_session_track(pinfo, packet, xmpp_info);
        }

        if (out_packet)
            outin_item = proto_tree_add_boolean(xmpp_tree, hf_xmpp_out, tvb, 0, 0, true);
        else
            outin_item = proto_tree_add_boolean(xmpp_tree, hf_xmpp_in, tvb, 0, 0, true);

        proto_item_set_hidden(outin_item);


        /*it hides tree generated by XML dissector*/
        xmpp_proto_tree_hide_first_child(xmpp_tree);

        if (strcmp(packet->name, "iq") == 0) {
            xmpp_iq(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "presence") == 0) {
            xmpp_presence(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "message") == 0) {
            xmpp_message(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "auth") == 0) {
            xmpp_auth(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "challenge") == 0) {
            xmpp_challenge_response_success(xmpp_tree, tvb, pinfo, packet, &ei_xmpp_challenge, ett_xmpp_challenge, "CHALLENGE");
        } else if (strcmp(packet->name, "response") == 0) {
            xmpp_challenge_response_success(xmpp_tree, tvb, pinfo, packet, &ei_xmpp_response, ett_xmpp_response, "RESPONSE");
        } else if (strcmp(packet->name, "success") == 0) {
            xmpp_challenge_response_success(xmpp_tree, tvb, pinfo, packet, &ei_xmpp_success, ett_xmpp_success, "SUCCESS");
        } else if (strcmp(packet->name, "failure") == 0) {
            xmpp_failure(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "xml") == 0) {
            xmpp_xml_header(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "stream") == 0) {
            xmpp_stream(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "features") == 0) {
            xmpp_features(xmpp_tree, tvb, pinfo, packet);
        } else if (strcmp(packet->name, "starttls") == 0) {
            xmpp_starttls(xmpp_tree, tvb, pinfo, packet, xmpp_info);
        } else if (strcmp(packet->name, "proceed") == 0) {
            xmpp_proceed(xmpp_tree, tvb, pinfo, packet, xmpp_info);
        } else {
            xmpp_proto_tree_show_first_child(xmpp_tree);
            expert_add_info_format(pinfo, xmpp_tree, &ei_xmpp_packet_unknown, "Unknown packet: %s", packet->name);
            col_set_str(pinfo->cinfo, COL_INFO, "UNKNOWN PACKET ");
        }

        /*appends to COL_INFO information about src or dst*/
        if (pinfo->match_uint == pinfo->destport) {
            xmpp_attr_t *to = xmpp_get_attr(packet, "to");
            if (to)
                col_append_fstr(pinfo->cinfo, COL_INFO, "> %s ", to->value);
        } else {
            xmpp_attr_t *from = xmpp_get_attr(packet, "from");
            if (from)
                col_append_fstr(pinfo->cinfo, COL_INFO, "< %s ", from->value);
        }

        CLEANUP_CALL_AND_POP;
        xml_frame = xml_frame->next_sibling;
    }
    return tvb_captured_length(tvb);
}


void
proto_register_xmpp(void) {
    static hf_register_info hf[] = {
        { &hf_xmpp_iq,
          {
              "IQ", "xmpp.iq", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq packet", HFILL
          }},
        {&hf_xmpp_xmlns,
         {
             "xmlns", "xmpp.xmlns", FT_STRING, BASE_NONE, NULL, 0x0,
             "element namespace", HFILL
         }},
        {&hf_xmpp_cdata,
         {
             "CDATA", "xmpp.cdata", FT_STRING, BASE_NONE, NULL, 0x0,
             NULL, HFILL
         }},
        {&hf_xmpp_attribute,
         {
             "Attribute", "xmpp.attribute", FT_STRING, BASE_NONE, NULL, 0x0,
             NULL, HFILL
         }},
        { &hf_xmpp_id,
          {
              "id", "xmpp.id", FT_STRING, BASE_NONE, NULL, 0x0,
              "packet id", HFILL
          }},
        { &hf_xmpp_type,
          {
              "type", "xmpp.type", FT_STRING, BASE_NONE, NULL, 0x0,
              "packet type", HFILL
          }},
        { &hf_xmpp_from,
          {
              "from", "xmpp.from", FT_STRING, BASE_NONE, NULL, 0x0,
              "packet from", HFILL
          }},
        { &hf_xmpp_to,
          {
              "to", "xmpp.to", FT_STRING, BASE_NONE, NULL, 0x0,
              "packet to", HFILL
          }},
        { &hf_xmpp_query,
          {
              "QUERY", "xmpp.query", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq query", HFILL
          }},
        { &hf_xmpp_query_node,
          {
              "node", "xmpp.query.node", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query node", HFILL
          }},
        { &hf_xmpp_query_item,
          {
              "ITEM", "xmpp.query.item", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq query item", HFILL

          }},
        { &hf_xmpp_query_item_jid,
          {
              "jid", "xmpp.query.item.jid", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query item jid", HFILL

          }},
        { &hf_xmpp_query_item_name,
          {
              "name", "xmpp.query.item.name", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query item name", HFILL
          }},
        { &hf_xmpp_query_item_subscription,
          {
              "subscription", "xmpp.query.item.subscription", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query item subscription", HFILL
          }},
        { &hf_xmpp_query_item_ask,
          {
              "ask", "xmpp.query.item.ask", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query item ask", HFILL
          }},
        { &hf_xmpp_query_item_group,
          {
              "GROUP", "xmpp.query.item.group", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query item group", HFILL

          }},
        { &hf_xmpp_query_item_approved,
          {
              "approved", "xmpp.query.item.approved", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query item approved", HFILL

          }},
        { &hf_xmpp_query_item_node,
          {
              "node", "xmpp.query.item.node", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query item node", HFILL

          }},
        { &hf_xmpp_query_identity,
          {
              "IDENTITY", "xmpp.query.identity", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq query identity", HFILL

          }},
        { &hf_xmpp_query_identity_category,
          {
              "category", "xmpp.query.identity.category", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query identity category", HFILL

          }},
        { &hf_xmpp_query_identity_type,
          {
              "type", "xmpp.query.identity.type", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query identity type", HFILL

          }},
        { &hf_xmpp_query_identity_name,
          {
              "name", "xmpp.query.identity.name", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query identity name", HFILL

          }},
        { &hf_xmpp_query_identity_lang,
          {
              "lang", "xmpp.query.identity.lang", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query identity lang", HFILL

          }},
        { &hf_xmpp_query_feature,
          {
              "FEATURE", "xmpp.query.feature", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query feature", HFILL

          }},
        { &hf_xmpp_query_streamhost,
          {
              "STREAMHOST", "xmpp.query.streamhost", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq query streamhost", HFILL

          }},
        { &hf_xmpp_query_streamhost_used,
          {
              "STREAMHOST-USED", "xmpp.query.streamhost-used", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq query streamhost-used", HFILL

          }},
        { &hf_xmpp_query_activate,
          {
              "ACTIVATE", "xmpp.query.activate", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq query activate", HFILL

          }},
        { &hf_xmpp_query_udpsuccess,
          {
              "UDPSUCCESS", "xmpp.query.udpsuccess", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq query streamhost-used", HFILL

          }},
        { &hf_xmpp_error,
          {
              "ERROR", "xmpp.error", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq error", HFILL
          }},
        { &hf_xmpp_error_code,
          {
              "code", "xmpp.error.code", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq stanza error code", HFILL

          }},
        { &hf_xmpp_error_type,
          {
              "type", "xmpp.error.type", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq error type", HFILL

          }},
        { &hf_xmpp_error_condition,
          {
              "CONDITION", "xmpp.error.condition", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq error condition", HFILL

          }},
        { &hf_xmpp_error_text,
          {
              "TEXT", "xmpp.error.text", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq error text", HFILL

          }},
        { &hf_xmpp_iq_bind,
          {
              "BIND", "xmpp.iq.bind", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq bind", HFILL

          }},
        { &hf_xmpp_iq_bind_jid,
          {
              "jid", "xmpp.iq.bind.jid", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq bind jid", HFILL

          }},
        { &hf_xmpp_iq_bind_resource,
          {
              "resource", "xmpp.iq.bind.resource", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq bind resource", HFILL

          }},
        { &hf_xmpp_services,
          {
              "SERVICES", "xmpp.services", FT_NONE, BASE_NONE, NULL, 0x0,
              "http://jabber.org/protocol/jinglenodes services", HFILL
          }},
        { &hf_xmpp_channel,
          {
              "CHANNEL", "xmpp.channel", FT_NONE, BASE_NONE, NULL, 0x0,
              "http://jabber.org/protocol/jinglenodes#channel", HFILL
          }},
        { &hf_xmpp_iq_session,
          {
              "SESSION", "xmpp.iq.session", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq session", HFILL
          }},
        { &hf_xmpp_vcard,
          {
              "VCARD", "xmpp.vcard", FT_NONE, BASE_NONE, NULL, 0x0,
              "vcard-temp", HFILL
          }},
        { &hf_xmpp_vcard_x_update,
          {
              "X VCARD-UPDATE", "xmpp.vcard-update", FT_NONE, BASE_NONE, NULL, 0x0,
              "vcard-temp:x:update", HFILL
          }},
        { &hf_xmpp_jingle,
          {
              "JINGLE", "xmpp.jingle", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle", HFILL
          }},
        { &hf_xmpp_jingle_action,
          {
              "action", "xmpp.jingle.action", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle action", HFILL
          }},
        { &hf_xmpp_jingle_sid,
          {
              "sid", "xmpp.jingle.sid", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle sid", HFILL
          }},
        { &hf_xmpp_jingle_initiator,
          {
              "initiator", "xmpp.jingle.initiator", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle initiator", HFILL
          }},
        { &hf_xmpp_jingle_responder,
          {
              "responder", "xmpp.jingle.responder", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle responder", HFILL
          }},
        { &hf_xmpp_jingle_content,
          {
              "CONTENT", "xmpp.jingle.content", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content", HFILL
          }},
        { &hf_xmpp_jingle_content_creator,
          {
              "creator", "xmpp.jingle.content.creator", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content creator", HFILL
          }},
        { &hf_xmpp_jingle_content_name,
          {
              "name", "xmpp.jingle.content.name", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content name", HFILL
          }},
        { &hf_xmpp_jingle_content_disposition,
          {
              "disposition", "xmpp.jingle.content.disposition", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content disposition", HFILL
          }},
        { &hf_xmpp_jingle_content_senders,
          {
              "senders", "xmpp.jingle.content.senders", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content senders", HFILL
          }},
        { &hf_xmpp_jingle_content_description,
          {
              "DESCRIPTION", "xmpp.jingle.content.description", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description", HFILL
          }},
        { &hf_xmpp_jingle_content_description_media,
          {
              "media", "xmpp.jingle.content.description.media", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description", HFILL
          }},
        { &hf_xmpp_jingle_content_description_ssrc,
          {
              "ssrc", "xmpp.jingle.content.description.ssrc", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description ssrc", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload,
          {
              "PAYLOAD-TYPE", "xmpp.jingle.content.description.payload-type", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_id,
          {
              "id", "xmpp.jingle.content.description.payload-type.id", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type id", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_channels,
          {
              "channels", "xmpp.jingle.content.description.payload-type.channels", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type channels", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_clockrate,
          {
              "clockrate", "xmpp.jingle.content.description.payload-type.clockrate", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type clockrate", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_maxptime,
          {
              "maxptime", "xmpp.jingle.content.description.payload-type.maxptime", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type maxptime", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_name,
          {
              "name", "xmpp.jingle.content.description.payload-type.name", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type name", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_ptime,
          {
              "ptime", "xmpp.jingle.content.description.payload-type.ptime", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type ptime", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_param,
          {
              "PARAMETER", "xmpp.jingle.content.description.payload-type.parameter", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type parameter", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_param_name,
          {
              "name", "xmpp.jingle.content.description.payload-type.parameter.name", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type parameter name", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_payload_param_value,
          {
              "value", "xmpp.jingle.content.description.payload-type.parameter.value", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content description payload-type parameter value", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans,
          {
              "TRANSPORT", "xmpp.jingle.content.transport", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content transport", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_ufrag,
          {
              "ufrag", "xmpp.jingle.content.transport.ufrag", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content transport ufrag", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_pwd,
          {
              "pwd", "xmpp.jingle.content.transport.pwd", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle content transport pwd", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_cand,
          {
              "CANDIDATE", "xmpp.jingle.content.transport.candidate", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content transport candidate", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_rem_cand,
          {
              "REMOTE-CANDIDATE", "xmpp.jingle.content.transport.remote-candidate", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content transport remote-candidate", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_activated,
          {
              "ACTIVATED", "xmpp.jingle.content.transport.activated", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:transports:s5b:1 activated", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_candidate_used,
          {
              "CANDIDATE-USED", "xmpp.jingle.content.transport.candidate-used", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:transports:s5b:1 candidate-used", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_candidate_error,
          {
              "CANDIDATE-ERROR", "xmpp.jingle.content.transport.candidate-error", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:transports:s5b:1 candidate-error", HFILL
          }},
        { &hf_xmpp_jingle_cont_trans_proxy_error,
          {
              "PROXY-ERROR", "xmpp.jingle.content.transport.proxy-error", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:transports:s5b:1 proxy-error", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_enc,
          {
              "ENCRYPTION", "xmpp.jingle.content.description.encryption", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description encryption", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_enc_zrtp_hash,
          {
              "ZRTP-HASH", "xmpp.jingle.content.description.encryption.zrtp-hash", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description encryption zrtp-hash", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_enc_crypto,
          {
              "CRYPTO", "xmpp.jingle.content.description.encryption.crypto", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description encryption crypto", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_bandwidth,
          {
              "BANDWIDTH", "xmpp.jingle.content.description.bandwidth", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description bandwidth", HFILL
          }},
        { &hf_xmpp_jingle_cont_desc_rtp_hdr,
          {
              "RTP-HDREXT", "xmpp.jingle.content.description.rtp-hdrext", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle content description rtp-hdrext", HFILL
          }},
        { &hf_xmpp_jingle_reason,
          {
              "REASON", "xmpp.jingle.reason", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq jingle reason", HFILL
          }},
        { &hf_xmpp_jingle_reason_condition,
          {
              "CONDITION", "xmpp.jingle.reason.condition", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle reason condition", HFILL
          }},
        { &hf_xmpp_jingle_reason_text,
          {
              "TEXT", "xmpp.jingle.reason.text", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle reason text", HFILL
          }},
        { &hf_xmpp_jingle_rtp_info,
          {
              "RTP-INFO", "xmpp.jingle.rtp_info", FT_STRING, BASE_NONE, NULL, 0x0,
              "iq jingle rtp-info(ringing, active, hold, mute, ...)", HFILL
          }},
        { &hf_xmpp_jingle_file_transfer_offer,
          {
              "OFFER", "xmpp.jingle.content.description.offer", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:apps:file-transfer:3 offer", HFILL
          }},
        { &hf_xmpp_jingle_file_transfer_request,
          {
              "REQUEST", "xmpp.jingle.content.description.request", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:apps:file-transfer:3 request", HFILL
          }},
        { &hf_xmpp_jingle_file_transfer_received,
          {
              "RECEIVED", "xmpp.jingle.content.received", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:apps:file-transfer:3 received", HFILL
          }},
        { &hf_xmpp_jingle_file_transfer_abort,
          {
              "ABORT", "xmpp.jingle.content.abort", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:apps:file-transfer:3 abort", HFILL
          }},
        { &hf_xmpp_jingle_file_transfer_checksum,
          {
              "CHECKSUM", "xmpp.jingle.content.checksum", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:jingle:apps:file-transfer:3 checksum", HFILL
          }},
        { &hf_xmpp_si,
          {
              "SI", "xmpp.si", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq si", HFILL
          }},
        { &hf_xmpp_si_file,
          {
              "FILE", "xmpp.si.file", FT_NONE, BASE_NONE, NULL, 0x0,
              "iq si file", HFILL
          }},
        { &hf_xmpp_iq_feature_neg,
          {
              "FEATURE", "xmpp.feature-neg", FT_NONE, BASE_NONE, NULL, 0x0,
              "http://jabber.org/protocol/feature-neg", HFILL
          }},
        { &hf_xmpp_x_data,
          {
              "X-DATA", "xmpp.x-data", FT_NONE, BASE_NONE, NULL, 0x0,
              "jabber:x:data", HFILL
          }},
        { &hf_xmpp_x_data_field,
          {
              "FIELD", "xmpp.x-data.field", FT_NONE, BASE_NONE, NULL, 0x0,
              "jabber:x:data field", HFILL
          }},
        { &hf_xmpp_x_data_field_value,
          {
              "VALUE", "xmpp.x-data.field.value", FT_NONE, BASE_NONE, NULL, 0x0,
              "jabber:x:data field value", HFILL
          }},
        { &hf_xmpp_x_data_instructions,
          {
              "INSTRUCTIONS", "xmpp.x-data.instructions", FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_muc_user_status,
          {
              "STATUS", "xmpp.muc_user_status", FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_delay,
          {
              "DELAY", "xmpp.delay", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:delay", HFILL
          }},
        { &hf_xmpp_x_event,
          {
              "X EVENT", "xmpp.x-event", FT_NONE, BASE_NONE, NULL, 0x0,
              "jabber:x:event", HFILL
          }},
        { &hf_xmpp_x_event_condition,
          {
              "CONDITION", "xmpp.x-event.condition", FT_STRING, BASE_NONE, NULL, 0x0,
              "jabber:x:event condition", HFILL
          }},
        { &hf_xmpp_presence,
          {
              "PRESENCE", "xmpp.presence", FT_NONE, BASE_NONE, NULL, 0x0,
              "presence packet", HFILL
          }},
        { &hf_xmpp_presence_show,
          {
              "SHOW", "xmpp.presence.show", FT_STRING, BASE_NONE, NULL, 0x0,
              "presence show", HFILL
          }},
        { &hf_xmpp_presence_status,
          {
              "STATUS", "xmpp.presence.status", FT_NONE, BASE_NONE, NULL, 0x0,
              "presence status", HFILL
          }},
        { &hf_xmpp_presence_caps,
          {
              "CAPS", "xmpp.presence.caps", FT_NONE, BASE_NONE, NULL, 0x0,
              "presence caps", HFILL
          }},
        { &hf_xmpp_message,
          {
              "MESSAGE", "xmpp.message", FT_NONE, BASE_NONE, NULL, 0x0,
              "message packet", HFILL
          }},
        { &hf_xmpp_message_chatstate,
          {
              "CHATSTATE", "xmpp.message.chatstate", FT_STRING, BASE_NONE, NULL, 0x0,
              "message chatstate", HFILL
          }},
        { &hf_xmpp_message_thread,
          {
              "THREAD", "xmpp.message.thread", FT_NONE, BASE_NONE, NULL, 0x0,
              "message thread", HFILL
          }},
        { &hf_xmpp_message_body,
          {
              "BODY", "xmpp.message.body", FT_NONE, BASE_NONE, NULL, 0x0,
              "message body", HFILL
          }},
        { &hf_xmpp_message_subject,
          {
              "SUBJECT", "xmpp.message.subject", FT_NONE, BASE_NONE, NULL, 0x0,
              "message subject", HFILL
          }},
        { &hf_xmpp_message_thread_parent,
          {
              "parent", "xmpp.message.thread.parent", FT_STRING, BASE_NONE, NULL, 0x0,
              "message thread parent", HFILL
          }},
        { &hf_xmpp_auth,
          {
              "AUTH", "xmpp.auth", FT_NONE, BASE_NONE, NULL, 0x0,
              "auth packet", HFILL
          }},
        { &hf_xmpp_stream,
          {
              "STREAM", "xmpp.stream", FT_NONE, BASE_NONE, NULL, 0x0,
              "XMPP stream", HFILL
          }},
        { &hf_xmpp_failure,
          {
              "FAILURE", "xmpp.failure", FT_NONE, BASE_NONE, NULL, 0x0,
              "failure packet", HFILL
          }},
        { &hf_xmpp_failure_text,
          {
              "FAILURE TEXT", "xmpp.failure_text", FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_xml_header_version,
          {
              "XML HEADER VER", "xmpp.xml_header_version", FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_stream_end,
          {
              "STREAM END", "xmpp.stream_end", FT_NONE, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_features,
          {
              "FEATURES", "xmpp.features", FT_NONE, BASE_NONE, NULL, 0x0,
              "stream features", HFILL
          }},
        { &hf_xmpp_starttls,
          {
              "STARTTLS", "xmpp.starttls", FT_NONE, BASE_NONE, NULL, 0x0,
              "starttls packet", HFILL
          }},
        { &hf_xmpp_proceed,
          {
              "PROCEED", "xmpp.proceed", FT_NONE, BASE_NONE, NULL, 0x0,
              "proceed packet", HFILL
          }},
        { &hf_xmpp_unknown,
          {
              "UNKNOWN", "xmpp.unknown", FT_STRING, BASE_NONE, NULL, 0x0,
              "unknown element", HFILL
          }},
        { &hf_xmpp_unknown_attr,
          {
              "UNKNOWN ATTR", "xmpp.unknown_attr", FT_STRING, BASE_NONE, NULL, 0x0,
              "unknown attribute", HFILL
          }},
        { &hf_xmpp_ibb_open,
          {
              "IBB-OPEN", "xmpp.ibb.open", FT_NONE, BASE_NONE, NULL, 0x0,
              "xmpp ibb open", HFILL
          }},
        { &hf_xmpp_ibb_close,
          {
              "IBB-CLOSE", "xmpp.ibb.close", FT_NONE, BASE_NONE, NULL, 0x0,
              "xmpp ibb close", HFILL
          }},
        { &hf_xmpp_ibb_data,
          {
              "IBB-DATA", "xmpp.ibb.data", FT_NONE, BASE_NONE, NULL, 0x0,
              "xmpp ibb data", HFILL
          }},
        { &hf_xmpp_muc_x,
          {
              "X MUC", "xmpp.muc-x", FT_NONE, BASE_NONE, NULL, 0x0,
              "http://jabber.org/protocol/muc", HFILL
          }},
        { &hf_xmpp_muc_user_x,
          {
              "X MUC-USER", "xmpp.muc-user-x", FT_NONE, BASE_NONE, NULL, 0x0,
              "http://jabber.org/protocol/muc#user", HFILL
          }},
        { &hf_xmpp_muc_user_item,
          {
              "ITEM", "xmpp.muc-user-x.item", FT_NONE, BASE_NONE, NULL, 0x0,
              "muc#user item", HFILL
          }},
        { &hf_xmpp_muc_user_invite,
          {
              "INVITE", "xmpp.muc-user-x.invite", FT_NONE, BASE_NONE, NULL, 0x0,
              "muc#user invite", HFILL
          }},
        { &hf_xmpp_gtalk_session,
          {
              "GTALK-SESSION", "xmpp.gtalk.session", FT_NONE, BASE_NONE, NULL, 0x0,
              "GTalk session", HFILL
          }},
        { &hf_xmpp_gtalk_session_type,
          {
              "type", "xmpp.gtalk.session.type", FT_STRING, BASE_NONE, NULL, 0x0,
              "GTalk session type", HFILL
          }},
        { &hf_xmpp_gtalk,
          {
              "GTALK SESSION", "xmpp.gtalk", FT_STRING, BASE_NONE, NULL, 0x0,
              "GTalk SID", HFILL
          }},
        { &hf_xmpp_gtalk_setting,
          {
              "USERSETTING", "xmpp.gtalk.setting", FT_NONE, BASE_NONE, NULL, 0x0,
              "google:setting usersetting", HFILL
          }},
        { &hf_xmpp_gtalk_setting_element,
          {
              "USERSETTING ELEMENT", "xmpp.gtalk.setting_element", FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_gtalk_nosave_x,
          {
              "X-NOSAVE", "xmpp.gtalk.nosave.x", FT_NONE, BASE_NONE, NULL, 0x0,
              "google:nosave x", HFILL
          }},
        { &hf_xmpp_gtalk_mail_mailbox,
          {
              "MAILBOX", "xmpp.gtalk.mailbox", FT_NONE, BASE_NONE, NULL, 0x0,
              "google:mail:notify mailbox", HFILL
          }},
        { &hf_xmpp_gtalk_mail_new_mail,
          {
              "NEW MAIL", "xmpp.gtalk.new-mail", FT_NONE, BASE_NONE, NULL, 0x0,
              "google:mail:notify new-mail", HFILL
          }},
        { &hf_xmpp_gtalk_transport_p2p,
          {
              "TRANSPORT", "xmpp.gtalk.transport-p2p", FT_NONE, BASE_NONE, NULL, 0x0,
              "google/transport/p2p", HFILL
          }},
        { &hf_xmpp_gtalk_mail_snippet,
          {
              "SNIPPET", "xmpp.gtalk.mail_snippet", FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_gtalk_status_status_list,
          {
              "STATUS", "xmpp.gtalk.status_status_list", FT_STRING, BASE_NONE, NULL, 0x0,
              NULL, HFILL
          }},
        { &hf_xmpp_conf_info,
          {
              "CONFERENCE INFO", "xmpp.conf-info", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:ietf:params:xml:ns:conference-info", HFILL
          }},
        { &hf_xmpp_conf_info_sid,
          {
              "sid", "xmpp.conf-info.sid", FT_STRING, BASE_NONE, NULL, 0x0,
              "urn:ietf:params:xml:ns:conference-info sid", HFILL
          }},
        { &hf_xmpp_response_in,
          { "Response In", "xmpp.response_in",
            FT_FRAMENUM, BASE_NONE, FRAMENUM_TYPE(FT_FRAMENUM_RESPONSE), 0x0,
            "The response to this request is in this frame", HFILL }
        },
        { &hf_xmpp_response_to,
          { "Request In", "xmpp.response_to",
            FT_FRAMENUM, BASE_NONE, FRAMENUM_TYPE(FT_FRAMENUM_REQUEST), 0x0,
            "This is a response to the request in this frame", HFILL }
        },
        { &hf_xmpp_out,
          {
              "Out", "xmpp.out", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
              "Outgoing packet", HFILL
          }},
        { &hf_xmpp_in,
          {
              "In", "xmpp.in", FT_BOOLEAN, BASE_NONE, NULL, 0x0,
              "Ingoing packet", HFILL
          }},
        { &hf_xmpp_ibb,
          {
              "IBB SESSION", "xmpp.ibb", FT_STRING, BASE_NONE, NULL, 0x0,
              "In-Band Bytestreams session", HFILL
          }},
        { &hf_xmpp_jingle_session,
          {
              "JINGLE SESSION", "xmpp.jingle_session", FT_STRING, BASE_NONE, NULL, 0x0,
              "Jingle SID", HFILL
          }},
        { &hf_xmpp_ping,
          {
              "PING", "xmpp.ping", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:ping", HFILL
          }},
        { &hf_xmpp_hashes,
          {
              "HASHES", "xmpp.hashes", FT_NONE, BASE_NONE, NULL, 0x0,
              "urn:xmpp:hashes:0", HFILL
          }},
        { &hf_xmpp_jitsi_inputevt,
          {
              "INPUTEVT", "xmpp.inputevt", FT_NONE, BASE_NONE, NULL, 0x0,
              "http://jitsi.org/protocol/inputevt", HFILL
          }},
        { &hf_xmpp_jitsi_inputevt_rmt_ctrl,
          {
              "REMOTE-CONTROL", "xmpp.inputevt.remote-control", FT_NONE, BASE_NONE, NULL, 0x0,
              "http://jitsi.org/protocol/inputevt remote-control", HFILL
          }},
    };

    static int * ett[] = {
        &ett_xmpp,
        &ett_xmpp_iq,
        &ett_xmpp_query,
        &ett_xmpp_query_item,
        &ett_xmpp_query_identity,
        &ett_xmpp_query_feature,
        &ett_xmpp_query_streamhost,
        &ett_xmpp_query_streamhost_used,
        &ett_xmpp_query_udpsuccess,
        &ett_xmpp_iq_error,
        &ett_xmpp_iq_bind,
        &ett_xmpp_iq_session,
        &ett_xmpp_vcard,
        &ett_xmpp_vcard_x_update,
        &ett_xmpp_jingle,
        &ett_xmpp_jingle_content,
        &ett_xmpp_jingle_content_description,
        &ett_xmpp_jingle_cont_desc_payload,
        &ett_xmpp_jingle_cont_desc_payload_param,
        &ett_xmpp_jingle_cont_desc_enc,
        &ett_xmpp_jingle_cont_desc_enc_zrtp_hash,
        &ett_xmpp_jingle_cont_desc_enc_crypto,
        &ett_xmpp_jingle_cont_desc_bandwidth,
        &ett_xmpp_jingle_cont_desc_rtp_hdr,
        &ett_xmpp_jingle_cont_trans,
        &ett_xmpp_jingle_cont_trans_cand,
        &ett_xmpp_jingle_cont_trans_rem_cand,
        &ett_xmpp_jingle_reason,
        &ett_xmpp_jingle_rtp_info,
        &ett_xmpp_services,
        &ett_xmpp_services_relay,
        &ett_xmpp_channel,
        &ett_xmpp_si,
        &ett_xmpp_si_file,
        &ett_xmpp_si_file_range,
        &ett_xmpp_iq_feature_neg,
        &ett_xmpp_x_data,
        &ett_xmpp_x_data_field,
        &ett_xmpp_x_data_field_value,
        &ett_xmpp_ibb_open,
        &ett_xmpp_ibb_close,
        &ett_xmpp_ibb_data,
        &ett_xmpp_delay,
        &ett_xmpp_x_event,
        &ett_xmpp_message,
        &ett_xmpp_message_thread,
        &ett_xmpp_message_subject,
        &ett_xmpp_message_body,
        &ett_xmpp_presence,
        &ett_xmpp_presence_status,
        &ett_xmpp_presence_caps,
        &ett_xmpp_auth,
        &ett_xmpp_challenge,
        &ett_xmpp_response,
        &ett_xmpp_success,
        &ett_xmpp_failure,
        &ett_xmpp_muc_x,
        &ett_xmpp_muc_hist,
        &ett_xmpp_muc_user_x,
        &ett_xmpp_muc_user_item,
        &ett_xmpp_muc_user_invite,
        &ett_xmpp_gtalk_session,
        &ett_xmpp_gtalk_session_desc,
        &ett_xmpp_gtalk_session_desc_payload,
        &ett_xmpp_gtalk_session_cand,
        &ett_xmpp_gtalk_session_reason,
        &ett_xmpp_gtalk_jingleinfo_stun,
        &ett_xmpp_gtalk_jingleinfo_server,
        &ett_xmpp_gtalk_jingleinfo_relay,
        &ett_xmpp_gtalk_jingleinfo_relay_serv,
        &ett_xmpp_gtalk_setting,
        &ett_xmpp_gtalk_nosave_x,
        &ett_xmpp_gtalk_mail_mailbox,
        &ett_xmpp_gtalk_mail_mail_info,
        &ett_xmpp_gtalk_mail_senders,
        &ett_xmpp_gtalk_mail_sender,
        &ett_xmpp_gtalk_status_status_list,
        &ett_xmpp_conf_info,
        &ett_xmpp_conf_desc,
        &ett_xmpp_conf_state,
        &ett_xmpp_conf_users,
        &ett_xmpp_conf_user,
        &ett_xmpp_conf_endpoint,
        &ett_xmpp_conf_media,
        &ett_xmpp_gtalk_transport_p2p,
        &ett_xmpp_gtalk_transport_p2p_cand,
        &ett_xmpp_ping,
        &ett_xmpp_hashes_hash,
        &ett_xmpp_hashes,
        &ett_xmpp_jingle_file_transfer_offer,
        &ett_xmpp_jingle_file_transfer_request,
        &ett_xmpp_jingle_file_transfer_received,
        &ett_xmpp_jingle_file_transfer_abort,
        &ett_xmpp_jingle_file_transfer_checksum,
        &ett_xmpp_jingle_file_transfer_file,
        &ett_xmpp_jitsi_inputevt,
        &ett_xmpp_jitsi_inputevt_rmt_ctrl,
        &ett_xmpp_stream,
        &ett_xmpp_features,
        &ett_xmpp_features_mechanisms,
        &ett_xmpp_starttls,
        &ett_xmpp_proceed,
    };

    static ei_register_info ei[] = {
        { &ei_xmpp_xml_disabled, { "xmpp.xml_disabled", PI_UNDECODED, PI_WARN, "XML dissector disabled, can't dissect XMPP", EXPFILL }},
        { &ei_xmpp_packet_unknown, { "xmpp.packet_unknown", PI_UNDECODED, PI_NOTE, "Unknown packet", EXPFILL }},
        { &ei_xmpp_packet_without_response, { "xmpp.packet_without_response", PI_PROTOCOL, PI_CHAT, "Packet without response", EXPFILL }},
        { &ei_xmpp_response, { "xmpp.response", PI_RESPONSE_CODE, PI_CHAT, "RESPONSE", EXPFILL }},
        { &ei_xmpp_challenge, { "xmpp.challenge", PI_RESPONSE_CODE, PI_CHAT, "CHALLENGE", EXPFILL }},
        { &ei_xmpp_success, { "xmpp.success", PI_RESPONSE_CODE, PI_CHAT, "SUCCESS", EXPFILL }},
        { &ei_xmpp_starttls_already_in_frame, { "xmpp.starttls.already_in_frame", PI_PROTOCOL, PI_WARN, "Already saw STARTTLS in frame X", EXPFILL }},
        { &ei_xmpp_starttls_missing, { "xmpp.starttls.missing", PI_PROTOCOL, PI_WARN, "Haven't seen a STARTTLS, did the capture start in the middle of a session?", EXPFILL }},
        { &ei_xmpp_proceed_already_in_frame, { "xmpp.proceed.already_in_frame", PI_PROTOCOL, PI_WARN, "Already saw PROCEED in frame X", EXPFILL }},
        { &ei_xmpp_unknown_element, { "xmpp.unknown.element", PI_UNDECODED, PI_NOTE, "Unknown element", EXPFILL }},
        { &ei_xmpp_unknown_attribute, { "xmpp.unknown.attribute", PI_UNDECODED, PI_NOTE, "Unknown attribute", EXPFILL }},
        { &ei_xmpp_required_attribute, { "xmpp.required_attribute", PI_PROTOCOL, PI_WARN, "Required attribute doesn't appear", EXPFILL }},
        { &ei_xmpp_field_unexpected_value, { "xmpp.field.unexpected_value", PI_PROTOCOL, PI_WARN, "Field has unexpected value", EXPFILL }},
    };

    module_t *xmpp_module;
    expert_module_t* expert_xmpp;

    static int* ett_unknown_ptr[ETT_UNKNOWN_LEN];
    int i;
    for(i=0;i<ETT_UNKNOWN_LEN;i++)
    {
        ett_unknown_ptr[i] = &ett_unknown[i];
    }

    proto_xmpp = proto_register_protocol("XMPP Protocol", "XMPP", "xmpp");

    xmpp_module = prefs_register_protocol(proto_xmpp, NULL);
    prefs_register_bool_preference(xmpp_module, "desegment",
                                   "Reassemble XMPP messages",
                                   "Whether the XMPP dissector should reassemble messages. "
                                   "To use this option, you must also enable"
                                   " \"Allow subdissectors to reassemble TCP streams\""
                                   " in the TCP protocol settings",
                                   &xmpp_desegment);

    proto_register_field_array(proto_xmpp, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    proto_register_subtree_array(ett_unknown_ptr, array_length(ett_unknown_ptr));
    expert_xmpp = expert_register_protocol(proto_xmpp);
    expert_register_field_array(expert_xmpp, ei, array_length(ei));

    xmpp_handle = register_dissector("xmpp", dissect_xmpp, proto_xmpp);

    xmpp_init_parsers();
}

void
proto_reg_handoff_xmpp(void) {
    xml_handle  = find_dissector_add_dependency("xml", proto_xmpp);

    dissector_add_uint_with_preference("tcp.port", XMPP_PORT, xmpp_handle);

}
/*
* Editor modelines - https://www.wireshark.org/tools/modelines.html
*
* Local variables:
* c-basic-offset: 4
* tab-width: 8
* indent-tabs-mode: nil
* End:
*
* ex: set shiftwidth=4 tabstop=8 expandtab:
* :indentSize=4:tabSize=8:noTabs=true:
*/
