/* packet-dcerpc-cprpc_server.c
 * Routines for DNS Control Program Server dissection
 * Copyright 2002, Jaime Fournier <Jaime.Fournier@hush.com>
 * This information is based off the released idl files from opengroup.
 * ftp://ftp.opengroup.org/pub/dce122/dce/src/directory.tar.gz directory/cds/stubs/cprpc_server.idl
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"


#include <epan/packet.h>
#include "packet-dcerpc.h"

void proto_register_cprpc_server (void);
void proto_reg_handoff_cprpc_server (void);

static int proto_cprpc_server;
static int hf_cprpc_server_opnum;


static int ett_cprpc_server;


static e_guid_t uuid_cprpc_server = { 0x4885772c, 0xc6d3, 0x11ca, { 0x84, 0xc6, 0x08, 0x00, 0x2b, 0x1c, 0x8f, 0x1f } };
static uint16_t ver_cprpc_server = 1;


static const dcerpc_sub_dissector cprpc_server_dissectors[] = {
	{ 0, "dnscp_server", NULL, NULL},
	{ 0, NULL, NULL, NULL }
};

void
proto_register_cprpc_server (void)
{
	static hf_register_info hf[] = {
	  { &hf_cprpc_server_opnum,
	    { "Operation", "cprpc_server.opnum", FT_UINT16, BASE_DEC,
	      NULL, 0x0, NULL, HFILL }}
	};

	static int *ett[] = {
		&ett_cprpc_server,
	};
	proto_cprpc_server = proto_register_protocol ("DNS Control Program Server", "cprpc_server", "cprpc_server");
	proto_register_field_array (proto_cprpc_server, hf, array_length (hf));
	proto_register_subtree_array (ett, array_length (ett));
}

void
proto_reg_handoff_cprpc_server (void)
{
	/* Register the protocol as dcerpc */
	dcerpc_init_uuid (proto_cprpc_server, ett_cprpc_server, &uuid_cprpc_server, ver_cprpc_server, cprpc_server_dissectors, hf_cprpc_server_opnum);
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
