/** @file
 *
 * Definitions of protocol IDs for the 00-80-C2 OUI, used for
 * bridging various networks over ATM (RFC 2684) or Frame Relay (RFC 2427).
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 - 2000 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __BRIDGED_PID_H__
#define __BRIDGED_PID_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define BPID_ETH_WITH_FCS	0x0001	/* 802.3/Ethernet with preserved FCS */
#define BPID_ETH_WITHOUT_FCS	0x0007	/* 802.3/Ethernet without preserved FCS */

#define BPID_802_4_WITH_FCS	0x0002	/* 802.4 with preserved FCS */
#define BPID_802_4_WITHOUT_FCS	0x0008	/* 802.4 without preserved FCS */

#define BPID_802_5_WITH_FCS	0x0003	/* 802.5 with preserved FCS */
#define BPID_802_5_WITHOUT_FCS	0x0009	/* 802.5 without preserved FCS */

#define BPID_FDDI_WITH_FCS	0x0004	/* FDDI with preserved FCS */
#define BPID_FDDI_WITHOUT_FCS	0x000A	/* FDDI without preserved FCS */

#define BPID_802_6_WITH_FCS	0x0005	/* 802.6 with preserved FCS */
#define BPID_802_6_WITHOUT_FCS	0x000B	/* 802.6 without preserved FCS */

#define BPID_FRAGMENTS		0x000D

#define BPID_BPDU		0x000E	/* 802.1(d) or 802.1(g) BPDUs */

#define BPID_SR_BPDU		0x000F	/* Source Routing BPDUs */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* bridged_pid.h */
