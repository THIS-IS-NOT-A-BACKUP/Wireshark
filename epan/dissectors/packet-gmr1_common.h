/* packet-gmr1_common.h
 *
 * Headers for GMR-1 dissection in wireshark (common stuff).
 * Copyright (c) 2011 Sylvain Munaut <tnt@246tNt.com>
 *
 * References:
 *  [1] ETSI TS 101 376-4-8 V1.3.1 - GMR-1 04.008
 *  [2] ETSI TS 101 376-4-8 V2.2.1 - GMPRS-1 04.008
 *  [3] ETSI TS 101 376-4-8 V3.1.1 - GMR-1 3G 44.008
 *  [4] ETSI TS 100 940 V7.21.0 - GSM 04.08
 *  [5] ETSI TS 101 376-4-12 V3.2.1 - GMR-1 3G 44.060
 *  [6] ETSI TS 101 376-5-6 V1.3.1 - GMR-1 05.008
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PACKET_GMR1_COMMON_H__
#define __PACKET_GMR1_COMMON_H__

#include "packet-gsm_a_common.h"


/* Protocol descriptor (see [1] 11.2 & [4] 10.2) */
typedef enum {
	GMR1_PD_CC	= 0x03,
	GMR1_PD_MM	= 0x05,
	GMR1_PD_RR	= 0x06,
	GMR1_PD_GMM	= 0x08,
	GMR1_PD_SM	= 0x0a,
	GMR1_PD_DTRS	= 0x1e
} gmr1_pd_e;

#define GMR1_PD_EXT_MSK	0x0f
#define GMR1_PD_EXT_VAL	0x0e

extern const value_string gmr1_pd_vals[];
extern const value_string gmr1_pd_short_vals[];


/* Common IEs */
enum gmr1_ie_common_idx {
	GMR1_IE_COM_CM2,			/* [1] 11.5.1.6 */
	GMR1_IE_COM_SPARE_NIBBLE,		/* [1] 11.5.1.8 */
	NUM_GMR1_IE_COMMON /* Terminator */
};

extern int hf_gmr1_skip_ind;
extern int hf_gmr1_l3_pd;
extern int hf_gmr1_elem_id;
extern int hf_gmr1_len;


/* Message & IEs parsing */

typedef void (*gmr1_msg_func_t)(tvbuff_t *tvb, proto_tree *tree, packet_info *pinfo, int offset, int len);

#define GMR1_IE_FUNC(fn_name)	\
	static uint16_t \
	fn_name (tvbuff_t *tvb _U_, proto_tree *tree _U_, packet_info *pinfo _U_, uint32_t offset _U_, unsigned len _U_, char *add_string _U_, int string_len _U_)

#define GMR1_MSG_FUNC(fn_name)	\
	static void \
	fn_name (tvbuff_t *tvb _U_, proto_tree *tree _U_, packet_info *pinfo _U_, int offset, int len)

#define GMR1_MSG_FUNC_BEGIN	\
	int curr_offset;	\
	int curr_len;		\
	int consumed;		\
				\
	curr_offset = offset;	\
	curr_len = len;		\
	consumed = 0;

#define GMR1_MSG_FUNC_END


extern void
gmr1_get_msg_params(gmr1_pd_e pd, uint8_t oct, const char **msg_str,
                    int *ett_tree, int *hf_idx, gmr1_msg_func_t *msg_func_p);


#endif /* __PACKET_GMR1_COMMON_H__ */
