/** @file
 *
 * TTX Logger (TTL) file format from TTTech Computertechnik AG decoder
 * for the Wiretap library.
 *
 * Copyright (c) 2024 by Giovanni Musto <giovanni.musto@partner.italdesign.it>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __W_TTL_H__
#define __W_TTL_H__

#include <stdint.h>
#include "wtap.h"

wtap_open_return_val ttl_open(wtap* wth, int* err, char** err_info);

WS_DLL_PUBLIC int ttl_get_address_iface_type(uint16_t addr);
WS_DLL_PUBLIC bool ttl_is_chb_addr(uint16_t addr);
WS_DLL_PUBLIC uint16_t ttl_get_master_address(GHashTable* ht, uint16_t addr);
WS_DLL_PUBLIC const char* ttl_get_cascade_name(uint16_t addr);
WS_DLL_PUBLIC const char* ttl_get_device_name(uint16_t addr);
WS_DLL_PUBLIC const char* ttl_get_function_name(uint16_t addr);

#define ttl_addr_get_cascade(x)     (((x) >> 10) & 0x7)
#define ttl_addr_get_device(x)      (((x) >> 6) & 0xf)
#define ttl_addr_get_function(x)    ((x) & 0x3f)

/*
 * A TTL file is divided into two sections:
 * 1. Header section
 * 2. Trace Data section
 *
 * The Trace Data section is divided into blocks of fixes size (default: 2 MB).
 *
 * All multi-byte fields are stored as Little Endian.
 */

#define TTL_LOGFILE_INFO_SIZE   4080

 /*
  * The header of the file changed between versions. The real size is
  * indicated by the header_size field, with the bare minimum being 16 bytes.
  * The data after header_size (if any) is composed of two parts:
  * 1. Log File information
  * 2. XML describing the logger configuration used to create the log file
  *
  * The XML is assumed to be there only if the header size is bigger
  * than 4096 bytes (i.e. Log File information is 4080 bytes in size).
  */

 /* TTL File Header */
typedef struct ttl_fileheader {
    uint8_t     magic[4];       /* Magic Number - "TTL " */
    uint32_t    version;        /* File Format version */
    uint32_t    block_size;     /* Size of the blocks */
    uint32_t    header_size;    /* Size of the complete header */
} ttl_fileheader_t;

/* TTL Entry Header */
typedef struct ttl_entryheader {
    uint16_t    size_type;          /* Type (4 bits) | Size (12 bits) */
    uint16_t    dest_addr;          /* Meta 1 (3 bits) | Destination Address (13 bits) */
    uint16_t    src_addr;           /* Meta 2 (3 bits) | Source Address (13 bits) */
    uint16_t    status_info;        /* Meaning changes between types */
} ttl_entryheader_t;

#define TTL_BUS_DATA_ENTRY          0
#define TTL_COMMAND_ENTRY           1
#define TTL_BUS_RESERVED1_ENTRY     2
#define TTL_JOURNAL_ENTRY           3
#define TTL_SEGMENTED_MESSAGE_ENTRY 4
#define TTL_SEND_FRAME_ENTRY        5
#define TTL_PADDING_ENTRY           6
#define TTL_SOFTWARE_DATA_ENTRY     7
#define TTL_DROPPED_FRAMES_ENTRY    8

/* The address is Cascade (3 bits) | Device (4 bits) | Function (6 bits) */
#define TTL_ADDRESS_MASK                        0x1FFF
#define TTL_SIZE_MASK                           0x0FFF
#define TTL_META1_TIMESTAMP_SOURCE_MASK         0x2000
#define TTL_META1_COMPRESSED_FORMAT_MASK        0x4000
#define TTL_META1_FRAME_DUPLICATION_MARKER_MASK 0x8000

#define TTL_LOGGER_DEVICE_FPGA      0
#define TTL_LOGGER_DEVICE_ATOM      1
#define TTL_LOGGER_DEVICE_TRICORE1  2
#define TTL_LOGGER_DEVICE_TRICORE2  3
#define TTL_LOGGER_DEVICE_TRICORE3  4
#define TTL_LOGGER_DEVICE_TDA4x     5
#define TTL_LOGGER_DEVICE_FPGAA     6
#define TTL_LOGGER_DEVICE_FPGAB     7

#define TTL_LOGGER_FPGA_FUNCTION_CORE           0
#define TTL_LOGGER_FPGA_FUNCTION_EXT0_MOST25    1
#define TTL_LOGGER_FPGA_FUNCTION_EXT0_MOST150   2
#define TTL_LOGGER_FPGA_FUNCTION_ETHA_CH1       3
#define TTL_LOGGER_FPGA_FUNCTION_ETHB_CH1       4
#define TTL_LOGGER_FPGA_FUNCTION_FLEXRAY1A      5
#define TTL_LOGGER_FPGA_FUNCTION_FLEXRAY1B      6
#define TTL_LOGGER_FPGA_FUNCTION_FLEXRAY2A      7
#define TTL_LOGGER_FPGA_FUNCTION_FLEXRAY2B      8
#define TTL_LOGGER_FPGA_FUNCTION_FLEXRAY3A      9
#define TTL_LOGGER_FPGA_FUNCTION_FLEXRAY3B      10
#define TTL_LOGGER_FPGA_FUNCTION_CAN1           11
#define TTL_LOGGER_FPGA_FUNCTION_CAN2           12
#define TTL_LOGGER_FPGA_FUNCTION_CAN3           13
#define TTL_LOGGER_FPGA_FUNCTION_CAN4           14
#define TTL_LOGGER_FPGA_FUNCTION_CAN12          15
#define TTL_LOGGER_FPGA_FUNCTION_CAN6           16
#define TTL_LOGGER_FPGA_FUNCTION_CAN7           17
#define TTL_LOGGER_FPGA_FUNCTION_CAN10          18
#define TTL_LOGGER_FPGA_FUNCTION_CAN11          19
#define TTL_LOGGER_FPGA_FUNCTION_CAN8           20
#define TTL_LOGGER_FPGA_FUNCTION_CAN5           21
#define TTL_LOGGER_FPGA_FUNCTION_CAN9           22
#define TTL_LOGGER_FPGA_FUNCTION_EXT1_MOST25    23
#define TTL_LOGGER_FPGA_FUNCTION_LIN10          24
#define TTL_LOGGER_FPGA_FUNCTION_LIN3           25
#define TTL_LOGGER_FPGA_FUNCTION_LIN5           26
#define TTL_LOGGER_FPGA_FUNCTION_LIN4           27
#define TTL_LOGGER_FPGA_FUNCTION_LIN11          28
#define TTL_LOGGER_FPGA_FUNCTION_LIN1           29
#define TTL_LOGGER_FPGA_FUNCTION_LIN7           30
#define TTL_LOGGER_FPGA_FUNCTION_LIN8           31
#define TTL_LOGGER_FPGA_FUNCTION_LIN12          32
#define TTL_LOGGER_FPGA_FUNCTION_LIN6           33
#define TTL_LOGGER_FPGA_FUNCTION_LIN2           34
#define TTL_LOGGER_FPGA_FUNCTION_LIN9           35
#define TTL_LOGGER_FPGA_FUNCTION_CAN13          36
#define TTL_LOGGER_FPGA_FUNCTION_CAN14          37
#define TTL_LOGGER_FPGA_FUNCTION_CAN15          38
#define TTL_LOGGER_FPGA_FUNCTION_CAN16          39
#define TTL_LOGGER_FPGA_FUNCTION_CAN17          40
#define TTL_LOGGER_FPGA_FUNCTION_CAN18          41
#define TTL_LOGGER_FPGA_FUNCTION_CAN19          42
#define TTL_LOGGER_FPGA_FUNCTION_CAN20          43
#define TTL_LOGGER_FPGA_FUNCTION_CAN21          44
#define TTL_LOGGER_FPGA_FUNCTION_CAN22          45
#define TTL_LOGGER_FPGA_FUNCTION_CAN23          46
#define TTL_LOGGER_FPGA_FUNCTION_CAN24          47
#define TTL_LOGGER_FPGA_FUNCTION_ETHA_CH2       48
#define TTL_LOGGER_FPGA_FUNCTION_ETHB_CH2       49
#define TTL_LOGGER_FPGA_FUNCTION_ETHA_CH3       50
#define TTL_LOGGER_FPGA_FUNCTION_ETHB_CH3       51
/* 5 unused */
#define TTL_LOGGER_FPGA_FUNCTION_CAN_EXT_BOARD  57
#define TTL_LOGGER_FPGA_FUNCTION_RESERVED1      58
#define TTL_LOGGER_FPGA_FUNCTION_SLOT_CTRL      59
#define TTL_LOGGER_FPGA_FUNCTION_DRAM           60
#define TTL_LOGGER_FPGA_FUNCTION_SINK           61
#define TTL_LOGGER_FPGA_FUNCTION_POWER_AGENT    62
#define TTL_LOGGER_FPGA_FUNCTION_PKT_GENERATOR  63

#define TTL_LOGGER_ATOM_FUNCTION_FRAME_DEVICE       0
#define TTL_LOGGER_ATOM_FUNCTION_CHARACTER_DEVICE   1
#define TTL_LOGGER_ATOM_FUNCTION_ATMEL              2
#define TTL_LOGGER_ATOM_FUNCTION_ETHA               3
#define TTL_LOGGER_ATOM_FUNCTION_ETHB               4

#define TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYA        1
#define TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYB        2

#define TTL_LOGGER_TRICORE1_FUNCTION_CORE           0
#define TTL_LOGGER_TRICORE1_FUNCTION_FLEXRAY1A      TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYA
#define TTL_LOGGER_TRICORE1_FUNCTION_FLEXRAY1B      TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYB
#define TTL_LOGGER_TRICORE1_FUNCTION_CAN1           3
#define TTL_LOGGER_TRICORE1_FUNCTION_CAN2           4
#define TTL_LOGGER_TRICORE1_FUNCTION_CAN3           5
#define TTL_LOGGER_TRICORE1_FUNCTION_CAN4           6
#define TTL_LOGGER_TRICORE1_FUNCTION_ANALOGOUT1     7
#define TTL_LOGGER_TRICORE1_FUNCTION_DIGITALOUT6    8
#define TTL_LOGGER_TRICORE1_FUNCTION_DIGITALOUT5    9
#define TTL_LOGGER_TRICORE1_FUNCTION_RESERVED1      10
#define TTL_LOGGER_TRICORE1_FUNCTION_RESERVED2      11
#define TTL_LOGGER_TRICORE1_FUNCTION_SERIAL1        12
#define TTL_LOGGER_TRICORE1_FUNCTION_SERIAL2        13
#define TTL_LOGGER_TRICORE1_FUNCTION_ANALOGIN6      14
#define TTL_LOGGER_TRICORE1_FUNCTION_ANALOGIN8      15
#define TTL_LOGGER_TRICORE1_FUNCTION_ANALOGIN14     16
#define TTL_LOGGER_TRICORE1_FUNCTION_ANALOGIN15     17
#define TTL_LOGGER_TRICORE1_FUNCTION_ANALOGIN11     18
#define TTL_LOGGER_TRICORE1_FUNCTION_DIGITALIN8     19
#define TTL_LOGGER_TRICORE1_FUNCTION_DIGITALIN10    20
#define TTL_LOGGER_TRICORE1_FUNCTION_DIGITALIN12    21
#define TTL_LOGGER_TRICORE1_FUNCTION_DIGITALIN13    22
#define TTL_LOGGER_TRICORE1_FUNCTION_DIGITALIN11    23
#define TTL_LOGGER_TRICORE1_FUNCTION_KL15IN         24
#define TTL_LOGGER_TRICORE1_FUNCTION_KL30IN         25
#define TTL_LOGGER_TRICORE1_FUNCTION_FLEXRAY1       26
#define TTL_LOGGER_TRICORE1_FUNCTION_FLEXRAY1AB     27

#define TTL_LOGGER_TRICORE2_FUNCTION_CORE           0
#define TTL_LOGGER_TRICORE2_FUNCTION_FLEXRAY2A      TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYA
#define TTL_LOGGER_TRICORE2_FUNCTION_FLEXRAY2B      TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYB
#define TTL_LOGGER_TRICORE2_FUNCTION_CAN12          3
#define TTL_LOGGER_TRICORE2_FUNCTION_CAN6           4
#define TTL_LOGGER_TRICORE2_FUNCTION_CAN7           5
#define TTL_LOGGER_TRICORE2_FUNCTION_CAN10          6
#define TTL_LOGGER_TRICORE2_FUNCTION_ANALOGOUT2     7
#define TTL_LOGGER_TRICORE2_FUNCTION_DIGITALOUT4    8
#define TTL_LOGGER_TRICORE2_FUNCTION_DIGITALOUT3    9
#define TTL_LOGGER_TRICORE2_FUNCTION_RESERVED1      10
#define TTL_LOGGER_TRICORE2_FUNCTION_RESERVED2      11
#define TTL_LOGGER_TRICORE2_FUNCTION_SERIAL3        12
#define TTL_LOGGER_TRICORE2_FUNCTION_SERIAL4        13
#define TTL_LOGGER_TRICORE2_FUNCTION_ANALOGIN4      14
#define TTL_LOGGER_TRICORE2_FUNCTION_ANALOGIN3      15
#define TTL_LOGGER_TRICORE2_FUNCTION_ANALOGIN5      16
#define TTL_LOGGER_TRICORE2_FUNCTION_ANALOGIN9      17
#define TTL_LOGGER_TRICORE2_FUNCTION_ANALOGIN7      18
#define TTL_LOGGER_TRICORE2_FUNCTION_DIGITALIN14    19
#define TTL_LOGGER_TRICORE2_FUNCTION_DIGITALIN9     20
#define TTL_LOGGER_TRICORE2_FUNCTION_DIGITALIN15    21
#define TTL_LOGGER_TRICORE2_FUNCTION_DIGITALIN7     22
#define TTL_LOGGER_TRICORE2_FUNCTION_DIGITALIN6     23
/* 2 unused */
#define TTL_LOGGER_TRICORE2_FUNCTION_FLEXRAY2       26
#define TTL_LOGGER_TRICORE2_FUNCTION_FLEXRAY2AB     27

#define TTL_LOGGER_TRICORE3_FUNCTION_CORE           0
#define TTL_LOGGER_TRICORE3_FUNCTION_FLEXRAY3A      TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYA
#define TTL_LOGGER_TRICORE3_FUNCTION_FLEXRAY3B      TTL_LOGGER_TRICORE_FUNCTION_FLEXRAYB
#define TTL_LOGGER_TRICORE3_FUNCTION_CAN11          3
#define TTL_LOGGER_TRICORE3_FUNCTION_CAN8           4
#define TTL_LOGGER_TRICORE3_FUNCTION_CAN5           5
#define TTL_LOGGER_TRICORE3_FUNCTION_CAN9           6
#define TTL_LOGGER_TRICORE3_FUNCTION_ANALOGOUT3     7
#define TTL_LOGGER_TRICORE3_FUNCTION_DIGITALOUT2    8
#define TTL_LOGGER_TRICORE3_FUNCTION_DIGITALOUT1    9
#define TTL_LOGGER_TRICORE3_FUNCTION_RESERVED1      10
#define TTL_LOGGER_TRICORE3_FUNCTION_RESERVED2      11
#define TTL_LOGGER_TRICORE3_FUNCTION_SERIAL5        12
#define TTL_LOGGER_TRICORE3_FUNCTION_SERIAL6        13
#define TTL_LOGGER_TRICORE3_FUNCTION_ANALOGIN1      14
#define TTL_LOGGER_TRICORE3_FUNCTION_ANALOGIN2      15
#define TTL_LOGGER_TRICORE3_FUNCTION_ANALOGIN10     16
#define TTL_LOGGER_TRICORE3_FUNCTION_ANALOGIN12     17
#define TTL_LOGGER_TRICORE3_FUNCTION_ANALOGIN13     18
#define TTL_LOGGER_TRICORE3_FUNCTION_DIGITALIN5     19
#define TTL_LOGGER_TRICORE3_FUNCTION_DIGITALIN4     20
#define TTL_LOGGER_TRICORE3_FUNCTION_DIGITALIN3     21
#define TTL_LOGGER_TRICORE3_FUNCTION_DIGITALIN2     22
#define TTL_LOGGER_TRICORE3_FUNCTION_DIGITALIN1     23
/* 2 unused */
#define TTL_LOGGER_TRICORE3_FUNCTION_FLEXRAY3       26
#define TTL_LOGGER_TRICORE3_FUNCTION_FLEXRAY3AB     27

#define TTL_LOGGER_TDA4x_FUNCTION_CORE              0
#define TTL_LOGGER_TDA4x_FUNCTION_CHARACTER_DEVICE  1
#define TTL_LOGGER_TDA4x_FUNCTION_CAN1              2
#define TTL_LOGGER_TDA4x_FUNCTION_CAN2              3
#define TTL_LOGGER_TDA4x_FUNCTION_CAN3              4
#define TTL_LOGGER_TDA4x_FUNCTION_CAN4              5
#define TTL_LOGGER_TDA4x_FUNCTION_CAN5              6
#define TTL_LOGGER_TDA4x_FUNCTION_CAN6              7
#define TTL_LOGGER_TDA4x_FUNCTION_CAN7              8
#define TTL_LOGGER_TDA4x_FUNCTION_CAN8              9
#define TTL_LOGGER_TDA4x_FUNCTION_CAN9              10
#define TTL_LOGGER_TDA4x_FUNCTION_CAN10             11
#define TTL_LOGGER_TDA4x_FUNCTION_CAN11             12
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL1           13
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL2           14
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL3           15
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL4           16
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL5           17
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL6           18
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGIN1         19
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGIN2         20
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGIN3         21
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGIN4         22
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGIN5         23
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGIN6         24
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGOUT1        25
#define TTL_LOGGER_TDA4x_FUNCTION_ANALOGOUT2        26
#define TTL_LOGGER_TDA4x_FUNCTION_KL15IN            27
#define TTL_LOGGER_TDA4x_FUNCTION_KL30IN            28
#define TTL_LOGGER_TDA4x_FUNCTION_FLEXRAY1A         29
#define TTL_LOGGER_TDA4x_FUNCTION_FLEXRAY1B         30
#define TTL_LOGGER_TDA4x_FUNCTION_FLEXRAY1AB        31
#define TTL_LOGGER_TDA4x_FUNCTION_CAN12             32
#define TTL_LOGGER_TDA4x_FUNCTION_CAN13             33
#define TTL_LOGGER_TDA4x_FUNCTION_CAN14             34
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL7           35
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL8           36
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL9           37
#define TTL_LOGGER_TDA4x_FUNCTION_SERIAL10          38

#define TTL_LOGGER_FPGAA_FUNCTION_CORE          0
#define TTL_LOGGER_FPGAA_FUNCTION_CAN1          1
#define TTL_LOGGER_FPGAA_FUNCTION_CAN2          2
#define TTL_LOGGER_FPGAA_FUNCTION_CAN3          3
#define TTL_LOGGER_FPGAA_FUNCTION_CAN4          4
#define TTL_LOGGER_FPGAA_FUNCTION_CAN5          5
#define TTL_LOGGER_FPGAA_FUNCTION_CAN6          6
#define TTL_LOGGER_FPGAA_FUNCTION_CAN7          7
#define TTL_LOGGER_FPGAA_FUNCTION_CAN8          8
#define TTL_LOGGER_FPGAA_FUNCTION_CAN9          9
#define TTL_LOGGER_FPGAA_FUNCTION_CAN10         10
#define TTL_LOGGER_FPGAA_FUNCTION_CAN11         11
#define TTL_LOGGER_FPGAA_FUNCTION_LIN1          12
#define TTL_LOGGER_FPGAA_FUNCTION_LIN2          13
#define TTL_LOGGER_FPGAA_FUNCTION_LIN3          14
#define TTL_LOGGER_FPGAA_FUNCTION_LIN4          15
#define TTL_LOGGER_FPGAA_FUNCTION_LIN5          16
#define TTL_LOGGER_FPGAA_FUNCTION_LIN6          17
#define TTL_LOGGER_FPGAA_FUNCTION_LIN7          18
#define TTL_LOGGER_FPGAA_FUNCTION_LIN8          19
#define TTL_LOGGER_FPGAA_FUNCTION_LIN9          20
#define TTL_LOGGER_FPGAA_FUNCTION_LIN10         21
#define TTL_LOGGER_FPGAA_FUNCTION_LIN11         22
#define TTL_LOGGER_FPGAA_FUNCTION_LIN12         23
#define TTL_LOGGER_FPGAA_FUNCTION_LIN13         24
#define TTL_LOGGER_FPGAA_FUNCTION_LIN14         25
#define TTL_LOGGER_FPGAA_FUNCTION_LIN15         26
#define TTL_LOGGER_FPGAA_FUNCTION_LIN16         27
#define TTL_LOGGER_FPGAA_FUNCTION_FLEXRAY1A     28
#define TTL_LOGGER_FPGAA_FUNCTION_FLEXRAY1B     29
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL1       30
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL2       31
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL3       32
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL4       33
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL5       34
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL6       35
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL7       36
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL8       37
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL9       38
#define TTL_LOGGER_FPGAA_FUNCTION_SERIAL10      39
#define TTL_LOGGER_FPGAA_FUNCTION_CAN12         40
#define TTL_LOGGER_FPGAA_FUNCTION_CAN13         41
#define TTL_LOGGER_FPGAA_FUNCTION_CAN14         42
/* 16 unused */
#define TTL_LOGGER_FPGAA_FUNCTION_SLOT_CTRL     59
#define TTL_LOGGER_FPGAA_FUNCTION_DRAM          60
#define TTL_LOGGER_FPGAA_FUNCTION_SINK          61
#define TTL_LOGGER_FPGAA_FUNCTION_POWER_AGENT   62
#define TTL_LOGGER_FPGAA_FUNCTION_PKT_GENERATOR 63

#define TTL_LOGGER_FPGAB_FUNCTION_ETHA_CH1  0
#define TTL_LOGGER_FPGAB_FUNCTION_ETHB_CH1  1
#define TTL_LOGGER_FPGAB_FUNCTION_AETH1a_CH1 2
#define TTL_LOGGER_FPGAB_FUNCTION_AETH1b_CH1 3
#define TTL_LOGGER_FPGAB_FUNCTION_AETH2a_CH1 4
#define TTL_LOGGER_FPGAB_FUNCTION_AETH2b_CH1 5
#define TTL_LOGGER_FPGAB_FUNCTION_AETH3a_CH1 6
#define TTL_LOGGER_FPGAB_FUNCTION_AETH3b_CH1 7
#define TTL_LOGGER_FPGAB_FUNCTION_AETH4a_CH1 8
#define TTL_LOGGER_FPGAB_FUNCTION_AETH4b_CH1 9
#define TTL_LOGGER_FPGAB_FUNCTION_AETH5a_CH1 10
#define TTL_LOGGER_FPGAB_FUNCTION_AETH5b_CH1 11
#define TTL_LOGGER_FPGAB_FUNCTION_AETH6a_CH1 12
#define TTL_LOGGER_FPGAB_FUNCTION_AETH6b_CH1 13
#define TTL_LOGGER_FPGAB_FUNCTION_ETHA_CH2  14
#define TTL_LOGGER_FPGAB_FUNCTION_ETHB_CH2  15
#define TTL_LOGGER_FPGAB_FUNCTION_AETH1a_CH2 16
#define TTL_LOGGER_FPGAB_FUNCTION_AETH1b_CH2 17
#define TTL_LOGGER_FPGAB_FUNCTION_AETH2a_CH2 18
#define TTL_LOGGER_FPGAB_FUNCTION_AETH2b_CH2 19
#define TTL_LOGGER_FPGAB_FUNCTION_AETH3a_CH2 20
#define TTL_LOGGER_FPGAB_FUNCTION_AETH3b_CH2 21
#define TTL_LOGGER_FPGAB_FUNCTION_AETH4a_CH2 22
#define TTL_LOGGER_FPGAB_FUNCTION_AETH4b_CH2 23
#define TTL_LOGGER_FPGAB_FUNCTION_AETH5a_CH2 24
#define TTL_LOGGER_FPGAB_FUNCTION_AETH5b_CH2 25
#define TTL_LOGGER_FPGAB_FUNCTION_AETH6a_CH2 26
#define TTL_LOGGER_FPGAB_FUNCTION_AETH6b_CH2 27

#define TTL_TAP_DEVICE_PT15_FPGA        0
#define TTL_TAP_DEVICE_PT15_HPS_LINUX   1
#define TTL_TAP_DEVICE_PT20_FPGA        2
#define TTL_TAP_DEVICE_PT20_HPS_LINUX   3
#define TTL_TAP_DEVICE_PC3_FPGA         4
#define TTL_TAP_DEVICE_PC3_HPS_LINUX    5
#define TTL_TAP_DEVICE_PC3_AURIX        6
#define TTL_TAP_DEVICE_ZELDA_CANFD      7
#define TTL_TAP_DEVICE_ZELDA_LIN        8
#define TTL_TAP_DEVICE_ILLEGAL          15

#define TTL_PT15_FPGA_FUNCTION_CORE     0
#define TTL_PT15_FPGA_FUNCTION_CAN1     1
#define TTL_PT15_FPGA_FUNCTION_CAN2     2
#define TTL_PT15_FPGA_FUNCTION_BrdR1a   3
#define TTL_PT15_FPGA_FUNCTION_BrdR1b   4
#define TTL_PT15_FPGA_FUNCTION_BrdR2a   5
#define TTL_PT15_FPGA_FUNCTION_BrdR2b   6
#define TTL_PT15_FPGA_FUNCTION_BrdR3a   7
#define TTL_PT15_FPGA_FUNCTION_BrdR3b   8
#define TTL_PT15_FPGA_FUNCTION_BrdR4a   9
#define TTL_PT15_FPGA_FUNCTION_BrdR4b   10
#define TTL_PT15_FPGA_FUNCTION_BrdR5a   11
#define TTL_PT15_FPGA_FUNCTION_BrdR5b   12
#define TTL_PT15_FPGA_FUNCTION_BrdR6a   13
#define TTL_PT15_FPGA_FUNCTION_BrdR6b   14
/* 7 unused */
#define TTL_PT15_FPGA_FUNCTION_MDIO     22

#define TTL_PT20_FPGA_FUNCTION_CORE     0
#define TTL_PT20_FPGA_FUNCTION_CAN1     1
#define TTL_PT20_FPGA_FUNCTION_CAN2     2
#define TTL_PT20_FPGA_FUNCTION_CAN3     3
#define TTL_PT20_FPGA_FUNCTION_CAN4     4
#define TTL_PT20_FPGA_FUNCTION_CAN5     5
#define TTL_PT20_FPGA_FUNCTION_GbEth1a  6
#define TTL_PT20_FPGA_FUNCTION_GbEth1b  7
#define TTL_PT20_FPGA_FUNCTION_GbEth2a  8
#define TTL_PT20_FPGA_FUNCTION_GbEth2b  9
#define TTL_PT20_FPGA_FUNCTION_GbEth3a  10
#define TTL_PT20_FPGA_FUNCTION_GbEth3b  11
/* 10 unused */
#define TTL_PT20_FPGA_FUNCTION_MDIO     22

#define TTL_PC3_FPGA_FUNCTION_CORE      0
/* 2 unused */
#define TTL_PC3_FPGA_FUNCTION_BrdR1a    3
#define TTL_PC3_FPGA_FUNCTION_BrdR1b    4

#define TTL_PC3_AURIX_FUNCTION_CORE         0
#define TTL_PC3_AURIX_FUNCTION_CAN1         1
#define TTL_PC3_AURIX_FUNCTION_CAN2         2
#define TTL_PC3_AURIX_FUNCTION_CAN3         3
#define TTL_PC3_AURIX_FUNCTION_CAN4         4
#define TTL_PC3_AURIX_FUNCTION_FLEXRAY1A    5
#define TTL_PC3_AURIX_FUNCTION_FLEXRAY1B    6
#define TTL_PC3_AURIX_FUNCTION_FLEXRAY2A    7
#define TTL_PC3_AURIX_FUNCTION_FLEXRAY2B    8
#define TTL_PC3_AURIX_FUNCTION_DIGITALIN1   9
#define TTL_PC3_AURIX_FUNCTION_DIGITALIN2   10
#define TTL_PC3_AURIX_FUNCTION_DIGITALOUT1  11
#define TTL_PC3_AURIX_FUNCTION_DIGITALOUT2  12

#define TTL_TAP_DEVICE_ZELDA_CORE       0

#define TTL_TAP_DEVICE_ZELDA_CANFD1     1
#define TTL_TAP_DEVICE_ZELDA_CANFD2     2
#define TTL_TAP_DEVICE_ZELDA_CANFD3     3
#define TTL_TAP_DEVICE_ZELDA_CANFD4     4
#define TTL_TAP_DEVICE_ZELDA_CANFD5     5
#define TTL_TAP_DEVICE_ZELDA_CANFD6     6
#define TTL_TAP_DEVICE_ZELDA_CANFD7     7
#define TTL_TAP_DEVICE_ZELDA_CANFD8     8
#define TTL_TAP_DEVICE_ZELDA_CANFD9     9
#define TTL_TAP_DEVICE_ZELDA_CANFD10    10
#define TTL_TAP_DEVICE_ZELDA_CANFD11    11
#define TTL_TAP_DEVICE_ZELDA_CANFD12    12
#define TTL_TAP_DEVICE_ZELDA_CANFD13    13
#define TTL_TAP_DEVICE_ZELDA_CANFD14    14
#define TTL_TAP_DEVICE_ZELDA_CANFD15    15

#define TTL_TAP_DEVICE_ZELDA_LIN1       1
#define TTL_TAP_DEVICE_ZELDA_LIN2       2
#define TTL_TAP_DEVICE_ZELDA_LIN3       3
#define TTL_TAP_DEVICE_ZELDA_LIN4       4
#define TTL_TAP_DEVICE_ZELDA_LIN5       5
#define TTL_TAP_DEVICE_ZELDA_LIN6       6
#define TTL_TAP_DEVICE_ZELDA_LIN7       7
#define TTL_TAP_DEVICE_ZELDA_LIN8       8
#define TTL_TAP_DEVICE_ZELDA_LIN9       9
#define TTL_TAP_DEVICE_ZELDA_LIN10      10
#define TTL_TAP_DEVICE_ZELDA_LIN11      11
#define TTL_TAP_DEVICE_ZELDA_LIN12      12
#define TTL_TAP_DEVICE_ZELDA_LIN13      13
#define TTL_TAP_DEVICE_ZELDA_LIN14      14
#define TTL_TAP_DEVICE_ZELDA_LIN15      15
#define TTL_TAP_DEVICE_ZELDA_LIN16      16
#define TTL_TAP_DEVICE_ZELDA_LIN17      17
#define TTL_TAP_DEVICE_ZELDA_LIN18      18
#define TTL_TAP_DEVICE_ZELDA_LIN19      19
#define TTL_TAP_DEVICE_ZELDA_LIN20      20
#define TTL_TAP_DEVICE_ZELDA_LIN21      21
#define TTL_TAP_DEVICE_ZELDA_LIN22      22
#define TTL_TAP_DEVICE_ZELDA_LIN23      23
#define TTL_TAP_DEVICE_ZELDA_LIN24      24

#define TTL_ETH_STATUS_VALID_FRAME              0x0001
#define TTL_ETH_STATUS_CRC_ERROR_FRAME          0x0002
#define TTL_ETH_STATUS_LENGTH_ERROR_FRAME       0x0004
#define TTL_ETH_STATUS_PHY_ERROR_FRAME          0x0008
#define TTL_ETH_STATUS_TX_ERROR_FRAME           0x0010
#define TTL_ETH_STATUS_TX_FREEMEM_INFO_FRAME    0x2000
#define TTL_ETH_STATUS_TX_FRAME                 0x4000
#define TTL_ETH_STATUS_PHY_STATUS               0x8000

#define TTL_CAN_STATUS_VALID_BIT_MASK   0x0001
#define TTL_CAN_STATUS_RTR_BIT_MASK     0x0002
#define TTL_CAN_STATUS_BUSOFF_MASK      0x0004
#define TTL_CAN_STATUS_MATCHED_BIT_MASK 0x0008
#define TTL_CAN_STATUS_ERROR_CODE_MASK  0x0070
#define TTL_CAN_STATUS_ERROR_CODE_POS   4
#define TTL_CAN_STATUS_DLC_MASK         0x0F00
#define TTL_CAN_STATUS_DLC_POS          8
#define TTL_CAN_STATUS_IDE_BIT_MASK     0x1000
#define TTL_CAN_STATUS_EDL_BIT_MASK     0x2000
#define TTL_CAN_STATUS_BRS_BIT_MASK     0x4000
#define TTL_CAN_STATUS_ESI_BIT_MASK     0x8000

#define TTL_CAN_ERROR_NO_ERROR      0x0
#define TTL_CAN_ERROR_STUFF_ERROR   0x1
#define TTL_CAN_ERROR_FORM_ERROR    0x2
#define TTL_CAN_ERROR_ACK_ERROR     0x3
#define TTL_CAN_ERROR_BIT1_ERROR    0x4
#define TTL_CAN_ERROR_BIT0_ERROR    0x5
#define TTL_CAN_ERROR_CRC_ERROR     0x6
#define TTL_CAN_ERROR_INVALID_DLC   0x7

#define TTL_LIN_STATUS_PID_MASK             0x00ff
#define TTL_LIN_ERROR_PARITY_ERROR          0x0100
#define TTL_LIN_ERROR_SYNC_ERROR            0x0200
#define TTL_LIN_ERROR_LIN2CHECKSUM_ERROR    0x0400
#define TTL_LIN_ERROR_LIN1CHECKSUM_ERROR    0x0800
#define TTL_LIN_ERROR_ANY_CHECKSUM          (TTL_LIN_ERROR_LIN2CHECKSUM_ERROR | TTL_LIN_ERROR_LIN1CHECKSUM_ERROR)
#define TTL_LIN_ERROR_NO_DATA_ERROR         0x1000
#define TTL_LIN_ERROR_ABORT_ERROR           0x2000

#define TTL_FLEXRAY_ITEM_MASK               0x0007
#define TTL_FLEXRAY_ITEM_REGULAR_FRAME      0
#define TTL_FLEXRAY_ITEM_ABORTED_FRAME      1
#define TTL_FLEXRAY_ITEM_0_PULSE            2
#define TTL_FLEXRAY_ITEM_1_PULSE            3
#define TTL_FLEXRAY_ITEM_ERROR_INFORMATION  4
#define TTL_FLEXRAY_MATCHED_MASK            0x0008
#define TTL_FLEXRAY_FSS_ERROR_MASK          0x0020
#define TTL_FLEXRAY_BSS_ERROR_MASK          0x0040
#define TTL_FLEXRAY_FES_ERROR_MASK          0x0080
#define TTL_FLEXRAY_FRAME_CRC_ERROR_MASK    0x0100
#define TTL_FLEXRAY_HEADER_CRC_ERROR_MASK   0x0200
#define TTL_FLEXRAY_IDLE_ERROR_MASK         0x0400

#define TTL_SEGMENTED_MESSAGE_ENTRY_TYPE_INVALID        0x00000000
#define TTL_SEGMENTED_MESSAGE_ENTRY_TYPE_FIRMWARE       0x00000001
#define TTL_SEGMENTED_MESSAGE_ENTRY_TYPE_CONFIGURATION  0x00000002
#define TTL_SEGMENTED_MESSAGE_ENTRY_TYPE_NESTED_FRAME   0x00000003

#endif

 /*
  * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
  *
  * Local variables:
  * c-basic-offset: 4
  * tab-width: 8
  * indent-tabs-mode: nil
  * End:
  *
  * vi: set shiftwidth=4 tabstop=8 expandtab:
  * :indentSize=4:tabSize=8:noTabs=true:
  */
