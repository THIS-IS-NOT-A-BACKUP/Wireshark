/* packet-portmap.h
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PACKET_PORTMAP_H
#define PACKET_PORTMAP_H

#define PORTMAP_PROGRAM  100000

#define PORTMAPPROC_NULL     0
#define PORTMAPPROC_SET      1
#define PORTMAPPROC_UNSET    2
#define PORTMAPPROC_GETPORT  3
#define PORTMAPPROC_DUMP     4
#define PORTMAPPROC_CALLIT   5

/* RFC 1833, Page 7 */
#define RPCBPROC_NULL		0
#define RPCBPROC_SET		1
#define RPCBPROC_UNSET		2
#define RPCBPROC_GETADDR	3
#define RPCBPROC_DUMP		4
#define RPCBPROC_CALLIT		5
#define RPCBPROC_GETTIME	6
#define RPCBPROC_UADDR2TADDR	7
#define RPCBPROC_TADDR2UADDR	8

/* RFC 1833, Page 8 */
#define RPCBPROC_BCAST		RPCBPROC_CALLIT
#define RPCBPROC_GETVERSADDR	9
#define RPCBPROC_INDIRECT	10
#define RPCBPROC_GETADDRLIST	11
#define RPCBPROC_GETSTAT	12

struct pmap {
        uint32_t pm_prog;
        uint32_t pm_vers;
        uint32_t pm_prot;
        uint32_t pm_port;
};

#endif
