/* packet-armagetronad.c
 * Routines for the Armagetronad packet dissection
 * Copyright 2005, Guillaume Chazarain <guichaz@yahoo.fr>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <epan/packet.h>
void proto_register_armagetronad(void);
void proto_reg_handoff_armagetronad(void);

/* Initialize the protocol and registered fields */
static int proto_armagetronad;
static int hf_armagetronad_descriptor_id;
static int hf_armagetronad_message_id;
static int hf_armagetronad_data_len;
static int hf_armagetronad_data;
static int hf_armagetronad_sender_id;
static int hf_armagetronad_msg_subtree;

/* Initialize the subtree pointers */
static int ett_armagetronad;
static int ett_message;

static dissector_handle_t armagetronad_handle;

#define ARMAGETRONAD_UDP_PORT_RANGE "4533-4534" /* 4533 is not IANA registered, 4534 is */

/*
 * The ACK packet is so common that we treat it
 * differently: it has no MessageID
 */
#define ACK 1

/*
 * armagetronad-0.2.8.2.1
 * The numbers and names were retrieved at runtime using the
 * 'nDescriptor* descriptors[MAXDESCRIPTORS]' array
 */
static const value_string descriptors[] = {
	{  1, "ack"},
	{  2, "req_info"},
	{  3, "login_deny"},
	{  4, "login_ignore"},
	{  5, "login_accept"},
	{  6, "login1"},
	{  7, "logout"},
	{  8, "sn_ConsoleOut"},
	{  9, "client_cen"},
	{ 10, "version"},
	{ 11, "login2"},
	{ 20, "req_id"},
	{ 21, "id_req_handler"},
	{ 22, "net_destroy"},
	{ 23, "net_control"},
	{ 24, "net_sync"},
	{ 25, "ready to get objects"},
	{ 26, "net_clear"},
	{ 27, "sync_ack"},
	{ 28, "sync_msg"},
	{ 40, "password_request"},
	{ 41, "password_answer"},
	{ 50, "small_server"},
	{ 51, "big_server"},
	{ 52, "small_request"},
	{ 53, "big_request"},
	{ 54, "big_server_master"},
	{ 55, "big_request_master"},
	{ 60, "transfer config"},
	{200, "Chat"},
	{201, "ePlayerNetID"},
	{202, "player_removed_from_game"},
	{203, "Chat Client"},
	{210, "eTimer"},
	{220, "eTeam"},
	{230, "vote cast"},
	{231, "Kick vote"},
	{232, "Server controlled vote"},
	{233, "Server controlled vote expired"},
	{300, "gNetPlayerWall"},
	{310, "game"},
	{311, "client_gamestate"},
	{320, "cycle"},
	{321, "destination"},
	{330, "gAIPlayer"},
	{331, "gAITeam"},
	{340, "zone"},
	{0, NULL}
};

static bool
is_armagetronad_packet(tvbuff_t * tvb)
{
	int offset = 0;

	/* For each message in the frame */
	while (tvb_captured_length_remaining(tvb, offset) > 2) {
		int data_len = tvb_get_ntohs(tvb, offset + 4) * 2;

#if 0
		/*
		 * If the descriptor_id is not in the table it's possibly
		 * because the protocol evoluated, losing synchronization
		 * with the table, that's why we don't consider that as
		 * a heuristic
		 */
		if (!try_val_to_str(tvb_get_ntohs(tvb, offset), descriptors))
			/* DescriptorID not found in the table */
			return false;
#endif

		if (!tvb_bytes_exist(tvb, offset + 6, data_len))
			/* Advertised length too long */
			return false;

		offset += 6 + data_len;
	}

	/* The packed should end with a 2 bytes ID */
	return tvb_captured_length_remaining(tvb, offset) == 2;
}

static void
add_message_data(tvbuff_t * tvb, int offset, int data_len, proto_tree * tree)
{
	if (!tree)
		return;

	/*
	 * XXX - if these are text strings, as the original dissector
	 * treated them as, why the byte swapping?  That would make
	 * sense only if they're UCS-2 strings, but the rest of the
	 * code wasn't treating them as such.
	 *
	 * Just treat it as a byte array.  If that's wrong, submit
	 * a change, with a sample packet, that treats it as whatever
	 * it is, an with a comment that *states* what it is.
	 *
	 * XXX - this claimed that "Armagetronad swaps unconditionally",
	 * but I'm not seeing any obvious unconditional byte-swapping
	 * in the code, and if somebody's never seen a big-endian
	 * machine in their life, they may well confuse "convert to
	 * network byte order" with "unconditionally swap".  So
	 * if you want to dump this out as a sequence of shorts,
	 * fetch the shorts one at a time using ENC_BIG_ENDIAN.
	 */
	proto_tree_add_item(tree, hf_armagetronad_data, tvb, offset,
			      data_len, ENC_NA);
}

static int
add_message(tvbuff_t * tvb, int offset, proto_tree * tree, wmem_strbuf_t * info)
{
	uint16_t     descriptor_id, message_id;
	int          data_len;
	proto_item  *msg;
	proto_tree  *msg_tree;
	const char *descriptor;

	descriptor_id = tvb_get_ntohs(tvb, offset);
	message_id = tvb_get_ntohs(tvb, offset + 2);
	data_len = tvb_get_ntohs(tvb, offset + 4) * 2;

	/* Message subtree */
	descriptor = val_to_str(descriptor_id, descriptors, "Unknown (%u)");
	if (descriptor_id == ACK)
		msg = proto_tree_add_none_format(tree,
						 hf_armagetronad_msg_subtree,
						 tvb, offset, data_len + 6,
						 "ACK %d messages",
						 data_len / 2);
	else
		msg = proto_tree_add_none_format(tree,
						 hf_armagetronad_msg_subtree,
						 tvb, offset, data_len + 6,
						 "Message 0x%04x [%s]",
						 message_id, descriptor);

	msg_tree = proto_item_add_subtree(msg, ett_message);

	/* DescriptorID field */
	proto_tree_add_item(msg_tree, hf_armagetronad_descriptor_id, tvb,
			    offset, 2, ENC_BIG_ENDIAN);
	if (info)
		wmem_strbuf_append_printf(info, "%s, ", descriptor);

	/* MessageID field */
	proto_tree_add_item(msg_tree, hf_armagetronad_message_id, tvb,
			    offset + 2, 2, ENC_BIG_ENDIAN);

	/* DataLen field */
	proto_tree_add_item(msg_tree, hf_armagetronad_data_len, tvb,
			    offset + 4, 2, ENC_BIG_ENDIAN);

	/* Data field */
	add_message_data(tvb, offset + 6, data_len, msg_tree);

	return data_len + 6;
}

/* Code to actually dissect the packets */
static int
dissect_armagetronad(tvbuff_t * tvb, packet_info * pinfo, proto_tree * tree, void * data _U_)
{
	proto_item    *ti;
	proto_tree    *armagetronad_tree;
	uint16_t       sender;
	int            offset = 0;
	wmem_strbuf_t *info;
	size_t         new_len;

	if (!is_armagetronad_packet(tvb))
		return 0;

	info = wmem_strbuf_new(pinfo->pool, "");

	col_set_str(pinfo->cinfo, COL_PROTOCOL, "Armagetronad");

	col_clear(pinfo->cinfo, COL_INFO);

	ti = proto_tree_add_item(tree, proto_armagetronad, tvb, 0, -1, ENC_NA);
	armagetronad_tree = proto_item_add_subtree(ti, ett_armagetronad);

	/* For each message in the frame */
	while (tvb_reported_length_remaining(tvb, offset) > 2)
		offset += add_message(tvb, offset, armagetronad_tree, info);

	/* After the messages, comes the SenderID */
	sender = tvb_get_ntohs(tvb, offset);
	proto_tree_add_item(ti, hf_armagetronad_sender_id, tvb, offset, 2,
			    ENC_BIG_ENDIAN);

	new_len = wmem_strbuf_get_len(info) - 2;	/* Remove the trailing ", " */
	if (new_len > 0)
		wmem_strbuf_truncate(info, new_len);
	else
		info = wmem_strbuf_new(pinfo->pool, "No message");

	col_add_fstr(pinfo->cinfo, COL_INFO, "[%s] from 0x%04x",
		     wmem_strbuf_get_str(info), sender);

	return offset + 2;
}

void proto_register_armagetronad(void)
{
	static hf_register_info hf[] = {
		{&hf_armagetronad_descriptor_id,
		 {"Descriptor", "armagetronad.descriptor_id",
		  FT_UINT16, BASE_DEC, VALS(descriptors), 0x0,
		  "The ID of the descriptor (the command)", HFILL}
		 },
		{&hf_armagetronad_message_id,
		 {"MessageID", "armagetronad.message_id",
		  FT_UINT16, BASE_HEX, NULL, 0x0,
		  "The ID of the message (to ack it)", HFILL}
		 },
		{&hf_armagetronad_data_len,
		 {"DataLen", "armagetronad.data_len",
		  FT_UINT16, BASE_DEC, NULL, 0x0,
		  "The length of the data (in shorts)", HFILL}
		 },
		{&hf_armagetronad_data,
		 {"Data", "armagetronad.data",
		  FT_BYTES, BASE_NONE, NULL, 0x0,
		  "The actual data (array of shorts in network order)", HFILL}
		 },
		{&hf_armagetronad_sender_id,
		 {"SenderID", "armagetronad.sender_id",
		  FT_UINT16, BASE_HEX, NULL, 0x0,
		  "The ID of the sender (0x0000 for the server)", HFILL}
		 },
		{&hf_armagetronad_msg_subtree,
		 {"Message", "armagetronad.message",
		  FT_NONE, BASE_NONE, NULL, 0x0,
		  "A message", HFILL}
		 }
	};

	static int *ett[] = {
		&ett_armagetronad,
		&ett_message
	};

	proto_armagetronad = proto_register_protocol("The Armagetron Advanced OpenGL Tron clone", "Armagetronad", "armagetronad");

	proto_register_field_array(proto_armagetronad, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
	armagetronad_handle = register_dissector("armagetronad", dissect_armagetronad, proto_armagetronad);
}

void proto_reg_handoff_armagetronad(void)
{
	dissector_add_uint_range_with_preference("udp.port", ARMAGETRONAD_UDP_PORT_RANGE, armagetronad_handle);
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
