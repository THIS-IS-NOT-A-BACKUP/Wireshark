/* packet-dcerpc-netlogon.c
 * Routines for SMB \PIPE\NETLOGON packet disassembly
 * Copyright 2001,2003 Tim Potter <tpot@samba.org>
 *  2002 structure and command dissectors by Ronnie Sahlberg
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#define WS_LOG_DOMAIN "packet-dcerpc-netlogon"

#include <epan/packet.h>
#include <epan/expert.h>
#include <epan/tfs.h>

#include <wsutil/wsgcrypt.h>
#include <wsutil/wslog.h>
#include <wsutil/str_util.h>
#include <wsutil/array.h>

/* for dissect_mscldap_string */
#include "packet-ldap.h"
#include "packet-dcerpc.h"
#include "packet-dcerpc-nt.h"
#include "packet-dcerpc-netlogon.h"
#include "packet-windows-common.h"
#include "packet-dcerpc-lsa.h"
#include "packet-ntlmssp.h"
#include "packet-dcerpc-misc.h"
/* for keytab format */
#include <epan/asn1.h>
#include "packet-kerberos.h"

void proto_register_dcerpc_netlogon(void);
void proto_reg_handoff_dcerpc_netlogon(void);

static proto_item *
netlogon_dissect_neg_options(tvbuff_t *tvb,proto_tree *tree,uint32_t flags,int offset);

#define NETLOGON_FLAG_80000000 0x80000000
#define NETLOGON_FLAG_40000000 0x40000000
#define NETLOGON_FLAG_20000000 0x20000000
#define NETLOGON_FLAG_10000000 0x10000000
#define NETLOGON_FLAG_8000000   0x8000000
#define NETLOGON_FLAG_4000000   0x4000000
#define NETLOGON_FLAG_2000000   0x2000000
#define NETLOGON_FLAG_AES       0x1000000
#define NETLOGON_FLAG_800000     0x800000
#define NETLOGON_FLAG_400000     0x400000
#define NETLOGON_FLAG_200000     0x200000
#define NETLOGON_FLAG_100000     0x100000
#define NETLOGON_FLAG_80000       0x80000
#define NETLOGON_FLAG_40000       0x40000
#define NETLOGON_FLAG_20000       0x20000
#define NETLOGON_FLAG_10000       0x10000
#define NETLOGON_FLAG_8000         0x8000
#define NETLOGON_FLAG_STRONGKEY    0x4000
#define NETLOGON_FLAG_2000         0x2000
#define NETLOGON_FLAG_1000         0x1000
#define NETLOGON_FLAG_800           0x800
#define NETLOGON_FLAG_400           0x400
#define NETLOGON_FLAG_200           0x200
#define NETLOGON_FLAG_100           0x100
#define NETLOGON_FLAG_80             0x80
#define NETLOGON_FLAG_40             0x40
#define NETLOGON_FLAG_20             0x20
#define NETLOGON_FLAG_10             0x10
#define NETLOGON_FLAG_8               0x8
#define NETLOGON_FLAG_4               0x4
#define NETLOGON_FLAG_2               0x2
#define NETLOGON_FLAG_1               0x1

static wmem_map_t *netlogon_auths;
static wmem_map_t *schannel_auths;
static int proto_dcerpc_netlogon;

static int hf_netlogon_TrustedDomainName_string;
static int hf_netlogon_UserName_string;
static int hf_domain_info_sid;
static int hf_dns_domain_info_sid;
static int hf_dns_domain_info_domain_guid;
static int hf_dns_domain_info_dns_domain;
static int hf_dns_domain_info_dns_forest;
static int hf_dns_domain_info_name;
static int hf_client_challenge;
static int hf_server_rid;
static int hf_server_challenge;
static int hf_client_credential;
static int hf_server_credential;
static int hf_netlogon_logon_dnslogondomainname;
static int hf_netlogon_logon_upn;
static int hf_netlogon_opnum;
static int hf_netlogon_data_length;
static int hf_netlogon_extraflags;
static int hf_netlogon_extra_flags_root_forest;
static int hf_netlogon_trust_flags_dc_firsthop;
static int hf_netlogon_trust_flags_rodc_to_dc;
static int hf_netlogon_trust_flags_rodc_ntlm;
static int hf_netlogon_package_name;
static int hf_netlogon_rc;
static int hf_netlogon_dos_rc;
static int hf_netlogon_werr_rc;
static int hf_netlogon_len;
static int hf_netlogon_password_version_reserved;
static int hf_netlogon_password_version_number;
static int hf_netlogon_password_version_present;
static int hf_netlogon_sensitive_data_flag;
static int hf_netlogon_sensitive_data_len;
static int hf_netlogon_sensitive_data;
static int hf_netlogon_security_information;
static int hf_netlogon_dummy;
static int hf_netlogon_neg_flags;
static int hf_netlogon_neg_flags_80000000;
static int hf_netlogon_neg_flags_40000000;
static int hf_netlogon_neg_flags_20000000;
/* static int hf_netlogon_neg_flags_10000000; */
/* static int hf_netlogon_neg_flags_8000000; */
/* static int hf_netlogon_neg_flags_4000000; */
/* static int hf_netlogon_neg_flags_2000000; */
static int hf_netlogon_neg_flags_1000000;
/* static int hf_netlogon_neg_flags_800000; */
/* static int hf_netlogon_neg_flags_400000; */
static int hf_netlogon_neg_flags_200000;
static int hf_netlogon_neg_flags_100000;
static int hf_netlogon_neg_flags_80000;
static int hf_netlogon_neg_flags_40000;
static int hf_netlogon_neg_flags_20000;
static int hf_netlogon_neg_flags_10000;
static int hf_netlogon_neg_flags_8000;
static int hf_netlogon_neg_flags_4000;
static int hf_netlogon_neg_flags_2000;
static int hf_netlogon_neg_flags_1000;
static int hf_netlogon_neg_flags_800;
static int hf_netlogon_neg_flags_400;
static int hf_netlogon_neg_flags_200;
static int hf_netlogon_neg_flags_100;
static int hf_netlogon_neg_flags_80;
static int hf_netlogon_neg_flags_40;
static int hf_netlogon_neg_flags_20;
static int hf_netlogon_neg_flags_10;
static int hf_netlogon_neg_flags_8;
static int hf_netlogon_neg_flags_4;
static int hf_netlogon_neg_flags_2;
static int hf_netlogon_neg_flags_1;
static int hf_netlogon_minworkingsetsize;
static int hf_netlogon_maxworkingsetsize;
static int hf_netlogon_pagedpoollimit;
static int hf_netlogon_pagefilelimit;
static int hf_netlogon_timelimit;
static int hf_netlogon_nonpagedpoollimit;
/* static int hf_netlogon_pac_size; */
/* static int hf_netlogon_pac_data; */
/* static int hf_netlogon_auth_size; */
/* static int hf_netlogon_auth_data; */
static int hf_netlogon_cipher_len;
static int hf_netlogon_cipher_maxlen;
static int hf_netlogon_cipher_current_data;
static int hf_netlogon_cipher_current_set_time;
static int hf_netlogon_cipher_old_data;
static int hf_netlogon_cipher_old_set_time;
static int hf_netlogon_priv;
static int hf_netlogon_privilege_entries;
static int hf_netlogon_privilege_control;
static int hf_netlogon_privilege_name;
static int hf_netlogon_systemflags;
static int hf_netlogon_pdc_connection_status;
static int hf_netlogon_tc_connection_status;
static int hf_netlogon_restart_state;
static int hf_netlogon_attrs;
static int hf_netlogon_lsapolicy_len;
/* static int hf_netlogon_lsapolicy_referentid; */
/* static int hf_netlogon_lsapolicy_pointer; */
static int hf_netlogon_count;
static int hf_netlogon_entries;
static int hf_netlogon_minpasswdlen;
static int hf_netlogon_passwdhistorylen;
static int hf_netlogon_level16;
static int hf_netlogon_validation_level;
static int hf_netlogon_reference;
static int hf_netlogon_next_reference;
static int hf_netlogon_timestamp;
static int hf_netlogon_level;
static int hf_netlogon_challenge;
static int hf_netlogon_reserved;
static int hf_netlogon_audit_retention_period;
static int hf_netlogon_auditing_mode;
static int hf_netlogon_max_audit_event_count;
static int hf_netlogon_event_audit_option;
static int hf_netlogon_unknown_string;
static int hf_netlogon_new_password;
static int hf_netlogon_trust_extension;
static int hf_netlogon_trust_max;
static int hf_netlogon_trust_offset;
static int hf_netlogon_trust_len;
static int hf_netlogon_opaque_buffer_enc;
static int hf_netlogon_opaque_buffer_dec;
static int hf_netlogon_opaque_buffer_size;
static int hf_netlogon_dummy_string;
static int hf_netlogon_dummy_string2;
static int hf_netlogon_dummy_string3;
static int hf_netlogon_dummy_string4;
static int hf_netlogon_dummy_string5;
static int hf_netlogon_dummy_string6;
static int hf_netlogon_dummy_string7;
static int hf_netlogon_dummy_string8;
static int hf_netlogon_dummy_string9;
static int hf_netlogon_dummy_string10;
static int hf_netlogon_unknown_short;
static int hf_netlogon_unknown_long;
static int hf_netlogon_dummy1_long;
static int hf_netlogon_dummy2_long;
static int hf_netlogon_dummy3_long;
static int hf_netlogon_dummy4_long;
static int hf_netlogon_dummy5_long;
static int hf_netlogon_dummy6_long;
static int hf_netlogon_dummy7_long;
static int hf_netlogon_dummy8_long;
static int hf_netlogon_dummy9_long;
static int hf_netlogon_dummy10_long;
static int hf_netlogon_unknown_char;
static int hf_netlogon_logon_time;
static int hf_netlogon_logoff_time;
static int hf_netlogon_last_logoff_time;
static int hf_netlogon_kickoff_time;
static int hf_netlogon_pwd_age;
static int hf_netlogon_pwd_last_set_time;
static int hf_netlogon_pwd_can_change_time;
static int hf_netlogon_pwd_must_change_time;
static int hf_netlogon_nt_chal_resp;
static int hf_netlogon_lm_chal_resp;
static int hf_netlogon_credential;
static int hf_netlogon_acct_name;
static int hf_netlogon_acct_desc;
static int hf_netlogon_group_desc;
static int hf_netlogon_full_name;
static int hf_netlogon_comment;
static int hf_netlogon_parameters;
static int hf_netlogon_logon_script;
static int hf_netlogon_profile_path;
static int hf_netlogon_home_dir;
static int hf_netlogon_dir_drive;
static int hf_netlogon_logon_count;
static int hf_netlogon_logon_count16;
static int hf_netlogon_bad_pw_count;
static int hf_netlogon_bad_pw_count16;
static int hf_netlogon_user_rid;
static int hf_netlogon_alias_rid;
static int hf_netlogon_group_rid;
static int hf_netlogon_logon_srv;
/* static int hf_netlogon_principal; */
static int hf_netlogon_logon_dom;
static int hf_netlogon_resourcegroupcount;
static int hf_netlogon_accountdomaingroupcount;
static int hf_netlogon_domaingroupcount;
static int hf_netlogon_membership_domains_count;
static int hf_netlogon_downlevel_domain_name;
static int hf_netlogon_dns_domain_name;
static int hf_netlogon_ad_client_dns_name;
static int hf_netlogon_domain_name;
static int hf_netlogon_domain_create_time;
static int hf_netlogon_domain_modify_time;
static int hf_netlogon_modify_count;
static int hf_netlogon_db_modify_time;
static int hf_netlogon_db_create_time;
static int hf_netlogon_oem_info;
static int hf_netlogon_serial_number;
static int hf_netlogon_num_rids;
static int hf_netlogon_num_trusts;
static int hf_netlogon_num_controllers;
static int hf_netlogon_num_sid;
static int hf_netlogon_computer_name;
static int hf_netlogon_site_name;
static int hf_netlogon_trusted_dc_name;
static int hf_netlogon_dc_name;
static int hf_netlogon_dc_site_name;
static int hf_netlogon_dns_forest_name;
static int hf_netlogon_dc_address;
static int hf_netlogon_dc_address_type;
static int hf_netlogon_client_site_name;
static int hf_netlogon_workstation;
static int hf_netlogon_workstation_site_name;
static int hf_netlogon_os_version;
static int hf_netlogon_workstation_os;
static int hf_netlogon_workstation_flags;
static int hf_netlogon_supportedenctypes;

static int hf_netlogon_workstations;
static int hf_netlogon_workstation_fqdn;
static int hf_netlogon_group_name;
static int hf_netlogon_alias_name;
static int hf_netlogon_country;
static int hf_netlogon_codepage;
static int hf_netlogon_flags;
static int hf_netlogon_trust_attribs;
static int hf_netlogon_trust_attribs_non_transitive;
static int hf_netlogon_trust_attribs_uplevel_only;
static int hf_netlogon_trust_attribs_quarantined_domain;
static int hf_netlogon_trust_attribs_forest_transitive;
static int hf_netlogon_trust_attribs_cross_organization;
static int hf_netlogon_trust_attribs_within_forest;
static int hf_netlogon_trust_attribs_treat_as_external;
static int hf_netlogon_trust_type;
static int hf_netlogon_trust_flags;
static int hf_netlogon_trust_flags_inbound;
static int hf_netlogon_trust_flags_outbound;
static int hf_netlogon_trust_flags_in_forest;
static int hf_netlogon_trust_flags_native_mode;
static int hf_netlogon_trust_flags_primary;
static int hf_netlogon_trust_flags_tree_root;
static int hf_netlogon_trust_parent_index;
static int hf_netlogon_user_account_control;
static int hf_netlogon_user_account_control_dont_require_preauth;
static int hf_netlogon_user_account_control_use_des_key_only;
static int hf_netlogon_user_account_control_not_delegated;
static int hf_netlogon_user_account_control_trusted_for_delegation;
static int hf_netlogon_user_account_control_smartcard_required;
static int hf_netlogon_user_account_control_encrypted_text_password_allowed;
static int hf_netlogon_user_account_control_account_auto_locked;
static int hf_netlogon_user_account_control_dont_expire_password;
static int hf_netlogon_user_account_control_server_trust_account;
static int hf_netlogon_user_account_control_workstation_trust_account;
static int hf_netlogon_user_account_control_interdomain_trust_account;
static int hf_netlogon_user_account_control_mns_logon_account;
static int hf_netlogon_user_account_control_normal_account;
static int hf_netlogon_user_account_control_temp_duplicate_account;
static int hf_netlogon_user_account_control_password_not_required;
static int hf_netlogon_user_account_control_home_directory_required;
static int hf_netlogon_user_account_control_account_disabled;
static int hf_netlogon_user_flags;
static int hf_netlogon_user_flags_extra_sids;
static int hf_netlogon_user_flags_resource_groups;
static int hf_netlogon_auth_flags;
static int hf_netlogon_pwd_expired;
static int hf_netlogon_nt_pwd_present;
static int hf_netlogon_lm_pwd_present;
static int hf_netlogon_code;
static int hf_netlogon_database_id;
static int hf_netlogon_sync_context;
static int hf_netlogon_max_size;
static int hf_netlogon_max_log_size;
static int hf_netlogon_dns_host;
static int hf_netlogon_acct_expiry_time;
static int hf_netlogon_encrypted_lm_owf_password;
static int hf_netlogon_lm_owf_password;
static int hf_netlogon_nt_owf_password;
static int hf_netlogon_param_ctrl;
static int hf_netlogon_logon_id;
static int hf_netlogon_num_deltas;
static int hf_netlogon_user_session_key;
static int hf_netlogon_blob_size;
static int hf_netlogon_blob;
static int hf_netlogon_logon_attempts;
static int hf_netlogon_authoritative;
static int hf_netlogon_secure_channel_type;
static int hf_netlogon_logonsrv_handle;
static int hf_netlogon_delta_type;
static int hf_netlogon_get_dcname_request_flags;
static int hf_netlogon_get_dcname_request_flags_force_rediscovery;
static int hf_netlogon_get_dcname_request_flags_directory_service_required;
static int hf_netlogon_get_dcname_request_flags_directory_service_preferred;
static int hf_netlogon_get_dcname_request_flags_gc_server_required;
static int hf_netlogon_get_dcname_request_flags_pdc_required;
static int hf_netlogon_get_dcname_request_flags_background_only;
static int hf_netlogon_get_dcname_request_flags_ip_required;
static int hf_netlogon_get_dcname_request_flags_kdc_required;
static int hf_netlogon_get_dcname_request_flags_timeserv_required;
static int hf_netlogon_get_dcname_request_flags_writable_required;
static int hf_netlogon_get_dcname_request_flags_good_timeserv_preferred;
static int hf_netlogon_get_dcname_request_flags_avoid_self;
static int hf_netlogon_get_dcname_request_flags_only_ldap_needed;
static int hf_netlogon_get_dcname_request_flags_is_flat_name;
static int hf_netlogon_get_dcname_request_flags_is_dns_name;
static int hf_netlogon_get_dcname_request_flags_return_dns_name;
static int hf_netlogon_get_dcname_request_flags_return_flat_name;
static int hf_netlogon_dc_flags;
static int hf_netlogon_dc_flags_pdc_flag;
static int hf_netlogon_dc_flags_gc_flag;
static int hf_netlogon_dc_flags_ldap_flag;
static int hf_netlogon_dc_flags_ds_flag;
static int hf_netlogon_dc_flags_kdc_flag;
static int hf_netlogon_dc_flags_timeserv_flag;
static int hf_netlogon_dc_flags_closest_flag;
static int hf_netlogon_dc_flags_writable_flag;
static int hf_netlogon_dc_flags_good_timeserv_flag;
static int hf_netlogon_dc_flags_ndnc_flag;
static int hf_netlogon_dc_flags_dns_controller_flag;
static int hf_netlogon_dc_flags_dns_domain_flag;
static int hf_netlogon_dc_flags_dns_forest_flag;
/* static int hf_netlogon_dnsdomaininfo; */
static int hf_netlogon_s4u2proxytarget;
static int hf_netlogon_transitedlistsize;
static int hf_netlogon_transited_service;
static int hf_netlogon_logon_duration;
static int hf_netlogon_time_created;
static int hf_netlogon_claims_set_size;
static int hf_netlogon_claims_compression_format;
static int hf_netlogon_claims_set_uncompressed_size;
static int hf_netlogon_claims_reserved_type;
static int hf_netlogon_claims_reserved_field_size;
static int hf_netlogon_claims_source_type;
static int hf_netlogon_claims_count;
static int hf_netlogon_claim_id;
static int hf_netlogon_claim_type;
static int hf_netlogon_claim_value_count;
static int hf_netlogon_claim_int64_value;
static int hf_netlogon_claim_uint64_value;
static int hf_netlogon_claim_string_value;
static int hf_netlogon_claim_boolean_value;
static int hf_netlogon_ticket_logon_options;
static int hf_netlogon_ticket_logon_options_0000000000000001;
static int hf_netlogon_ticket_logon_options_0000000000010000;
static int hf_netlogon_ticket_logon_options_0000000000020000;
static int hf_netlogon_ticket_logon_options_0000000100000000;
static int hf_netlogon_ticket_logon_options_0000000200000000;
static int hf_netlogon_ticket_logon_options_0001000000000000;
static int hf_netlogon_ticket_logon_options_0002000000000000;
static int hf_netlogon_ticket_logon_service_ticket_size;
static int hf_netlogon_ticket_logon_additional_ticket_size;
static int hf_netlogon_ticket_logon_results;
static int hf_netlogon_ticket_logon_results_0000000000000001;
static int hf_netlogon_ticket_logon_results_0000000100000000;
static int hf_netlogon_ticket_logon_results_0000000200000000;
static int hf_netlogon_ticket_logon_results_0000000400000000;
static int hf_netlogon_ticket_logon_results_0000000800000000;
static int hf_netlogon_ticket_logon_results_0000001000000000;
static int hf_netlogon_ticket_logon_results_0000002000000000;
static int hf_netlogon_ticket_logon_results_0000004000000000;
static int hf_netlogon_ticket_logon_results_0001000000000000;
static int hf_netlogon_ticket_logon_results_0002000000000000;
static int hf_netlogon_ticket_logon_results_0004000000000000;
static int hf_netlogon_ticket_logon_results_0008000000000000;
static int hf_netlogon_ticket_logon_results_0010000000000000;
static int hf_netlogon_ticket_logon_results_0020000000000000;
static int hf_netlogon_ticket_logon_results_0040000000000000;
static int hf_netlogon_ticket_logon_kerberos_status;
static int hf_netlogon_ticket_logon_netlogon_status;
static int hf_netlogon_ticket_logon_source_of_status;
static int hf_netlogon_ticket_logon_user_claims_size;
static int hf_netlogon_ticket_logon_device_claims_size;
static int hf_netlogon_ticket_logon_claims;
static int hf_netlogon_forest_trust_info_flags;
static int hf_netlogon_forest_trust_info_flags_00000001;
static int hf_netlogon_forest_trust_info;

static int ett_nt_counted_longs_as_string;
static int ett_dcerpc_netlogon;
static int ett_group_attrs;
static int ett_user_flags;
static int ett_user_account_control;
static int ett_QUOTA_LIMITS;
static int ett_IDENTITY_INFO;
static int ett_DELTA_ENUM;
static int ett_authenticate_flags;
static int ett_CYPHER_VALUE;
static int ett_UNICODE_MULTI;
static int ett_DOMAIN_CONTROLLER_INFO;
static int ett_netr_CryptPassword;
static int ett_NL_PASSWORD_VERSION;
static int ett_NL_GENERIC_RPC_DATA;
static int ett_TYPE_50;
static int ett_TYPE_52;
static int ett_DELTA_ID_UNION;
static int ett_CAPABILITIES;
static int ett_DELTA_UNION;
static int ett_LM_OWF_PASSWORD;
static int ett_NT_OWF_PASSWORD;
static int ett_GROUP_MEMBERSHIP;
static int ett_BLOB;
static int ett_DS_DOMAIN_TRUSTS;
static int ett_LSA_POLICY_INFO;
static int ett_DOMAIN_TRUST_INFO;
static int ett_trust_flags;
static int ett_trust_attribs;
static int ett_get_dcname_request_flags;
static int ett_dc_flags;
static int ett_wstr_LOGON_IDENTITY_INFO_string;
static int ett_domain_group_memberships;
static int ett_domains_group_memberships;
static int ett_netlogon_ticket_logon_options;
static int ett_netlogon_ticket_logon_results;
static int ett_netlogon_ticket_logon_claims;
static int ett_netlogon_forest_trust_info_flags;

static expert_field ei_netlogon_auth_nthash;
static expert_field ei_netlogon_session_key;

typedef struct _netlogon_auth_vars {
    uint64_t client_challenge;
    uint64_t server_challenge;
    md4_pass nthash;
    int auth_fd_num;
    uint8_t session_key[16];
    uint8_t encryption_key[16];
    uint8_t sequence[16];
    uint32_t flags;
    uint64_t seq;
    uint64_t confounder;
    uint8_t private_type;
    bool can_decrypt;
    char* client_name;
    int start;
    int next_start;
    struct _netlogon_auth_vars *next;
} netlogon_auth_vars;

static gcry_error_t prepare_session_key_cipher(netlogon_auth_vars *vars,
                                               gcry_cipher_hd_t *_cipher_hd);

typedef struct _seen_packet {
    bool isseen;
    uint32_t num;
} seen_packet;

static seen_packet seen;

static e_guid_t uuid_dcerpc_netlogon = {
    0x12345678, 0x1234, 0xabcd,
    { 0xef, 0x00, 0x01, 0x23, 0x45, 0x67, 0xcf, 0xfb }
};

static uint16_t ver_dcerpc_netlogon = 1;

static int dissect_dcerpc_8bytes (tvbuff_t *tvb, int offset, packet_info *pinfo _U_,
                                   proto_tree *tree, const uint8_t *drep,
                                   int hfindex, uint64_t *pdata)
{
    uint64_t data;

    data = ((drep[0] & DREP_LITTLE_ENDIAN)
            ? tvb_get_letoh64 (tvb, offset)
            : tvb_get_ntoh64 (tvb, offset));

    /* These fields are FT_BYTES, hence the byte order doesn't matter */
    if (tree) {
        proto_tree_add_item(tree, hfindex, tvb, offset, 8, ENC_NA);
    }
    if (pdata)
        *pdata = data;
    return offset+8;
}

static const true_false_string user_account_control_dont_require_preauth= {
    "This account DOESN'T_REQUIRE_PREAUTHENTICATION",
    "This account REQUIRES preauthentication",
};
static const true_false_string user_account_control_use_des_key_only= {
    "This account must USE_DES_KEY_ONLY for passwords",
    "This account does NOT have to use_des_key_only",
};
static const true_false_string user_account_control_not_delegated= {
    "This account is NOT_DELEGATED",
    "This might have been delegated",
};
static const true_false_string user_account_control_trusted_for_delegation= {
    "This account is TRUSTED_FOR_DELEGATION",
    "This account is NOT trusted_for_delegation",
};
static const true_false_string user_account_control_smartcard_required= {
    "This account REQUIRES_SMARTCARD to authenticate",
    "This account does NOT require_smartcard to authenticate",
};
static const true_false_string user_account_control_encrypted_text_password_allowed= {
    "This account allows ENCRYPTED_TEXT_PASSWORD",
    "This account does NOT allow encrypted_text_password",
};
static const true_false_string user_account_control_account_auto_locked= {
    "This account is AUTO_LOCKED",
    "This account is NOT auto_locked",
};
static const true_false_string user_account_control_dont_expire_password= {
    "This account DOESN'T_EXPIRE_PASSWORDs",
    "This account might expire_passwords",
};
static const true_false_string user_account_control_server_trust_account= {
    "This account is a SERVER_TRUST_ACCOUNT",
    "This account is NOT a server_trust_account",
};
static const true_false_string user_account_control_workstation_trust_account= {
    "This account is a WORKSTATION_TRUST_ACCOUNT",
    "This account is NOT a workstation_trust_account",
};
static const true_false_string user_account_control_interdomain_trust_account= {
    "This account is an INTERDOMAIN_TRUST_ACCOUNT",
    "This account is NOT an interdomain_trust_account",
};
static const true_false_string user_account_control_mns_logon_account= {
    "This account is a MNS_LOGON_ACCOUNT",
    "This account is NOT a mns_logon_account",
};
static const true_false_string user_account_control_normal_account= {
    "This account is a NORMAL_ACCOUNT",
    "This account is NOT a normal_account",
};
static const true_false_string user_account_control_temp_duplicate_account= {
    "This account is a TEMP_DUPLICATE_ACCOUNT",
    "This account is NOT a temp_duplicate_account",
};
static const true_false_string user_account_control_password_not_required= {
    "This account REQUIRES_NO_PASSWORD",
    "This account REQUIRES a password",
};
static const true_false_string user_account_control_home_directory_required= {
    "This account REQUIRES_HOME_DIRECTORY",
    "This account does NOT require_home_directory",
};
static const true_false_string user_account_control_account_disabled= {
    "This account is DISABLED",
    "This account is NOT disabled",
};

static const value_string netlogon_claims_compression_format_vals[] = {
    { 0, "COMPRESSION_FORMAT_NONE" },
    { 2, "COMPRESSION_FORMAT_LZNT1" },
    { 3, "COMPRESSION_FORMAT_XPRESS" },
    { 4, "COMPRESSION_FORMAT_XPRESS_HUFF" },
    { 0, NULL }
};

static const value_string hf_netlogon_claims_source_type_vals[] = {
    { 1, "CLAIMS_SOURCE_TYPE_AD" },
    { 2, "CLAIMS_SOURCE_TYPE_CERTIFICATE" },
    { 0, NULL }
};

static const value_string netlogon_claim_type_vals[] = {
    { 1, "CLAIM_TYPE_INT64" },
    { 2, "CLAIM_TYPE_UINT64" },
    { 3, "CLAIM_TYPE_STRING" },
    { 6, "CLAIM_TYPE_BOOLEAN" },
    { 0, NULL }
};

typedef struct _netlogon_auth_key {
    /*
     * For now we only match the client and server ip
     * addresses, as keys can be used across tcp connections.
     *
     * Also note that ServerChallenge and ServerAuthenticate
     * can be on different tcp connections!
     *
     * TODO:
     * * We could have a challenge table indexed by client, server
     *   and computer name
     * * A good ServerAuthenticate could fill a session key table
     *   indexed by computer name.
     * * A DCERPC bind/alter context could lookup the session key table
     *   and copy the session key to the DCERPC connection/auth_context.
     */
    address client;
    address server;
} netlogon_auth_key;

static int
netlogon_auth_equal (const void *k1, const void *k2)
{
    const netlogon_auth_key *key1 = (const netlogon_auth_key *)k1;
    const netlogon_auth_key *key2 = (const netlogon_auth_key *)k2;

    return (addresses_equal(&key1->client,&key2->client) && addresses_equal(&key1->server,&key2->server));
}

static unsigned
netlogon_auth_hash (const void *k)
{
    const netlogon_auth_key *key1 = (const netlogon_auth_key *)k;
    unsigned hash_val1 = 0;

    hash_val1 = add_address_to_hash(hash_val1, &key1->client);
    hash_val1 = add_address_to_hash(hash_val1, &key1->server);
    return hash_val1;
}

typedef struct _dcerpc_auth_schannel_key {
    conversation_t *conv;
    uint64_t        transport_salt;
    uint32_t        auth_context_id;
} dcerpc_auth_schannel_key;

static unsigned
dcerpc_auth_schannel_key_hash(const void *k)
{
    const dcerpc_auth_schannel_key *key = (const dcerpc_auth_schannel_key *)k;
    unsigned hash;

    hash = GPOINTER_TO_UINT(key->conv);
    /* sizeof(unsigned) might be smaller than sizeof(uint64_t) */
    hash += (unsigned)key->transport_salt;
    hash += (unsigned)(key->transport_salt << sizeof(unsigned));
    hash += key->auth_context_id;

    return hash;
}

static int
dcerpc_auth_schannel_key_equal(const void *k1, const void *k2)
{
    const dcerpc_auth_schannel_key *key1 = (const dcerpc_auth_schannel_key *)k1;
    const dcerpc_auth_schannel_key *key2 = (const dcerpc_auth_schannel_key *)k2;

    return ((key1->conv == key2->conv)
            && (key1->transport_salt == key2->transport_salt)
            && (key1->auth_context_id == key2->auth_context_id));
}

static int
netlogon_dissect_EXTRA_FLAGS(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t mask;
    static int * const extraflags[] = {
        &hf_netlogon_extra_flags_root_forest,
        &hf_netlogon_trust_flags_dc_firsthop,
        &hf_netlogon_trust_flags_rodc_to_dc,
        &hf_netlogon_trust_flags_rodc_ntlm,
        NULL
    };

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset=dissect_ndr_uint32(tvb, offset, pinfo, NULL, di, drep,
                              -1, &mask);

    proto_tree_add_bitmask_value_with_flags(parent_tree, tvb, offset-4, hf_netlogon_extraflags, ett_trust_flags, extraflags, mask, BMT_NO_APPEND);
    return offset;
}

struct LOGON_INFO_STATE;

struct LOGON_INFO_STATE_CB {
    struct LOGON_INFO_STATE *state;
    ntlmssp_blob     *response;
    const uint8_t    **name_ptr;
    int              name_levels;
};

struct LOGON_INFO_STATE {
    packet_info      *pinfo;
    proto_tree       *tree;
    uint8_t          server_challenge[8];
    ntlmssp_blob     nt_response;
    ntlmssp_blob     lm_response;
    ntlmssp_header_t ntlmssph;
    struct LOGON_INFO_STATE_CB domain_cb, acct_cb, host_cb, nt_cb, lm_cb;
};

static void dissect_LOGON_INFO_STATE_finish(struct LOGON_INFO_STATE *state)
{
    if (state->ntlmssph.acct_name != NULL &&
        state->nt_response.length >= 24 &&
        state->lm_response.length >= 24)
    {
        if (state->ntlmssph.domain_name == NULL) {
                state->ntlmssph.domain_name = (const uint8_t *)"";
        }
        if (state->ntlmssph.host_name == NULL) {
                state->ntlmssph.host_name = (const uint8_t *)"";
        }

        ntlmssp_create_session_key(state->pinfo,
                                   state->tree,
                                   &state->ntlmssph,
                                   0, /* NTLMSSP_ flags */
                                   state->server_challenge,
                                   NULL, /* encryptedsessionkey */
                                   &state->nt_response,
                                   &state->lm_response);
    }
}

static void dissect_ndr_lm_nt_byte_array(packet_info *pinfo,
                                         proto_tree *tree,
                                         proto_item *item _U_,
                                         dcerpc_info *di,
                                         tvbuff_t *tvb,
                                         int start_offset,
                                         int end_offset,
                                         void *callback_args)
{
    struct LOGON_INFO_STATE_CB *cb_ref = (struct LOGON_INFO_STATE_CB *)callback_args;
    struct LOGON_INFO_STATE *state = NULL;
    int offset = start_offset;
    uint64_t tmp;
    uint16_t len;

    if (cb_ref == NULL) {
        return;
    }
    state = cb_ref->state;

    if (di->conformant_run) {
        /* just a run to handle conformant arrays, no scalars to dissect */
        return;
    }

    /* NDR array header */
    ALIGN_TO_5_BYTES
    if (di->call_data->flags & DCERPC_IS_NDR64) {
        offset += 3 * 8;
    } else {
        offset += 3 * 4;
    }

    tmp = end_offset - offset;
    if (tmp > NTLMSSP_BLOB_MAX_SIZE) {
        tmp = NTLMSSP_BLOB_MAX_SIZE;
    }
    len = (uint16_t)tmp;
    cb_ref->response->length = len;
    cb_ref->response->contents = (uint8_t *)tvb_memdup(pinfo->pool, tvb, offset, len);
    if (len > 24) {
        dissect_ntlmv2_response(tvb, pinfo, tree, offset, len);
    }

    dissect_LOGON_INFO_STATE_finish(state);
}

static int
dissect_ndr_lm_nt_hash_cb(tvbuff_t *tvb, int offset,
                          packet_info *pinfo, proto_tree *tree,
                          dcerpc_info *di, uint8_t *drep, int hf_index,
                          dcerpc_callback_fnct_t *callback,
                          void *callback_args)
{
    uint16_t len, size;

    /* Structure starts with short, but is aligned for longs */

    ALIGN_TO_4_BYTES;

    if (di->conformant_run)
        return offset;

#if 0
    struct {
        short len;
        short size;
        [size_is(size/2), length_is(len/2), ptr] unsigned short *string;
    } HASH;

#endif

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_nt_cs_len, &len);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_nt_cs_size, &size);

    offset = dissect_ndr_pointer_cb(tvb, offset, pinfo, tree, di, drep,
                                    dissect_ndr_byte_array, NDR_POINTER_UNIQUE,
                                    "Bytes Array", hf_index, callback, callback_args);

    return offset;
}

static int
dissect_ndr_lm_nt_hash_helper(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep, int hf_index,
                              struct LOGON_INFO_STATE_CB *cb_ref)
{
    proto_tree *subtree;

    subtree = proto_tree_add_subtree(
            tree, tvb, offset, 0, ett_LM_OWF_PASSWORD, NULL,
            proto_registrar_get_name(hf_index));

    return dissect_ndr_lm_nt_hash_cb(
        tvb, offset, pinfo, subtree, di, drep, hf_index,
        dissect_ndr_lm_nt_byte_array, cb_ref);
}

static int
netlogon_dissect_USER_ACCOUNT_CONTROL(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t mask;
    static int * const uac[] = {
        &hf_netlogon_user_account_control_dont_require_preauth,
        &hf_netlogon_user_account_control_use_des_key_only,
        &hf_netlogon_user_account_control_not_delegated,
        &hf_netlogon_user_account_control_trusted_for_delegation,
        &hf_netlogon_user_account_control_smartcard_required,
        &hf_netlogon_user_account_control_encrypted_text_password_allowed,
        &hf_netlogon_user_account_control_account_auto_locked,
        &hf_netlogon_user_account_control_dont_expire_password,
        &hf_netlogon_user_account_control_server_trust_account,
        &hf_netlogon_user_account_control_workstation_trust_account,
        &hf_netlogon_user_account_control_interdomain_trust_account,
        &hf_netlogon_user_account_control_mns_logon_account,
        &hf_netlogon_user_account_control_normal_account,
        &hf_netlogon_user_account_control_temp_duplicate_account,
        &hf_netlogon_user_account_control_password_not_required,
        &hf_netlogon_user_account_control_home_directory_required,
        &hf_netlogon_user_account_control_account_disabled,
        NULL
    };

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset=dissect_ndr_uint32(tvb, offset, pinfo, NULL, di, drep,
                              -1, &mask);

    proto_tree_add_bitmask_value_with_flags(parent_tree, tvb, offset-4, hf_netlogon_user_account_control, ett_user_account_control, uac, mask, BMT_NO_APPEND);

    return offset;
}


static int
netlogon_dissect_LOGONSRV_HANDLE(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Server Handle",
                                          hf_netlogon_logonsrv_handle, 0);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL    [unique][string] wchar_t *effective_name;
 * IDL    long priv;
 * IDL    long auth_flags;
 * IDL    long logon_count;
 * IDL    long bad_pw_count;
 * IDL    long last_logon;
 * IDL    long last_logoff;
 * IDL    long logoff_time;
 * IDL    long kickoff_time;
 * IDL    long password_age;
 * IDL    long pw_can_change;
 * IDL    long pw_must_change;
 * IDL    [unique][string] wchar_t *computer;
 * IDL    [unique][string] wchar_t *domain;
 * IDL    [unique][string] wchar_t *script_path;
 * IDL    long reserved;
 */
static int
netlogon_dissect_VALIDATION_UAS_INFO(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Effective Account",
                                          hf_netlogon_acct_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_priv, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_auth_flags, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_logon_count, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_bad_pw_count, NULL);


    offset = dissect_ndr_time_t(tvb, offset, pinfo, tree, di, drep, hf_netlogon_logon_time, NULL);

    offset = dissect_ndr_time_t(tvb, offset, pinfo, tree, di, drep, hf_netlogon_last_logoff_time, NULL);

    offset = dissect_ndr_time_t(tvb, offset, pinfo, tree, di, drep, hf_netlogon_logoff_time, NULL);

    offset = dissect_ndr_time_t(tvb, offset, pinfo, tree, di, drep, hf_netlogon_kickoff_time, NULL);

    offset = dissect_ndr_time_t(tvb, offset, pinfo, tree, di, drep, hf_netlogon_pwd_age, NULL);

    offset = dissect_ndr_time_t(tvb, offset, pinfo, tree, di, drep, hf_netlogon_pwd_can_change_time, NULL);

    offset = dissect_ndr_time_t(tvb, offset, pinfo, tree, di, drep, hf_netlogon_pwd_must_change_time, NULL);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_domain_name, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Script", hf_netlogon_logon_script, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}

/*
 * IDL long NetrLogonUasLogon(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][ref][string] wchar_t *UserName,
 * IDL      [in][ref][string] wchar_t *Workstation,
 * IDL      [out][unique] VALIDATION_UAS_INFO *info
 * IDL );
 */
static int
netlogon_dissect_netrlogonuaslogon_rqst(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Account", hf_netlogon_acct_name, CB_STR_COL_INFO);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Workstation", hf_netlogon_workstation, 0);

    return offset;
}


static int
netlogon_dissect_netrlogonuaslogon_reply(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION_UAS_INFO, NDR_POINTER_UNIQUE,
                                 "VALIDATION_UAS_INFO", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   long duration;
 * IDL   short logon_count;
 * IDL } LOGOFF_UAS_INFO;
 */
static int
netlogon_dissect_LOGOFF_UAS_INFO(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    uint32_t duration;

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    duration = tvb_get_uint32(tvb, offset, DREP_ENC_INTEGER(drep));
    proto_tree_add_uint_format_value(tree, hf_netlogon_logon_duration, tvb, offset, 4, duration, "unknown time format");
    offset+= 4;

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_logon_count16, NULL);

    return offset;
}

/*
 * IDL long NetrLogonUasLogoff(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][ref][string] wchar_t *UserName,
 * IDL      [in][ref][string] wchar_t *Workstation,
 * IDL      [out][ref] LOGOFF_UAS_INFO *info
 * IDL );
 */
static int
netlogon_dissect_netrlogonuaslogoff_rqst(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Account", hf_netlogon_acct_name, CB_STR_COL_INFO);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Workstation", hf_netlogon_workstation, 0);

    return offset;
}


static int
netlogon_dissect_netrlogonuaslogoff_reply(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_LOGOFF_UAS_INFO, NDR_POINTER_REF,
                                 "LOGOFF_UAS_INFO", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}

static int
netlogon_dissect_BLOB(tvbuff_t *tvb, int offset, int length,
                      packet_info *pinfo _U_ , proto_tree *tree,
                      dcerpc_info *di _U_, uint8_t *drep _U_)
{
    proto_tree_add_item(tree, hf_netlogon_blob, tvb, offset, length,
                        ENC_NA);
    offset += length;
    return offset;
}

static int
netlogon_dissect_BYTE_array(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_BLOB);

    return offset;
}


static void cb_wstr_LOGON_IDENTITY_INFO(packet_info *pinfo, proto_tree *tree,
                                        proto_item *item, dcerpc_info *di,
                                        tvbuff_t *tvb,
                                        int start_offset, int end_offset,
                                        void *callback_args)
{
    dcerpc_call_value *dcv = (dcerpc_call_value *)di->call_data;
    struct LOGON_INFO_STATE_CB *cb_ref =
       (struct LOGON_INFO_STATE_CB *)callback_args;
    struct LOGON_INFO_STATE *state = cb_ref->state;

    cb_wstr_postprocess(pinfo, tree, item, di, tvb, start_offset, end_offset,
                        GINT_TO_POINTER(cb_ref->name_levels));

    if (*cb_ref->name_ptr == NULL) {
        *cb_ref->name_ptr = (const uint8_t *)dcv->private_data;
    }

    dissect_LOGON_INFO_STATE_finish(state);
}

static int
dissect_ndr_wstr_LOGON_IDENTITY_INFO(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep,
                                     int hf_index, int levels,
                                     struct LOGON_INFO_STATE_CB *cb_ref)
{
    proto_item *item = NULL;
    proto_tree *subtree = NULL;

    if (cb_ref == NULL) {
          return dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                            hf_index, levels);
    }

    subtree = proto_tree_add_subtree(tree, tvb, offset, 0,
                                     ett_wstr_LOGON_IDENTITY_INFO_string, &item,
                                     proto_registrar_get_name(hf_index));

    /*
     * Add 2 levels, so that the string gets attached to the
     * "Character Array" top-level item and to the top-level item
     * added above.
     */
    cb_ref->name_levels = 2 + levels;
    cb_ref->name_levels |= CB_STR_SAVE;
    return dissect_ndr_counted_string_cb(tvb, offset, pinfo, subtree, di, drep,
                                         hf_index, cb_wstr_LOGON_IDENTITY_INFO, cb_ref);
}

/*
 * IDL typedef struct {
 * IDL   UNICODESTRING LogonDomainName;
 * IDL   long ParameterControl;
 * IDL   uint64 LogonID;
 * IDL   UNICODESTRING UserName;
 * IDL   UNICODESTRING Workstation;
 * IDL } LOGON_IDENTITY_INFO;
 */
static int
netlogon_dissect_LOGON_IDENTITY_INFO(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *parent_tree,
                                     dcerpc_info *di, uint8_t *drep,
                                     struct LOGON_INFO_STATE *state)
{
    struct LOGON_INFO_STATE_CB *domain_cb = NULL;
    struct LOGON_INFO_STATE_CB *acct_cb = NULL;
    struct LOGON_INFO_STATE_CB *host_cb = NULL;
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if (state != NULL) {
        domain_cb = &state->domain_cb;
        acct_cb = &state->acct_cb;
        host_cb = &state->host_cb;
    }

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_IDENTITY_INFO, &item, "IDENTITY_INFO:");
    }

    /* XXX: It would be nice to get the domain and account name
       displayed in COL_INFO. */

    offset = dissect_ndr_wstr_LOGON_IDENTITY_INFO(tvb, offset, pinfo, tree, di, drep,
                                                  hf_netlogon_logon_dom, 0, domain_cb);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_param_ctrl, NULL);

    offset = dissect_ndr_duint32(tvb, offset, pinfo, tree, di, drep,
                                 hf_netlogon_logon_id, NULL);

    offset = dissect_ndr_wstr_LOGON_IDENTITY_INFO(tvb, offset, pinfo, tree, di, drep,
                                                  hf_netlogon_acct_name, 1, acct_cb);

    offset = dissect_ndr_wstr_LOGON_IDENTITY_INFO(tvb, offset, pinfo, tree, di, drep,
                                                  hf_netlogon_workstation, 0, host_cb);

#ifdef REMOVED
    /* NetMon does not recognize these bytes. I'll comment them out until someone complains */
    /* XXX 8 extra bytes here */
    /* there were 8 extra bytes, either here or in NETWORK_INFO that does not match
       the idl file. Could be a bug in either the NETLOGON implementation or in the
       idl file.
    */
    offset = netlogon_dissect_8_unknown_bytes(tvb, offset, pinfo, tree, di, drep);
#endif

    proto_item_set_len(item, offset-old_offset);
    return offset;
}


/*
 * IDL typedef struct {
 * IDL   char password[16];
 * IDL } LM_OWF_PASSWORD;
 */
static int
netlogon_dissect_LM_OWF_PASSWORD(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo _U_, proto_tree *parent_tree,
                                 dcerpc_info *di, uint8_t *drep _U_)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 16,
                                   ett_LM_OWF_PASSWORD, &item, "LM_OWF_PASSWORD:");
    }

    proto_tree_add_item(tree, hf_netlogon_lm_owf_password, tvb, offset, 16,
                        ENC_NA);
    offset += 16;

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   char password[16];
 * IDL } NT_OWF_PASSWORD;
 */
static int
netlogon_dissect_NT_OWF_PASSWORD(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo _U_, proto_tree *parent_tree,
                                 dcerpc_info *di, uint8_t *drep _U_)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 16,
                                   ett_NT_OWF_PASSWORD, &item, "NT_OWF_PASSWORD:");
    }

    proto_tree_add_item(tree, hf_netlogon_nt_owf_password, tvb, offset, 16,
                        ENC_NA);
    offset += 16;

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   LOGON_IDENTITY_INFO identity_info;
 * IDL   LM_OWF_PASSWORD lmpassword;
 * IDL   NT_OWF_PASSWORD ntpassword;
 * IDL } INTERACTIVE_INFO;
 */
static int
netlogon_dissect_INTERACTIVE_INFO(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGON_IDENTITY_INFO(tvb, offset,
                                                  pinfo, tree, di, drep,
                                                  NULL);

    offset = netlogon_dissect_LM_OWF_PASSWORD(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = netlogon_dissect_NT_OWF_PASSWORD(tvb, offset,
                                              pinfo, tree, di, drep);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   char chl[8];
 * IDL } CHALLENGE;
 */
static int
netlogon_dissect_CHALLENGE(tvbuff_t *tvb, int offset,
                           packet_info *pinfo _U_, proto_tree *tree,
                           dcerpc_info *di, uint8_t *drep _U_)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    proto_tree_add_item(tree, hf_netlogon_challenge, tvb, offset, 8,
                        ENC_NA);
    offset += 8;

    return offset;
}

static int
netlogon_dissect_NETWORK_INFO(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    struct LOGON_INFO_STATE *state =
        (struct LOGON_INFO_STATE *)di->private_data;
    int              last_offset;
    struct LOGON_INFO_STATE_CB *nt_cb = NULL;
    struct LOGON_INFO_STATE_CB *lm_cb = NULL;

    if (state == NULL) {
        state = wmem_new0(pinfo->pool, struct LOGON_INFO_STATE);
        state->ntlmssph = (ntlmssp_header_t) { .type = NTLMSSP_AUTH, };
        state->domain_cb.state = state;
        state->domain_cb.name_ptr = &state->ntlmssph.domain_name;
        state->acct_cb.state = state;
        state->acct_cb.name_ptr = &state->ntlmssph.acct_name;
        state->host_cb.state = state;
        state->host_cb.name_ptr = &state->ntlmssph.host_name;
        state->nt_cb.state = state;
        state->nt_cb.response = &state->nt_response;
        state->lm_cb.state = state;
        state->lm_cb.response = &state->lm_response;
        di->private_data = state;
    }
    state->pinfo = pinfo;
    state->tree = tree;

    offset = netlogon_dissect_LOGON_IDENTITY_INFO(tvb, offset,
                                                  pinfo, tree, di, drep,
                                                  state);
    last_offset = offset;
    offset = netlogon_dissect_CHALLENGE(tvb, offset,
                                        pinfo, tree, di, drep);
    if (offset == (last_offset + 8)) {
        tvb_memcpy(tvb, state->server_challenge, last_offset, 8);
        nt_cb = &state->nt_cb;
        lm_cb = &state->lm_cb;
    }
    offset = dissect_ndr_lm_nt_hash_helper(tvb,offset,pinfo, tree, di, drep,
                                           hf_netlogon_nt_chal_resp,
                                           nt_cb);
    offset = dissect_ndr_lm_nt_hash_helper(tvb,offset,pinfo, tree, di, drep,
                                           hf_netlogon_lm_chal_resp,
                                           lm_cb);
    return offset;
}


/*
 * IDL typedef struct {
 * IDL   LOGON_IDENTITY_INFO logon_info;
 * IDL   LM_OWF_PASSWORD lmpassword;
 * IDL   NT_OWF_PASSWORD ntpassword;
 * IDL } SERVICE_INFO;
 */
static int
netlogon_dissect_SERVICE_INFO(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGON_IDENTITY_INFO(tvb, offset,
                                                  pinfo, tree, di, drep,
                                                  NULL);

    offset = netlogon_dissect_LM_OWF_PASSWORD(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = netlogon_dissect_NT_OWF_PASSWORD(tvb, offset,
                                              pinfo, tree, di, drep);

    return offset;
}

static int
netlogon_dissect_GENERIC_INFO(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGON_IDENTITY_INFO(tvb, offset,
                                                  pinfo, tree, di, drep,
                                                  NULL);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_package_name, 0|CB_STR_SAVE);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_data_length, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_REF,
                                 "Logon Data", -1);
    return offset;
}

static int
netlogon_dissect_KRB5_TICKET_BLOB(tvbuff_t *tvb, int offset, int length,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep _U_)
{
    tvbuff_t *subtvb = NULL;

    if (di->conformant_run) {
        return offset;
    }

    subtvb = tvb_new_subset_length(tvb, offset, length);
    offset += length;
    dissect_kerberos_main(subtvb, pinfo, tree, false, NULL);
    return offset;
}

static int
netlogon_dissect_BYTE_ARRAY_AS_KRB5_TICKET(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo, proto_tree *tree,
                                           dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_KRB5_TICKET_BLOB);

    return offset;
}

static int
netlogon_dissect_TICKET_INFO(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *tree,
                             dcerpc_info *di, uint8_t *drep)
{
    static int * const hf_netlogon_ticket_logon_options_bits[] = {
        &hf_netlogon_ticket_logon_options_0000000000000001,
        &hf_netlogon_ticket_logon_options_0000000000010000,
        &hf_netlogon_ticket_logon_options_0000000000020000,
        &hf_netlogon_ticket_logon_options_0000000100000000,
        &hf_netlogon_ticket_logon_options_0000000200000000,
        &hf_netlogon_ticket_logon_options_0001000000000000,
        &hf_netlogon_ticket_logon_options_0002000000000000,
        NULL
    };
    uint64_t options = 0;

    if (di->conformant_run) {
        /* just a run to handle conformant arrays, no scalars to dissect */
        return offset;
    }

    offset = netlogon_dissect_LOGON_IDENTITY_INFO(tvb, offset,
                                                  pinfo, tree, di, drep,
                                                  NULL);

    offset = dissect_ndr_uint64(tvb, offset, pinfo, tree, di, drep,
                                -1, &options);
    proto_tree_add_bitmask_value_with_flags(tree, tvb, offset-8,
                                            hf_netlogon_ticket_logon_options,
                                            ett_netlogon_ticket_logon_options,
                                            hf_netlogon_ticket_logon_options_bits,
                                            options,
                                            BMT_NO_APPEND);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_ticket_logon_service_ticket_size, NULL);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_ARRAY_AS_KRB5_TICKET, NDR_POINTER_UNIQUE,
                                 "Service Ticket", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_ticket_logon_additional_ticket_size, NULL);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_ARRAY_AS_KRB5_TICKET, NDR_POINTER_UNIQUE,
                                 "Additional Ticket", -1);

    return offset;
}

/*
 * IDL typedef [switch_type(short)] union {
 * IDL    [case(1)][unique] INTERACTIVE_INFO *iinfo;
 * IDL    [case(2)][unique] NETWORK_INFO *ninfo;
 * IDL    [case(3)][unique] SERVICE_INFO *sinfo;
 * IDL } LEVEL;
 */
static int
netlogon_dissect_LEVEL(tvbuff_t *tvb, int offset,
                       packet_info *pinfo, proto_tree *tree,
                       dcerpc_info *di, uint8_t *drep)
{
    uint16_t level = 0;

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level16, &level);
    ALIGN_TO_4_BYTES;
    switch(level){
    case 1:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_INTERACTIVE_INFO, NDR_POINTER_UNIQUE,
                                     "INTERACTIVE_INFO:", -1);
        break;
    case 2:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_NETWORK_INFO, NDR_POINTER_UNIQUE,
                                     "NETWORK_INFO:", -1);
        break;
    case 3:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_SERVICE_INFO, NDR_POINTER_UNIQUE,
                                     "SERVICE_INFO:", -1);
        break;
    case 4:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_GENERIC_INFO, NDR_POINTER_UNIQUE,
                                     "GENERIC_INFO:", -1);
        break;
    case 5:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_INTERACTIVE_INFO, NDR_POINTER_UNIQUE,
                                     "INTERACTIVE_TRANSITIVE_INFO:", -1);
        break;
    case 6:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_NETWORK_INFO, NDR_POINTER_UNIQUE,
                                     "NETWORK_TRANSITIVE_INFO", -1);
        break;
    case 7:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_SERVICE_INFO, NDR_POINTER_UNIQUE,
                                     "SERVICE_TRANSITIVE_INFO", -1);
        break;
    case 8:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_TICKET_INFO, NDR_POINTER_UNIQUE,
                                     "TICKET_INFO", -1);
        break;
    }
    return offset;
}

/*
 * IDL typedef struct {
 * IDL   char cred[8];
 * IDL } CREDENTIAL;
 */
static int
netlogon_dissect_CREDENTIAL(tvbuff_t *tvb, int offset,
                            packet_info *pinfo _U_, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep _U_)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    proto_tree_add_item(tree, hf_netlogon_credential, tvb, offset, 8,
                        ENC_NA);
    offset += 8;

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   CREDENTIAL cred;
 * IDL   long timestamp;
 * IDL } AUTHENTICATOR;
 */
static int
netlogon_dissect_AUTHENTICATOR(tvbuff_t *tvb, int offset,
                               packet_info *pinfo, proto_tree *tree,
                               dcerpc_info *di, uint8_t *drep)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    ALIGN_TO_4_BYTES;

    offset = netlogon_dissect_CREDENTIAL(tvb, offset,
                                         pinfo, tree, di, drep);

    /*
     * XXX - this appears to be a UNIX time_t in some credentials, but
     * appears to be random junk in other credentials.
     * For example, it looks like a UNIX time_t in "credential"
     * AUTHENTICATORs, but like random junk in "return_authenticator"
     * AUTHENTICATORs.
     */
    proto_tree_add_item(tree, hf_netlogon_timestamp, tvb, offset, 4, ENC_TIME_SECS|ENC_LITTLE_ENDIAN);
    offset+= 4;

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   long user_id;
 * IDL   long attributes;
 * IDL } GROUP_MEMBERSHIP;
 */
static int
netlogon_dissect_GROUP_MEMBERSHIP(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *parent_tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    uint32_t rid = 0;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_GROUP_MEMBERSHIP, &item, "GROUP_MEMBERSHIP:");
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_group_rid, &rid);
    if (tree) {
        proto_item_append_text(item, " RID=%"PRIu32"", rid);
    }

    offset = dissect_ndr_nt_SE_GROUP_ATTRIBUTES(tvb, offset, pinfo, tree, di, drep);

    return offset;
}

static int
netlogon_dissect_GROUP_MEMBERSHIP_ARRAY(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree,
                                        dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_GROUP_MEMBERSHIP);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   char user_session_key[16];
 * IDL } USER_SESSION_KEY;
 */
static int
netlogon_dissect_USER_SESSION_KEY(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo _U_, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep _U_)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    proto_tree_add_item(tree, hf_netlogon_user_session_key, tvb, offset, 16,
                        ENC_NA);
    offset += 16;

    return offset;
}



static const true_false_string user_flags_extra_sids= {
    "The EXTRA_SIDS bit is SET",
    "The extra_sids is NOT set",
};
static const true_false_string user_flags_resource_groups= {
    "The RESOURCE_GROUPS bit is SET",
    "The resource_groups is NOT set",
};
static int
netlogon_dissect_USER_FLAGS(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t mask;
    static int * const flags[] = {
        &hf_netlogon_user_flags_resource_groups,
        &hf_netlogon_user_flags_extra_sids,
        NULL
    };

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset=dissect_ndr_uint32(tvb, offset, pinfo, NULL, di, drep,
                              -1, &mask);

    proto_tree_add_bitmask_value_with_flags(parent_tree, tvb, offset-4, hf_netlogon_user_flags, ett_user_flags, flags, mask, BMT_NO_APPEND);
    return offset;
}

static int
netlogon_dissect_GROUP_MEMBERSHIPS(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep,
                                   int hf_count, const char *array_name)
{
    uint32_t rgc;

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_count, &rgc);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_GROUP_MEMBERSHIP_ARRAY, NDR_POINTER_UNIQUE,
                                 array_name, -1);

    return offset;
}

static int
netlogon_dissect_DOMAIN_GROUP_MEMBERSHIPS(tvbuff_t *tvb, int offset,
                        packet_info *pinfo, proto_tree *parent_tree,
                        dcerpc_info *di, uint8_t *drep,
                        int hf_count, const char *name)
{
        proto_item *item=NULL;
        proto_tree *tree=NULL;
        int old_offset=offset;

        if(parent_tree){
                tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                              ett_domain_group_memberships,
                                              &item, name);
        }

        offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);

        offset = netlogon_dissect_GROUP_MEMBERSHIPS(tvb, offset,
                                                    pinfo, tree,
                                                    di, drep,
                                                    hf_count,
                                                    "GroupIDs");

        proto_item_set_len(item, offset-old_offset);
        return offset;
}

static int
netlogon_dissect_DOMAIN_GROUP_MEMBERSHIPS_WRAPPER(tvbuff_t *tvb, int offset,
                        packet_info *pinfo, proto_tree *tree,
                        dcerpc_info *di, uint8_t *drep)
{
        return netlogon_dissect_DOMAIN_GROUP_MEMBERSHIPS(tvb, offset,
                                                         pinfo, tree,
                                                         di, drep,
                                                         hf_netlogon_domaingroupcount,
                                                         "DomainGroupIDs");
}

static int
netlogon_dissect_DOMAIN_GROUP_MEMBERSHIP_ARRAY(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree,
                                        dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DOMAIN_GROUP_MEMBERSHIPS_WRAPPER);

    return offset;
}

static int
netlogon_dissect_DOMAINS_GROUP_MEMBERSHIPS(tvbuff_t *tvb, int offset,
                        packet_info *pinfo, proto_tree *parent_tree,
                        dcerpc_info *di, uint8_t *drep,
                        int hf_count, const char *name)
{
        proto_item *item=NULL;
        proto_tree *tree=NULL;
        int old_offset=offset;
        uint32_t rgc;

        if(parent_tree){
                tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                              ett_domains_group_memberships,
                                              &item, name);
        }

        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_count, &rgc);

        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DOMAIN_GROUP_MEMBERSHIP_ARRAY,
                                     NDR_POINTER_UNIQUE,
                                     name, -1);

        proto_item_set_len(item, offset-old_offset);
        return offset;
}

/*
 * IDL typedef struct {
 * IDL   uint64 LogonTime;
 * IDL   uint64 LogoffTime;
 * IDL   uint64 KickOffTime;
 * IDL   uint64 PasswdLastSet;
 * IDL   uint64 PasswdCanChange;
 * IDL   uint64 PasswdMustChange;
 * IDL   unicodestring effectivename;
 * IDL   unicodestring fullname;
 * IDL   unicodestring logonscript;
 * IDL   unicodestring profilepath;
 * IDL   unicodestring homedirectory;
 * IDL   unicodestring homedirectorydrive;
 * IDL   short LogonCount;
 * IDL   short BadPasswdCount;
 * IDL   long userid;
 * IDL   long primarygroup;
 * IDL   long groupcount;
 * IDL   [unique][size_is(groupcount)] GROUP_MEMBERSHIP *groupids;
 * IDL   long userflags;
 * IDL   USER_SESSION_KEY key;
 * IDL   unicodestring logonserver;
 * IDL   unicodestring domainname;
 * IDL   [unique] SID logondomainid;
 * IDL   long expansionroom[2];
 * IDL   long useraccountcontrol;
 * IDL   long expansionroom[7];
 * IDL } VALIDATION_SAM_INFO;
 */
static int
netlogon_dissect_VALIDATION_SAM_INFO(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logon_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logoff_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_kickoff_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_last_set_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_can_change_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_must_change_time);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_acct_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_full_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_script, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_profile_path, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_home_dir, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dir_drive, 0);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_logon_count16, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_bad_pw_count16, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_user_rid, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_group_rid, NULL);

    offset = netlogon_dissect_GROUP_MEMBERSHIPS(tvb, offset,
                              pinfo, tree, di, drep,
                              hf_netlogon_num_rids,
                              "GroupIDs");

    offset = netlogon_dissect_USER_FLAGS(tvb, offset,
                                         pinfo, tree, di, drep);

    offset = netlogon_dissect_USER_SESSION_KEY(tvb, offset,
                                               pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_srv, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy1_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy2_long, NULL);

    offset = netlogon_dissect_USER_ACCOUNT_CONTROL(tvb, offset,
                                                   pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy4_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy5_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy6_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy7_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy8_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy9_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy10_long, NULL);

    return offset;
}



/*
 * IDL typedef struct {
 * IDL   uint64 LogonTime;
 * IDL   uint64 LogoffTime;
 * IDL   uint64 KickOffTime;
 * IDL   uint64 PasswdLastSet;
 * IDL   uint64 PasswdCanChange;
 * IDL   uint64 PasswdMustChange;
 * IDL   unicodestring effectivename;
 * IDL   unicodestring fullname;
 * IDL   unicodestring logonscript;
 * IDL   unicodestring profilepath;
 * IDL   unicodestring homedirectory;
 * IDL   unicodestring homedirectorydrive;
 * IDL   short LogonCount;
 * IDL   short BadPasswdCount;
 * IDL   long userid;
 * IDL   long primarygroup;
 * IDL   long groupcount;
 * IDL   [unique] GROUP_MEMBERSHIP *groupids;
 * IDL   long userflags;
 * IDL   USER_SESSION_KEY key;
 * IDL   unicodestring logonserver;
 * IDL   unicodestring domainname;
 * IDL   [unique] SID logondomainid;
 * IDL   long expansionroom[2];
 * IDL   long useraccountcontrol;
 * IDL   long expansionroom[7];
 * IDL   long sidcount;
 * IDL   [unique] SID_AND_ATTRIBS;
 * IDL } VALIDATION_SAM_INFO2;
 */
static int
netlogon_dissect_VALIDATION_SAM_INFO2(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_VALIDATION_SAM_INFO(tvb,offset,pinfo,tree,di,drep);
#if 0
    int i;

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logon_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logoff_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_kickoff_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_last_set_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_can_change_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_must_change_time);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_acct_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_full_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_script, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_profile_path, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_home_dir, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dir_drive, 0);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_logon_count16, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_bad_pw_count16, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_user_rid, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_group_rid, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_rids, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_GROUP_MEMBERSHIP_ARRAY, NDR_POINTER_UNIQUE,
                                 "GROUP_MEMBERSHIP_ARRAY", -1);

    offset = netlogon_dissect_USER_FLAGS(tvb, offset,
                                         pinfo, tree, di, drep);

    offset = netlogon_dissect_USER_SESSION_KEY(tvb, offset,
                                               pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_srv, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);

    for(i=0;i<2;i++){
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_unknown_long, NULL);
    }
    offset = netlogon_dissect_USER_ACCOUNT_CONTROL(tvb, offset,
                                                   pinfo, tree, di, drep);

    for(i=0;i<7;i++){
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_unknown_long, NULL);
    }
#endif
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_sid, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_ndr_nt_SID_AND_ATTRIBUTES_ARRAY, NDR_POINTER_UNIQUE,
                                 "SID_AND_ATTRIBUTES_ARRAY:", -1);

    return offset;
}


static int
netlogon_dissect_VALIDATION_SAM_INFO4(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_VALIDATION_SAM_INFO2(tvb,offset,pinfo,tree,di,drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_dnslogondomainname, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_upn, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string2, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string3, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string4, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string5, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string6, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string7, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string8, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string9, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string10, 0);
    return offset;
}

static int
netlogon_dissect_VALIDATION_TICKET_LOGON_CLAIMS_BLOB(tvbuff_t *tvb, int offset, int length,
                                                     packet_info *pinfo, proto_tree *tree,
                                                     dcerpc_info *di _U_, uint8_t *drep _U_)
{
    return netlogon_dissect_CLAIMS_SET_METADATA_BLOB(tvb, offset, length,
                                                     pinfo,
                                                     tree,
                                                     hf_netlogon_ticket_logon_claims,
                                                     ett_netlogon_ticket_logon_claims,
                                                     "Claims:");
}

static int
netlogon_dissect_VALIDATION_TICKET_LOGON_CLAIMS(tvbuff_t *tvb,
                                                int offset,
                                                packet_info *pinfo,
                                                proto_tree *tree,
                                                dcerpc_info *di,
                                                uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_VALIDATION_TICKET_LOGON_CLAIMS_BLOB);

    return offset;
}

static int
netlogon_dissect_VALIDATION_TICKET_LOGON(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree,
                                         dcerpc_info *di, uint8_t *drep)
{
    static int * const hf_netlogon_ticket_logon_results_bits[] = {
        &hf_netlogon_ticket_logon_results_0000000000000001,
        &hf_netlogon_ticket_logon_results_0000000100000000,
        &hf_netlogon_ticket_logon_results_0000000200000000,
        &hf_netlogon_ticket_logon_results_0000000400000000,
        &hf_netlogon_ticket_logon_results_0000000800000000,
        &hf_netlogon_ticket_logon_results_0000001000000000,
        &hf_netlogon_ticket_logon_results_0000002000000000,
        &hf_netlogon_ticket_logon_results_0000004000000000,
        &hf_netlogon_ticket_logon_results_0001000000000000,
        &hf_netlogon_ticket_logon_results_0002000000000000,
        &hf_netlogon_ticket_logon_results_0004000000000000,
        &hf_netlogon_ticket_logon_results_0008000000000000,
        &hf_netlogon_ticket_logon_results_0010000000000000,
        &hf_netlogon_ticket_logon_results_0020000000000000,
        &hf_netlogon_ticket_logon_results_0040000000000000,
        NULL
    };
    uint64_t results = 0;

    if (di->conformant_run) {
        return offset;
    }

    offset = dissect_ndr_uint64(tvb, offset, pinfo, tree, di, drep,
                                -1, &results);
    proto_tree_add_bitmask_value_with_flags(tree, tvb, offset-8,
                                            hf_netlogon_ticket_logon_results,
                                            ett_netlogon_ticket_logon_results,
                                            hf_netlogon_ticket_logon_results_bits,
                                            results,
                                            BMT_NO_APPEND);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_ticket_logon_kerberos_status, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_ticket_logon_netlogon_status, NULL);

    offset = lsarpc_dissect_struct_lsa_String(tvb, offset, pinfo, tree, di, drep,
                                              hf_netlogon_ticket_logon_source_of_status, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION_SAM_INFO4,
                                 NDR_POINTER_UNIQUE,
                                 "USER", -1);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION_SAM_INFO4,
                                 NDR_POINTER_UNIQUE,
                                 "DEVICE", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_ticket_logon_user_claims_size, NULL);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION_TICKET_LOGON_CLAIMS,
                                 NDR_POINTER_UNIQUE,
                                 "USER_CLAIMS", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_ticket_logon_device_claims_size, NULL);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION_TICKET_LOGON_CLAIMS,
                                 NDR_POINTER_UNIQUE,
                                 "DEVICE_CLAIMS", -1);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   uint64 LogonTime;
 * IDL   uint64 LogoffTime;
 * IDL   uint64 KickOffTime;
 * IDL   uint64 PasswdLastSet;
 * IDL   uint64 PasswdCanChange;
 * IDL   uint64 PasswdMustChange;
 * IDL   unicodestring effectivename;
 * IDL   unicodestring fullname;
 * IDL   unicodestring logonscript;
 * IDL   unicodestring profilepath;
 * IDL   unicodestring homedirectory;
 * IDL   unicodestring homedirectorydrive;
 * IDL   short LogonCount;
 * IDL   short BadPasswdCount;
 * IDL   long userid;
 * IDL   long primarygroup;
 * IDL   long groupcount;
 * IDL   [unique] GROUP_MEMBERSHIP *groupids;
 * IDL   long userflags;
 * IDL   USER_SESSION_KEY key;
 * IDL   unicodestring logonserver;
 * IDL   unicodestring domainname;
 * IDL   [unique] SID logondomainid;
 * IDL   long expansionroom[2];
 * IDL   long useraccountcontrol;
 * IDL   long expansionroom[7];
 * IDL   long sidcount;
 * IDL   [unique] SID_AND_ATTRIBS;
 * IDL   [unique] SID resourcegroupdomainsid;
 * IDL   long resourcegroupcount;
 qqq
 * IDL } PAC_LOGON_INFO;
 */
int
netlogon_dissect_PAC_LOGON_INFO(tvbuff_t *tvb, int offset,
                                packet_info *pinfo, proto_tree *tree,
                                dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_VALIDATION_SAM_INFO(tvb,offset,pinfo,tree,di, drep);
#if 0
    int i;

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logon_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logoff_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_kickoff_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_last_set_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_can_change_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_must_change_time);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_acct_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_full_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_script, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_profile_path, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_home_dir, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dir_drive, 0);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_logon_count16, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_bad_pw_count16, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_user_rid, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_group_rid, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_rids, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_GROUP_MEMBERSHIP_ARRAY, NDR_POINTER_UNIQUE,
                                 "GROUP_MEMBERSHIP_ARRAY", -1);

    offset = netlogon_dissect_USER_FLAGS(tvb, offset,
                                         pinfo, tree, di, drep);

    offset = netlogon_dissect_USER_SESSION_KEY(tvb, offset,
                                               pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_srv, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);

    for(i=0;i<2;i++){
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_unknown_long, NULL);
    }
    offset = netlogon_dissect_USER_ACCOUNT_CONTROL(tvb, offset,
                                                   pinfo, tree, di, drep);

    for(i=0;i<7;i++){
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_unknown_long, NULL);
    }
#endif

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_sid, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_ndr_nt_SID_AND_ATTRIBUTES_ARRAY, NDR_POINTER_UNIQUE,
                                 "SID_AND_ATTRIBUTES_ARRAY:", -1);

    offset = netlogon_dissect_DOMAIN_GROUP_MEMBERSHIPS(tvb, offset,
                              pinfo, tree, di, drep,
                              hf_netlogon_resourcegroupcount,
                              "ResourceGroupIDs");

    return offset;
}

static int
netlogon_dissect_S4U_Transited_Service_name(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree,
                                             dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_transited_service, 1);

    return offset;
}

static int
netlogon_dissect_S4U_Transited_Services_array(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree,
                                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_S4U_Transited_Service_name);

    return offset;
}

int
netlogon_dissect_PAC_S4U_DELEGATION_INFO(tvbuff_t *tvb, int offset,
                                            packet_info *pinfo, proto_tree *tree,
                                            dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_s4u2proxytarget, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_transitedlistsize, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_S4U_Transited_Services_array, NDR_POINTER_UNIQUE,
                                 "S4UTransitedServices", -1);

    return offset;
}

struct device_sid_callback_args {
    const char **device_sid_ptr;
    uint32_t user_rid;
    const char *domain_sid;
    const char *device_sid;
};

static void device_sid_callback_fnct(packet_info *pinfo _U_,
                                     proto_tree *tree _U_,
                                     proto_item *item _U_,
                                     dcerpc_info *di,
                                     tvbuff_t *tvb _U_,
                                     int start_offset _U_,
                                     int end_offset _U_,
                                     void *callback_args)
{
    struct device_sid_callback_args *args =
        (struct device_sid_callback_args *)callback_args;
    dcerpc_call_value *dcv = (dcerpc_call_value *)di->call_data;
    const char *p = NULL;
    ptrdiff_t len;

    if (di->ptype != UINT8_MAX) {
        return;
    }

    if (dcv == NULL) {
        return;
    }

    if (args == NULL) {
        return;
    }

    args->domain_sid = (const char *)dcv->private_data;
    if (args->domain_sid == NULL) {
        /* this should not happen... */
        return;
    }

    len = strnlen(args->domain_sid, 64);

    /* remove any debug info after the sid */
    p = memchr(args->domain_sid, ' ', len);
    if (p != NULL) {
        ptrdiff_t mlen = p - args->domain_sid;
        if (mlen < len) {
            len = mlen;
        }
    }
    p = memchr(args->domain_sid, '(', len);
    if (p != NULL) {
        ptrdiff_t mlen = p - args->domain_sid;
        if (mlen < len) {
            len = mlen;
        }
    }

    /*
     * we know we're called dissect_krb5_PAC_DEVICE_INFO
     * so we should allocate the device_sid on wmem_epan_scope()
     */
    args->device_sid = wmem_strdup_printf(wmem_epan_scope(),
                                          "%*.*s-%" PRIu32,
                                          (int)len, (int)len,
                                          args->domain_sid,
                                          args->user_rid);
    *args->device_sid_ptr = args->device_sid;
}

/*
 * IDL typedef struct {
 * IDL   long UserId;
 * IDL   long PrimaryGroupId;
 * IDL   SID AccountDomainId;
 * IDL   long AccountGroupCount;
 * IDL   [size_is(AccountGroupCount)] PGROUP_MEMBERSHIP AccountGroupIds;
 * IDL   ULONG SidCount;
 * IDL   [size_is(SidCount)] PKERB_SID_AND_ATTRIBUTES ExtraSids;
 * IDL   ULONG DomainGroupCount;
 * IDL   [size_is(DomainGroupCount)] PDOMAIN_GROUP_MEMBERSHIP DomainGroup;
 * IDL } PAC_DEVICE_INFO;
 */
int
netlogon_dissect_PAC_DEVICE_INFO(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    dcerpc_call_value *dcv = (dcerpc_call_value *)di->call_data;
    struct device_sid_callback_args *args = NULL;
    uint32_t *user_rid_ptr = NULL;

    if (dcv && di->ptype == UINT8_MAX && dcv->private_data) {
        args = wmem_new0(pinfo->pool, struct device_sid_callback_args);
        /*
         * dissect_krb5_PAC_DEVICE_INFO passes
         * a pointer to const char *device_sid
         */
        args->device_sid_ptr = dcv->private_data;
        user_rid_ptr = &args->user_rid;
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_user_rid, user_rid_ptr);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_group_rid, NULL);

    offset = dissect_ndr_nt_PSID_cb(tvb, offset, pinfo, tree, di, drep,
                                    device_sid_callback_fnct, args);

    offset = netlogon_dissect_GROUP_MEMBERSHIPS(tvb, offset,
                              pinfo, tree, di, drep,
                              hf_netlogon_accountdomaingroupcount,
                              "AccountDomainGroupIds");

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_sid, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_ndr_nt_SID_AND_ATTRIBUTES_ARRAY, NDR_POINTER_UNIQUE,
                                 "ExtraSids:SID_AND_ATTRIBUTES_ARRAY:", -1);

    offset = netlogon_dissect_DOMAINS_GROUP_MEMBERSHIPS(tvb, offset,
                              pinfo, tree, di, drep,
                              hf_netlogon_membership_domains_count,
                              "ExtraDomain Membership Array");

    return offset;
}

static int
netlogon_dissect_CLAIM_INT64_VALUE(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint64(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claim_int64_value, NULL);

    return offset;
}

static int
netlogon_dissect_CLAIM_INT64_ARRAY(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_INT64_VALUE);

    return offset;
}

static int
netlogon_dissect_CLAIM_INT64_VALUES(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claim_value_count, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_INT64_ARRAY, NDR_POINTER_UNIQUE,
                                 "Claim INT64 Values:", -1);
    return offset;
}

static int
netlogon_dissect_CLAIM_UINT64_VALUE(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint64(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claim_uint64_value, NULL);

    return offset;
}

static int
netlogon_dissect_CLAIM_UINT64_ARRAY(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_UINT64_VALUE);

    return offset;
}

static int
netlogon_dissect_CLAIM_UINT64_VALUES(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claim_value_count, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_UINT64_ARRAY, NDR_POINTER_UNIQUE,
                                 "Claim UINT64 Values:", -1);
    return offset;
}

static int
netlogon_dissect_CLAIM_STRING_VALUE(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Claim STRING Value",
                                          hf_netlogon_claim_string_value, 0);

    return offset;
}

static int
netlogon_dissect_CLAIM_STRING_ARRAY(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_STRING_VALUE);

    return offset;
}

static int
netlogon_dissect_CLAIM_STRING_VALUES(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claim_value_count, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_STRING_ARRAY, NDR_POINTER_UNIQUE,
                                 "Claim STRING Values:", -1);
    return offset;
}

static int
netlogon_dissect_CLAIM_BOOLEAN_VALUE(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint64(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claim_boolean_value, NULL);

    return offset;
}

static int
netlogon_dissect_CLAIM_BOOLEAN_ARRAY(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_BOOLEAN_VALUE);

    return offset;
}

static int
netlogon_dissect_CLAIM_BOOLEAN_VALUES(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claim_value_count, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIM_BOOLEAN_ARRAY, NDR_POINTER_UNIQUE,
                                 "Claim BOOLEAN Values:", -1);
    return offset;
}

static int
netlogon_dissect_CLAIMS_ENTRY_WRAPPER(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    uint1632_t type;

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Claim ID",
                                          hf_netlogon_claim_id, 0);

    offset = dissect_ndr_uint1632(tvb, offset, pinfo, tree, di, drep,
                                  hf_netlogon_claim_type, &type);

    UNION_ALIGN_TO_4_BYTES;
    switch (type) {
    case 1:
        offset = netlogon_dissect_CLAIM_INT64_VALUES(tvb, offset, pinfo, tree, di, drep);
        break;
    case 2:
        offset = netlogon_dissect_CLAIM_UINT64_VALUES(tvb, offset, pinfo, tree, di, drep);
        break;
    case 3:
        offset = netlogon_dissect_CLAIM_STRING_VALUES(tvb, offset, pinfo, tree, di, drep);
        break;
    case 6:
        offset = netlogon_dissect_CLAIM_BOOLEAN_VALUES(tvb, offset, pinfo, tree, di, drep);
        break;
    }

    return offset;
}

static int
netlogon_dissect_CLAIMS_ENTRY_ARRAY(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIMS_ENTRY_WRAPPER);

    return offset;
}

static int
netlogon_dissect_CLAIMS_ARRAY_WRAPPER(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint1632(tvb, offset, pinfo, tree, di, drep,
                                  hf_netlogon_claims_source_type, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_count, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIMS_ENTRY_ARRAY, NDR_POINTER_UNIQUE,
                                 "Claims Entries:", -1);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_ARRAYS(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIMS_ARRAY_WRAPPER);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_set_size, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CLAIMS_SET_ARRAYS, NDR_POINTER_UNIQUE,
                                 "Claims Set ARRAYS:", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_reserved_type, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_reserved_field_size, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_UNIQUE,
                                 "Reserved Field:", -1);
    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_BUFFER(tvbuff_t *tvb, int offset, int length,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *caller_di, uint1632_t format)
{
    uint8_t drep[4] = { 0x10, 0x00, 0x00, 0x00}; /* fake DREP struct */
    /* fake dcerpc_info struct */
    dcerpc_call_value call_data = { .flags = 0, };
    dcerpc_info di = { .ptype = UINT8_MAX, .call_data = &call_data, };
    tvbuff_t *subtvb = NULL;
    int suboffset = 0;

    if (caller_di->conformant_run) {
        /* just a run to handle conformant arrays, no scalars to dissect */
        return offset;
    }

    switch (format) {
    case 0:
        subtvb = tvb_new_subset_length(tvb, offset, length);
        break;
    case 2:
        subtvb = tvb_uncompress_lznt1(tvb, offset, length);
        if (subtvb != NULL) {
            add_new_data_source(pinfo, subtvb, "Claims LZNT1 decompressed");
        }
        break;
    case 3:
        subtvb = tvb_uncompress_lz77(tvb, offset, length);
        if (subtvb != NULL) {
            add_new_data_source(pinfo, subtvb, "Claims XPRESS decompressed");
        }
        break;
    case 4:
        subtvb = tvb_uncompress_lz77huff(tvb, offset, length);
        if (subtvb != NULL) {
            add_new_data_source(pinfo, subtvb, "Claims XPRESS+HUFF decompressed");
        }
        break;
    }

    if (subtvb == NULL) {
        proto_tree_add_item(tree, hf_netlogon_blob, tvb, offset, length,
                            ENC_NA);
        offset += length;
        return offset;
    }
    offset += length;

    suboffset = nt_dissect_MIDL_NDRHEADERBLOB(tree, subtvb, suboffset, &drep[0]);

    init_ndr_pointer_list(&di);
    dissect_ndr_pointer(subtvb, suboffset, pinfo, tree, &di, drep,
                        netlogon_dissect_CLAIMS_SET, NDR_POINTER_UNIQUE,
                        "Claims Set:", -1);
    free_ndr_pointer_list(&di);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_BUFFER_0(tvbuff_t *tvb, int offset, int length,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep _U_)
{
    return netlogon_dissect_CLAIMS_SET_BUFFER(tvb, offset, length,
                                              pinfo, tree, di, 0);
}

static int
netlogon_dissect_CLAIMS_SET_ucarray_0(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_CLAIMS_SET_BUFFER_0);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_BUFFER_2(tvbuff_t *tvb, int offset, int length,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep _U_)
{
    return netlogon_dissect_CLAIMS_SET_BUFFER(tvb, offset, length,
                                              pinfo, tree, di, 2);
}

static int
netlogon_dissect_CLAIMS_SET_ucarray_2(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_CLAIMS_SET_BUFFER_2);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_BUFFER_3(tvbuff_t *tvb, int offset, int length,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep _U_)
{
    return netlogon_dissect_CLAIMS_SET_BUFFER(tvb, offset, length,
                                              pinfo, tree, di, 3);
}

static int
netlogon_dissect_CLAIMS_SET_ucarray_3(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_CLAIMS_SET_BUFFER_3);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_BUFFER_4(tvbuff_t *tvb, int offset, int length,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep _U_)
{
    return netlogon_dissect_CLAIMS_SET_BUFFER(tvb, offset, length,
                                              pinfo, tree, di, 4);
}

static int
netlogon_dissect_CLAIMS_SET_ucarray_4(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_CLAIMS_SET_BUFFER_4);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_BUFFER_U(tvbuff_t *tvb, int offset, int length,
                                     packet_info *pinfo _U_, proto_tree *tree,
                                     dcerpc_info *di _U_, uint8_t *drep _U_)
{
    proto_tree_add_item(tree, hf_netlogon_blob, tvb, offset, length,
                        ENC_NA);
    offset += length;
    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_ucarray_U(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree,
                                      dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_CLAIMS_SET_BUFFER_U);

    return offset;
}

static int
netlogon_dissect_CLAIMS_SET_METADATA(tvbuff_t *tvb,
                                     int offset,
                                     packet_info *pinfo,
                                     proto_tree *tree,
                                     dcerpc_info *di,
                                     uint8_t *drep)
{
    int format_offset;
    uint1632_t format = 0;

    if (di->conformant_run) {
        /* just a run to handle conformant arrays, no scalars to dissect */
        return offset;
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_set_size, NULL);

    ALIGN_TO_4_OR_8_BYTES;
    if (di->call_data->flags & DCERPC_IS_NDR64) {
        format_offset = offset + 8;
        format = tvb_get_uint32(tvb, format_offset, DREP_ENC_INTEGER(drep));
    } else { \
        format_offset = offset + 4;
        format = tvb_get_uint16(tvb, format_offset, DREP_ENC_INTEGER(drep));
    }
    switch (format) {
    case 0:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_CLAIMS_SET_ucarray_0,
                                     NDR_POINTER_UNIQUE,
                                     "Claims Set Uncompressed:", -1);
        break;
    case 2:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_CLAIMS_SET_ucarray_2,
                                     NDR_POINTER_UNIQUE,
                                     "Claims Set LZNT1:", -1);
        break;
    case 3:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_CLAIMS_SET_ucarray_3,
                                     NDR_POINTER_UNIQUE,
                                     "Claims Set XPRESS:", -1);
        break;
    case 4:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_CLAIMS_SET_ucarray_4,
                                     NDR_POINTER_UNIQUE,
                                     "Claims Set XPRESS+HUFF:", -1);
        break;
    default:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_CLAIMS_SET_ucarray_U,
                                     NDR_POINTER_UNIQUE,
                                     "Claims Set Unknown Compression:", -1);
    }

    offset = dissect_ndr_uint1632(tvb, offset, pinfo, tree, di, drep,
                                  hf_netlogon_claims_compression_format, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_set_uncompressed_size, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_reserved_type, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_claims_reserved_field_size, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_UNIQUE,
                                 "Reserved Field:", -1);

    return offset;
}

int
netlogon_dissect_CLAIMS_SET_METADATA_BLOB(tvbuff_t *tvb,
                                          int offset,
                                          int length,
                                          packet_info *pinfo,
                                          proto_tree *parent_tree,
                                          int hf_index,
                                          int ett_index,
                                          const char *info_str)
{
    proto_item *item;
    proto_tree *tree;
    uint8_t drep[4] = { 0x10, 0x00, 0x00, 0x00}; /* fake DREP struct */
    /* fake dcerpc_info struct */
    dcerpc_call_value call_data = { .flags = 0, };
    dcerpc_info di = { .ptype = UINT8_MAX, .call_data = &call_data, };

    item = proto_tree_add_item(parent_tree, hf_index, tvb, offset, length, ENC_NA);
    tree = proto_item_add_subtree(item, ett_index);

    if (length == 0) {
        proto_tree_add_item(tree, hf_netlogon_blob, tvb, offset, length,
                            ENC_NA);
        return offset;
    }

    offset = nt_dissect_MIDL_NDRHEADERBLOB(tree, tvb, offset, &drep[0]);

    init_ndr_pointer_list(&di);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, &di, drep,
                                 netlogon_dissect_CLAIMS_SET_METADATA,
                                 NDR_POINTER_UNIQUE,
                                 info_str, -1);
    free_ndr_pointer_list(&di);

    return offset;
}

#if 0
static int
netlogon_dissect_PAC(tvbuff_t *tvb, int offset,
                     packet_info *pinfo, proto_tree *tree,
                     dcerpc_info *di, uint8_t *drep _U_)
{
    uint32_t pac_size;

    if(di->conformant_run){
        return offset;
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_pac_size, &pac_size);

    proto_tree_add_item(tree, hf_netlogon_pac_data, tvb, offset, pac_size,
                        ENC_NA);
    offset += pac_size;

    return offset;
}

static int
netlogon_dissect_AUTH(tvbuff_t *tvb, int offset,
                      packet_info *pinfo, proto_tree *tree,
                      dcerpc_info *di, uint8_t *drep _U_)
{
    uint32_t auth_size;

    if(di->conformant_run){
        return offset;
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_auth_size, &auth_size);

    proto_tree_add_item(tree, hf_netlogon_auth_data, tvb, offset, auth_size,
                        ENC_NA);
    offset += auth_size;

    return offset;
}
#endif

static int
netlogon_dissect_VALIDATION_GENERIC_INFO2 (tvbuff_t *tvb, int offset,
                                           packet_info *pinfo, proto_tree *tree,
                                           dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_data_length, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_REF,
                                 "Validation Data", -1);

    return offset;
}
/*
 * IDL typedef struct {
 * IDL   long pac_size
 * IDL   [unique][size_is(pac_size)] char *pac;
 * IDL   UNICODESTRING logondomain;
 * IDL   UNICODESTRING logonserver;
 * IDL   UNICODESTRING principalname;
 * IDL   long auth_size;
 * IDL   [unique][size_is(auth_size)] char *auth;
 * IDL   USER_SESSION_KEY user_session_key;
 * IDL   long expansionroom[2];
 * IDL   long useraccountcontrol;
 * IDL   long expansionroom[7];
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL } VALIDATION_PAC_INFO;
 */
#if 0 /* Not used (anymore ?) */
static int
netlogon_dissect_VALIDATION_PAC_INFO(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    int i;

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_pac_size, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_PAC, NDR_POINTER_UNIQUE, "PAC:", -1);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_srv, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_principal, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_auth_size, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTH, NDR_POINTER_UNIQUE, "AUTH:", -1);

    offset = netlogon_dissect_USER_SESSION_KEY(tvb, offset,
                                               pinfo, tree, di, drep);

    for(i=0;i<2;i++){
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_unknown_long, NULL);
    }
    offset = netlogon_dissect_USER_ACCOUNT_CONTROL(tvb, offset,
                                                   pinfo, tree, di, drep);

    for(i=0;i<7;i++){
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_unknown_long, NULL);
    }

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    return offset;
}
#endif

/*
 * IDL typedef [switch_type(short)] union {
 * IDL    [case(1)][unique] VALIDATION_UAS *uas;
 * IDL    [case(2)][unique] VALIDATION_SAM_INFO *sam;
 * IDL    [case(3)][unique] VALIDATION_SAM_INFO2 *sam2;
 * IDL    [case(4)][unique] VALIDATION_GENERIC_INFO *generic;
 * IDL    [case(5)][unique] VALIDATION_GENERIC_INFO *generic2;
 * IDL    [case(5)][unique] VALIDATION_GENERIC_INFO *generic2;
 * IDL    [case(6)][unique] VALIDATION_SAM_INFO4 *sam4;
 * IDL    [case(7)][unique] VALIDATION_TICKET_LOGON *ticket;
 * IDL } VALIDATION;
 */
static int
netlogon_dissect_VALIDATION(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    uint16_t level = 0;

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_validation_level, &level);

    ALIGN_TO_4_BYTES;
    switch(level){
    case 1:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_VALIDATION_UAS_INFO, NDR_POINTER_UNIQUE,
                                     "VALIDATION_UAS_INFO:", -1);
        break;
    case 2:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_VALIDATION_SAM_INFO, NDR_POINTER_UNIQUE,
                                     "VALIDATION_SAM_INFO:", -1);
        break;
    case 3:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_VALIDATION_SAM_INFO2, NDR_POINTER_UNIQUE,
                                     "VALIDATION_SAM_INFO2:", -1);
        break;
    case 4:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_VALIDATION_GENERIC_INFO2, NDR_POINTER_UNIQUE,
                                     "VALIDATION_INFO:", -1);
        break;
    case 5:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_VALIDATION_GENERIC_INFO2, NDR_POINTER_UNIQUE,
                                     "VALIDATION_INFO2:", -1);
        break;
    case 6:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_VALIDATION_SAM_INFO4, NDR_POINTER_UNIQUE,
                                     "VALIDATION_SAM_INFO4:", -1);
        break;
    case 7:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_VALIDATION_TICKET_LOGON, NDR_POINTER_UNIQUE,
                                     "VALIDATION_TICKET_LOGON:", -1);
        break;
    }
    return offset;
}

static int
netlogon_forest_trust_info(tvbuff_t *tvb, int offset, packet_info *pinfo,
                           proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = lsarpc_dissect_struct_lsa_ForestTrustInformation(tvb,
                                                              offset,
                                                              pinfo,
                                                              tree,
                                                              di,
                                                              drep,
                                                              hf_netlogon_forest_trust_info,
                                                              0);

    return offset;
}

/*
 * IDL NET_API_STATUS DsrGetForestTrustInformation(
 * IDL     [in, unique, string] LOGONSRV_HANDLE ServerName,
 * IDL     [in, unique, string] wchar_t* TrustedDomainName,
 * IDL     [in] DWORD Flags,
 * IDL     [out] PLSA_FOREST_TRUST_INFORMATION* ForestTrustInfo
 * IDL );
 */
static int
netlogon_dissect_dsrgetforesttrustinformation_rqst(tvbuff_t *tvb,
                                                   int offset,
                                                   packet_info *pinfo,
                                                   proto_tree *tree,
                                                   dcerpc_info *di,
                                                   uint8_t *drep)
{
    static int * const hf_netlogon_forest_trust_info_flags_bits[] = {
        &hf_netlogon_forest_trust_info_flags_00000001,
        NULL
    };
    uint32_t flags = 0;

    if (di->conformant_run) {
        return offset;
    }

    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Trusted Domain Name",
                                          hf_netlogon_domain_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                -1, &flags);
    proto_tree_add_bitmask_value_with_flags(tree, tvb, offset-8,
                                            hf_netlogon_forest_trust_info_flags,
                                            ett_netlogon_forest_trust_info_flags,
                                            hf_netlogon_forest_trust_info_flags_bits,
                                            flags,
                                            BMT_NO_APPEND);

    return offset;
}

static int
netlogon_dissect_dsrgetforesttrustinformation_reply(tvbuff_t *tvb,
                                                    int offset,
                                                    packet_info *pinfo,
                                                    proto_tree *tree,
                                                    dcerpc_info *di,
                                                    uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_forest_trust_info, NDR_POINTER_UNIQUE,
                                 "ForestTrustInfo:", -1);

    offset = dissect_werror(tvb, offset, pinfo, tree, di, drep,
                            hf_netlogon_werr_rc, NULL);

    return offset;
}

/*
 * IDL NTSTATUS NetrGetForestTrustInformation(
 * IDL     [in, unique, string] LOGONSRV_HANDLE ServerName,
 * IDL     [in, string] wchar_t* ComputerName,
 * IDL     [in] PNETLOGON_AUTHENTICATOR Authenticator,
 * IDL     [out] PNETLOGON_AUTHENTICATOR ReturnAuthenticator,
 * IDL     [in] DWORD Flags,
 * IDL     [out] PLSA_FOREST_TRUST_INFORMATION* ForestTrustInfo
 * IDL );
 */
static int
netlogon_dissect_netrgetforesttrustinformation_rqst(tvbuff_t *tvb,
                                                    int offset,
                                                    packet_info *pinfo,
                                                    proto_tree *tree,
                                                    dcerpc_info *di,
                                                    uint8_t *drep)
{
    static int * const hf_netlogon_forest_trust_info_flags_bits[] = {
        &hf_netlogon_forest_trust_info_flags_00000001,
        NULL
    };
    uint32_t flags = 0;

    if (di->conformant_run) {
        return offset;
    }

    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                -1, &flags);
    proto_tree_add_bitmask_value_with_flags(tree, tvb, offset-8,
                                            hf_netlogon_forest_trust_info_flags,
                                            ett_netlogon_forest_trust_info_flags,
                                            hf_netlogon_forest_trust_info_flags_bits,
                                            flags,
                                            BMT_NO_APPEND);

    return offset;
}

static int
netlogon_dissect_netrgetforesttrustinformation_reply(tvbuff_t *tvb,
                                                     int offset,
                                                     packet_info *pinfo,
                                                     proto_tree *tree,
                                                     dcerpc_info *di,
                                                     uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_forest_trust_info, NDR_POINTER_UNIQUE,
                                 "ForestTrustInfo:", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

/*
 * IDL long NetrLogonSamLogonWithFlags(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][unique][string] wchar_t *Workstation,
 * IDL      [in][unique] AUTHENTICATOR *credential,
 * IDL      [in][out][unique] AUTHENTICATOR *returnauthenticator,
 * IDL      [in] short LogonLevel,
 * IDL      [in][ref] LOGON_LEVEL *logonlevel,
 * IDL      [in] short ValidationLevel,
 * IDL      [out][ref] VALIDATION *validation,
 * IDL      [out][ref] boolean Authoritative
 * IDL      [in][out] unsigned long ExtraFlags
 * IDL );
 */
static int
netlogon_dissect_netrlogonsamlogonflags_rqst(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level16, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_LEVEL, NDR_POINTER_REF,
                                 "LEVEL: LogonLevel", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_validation_level, NULL);

    offset = netlogon_dissect_EXTRA_FLAGS(tvb, offset, pinfo, tree, di, drep);

    return offset;
}

static int
netlogon_dissect_netrlogonsamlogonflags_reply(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION, NDR_POINTER_REF,
                                 "VALIDATION:", -1);

    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_authoritative, NULL);

    offset = netlogon_dissect_EXTRA_FLAGS(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}



/*
 * IDL long NetrLogonSamLogon(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][unique][string] wchar_t *Workstation,
 * IDL      [in][unique] AUTHENTICATOR *credential,
 * IDL      [in][out][unique] AUTHENTICATOR *returnauthenticator,
 * IDL      [in] short LogonLevel,
 * IDL      [in][ref] LOGON_LEVEL *logonlevel,
 * IDL      [in] short ValidationLevel,
 * IDL      [out][ref] VALIDATION *validation,
 * IDL      [out][ref] boolean Authoritative
 * IDL );
 */
static int
netlogon_dissect_netrlogonsamlogon_rqst(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level16, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_LEVEL, NDR_POINTER_REF,
                                 "LEVEL: LogonLevel", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_validation_level, NULL);

    return offset;
}

static int
netlogon_dissect_netrlogonsamlogon_reply(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION, NDR_POINTER_REF,
                                 "VALIDATION:", -1);

    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_authoritative, NULL);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


/*
 * IDL long NetrLogonSamLogoff(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][unique][string] wchar_t *ComputerName,
 * IDL      [in][unique] AUTHENTICATOR credential,
 * IDL      [in][unique] AUTHENTICATOR return_authenticator,
 * IDL      [in] short logon_level,
 * IDL      [in][ref] LEVEL logoninformation
 * IDL );
 */
static int
netlogon_dissect_netrlogonsamlogoff_rqst(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level16, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_LEVEL, NDR_POINTER_REF,
                                 "LEVEL: logoninformation", -1);

    return offset;
}
static int
netlogon_dissect_netrlogonsamlogoff_reply(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_UNIQUE,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static void generate_hash_key(packet_info *pinfo,unsigned char is_server,netlogon_auth_key *key)
{
    if(is_server) {
        copy_address_shallow(&key->server,&pinfo->src);
        copy_address_shallow(&key->client,&pinfo->dst);
    }
    else {
        copy_address_shallow(&key->server,&pinfo->dst);
        copy_address_shallow(&key->client,&pinfo->src);
    }

}

static netlogon_auth_vars *create_global_netlogon_auth_vars(packet_info *pinfo,
                                                            const char *computer_name,
                                                            unsigned char is_server)
{
    netlogon_auth_vars *vars = NULL;
    netlogon_auth_vars *old_vars_head = NULL;
    netlogon_auth_vars *last = NULL;
    netlogon_auth_vars *cur = NULL;
    netlogon_auth_vars *new_vars_head = NULL;
    netlogon_auth_key key;

    vars = wmem_new0(wmem_file_scope(), netlogon_auth_vars);
    vars->client_name = wmem_strdup(wmem_file_scope(), computer_name);
    vars->start = pinfo->num;
    vars->auth_fd_num = -1;
    vars->next_start = -1;
    vars->next = NULL;

    generate_hash_key(pinfo, is_server, &key);
    old_vars_head = (netlogon_auth_vars *)wmem_map_lookup(netlogon_auths, &key);
    for (cur = old_vars_head; cur != NULL; last = cur, cur = cur->next) {
        if (cur->start == vars->start) {
            ws_debug("It seems that I already record this vars start packet = %d",vars->start);
            wmem_free(wmem_file_scope(), vars);
            return cur;
        }

        if (cur->start > vars->start) {
            vars->next = cur;
            vars->next_start = cur->start;
            if (last != NULL) {
                last->next = vars;
                last->next_start = vars->start;
            }
            break;
        }
        if (new_vars_head == NULL) {
            new_vars_head = cur;
        }

        if (cur->next == NULL) {
            cur->next = vars;
            cur->next_start = vars->start;
            break;
        }
        if (cur->next->start > vars->start) {
            vars->next = cur->next;
            vars->next_start = cur->next_start;
            cur->next = vars;
            cur->next_start = vars->start;
            break;
        }
    }
    if (new_vars_head == NULL) {
        new_vars_head = vars;
    }

    for (cur = new_vars_head; cur != NULL; cur = cur->next) {
        if (cur->auth_fd_num != -1) {
            ws_assert(cur->start <= cur->auth_fd_num);
            ws_abort_if_fail(cur->start <= cur->auth_fd_num);
        }
        if (cur->next == NULL) {
            ws_assert(cur->next_start == -1);
            ws_abort_if_fail(cur->next_start == -1);
            continue;
        }
        ws_assert(cur->start < cur->next->start);
        ws_abort_if_fail(cur->start < cur->next->start);
        ws_assert(cur->next_start == cur->next->start);
        ws_abort_if_fail(cur->next_start == cur->next->start);
    }

    if (old_vars_head != new_vars_head) {
        netlogon_auth_key *k = (netlogon_auth_key *)wmem_memdup(wmem_file_scope(), &key, sizeof(netlogon_auth_key));
        copy_address_wmem(wmem_file_scope(), &k->client, &key.client);
        copy_address_wmem(wmem_file_scope(), &k->server, &key.server);
        if (old_vars_head != NULL) {
                wmem_map_remove(netlogon_auths, &key);
        }
        wmem_map_insert(netlogon_auths, k, vars);
    }

    return vars;
}

static netlogon_auth_vars *find_tmp_netlogon_auth_vars(packet_info *pinfo, unsigned char is_server)
{
    netlogon_auth_vars *lvars = NULL;
    netlogon_auth_vars *avars = NULL;
    netlogon_auth_key akey;

    generate_hash_key(pinfo, is_server, &akey);
    lvars = (netlogon_auth_vars *)wmem_map_lookup(netlogon_auths, &akey);

    for (; lvars != NULL; lvars = lvars->next) {
        int fd_num = (int) pinfo->num;

        if (fd_num <= lvars->start) {
            /*
             * Before it even started,
             * can't be used..., keep
             * avars if we already found
             * one.
             */
            break;
        }
        /*
         * remember the current match,
         * but try to find a better one...
         */
        avars = lvars;
        if (lvars->auth_fd_num == -1) {
            /*
             * No ServerAuthenticate{,1,3}, keep
             * avars if we already found one,
             * but try to find a better one...
             */
            continue;
        }
        if (fd_num <= lvars->auth_fd_num) {
            /*
             * Before ServerAuthenticate{,1,3},
             * take it...
             */
            break;
        }
        /*
         * try to find a better one...
         */
        avars = NULL;
    }

    return avars;
}

static netlogon_auth_vars *find_global_netlogon_auth_vars(packet_info *pinfo, unsigned char is_server)
{
    netlogon_auth_vars *lvars = NULL;
    netlogon_auth_vars *avars = NULL;
    netlogon_auth_key akey;

    generate_hash_key(pinfo, is_server, &akey);
    lvars = (netlogon_auth_vars *)wmem_map_lookup(netlogon_auths, &akey);

    for (; lvars != NULL; lvars = lvars->next) {
        int fd_num = (int) pinfo->num;

        if (fd_num <= lvars->start) {
            /*
             * Before it even started,
             * can't be used..., keep
             * avars if we already found
             * one.
             */
            break;
        }
        if (lvars->auth_fd_num == -1) {
            /*
             * No ServerAuthenticate{,1,3},
             * no session key available,
             * just ignore...
             */
            continue;
        }
        if (fd_num <= lvars->auth_fd_num) {
            /*
             * Before ServerAuthenticate{,1,3}
             * can't be used..., keep
             * avars if we already found
             * one.
             */
            break;
        }
        /*
         * remember the current match,
         * but try to find a better one...
         */
        avars = lvars;
    }

    return avars;
}

static netlogon_auth_vars *find_or_create_schannel_netlogon_auth_vars(packet_info *pinfo,
                                                                      dcerpc_auth_info *auth_info,
                                                                      unsigned char is_server)
{
    dcerpc_auth_schannel_key skey = {
        .conv = find_or_create_conversation(pinfo),
        .transport_salt = dcerpc_get_transport_salt(pinfo),
        .auth_context_id = auth_info->auth_context_id,
    };
    dcerpc_auth_schannel_key *sk = NULL;
    netlogon_auth_vars *svars = NULL;
    netlogon_auth_vars *avars = NULL;

    svars = (netlogon_auth_vars *)wmem_map_lookup(schannel_auths, &skey);
    if (svars != NULL) {
        return svars;
    }

    avars = find_global_netlogon_auth_vars(pinfo, is_server);
    if (avars == NULL) {
        return NULL;
    }

    sk = wmem_memdup(wmem_file_scope(), &skey, sizeof(dcerpc_auth_schannel_key));
    if (sk == NULL) {
        return NULL;
    }

    svars = wmem_memdup(wmem_file_scope(), avars, sizeof(netlogon_auth_vars));
    if (svars == NULL) {
        return NULL;
    }
    svars->client_name = wmem_strdup(wmem_file_scope(), avars->client_name);
    if (svars->client_name == NULL) {
        return NULL;
    }
    svars->next_start = -1;
    svars->next = NULL;

    wmem_map_insert(schannel_auths, sk, svars);

    return svars;
}

/*
 * IDL long NetrServerReqChallenge(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][ref][string] wchar_t *ComputerName,
 * IDL      [in][ref] CREDENTIAL client_credential,
 * IDL      [out][ref] CREDENTIAL server_credential
 * IDL );
 */
static int
netlogon_dissect_netrserverreqchallenge_rqst(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    /*int oldoffset = offset;*/
    netlogon_auth_vars *vars;
    dcerpc_call_value *dcv = (dcerpc_call_value *)di->call_data;

    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset, pinfo, tree, di, drep);
    offset = dissect_ndr_pointer_cb(
        tvb, offset, pinfo, tree, di, drep,
        dissect_ndr_wchar_cvstring, NDR_POINTER_REF,
        "Computer Name", hf_netlogon_computer_name,
        cb_wstr_postprocess,
        GINT_TO_POINTER(CB_STR_COL_INFO |CB_STR_SAVE | 1));

    ws_debug("1)Len %zu offset %d txt %s",
        dcv->private_data ? strlen((char *)dcv->private_data) : 0,
        offset,
        dcv->private_data ? (char*)dcv->private_data : "(null)");
    vars = create_global_netlogon_auth_vars(pinfo, (char*)dcv->private_data, 0);
    ws_debug("2)Txt %s", vars->client_name);

    offset = dissect_dcerpc_8bytes(tvb, offset, pinfo, tree, drep,
                                   hf_client_challenge,&vars->client_challenge);

    return offset;
}

static int
netlogon_dissect_netrserverreqchallenge_reply(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    netlogon_auth_vars *vars;
    uint64_t server_challenge;

    vars = find_tmp_netlogon_auth_vars(pinfo, 1);

    offset = dissect_dcerpc_8bytes(tvb, offset, pinfo, tree, drep,
                                   hf_server_challenge, &server_challenge);
    /*offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
      netlogon_dissect_CREDENTIAL, NDR_POINTER_REF,
      "CREDENTIAL: server credential", -1);*/

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);
    if(vars != NULL) {
        vars->server_challenge = server_challenge;
    }
/*
  else
  {
  ws_debug("Vars not found in challenge reply");
  }
*/
    return offset;
}


static int
netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree,
                                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint1632(tvb, offset, pinfo, tree, di, drep,
                                  hf_netlogon_secure_channel_type, NULL);

    return offset;
}


/*
 * IDL long NetrServerAuthenticate(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][ref][string] wchar_t *UserName,
 * IDL      [in] short secure_challenge_type,
 * IDL      [in][ref][string] wchar_t *ComputerName,
 * IDL      [in][ref] CREDENTIAL client_challenge,
 * IDL      [out][ref] CREDENTIAL server_challenge
 * IDL );
 */
static int
netlogon_dissect_netrserverauthenticate_rqst(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "User Name", hf_netlogon_acct_name, CB_STR_COL_INFO);

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, CB_STR_COL_INFO);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CREDENTIAL, NDR_POINTER_REF,
                                 "CREDENTIAL: client challenge", -1);

    return offset;
}
static int
netlogon_dissect_netrserverauthenticate023_reply(tvbuff_t *tvb, int offset,
                                                 packet_info *pinfo,
                                                 proto_tree *tree,
                                                 dcerpc_info *di,
                                                 uint8_t *drep,
                                                 int version);
static int
netlogon_dissect_netrserverauthenticate_reply(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    return netlogon_dissect_netrserverauthenticate023_reply(tvb,offset,pinfo,tree,di,drep,0);
}



/*
 * IDL typedef struct {
 * IDL   char encrypted_password[16];
 * IDL } ENCRYPTED_LM_OWF_PASSWORD;
 */
static int
netlogon_dissect_ENCRYPTED_LM_OWF_PASSWORD(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo _U_, proto_tree *tree,
                                           dcerpc_info *di, uint8_t *drep _U_)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    proto_tree_add_item(tree, hf_netlogon_encrypted_lm_owf_password, tvb, offset, 16,
                        ENC_NA);
    offset += 16;

    return offset;
}

/*
 * IDL long NetrServerPasswordSet(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][ref][string] wchar_t *UserName,
 * IDL      [in] short secure_challenge_type,
 * IDL      [in][ref][string] wchar_t *ComputerName,
 * IDL      [in][ref] AUTHENTICATOR credential,
 * IDL      [in][ref] LM_OWF_PASSWORD UasNewPassword,
 * IDL      [out][ref] AUTHENTICATOR return_authenticator
 * IDL );
 */
static int
netlogon_dissect_netrserverpasswordset_rqst(tvbuff_t *tvb, int offset,
                                            packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "User Name", hf_netlogon_acct_name, 0);

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_ENCRYPTED_LM_OWF_PASSWORD, NDR_POINTER_REF,
                                 "ENCRYPTED_LM_OWF_PASSWORD: hashed_pwd", -1);

    return offset;
}
static int
netlogon_dissect_netrserverpasswordset_reply(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   [unique][string] wchar_t *UserName;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_DELETE_USER;
 */
static int
netlogon_dissect_DELTA_DELETE_USER(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Account Name", hf_netlogon_acct_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   bool SensitiveDataFlag;
 * IDL   long DataLength;
 * IDL   [unique][size_is(DataLength)] char *SensitiveData;
 * IDL } USER_PRIVATE_INFO;
 */
static int
netlogon_dissect_SENSITIVE_DATA(tvbuff_t *tvb, int offset,
                                packet_info *pinfo, proto_tree *tree,
                                dcerpc_info *di, uint8_t *drep)
{
    uint32_t data_len;

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_sensitive_data_len, &data_len);

    proto_tree_add_item(tree, hf_netlogon_sensitive_data, tvb, offset,
                        data_len, ENC_NA);
    offset += data_len;

    return offset;
}
static int
netlogon_dissect_USER_PRIVATE_INFO(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_sensitive_data_flag, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_sensitive_data_len, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_SENSITIVE_DATA, NDR_POINTER_UNIQUE,
                                 "SENSITIVE_DATA", -1);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   UNICODESTRING UserName;
 * IDL   UNICODESTRING FullName;
 * IDL   long UserID;
 * IDL   long PrimaryGroupID;
 * IDL   UNICODESTRING HomeDir;
 * IDL   UNICODESTRING HomeDirDrive;
 * IDL   UNICODESTRING LogonScript;
 * IDL   UNICODESTRING Comment;
 * IDL   UNICODESTRING Workstations;
 * IDL   NTTIME LastLogon;
 * IDL   NTTIME LastLogoff;
 * IDL   LOGON_HOURS logonhours;
 * IDL   short BadPwCount;
 * IDL   short LogonCount;
 * IDL   NTTIME PwLastSet;
 * IDL   NTTIME AccountExpires;
 * IDL   long AccountControl;
 * IDL   LM_OWF_PASSWORD lmpw;
 * IDL   NT_OWF_PASSWORD ntpw;
 * IDL   bool NTPwPresent;
 * IDL   bool LMPwPresent;
 * IDL   bool PwExpired;
 * IDL   UNICODESTRING UserComment;
 * IDL   UNICODESTRING Parameters;
 * IDL   short CountryCode;
 * IDL   short CodePage;
 * IDL   USER_PRIVATE_INFO user_private_info;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_USER;
 */
static int
netlogon_dissect_DELTA_USER(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_acct_name, 3);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_full_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_user_rid, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_group_rid, NULL);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_home_dir, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dir_drive, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_logon_script, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_acct_desc, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_workstations, 0);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logon_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_logoff_time);

    offset = dissect_ndr_nt_LOGON_HOURS(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_bad_pw_count16, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_logon_count16, NULL);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_last_set_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_acct_expiry_time);

    offset = dissect_ndr_nt_acct_ctrl(tvb, offset, pinfo, tree, di, drep);

    offset = netlogon_dissect_LM_OWF_PASSWORD(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = netlogon_dissect_NT_OWF_PASSWORD(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_nt_pwd_present, NULL);

    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_lm_pwd_present, NULL);

    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_pwd_expired, NULL);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_comment, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_parameters, 0);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_country, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_codepage, NULL);

    offset = netlogon_dissect_USER_PRIVATE_INFO(tvb, offset, pinfo, tree,
                                                di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   UNICODESTRING DomainName;
 * IDL   UNICODESTRING OEMInfo;
 * IDL   NTTIME forcedlogoff;
 * IDL   short minpasswdlen;
 * IDL   short passwdhistorylen;
 * IDL   NTTIME pwd_must_change_time;
 * IDL   NTTIME pwd_can_change_time;
 * IDL   NTTIME domain_modify_time;
 * IDL   NTTIME domain_create_time;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_DOMAIN;
 */
static int
netlogon_dissect_DELTA_DOMAIN(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_domain_name, 3);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_oem_info, 0);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_kickoff_time);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_minpasswdlen, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_passwdhistorylen, NULL);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_must_change_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_pwd_can_change_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_domain_modify_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_domain_create_time);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   UNICODESTRING groupname;
 * IDL   GROUP_MEMBERSHIP group_membership;
 * IDL   UNICODESTRING comment;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_GROUP;
 */
static int
netlogon_dissect_DELTA_GROUP(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *tree,
                             dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_group_name, 3);

    offset = netlogon_dissect_GROUP_MEMBERSHIP(tvb, offset,
                                               pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_group_desc, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   UNICODESTRING OldName;
 * IDL   UNICODESTRING NewName;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_RENAME;
 */
static int
netlogon_dissect_DELTA_RENAME(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        di->hf_index, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        di->hf_index, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


static int
netlogon_dissect_RID(tvbuff_t *tvb, int offset,
                     packet_info *pinfo, proto_tree *tree,
                     dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_user_rid, NULL);

    return offset;
}

static int
netlogon_dissect_RID_array(tvbuff_t *tvb, int offset,
                           packet_info *pinfo, proto_tree *tree,
                           dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_RID);

    return offset;
}

static int
netlogon_dissect_ATTRIB(tvbuff_t *tvb, int offset,
                        packet_info *pinfo, proto_tree *tree,
                        dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_attrs, NULL);

    return offset;
}

static int
netlogon_dissect_ATTRIB_array(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_ATTRIB);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   [unique][size_is(num_rids)] long *rids;
 * IDL   [unique][size_is(num_rids)] long *attribs;
 * IDL   long num_rids;
 * IDL   long dummy1;
 * IDL   long dummy2;
 * IDL   long dummy3;
 * IDL   long dummy4;
 * IDL } DELTA_GROUP_MEMBER;
 */
static int
netlogon_dissect_DELTA_GROUP_MEMBER(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_RID_array, NDR_POINTER_UNIQUE,
                                 "RIDs:", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_ATTRIB_array, NDR_POINTER_UNIQUE,
                                 "Attribs:", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_rids, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   UNICODESTRING alias_name;
 * IDL   long rid;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_ALIAS;
 */
static int
netlogon_dissect_DELTA_ALIAS(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *tree,
                             dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_alias_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_alias_rid, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   [unique] SID_ARRAY sids;
 * IDL   long dummy1;
 * IDL   long dummy2;
 * IDL   long dummy3;
 * IDL   long dummy4;
 * IDL } DELTA_ALIAS_MEMBER;
 */
static int
netlogon_dissect_DELTA_ALIAS_MEMBER(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_nt_PSID_ARRAY(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


static int
netlogon_dissect_EVENT_AUDIT_OPTION(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_event_audit_option, NULL);

    return offset;
}

static int
netlogon_dissect_EVENT_AUDIT_OPTIONS_ARRAY(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo, proto_tree *tree,
                                           dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_EVENT_AUDIT_OPTION);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   long pagedpoollimit;
 * IDL   long nonpagedpoollimit;
 * IDL   long minimumworkingsetsize;
 * IDL   long maximumworkingsetsize;
 * IDL   long pagefilelimit;
 * IDL   NTTIME timelimit;
 * IDL } QUOTA_LIMITS;
 */
static int
netlogon_dissect_QUOTA_LIMITS(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *parent_tree,
                              dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_QUOTA_LIMITS, &item, "QUOTA_LIMTS:");
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_pagedpoollimit, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_nonpagedpoollimit, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_minworkingsetsize, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_maxworkingsetsize, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_pagefilelimit, NULL);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_timelimit);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}


/*
 * IDL typedef struct {
 * IDL   long maxlogsize;
 * IDL   NTTIME auditretentionperiod;
 * IDL   bool auditingmode;
 * IDL   long maxauditeventcount;
 * IDL   [unique][size_is(maxauditeventcount)] long *eventauditoptions;
 * IDL   UNICODESTRING primarydomainname;
 * IDL   [unique] SID *sid;
 * IDL   QUOTA_LIMITS quota_limits;
 * IDL   NTTIME db_modify_time;
 * IDL   NTTIME db_create_time;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_POLICY;
 */
static int
netlogon_dissect_DELTA_POLICY(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_log_size, NULL);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_audit_retention_period);

    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_auditing_mode, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_audit_event_count, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_EVENT_AUDIT_OPTIONS_ARRAY, NDR_POINTER_UNIQUE,
                                 "Event Audit Options:", -1);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_domain_name, 0);

    offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);

    offset = netlogon_dissect_QUOTA_LIMITS(tvb, offset,
                                           pinfo, tree, di, drep);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_db_modify_time);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_db_create_time);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


static int
netlogon_dissect_CONTROLLER(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dc_name, 0);

    return offset;
}

static int
netlogon_dissect_CONTROLLER_ARRAY(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CONTROLLER);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   UNICODESTRING DomainName;
 * IDL   long num_controllers;
 * IDL   [unique][size_is(num_controllers)] UNICODESTRING *controller_names;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_TRUSTED_DOMAINS;
 */
static int
netlogon_dissect_DELTA_TRUSTED_DOMAINS(tvbuff_t *tvb, int offset,
                                       packet_info *pinfo, proto_tree *tree,
                                       dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_domain_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_controllers, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CONTROLLER_ARRAY, NDR_POINTER_UNIQUE,
                                 "Domain Controllers:", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


static int
netlogon_dissect_PRIV_ATTR(tvbuff_t *tvb, int offset,
                           packet_info *pinfo, proto_tree *tree,
                           dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_attrs, NULL);

    return offset;
}

static int
netlogon_dissect_PRIV_ATTR_ARRAY(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_PRIV_ATTR);

    return offset;
}

static int
netlogon_dissect_PRIV_NAME(tvbuff_t *tvb, int offset,
                           packet_info *pinfo, proto_tree *tree,
                           dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_privilege_name, 1);

    return offset;
}

static int
netlogon_dissect_PRIV_NAME_ARRAY(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_PRIV_NAME);

    return offset;
}



/*
 * IDL typedef struct {
 * IDL   long privilegeentries;
 * IDL   long provolegecontrol;
 * IDL   [unique][size_is(privilege_entries)] long *privilege_attrib;
 * IDL   [unique][size_is(privilege_entries)] UNICODESTRING *privilege_name;
 * IDL   QUOTALIMITS quotalimits;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_ACCOUNTS;
 */
static int
netlogon_dissect_DELTA_ACCOUNTS(tvbuff_t *tvb, int offset,
                                packet_info *pinfo, proto_tree *tree,
                                dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_privilege_entries, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_privilege_control, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_PRIV_ATTR_ARRAY, NDR_POINTER_UNIQUE,
                                 "PRIV_ATTR_ARRAY:", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_PRIV_NAME_ARRAY, NDR_POINTER_UNIQUE,
                                 "PRIV_NAME_ARRAY:", -1);

    offset = netlogon_dissect_QUOTA_LIMITS(tvb, offset,
                                           pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_systemflags, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   long len;
 * IDL   long maxlen;
 * IDL   [unique][size_is(maxlen)][length_is(len)] char *cipher_data;
 * IDL } CIPHER_VALUE;
 */
static int
netlogon_dissect_CIPHER_VALUE_DATA(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    uint32_t data_len;

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset = dissect_ndr_uint32 (tvb, offset, pinfo, tree, di, drep,
                                 hf_netlogon_cipher_maxlen, NULL);

    /* skip offset */
    offset += 4;

    offset = dissect_ndr_uint32 (tvb, offset, pinfo, tree, di, drep,
                                 hf_netlogon_cipher_len, &data_len);

    proto_tree_add_item(tree, di->hf_index, tvb, offset,
                        data_len, ENC_NA);
    offset += data_len;

    return offset;
}
static int
netlogon_dissect_CIPHER_VALUE(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *parent_tree,
                              dcerpc_info *di, uint8_t *drep, const char *name, int hf_index)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_CYPHER_VALUE, &item, name);
    }

    offset = dissect_ndr_uint32 (tvb, offset, pinfo, tree, di, drep,
                                 hf_netlogon_cipher_len, NULL);

    offset = dissect_ndr_uint32 (tvb, offset, pinfo, tree, di, drep,
                                 hf_netlogon_cipher_maxlen, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CIPHER_VALUE_DATA, NDR_POINTER_UNIQUE,
                                 name, hf_index);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

/*
 * IDL typedef struct {
 * IDL   CIPHER_VALUE current_cipher;
 * IDL   NTTIME current_cipher_set_time;
 * IDL   CIPHER_VALUE old_cipher;
 * IDL   NTTIME old_cipher_set_time;
 * IDL   long SecurityInformation;
 * IDL   LSA_SECURITY_DESCRIPTOR sec_desc;
 * IDL   UNICODESTRING dummy1;
 * IDL   UNICODESTRING dummy2;
 * IDL   UNICODESTRING dummy3;
 * IDL   UNICODESTRING dummy4;
 * IDL   long dummy5;
 * IDL   long dummy6;
 * IDL   long dummy7;
 * IDL   long dummy8;
 * IDL } DELTA_SECRET;
 */
static int
netlogon_dissect_DELTA_SECRET(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_CIPHER_VALUE(tvb, offset,
                                           pinfo, tree, di, drep,
                                           "CIPHER_VALUE: current cipher value",
                                           hf_netlogon_cipher_current_data);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_cipher_current_set_time);

    offset = netlogon_dissect_CIPHER_VALUE(tvb, offset,
                                           pinfo, tree, di, drep,
                                           "CIPHER_VALUE: old cipher value",
                                           hf_netlogon_cipher_old_data);

    offset = dissect_ndr_nt_NTTIME(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_cipher_old_set_time);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_security_information, NULL);

    offset = lsarpc_dissect_sec_desc_buf(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   long low_value;
 * IDL   long high_value;
 * } MODIFIED_COUNT;
 */
static int
netlogon_dissect_MODIFIED_COUNT(tvbuff_t *tvb, int offset,
                                packet_info *pinfo, proto_tree *tree,
                                dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_duint32(tvb, offset, pinfo, tree, di, drep,
                                 hf_netlogon_modify_count, NULL);

    return offset;
}


#define DT_DELTA_DOMAIN                  1
#define DT_DELTA_GROUP                   2
#define DT_DELTA_DELETE_GROUP            3
#define DT_DELTA_RENAME_GROUP            4
#define DT_DELTA_USER                    5
#define DT_DELTA_DELETE_USER             6
#define DT_DELTA_RENAME_USER             7
#define DT_DELTA_GROUP_MEMBER            8
#define DT_DELTA_ALIAS                   9
#define DT_DELTA_DELETE_ALIAS           10
#define DT_DELTA_RENAME_ALIAS           11
#define DT_DELTA_ALIAS_MEMBER           12
#define DT_DELTA_POLICY                 13
#define DT_DELTA_TRUSTED_DOMAINS        14
#define DT_DELTA_DELETE_TRUST           15
#define DT_DELTA_ACCOUNTS               16
#define DT_DELTA_DELETE_ACCOUNT         17
#define DT_DELTA_SECRET                 18
#define DT_DELTA_DELETE_SECRET          19
#define DT_DELTA_DELETE_GROUP2          20
#define DT_DELTA_DELETE_USER2           21
#define DT_MODIFIED_COUNT               22

static const value_string delta_type_vals[] = {
    { DT_DELTA_DOMAIN,          "Domain" },
    { DT_DELTA_GROUP,           "Group" },
    { DT_DELTA_DELETE_GROUP,    "Delete Group" },
    { DT_DELTA_RENAME_GROUP,    "Rename Group" },
    { DT_DELTA_USER,            "User" },
    { DT_DELTA_DELETE_USER,     "Delete User" },
    { DT_DELTA_RENAME_USER,     "Rename User" },
    { DT_DELTA_GROUP_MEMBER,    "Group Member" },
    { DT_DELTA_ALIAS,           "Alias" },
    { DT_DELTA_DELETE_ALIAS,    "Delete Alias" },
    { DT_DELTA_RENAME_ALIAS,    "Rename Alias" },
    { DT_DELTA_ALIAS_MEMBER,    "Alias Member" },
    { DT_DELTA_POLICY,          "Policy" },
    { DT_DELTA_TRUSTED_DOMAINS, "Trusted Domains" },
    { DT_DELTA_DELETE_TRUST,    "Delete Trust" },
    { DT_DELTA_ACCOUNTS,        "Accounts" },
    { DT_DELTA_DELETE_ACCOUNT,  "Delete Account" },
    { DT_DELTA_SECRET,          "Secret" },
    { DT_DELTA_DELETE_SECRET,   "Delete Secret" },
    { DT_DELTA_DELETE_GROUP2,   "Delete Group2" },
    { DT_DELTA_DELETE_USER2,    "Delete User2" },
    { DT_MODIFIED_COUNT,        "Modified Count" },
    { 0, NULL }
};
/*
 * IDL typedef [switch_type(short)] union {
 * IDL   [case(1)][unique] DELTA_DOMAIN *domain;
 * IDL   [case(2)][unique] DELTA_GROUP *group;
 * IDL   [case(3)][unique] rid only ;
 * IDL   [case(4)][unique] DELTA_RENAME_GROUP *rename_group;
 * IDL   [case(5)][unique] DELTA_USER *user;
 * IDL   [case(6)][unique] rid only ;
 * IDL   [case(7)][unique] DELTA_RENAME_USER *rename_user;
 * IDL   [case(8)][unique] DELTA_GROUP_MEMBER *group_member;
 * IDL   [case(9)][unique] DELTA_ALIAS *alias;
 * IDL   [case(10)][unique] rid only ;
 * IDL   [case(11)][unique] DELTA_RENAME_ALIAS *alias;
 * IDL   [case(12)][unique] DELTA_ALIAS_MEMBER *alias_member;
 * IDL   [case(13)][unique] DELTA_POLICY *policy;
 * IDL   [case(14)][unique] DELTA_TRUSTED_DOMAINS *trusted_domains;
 * IDL   [case(15)][unique] PSID ;
 * IDL   [case(16)][unique] DELTA_ACCOUNTS *accounts;
 * IDL   [case(17)][unique] PSID ;
 * IDL   [case(18)][unique] DELTA_SECRET *secret;
 * IDL   [case(19)][unique] string;
 * IDL   [case(20)][unique] DELTA_DELETE_GROUP2 *delete_group;
 * IDL   [case(21)][unique] DELTA_DELETE_USER2 *delete_user;
 * IDL   [case(22)][unique] MODIFIED_COUNT *modified_count;
 * IDL } DELTA_UNION;
 */
static int
netlogon_dissect_DELTA_UNION(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *parent_tree,
                             dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;
    uint16_t level = 0;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_DELTA_UNION, &item, "DELTA_UNION:");
    }

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_delta_type, &level);

    ALIGN_TO_4_BYTES;
    switch(level){
    case 1:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_DOMAIN, NDR_POINTER_UNIQUE,
                                     "DELTA_DOMAIN:", -1);
        break;
    case 2:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_GROUP, NDR_POINTER_UNIQUE,
                                     "DELTA_GROUP:", -1);
        break;
    case 4:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_RENAME, NDR_POINTER_UNIQUE,
                                     "DELTA_RENAME_GROUP:", hf_netlogon_group_name);
        break;
    case 5:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_USER, NDR_POINTER_UNIQUE,
                                     "DELTA_USER:", -1);
        break;
    case 7:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_RENAME, NDR_POINTER_UNIQUE,
                                     "DELTA_RENAME_USER:", hf_netlogon_acct_name);
        break;
    case 8:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_GROUP_MEMBER, NDR_POINTER_UNIQUE,
                                     "DELTA_GROUP_MEMBER:", -1);
        break;
    case 9:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_ALIAS, NDR_POINTER_UNIQUE,
                                     "DELTA_ALIAS:", -1);
        break;
    case 11:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_RENAME, NDR_POINTER_UNIQUE,
                                     "DELTA_RENAME_ALIAS:", hf_netlogon_alias_name);
        break;
    case 12:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_ALIAS_MEMBER, NDR_POINTER_UNIQUE,
                                     "DELTA_ALIAS_MEMBER:", -1);
        break;
    case 13:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_POLICY, NDR_POINTER_UNIQUE,
                                     "DELTA_POLICY:", -1);
        break;
    case 14:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_TRUSTED_DOMAINS, NDR_POINTER_UNIQUE,
                                     "DELTA_TRUSTED_DOMAINS:", -1);
        break;
    case 16:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_ACCOUNTS, NDR_POINTER_UNIQUE,
                                     "DELTA_ACCOUNTS:", -1);
        break;
    case 18:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_SECRET, NDR_POINTER_UNIQUE,
                                     "DELTA_SECRET:", -1);
        break;
    case 20:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_DELETE_USER, NDR_POINTER_UNIQUE,
                                     "DELTA_DELETE_GROUP:", -1);
        break;
    case 21:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DELTA_DELETE_USER, NDR_POINTER_UNIQUE,
                                     "DELTA_DELETE_USER:", -1);
        break;
    case 22:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_MODIFIED_COUNT, NDR_POINTER_UNIQUE,
                                     "MODIFIED_COUNT:", -1);
        break;
    }

    proto_item_set_len(item, offset-old_offset);
    return offset;
}



/* IDL XXX must verify this one, especially 13-19
 * IDL typedef [switch_type(short)] union {
 * IDL   [case(1)] long rid;
 * IDL   [case(2)] long rid;
 * IDL   [case(3)] long rid;
 * IDL   [case(4)] long rid;
 * IDL   [case(5)] long rid;
 * IDL   [case(6)] long rid;
 * IDL   [case(7)] long rid;
 * IDL   [case(8)] long rid;
 * IDL   [case(9)] long rid;
 * IDL   [case(10)] long rid;
 * IDL   [case(11)] long rid;
 * IDL   [case(12)] long rid;
 * IDL   [case(13)] [unique] SID *sid;
 * IDL   [case(14)] [unique] SID *sid;
 * IDL   [case(15)] [unique] SID *sid;
 * IDL   [case(16)] [unique] SID *sid;
 * IDL   [case(17)] [unique] SID *sid;
 * IDL   [case(18)] [unique][string] wchar_t *Name ;
 * IDL   [case(19)] [unique][string] wchar_t *Name ;
 * IDL   [case(20)] long rid;
 * IDL   [case(21)] long rid;
 * IDL } DELTA_ID_UNION;
 */
static int
netlogon_dissect_DELTA_ID_UNION(tvbuff_t *tvb, int offset,
                                packet_info *pinfo, proto_tree *parent_tree,
                                dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;
    uint16_t level = 0;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_DELTA_ID_UNION, &item, "DELTA_ID_UNION:");
    }

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_delta_type, &level);

    ALIGN_TO_4_BYTES;
    switch(level){
    case 1:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_group_rid, NULL);
        break;
    case 2:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 3:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 4:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 5:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 6:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 7:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 8:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 9:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 10:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 11:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 12:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 13:
        offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);
        break;
    case 14:
        offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);
        break;
    case 15:
        offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);
        break;
    case 16:
        offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);
        break;
    case 17:
        offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);
        break;
    case 18:
        offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo,
                                              tree, di, drep, NDR_POINTER_UNIQUE, "unknown",
                                              hf_netlogon_unknown_string, 0);
        break;
    case 19:
        offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo,
                                              tree, di, drep, NDR_POINTER_UNIQUE, "unknown",
                                              hf_netlogon_unknown_string, 0);
        break;
    case 20:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    case 21:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_user_rid, NULL);
        break;
    }

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

/*
 * IDL typedef struct {
 * IDL   short delta_type;
 * IDL   DELTA_ID_UNION delta_id_union;
 * IDL   DELTA_UNION delta_union;
 * IDL } DELTA_ENUM;
 */
static int
netlogon_dissect_DELTA_ENUM(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *parent_tree,
                            dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;
    uint16_t type;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_DELTA_ENUM, &item, "DELTA_ENUM:");
    }

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_delta_type, &type);

    proto_item_append_text(item, "%s", val_to_str(
                               type, delta_type_vals, "Unknown"));

    offset = netlogon_dissect_DELTA_ID_UNION(tvb, offset,
                                             pinfo, tree, di, drep);

    offset = netlogon_dissect_DELTA_UNION(tvb, offset,
                                          pinfo, tree, di, drep);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

static int
netlogon_dissect_DELTA_ENUM_array(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DELTA_ENUM);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   long num_deltas;
 * IDL   [unique][size_is(num_deltas)] DELTA_ENUM *delta_enum;
 * IDL } DELTA_ENUM_ARRAY;
 */
static int
netlogon_dissect_DELTA_ENUM_ARRAY(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_deltas, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DELTA_ENUM_array, NDR_POINTER_UNIQUE,
                                 "DELTA_ENUM: deltas", -1);

    return offset;
}


/*
 * IDL long NetrDatabaseDeltas(
 * IDL      [in][string][ref] wchar_t *logonserver, # REF!!!
 * IDL      [in][string][ref] wchar_t *computername,
 * IDL      [in][ref] AUTHENTICATOR credential,
 * IDL      [in][out][ref] AUTHENTICATOR return_authenticator,
 * IDL      [in] long database_id,
 * IDL      [in][out][ref] MODIFIED_COUNT domain_modify_count,
 * IDL      [in] long preferredmaximumlength,
 * IDL      [out][unique] DELTA_ENUM_ARRAY *delta_enum_array
 * IDL );
 */
static int
netlogon_dissect_netrdatabasedeltas_rqst(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle", hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_database_id, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_MODIFIED_COUNT, NDR_POINTER_REF,
                                 "MODIFIED_COUNT: domain modified count", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_size, NULL);

    return offset;
}
static int
netlogon_dissect_netrdatabasedeltas_reply(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_MODIFIED_COUNT, NDR_POINTER_REF,
                                 "MODIFIED_COUNT: domain modified count", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DELTA_ENUM_ARRAY, NDR_POINTER_UNIQUE,
                                 "DELTA_ENUM_ARRAY: deltas", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


/*
 * IDL long NetrDatabaseSync(
 * IDL      [in][string][ref] wchar_t *logonserver, # REF!!!
 * IDL      [in][string][ref] wchar_t *computername,
 * IDL      [in][ref] AUTHENTICATOR credential,
 * IDL      [in][out][ref] AUTHENTICATOR return_authenticator,
 * IDL      [in] long database_id,
 * IDL      [in][out][ref] long sync_context,
 * IDL      [in] long preferredmaximumlength,
 * IDL      [out][unique] DELTA_ENUM_ARRAY *delta_enum_array
 * IDL );
 */
static int
netlogon_dissect_netrdatabasesync_rqst(tvbuff_t *tvb, int offset,
                                       packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle", hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_database_id, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_sync_context, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_size, NULL);

    return offset;
}


static int
netlogon_dissect_netrdatabasesync_reply(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_sync_context, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DELTA_ENUM_ARRAY, NDR_POINTER_UNIQUE,
                                 "DELTA_ENUM_ARRAY: deltas", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

/*
 * IDL typedef struct {
 * IDL   char computer_name[16];
 * IDL   long timecreated;
 * IDL   long serial_number;
 * IDL } UAS_INFO_0;
 */
static int
netlogon_dissect_UAS_INFO_0(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    uint32_t time_created;
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    proto_tree_add_item(tree, hf_netlogon_computer_name, tvb, offset, 16, ENC_ASCII);
    offset += 16;

    time_created = tvb_get_uint32(tvb, offset, DREP_ENC_INTEGER(drep));
    proto_tree_add_uint_format_value(tree, hf_netlogon_time_created, tvb, offset, 4, time_created, "unknown time format");
    offset+= 4;

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_serial_number, NULL);

    return offset;
}


/*
 * IDL long NetrAccountDeltas(
 * IDL      [in][string][unique] wchar_t *logonserver,
 * IDL      [in][string][ref] wchar_t *computername,
 * IDL      [in][ref] AUTHENTICATOR credential,
 * IDL      [in][out][ref] AUTHENTICATOR return_authenticator,
 * IDL      [out][ref][size_is(count_returned)] char *Buffer,
 * IDL      [out][ref] long count_returned,
 * IDL      [out][ref] long total_entries,
 * IDL      [in][out][ref] UAS_INFO_0 recordid,
 * IDL      [in][long] count,
 * IDL      [in][long] level,
 * IDL      [in][long] buffersize,
 * IDL );
 */
static int
netlogon_dissect_netraccountdeltas_rqst(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_UAS_INFO_0, NDR_POINTER_REF,
                                 "UAS_INFO_0: RecordID", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_count, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_size, NULL);

    return offset;
}
static int
netlogon_dissect_netraccountdeltas_reply(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_REF,
                                 "BYTE_array: Buffer", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_count, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_entries, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_UAS_INFO_0, NDR_POINTER_REF,
                                 "UAS_INFO_0: RecordID", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


/*
 * IDL long NetrAccountSync(
 * IDL      [in][string][unique] wchar_t *logonserver,
 * IDL      [in][string][ref] wchar_t *computername,
 * IDL      [in][ref] AUTHENTICATOR credential,
 * IDL      [in][out][ref] AUTHENTICATOR return_authenticator,
 * IDL      [out][ref][size_is(count_returned)] char *Buffer,
 * IDL      [out][ref] long count_returned,
 * IDL      [out][ref] long total_entries,
 * IDL      [out][ref] long next_reference,
 * IDL      [in][long] reference,
 * IDL      [in][long] level,
 * IDL      [in][long] buffersize,
 * IDL      [in][out][ref] UAS_INFO_0 recordid,
 * IDL );
 */
static int
netlogon_dissect_netraccountsync_rqst(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reference, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_size, NULL);

    return offset;
}
static int
netlogon_dissect_netraccountsync_reply(tvbuff_t *tvb, int offset,
                                       packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_REF,
                                 "BYTE_array: Buffer", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_count, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_entries, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_next_reference, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_UAS_INFO_0, NDR_POINTER_REF,
                                 "UAS_INFO_0: RecordID", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


/*
 * IDL long NetrGetDcName(
 * IDL    [in][ref][string] wchar_t *logon_server,
 * IDL    [in][unique][string] wchar_t *domainname,
 * IDL    [out][unique][string] wchar_t *dcname,
 * IDL };
 */
static int
netlogon_dissect_netrgetdcname_rqst(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle", hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_domain_name, 0);

    return offset;
}
static int
netlogon_dissect_netrgetdcname_reply(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_dc_name, 0);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}



/*
 * IDL typedef struct {
 * IDL   long flags;
 * IDL   long pdc_connection_status;
 * IDL } NETLOGON_INFO_1;
 */
static int
netlogon_dissect_NETLOGON_INFO_1(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_flags, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_pdc_connection_status, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   long flags;
 * IDL   long pdc_connection_status;
 * IDL   [unique][string] wchar_t trusted_dc_name;
 * IDL   long tc_connection_status;
 * IDL } NETLOGON_INFO_2;
 */
static int
netlogon_dissect_NETLOGON_INFO_2(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_flags, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_pdc_connection_status, NULL);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Trusted DC Name",
                                          hf_netlogon_trusted_dc_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_tc_connection_status, NULL);

    return offset;
}


/*
 * IDL typedef struct {
 * IDL   long flags;
 * IDL   long logon_attempts;
 * IDL   long reserved;
 * IDL   long reserved;
 * IDL   long reserved;
 * IDL   long reserved;
 * IDL   long reserved;
 * IDL } NETLOGON_INFO_3;
 */
static int
netlogon_dissect_NETLOGON_INFO_3(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_flags, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_logon_attempts, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_reserved, NULL);

    return offset;
}


/*
 * IDL typedef [switch_type(long)] union {
 * IDL   [case(1)] [unique] NETLOGON_INFO_1 *i1;
 * IDL   [case(2)] [unique] NETLOGON_INFO_2 *i2;
 * IDL   [case(3)] [unique] NETLOGON_INFO_3 *i3;
 * IDL } CONTROL_QUERY_INFORMATION;
 */
static int
netlogon_dissect_CONTROL_QUERY_INFORMATION(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo, proto_tree *tree,
                                           dcerpc_info *di, uint8_t *drep)
{
    uint32_t level = 0;

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, &level);

    ALIGN_TO_4_BYTES;
    switch(level){
    case 1:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_NETLOGON_INFO_1, NDR_POINTER_UNIQUE,
                                     "NETLOGON_INFO_1:", -1);
        break;
    case 2:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_NETLOGON_INFO_2, NDR_POINTER_UNIQUE,
                                     "NETLOGON_INFO_2:", -1);
        break;
    case 3:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_NETLOGON_INFO_3, NDR_POINTER_UNIQUE,
                                     "NETLOGON_INFO_3:", -1);
        break;
    }

    return offset;
}


/*
 * IDL long NetrLogonControl(
 * IDL      [in][string][unique] wchar_t *logonserver,
 * IDL      [in] long function_code,
 * IDL      [in] long level,
 * IDL      [out][ref] CONTROL_QUERY_INFORMATION
 * IDL );
 */
static int
netlogon_dissect_netrlogoncontrol_rqst(tvbuff_t *tvb, int offset,
                                       packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_code, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL);

    return offset;
}
static int
netlogon_dissect_netrlogoncontrol_reply(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CONTROL_QUERY_INFORMATION, NDR_POINTER_REF,
                                 "CONTROL_QUERY_INFORMATION:", -1);

    offset = dissect_werror(tvb, offset, pinfo, tree, di, drep,
                            hf_netlogon_werr_rc, NULL);

    return offset;
}


/*
 * IDL long NetrGetAnyDCName(
 * IDL    [in][unique][string] wchar_t *logon_server,
 * IDL    [in][unique][string] wchar_t *domainname,
 * IDL    [out][unique][string] wchar_t *dcname,
 * IDL };
 */
static int
netlogon_dissect_netrgetanydcname_rqst(tvbuff_t *tvb, int offset,
                                       packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Server Handle",
                                          hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_domain_name, 0);

    return offset;
}
static int
netlogon_dissect_netrgetanydcname_reply(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_dc_name, 0);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}


/*
 * IDL typedef [switch_type(long)] union {
 * IDL   [case(5)] [unique][string] wchar_t *unknown;
 * IDL   [case(6)] [unique][string] wchar_t *unknown;
 * IDL   [case(0xfffe)] long unknown;
 * IDL   [case(7)] [unique][string] wchar_t *unknown;
 * IDL } CONTROL_DATA_INFORMATION;
 */
/* XXX
 * According to muddle this is what CONTROL_DATA_INFORMATION is supposed
 * to look like. However NetMon does not recognize any such informationlevels.
 *
 * I'll leave it as CONTROL_DATA_INFORMATION with no informationlevels
 * until someone has any source of better authority to call upon.
 */
static int
netlogon_dissect_CONTROL_DATA_INFORMATION(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *tree,
                                          dcerpc_info *di, uint8_t *drep)
{
    uint32_t level = 0;

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, &level);

    ALIGN_TO_4_BYTES;
    switch(level){
    case 5:
        offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo,
                                              tree, di, drep, NDR_POINTER_UNIQUE, "Trusted Domain Name",
                                              hf_netlogon_TrustedDomainName_string, 0);
        break;
    case 6:
        offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo,
                                              tree, di, drep, NDR_POINTER_UNIQUE, "Trusted Domain Name",
                                              hf_netlogon_TrustedDomainName_string, 0);
        break;
    case 0xfffe:
        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_unknown_long, NULL);
        break;
    case 8:
        offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo,
                                              tree, di, drep, NDR_POINTER_UNIQUE, "UserName",
                                              hf_netlogon_UserName_string, 0);
        break;
    }

    return offset;
}


/*
 * IDL long NetrLogonControl2(
 * IDL      [in][string][unique] wchar_t *logonserver,
 * IDL      [in] long function_code,
 * IDL      [in] long level,
 * IDL      [in][ref] CONTROL_DATA_INFORMATION *data,
 * IDL      [out][ref] CONTROL_QUERY_INFORMATION *query
 * IDL );
 */
static int
netlogon_dissect_netrlogoncontrol2_rqst(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_code, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CONTROL_DATA_INFORMATION, NDR_POINTER_REF,
                                 "CONTROL_DATA_INFORMATION: ", -1);

    return offset;
}

static int
netlogon_dissect_netrlogoncontrol2_reply(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    return netlogon_dissect_netrlogoncontrol_reply(tvb, offset, pinfo, tree, di, drep);
}




/*
 * IDL long NetrDatabaseSync2(
 * IDL      [in][string][ref] wchar_t *logonserver, # REF!!!
 * IDL      [in][string][ref] wchar_t *computername,
 * IDL      [in][ref] AUTHENTICATOR credential,
 * IDL      [in][out][ref] AUTHENTICATOR return_authenticator,
 * IDL      [in] long database_id,
 * IDL      [in] short restart_state,
 * IDL      [in][out][ref] long *sync_context,
 * IDL      [in] long preferredmaximumlength,
 * IDL      [out][unique] DELTA_ENUM_ARRAY *delta_enum_array
 * IDL );
 */
static int
netlogon_dissect_netrdatabasesync2_rqst(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle", hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_database_id, NULL);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_restart_state, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_sync_context, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_size, NULL);

    return offset;
}

static int
netlogon_dissect_netrdatabasesync2_reply(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_sync_context, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DELTA_ENUM_ARRAY, NDR_POINTER_UNIQUE,
                                 "DELTA_ENUM_ARRAY: deltas", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


/*
 * IDL long NetrDatabaseRedo(
 * IDL      [in][string][ref] wchar_t *logonserver, # REF!!!
 * IDL      [in][string][ref] wchar_t *computername,
 * IDL      [in][ref] AUTHENTICATOR credential,
 * IDL      [in][out][ref] AUTHENTICATOR return_authenticator,
 * IDL      [in][ref][size_is(change_log_entry_size)] char *change_log_entry,
 * IDL      [in] long change_log_entry_size,
 * IDL      [out][unique] DELTA_ENUM_ARRAY *delta_enum_array
 * IDL );
 */
static int
netlogon_dissect_netrdatabaseredo_rqst(tvbuff_t *tvb, int offset,
                                       packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle", hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_REF,
                                 "Change log entry: ", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_max_log_size, NULL);

    return offset;
}

static int
netlogon_dissect_netrdatabaseredo_reply(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DELTA_ENUM_ARRAY, NDR_POINTER_UNIQUE,
                                 "DELTA_ENUM_ARRAY: deltas", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


/*
 * IDL long NetrLogonControl2Ex(
 * IDL      [in][string][unique] wchar_t *logonserver,
 * IDL      [in] long function_code,
 * IDL      [in] long level,
 * IDL      [in][ref] CONTROL_DATA_INFORMATION *data,
 * IDL      [out][ref] CONTROL_QUERY_INFORMATION *query
 * IDL );
 */
static int
netlogon_dissect_netrlogoncontrol2ex_rqst(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_code, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CONTROL_DATA_INFORMATION, NDR_POINTER_REF,
                                 "CONTROL_DATA_INFORMATION: ", -1);

    return offset;
}
static int
netlogon_dissect_netrlogoncontrol2ex_reply(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    return netlogon_dissect_netrlogoncontrol_reply(tvb, offset, pinfo, tree, di, drep);
}




static const value_string trust_type_vals[] = {
    { 1, "NT4 Domain" },
    { 2, "AD Domain" },
    { 3, "MIT Kerberos realm" },
    { 4, "DCE realm" },
    { 0, NULL }
};

#define DS_INET_ADDRESS         1
#define DS_NETBIOS_ADDRESS      2

static const value_string dc_address_types[] = {
    { DS_INET_ADDRESS,    "IP/DNS name" },
    { DS_NETBIOS_ADDRESS, "NetBIOS name" },
    { 0, NULL}
};


#define RQ_ROOT_FOREST              0x00000001
#define RQ_DC_XFOREST               0x00000002
#define RQ_RODC_DIF_DOMAIN          0x00000004
#define RQ_NTLM_FROM_RODC           0x00000008

#define DS_DOMAIN_IN_FOREST         0x00000001
#define DS_DOMAIN_DIRECT_OUTBOUND   0x00000002
#define DS_DOMAIN_TREE_ROOT         0x00000004
#define DS_DOMAIN_PRIMARY           0x00000008
#define DS_DOMAIN_NATIVE_MODE       0x00000010
#define DS_DOMAIN_DIRECT_INBOUND    0x00000020

static const true_false_string trust_inbound = {
    "There is a DIRECT INBOUND trust for the servers domain",
    "There is NO direct inbound trust for the servers domain"
};
static const true_false_string trust_outbound = {
    "There is a DIRECT OUTBOUND trust for this domain",
    "There is NO direct outbound trust for this domain"
};
static const true_false_string trust_in_forest = {
    "The domain is a member IN the same FOREST as the queried server",
    "The domain is NOT a member of the queried servers domain"
};
static const true_false_string trust_native_mode = {
    "The primary domain is a NATIVE MODE w2k domain",
    "The primary is NOT a native mode w2k domain"
};
static const true_false_string trust_primary = {
    "The domain is the PRIMARY domain of the queried server",
    "The domain is NOT the primary domain of the queried server"
};
static const true_false_string trust_tree_root = {
    "The domain is the ROOT of a domain TREE",
    "The domain is NOT a root of a domain tree"
};


static int
netlogon_dissect_DOMAIN_TRUST_FLAGS(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t mask;
    static int * const flags[] = {
        &hf_netlogon_trust_flags_inbound,
        &hf_netlogon_trust_flags_native_mode,
        &hf_netlogon_trust_flags_primary,
        &hf_netlogon_trust_flags_tree_root,
        &hf_netlogon_trust_flags_outbound,
        &hf_netlogon_trust_flags_in_forest,
        NULL
    };

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset=dissect_ndr_uint32(tvb, offset, pinfo, NULL, di, drep,
                              -1, &mask);

    proto_tree_add_bitmask_value_with_flags(parent_tree, tvb, offset-4, hf_netlogon_trust_flags, ett_trust_flags, flags, mask, BMT_NO_APPEND);
    return offset;
}



static const true_false_string trust_attribs_non_transitive = {
    "This is a NON TRANSITIVE trust relation",
    "This is a normal trust"
};
static const true_false_string trust_attribs_uplevel_only = {
    "This is an UPLEVEL ONLY trust relation",
    "This is a normal trust"
};
static const true_false_string trust_attribs_quarantined_domain = {
    "This is a QUARANTINED DOMAIN (so don't expect lookupsids to work)",
    "This is a normal trust"
};
static const true_false_string trust_attribs_forest_transitive = {
    "This is a FOREST TRANSITIVE trust",
    "This is a normal trust"
};
static const true_false_string trust_attribs_cross_organization = {
    "This is a CROSS ORGANIZATION trust",
    "This is a normal trust"
};
static const true_false_string trust_attribs_within_forest = {
    "This is a WITHIN FOREST trust",
    "This is a normal trust"
};
static const true_false_string trust_attribs_treat_as_external = {
    "TREAT this trust AS an EXTERNAL trust",
    "This is a normal trust"
};

static int
netlogon_dissect_DOMAIN_TRUST_ATTRIBS(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t mask;
    static int * const attr[] = {
        &hf_netlogon_trust_attribs_treat_as_external,
        &hf_netlogon_trust_attribs_within_forest,
        &hf_netlogon_trust_attribs_cross_organization,
        &hf_netlogon_trust_attribs_forest_transitive,
        &hf_netlogon_trust_attribs_quarantined_domain,
        &hf_netlogon_trust_attribs_uplevel_only,
        &hf_netlogon_trust_attribs_non_transitive,
        NULL
    };

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, NULL, di, drep,
                                -1, &mask);

    proto_tree_add_bitmask_value_with_flags(parent_tree, tvb, offset-4, hf_netlogon_trust_attribs, ett_trust_attribs, attr, mask, BMT_NO_APPEND);
    return offset;
}


#define DS_FORCE_REDISCOVERY            0x00000001
#define DS_DIRECTORY_SERVICE_REQUIRED   0x00000010
#define DS_DIRECTORY_SERVICE_PREFERRED  0x00000020
#define DS_GC_SERVER_REQUIRED           0x00000040
#define DS_PDC_REQUIRED                 0x00000080
#define DS_BACKGROUND_ONLY              0x00000100
#define DS_IP_REQUIRED                  0x00000200
#define DS_KDC_REQUIRED                 0x00000400
#define DS_TIMESERV_REQUIRED            0x00000800
#define DS_WRITABLE_REQUIRED            0x00001000
#define DS_GOOD_TIMESERV_PREFERRED      0x00002000
#define DS_AVOID_SELF                   0x00004000
#define DS_ONLY_LDAP_NEEDED             0x00008000
#define DS_IS_FLAT_NAME                 0x00010000
#define DS_IS_DNS_NAME                  0x00020000
#define DS_RETURN_DNS_NAME              0x40000000
#define DS_RETURN_FLAT_NAME             0x80000000

static const true_false_string get_dcname_request_flags_force_rediscovery = {
    "FORCE REDISCOVERY of any cached data",
    "You may return cached data"
};
static const true_false_string get_dcname_request_flags_directory_service_required = {
    "DIRECTORY SERVICE is REQUIRED on the server",
    "We do NOT require directory service servers"
};
static const true_false_string get_dcname_request_flags_directory_service_preferred = {
    "DIRECTORY SERVICE servers are PREFERRED",
    "We do NOT have a preference for directory service servers"
};
static const true_false_string get_dcname_request_flags_gc_server_required = {
    "GC SERVER is REQUIRED",
    "gc server is NOT required"
};
static const true_false_string get_dcname_request_flags_pdc_required = {
    "PDC SERVER is REQUIRED",
    "pdc server is NOT required"
};
static const true_false_string get_dcname_request_flags_background_only = {
    "Only return cached data, even if it has expired",
    "Return cached data unless it has expired"
};
static const true_false_string get_dcname_request_flags_ip_required = {
    "IP address is REQUIRED",
    "ip address is NOT required"
};
static const true_false_string get_dcname_request_flags_kdc_required = {
    "KDC server is REQUIRED",
    "kdc server is NOT required"
};
static const true_false_string get_dcname_request_flags_timeserv_required = {
    "TIMESERV service is REQUIRED",
    "timeserv service is NOT required"
};
static const true_false_string get_dcname_request_flags_writable_required = {
    "the returned dc MUST be WRITEABLE",
    "a read-only dc may be returned"
};
static const true_false_string get_dcname_request_flags_good_timeserv_preferred = {
    "GOOD TIMESERV servers are PREFERRED",
    "we do NOT have a preference for good timeserv servers"
};
static const true_false_string get_dcname_request_flags_avoid_self = {
    "do NOT return self as dc; return someone else",
    "you may return yourSELF as the dc"
};
static const true_false_string get_dcname_request_flags_only_ldap_needed = {
    "we ONLY NEED LDAP; you don't have to return a dc",
    "we need a normal dc; an ldap only server will not do"
};
static const true_false_string get_dcname_request_flags_is_flat_name = {
    "the name we specify is a NetBIOS name",
    "the name we specify is NOT a NetBIOS name"
};
static const true_false_string get_dcname_request_flags_is_dns_name = {
    "the name we specify is a DNS name",
    "the name we specify is NOT a dns name"
};
static const true_false_string get_dcname_request_flags_return_dns_name = {
    "return a DNS name",
    "you may return a NON-dns name"
};
static const true_false_string get_dcname_request_flags_return_flat_name = {
    "return a NetBIOS name",
    "you may return a NON-NetBIOS name"
};
static int
netlogon_dissect_GET_DCNAME_REQUEST_FLAGS(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t mask;
    static int * const flags[] = {
        &hf_netlogon_get_dcname_request_flags_return_flat_name,
        &hf_netlogon_get_dcname_request_flags_return_dns_name,
        &hf_netlogon_get_dcname_request_flags_is_flat_name,
        &hf_netlogon_get_dcname_request_flags_is_dns_name,
        &hf_netlogon_get_dcname_request_flags_only_ldap_needed,
        &hf_netlogon_get_dcname_request_flags_avoid_self,
        &hf_netlogon_get_dcname_request_flags_good_timeserv_preferred,
        &hf_netlogon_get_dcname_request_flags_writable_required,
        &hf_netlogon_get_dcname_request_flags_timeserv_required,
        &hf_netlogon_get_dcname_request_flags_kdc_required,
        &hf_netlogon_get_dcname_request_flags_ip_required,
        &hf_netlogon_get_dcname_request_flags_background_only,
        &hf_netlogon_get_dcname_request_flags_pdc_required,
        &hf_netlogon_get_dcname_request_flags_gc_server_required,
        &hf_netlogon_get_dcname_request_flags_directory_service_preferred,
        &hf_netlogon_get_dcname_request_flags_directory_service_required,
        &hf_netlogon_get_dcname_request_flags_force_rediscovery,
        NULL
    };

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset=dissect_ndr_uint32(tvb, offset, pinfo, NULL, di, drep, -1, &mask);

    proto_tree_add_bitmask_value_with_flags(parent_tree, tvb, offset-4, hf_netlogon_get_dcname_request_flags, ett_get_dcname_request_flags, flags, mask, BMT_NO_APPEND);
    return offset;
}



#define DS_PDC_FLAG             0x00000001
#define DS_GC_FLAG              0x00000004
#define DS_LDAP_FLAG            0x00000008
#define DS_DS_FLAG              0x00000010
#define DS_KDC_FLAG             0x00000020
#define DS_TIMESERV_FLAG        0x00000040
#define DS_CLOSEST_FLAG         0x00000080
#define DS_WRITABLE_FLAG        0x00000100
#define DS_GOOD_TIMESERV_FLAG   0x00000200
#define DS_NDNC_FLAG            0x00000400
#define DS_DNS_CONTROLLER_FLAG  0x20000000
#define DS_DNS_DOMAIN_FLAG      0x40000000
#define DS_DNS_FOREST_FLAG      0x80000000

static const true_false_string dc_flags_pdc_flag = {
    "this is the PDC of the domain",
    "this is NOT the pdc of the domain"
};
static const true_false_string dc_flags_gc_flag = {
    "this is the GC of the forest",
    "this is NOT the gc of the forest"
};
static const true_false_string dc_flags_ldap_flag = {
    "this is an LDAP server",
    "this is NOT an ldap server"
};
static const true_false_string dc_flags_ds_flag = {
    "this is a DS server",
    "this is NOT a ds server"
};
static const true_false_string dc_flags_kdc_flag = {
    "this is a KDC server",
    "this is NOT a kdc server"
};
static const true_false_string dc_flags_timeserv_flag = {
    "this is a TIMESERV server",
    "this is NOT a timeserv server"
};
static const true_false_string dc_flags_closest_flag = {
    "this is the CLOSEST server",
    "this is NOT the closest server"
};
static const true_false_string dc_flags_writable_flag = {
    "this server has a WRITABLE ds database",
    "this server has a READ-ONLY ds database"
};
static const true_false_string dc_flags_good_timeserv_flag = {
    "this server is a GOOD TIMESERV server",
    "this is NOT a good timeserv server"
};
static const true_false_string dc_flags_ndnc_flag = {
    "NDNC is set",
    "ndnc is NOT set"
};
static const true_false_string dc_flags_dns_controller_flag = {
    "DomainControllerName is a DNS name",
    "DomainControllerName is NOT a dns name"
};
static const true_false_string dc_flags_dns_domain_flag = {
    "DomainName is a DNS name",
    "DomainName is NOT a dns name"
};
static const true_false_string dc_flags_dns_forest_flag = {
    "DnsForestName is a DNS name",
    "DnsForestName is NOT a dns name"
};
static int
netlogon_dissect_DC_FLAGS(tvbuff_t *tvb, int offset,
                          packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t mask;
    proto_item *item;
    static int * const flags[] = {
        &hf_netlogon_dc_flags_dns_forest_flag,
        &hf_netlogon_dc_flags_dns_domain_flag,
        &hf_netlogon_dc_flags_dns_controller_flag,
        &hf_netlogon_dc_flags_ndnc_flag,
        &hf_netlogon_dc_flags_good_timeserv_flag,
        &hf_netlogon_dc_flags_writable_flag,
        &hf_netlogon_dc_flags_closest_flag,
        &hf_netlogon_dc_flags_timeserv_flag,
        &hf_netlogon_dc_flags_kdc_flag,
        &hf_netlogon_dc_flags_ds_flag,
        &hf_netlogon_dc_flags_ldap_flag,
        &hf_netlogon_dc_flags_gc_flag,
        &hf_netlogon_dc_flags_pdc_flag,
        NULL
    };

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect */
        return offset;
    }

    offset=dissect_ndr_uint32(tvb, offset, pinfo, NULL, di, drep, -1, &mask);

    item = proto_tree_add_bitmask_value_with_flags(parent_tree, tvb, offset-4, hf_netlogon_dc_flags, ett_dc_flags, flags, mask, BMT_NO_APPEND);
    if (mask==0x0000ffff)
        proto_item_append_text(item, "  PING (mask==0x0000ffff)");

    return offset;
}



static int
netlogon_dissect_pointer_long(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32 (tvb, offset, pinfo, tree, di, drep,
                                 di->hf_index, NULL);
    return offset;
}

#if 0
static int
netlogon_dissect_pointer_char(tvbuff_t *tvb, int offset,
                              packet_info *pinfo, proto_tree *tree,
                              dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               di->hf_index, NULL);
    return offset;
}
#endif

static int
netlogon_dissect_UNICODE_MULTI_byte(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_unknown_char, NULL);

    return offset;
}

static int
netlogon_dissect_UNICODE_MULTI_array(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_UNICODE_MULTI_byte);

    return offset;
}

static int
netlogon_dissect_UNICODE_MULTI(tvbuff_t *tvb, int offset,
                               packet_info *pinfo, proto_tree *parent_tree,
                               dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_UNICODE_MULTI, &item, "UNICODE_MULTI:");
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_len, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_UNICODE_MULTI_array, NDR_POINTER_UNIQUE,
                                 "unknown", hf_netlogon_unknown_string);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

static int
netlogon_dissect_DOMAIN_CONTROLLER_INFO(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *parent_tree,
                                        dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_DOMAIN_CONTROLLER_INFO, &item, "DOMAIN_CONTROLLER_INFO:");
    }

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "DC Name", hf_netlogon_dc_name, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "DC Address", hf_netlogon_dc_address, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dc_address_type, NULL);

    offset = dissect_nt_GUID(tvb, offset,
                             pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Logon Domain", hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "DNS Forest", hf_netlogon_dns_forest_name, 0);

    offset = netlogon_dissect_DC_FLAGS(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "DC Site", hf_netlogon_dc_site_name, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Client Site",
                                          hf_netlogon_client_site_name, 0);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}



static int
dissect_ndr_trust_extension(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    uint64_t len,max;

    if(di->conformant_run){
        return offset;
    }
    offset = dissect_ndr_uint3264(tvb, offset, pinfo, tree, di, drep,
                                  hf_netlogon_trust_max, &max);

    offset = dissect_ndr_uint3264(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_trust_offset, NULL);

    offset = dissect_ndr_uint3264(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_trust_len, &len);

    if( max * 2 == 16 ) {
        offset = netlogon_dissect_DOMAIN_TRUST_FLAGS(tvb, offset, pinfo, tree, di, drep);

        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_trust_parent_index, NULL);

        offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                    hf_netlogon_trust_type, NULL);

        offset = netlogon_dissect_DOMAIN_TRUST_ATTRIBS(tvb, offset, pinfo, tree, di, drep);
    }
    /* else do something scream shout .... */

    return offset;
}

static int
netlogon_dissect_BLOB_array(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    uint32_t len;

    if(di->conformant_run){
        return offset;
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_blob_size, &len);

    proto_tree_add_item(tree, hf_netlogon_blob, tvb, offset, len,
                        ENC_NA);
    offset += len;

    return offset;
}

static int
dissect_ndr_ulongs_as_counted_string(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep, int hf_index)
{
    uint16_t len, size;
    bool add_subtree = true; /* Manage room for evolution*/
    proto_item *item;
    proto_tree *subtree = tree;

    if (add_subtree) {

        subtree = proto_tree_add_subtree(
            tree, tvb, offset, 0, ett_nt_counted_longs_as_string, &item,
            proto_registrar_get_name(hf_index));
    }
    /* Structure starts with short, but is aligned for longs */
    ALIGN_TO_4_BYTES;

    if (di->conformant_run)
        return offset;

    /*
      struct {
      short len;
      short size;
      [size_is(size/2), length_is(len/2), ptr] unsigned short *string;
      } UNICODE_STRING;

    */

    offset = dissect_ndr_uint16(tvb, offset, pinfo, subtree, di, drep,
                                hf_nt_cs_len, &len);
    offset = dissect_ndr_uint16(tvb, offset, pinfo, subtree, di, drep,
                                hf_nt_cs_size, &size);
    offset = dissect_ndr_pointer_cb(tvb, offset, pinfo, subtree, di, drep,
                                    dissect_ndr_trust_extension, NDR_POINTER_UNIQUE,
                                    "Buffer", hf_index,NULL,NULL);
    return offset;
}

static int
DomainInfo_sid_(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = lsarpc_dissect_struct_dom_sid2(tvb, offset, pinfo, tree, di, drep, hf_domain_info_sid, 0);

    return offset;
}
static int
dissect_element_lsa_DnsDomainInfo_sid(tvbuff_t *tvb , int offset , packet_info *pinfo , proto_tree *tree , dcerpc_info *di, uint8_t *drep )
{
    offset = dissect_ndr_embedded_pointer(tvb, offset, pinfo, tree, di, drep, DomainInfo_sid_, NDR_POINTER_UNIQUE, "Pointer to Sid (dom_sid2)", hf_dns_domain_info_sid);

    return offset;
}
static int
dissect_element_lsa_DnsDomainInfo_domain_guid(tvbuff_t *tvb, int offset, packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep )
{
    offset = dissect_ndr_uuid_t(tvb, offset, pinfo, tree, di, drep, hf_dns_domain_info_domain_guid, NULL);

    return offset;
}


static int dissect_part_DnsDomainInfo(tvbuff_t *tvb , int offset, packet_info *pinfo, proto_tree *tree , dcerpc_info *di, uint8_t *drep,  int hf_index _U_, uint32_t param _U_)
{

    offset = lsarpc_dissect_struct_lsa_StringLarge(tvb, offset, pinfo, tree, di, drep, hf_dns_domain_info_name, 0);

    offset = lsarpc_dissect_struct_lsa_StringLarge(tvb,offset, pinfo, tree, di, drep, hf_dns_domain_info_dns_domain, 0);

    offset = lsarpc_dissect_struct_lsa_StringLarge(tvb,offset, pinfo, tree, di, drep, hf_dns_domain_info_dns_forest, 0);

    offset = dissect_element_lsa_DnsDomainInfo_domain_guid(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_element_lsa_DnsDomainInfo_sid(tvb, offset, pinfo, tree, di, drep);


    return offset;
}


static int
netlogon_dissect_ONE_DOMAIN_INFO(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *parent_tree,
                                 dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_DOMAIN_TRUST_INFO, &item, "ONE_DOMAIN_INFO");
    }
/*hf_netlogon_dnsdomaininfo*/
    offset = dissect_part_DnsDomainInfo(tvb, offset, pinfo, tree, di, drep, 0, 0);


    /* It is structed as a string but it's not ... it's 4 ulong */
    offset = dissect_ndr_ulongs_as_counted_string(tvb, offset, pinfo, tree, di, drep,
                                                  hf_netlogon_trust_extension);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string2, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string3, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string4, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy1_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy2_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy3_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy4_long, NULL);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

static int
netlogon_dissect_DOMAIN_TRUST_INFO(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_ONE_DOMAIN_INFO);

    return offset;
}


static int
netlogon_dissect_LSA_POLICY_INFO(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree,
                                 dcerpc_info *di, uint8_t *drep )
{
    proto_item *item=NULL;
    proto_tree *subtree=NULL;
    uint32_t len;

    if(di->conformant_run){
        return offset;
    }

    if(tree){
        subtree = proto_tree_add_subtree(tree, tvb, offset, 0,
                                   ett_LSA_POLICY_INFO, &item, "LSA Policy");
    }
    offset = dissect_ndr_uint32(tvb, offset, pinfo, subtree, di, drep,
                                hf_netlogon_lsapolicy_len, &len);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, subtree, di, drep,
                                 netlogon_dissect_BLOB_array, NDR_POINTER_UNIQUE,
                                 "Pointer:", -1);

    return offset;
}




static int
netlogon_dissect_WORKSTATION_INFO(tvbuff_t *tvb , int offset ,
                                  packet_info *pinfo , proto_tree *tree ,
                                  dcerpc_info *di, uint8_t *drep )
{
    /* This is not the good way to do it ... it stinks ...
     * but after half of a day fighting against wireshark and ndr ...
     * I decided to keep this hack ...
     * At least data are correctly displayed without invented ints ...
     */
    offset = netlogon_dissect_LSA_POLICY_INFO(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Workstation FQDN",
                                          hf_netlogon_workstation_fqdn, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Workstation Site",
                                          hf_netlogon_workstation_site_name, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Dummy 1", hf_netlogon_dummy_string, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Dummy 2", hf_netlogon_dummy_string2, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Dummy 3", hf_netlogon_dummy_string3, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Dummy 4", hf_netlogon_dummy_string4, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_os_version, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_workstation_os, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string3, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string4, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_workstation_flags, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_supportedenctypes, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy3_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy4_long, NULL);
    return offset;
}

static int
netlogon_dissect_WORKSTATION_INFORMATION(tvbuff_t *tvb , int offset ,
                                         packet_info *pinfo , proto_tree *tree ,
                                         dcerpc_info *di, uint8_t *drep ) {

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_WORKSTATION_INFO, NDR_POINTER_UNIQUE,
                                 "WORKSTATION INFO", -1);
    return offset;
}

static int
netlogon_dissect_DOMAIN_INFO(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *tree,
                             dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_ONE_DOMAIN_INFO(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_num_trusts, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DOMAIN_TRUST_INFO, NDR_POINTER_UNIQUE,
                                 "DOMAIN_TRUST_ARRAY: Trusted domains", -1);

    offset = netlogon_dissect_LSA_POLICY_INFO(tvb,offset,pinfo, tree,di,drep);

/*      offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
        hf_netlogon_num_trusts, NULL);

        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
        netlogon_dissect_DOMAIN_TRUST_INFO, NDR_POINTER_UNIQUE,
        "LSA Policy", -1);
*/
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_ad_client_dns_name, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string2, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string3, 0);

    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_dummy_string4, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_workstation_flags, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_supportedenctypes, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy3_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_dummy4_long, NULL);

    return offset;
}


static int
netlogon_dissect_DOMAIN_INFORMATION(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    uint32_t level;

    UNION_ALIGN_TO_5_BYTES;
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep, hf_netlogon_level, &level);
    UNION_ALIGN_TO_5_BYTES;

    switch (level) {
    case 1:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_DOMAIN_INFO, NDR_POINTER_UNIQUE,
                                     "DOMAIN_INFO", -1);
        break;
    case 2:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_LSA_POLICY_INFO, NDR_POINTER_UNIQUE,
                                     "LSA_POLICY_INFO", -1);
        break;
    }

    return offset;
}

static int
netlogon_dissect_netr_CryptPassword(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *parent_tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    int ret_offset = offset + 516;
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    netlogon_auth_vars *vars = NULL;
    uint32_t pw_len;
    char *pw = NULL;
    uint32_t confounder_len;
    bool version_present = false;

    /*
     * We have
     * uint16 array[256];
     * uint32 length;
     *
     * All these 516 bytes are potentially encrypted.
     *
     * The unencrypted length is in bytes in
     * instead of uint16 units, so it's a multiple
     * of 2 and it should be smaller than 512 -
     * SIZEOF(NL_PASSWORD_VERSION), so it's 500
     * as SIZEOF(NL_PASSWORD_VERSION) is 12.
     * The confounder should also be there with
     * a few bytes.
     *
     * Real clients typically use 28 or 240,
     * which means 14 or 120 uint16 characters.
     *
     * So if the value is larger than 500 or
     * bit 1 is set it's very likely an
     * encrypted value.
     */
    tvb_ensure_bytes_exist(tvb, offset, 516);

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 516,
                                      ett_netr_CryptPassword, &item,
                                      "netr_CryptPassword:");
    }

    vars = find_global_netlogon_auth_vars(pinfo, 0);
    pw_len = tvb_get_uint32(tvb, offset+512, DREP_ENC_INTEGER(drep));
    if (pw_len > 500 || pw_len & 0x1) {
        gcry_error_t err;
        gcry_cipher_hd_t cipher_hd = NULL;
        uint8_t *buffer = NULL;
        tvbuff_t *dectvb = NULL;

        proto_tree_add_bytes_format(tree, hf_netlogon_blob,
                                    tvb, offset, 516, NULL,
                                    "Encrypted netr_CryptPassword");

        if (vars == NULL) {
                expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                                       &ei_netlogon_session_key,
                                       "No session key found");
                return ret_offset;
        }

        err = prepare_session_key_cipher(vars, &cipher_hd);
        if (err != 0) {
            expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                                   &ei_netlogon_session_key,
                                   "Decryption not possible (%s/%s) with "
                                   "session key learned in frame %d ("
                                   "%02x%02x%02x%02x"
                                   ") from %s",
                                   gcry_strsource(err),
                                   gcry_strerror(err),
                                   vars->auth_fd_num,
                                   vars->session_key[0] & 0xFF,
                                   vars->session_key[1] & 0xFF,
                                   vars->session_key[2] & 0xFF,
                                   vars->session_key[3] & 0xFF,
                                   vars->nthash.key_origin);
            ws_warning("GCRY: prepare_session_key_cipher %s/%s\n",
                       gcry_strsource(err), gcry_strerror(err));
            return ret_offset;
        }

        buffer = (uint8_t*)tvb_memdup(pinfo->pool, tvb, offset, 516);
        if (buffer == NULL) {
            gcry_cipher_close(cipher_hd);
            return ret_offset;
        }

        err = gcry_cipher_decrypt(cipher_hd, buffer, 516, NULL, 0);
        gcry_cipher_close(cipher_hd);
        if (err != 0) {
            ws_warning("GCRY: gcry_cipher_decrypt %s/%s\n",
                       gcry_strsource(err), gcry_strerror(err));
            return ret_offset;
        }

        dectvb = tvb_new_child_real_data(tvb, buffer, 516, 516);
        if (dectvb == NULL) {
            return ret_offset;
        }

        pw_len = tvb_get_uint32(dectvb, 512, DREP_ENC_INTEGER(drep));
        if ((pw_len > 500) || (pw_len & 0x1)) {
            expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                                   &ei_netlogon_session_key,
                                   "Unusable session key learned in frame %d ("
                                   "%02x%02x%02x%02x"
                                   ") from %s",
                                   vars->auth_fd_num,
                                   vars->session_key[0] & 0xFF,
                                   vars->session_key[1] & 0xFF,
                                   vars->session_key[2] & 0xFF,
                                   vars->session_key[3] & 0xFF,
                                   vars->nthash.key_origin);
            return ret_offset;
        }

        expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                               &ei_netlogon_session_key,
                               "Used session key learned in frame %d ("
                               "%02x%02x%02x%02x"
                               ") from %s",
                               vars->auth_fd_num,
                               vars->session_key[0] & 0xFF,
                               vars->session_key[1] & 0xFF,
                               vars->session_key[2] & 0xFF,
                               vars->session_key[3] & 0xFF,
                               vars->nthash.key_origin);
        add_new_data_source(pinfo, dectvb, "netr_CryptPassword (Decrypted)");
        tvb = dectvb;
        offset = 0;
        proto_tree_add_bytes_format(tree, hf_netlogon_blob,
                                    tvb, offset, 516, NULL,
                                    "Decrypted netr_CryptPassword");
    } else {
        proto_tree_add_bytes_format(tree, hf_netlogon_blob,
                                    tvb, offset, 516, NULL,
                                    "Unencryption netr_CryptPassword");
        if (vars != NULL) {
            expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                                   &ei_netlogon_session_key,
                                   "Not encrypted with session key learned in frame %d ("
                                   "%02x%02x%02x%02x"
                                   ") from %s",
                                   vars->auth_fd_num,
                                   vars->session_key[0] & 0xFF,
                                   vars->session_key[1] & 0xFF,
                                   vars->session_key[2] & 0xFF,
                                   vars->session_key[3] & 0xFF,
                                   vars->nthash.key_origin);
        } else {
            expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                                   &ei_netlogon_session_key,
                                   "Not encrypted and no session key found nor needed");
        }
    }

    confounder_len = 512 - pw_len;
    if (confounder_len >= 12) {
        uint32_t voffset = confounder_len - 12;
        uint32_t rf;
        uint32_t vp;

        rf = tvb_get_uint32(tvb, voffset+0, DREP_ENC_INTEGER(drep));
        vp = tvb_get_uint32(tvb, voffset+8, DREP_ENC_INTEGER(drep));
        if (rf == 0 && vp == 0x02231968) {
            confounder_len -= 12;
            version_present = true;
        }
    }

    if (confounder_len > 0) {
        proto_tree_add_bytes_format(tree, hf_netlogon_blob,
                                    tvb, offset, confounder_len,
                                    NULL, "Confounder: %"PRIu32" byte%s",
                                    confounder_len,
                                    plurality(confounder_len, "", "s"));
        offset += confounder_len;
    }

    if (version_present) {
        proto_item *vitem=NULL;
        proto_tree *vtree=NULL;

        if (tree) {
            vtree = proto_tree_add_subtree(tree, tvb, offset, 12,
                                           ett_NL_PASSWORD_VERSION, &vitem,
                                           "NL_PASSWORD_VERSION:");
        }

        offset = dissect_ndr_uint32(tvb, offset, pinfo, vtree, di, drep,
                                    hf_netlogon_password_version_reserved, NULL);
        offset = dissect_ndr_uint32(tvb, offset, pinfo, vtree, di, drep,
                                    hf_netlogon_password_version_number, NULL);
        offset = dissect_ndr_uint32(tvb, offset, pinfo, vtree, di, drep,
                                    hf_netlogon_password_version_present, NULL);
    }

    proto_tree_add_bytes_format(tree, hf_netlogon_blob,
                                tvb, offset, pw_len, NULL,
                                "Raw Password Bytes: %"PRIu32" byte%s",
                                pw_len,
                                plurality(pw_len, "", "s"));
    pw = (char *)tvb_get_string_enc(pinfo->pool, tvb, offset, pw_len,
                                    ENC_UTF_16|DREP_ENC_INTEGER(drep));
    proto_tree_add_string(tree, hf_netlogon_new_password, tvb, offset,
                          pw_len, pw);
    offset += pw_len;

    /*offset = */dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_len, NULL);

    return ret_offset;
}

static int
netlogon_dissect_element_844_byte(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_unknown_char, NULL);

    return offset;
}

static int
netlogon_dissect_element_844_array(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_element_844_byte);

    return offset;
}

static int
netlogon_dissect_TYPE_50(tvbuff_t *tvb, int offset,
                         packet_info *pinfo, proto_tree *parent_tree,
                         dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_TYPE_50, &item, "TYPE_50:");
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_element_844_array, NDR_POINTER_UNIQUE,
                                 "unknown", hf_netlogon_unknown_string);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

static int
netlogon_dissect_TYPE_50_ptr(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *tree,
                             dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_TYPE_50, NDR_POINTER_UNIQUE,
                                 "TYPE_50 pointer: unknown_TYPE_50", -1);

    return offset;
}

static int
netlogon_dissect_DS_DOMAIN_TRUSTS(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *parent_tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t tmp;
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_DS_DOMAIN_TRUSTS, NULL, "DS_DOMAIN_TRUSTS");
    }

    /* name */
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "NetBIOS Name",
                                          hf_netlogon_downlevel_domain_name, 0);

    /* domain */
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "DNS Domain Name",
                                          hf_netlogon_dns_domain_name, 0);

    offset = netlogon_dissect_DOMAIN_TRUST_FLAGS(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_trust_parent_index, &tmp);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_trust_type, &tmp);

    offset = netlogon_dissect_DOMAIN_TRUST_ATTRIBS(tvb, offset, pinfo, tree, di, drep);

    /* SID pointer */
    offset = dissect_ndr_nt_PSID(tvb, offset, pinfo, tree, di, drep);

    /* GUID */
    offset = dissect_nt_GUID(tvb, offset, pinfo, tree, di, drep);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

static int
netlogon_dissect_DS_DOMAIN_TRUSTS_ARRAY(tvbuff_t *tvb, int offset,
                                        packet_info *pinfo, proto_tree *tree,
                                        dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DS_DOMAIN_TRUSTS);

    return offset;
}

static int
netlogon_dissect_element_865_byte(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_unknown_char, NULL);

    return offset;
}

static int
netlogon_dissect_element_865_array(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_element_865_byte);

    return offset;
}

static int
netlogon_dissect_element_866_byte(tvbuff_t *tvb, int offset,
                                  packet_info *pinfo, proto_tree *tree,
                                  dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_unknown_char, NULL);

    return offset;
}

static int
netlogon_dissect_element_866_array(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree,
                                   dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_element_866_byte);

    return offset;
}

static int
netlogon_dissect_TYPE_52(tvbuff_t *tvb, int offset,
                         packet_info *pinfo, proto_tree *parent_tree,
                         dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    int old_offset=offset;

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                   ett_TYPE_52, &item, "TYPE_52:");
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_element_865_array, NDR_POINTER_UNIQUE,
                                 "unknown", hf_netlogon_unknown_string);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_element_866_array, NDR_POINTER_UNIQUE,
                                 "unknown", hf_netlogon_unknown_string);

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

static int
netlogon_dissect_TYPE_52_ptr(tvbuff_t *tvb, int offset,
                             packet_info *pinfo, proto_tree *tree,
                             dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_TYPE_52, NDR_POINTER_UNIQUE,
                                 "TYPE_52 pointer: unknown_TYPE_52", -1);
    return offset;
}


static int
netlogon_dissect_Capabilities(tvbuff_t *tvb, int offset,
                         packet_info *pinfo, proto_tree *parent_tree,
                         dcerpc_info *di, uint8_t *drep)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;
    proto_item *pitem=NULL;
    proto_item *nitem=NULL;
    int old_offset=offset;
    uint32_t level = 0;

    if(parent_tree){
        pitem = proto_tree_get_parent(parent_tree);
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, 0,
                                      ett_CAPABILITIES, &item,
                                      "Capabilities");
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, &level);

    ALIGN_TO_4_BYTES;
    switch(level){
    case 1: {
        uint32_t flags;
        dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep, -1, &flags);
        nitem = netlogon_dissect_neg_options(tvb,tree,flags,offset);
        proto_item_set_text(nitem, "NegotiatedFlags: 0x%08x", flags);
        proto_item_set_text(item, "ServerCapabilities");
        proto_item_append_text(pitem, ": ServerCapabilities");
        offset +=4;
        }
        break;
    case 2: {
        uint32_t flags;
        dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep, -1, &flags);
        nitem = netlogon_dissect_neg_options(tvb,tree,flags,offset);
        proto_item_set_text(nitem, "RequestedFlags: 0x%08x", flags);
        proto_item_set_text(item, "RequestedFlags");
        proto_item_append_text(pitem, ": RequestedFlags");
        offset +=4;
        }
        break;
    }

    proto_item_set_len(item, offset-old_offset);
    return offset;
}

static int
netlogon_dissect_WORKSTATION_BUFFER(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    uint32_t level;

    UNION_ALIGN_TO_5_BYTES;
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep, hf_netlogon_level, &level);
    UNION_ALIGN_TO_5_BYTES;

    switch (level) {
    case 1:
    case 2:
        offset = netlogon_dissect_WORKSTATION_INFORMATION(tvb, offset, pinfo, tree, di, drep);
        break;
    }

    return offset;
}

static int
netlogon_dissect_netrenumeratetrusteddomains_rqst(tvbuff_t *tvb, int offset,
                                                  packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    return offset;
}


static int
netlogon_dissect_netrenumeratetrusteddomains_reply(tvbuff_t *tvb, int offset,
                                                   packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_UNICODE_MULTI, NDR_POINTER_REF,
                                 "UNICODE_MULTI pointer: trust_dom_name_list", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}

static int
netlogon_dissect_dsrgetdcname_rqst(tvbuff_t *tvb, int offset,
                                   packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_nt_GUID, NDR_POINTER_UNIQUE,
                                 "GUID pointer: domain_guid", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_nt_GUID, NDR_POINTER_UNIQUE,
                                 "GUID pointer: site_guid", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_flags, NULL);

    return offset;
}


static int
netlogon_dissect_dsrgetdcname_reply(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DOMAIN_CONTROLLER_INFO, NDR_POINTER_UNIQUE,
                                 "DOMAIN_CONTROLLER_INFO:", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrlogondummyroutine1_rqst(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t level = 0;
    proto_item *litem = NULL;

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle",
                                          hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                -1, &level);
    litem = proto_tree_add_item(tree, hf_netlogon_level, tvb, offset-4, 4,
                                DREP_ENC_INTEGER(drep));
    switch(level){
    case 1:
        proto_item_append_text(litem, " (ServerCapabilities)");
        break;
    case 2:
        proto_item_append_text(litem, " (RequestedFlags)");
        break;
    }

    return offset;
}


static int
netlogon_dissect_netrlogondummyroutine1_reply(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_Capabilities, NDR_POINTER_REF,
                                 "Capabilities", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrlogonsetservicebits_rqst(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    return offset;
}


static int
netlogon_dissect_netrlogonsetservicebits_reply(tvbuff_t *tvb, int offset,
                                               packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


static int
netlogon_dissect_netrlogongettrustrid_rqst(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "unknown string",
                                          hf_netlogon_unknown_string, 0);

    return offset;
}


static int
netlogon_dissect_netrlogongettrustrid_reply(tvbuff_t *tvb, int offset,
                                            packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_pointer_long, NDR_POINTER_UNIQUE,
                                 "ULONG pointer: unknown_ULONG", hf_netlogon_unknown_long);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


static int
netlogon_dissect_netrlogoncomputeserverdigest_rqst(tvbuff_t *tvb, int offset,
                                                   packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_UNIQUE,
                                 "BYTE pointer: unknown_BYTE", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    return offset;
}

static int
netlogon_dissect_BYTE_16_array(tvbuff_t *tvb, int offset,
                               packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    int i;

    for(i=0;i<16;i++){
        offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                                   hf_netlogon_unknown_char, NULL);
    }

    return offset;
}

static int
netlogon_dissect_netrlogoncomputeserverdigest_reply(tvbuff_t *tvb, int offset,
                                                    packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_16_array, NDR_POINTER_UNIQUE,
                                 "BYTE pointer: unknown_BYTE", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrlogoncomputeclientdigest_rqst(tvbuff_t *tvb, int offset,
                                                   packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "unknown string",
                                          hf_netlogon_unknown_string, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_UNIQUE,
                                 "BYTE pointer: unknown_BYTE", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    return offset;
}


static int
netlogon_dissect_netrlogoncomputeclientdigest_reply(tvbuff_t *tvb, int offset,
                                                    packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_16_array, NDR_POINTER_UNIQUE,
                                 "BYTE pointer: unknown_BYTE", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static proto_item *
netlogon_dissect_neg_options(tvbuff_t *tvb,proto_tree *tree,uint32_t flags,int offset)
{
    static int * const hf_flags[] = {
        &hf_netlogon_neg_flags_80000000,
        &hf_netlogon_neg_flags_40000000,
        &hf_netlogon_neg_flags_20000000,
#if 0
        &hf_netlogon_neg_flags_10000000,
        &hf_netlogon_neg_flags_8000000,
        &hf_netlogon_neg_flags_4000000,
        &hf_netlogon_neg_flags_2000000,
        &hf_netlogon_neg_flags_800000,
        &hf_netlogon_neg_flags_400000,
#endif
        &hf_netlogon_neg_flags_1000000,
        &hf_netlogon_neg_flags_200000,
        &hf_netlogon_neg_flags_100000,
        &hf_netlogon_neg_flags_80000,
        &hf_netlogon_neg_flags_40000,
        &hf_netlogon_neg_flags_20000,
        &hf_netlogon_neg_flags_10000,
        &hf_netlogon_neg_flags_8000,
        &hf_netlogon_neg_flags_4000,
        &hf_netlogon_neg_flags_2000,
        &hf_netlogon_neg_flags_1000,
        &hf_netlogon_neg_flags_800,
        &hf_netlogon_neg_flags_400,
        &hf_netlogon_neg_flags_200,
        &hf_netlogon_neg_flags_100,
        &hf_netlogon_neg_flags_80,
        &hf_netlogon_neg_flags_40,
        &hf_netlogon_neg_flags_20,
        &hf_netlogon_neg_flags_10,
        &hf_netlogon_neg_flags_8,
        &hf_netlogon_neg_flags_4,
        &hf_netlogon_neg_flags_2,
        &hf_netlogon_neg_flags_1,
        NULL
    };

    return proto_tree_add_bitmask_value_with_flags(tree, tvb, offset, hf_netlogon_neg_flags, ett_authenticate_flags, hf_flags, flags, BMT_NO_APPEND);
}

static int
netlogon_dissect_netrserverauthenticate3_rqst(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    uint32_t flags;
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);
    ALIGN_TO_5_BYTES

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Acct Name", hf_netlogon_acct_name, 0);

    if (di->call_data->flags & DCERPC_IS_NDR64) {
        ALIGN_TO_4_BYTES
    } else {
        ALIGN_TO_2_BYTES
    }

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    ALIGN_TO_5_BYTES

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name", hf_netlogon_computer_name, 0);

    offset = dissect_dcerpc_8bytes(tvb, offset, pinfo, tree, drep,
                                   hf_client_credential, NULL);
#if 0
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_CREDENTIAL, NDR_POINTER_REF,
                                 "Client Challenge", -1);
#endif

#if 0
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_neg_flags, NULL);
#endif
    ALIGN_TO_4_BYTES;

    flags = tvb_get_letohl (tvb, offset);
    netlogon_dissect_neg_options(tvb,tree,flags,offset);
    seen.isseen = false;
    seen.num = 0;
    offset +=4;
    return offset;
}

static int
netlogon_dissect_netrserverauthenticatekerberos_rqst(tvbuff_t *tvb, int offset,
                                                     packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    netlogon_auth_vars *vars = NULL;
    dcerpc_call_value *dcv = (dcerpc_call_value *)di->call_data;
    uint32_t flags;
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);
    ALIGN_TO_5_BYTES

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Acct Name", hf_netlogon_acct_name, 0);

    if (di->call_data->flags & DCERPC_IS_NDR64) {
        ALIGN_TO_4_BYTES
    } else {
        ALIGN_TO_2_BYTES
    }

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    ALIGN_TO_5_BYTES

    offset = dissect_ndr_pointer_cb(
        tvb, offset, pinfo, tree, di, drep,
        dissect_ndr_wchar_cvstring, NDR_POINTER_REF,
        "Computer Name", hf_netlogon_computer_name,
        cb_wstr_postprocess,
        GINT_TO_POINTER(CB_STR_COL_INFO |CB_STR_SAVE | 1));

    ws_debug("1)Len %zu offset %d txt %s",
        dcv->private_data ? strlen((char *)dcv->private_data) : 0,
        offset,
        dcv->private_data ? (char*)dcv->private_data : "(null)");
    vars = create_global_netlogon_auth_vars(pinfo, (char*)dcv->private_data, 0);
    ws_debug("2)Txt %s", vars->client_name);

    ALIGN_TO_4_BYTES;

    flags = tvb_get_letohl (tvb, offset);
    netlogon_dissect_neg_options(tvb,tree,flags,offset);
    seen.isseen = false;
    seen.num = 0;
    offset +=4;

    vars->flags = flags;

    return offset;
}

/*
 * IDL long NetrServerAuthenticate2(
 * IDL      [in][string][unique] wchar_t *logonserver,
 * IDL      [in][ref][string] wchar_t *username,
 * IDL      [in] short secure_channel_type,
 * IDL      [in][ref][string] wchar_t *computername,
 * IDL      [in][ref] CREDENTIAL *client_chal,
 * IDL      [out][ref] CREDENTIAL *server_chal,
 * IDL      [in][out][ref] long *negotiate_flags,
 * IDL );
 */
static int
netlogon_dissect_netrserverauthenticate2_rqst(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    return netlogon_dissect_netrserverauthenticate3_rqst(tvb,offset,pinfo,tree,di,drep);
}

static int
netlogon_dissect_netrserverauthenticate023_reply(tvbuff_t *tvb, int offset,
                                                 packet_info *pinfo,
                                                 proto_tree *tree,
                                                 dcerpc_info *di,
                                                 uint8_t *drep,
                                                 int version)
{
    uint32_t flags = 0;
    netlogon_auth_vars *vars;
    uint64_t server_cred;

    offset = dissect_dcerpc_8bytes(tvb, offset, pinfo, tree, drep,
                                   hf_server_credential, &server_cred);

    if (version >= 2) {
        flags = tvb_get_letohl (tvb, offset);
        netlogon_dissect_neg_options(tvb,tree,flags,offset);
        offset +=4;
    }
    ALIGN_TO_4_BYTES;
    if (version >= 3) {
        offset = dissect_dcerpc_uint32(tvb, offset, pinfo, tree, drep,
                                       hf_server_rid, NULL);
    }
    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    vars = find_tmp_netlogon_auth_vars(pinfo, 1);
    if(vars != NULL) {
        ws_debug("Found some vars (ie. server/client challenges), let's see if I can get a session key");
        {
            md4_pass *pass_list=NULL;
            const md4_pass *used_md4 = NULL;
            const char *used_method = NULL;
            uint32_t list_size = 0;
            unsigned int i = 0;
            md4_pass password;
            uint8_t session_key[16];
            int found = 0;

            vars->flags = flags;
            vars->can_decrypt = false;
            list_size = get_md4pass_list(pinfo->pool, &pass_list);
            ws_debug("Found %d passwords ",list_size);
            if( flags & NETLOGON_FLAG_AES )
            {
                uint8_t salt_buf[16] = { 0 };
                uint8_t sha256[HASH_SHA2_256_LENGTH];
                uint64_t calculated_cred;

                memcpy(&salt_buf[0], (uint8_t*)&vars->client_challenge, 8);
                memcpy(&salt_buf[8], (uint8_t*)&vars->server_challenge, 8);

                used_method = "AES";
                ws_log_buffer((uint8_t*)&vars->client_challenge, 8, "Client challenge");
                ws_log_buffer((uint8_t*)&vars->server_challenge, 8, "Server challenge");
                ws_log_buffer((uint8_t*)&server_cred, 8, "Server creds");
                for(i=0;i<list_size;i++)
                {
                    used_md4 = &pass_list[i];
                    password = pass_list[i];
                    ws_log_buffer((uint8_t*)&password, 16, "NTHASH");
                    if (!ws_hmac_buffer(GCRY_MD_SHA256, sha256, salt_buf, sizeof(salt_buf), (uint8_t*) &password, 16)) {
                        gcry_error_t err;
                        gcry_cipher_hd_t cipher_hd = NULL;
                        uint8_t iv[16] = { 0 };

                        /* truncate the session key to 16 bytes */
                        memcpy(session_key, sha256, 16);
                        ws_log_buffer((uint8_t*)session_key, 16, "Session Key");

                        /* Open the cipher */
                        err = gcry_cipher_open(&cipher_hd, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CFB8, 0);
                        if (err != 0) {
                            ws_warning("GCRY: cipher open %s/%s\n", gcry_strsource(err), gcry_strerror(err));
                            break;
                        }

                        /* Set the initial value */
                        err = gcry_cipher_setiv(cipher_hd, iv, sizeof(iv));
                        if (err != 0) {
                            ws_warning("GCRY: setiv %s/%s\n", gcry_strsource(err), gcry_strerror(err));
                            gcry_cipher_close(cipher_hd);
                            break;
                        }

                        /* Set the key */
                        err = gcry_cipher_setkey(cipher_hd, session_key, 16);
                        if (err != 0) {
                            ws_warning("GCRY: setkey %s/%s\n", gcry_strsource(err), gcry_strerror(err));
                            gcry_cipher_close(cipher_hd);
                            break;
                        }

                        calculated_cred = 0x1234567812345678;
                        err = gcry_cipher_encrypt(cipher_hd,
                                                  (uint8_t *)&calculated_cred, 8,
                                                  (const uint8_t *)&vars->server_challenge, 8);
                        if (err != 0) {
                            ws_warning("GCRY: encrypt %s/%s\n", gcry_strsource(err), gcry_strerror(err));
                            gcry_cipher_close(cipher_hd);
                            break;
                        }

                        /* Done with the cipher */
                        gcry_cipher_close(cipher_hd);

                        ws_log_buffer((uint8_t*)&calculated_cred, 8, "Calculated creds");

                        if(calculated_cred==server_cred) {
                            found = 1;
                            break;
                        }
                    }
                }
            } else if ( flags & NETLOGON_FLAG_STRONGKEY ) {
                uint8_t zeros[4] = { 0 };
                uint8_t md5[HASH_MD5_LENGTH] = { 0 };
                gcry_md_hd_t md5_handle;
                uint8_t buf[8] = { 0 };
                uint64_t calculated_cred;

                used_method = "MD5";
                if (!gcry_md_open(&md5_handle, GCRY_MD_MD5, 0)) {
                    gcry_md_write(md5_handle, zeros, 4);
                    gcry_md_write(md5_handle, (uint8_t*)&vars->client_challenge, 8);
                    gcry_md_write(md5_handle, (uint8_t*)&vars->server_challenge, 8);
                    memcpy(md5, gcry_md_read(md5_handle, 0), 16);
                    gcry_md_close(md5_handle);
                }
                ws_log_buffer(md5, 8, "MD5");
                ws_log_buffer((uint8_t*)&vars->client_challenge, 8, "Client challenge");
                ws_log_buffer((uint8_t*)&vars->server_challenge, 8, "Server challenge");
                ws_log_buffer((uint8_t*)&server_cred, 8, "Server creds");
                for(i=0;i<list_size;i++)
                {
                    used_md4 = &pass_list[i];
                    password = pass_list[i];
                    if (!ws_hmac_buffer(GCRY_MD_MD5, session_key, md5, HASH_MD5_LENGTH, (uint8_t*) &password, 16)) {
                        crypt_des_ecb(buf,(unsigned char*)&vars->server_challenge,session_key);
                        crypt_des_ecb((unsigned char*)&calculated_cred,buf,session_key+7);
                        ws_log_buffer((uint8_t*)&calculated_cred, 8, "Calculated creds");
                        if(calculated_cred==server_cred) {
                            found = 1;
                            break;
                        }
                    }
                }
            }
            else
            {
                uint32_t c1 = (uint32_t)(vars->client_challenge & UINT32_MAX);
                uint32_t c2 = (uint32_t)((vars->client_challenge >> 32) & UINT32_MAX);
                uint32_t s1 = (uint32_t)(vars->server_challenge & UINT32_MAX);
                uint32_t s2 = (uint32_t)((vars->server_challenge >> 32) & UINT32_MAX);
                uint32_t sum1 = c1 + s1;
                uint32_t sum2 = c2 + s2;
                uint64_t sum = (uint64_t)sum1 | ((uint64_t)sum2 << 32);

                used_method = "DES";
                ws_log_buffer((uint8_t*)&sum, 8,"SUM for DES");
                ws_log_buffer((uint8_t*)&vars->client_challenge,8,"Client challenge");
                ws_log_buffer((uint8_t*)&vars->server_challenge,8,"Server challenge");
                ws_log_buffer((uint8_t*)&server_cred,8,"Server creds");
                for(i=0;i<list_size;i++)
                {
                    uint8_t buf[8] = { 0 };
                    uint64_t calculated_cred;

                    memset(session_key, 0, 16);

                    used_md4 = &pass_list[i];
                    crypt_des_ecb(buf, (unsigned char*)&sum, used_md4->md4);
                    crypt_des_ecb((unsigned char*)session_key, buf, used_md4->md4+9);

                    crypt_des_ecb(buf,(unsigned char*)&vars->server_challenge,session_key);
                    crypt_des_ecb((unsigned char*)&calculated_cred,buf,session_key+7);
                    ws_log_buffer((uint8_t*)&calculated_cred,8,"Calculated creds");
                    if(calculated_cred==server_cred) {
                        found = 1;
                        break;
                    }
                }
            }
            if(found) {
                vars->nthash = *used_md4;
                vars->auth_fd_num = pinfo->num;
                memcpy(&vars->session_key,session_key,16);
                ws_debug("Found the good session key !");
                expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                         &ei_netlogon_auth_nthash,
                         "%s authenticated using %s (%02x%02x%02x%02x...)",
                         used_method, used_md4->key_origin,
                         used_md4->md4[0] & 0xFF, used_md4->md4[1] & 0xFF,
                         used_md4->md4[2] & 0xFF, used_md4->md4[3] & 0xFF);
                expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                         &ei_netlogon_session_key,
                         "session key ("
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         "%02x%02x%02x%02x"
                         ")",
                         session_key[0] & 0xFF,  session_key[1] & 0xFF,
                         session_key[2] & 0xFF,  session_key[3] & 0xFF,
                         session_key[4] & 0xFF,  session_key[5] & 0xFF,
                         session_key[6] & 0xFF,  session_key[7] & 0xFF,
                         session_key[8] & 0xFF,  session_key[9] & 0xFF,
                         session_key[10] & 0xFF, session_key[11] & 0xFF,
                         session_key[12] & 0xFF, session_key[13] & 0xFF,
                         session_key[14] & 0xFF, session_key[15] & 0xFF);
            }
            else {
                ws_debug("Session key not found !");
                memset(&vars->session_key,0,16);
            }
        }
    }

    return offset;
}

static int
netlogon_dissect_netrserverauthenticate3_reply(tvbuff_t *tvb, int offset,
                                               packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    return netlogon_dissect_netrserverauthenticate023_reply(tvb,offset,pinfo,tree,di,drep,3);
}

static int
netlogon_dissect_netrserverauthenticate2_reply(tvbuff_t *tvb, int offset,
                                               packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    return netlogon_dissect_netrserverauthenticate023_reply(tvb,offset,pinfo,tree,di,drep,2);
}

static int
netlogon_dissect_netrserverauthenticatekerberos_reply(tvbuff_t *tvb, int offset,
                                                      packet_info *pinfo,
                                                      proto_tree *tree,
                                                      dcerpc_info *di,
                                                      uint8_t *drep)
{
    netlogon_auth_vars *vars = NULL;
    uint32_t flags = 0;

    flags = tvb_get_letohl (tvb, offset);
    netlogon_dissect_neg_options(tvb,tree,flags,offset);
    offset +=4;
    ALIGN_TO_4_BYTES;
    offset = dissect_dcerpc_uint32(tvb, offset, pinfo, tree, drep,
                                   hf_server_rid, NULL);
    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    vars = find_tmp_netlogon_auth_vars(pinfo, 1);
    if (vars != NULL) {
        vars->flags = flags;
        snprintf(vars->nthash.key_origin, NTLMSSP_MAX_ORIG_LEN,
                 "ServerAuthenticateKerberos(%s) at frame %u",
                 vars->client_name, pinfo->num);
        vars->auth_fd_num = pinfo->num;
        expert_add_info_format(pinfo, proto_tree_get_parent(tree),
                               &ei_netlogon_session_key,
                               "zero session key");
    } else {
        ws_debug("ServerAuthenticateKerberos request not found !");
    }

    return offset;
}


static int
netlogon_dissect_dsrgetdcnameex_rqst(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_nt_GUID, NDR_POINTER_UNIQUE,
                                 "GUID pointer: domain_guid", -1);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Site Name", hf_netlogon_site_name, 0);

    offset = netlogon_dissect_GET_DCNAME_REQUEST_FLAGS(tvb, offset, pinfo, tree, di, drep);

    return offset;
}


static int
netlogon_dissect_dsrgetdcnameex_reply(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DOMAIN_CONTROLLER_INFO, NDR_POINTER_UNIQUE,
                                 "DOMAIN_CONTROLLER_INFO:", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_dsrgetsitename_rqst(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    return offset;
}


static int
netlogon_dissect_dsrgetsitename_reply(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{

    /* XXX hmmm this does not really look like a UNIQUE pointer but
       will do for now.   I think it is really a 32bit integer followed by
       a REF pointer to a unicode string */
    offset = dissect_ndr_pointer_cb(tvb, offset, pinfo, tree, di, drep,
                                    dissect_ndr_wchar_cvstring, NDR_POINTER_UNIQUE, "Site Name",
                                    hf_netlogon_site_name, cb_wstr_postprocess,
                                    GINT_TO_POINTER(CB_STR_COL_INFO | 1));

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrlogongetdomaininfo_rqst(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    /* Unlike the other NETLOGON RPCs, this is not a unique pointer. */
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle", hf_netlogon_computer_name, 0);
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: client", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_WORKSTATION_BUFFER, NDR_POINTER_REF,
                                 "WORKSTATION_BUFFER", -1);
    return offset;
}


static int
netlogon_dissect_netrlogongetdomaininfo_reply(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DOMAIN_INFORMATION, NDR_POINTER_REF,
                                 "DOMAIN_INFORMATION", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrserverpasswordset2_rqst(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Acct Name",
                                          hf_netlogon_acct_name, 0);

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = netlogon_dissect_netr_CryptPassword(tvb, offset,
                                                 pinfo, tree, di, drep);

    return offset;
}


static int
netlogon_dissect_netrserverpasswordset2_reply(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrserverpasswordget_rqst(tvbuff_t *tvb, int offset,
                                            packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Acct Name", hf_netlogon_acct_name, 0);

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    return offset;
}


static int
netlogon_dissect_netrserverpasswordget_reply(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_LM_OWF_PASSWORD, NDR_POINTER_REF,
                                 "LM_OWF_PASSWORD pointer: server_pwd", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

#if GCRYPT_VERSION_NUMBER >= 0x010800 /* 1.8.0 */
static gcry_error_t prepare_session_key_cipher_aes(netlogon_auth_vars *vars,
                                                   gcry_cipher_hd_t *_cipher_hd)
{
    gcry_error_t err;
    gcry_cipher_hd_t cipher_hd = NULL;
    uint8_t iv[16] = { 0 };

    /* Open the cipher */
    err = gcry_cipher_open(&cipher_hd, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CFB8, 0);
    if (err != 0) {
        ws_warning("GCRY: cipher open %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        return err;
    }

    /* Set the initial value */
    err = gcry_cipher_setiv(cipher_hd, iv, sizeof(iv));
    if (err != 0) {
        ws_warning("GCRY: setiv %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return err;
    }

    /* Set the key */
    err = gcry_cipher_setkey(cipher_hd, vars->session_key, 16);
    if (err != 0) {
        ws_warning("GCRY: setkey %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return err;
    }

    *_cipher_hd = cipher_hd;
    return 0;
}
#endif

static gcry_error_t prepare_session_key_cipher_strong(netlogon_auth_vars *vars,
                                                      gcry_cipher_hd_t *_cipher_hd)
{
    gcry_error_t err;
    gcry_cipher_hd_t cipher_hd = NULL;

    /* Open the cipher */
    err = gcry_cipher_open(&cipher_hd, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0);
    if (err != 0) {
        ws_warning("GCRY: cipher open %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        return err;
    }

    /* Set the key */
    err = gcry_cipher_setkey(cipher_hd, vars->session_key, 16);
    if (err != 0) {
        ws_warning("GCRY: setkey %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return err;
    }

    *_cipher_hd = cipher_hd;
    return 0;
}

static gcry_error_t prepare_session_key_cipher(netlogon_auth_vars *vars,
                                               gcry_cipher_hd_t *_cipher_hd)
{
    *_cipher_hd = NULL;

#if GCRYPT_VERSION_NUMBER >= 0x010800 /* 1.8.0 */
    if (vars->flags & NETLOGON_FLAG_AES) {
        return prepare_session_key_cipher_aes(vars, _cipher_hd);
    }
#endif

    if (vars->flags & NETLOGON_FLAG_STRONGKEY) {
        return prepare_session_key_cipher_strong(vars, _cipher_hd);
    }

    return GPG_ERR_UNSUPPORTED_ALGORITHM;
}

static int
netlogon_dissect_opaque_buffer_block(tvbuff_t *tvb, int offset, int length,
                                     packet_info *pinfo, proto_tree *tree,
                                     dcerpc_info *di, uint8_t *drep _U_)
{
    int orig_offset = offset;
    unsigned char is_server = 0;
    netlogon_auth_vars *vars;
    gcry_error_t err;
    gcry_cipher_hd_t cipher_hd = NULL;
    uint8_t *buffer = NULL;
    tvbuff_t *dectvb = NULL;
    uint32_t expected_len;
    uint32_t decrypted_len;

    proto_tree_add_item(tree, di->hf_index, tvb, offset, length, ENC_NA);
    offset += length;

    if (length < 8) {
        return offset;
    }

    vars = find_global_netlogon_auth_vars(pinfo, is_server);
    if (vars == NULL ) {
        ws_debug("Vars not found %d (packet_data)",wmem_map_size(netlogon_auths));
        expert_add_info_format(pinfo, proto_tree_get_parent(tree),
            &ei_netlogon_session_key,
            "No session key found");
        return offset;
    }

    err = prepare_session_key_cipher(vars, &cipher_hd);
    if (err != 0) {
        ws_warning("GCRY: prepare_session_key_cipher %s/%s\n",
                   gcry_strsource(err), gcry_strerror(err));
        return offset;
    }

    buffer = (uint8_t*)tvb_memdup(pinfo->pool, tvb, orig_offset, length);
    if (buffer == NULL) {
        gcry_cipher_close(cipher_hd);
        return offset;
    }

    err = gcry_cipher_decrypt(cipher_hd, buffer, length, NULL, 0);
    gcry_cipher_close(cipher_hd);
    if (err != 0) {
        ws_warning("GCRY: prepare_session_key_cipher %s/%s\n",
                   gcry_strsource(err), gcry_strerror(err));
        return offset;
    }

    dectvb = tvb_new_child_real_data(tvb, buffer, length, length);
    if (dectvb == NULL) {
        return offset;
    }

    expected_len = length - 8;
    decrypted_len = tvb_get_letohl(dectvb, 4);
    if (decrypted_len != expected_len) {
        expert_add_info_format(pinfo, proto_tree_get_parent(tree),
             &ei_netlogon_session_key,
             "Unusable session key learned in frame %d ("
             "%02x%02x%02x%02x"
             ") from %s",
             vars->auth_fd_num,
             vars->session_key[0] & 0xFF,  vars->session_key[1] & 0xFF,
             vars->session_key[2] & 0xFF,  vars->session_key[3] & 0xFF,
             vars->nthash.key_origin);
        return offset;
    }

    expert_add_info_format(pinfo, proto_tree_get_parent(tree),
             &ei_netlogon_session_key,
             "Using session key learned in frame %d ("
             "%02x%02x%02x%02x"
             ") from %s",
             vars->auth_fd_num,
             vars->session_key[0] & 0xFF,  vars->session_key[1] & 0xFF,
             vars->session_key[2] & 0xFF,  vars->session_key[3] & 0xFF,
             vars->nthash.key_origin);

    add_new_data_source(pinfo, dectvb, "OpaqueBuffer (Decrypted)");

    proto_tree_add_item(tree, hf_netlogon_opaque_buffer_dec, dectvb, 0, length, ENC_NA);
    return offset;
}

static int
netlogon_dissect_opaque_buffer(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree,
                            dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray_block(tvb, offset, pinfo, tree, di, drep,
                                       netlogon_dissect_opaque_buffer_block);

    return offset;
}

/*
 * IDL long NetrLogonSendToSam(
 * IDL      [in][unique][string] wchar_t *ServerName,
 * IDL      [in][ref][string] wchar_t *Workstation,
 * IDL      [in][ref] AUTHENTICATOR *credential,
 * IDL      [in][out][ref] AUTHENTICATOR *returnauthenticator,
 * IDL      [in, size_is(OpaqueBufferSize)][ref] UCHAR * OpaqueBuffer,
 * IDL      [in] ULONG OpaqueBufferSize
 * IDL );
 */
static int
netlogon_dissect_netrlogonsendtosam_rqst(tvbuff_t *tvb, int offset,
                                         packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_opaque_buffer, NDR_POINTER_REF,
                                 "OpaqueBuffer", hf_netlogon_opaque_buffer_enc);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_opaque_buffer_size, NULL);

    return offset;
}


static int
netlogon_dissect_netrlogonsendtosam_reply(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_dsraddresstositenamesw_rqst(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_UNIQUE,
                                 "BYTE pointer: unknown_BYTE", -1);

    return offset;
}


static int
netlogon_dissect_dsraddresstositenamesw_reply(tvbuff_t *tvb, int offset,
                                              packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_TYPE_50_ptr, NDR_POINTER_UNIQUE,
                                 "TYPE_50** pointer: unknown_TYPE_50", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_dsrgetdcnameex2_rqst(tvbuff_t *tvb, int offset,
                                      packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Client Account",
                                          hf_netlogon_acct_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Client Account",
                                          hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_nt_GUID, NDR_POINTER_UNIQUE,
                                 "Domain GUID:", -1);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Client Site",
                                          hf_netlogon_site_name, 0);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    return offset;
}


static int
netlogon_dissect_dsrgetdcnameex2_reply(tvbuff_t *tvb, int offset,
                                       packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DOMAIN_CONTROLLER_INFO, NDR_POINTER_UNIQUE,
                                 "DOMAIN_CONTROLLER_INFO:", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrlogongettimeserviceparentdomain_rqst(tvbuff_t *tvb, int offset,
                                                          packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    return offset;
}


static int
netlogon_dissect_netrlogongettimeserviceparentdomain_reply(tvbuff_t *tvb, int offset,
                                                           packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "unknown string",
                                          hf_netlogon_unknown_string, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_pointer_long, NDR_POINTER_UNIQUE,
                                 "ULONG pointer: unknown_ULONG", hf_netlogon_unknown_long);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrenumeratetrusteddomainsex_rqst(tvbuff_t *tvb, int offset,
                                                    packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    return offset;
}

static int
netlogon_dissect_netrenumeratetrusteddomainsex_reply(tvbuff_t *tvb, int offset,
                                                     packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_entries, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DS_DOMAIN_TRUSTS_ARRAY, NDR_POINTER_UNIQUE,
                                 "DS_DOMAIN_TRUSTS_ARRAY:", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_dsraddresstositenamesexw_rqst(tvbuff_t *tvb, int offset,
                                               packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_long, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_BYTE_array, NDR_POINTER_UNIQUE,
                                 "BYTE pointer: unknown_BYTE", -1);

    return offset;
}


static int
netlogon_dissect_dsraddresstositenamesexw_reply(tvbuff_t *tvb, int offset,
                                                packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_TYPE_52_ptr, NDR_POINTER_UNIQUE,
                                 "TYPE_52 pointer: unknown_TYPE_52", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


static int
netlogon_dissect_site_name_item(tvbuff_t *tvb, int offset,
                                packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_counted_string_cb(
        tvb, offset, pinfo, tree, di, drep, hf_netlogon_site_name,
        cb_wstr_postprocess,
        GINT_TO_POINTER(CB_STR_COL_INFO | 1));

    return offset;
}
static int
netlogon_dissect_site_name_array(tvbuff_t *tvb, int offset,
                                 packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_site_name_item);

    return offset;
}

static int
netlogon_dissect_site_names(tvbuff_t *tvb, int offset,
                            packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_count, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_site_name_array, NDR_POINTER_UNIQUE,
                                 "Site name array", -1);

    return offset;
}

static int
netlogon_dissect_dsrgetdcsitecoveragew_rqst(tvbuff_t *tvb, int offset,
                                            packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    return offset;
}


static int
netlogon_dissect_dsrgetdcsitecoveragew_reply(tvbuff_t *tvb, int offset,
                                             packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_site_names, NDR_POINTER_UNIQUE,
                                 "Site names", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_netrlogonsamlogonex_rqst(tvbuff_t *tvb, int offset,
                                          packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "LogonServer",
                                          hf_netlogon_computer_name, 0);
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Computer Name",
                                          hf_netlogon_computer_name, 0);
    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level16, NULL);
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_LEVEL, NDR_POINTER_REF,
                                 "LEVEL: LogonLevel", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_validation_level, NULL);

    offset = netlogon_dissect_EXTRA_FLAGS(tvb, offset, pinfo, tree, di, drep);

#if 0
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "unknown string",
                                          hf_netlogon_unknown_string, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "unknown string",
                                          hf_netlogon_unknown_string, 0);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_short, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_LEVEL, NDR_POINTER_UNIQUE,
                                 "LEVEL pointer: unknown_NETLOGON_LEVEL", -1);

    offset = dissect_ndr_uint16(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_unknown_short, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_pointer_long, NDR_POINTER_UNIQUE,
                                 "ULONG pointer: unknown_ULONG", hf_netlogon_unknown_long);
#endif
    return offset;
}


static int
netlogon_dissect_netrlogonsamlogonex_reply(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION, NDR_POINTER_REF,
                                 "VALIDATION:", -1);

    offset = dissect_ndr_uint8(tvb, offset, pinfo, tree, di, drep,
                               hf_netlogon_authoritative, NULL);

    offset = netlogon_dissect_EXTRA_FLAGS(tvb, offset, pinfo, tree, di, drep);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);
#if 0
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_VALIDATION, NDR_POINTER_UNIQUE,
                                 "VALIDATION: unknown_NETLOGON_VALIDATION", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_pointer_char, NDR_POINTER_UNIQUE,
                                 "BOOLEAN pointer: unknown_BOOLEAN", hf_netlogon_unknown_char);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_pointer_long, NDR_POINTER_UNIQUE,
                                 "ULONG pointer: unknown_ULONG", hf_netlogon_unknown_long);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);
#endif
    return offset;
}

static int
netlogon_dissect_netrservertrustpasswordsget_rqst(tvbuff_t *tvb,
                                             int offset,
                                             packet_info *pinfo,
                                             proto_tree *tree,
                                             dcerpc_info *di,
                                             uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Acct Name",
                                          hf_netlogon_acct_name, 0);

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    return offset;
}

static int
netlogon_dissect_netrservertrustpasswordsget_reply(tvbuff_t *tvb,
                                              int offset,
                                              packet_info *pinfo,
                                              proto_tree *tree,
                                              dcerpc_info *di,
                                              uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NT_OWF_PASSWORD, NDR_POINTER_REF,
                                 "NT_OWF_PASSWORD pointer: new_password", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NT_OWF_PASSWORD, NDR_POINTER_REF,
                                 "NT_OWF_PASSWORD pointer: old_password", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}


static int
netlogon_dissect_netrservergettrustinfo_rqst(tvbuff_t *tvb,
                                             int offset,
                                             packet_info *pinfo,
                                             proto_tree *tree,
                                             dcerpc_info *di,
                                             uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Acct Name",
                                          hf_netlogon_acct_name, 0);

    offset = netlogon_dissect_NETLOGON_SECURE_CHANNEL_TYPE(tvb, offset,
                                                           pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Computer Name",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    return offset;
}

static int
netlogon_dissect_NL_GENERIC_RPC_DATA_UINT32_ARRAY(tvbuff_t *tvb, int offset,
                                                  packet_info *pinfo _U_, proto_tree *tree,
                                                  dcerpc_info *di, uint8_t *drep _U_)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DOMAIN_TRUST_ATTRIBS);

    return offset;
}

static int
netlogon_dissect_NL_GENERIC_RPC_DATA_STRING(tvbuff_t *tvb, int offset,
                                            packet_info *pinfo _U_, proto_tree *tree,
                                            dcerpc_info *di, uint8_t *drep _U_)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }
// TODO
    offset = dissect_ndr_counted_string(tvb, offset, pinfo, tree, di, drep,
                                        hf_netlogon_package_name, 0|CB_STR_SAVE);

    return offset;
}

static int
netlogon_dissect_NL_GENERIC_RPC_DATA_STRING_ARRAY(tvbuff_t *tvb, int offset,
                                                  packet_info *pinfo _U_, proto_tree *tree,
                                                  dcerpc_info *di, uint8_t *drep _U_)
{
    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    offset = dissect_ndr_ucarray(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NL_GENERIC_RPC_DATA_STRING);

    return offset;
}

static int
netlogon_dissect_NL_GENERIC_RPC_DATA(tvbuff_t *tvb, int offset,
                                     packet_info *pinfo _U_, proto_tree *parent_tree,
                                     dcerpc_info *di, uint8_t *drep _U_)
{
    proto_item *item=NULL;
    proto_tree *tree=NULL;

    if(di->conformant_run){
        /*just a run to handle conformant arrays, nothing to dissect.*/
        return offset;
    }

    if(parent_tree){
        tree = proto_tree_add_subtree(parent_tree, tvb, offset, -1,
                                      ett_NL_GENERIC_RPC_DATA, &item,
                                      "NL_GENERIC_RPC_DATA:");
    }

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_trust_len, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NL_GENERIC_RPC_DATA_UINT32_ARRAY,
                                 NDR_POINTER_UNIQUE,
                                 "UINT32 ARRAY pointer: ", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_trust_len, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NL_GENERIC_RPC_DATA_STRING_ARRAY,
                                 NDR_POINTER_UNIQUE,
                                 "STRING ARRAY pointer: ", -1);

    return offset;
}

static int
netlogon_dissect_netrservergettrustinfo_reply(tvbuff_t *tvb,
                                              int offset,
                                              packet_info *pinfo,
                                              proto_tree *tree,
                                              dcerpc_info *di,
                                              uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NT_OWF_PASSWORD, NDR_POINTER_REF,
                                 "NT_OWF_PASSWORD pointer: new_password", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NT_OWF_PASSWORD, NDR_POINTER_REF,
                                 "NT_OWF_PASSWORD pointer: old_password", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_NL_GENERIC_RPC_DATA, NDR_POINTER_UNIQUE,
                                 "NL_GENERIC_RPC_DATA pointer: trust_info", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

static int
netlogon_dissect_dsrenumeratedomaintrusts_rqst(tvbuff_t *tvb, int offset,
                                               packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = netlogon_dissect_DOMAIN_TRUST_FLAGS(tvb, offset, pinfo, tree, di, drep);

    return offset;
}


static int
netlogon_dissect_dsrenumeratedomaintrusts_reply(tvbuff_t *tvb, int offset,
                                                packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_entries, NULL);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_DS_DOMAIN_TRUSTS_ARRAY, NDR_POINTER_UNIQUE,
                                 "DS_DOMAIN_TRUSTS_ARRAY:", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_dos_rc, NULL);

    return offset;
}

static int
netlogon_dissect_dsrderegisterdnshostrecords_rqst(tvbuff_t *tvb, int offset,
                                                  packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = netlogon_dissect_LOGONSRV_HANDLE(tvb, offset,
                                              pinfo, tree, di, drep);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_UNIQUE, "Domain", hf_netlogon_logon_dom, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_nt_GUID, NDR_POINTER_UNIQUE,
                                 "GUID pointer: domain_guid", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 dissect_nt_GUID, NDR_POINTER_UNIQUE,
                                 "GUID pointer: dsa_guid", -1);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "dns_host", hf_netlogon_dns_host, 0);

    return offset;
}


static int
netlogon_dissect_dsrderegisterdnshostrecords_reply(tvbuff_t *tvb, int offset,
                                                   packet_info *pinfo, proto_tree *tree, dcerpc_info *di, uint8_t *drep)
{
    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

/*
 * TODO
 * IDL long NetrChainSetClientAttributes(
 * IDL );

NetrChainSetClientAttributes(
[in,string,ref] LOGONSRV_HANDLE PrimaryName,
[in,string,ref] wchar_t * ChainedFromServerName,
[in,string,ref] wchar_t * ChainedForClientName,
[in,ref] PNETLOGON_AUTHENTICATOR Authenticator,
[in,out,ref] PNETLOGON_AUTHENTICATOR ReturnAuthenticator,
[in] DWORD dwInVersion,
[in,ref] [switch_is(dwInVersion)]
NL_IN_CHAIN_SET_CLIENT_ATTRIBUTES *pmsgIn,
[in,out,ref] DWORD * pdwOutVersion,
[in,out,ref] [switch_is(*pdwOutVersion)]
NL_OUT_CHAIN_SET_CLIENT_ATTRIBUTES *pmsgOut
);

typedef struct _NL_OSVERSIONINFO_V1{
DWORD dwOSVersionInfoSize;
DWORD dwMajorVersion;
DWORD dwMinorVersion;
DWORD dwBuildNumber;
DWORD dwPlatformId;
wchar_t szCSDVersion[128];
USHORT wServicePackMajor;
USHORT wServicePackMinor;
USHORT wSuiteMask;
UCHAR wProductType;
UCHAR wReserved;
} NL_OSVERSIONINFO_V1;
typedef struct _NL_IN_CHAIN_SET_CLIENT_ATTRIBUTES_V1{
[unique,string] wchar_t * ClientDnsHostName;
[unique] NL_OSVERSIONINFO_V1 *OsVersionInfo_V1;
[unique,string] wchar_t * OsName;
} NL_IN_CHAIN_SET_CLIENT_ATTRIBUTES_V1;
typedef [switch_type(DWORD)] union{
[case(1)] NL_IN_CHAIN_SET_CLIENT_ATTRIBUTES_V1 V1;
} NL_IN_CHAIN_SET_CLIENT_ATTRIBUTES;
typedef struct _NL_OUT_CHAIN_SET_CLIENT_ATTRIBUTES_V1{
[unique,string] wchar_t *HubName;
[unique,string] wchar_t **OldDnsHostName;
[unique] ULONG * SupportedEncTypes;
} NL_OUT_CHAIN_SET_CLIENT_ATTRIBUTES_V1;
typedef [switch_type(DWORD)] union{
[case(1)] NL_OUT_CHAIN_SET_CLIENT_ATTRIBUTES_V1 V1;
} NL_OUT_CHAIN_SET_CLIENT_ATTRIBUTES;

static int
netlogon_dissect_NL_IN_CHAIN_SET_CLIENT_ATTRIBUTES(tvbuff_t *tvb, int offset,
                                    packet_info *pinfo, proto_tree *tree,
                                    dcerpc_info *di, uint8_t *drep)
{
    uint32_t level;

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, &level);
    switch (level) {
    case 1:
        offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                     netlogon_dissect_WORKSTATION_INFORMATION, NDR_POINTER_UNIQUE,
                                     "LSA POLICY INFO", -1);
        break;
    }
    return offset;
}
 */
static int
netlogon_dissect_netrchainsetclientattributes_rqst(tvbuff_t *tvb, int offset,
                                                   packet_info *pinfo,
                                                   proto_tree *tree,
                                                   dcerpc_info *di,
                                                   uint8_t *drep)
{
    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "Server Handle",
                                          hf_netlogon_logonsrv_handle, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "ChainedFromServerName",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_str_pointer_item(tvb, offset, pinfo, tree, di, drep,
                                          NDR_POINTER_REF, "ChainedForClientName",
                                          hf_netlogon_computer_name, 0);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: credential", -1);

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL); // in_version

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 NULL, NDR_POINTER_REF,
                                 "IN_CHAIN_SET_CLIENT_ATTRIBUTES", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL); // out_version

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 NULL, NDR_POINTER_REF,
                                 "OUT_CHAIN_SET_CLIENT_ATTRIBUTES", -1);

    return offset;
}

static int
netlogon_dissect_netrchainsetclientattributes_reply(tvbuff_t *tvb, int offset,
                                                    packet_info *pinfo,
                                                    proto_tree *tree,
                                                    dcerpc_info *di,
                                                    uint8_t *drep)
{
    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 netlogon_dissect_AUTHENTICATOR, NDR_POINTER_REF,
                                 "AUTHENTICATOR: return_authenticator", -1);

    offset = dissect_ndr_uint32(tvb, offset, pinfo, tree, di, drep,
                                hf_netlogon_level, NULL); // out_version

    offset = dissect_ndr_pointer(tvb, offset, pinfo, tree, di, drep,
                                 NULL, NDR_POINTER_REF,
                                 "OUT_CHAIN_SET_CLIENT_ATTRIBUTES", -1);

    offset = dissect_ntstatus(tvb, offset, pinfo, tree, di, drep,
                              hf_netlogon_rc, NULL);

    return offset;
}

/* Dissect secure channel stuff */

static int hf_netlogon_secchan_nl_message_type;
static int hf_netlogon_secchan_nl_message_flags;
static int hf_netlogon_secchan_nl_message_flags_nb_domain;
static int hf_netlogon_secchan_nl_message_flags_nb_host;
static int hf_netlogon_secchan_nl_message_flags_dns_domain;
static int hf_netlogon_secchan_nl_message_flags_dns_host;
static int hf_netlogon_secchan_nl_message_flags_nb_host_utf8;
static int hf_netlogon_secchan_nl_nb_domain;
static int hf_netlogon_secchan_nl_nb_host;
static int hf_netlogon_secchan_nl_dns_domain;
static int hf_netlogon_secchan_nl_dns_host;
static int hf_netlogon_secchan_nl_nb_host_utf8;

static int ett_secchan_verf;
static int ett_secchan_nl_auth_message;
static int ett_secchan_nl_auth_message_flags;

static const value_string nl_auth_types[] = {
    { 0x00000000,         "Request"},
    { 0x00000001,         "Response"},
    { 0, NULL }
};


/* MS-NRPC : 2.2.1.3.1 NL_AUTH_MESSAGE */
static int dissect_secchan_nl_auth_message(tvbuff_t *tvb, int offset,
                                           packet_info *pinfo,
                                           proto_tree *tree, dcerpc_info *di _U_, uint8_t *drep)
{
    dcerpc_auth_info *auth_info = di->auth_info;
    proto_item *item = NULL;
    proto_tree *subtree = NULL;
    uint32_t messagetype;
    uint64_t messageflags;
    static int * const flag_fields[] = {
        &hf_netlogon_secchan_nl_message_flags_nb_domain,
        &hf_netlogon_secchan_nl_message_flags_nb_host,
        &hf_netlogon_secchan_nl_message_flags_dns_domain,
        &hf_netlogon_secchan_nl_message_flags_dns_host,
        &hf_netlogon_secchan_nl_message_flags_nb_host_utf8,
        NULL
    };
    int len;
    netlogon_auth_vars *vars = NULL;
    unsigned char is_server;

    if (tree) {
        subtree = proto_tree_add_subtree(
            tree, tvb, offset, -1, ett_secchan_nl_auth_message, &item,
            "Secure Channel NL_AUTH_MESSAGE");
    }

    /* We can't use the NDR routines as the DCERPC call data hasn't
       been initialised since we haven't made a DCERPC call yet, just
       a bind request. */

    /* Type */
    offset = dissect_dcerpc_uint32(
        tvb, offset, pinfo, subtree, drep,
        hf_netlogon_secchan_nl_message_type, &messagetype);

    /* Flags */
    proto_tree_add_bitmask_ret_uint64(subtree, tvb, offset,
                                      hf_netlogon_secchan_nl_message_flags,
                                      ett_secchan_nl_auth_message_flags,
                                      flag_fields,
                                      (drep[0] & DREP_LITTLE_ENDIAN) ?
                                          ENC_LITTLE_ENDIAN :
                                          ENC_BIG_ENDIAN,
                                      &messageflags);
    offset += 4;


    /* Buffer */
    /* netbios domain name */
    if (messageflags&0x00000001) {
        len = tvb_strsize(tvb, offset);
        proto_tree_add_item(subtree, hf_netlogon_secchan_nl_nb_domain, tvb, offset, len, ENC_ASCII);
        offset += len;
    }

    /* netbios host name */
    if (messageflags&0x00000002) {
        len = tvb_strsize(tvb, offset);
        proto_tree_add_item(subtree, hf_netlogon_secchan_nl_nb_host, tvb, offset, len, ENC_ASCII);
        offset += len;
    }

    /* DNS domain name */
    if (messageflags&0x00000004) {
        int old_offset=offset;
        char *str;

        offset=dissect_mscldap_string(pinfo->pool, tvb, offset, 255, &str);
        proto_tree_add_string(subtree, hf_netlogon_secchan_nl_dns_domain, tvb, old_offset, offset-old_offset, str);
    }

    /* DNS host name */
    if (messageflags&0x00000008) {
        int old_offset=offset;
        char *str;

        offset=dissect_mscldap_string(pinfo->pool, tvb, offset, 255, &str);
        proto_tree_add_string(subtree, hf_netlogon_secchan_nl_dns_host, tvb, old_offset, offset-old_offset, str);
    }

    /* NetBios host name (UTF8) */
    if (messageflags&0x00000010) {
        int old_offset=offset;
        char *str;

        offset=dissect_mscldap_string(pinfo->pool, tvb, offset, 255, &str);
        proto_tree_add_string(subtree, hf_netlogon_secchan_nl_nb_host_utf8, tvb, old_offset, offset-old_offset, str);
    }

    switch (di->ptype) {
    case PDU_BIND:
    case PDU_ALTER:
    case PDU_AUTH3:
        is_server = 0;
        break;
    case PDU_BIND_ACK:
    case PDU_BIND_NAK:
    case PDU_ALTER_ACK:
    case PDU_FAULT:
        is_server = 1;
        break;
    default:
        return offset;
    }

    vars = find_or_create_schannel_netlogon_auth_vars(pinfo, auth_info, is_server);
    if (vars != NULL) {
        expert_add_info_format(pinfo, proto_tree_get_parent(subtree),
                               &ei_netlogon_session_key,
                               "Using session key learned in frame %d ("
                               "%02x%02x%02x%02x"
                               ") from %s",
                               vars->auth_fd_num,
                               vars->session_key[0] & 0xFF,
                               vars->session_key[1] & 0xFF,
                               vars->session_key[2] & 0xFF,
                               vars->session_key[3] & 0xFF,
                               vars->nthash.key_origin);
    }
    else
    {
        ws_debug("Vars not found (is null %d) %d (dissect_verf)",vars==NULL,wmem_map_size(netlogon_auths));
    }

    return offset;
}

/* Subdissectors */

static const dcerpc_sub_dissector dcerpc_netlogon_dissectors[] = {
    { NETLOGON_NETRLOGONUASLOGON, "NetrLogonUasLogon",
      netlogon_dissect_netrlogonuaslogon_rqst,
      netlogon_dissect_netrlogonuaslogon_reply },
    { NETLOGON_NETRLOGONUASLOGOFF, "NetrLogonUasLogoff",
      netlogon_dissect_netrlogonuaslogoff_rqst,
      netlogon_dissect_netrlogonuaslogoff_reply },
    { NETLOGON_NETRLOGONSAMLOGON, "NetrLogonSamLogon",
      netlogon_dissect_netrlogonsamlogon_rqst,
      netlogon_dissect_netrlogonsamlogon_reply },
    { NETLOGON_NETRLOGONSAMLOGOFF, "NetrLogonSamLogoff",
      netlogon_dissect_netrlogonsamlogoff_rqst,
      netlogon_dissect_netrlogonsamlogoff_reply },
    { NETLOGON_NETRSERVERREQCHALLENGE, "NetrServerReqChallenge",
      netlogon_dissect_netrserverreqchallenge_rqst,
      netlogon_dissect_netrserverreqchallenge_reply },
    { NETLOGON_NETRSERVERAUTHENTICATE, "NetrServerAuthenticate",
      netlogon_dissect_netrserverauthenticate_rqst,
      netlogon_dissect_netrserverauthenticate_reply },
    { NETLOGON_NETRSERVERPASSWORDSET, "NetrServerPasswordSet",
      netlogon_dissect_netrserverpasswordset_rqst,
      netlogon_dissect_netrserverpasswordset_reply },
    { NETLOGON_NETRDATABASEDELTAS, "NetrDatabaseDeltas",
      netlogon_dissect_netrdatabasedeltas_rqst,
      netlogon_dissect_netrdatabasedeltas_reply },
    { NETLOGON_NETRDATABASESYNC, "NetrDatabaseSync",
      netlogon_dissect_netrdatabasesync_rqst,
      netlogon_dissect_netrdatabasesync_reply },
    { NETLOGON_NETRACCOUNTDELTAS, "NetrAccountDeltas",
      netlogon_dissect_netraccountdeltas_rqst,
      netlogon_dissect_netraccountdeltas_reply },
    { NETLOGON_NETRACCOUNTSYNC, "NetrAccountSync",
      netlogon_dissect_netraccountsync_rqst,
      netlogon_dissect_netraccountsync_reply },
    { NETLOGON_NETRGETDCNAME, "NetrGetDCName",
      netlogon_dissect_netrgetdcname_rqst,
      netlogon_dissect_netrgetdcname_reply },
    { NETLOGON_NETRLOGONCONTROL, "NetrLogonControl",
      netlogon_dissect_netrlogoncontrol_rqst,
      netlogon_dissect_netrlogoncontrol_reply },
    { NETLOGON_NETRGETANYDCNAME, "NetrGetAnyDCName",
      netlogon_dissect_netrgetanydcname_rqst,
      netlogon_dissect_netrgetanydcname_reply },
    { NETLOGON_NETRLOGONCONTROL2, "NetrLogonControl2",
      netlogon_dissect_netrlogoncontrol2_rqst,
      netlogon_dissect_netrlogoncontrol2_reply },
    { NETLOGON_NETRSERVERAUTHENTICATE2, "NetrServerAuthenticate2",
      netlogon_dissect_netrserverauthenticate2_rqst,
      netlogon_dissect_netrserverauthenticate2_reply },
    { NETLOGON_NETRDATABASESYNC2, "NetrDatabaseSync2",
      netlogon_dissect_netrdatabasesync2_rqst,
      netlogon_dissect_netrdatabasesync2_reply },
    { NETLOGON_NETRDATABASEREDO, "NetrDatabaseRedo",
      netlogon_dissect_netrdatabaseredo_rqst,
      netlogon_dissect_netrdatabaseredo_reply },
    { NETLOGON_NETRLOGONCONTROL2EX, "NetrLogonControl2Ex",
      netlogon_dissect_netrlogoncontrol2ex_rqst,
      netlogon_dissect_netrlogoncontrol2ex_reply },
    { NETLOGON_NETRENUMERATETRUSTEDDOMAINS, "NetrEnumerateTrustedDomains",
      netlogon_dissect_netrenumeratetrusteddomains_rqst,
      netlogon_dissect_netrenumeratetrusteddomains_reply },
    { NETLOGON_DSRGETDCNAME, "DsrGetDcName",
      netlogon_dissect_dsrgetdcname_rqst,
      netlogon_dissect_dsrgetdcname_reply },
    { NETLOGON_NETRLOGONDUMMYROUTINE1, "NetrLogonGetCapabilities",
      netlogon_dissect_netrlogondummyroutine1_rqst,
      netlogon_dissect_netrlogondummyroutine1_reply },
    { NETLOGON_NETRLOGONSETSERVICEBITS, "NetrLogonSetServiceBits",
      netlogon_dissect_netrlogonsetservicebits_rqst,
      netlogon_dissect_netrlogonsetservicebits_reply },
    { NETLOGON_NETRLOGONGETTRUSTRID, "NetrLogonGetTrustRid",
      netlogon_dissect_netrlogongettrustrid_rqst,
      netlogon_dissect_netrlogongettrustrid_reply },
    { NETLOGON_NETRLOGONCOMPUTESERVERDIGEST, "NetrLogonComputeServerDigest",
      netlogon_dissect_netrlogoncomputeserverdigest_rqst,
      netlogon_dissect_netrlogoncomputeserverdigest_reply },
    { NETLOGON_NETRLOGONCOMPUTECLIENTDIGEST, "NetrLogonComputeClientDigest",
      netlogon_dissect_netrlogoncomputeclientdigest_rqst,
      netlogon_dissect_netrlogoncomputeclientdigest_reply },
    { NETLOGON_NETRSERVERAUTHENTICATE3, "NetrServerAuthenticate3",
      netlogon_dissect_netrserverauthenticate3_rqst,
      netlogon_dissect_netrserverauthenticate3_reply },
    { NETLOGON_DSRGETDCNAMEX, "DsrGetDcNameEx",
      netlogon_dissect_dsrgetdcnameex_rqst,
      netlogon_dissect_dsrgetdcnameex_reply },
    { NETLOGON_DSRGETSITENAME, "DsrGetSiteName",
      netlogon_dissect_dsrgetsitename_rqst,
      netlogon_dissect_dsrgetsitename_reply },
    { NETLOGON_NETRLOGONGETDOMAININFO, "NetrLogonGetDomainInfo",
      netlogon_dissect_netrlogongetdomaininfo_rqst,
      netlogon_dissect_netrlogongetdomaininfo_reply },
    { NETLOGON_NETRSERVERPASSWORDSET2, "NetrServerPasswordSet2",
      netlogon_dissect_netrserverpasswordset2_rqst,
      netlogon_dissect_netrserverpasswordset2_reply },
    { NETLOGON_NETRSERVERPASSWORDGET, "NetrServerPasswordGet",
      netlogon_dissect_netrserverpasswordget_rqst,
      netlogon_dissect_netrserverpasswordget_reply },
    { NETLOGON_NETRLOGONSENDTOSAM, "NetrLogonSendToSam",
      netlogon_dissect_netrlogonsendtosam_rqst,
      netlogon_dissect_netrlogonsendtosam_reply },
    { NETLOGON_DSRADDRESSTOSITENAMESW, "DsrAddressToSiteNamesW",
      netlogon_dissect_dsraddresstositenamesw_rqst,
      netlogon_dissect_dsraddresstositenamesw_reply },
    { NETLOGON_DSRGETDCNAMEEX2, "DsrGetDcNameEx2",
      netlogon_dissect_dsrgetdcnameex2_rqst,
      netlogon_dissect_dsrgetdcnameex2_reply },
    { NETLOGON_NETRLOGONGETTIMESERVICEPARENTDOMAIN,
      "NetrLogonGetTimeServiceParentDomain",
      netlogon_dissect_netrlogongettimeserviceparentdomain_rqst,
      netlogon_dissect_netrlogongettimeserviceparentdomain_reply },
    { NETLOGON_NETRENUMERATETRUSTEDDOMAINSEX, "NetrEnumerateTrustedDomainsEx",
      netlogon_dissect_netrenumeratetrusteddomainsex_rqst,
      netlogon_dissect_netrenumeratetrusteddomainsex_reply },
    { NETLOGON_DSRADDRESSTOSITENAMESEXW, "DsrAddressToSiteNamesExW",
      netlogon_dissect_dsraddresstositenamesexw_rqst,
      netlogon_dissect_dsraddresstositenamesexw_reply },
    { NETLOGON_DSRGETDCSITECOVERAGEW, "DsrGetDcSiteCoverageW",
      netlogon_dissect_dsrgetdcsitecoveragew_rqst,
      netlogon_dissect_dsrgetdcsitecoveragew_reply },
    { NETLOGON_NETRLOGONSAMLOGONEX, "NetrLogonSamLogonEx",
      netlogon_dissect_netrlogonsamlogonex_rqst,
      netlogon_dissect_netrlogonsamlogonex_reply },
    { NETLOGON_DSRENUMERATEDOMAINTRUSTS, "DsrEnumerateDomainTrusts",
      netlogon_dissect_dsrenumeratedomaintrusts_rqst,
      netlogon_dissect_dsrenumeratedomaintrusts_reply },
    { NETLOGON_DSRDEREGISTERDNSHOSTRECORDS, "DsrDeregisterDnsHostRecords",
      netlogon_dissect_dsrderegisterdnshostrecords_rqst,
      netlogon_dissect_dsrderegisterdnshostrecords_reply },
    { NETLOGON_NETRSERVERTRUSTPASSWORDSGET, "NetrServerTrustPasswordsGet",
      netlogon_dissect_netrservertrustpasswordsget_rqst,
      netlogon_dissect_netrservertrustpasswordsget_reply },
    { NETLOGON_DSRGETFORESTTRUSTINFORMATION, "DsrGetForestTrustInformation",
      netlogon_dissect_dsrgetforesttrustinformation_rqst,
      netlogon_dissect_dsrgetforesttrustinformation_reply },
    { NETLOGON_NETRGETFORESTTRUSTINFORMATION, "NetrGetForestTrustInformation",
      netlogon_dissect_netrgetforesttrustinformation_rqst,
      netlogon_dissect_netrgetforesttrustinformation_reply },
    { NETLOGON_NETRLOGONSAMLOGONWITHFLAGS, "NetrLogonSamLogonWithFlags",
      netlogon_dissect_netrlogonsamlogonflags_rqst,
      netlogon_dissect_netrlogonsamlogonflags_reply },
    { NETLOGON_NETRSERVERGETTRUSTINFO, "NetrServerGetTrustInfo",
      netlogon_dissect_netrservergettrustinfo_rqst,
      netlogon_dissect_netrservergettrustinfo_reply },
    { NETLOGON_DSRUPDATEREADONLYSERVERDNSRECORDS, "DsrUpdateReadOnlyServerDnsRecords",
      NULL, NULL },
    { NETLOGON_NETRCHAINSETCLIENTATTRIBUTES, "NetrChainSetClientAttributes",
      netlogon_dissect_netrchainsetclientattributes_rqst,
      netlogon_dissect_netrchainsetclientattributes_reply },
    { NETLOGON_NETRSERVERAUTHENTICATEKERBEROS, "NetrServerAuthenticateKerberos",
      netlogon_dissect_netrserverauthenticatekerberos_rqst,
      netlogon_dissect_netrserverauthenticatekerberos_reply },
    {0, NULL, NULL,  NULL }
};

static int hf_netlogon_secchan_verf;
static int hf_netlogon_secchan_verf_signalg;
static int hf_netlogon_secchan_verf_sealalg;
static int hf_netlogon_secchan_verf_flag;
static int hf_netlogon_secchan_verf_digest;
static int hf_netlogon_secchan_verf_seq;
static int hf_netlogon_secchan_verf_nonce;

static const value_string sign_algs[] = {
    { 0x0077, "HMAC-MD5"},
    { 0x0013, "HMAC-SHA256"},
    { 0, NULL}
};

static const value_string seal_algs[] = {
    { 0xFFFF, "Not Encrypted"},
    { 0x007A, "RC4"},
    { 0x001A, "AES-128"},
    { 0, NULL}
};

static int get_seal_key(const uint8_t *session_key,int key_len,uint8_t* seal_key)
{
    uint8_t zero_sk[16] = { 0 };

    memset(seal_key,0,16);
    if(memcmp(session_key,zero_sk,16)) {
        for(int i=0;i<key_len;i++) {
            seal_key[i] = session_key[i] ^ 0xF0;
        }
        return 1;
    } else {
        return 0;
    }

}

static uint64_t uncrypt_sequence_aes(uint8_t* session_key,uint64_t checksum,uint64_t enc_seq,unsigned char is_server _U_)
{
    gcry_error_t err;
    gcry_cipher_hd_t cipher_hd = NULL;
    uint8_t iv[16] = { 0 };

    memcpy(&iv[0], (uint8_t*)&checksum, 8);
    memcpy(&iv[8], (uint8_t*)&checksum, 8);

    /* Open the cipher */
    err = gcry_cipher_open(&cipher_hd, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CFB8, 0);
    if (err != 0) {
        ws_warning("GCRY: cipher open %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        return 0;
    }

    /* Set the initial value */
    err = gcry_cipher_setiv(cipher_hd, iv, sizeof(iv));
    if (err != 0) {
        ws_warning("GCRY: setiv %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return 0;
    }

    /* Set the key */
    err = gcry_cipher_setkey(cipher_hd, session_key, 16);
    if (err != 0) {
        ws_warning("GCRY: setkey %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return 0;
    }

    err = gcry_cipher_decrypt(cipher_hd, (uint8_t*) &enc_seq, 8, NULL, 0);
    if (err != 0) {
        ws_warning("GCRY: encrypt %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return 0;
    }
    /* Done with the cipher */
    gcry_cipher_close(cipher_hd);
    return enc_seq;
}

static uint64_t uncrypt_sequence_md5(uint8_t* session_key,uint64_t checksum,uint64_t enc_seq,unsigned char is_server _U_)
{
    uint8_t zeros[4] = { 0 };
    uint8_t buf[HASH_MD5_LENGTH];
    uint8_t key[HASH_MD5_LENGTH];
    gcry_cipher_hd_t rc4_handle;
    uint8_t *p_seq = (uint8_t*) &enc_seq;
    /*uint32_t temp;*/

    if (ws_hmac_buffer(GCRY_MD_MD5, buf, zeros, 4, session_key, 16)) {
        return 0;
    }

    if (ws_hmac_buffer(GCRY_MD_MD5, key, (uint8_t*)&checksum, 8, buf, HASH_MD5_LENGTH)) {
        return 0;
    }

    if (!gcry_cipher_open (&rc4_handle, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0)) {
      if (!gcry_cipher_setkey(rc4_handle, key, HASH_MD5_LENGTH)) {
        gcry_cipher_decrypt(rc4_handle, p_seq, 8, NULL, 0);
      }
      gcry_cipher_close(rc4_handle);
    }
    /*temp = *((uint32_t*)p_seq);
     *((uint32_t*)p_seq) = *((uint32_t*)p_seq+1);
     *((uint32_t*)p_seq+1) = temp;

     if(!is_server) {
     *p_seq = *p_seq & 0x7F;
     }
    */
    return enc_seq;
}

static uint64_t uncrypt_sequence(uint32_t flags, uint8_t* session_key,uint64_t checksum,uint64_t enc_seq,unsigned char is_server _U_)
{
    if (flags & NETLOGON_FLAG_AES) {
        return uncrypt_sequence_aes(session_key, checksum, enc_seq, is_server);
    }

    return uncrypt_sequence_md5(session_key, checksum, enc_seq, is_server);
}

static gcry_error_t prepare_decryption_cipher_aes(netlogon_auth_vars *vars,
                                                  gcry_cipher_hd_t *_cipher_hd)
{
    gcry_error_t err;
    gcry_cipher_hd_t cipher_hd = NULL;
    uint64_t sequence = vars->seq;

    uint8_t iv[16] = { 0 };

    memcpy(&iv[0], (uint8_t*)&sequence, 8);
    memcpy(&iv[8], (uint8_t*)&sequence, 8);

    /* Open the cipher */
    err = gcry_cipher_open(&cipher_hd, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CFB8, 0);
    if (err != 0) {
        ws_warning("GCRY: cipher open %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        return 0;
    }

    /* Set the initial value */
    err = gcry_cipher_setiv(cipher_hd, iv, sizeof(iv));
    if (err != 0) {
        ws_warning("GCRY: setiv %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return 0;
    }

    /* Set the key */
    err = gcry_cipher_setkey(cipher_hd, vars->encryption_key, 16);
    if (err != 0) {
        ws_warning("GCRY: setkey %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return 0;
    }

    *_cipher_hd = cipher_hd;
    return 0;
}

static gcry_error_t prepare_decryption_cipher_md5(netlogon_auth_vars *vars,
                                                  gcry_cipher_hd_t *_cipher_hd)
{
    gcry_error_t err;
    gcry_cipher_hd_t cipher_hd = NULL;
    uint8_t zeros[4] = { 0 };
    uint64_t sequence = vars->seq;
    uint8_t tmp[HASH_MD5_LENGTH] = { 0 };
    uint8_t seal_key[16] = { 0 };

    err = ws_hmac_buffer(GCRY_MD_MD5, tmp, zeros, 4, vars->encryption_key, 16);
    if (err != 0) {
        ws_warning("GCRY: GCRY_MD_MD5 %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        return err;
    }
    err = ws_hmac_buffer(GCRY_MD_MD5, seal_key, (uint8_t*)&sequence, 8, tmp, HASH_MD5_LENGTH);
    if (err != 0) {
        ws_warning("GCRY: GCRY_MD_MD5 %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        return err;
    }

    /* Open the cipher */
    err = gcry_cipher_open(&cipher_hd, GCRY_CIPHER_ARCFOUR, GCRY_CIPHER_MODE_STREAM, 0);
    if (err != 0) {
        ws_warning("GCRY: cipher open %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        return err;
    }

    /* Set the key */
    err = gcry_cipher_setkey(cipher_hd, seal_key, 16);
    if (err != 0) {
        ws_warning("GCRY: setkey %s/%s\n", gcry_strsource(err), gcry_strerror(err));
        gcry_cipher_close(cipher_hd);
        return err;
    }

    *_cipher_hd = cipher_hd;
    return 0;
}

static gcry_error_t prepare_decryption_cipher(netlogon_auth_vars *vars,
                                              gcry_cipher_hd_t *_cipher_hd)
{
    *_cipher_hd = NULL;

    if (vars->flags & NETLOGON_FLAG_AES) {
        return prepare_decryption_cipher_aes(vars, _cipher_hd);
    }

    return prepare_decryption_cipher_md5(vars, _cipher_hd);
}

static tvbuff_t *
dissect_packet_data(tvbuff_t *tvb ,tvbuff_t *auth_tvb _U_,
                    int offset , packet_info *pinfo ,dcerpc_auth_info *auth_info _U_,unsigned char is_server)
{

    tvbuff_t  *buf = NULL;
    uint8_t* decrypted;
    netlogon_auth_vars *vars;
    /*ws_debug("Dissection of request data offset %d len=%d on packet %d",offset,tvb_length_remaining(tvb,offset),pinfo->num);*/

    vars = find_or_create_schannel_netlogon_auth_vars(pinfo, auth_info, is_server);
    if (vars == NULL) {
        ws_debug("Vars not found  %d (packet_data)",wmem_map_size(netlogon_auths));
        return(buf);
    }

    if (vars->can_decrypt == true) {
        gcry_error_t err;
        gcry_cipher_hd_t cipher_hd = NULL;
        int data_len;
        uint64_t copyconfounder = vars->confounder;

        data_len = tvb_captured_length_remaining(tvb,offset);
        if (data_len < 0) {
            return NULL;
        }
        err = prepare_decryption_cipher(vars, &cipher_hd);
        if (err != 0) {
            ws_warning("GCRY: prepare_decryption_cipher %s/%s",
                      gcry_strsource(err), gcry_strerror(err));
            return NULL;
        }
        gcry_cipher_decrypt(cipher_hd, (uint8_t*)&copyconfounder, 8, NULL, 0);
        decrypted = (uint8_t*)tvb_memdup(pinfo->pool, tvb, offset,data_len);
        if (!(vars->flags & NETLOGON_FLAG_AES)) {
            gcry_cipher_reset(cipher_hd);
        }
        gcry_cipher_decrypt(cipher_hd, decrypted, data_len, NULL, 0);
        gcry_cipher_close(cipher_hd);
        buf = tvb_new_child_real_data(tvb, decrypted, data_len, data_len);
        /* Note: caller does add_new_data_source(...) */
    } else {
        ws_debug("Session key not found can't decrypt ...");
    }

    return(buf);
}

static tvbuff_t* dissect_request_data(tvbuff_t *header_tvb _U_,
                                      tvbuff_t *payload_tvb,
                                      tvbuff_t *trailer_tvb _U_,
                                      tvbuff_t *auth_tvb,
                                      packet_info *pinfo,
                                      dcerpc_auth_info *auth_info)
{
    return dissect_packet_data(payload_tvb,auth_tvb,0,pinfo,auth_info,0);
}

static tvbuff_t* dissect_response_data(tvbuff_t *header_tvb _U_,
                                       tvbuff_t *payload_tvb,
                                       tvbuff_t *trailer_tvb _U_,
                                       tvbuff_t *auth_tvb,
                                       packet_info *pinfo,
                                       dcerpc_auth_info *auth_info)
{
    return dissect_packet_data(payload_tvb,auth_tvb,0,pinfo,auth_info,1);
}

/* MS-NRPC 2.2.1.3.2 */
static int
dissect_secchan_verf(tvbuff_t *tvb, int offset, packet_info *pinfo,
                     proto_tree *tree, uint8_t *drep,
                     dcerpc_auth_info *auth_info,
                     unsigned char is_server)
{
    netlogon_auth_vars *vars;
    proto_item *vf = NULL;
    proto_tree *subtree = NULL;
    uint64_t encrypted_seq;
    uint64_t digest = 0;
    uint64_t confounder = 0;
    int update_vars = 0;

    if(  ! (seen.isseen && seen.num == pinfo->num) ) {
        /*
         * Create a new tree, and split into x components ...
         */
        vf = proto_tree_add_item(tree, hf_netlogon_secchan_verf, tvb,
                                 offset, -1, ENC_NA);
        subtree = proto_item_add_subtree(vf, ett_secchan_verf);

        proto_tree_add_item(subtree, hf_netlogon_secchan_verf_signalg, tvb,
                            offset, 2, ENC_LITTLE_ENDIAN);
        proto_tree_add_item(subtree, hf_netlogon_secchan_verf_sealalg, tvb,
                            offset+2, 2, ENC_LITTLE_ENDIAN);
        /* 2 pad bytes */
        proto_tree_add_item(subtree, hf_netlogon_secchan_verf_flag, tvb,
                            offset+6, 2, ENC_NA);
        offset += 8;

        offset = dissect_dcerpc_8bytes(tvb, offset, pinfo, subtree, drep,
                                       hf_netlogon_secchan_verf_seq, &encrypted_seq);

        offset = dissect_dcerpc_8bytes(tvb, offset, pinfo, subtree, drep,
                                       hf_netlogon_secchan_verf_digest, &digest);

        /* In some cases the nonce if the data/signature are encrypted ("integrity/seal  in MS language")*/

        if (tvb_bytes_exist(tvb, offset, 8)) {
            offset = dissect_dcerpc_8bytes(tvb, offset, pinfo, subtree, drep,
                                           hf_netlogon_secchan_verf_nonce, &confounder);
        }
        update_vars = 1;
    }

    /*ws_debug("Setting isseen to true, old packet %d new %d",seen.num,pinfo->num);*/
    seen.isseen = true;
    seen.num = pinfo->num;

    vars = find_or_create_schannel_netlogon_auth_vars(pinfo, auth_info, is_server);
    if (vars == NULL) {
        ws_debug("Vars not found %d (packet_data)",wmem_map_size(netlogon_auths));
        return(offset);
    }
    if(update_vars) {
        vars->confounder = confounder;
        vars->seq = uncrypt_sequence(vars->flags,vars->session_key,digest,encrypted_seq,is_server);
    }

    if(get_seal_key(vars->session_key,16,vars->encryption_key))
    {
        vars->can_decrypt = true;
    }
    else
    {
        ws_debug("get seal key returned 0");
    }

    if (vars->can_decrypt) {
        expert_add_info_format(pinfo, proto_tree_get_parent(subtree),
                 &ei_netlogon_session_key,
                 "Using session key learned in frame %d ("
                 "%02x%02x%02x%02x"
                 ") from %s",
                 vars->auth_fd_num,
                 vars->session_key[0] & 0xFF,  vars->session_key[1] & 0xFF,
                 vars->session_key[2] & 0xFF,  vars->session_key[3] & 0xFF,
                 vars->nthash.key_origin);
    }

    return offset;
}
static int
dissect_request_secchan_verf(tvbuff_t *tvb, int offset, packet_info *pinfo ,
                             proto_tree *tree, dcerpc_info *di _U_, uint8_t *drep )
{
    return dissect_secchan_verf(tvb,offset,pinfo,tree,drep, di->auth_info, 0);
}
static int
dissect_response_secchan_verf(tvbuff_t *tvb, int offset, packet_info *pinfo ,
                              proto_tree *tree, dcerpc_info *di _U_, uint8_t *drep )
{
    return dissect_secchan_verf(tvb,offset,pinfo,tree,drep, di->auth_info, 1);
}

void
proto_register_dcerpc_netlogon(void)
{

    static hf_register_info hf[] = {
        { &hf_netlogon_opnum,
          { "Operation", "netlogon.opnum", FT_UINT16, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_rc, {
                "Return code", "netlogon.rc", FT_UINT32, BASE_HEX | BASE_EXT_STRING,
                &NT_errors_ext, 0x0, "Netlogon return code", HFILL }},

        { &hf_netlogon_dos_rc,
          { "DOS error code", "netlogon.dos.rc", FT_UINT32,
            BASE_HEX | BASE_EXT_STRING, &DOS_errors_ext, 0x0, NULL, HFILL}},

        { &hf_netlogon_werr_rc,
          { "WERR error code", "netlogon.werr.rc", FT_UINT32,
            BASE_HEX | BASE_EXT_STRING, &WERR_errors_ext, 0x0, NULL, HFILL}},

        { &hf_netlogon_param_ctrl, {
                "Param Ctrl", "netlogon.param_ctrl", FT_UINT32, BASE_HEX,
                NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_logon_id, {
                "Logon ID", "netlogon.logon_id", FT_UINT64, BASE_DEC,
                NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_modify_count, {
                "Modify Count", "netlogon.modify_count", FT_UINT64, BASE_DEC,
                NULL, 0x0, "How many times the object has been modified", HFILL }},

        { &hf_netlogon_security_information, {
                "Security Information", "netlogon.security_information", FT_UINT32, BASE_DEC,
                NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_count, {
                "Count", "netlogon.count", FT_UINT32, BASE_DEC,
                NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_entries, {
                "Entries", "netlogon.entries", FT_UINT32, BASE_DEC,
                NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_credential, {
                "Credential", "netlogon.credential", FT_BYTES, BASE_NONE,
                NULL, 0x0, "Netlogon Credential", HFILL }},

        { &hf_netlogon_challenge, {
                "Challenge", "netlogon.challenge", FT_BYTES, BASE_NONE,
                NULL, 0x0, "Netlogon challenge", HFILL }},

        { &hf_netlogon_lm_owf_password, {
                "LM Pwd", "netlogon.lm_owf_pwd", FT_BYTES, BASE_NONE,
                NULL, 0x0, "LanManager OWF Password", HFILL }},

        { &hf_netlogon_user_session_key, {
                "User Session Key", "netlogon.user_session_key", FT_BYTES, BASE_NONE,
                NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_encrypted_lm_owf_password, {
                "Encrypted LM Pwd", "netlogon.lm_owf_pwd.encrypted", FT_BYTES, BASE_NONE,
                NULL, 0x0, "Encrypted LanManager OWF Password", HFILL }},

        { &hf_netlogon_nt_owf_password, {
                "NT Pwd", "netlogon.nt_owf_pwd", FT_BYTES, BASE_NONE,
                NULL, 0x0, "NT OWF Password", HFILL }},

        { &hf_netlogon_blob, {
                "BLOB", "netlogon.blob", FT_BYTES, BASE_NONE,
                NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_len, {
                "Len", "netlogon.len", FT_UINT32, BASE_DEC,
                NULL, 0, "Length", HFILL }},

        { &hf_netlogon_password_version_reserved, {
                "ReservedField", "netlogon.password_version.reservedfield", FT_UINT32, BASE_HEX,
                NULL, 0, "ReservedField zero", HFILL }},

        { &hf_netlogon_password_version_number, {
                "PasswordVersionNumber", "netlogon.password_version.number", FT_UINT32, BASE_HEX,
                NULL, 0, "PasswordVersionNumber trust", HFILL }},

        { &hf_netlogon_password_version_present, {
                "PasswordVersionPresent", "netlogon.password_version.present", FT_UINT32, BASE_HEX,
                NULL, 0, "PasswordVersionPresent magic", HFILL }},

        { &hf_netlogon_priv, {
                "Priv", "netlogon.priv", FT_UINT32, BASE_DEC,
                NULL, 0, NULL, HFILL }},

        { &hf_netlogon_privilege_entries, {
                "Privilege Entries", "netlogon.privilege_entries", FT_UINT32, BASE_DEC,
                NULL, 0, NULL, HFILL }},

        { &hf_netlogon_privilege_control, {
                "Privilege Control", "netlogon.privilege_control", FT_UINT32, BASE_HEX,
                NULL, 0, NULL, HFILL }},

        { &hf_netlogon_privilege_name, {
                "Privilege Name", "netlogon.privilege_name", FT_STRING, BASE_NONE,
                NULL, 0, NULL, HFILL }},

        { &hf_netlogon_pdc_connection_status, {
                "PDC Connection Status", "netlogon.pdc_connection_status", FT_UINT32, BASE_DEC,
                NULL, 0, NULL, HFILL }},

        { &hf_netlogon_tc_connection_status, {
                "TC Connection Status", "netlogon.tc_connection_status", FT_UINT32, BASE_DEC,
                NULL, 0, NULL, HFILL }},

        { &hf_netlogon_attrs, {
                "Attributes", "netlogon.attrs", FT_UINT32, BASE_HEX,
                NULL, 0, NULL, HFILL }},

#if 0
        { &hf_netlogon_lsapolicy_referentid,
          { "Referent ID", "netlogon.lsapolicy.referentID", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},
#endif

        { &hf_netlogon_lsapolicy_len,
          { "Length", "netlogon.lsapolicy.length", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Length of the policy buffer", HFILL }},

#if 0
        { &hf_netlogon_lsapolicy_pointer,
          { "Pointer", "netlogon.lsapolicy.pointer", FT_BYTES, BASE_NONE,
            NULL, 0x0, "Pointer to LSA POLICY", HFILL }},
#endif

        { &hf_netlogon_unknown_string,
          { "Unknown string", "netlogon.unknown_string", FT_STRING, BASE_NONE,
            NULL, 0, "Unknown string. If you know what this is, contact wireshark developers.", HFILL }},

        { &hf_netlogon_new_password,
          { "New Password", "netlogon.new_password", FT_STRING, BASE_NONE,
            NULL, 0, "New Password for Computer or Trust", HFILL }},

        { &hf_netlogon_TrustedDomainName_string,
          { "TrustedDomainName", "netlogon.TrustedDomainName", FT_STRING, BASE_NONE,
            NULL, 0, "TrustedDomainName string.", HFILL }},

        { &hf_netlogon_UserName_string,
          { "UserName", "netlogon.UserName", FT_STRING, BASE_NONE,
            NULL, 0, "UserName string.", HFILL }},

        { &hf_netlogon_dummy_string,
          { "Dummy String", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_trust_extension,
          { "Trust extension", "netlogon.trust.extension", FT_STRING, BASE_NONE,
            NULL, 0, "Trusts extension.", HFILL }},

        { &hf_netlogon_trust_offset,
          { "Offset", "netlogon.trust.extension_offset", FT_UINT32, BASE_DEC,
            NULL, 0, "Trusts extension.", HFILL }},

        { &hf_netlogon_trust_len,
          { "Length", "netlogon.trust.extension_length", FT_UINT32, BASE_DEC,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_trust_max,
          { "Max Count", "netlogon.trust.extension.maxcount", FT_UINT32, BASE_DEC,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_opaque_buffer_enc,
          { "Encrypted", "netlogon.sendtosam.opaquebuffer.enc", FT_BYTES, BASE_NONE,
            NULL, 0x0, "OpaqueBuffer (Encrypted)", HFILL }},

        { &hf_netlogon_opaque_buffer_dec,
          { "Decrypted", "netlogon.sendtosam.opaquebuffer.dec", FT_BYTES, BASE_NONE,
            NULL, 0x0, "OpaqueBuffer (Decrypted)", HFILL }},

        { &hf_netlogon_opaque_buffer_size,
          { "OpaqueBufferSize", "netlogon.sendtosam.opaquebuffer.size", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Size of the OpaqueBuffer", HFILL }},

        { &hf_netlogon_dummy_string2,
          { "Dummy String2", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 2. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string3,
          { "Dummy String3", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 3. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string4,
          { "Dummy String4", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 4. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string5,
          { "Dummy String5", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 5. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string6,
          { "Dummy String6", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 6. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string7,
          { "Dummy String7", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 7. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string8,
          { "Dummy String8", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 8. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string9,
          { "Dummy String9", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 9. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy_string10,
          { "Dummy String10", "netlogon.dummy_string", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy String 10. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_unknown_long,
          { "Unknown long", "netlogon.unknown.long", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Unknown long. If you know what this is, contact wireshark developers.", HFILL }},

        { &hf_netlogon_dummy1_long,
          { "Dummy1 Long", "netlogon.dummy.long1", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 1. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy2_long,
          { "Dummy2 Long", "netlogon.dummy.long2", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 2. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy3_long,
          { "Dummy3 Long", "netlogon.dummy.long3", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 3. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy4_long,
          { "Dummy4 Long", "netlogon.dummy.long4", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 4. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy5_long,
          { "Dummy5 Long", "netlogon.dummy.long5", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 5. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy6_long,
          { "Dummy6 Long", "netlogon.dummy.long6", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 6. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy7_long,
          { "Dummy7 Long", "netlogon.dummy.long7", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 7. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy8_long,
          { "Dummy8 Long", "netlogon.dummy.long8", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 8. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy9_long,
          { "Dummy9 Long", "netlogon.dummy.long9", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 9. Used is reserved for next evolutions.", HFILL }},

        { &hf_netlogon_dummy10_long,
          { "Dummy10 Long", "netlogon.dummy.long10", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Dummy long 10. Used is reserved for next evolutions.", HFILL }},


        { &hf_netlogon_supportedenctypes,
          { "Supported Encryption Types", "netlogon.encryption.types", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_workstation_flags,
          { "Workstation Flags", "netlogon.workstation.flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_reserved,
          { "Reserved", "netlogon.reserved", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_unknown_short,
          { "Unknown short", "netlogon.unknown.short", FT_UINT16, BASE_HEX,
            NULL, 0x0, "Unknown short. If you know what this is, contact wireshark developers.", HFILL }},

        { &hf_netlogon_unknown_char,
          { "Unknown char", "netlogon.unknown.char", FT_UINT8, BASE_HEX,
            NULL, 0x0, "Unknown char. If you know what this is, contact wireshark developers.", HFILL }},

        { &hf_netlogon_acct_expiry_time,
          { "Acct Expiry Time", "netlogon.acct.expiry_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0x0, "When this account will expire", HFILL }},

        { &hf_netlogon_nt_pwd_present,
          { "NT PWD Present", "netlogon.nt_pwd_present", FT_UINT8, BASE_HEX,
            NULL, 0x0, "Is NT password present for this account?", HFILL }},

        { &hf_netlogon_lm_pwd_present,
          { "LM PWD Present", "netlogon.lm_pwd_present", FT_UINT8, BASE_HEX,
            NULL, 0x0, "Is LanManager password present for this account?", HFILL }},

        { &hf_netlogon_pwd_expired,
          { "PWD Expired", "netlogon.pwd_expired", FT_UINT8, BASE_HEX,
            NULL, 0x0, "Whether this password has expired or not", HFILL }},

        { &hf_netlogon_authoritative,
          { "Authoritative", "netlogon.authoritative", FT_UINT8, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_sensitive_data_flag,
          { "Sensitive Data", "netlogon.sensitive_data_flag", FT_UINT8, BASE_DEC,
            NULL, 0x0, "Sensitive data flag", HFILL }},

        { &hf_netlogon_auditing_mode,
          { "Auditing Mode", "netlogon.auditing_mode", FT_UINT8, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_max_audit_event_count,
          { "Max Audit Event Count", "netlogon.max_audit_event_count", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_event_audit_option,
          { "Event Audit Option", "netlogon.event_audit_option", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_sensitive_data_len,
          { "Length", "netlogon.sensitive_data_len", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Length of sensitive data", HFILL }},

        { &hf_netlogon_nt_chal_resp,
          { "NT Chal resp", "netlogon.nt_chal_resp", FT_BYTES, BASE_NONE,
            NULL, 0, "Challenge response for NT authentication", HFILL }},

        { &hf_netlogon_lm_chal_resp,
          { "LM Chal resp", "netlogon.lm_chal_resp", FT_BYTES, BASE_NONE,
            NULL, 0, "Challenge response for LM authentication", HFILL }},

        { &hf_netlogon_cipher_len,
          { "Cipher Len", "netlogon.cipher_len", FT_UINT32, BASE_DEC,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_cipher_maxlen,
          { "Cipher Max Len", "netlogon.cipher_maxlen", FT_UINT32, BASE_DEC,
            NULL, 0, NULL, HFILL }},

#if 0
        { &hf_netlogon_pac_data,
          { "Pac Data", "netlogon.pac.data", FT_BYTES, BASE_NONE,
            NULL, 0, NULL, HFILL }},
#endif

        { &hf_netlogon_sensitive_data,
          { "Data", "netlogon.sensitive_data", FT_BYTES, BASE_NONE,
            NULL, 0, "Sensitive Data", HFILL }},

#if 0
        { &hf_netlogon_auth_data,
          { "Auth Data", "netlogon.auth.data", FT_BYTES, BASE_NONE,
            NULL, 0, NULL, HFILL }},
#endif

        { &hf_netlogon_cipher_current_data,
          { "Cipher Current Data", "netlogon.cipher_current_data", FT_BYTES, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_cipher_old_data,
          { "Cipher Old Data", "netlogon.cipher_old_data", FT_BYTES, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_acct_name,
          { "Acct Name", "netlogon.acct_name", FT_STRING, BASE_NONE,
            NULL, 0, "Account Name", HFILL }},

        { &hf_netlogon_acct_desc,
          { "Acct Desc", "netlogon.acct_desc", FT_STRING, BASE_NONE,
            NULL, 0, "Account Description", HFILL }},

        { &hf_netlogon_group_desc,
          { "Group Desc", "netlogon.group_desc", FT_STRING, BASE_NONE,
            NULL, 0, "Group Description", HFILL }},

        { &hf_netlogon_full_name,
          { "Full Name", "netlogon.full_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_comment,
          { "Comment", "netlogon.comment", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_parameters,
          { "Parameters", "netlogon.parameters", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_logon_script,
          { "Logon Script", "netlogon.logon_script", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_profile_path,
          { "Profile Path", "netlogon.profile_path", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_home_dir,
          { "Home Dir", "netlogon.home_dir", FT_STRING, BASE_NONE,
            NULL, 0, "Home Directory", HFILL }},

        { &hf_netlogon_dir_drive,
          { "Dir Drive", "netlogon.dir_drive", FT_STRING, BASE_NONE,
            NULL, 0, "Drive letter for home directory", HFILL }},

        { &hf_netlogon_logon_srv,
          { "Server", "netlogon.server", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

#if 0
        { &hf_netlogon_principal,
          { "Principal", "netlogon.principal", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},
#endif

        { &hf_netlogon_logon_dom,
          { "Domain", "netlogon.domain", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_resourcegroupcount,
          { "ResourceGroup count", "netlogon.resourcegroupcount", FT_UINT32, BASE_DEC,
            NULL, 0, "Number of Resource Groups", HFILL }},

        { &hf_netlogon_accountdomaingroupcount,
          { "AccountDomainGroup count", "netlogon.accountdomaingroupcount", FT_UINT32, BASE_DEC,
            NULL, 0, "Number of Account Domain Groups", HFILL }},

        { &hf_netlogon_domaingroupcount,
          { "DomainGroup count", "netlogon.domaingroupcount", FT_UINT32, BASE_DEC,
            NULL, 0, "Number of Domain Groups", HFILL }},

        { &hf_netlogon_membership_domains_count,
          { "Membership Domains count", "netlogon.membershipsdomainscount", FT_UINT32, BASE_DEC,
            NULL, 0, "Number of ExtraDomain Membership Arrays", HFILL }},

        { &hf_netlogon_computer_name,
          { "Computer Name", "netlogon.computer_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_site_name,
          { "Site Name", "netlogon.site_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_dc_name,
          { "DC Name", "netlogon.dc.name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_dc_site_name,
          { "DC Site Name", "netlogon.dc.site_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_dns_forest_name,
          { "DNS Forest Name", "netlogon.dns.forest_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_dc_address,
          { "DC Address", "netlogon.dc.address", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_dc_address_type,
          { "DC Address Type", "netlogon.dc.address_type", FT_UINT32, BASE_DEC,
            VALS(dc_address_types), 0, NULL, HFILL }},

        { &hf_netlogon_client_site_name,
          { "Client Site Name", "netlogon.client.site_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_workstation_site_name,
          { "Wkst Site Name", "netlogon.wkst.site_name", FT_STRING, BASE_NONE,
            NULL, 0, "Workstation Site Name", HFILL }},

        { &hf_netlogon_workstation,
          { "Wkst Name", "netlogon.wkst.name", FT_STRING, BASE_NONE,
            NULL, 0, "Workstation Name", HFILL }},

        { &hf_netlogon_os_version,
          { "OS version", "netlogon.os.version", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_workstation_os,
          { "Wkst OS", "netlogon.wkst.os", FT_STRING, BASE_NONE,
            NULL, 0, "Workstation OS", HFILL }},

        { &hf_netlogon_workstations,
          { "Workstations", "netlogon.wksts", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_workstation_fqdn,
          { "Wkst FQDN", "netlogon.wkst.fqdn", FT_STRING, BASE_NONE,
            NULL, 0, "Workstation FQDN", HFILL }},

        { &hf_netlogon_group_name,
          { "Group Name", "netlogon.group_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_alias_name,
          { "Alias Name", "netlogon.alias_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_dns_host,
          { "DNS Host", "netlogon.dns_host", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_downlevel_domain_name,
          { "Downlevel Domain", "netlogon.downlevel_domain", FT_STRING, BASE_NONE,
            NULL, 0, "Downlevel Domain Name", HFILL }},

        { &hf_netlogon_dns_domain_name,
          { "DNS Domain", "netlogon.dns_domain", FT_STRING, BASE_NONE,
            NULL, 0, "DNS Domain Name", HFILL }},

        { &hf_netlogon_ad_client_dns_name,
          { "Client DNS Name", "netlogon.client_dns_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_domain_name,
          { "Domain", "netlogon.domain", FT_STRING, BASE_NONE,
            NULL, 0, "Domain Name", HFILL }},

        { &hf_netlogon_oem_info,
          { "OEM Info", "netlogon.oem_info", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_trusted_dc_name,
          { "Trusted DC", "netlogon.trusted_dc", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_logon_dnslogondomainname,
          { "DNS Logon Domain name", "netlogon.logon.dnslogondomainname", FT_STRING, BASE_NONE,
            NULL, 0, "DNS Name of the logon domain", HFILL }},

        { &hf_netlogon_logon_upn,
          { "UPN", "netlogon.logon.upn", FT_STRING, BASE_NONE,
            NULL, 0, "User Principal Name", HFILL }},

        { &hf_netlogon_logonsrv_handle,
          { "Handle", "netlogon.handle", FT_STRING, BASE_NONE,
            NULL, 0, "Logon Srv Handle", HFILL }},

        { &hf_netlogon_dummy,
          { "Dummy", "netlogon.dummy", FT_STRING, BASE_NONE,
            NULL, 0, "Dummy string", HFILL }},

        { &hf_netlogon_logon_count16,
          { "Logon Count", "netlogon.logon_count16", FT_UINT16, BASE_DEC,
            NULL, 0x0, "Number of successful logins", HFILL }},

        { &hf_netlogon_logon_count,
          { "Logon Count", "netlogon.logon_count", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Number of successful logins", HFILL }},

        { &hf_netlogon_bad_pw_count16,
          { "Bad PW Count", "netlogon.bad_pw_count16", FT_UINT16, BASE_DEC,
            NULL, 0x0, "Number of failed logins", HFILL }},

        { &hf_netlogon_bad_pw_count,
          { "Bad PW Count", "netlogon.bad_pw_count", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Number of failed logins", HFILL }},

        { &hf_netlogon_country,
          { "Country", "netlogon.country", FT_UINT16, BASE_DEC | BASE_EXT_STRING,
            &ms_country_codes_ext, 0x0, "Country setting for this account", HFILL }},

        { &hf_netlogon_codepage,
          { "Codepage", "netlogon.codepage", FT_UINT16, BASE_DEC,
            NULL, 0x0, "Codepage setting for this account", HFILL }},

        { &hf_netlogon_level16,
          { "Level", "netlogon.level16", FT_UINT16, BASE_DEC,
            NULL, 0x0, "Which option of the union is represented here", HFILL }},

        { &hf_netlogon_validation_level,
          { "Validation Level", "netlogon.validation_level", FT_UINT16, BASE_DEC,
            NULL, 0x0, "Requested level of validation", HFILL }},

        { &hf_netlogon_minpasswdlen,
          { "Min Password Len", "netlogon.min_passwd_len", FT_UINT16, BASE_DEC,
            NULL, 0x0, "Minimum length of password", HFILL }},

        { &hf_netlogon_passwdhistorylen,
          { "Passwd History Len", "netlogon.passwd_history_len", FT_UINT16, BASE_DEC,
            NULL, 0x0, "Length of password history", HFILL }},

        { &hf_netlogon_secure_channel_type,
          { "Sec Chan Type", "netlogon.sec_chan_type", FT_UINT16, BASE_DEC,
            VALS(misc_netr_SchannelType_vals), 0x0, "Secure Channel Type", HFILL }},

        { &hf_netlogon_restart_state,
          { "Restart State", "netlogon.restart_state", FT_UINT16, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_delta_type,
          { "Delta Type", "netlogon.delta_type", FT_UINT16, BASE_DEC,
            VALS(delta_type_vals), 0x0, NULL, HFILL }},

        { &hf_netlogon_blob_size,
          { "Size", "netlogon.blob.size", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Size in bytes of BLOB", HFILL }},

        { &hf_netlogon_code,
          { "Code", "netlogon.code", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_level,
          { "Level", "netlogon.level", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Which option of the union is represented here", HFILL }},

        { &hf_netlogon_reference,
          { "Reference", "netlogon.reference", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_next_reference,
          { "Next Reference", "netlogon.next_reference", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_timestamp,
          { "Timestamp", "netlogon.timestamp", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_user_rid,
          { "User RID", "netlogon.rid", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_alias_rid,
          { "Alias RID", "netlogon.alias_rid", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_group_rid,
          { "Group RID", "netlogon.group_rid", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_num_rids,
          { "Num RIDs", "netlogon.num_rids", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Number of RIDs", HFILL }},

        { &hf_netlogon_num_controllers,
          { "Num DCs", "netlogon.num_dc", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Number of domain controllers", HFILL }},

        { &hf_netlogon_num_sid,
          { "Num Extra SID", "netlogon.num_sid", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_flags,
          { "Flags", "netlogon.flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_user_account_control,
          { "User Account Control", "netlogon.user_account_control", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_user_flags,
          { "User Flags", "netlogon.user_flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_auth_flags,
          { "Auth Flags", "netlogon.auth_flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_systemflags,
          { "System Flags", "netlogon.system_flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_database_id,
          { "Database Id", "netlogon.database_id", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_sync_context,
          { "Sync Context", "netlogon.sync_context", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_max_size,
          { "Max Size", "netlogon.max_size", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Max Size of database", HFILL }},

        { &hf_netlogon_max_log_size,
          { "Max Log Size", "netlogon.max_log_size", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Max Size of log", HFILL }},

#if 0
        { &hf_netlogon_pac_size,
          { "Pac Size", "netlogon.pac.size", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Size of PacData in bytes", HFILL }},
#endif

#if 0
        { &hf_netlogon_auth_size,
          { "Auth Size", "netlogon.auth.size", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Size of AuthData in bytes", HFILL }},
#endif

        { &hf_netlogon_num_deltas,
          { "Num Deltas", "netlogon.num_deltas", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Number of SAM Deltas in array", HFILL }},

        { &hf_netlogon_num_trusts,
          { "Num Trusts", "netlogon.num_trusts", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_logon_attempts,
          { "Logon Attempts", "netlogon.logon_attempts", FT_UINT32, BASE_DEC,
            NULL, 0x0, "Number of logon attempts", HFILL }},

        { &hf_netlogon_pagefilelimit,
          { "Page File Limit", "netlogon.page_file_limit", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_pagedpoollimit,
          { "Paged Pool Limit", "netlogon.paged_pool_limit", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_nonpagedpoollimit,
          { "Non-Paged Pool Limit", "netlogon.nonpaged_pool_limit", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_minworkingsetsize,
          { "Min Working Set Size", "netlogon.min_working_set_size", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_maxworkingsetsize,
          { "Max Working Set Size", "netlogon.max_working_set_size", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_serial_number,
          { "Serial Number", "netlogon.serial_number", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_neg_flags,
          { "Negotiation options", "netlogon.neg_flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Negotiation Flags", HFILL }},

        { &hf_netlogon_neg_flags_80000000,
          { "Supports Kerberos Auth", "ntlmssp.neg_flags.supports_kerberos_auth", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_80000000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_40000000,
          { "Authenticated RPC supported", "ntlmssp.neg_flags.na4000000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_40000000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_20000000,
          { "Authenticated RPC via lsass supported", "ntlmssp.neg_flags.na2000000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_20000000, NULL, HFILL }},

#if 0
        { &hf_netlogon_neg_flags_10000000,
          { "Not used 10000000", "ntlmssp.neg_flags.na1000000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_10000000, NULL, HFILL }},
#endif

#if 0
        { &hf_netlogon_neg_flags_8000000,
          { "Not used 8000000", "ntlmssp.neg_flags.na800000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_8000000, NULL, HFILL }},
#endif

#if 0
        { &hf_netlogon_neg_flags_4000000,
          { "Not used 4000000", "ntlmssp.neg_flags.na400000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_4000000, NULL, HFILL }},
#endif

#if 0
        { &hf_netlogon_neg_flags_2000000,
          { "Not used 2000000", "ntlmssp.neg_flags.na200000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_2000000, NULL, HFILL }},
#endif

        { &hf_netlogon_neg_flags_1000000,
          { "AES supported", "ntlmssp.neg_flags.na1000000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_AES, NULL, HFILL }},

#if 0
        { &hf_netlogon_neg_flags_800000,
          { "Not used 800000", "ntlmssp.neg_flags.na800000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_800000, "Not used", HFILL }},
#endif

#if 0
        { &hf_netlogon_neg_flags_400000,
          { "Not used 400000", "ntlmssp.neg_flags.na400000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_400000, "AES&SHA2", HFILL }},
#endif

        { &hf_netlogon_neg_flags_200000,
          { "RODC pass-through", "ntlmssp.neg_flags.na200000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_200000, "rodc pt", HFILL }},

        { &hf_netlogon_neg_flags_100000,
          { "NO NT4 emulation", "ntlmssp.neg_flags.na100000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_100000, "No NT4 emu", HFILL }},

        { &hf_netlogon_neg_flags_80000,
          { "Cross forest trust", "ntlmssp.neg_flags.na80000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_80000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_40000,
          { "GetDomainInfo supported", "ntlmssp.neg_flags.na40000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_40000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_20000,
          { "ServerPasswordSet2 supported", "ntlmssp.neg_flags.na20000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_20000, "PasswordSet2", HFILL }},

        { &hf_netlogon_neg_flags_10000,
          { "DNS trusts supported", "ntlmssp.neg_flags.na10000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_10000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_8000,
          { "Transitive trusts", "ntlmssp.neg_flags.na8000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_8000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_4000,
          { "Strong key", "ntlmssp.neg_flags.na4000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_STRONGKEY, NULL, HFILL }},

        { &hf_netlogon_neg_flags_2000,
          { "Avoid replication Auth database", "ntlmssp.neg_flags.na2000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_2000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_1000,
          { "Avoid replication account database", "ntlmssp.neg_flags.na1000", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_1000, NULL, HFILL }},

        { &hf_netlogon_neg_flags_800,
          { "Concurrent RPC", "ntlmssp.neg_flags.na800", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_800, NULL, HFILL }},

        { &hf_netlogon_neg_flags_400,
          { "Generic pass-through", "ntlmssp.neg_flags.na400", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_400, NULL, HFILL }},

        { &hf_netlogon_neg_flags_200,
          { "SendToSam", "ntlmssp.neg_flags.na200", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_200, NULL, HFILL }},

        { &hf_netlogon_neg_flags_100,
          { "Refusal of password change", "ntlmssp.neg_flags.na100", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_100, "PWD change refusal", HFILL }},

        { &hf_netlogon_neg_flags_80,
          { "DatabaseRedo call", "ntlmssp.neg_flags.na80", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_80, NULL, HFILL }},

        { &hf_netlogon_neg_flags_40,
          { "Handle multiple SIDs", "ntlmssp.neg_flags.na40", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_40, NULL, HFILL }},

        { &hf_netlogon_neg_flags_20,
          { "Restarting full DC sync", "ntlmssp.neg_flags.na20", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_20, NULL, HFILL }},

        { &hf_netlogon_neg_flags_10,
          { "BDC handling Changelogs", "ntlmssp.neg_flags.na10", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_10, NULL, HFILL }},

        { &hf_netlogon_neg_flags_8,
          { "Promotion count(deprecated)", "ntlmssp.neg_flags.na8", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_8, NULL, HFILL }},

        { &hf_netlogon_neg_flags_4,
          { "RC4 encryption", "ntlmssp.neg_flags.na4", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_4, NULL, HFILL }},

        { &hf_netlogon_neg_flags_2,
          { "NT3.5 BDC continuous update", "ntlmssp.neg_flags.na2", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_2, NULL, HFILL }},

        { &hf_netlogon_neg_flags_1,
          { "Account lockout", "ntlmssp.neg_flags.na1", FT_BOOLEAN, 32, TFS(&tfs_set_notset), NETLOGON_FLAG_1, NULL, HFILL }},

        { &hf_netlogon_dc_flags,
          { "Domain Controller Flags", "netlogon.dc.flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_dc_flags_pdc_flag,
          { "PDC", "netlogon.dc.flags.pdc",
            FT_BOOLEAN, 32, TFS(&dc_flags_pdc_flag), DS_PDC_FLAG,
            "If this server is a PDC", HFILL }},

        { &hf_netlogon_dc_flags_gc_flag,
          { "GC", "netlogon.dc.flags.gc",
            FT_BOOLEAN, 32, TFS(&dc_flags_gc_flag), DS_GC_FLAG,
            "If this server is a GC", HFILL }},

        { &hf_netlogon_dc_flags_ldap_flag,
          { "LDAP", "netlogon.dc.flags.ldap",
            FT_BOOLEAN, 32, TFS(&dc_flags_ldap_flag), DS_LDAP_FLAG,
            "If this is an LDAP server", HFILL }},

        { &hf_netlogon_dc_flags_ds_flag,
          { "DS", "netlogon.dc.flags.ds",
            FT_BOOLEAN, 32, TFS(&dc_flags_ds_flag), DS_DS_FLAG,
            "If this server is a DS", HFILL }},

        { &hf_netlogon_dc_flags_kdc_flag,
          { "KDC", "netlogon.dc.flags.kdc",
            FT_BOOLEAN, 32, TFS(&dc_flags_kdc_flag), DS_KDC_FLAG,
            "If this is a KDC", HFILL }},

        { &hf_netlogon_dc_flags_timeserv_flag,
          { "Timeserv", "netlogon.dc.flags.timeserv",
            FT_BOOLEAN, 32, TFS(&dc_flags_timeserv_flag), DS_TIMESERV_FLAG,
            "If this server is a TimeServer", HFILL }},

        { &hf_netlogon_dc_flags_closest_flag,
          { "Closest", "netlogon.dc.flags.closest",
            FT_BOOLEAN, 32, TFS(&dc_flags_closest_flag), DS_CLOSEST_FLAG,
            "If this is the closest server", HFILL }},

        { &hf_netlogon_dc_flags_writable_flag,
          { "Writable", "netlogon.dc.flags.writable",
            FT_BOOLEAN, 32, TFS(&dc_flags_writable_flag), DS_WRITABLE_FLAG,
            "If this server can do updates to the database", HFILL }},

        { &hf_netlogon_dc_flags_good_timeserv_flag,
          { "Good Timeserv", "netlogon.dc.flags.good_timeserv",
            FT_BOOLEAN, 32, TFS(&dc_flags_good_timeserv_flag), DS_GOOD_TIMESERV_FLAG,
            "If this is a Good TimeServer", HFILL }},

        { &hf_netlogon_dc_flags_ndnc_flag,
          { "NDNC", "netlogon.dc.flags.ndnc",
            FT_BOOLEAN, 32, TFS(&dc_flags_ndnc_flag), DS_NDNC_FLAG,
            "If this is an NDNC server", HFILL }},

        { &hf_netlogon_dc_flags_dns_controller_flag,
          { "DNS Controller", "netlogon.dc.flags.dns_controller",
            FT_BOOLEAN, 32, TFS(&dc_flags_dns_controller_flag), DS_DNS_CONTROLLER_FLAG,
            "If this server is a DNS Controller", HFILL }},

        { &hf_netlogon_dc_flags_dns_domain_flag,
          { "DNS Domain", "netlogon.dc.flags.dns_domain",
            FT_BOOLEAN, 32, TFS(&dc_flags_dns_domain_flag), DS_DNS_DOMAIN_FLAG,
            NULL, HFILL }},

        { &hf_netlogon_dc_flags_dns_forest_flag,
          { "DNS Forest", "netlogon.dc.flags.dns_forest",
            FT_BOOLEAN, 32, TFS(&dc_flags_dns_forest_flag), DS_DNS_FOREST_FLAG,
            NULL, HFILL }},

        { &hf_netlogon_get_dcname_request_flags,
          { "Flags", "netlogon.get_dcname.request.flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Flags for DSGetDCName request", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_force_rediscovery,
          { "Force Rediscovery", "netlogon.get_dcname.request.flags.force_rediscovery",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_force_rediscovery), DS_FORCE_REDISCOVERY,
            "Whether to allow the server to returned cached information or not", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_directory_service_required,
          { "DS Required", "netlogon.get_dcname.request.flags.ds_required",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_directory_service_required), DS_DIRECTORY_SERVICE_REQUIRED,
            "Whether we require that the returned DC supports w2k or not", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_directory_service_preferred,
          { "DS Preferred", "netlogon.get_dcname.request.flags.ds_preferred",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_directory_service_preferred), DS_DIRECTORY_SERVICE_PREFERRED,
            "Whether we prefer the call to return a w2k server (if available)", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_gc_server_required,
          { "GC Required", "netlogon.get_dcname.request.flags.gc_server_required",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_gc_server_required), DS_GC_SERVER_REQUIRED,
            "Whether we require that the returned DC is a Global Catalog server", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_pdc_required,
          { "PDC Required", "netlogon.get_dcname.request.flags.pdc_required",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_pdc_required), DS_PDC_REQUIRED,
            "Whether we require the returned DC to be the PDC", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_background_only,
          { "Background Only", "netlogon.get_dcname.request.flags.background_only",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_background_only), DS_BACKGROUND_ONLY,
            "If we want cached data, even if it may have expired", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_ip_required,
          { "IP Required", "netlogon.get_dcname.request.flags.ip_required",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_ip_required), DS_IP_REQUIRED,
            "If we require the IP of the DC in the reply", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_kdc_required,
          { "KDC Required", "netlogon.get_dcname.request.flags.kdc_required",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_kdc_required), DS_KDC_REQUIRED,
            "If we require that the returned server is a KDC", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_timeserv_required,
          { "Timeserv Required", "netlogon.get_dcname.request.flags.timeserv_required",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_timeserv_required), DS_TIMESERV_REQUIRED,
            "If we require the returned server to be a WindowsTimeServ server", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_writable_required,
          { "Writable Required", "netlogon.get_dcname.request.flags.writable_required",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_writable_required), DS_WRITABLE_REQUIRED,
            "If we require that the returned server is writable", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_good_timeserv_preferred,
          { "Timeserv Preferred", "netlogon.get_dcname.request.flags.good_timeserv_preferred",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_good_timeserv_preferred), DS_GOOD_TIMESERV_PREFERRED,
            "If we prefer Windows Time Servers", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_avoid_self,
          { "Avoid Self", "netlogon.get_dcname.request.flags.avoid_self",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_avoid_self), DS_AVOID_SELF,
            "Return another DC than the one we ask", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_only_ldap_needed,
          { "Only LDAP Needed", "netlogon.get_dcname.request.flags.only_ldap_needed",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_only_ldap_needed), DS_ONLY_LDAP_NEEDED,
            "We just want an LDAP server, it does not have to be a DC", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_is_flat_name,
          { "Is Flat Name", "netlogon.get_dcname.request.flags.is_flat_name",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_is_flat_name), DS_IS_FLAT_NAME,
            "If the specified domain name is a NetBIOS name", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_is_dns_name,
          { "Is DNS Name", "netlogon.get_dcname.request.flags.is_dns_name",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_is_dns_name), DS_IS_DNS_NAME,
            "If the specified domain name is a DNS name", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_return_dns_name,
          { "Return DNS Name", "netlogon.get_dcname.request.flags.return_dns_name",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_return_dns_name), DS_RETURN_DNS_NAME,
            "Only return a DNS name (or an error)", HFILL }},

        { &hf_netlogon_get_dcname_request_flags_return_flat_name,
          { "Return Flat Name", "netlogon.get_dcname.request.flags.return_flat_name",
            FT_BOOLEAN, 32, TFS(&get_dcname_request_flags_return_flat_name), DS_RETURN_FLAT_NAME,
            "Only return a NetBIOS name (or an error)", HFILL }},

        { &hf_netlogon_trust_attribs,
          { "Trust Attributes", "netlogon.trust_attribs", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_trust_attribs_non_transitive,
          { "Non Transitive", "netlogon.trust.attribs.non_transitive", FT_BOOLEAN, 32,
            TFS(&trust_attribs_non_transitive), 0x00000001, NULL, HFILL }},

        { &hf_netlogon_trust_attribs_uplevel_only,
          { "Uplevel Only", "netlogon.trust.attribs.uplevel_only", FT_BOOLEAN, 32,
            TFS(&trust_attribs_uplevel_only), 0x00000002, NULL, HFILL }},

        { &hf_netlogon_trust_attribs_quarantined_domain,
          { "Quarantined Domain", "netlogon.trust.attribs.quarantined_domain", FT_BOOLEAN, 32,
            TFS(&trust_attribs_quarantined_domain), 0x00000004, NULL, HFILL }},

        { &hf_netlogon_trust_attribs_forest_transitive,
          { "Forest Transitive", "netlogon.trust.attribs.forest_transitive", FT_BOOLEAN, 32,
            TFS(&trust_attribs_forest_transitive), 0x00000008, NULL, HFILL }},

        { &hf_netlogon_trust_attribs_cross_organization,
          { "Cross Organization", "netlogon.trust.attribs.cross_organization", FT_BOOLEAN, 32,
            TFS(&trust_attribs_cross_organization), 0x00000010, NULL, HFILL }},

        { &hf_netlogon_trust_attribs_within_forest,
          { "Within Forest", "netlogon.trust.attribs.within_forest", FT_BOOLEAN, 32,
            TFS(&trust_attribs_within_forest), 0x00000020, NULL, HFILL }},

        { &hf_netlogon_trust_attribs_treat_as_external,
          { "Treat As External", "netlogon.trust.attribs.treat_as_external", FT_BOOLEAN, 32,
            TFS(&trust_attribs_treat_as_external), 0x00000040, NULL, HFILL }},

        { &hf_netlogon_trust_type,
          { "Trust Type", "netlogon.trust_type", FT_UINT32, BASE_DEC,
            VALS(trust_type_vals), 0x0, NULL, HFILL }},

        { &hf_netlogon_extraflags,
          { "Extra Flags", "netlogon.extra_flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_extra_flags_root_forest,
          { "Request passed to DC of root forest", "netlogon.extra.flags.rootdc",
            FT_BOOLEAN, 32, TFS(&tfs_set_notset), RQ_ROOT_FOREST,
            NULL, HFILL }},

        { &hf_netlogon_trust_flags_dc_firsthop,
          { "DC at the end of the first hop of cross forest", "netlogon.extra.flags.dc_firsthop",
            FT_BOOLEAN, 32, TFS(&tfs_set_notset), RQ_DC_XFOREST,
            NULL, HFILL }},

        { &hf_netlogon_trust_flags_rodc_to_dc,
          { "Request from a RODC to a DC from another domain", "netlogon.extra.flags.rodc_to_dc",
            FT_BOOLEAN, 32, TFS(&tfs_set_notset), RQ_RODC_DIF_DOMAIN,
            NULL, HFILL }},

        { &hf_netlogon_trust_flags_rodc_ntlm,
          { "Request is a NTLM auth passed by a RODC", "netlogon.extra.flags.rodc_ntlm",
            FT_BOOLEAN, 32, TFS(&tfs_set_notset), RQ_NTLM_FROM_RODC,
            NULL, HFILL }},

        { &hf_netlogon_trust_flags,
          { "Trust Flags", "netlogon.trust_flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_trust_flags_inbound,
          { "Inbound Trust", "netlogon.trust.flags.inbound",
            FT_BOOLEAN, 32, TFS(&trust_inbound), DS_DOMAIN_DIRECT_INBOUND,
            "Inbound trust. Whether the domain directly trusts the queried servers domain", HFILL }},

        { &hf_netlogon_trust_flags_outbound,
          { "Outbound Trust", "netlogon.trust.flags.outbound",
            FT_BOOLEAN, 32, TFS(&trust_outbound), DS_DOMAIN_DIRECT_OUTBOUND,
            "Outbound Trust. Whether the domain is directly trusted by the servers domain", HFILL }},

        { &hf_netlogon_trust_flags_in_forest,
          { "In Forest", "netlogon.trust.flags.in_forest",
            FT_BOOLEAN, 32, TFS(&trust_in_forest), DS_DOMAIN_IN_FOREST,
            "Whether this domain is a member of the same forest as the servers domain", HFILL }},

        { &hf_netlogon_trust_flags_native_mode,
          { "Native Mode", "netlogon.trust.flags.native_mode",
            FT_BOOLEAN, 32, TFS(&trust_native_mode), DS_DOMAIN_NATIVE_MODE,
            "Whether the domain is a w2k native mode domain or not", HFILL }},

        { &hf_netlogon_trust_flags_primary,
          { "Primary", "netlogon.trust.flags.primary",
            FT_BOOLEAN, 32, TFS(&trust_primary), DS_DOMAIN_PRIMARY,
            "Whether the domain is the primary domain for the queried server or not", HFILL }},

        { &hf_netlogon_trust_flags_tree_root,
          { "Tree Root", "netlogon.trust.flags.tree_root",
            FT_BOOLEAN, 32, TFS(&trust_tree_root), DS_DOMAIN_TREE_ROOT,
            "Whether the domain is the root of the tree for the queried server", HFILL }},

        { &hf_netlogon_trust_parent_index,
          { "Parent Index", "netlogon.parent_index", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_logon_time,
          { "Logon Time", "netlogon.logon_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time for last time this user logged on", HFILL }},

        { &hf_netlogon_kickoff_time,
          { "Kickoff Time", "netlogon.kickoff_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time when this user will be kicked off", HFILL }},

        { &hf_netlogon_logoff_time,
          { "Logoff Time", "netlogon.logoff_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time for last time this user logged off", HFILL }},

        { &hf_netlogon_last_logoff_time,
          { "Last Logoff Time", "netlogon.last_logoff_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time for last time this user logged off", HFILL }},

        { &hf_netlogon_pwd_last_set_time,
          { "PWD Last Set", "netlogon.pwd_last_set_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Last time this users password was changed", HFILL }},

        { &hf_netlogon_pwd_age,
          { "PWD Age", "netlogon.pwd_age", FT_RELATIVE_TIME, BASE_NONE,
            NULL, 0, "Time since this users password was changed", HFILL }},

        { &hf_netlogon_pwd_can_change_time,
          { "PWD Can Change", "netlogon.pwd_can_change_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "When this users password may be changed", HFILL }},

        { &hf_netlogon_pwd_must_change_time,
          { "PWD Must Change", "netlogon.pwd_must_change_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "When this users password must be changed", HFILL }},

        { &hf_netlogon_domain_create_time,
          { "Domain Create Time", "netlogon.domain_create_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time when this domain was created", HFILL }},

        { &hf_netlogon_domain_modify_time,
          { "Domain Modify Time", "netlogon.domain_modify_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time when this domain was last modified", HFILL }},

        { &hf_netlogon_db_modify_time,
          { "DB Modify Time", "netlogon.db_modify_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time when last modified", HFILL }},

        { &hf_netlogon_db_create_time,
          { "DB Create Time", "netlogon.db_create_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time when created", HFILL }},

        { &hf_netlogon_cipher_current_set_time,
          { "Cipher Current Set Time", "netlogon.cipher_current_set_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time when current cipher was initiated", HFILL }},

        { &hf_netlogon_cipher_old_set_time,
          { "Cipher Old Set Time", "netlogon.cipher_old_set_time", FT_ABSOLUTE_TIME, ABSOLUTE_TIME_LOCAL,
            NULL, 0, "Time when previous cipher was initiated", HFILL }},

        { &hf_netlogon_audit_retention_period,
          { "Audit Retention Period", "netlogon.audit_retention_period", FT_RELATIVE_TIME, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_timelimit,
          { "Time Limit", "netlogon.time_limit", FT_RELATIVE_TIME, BASE_NONE,
            NULL, 0, NULL, HFILL }},


        { &hf_client_credential,
          { "Client Credential", "netlogon.clientcred", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},

        { &hf_server_credential,
          { "Server Credential", "netlogon.servercred", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},

        { &hf_server_rid,
          { "Account RID", "netlogon.serverrid", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},

        { &hf_client_challenge,
          { "Client Challenge", "netlogon.clientchallenge", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},

        { &hf_server_challenge,
          { "Server Challenge", "netlogon.serverchallenge", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_message_type,
          { "Message Type", "netlogon.secchan.nl_auth_message.message_type", FT_UINT32, BASE_HEX,
            VALS(nl_auth_types), 0x0, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_message_flags,
          { "Message Flags", "netlogon.secchan.nl_auth_message.message_flags", FT_UINT32, BASE_HEX,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_message_flags_nb_domain,
          { "NetBios Domain", "netlogon.secchan.nl_auth_message.message_flags.nb_domain", FT_BOOLEAN, 32,
            NULL, 0x00000001, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_message_flags_nb_host,
          { "NetBios Host", "netlogon.secchan.nl_auth_message.message_flags.nb_host", FT_BOOLEAN, 32,
            NULL, 0x00000002, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_message_flags_dns_domain,
          { "DNS Domain", "netlogon.secchan.nl_auth_message.message_flags.dns_domain", FT_BOOLEAN, 32,
            NULL, 0x00000004, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_message_flags_dns_host,
          { "DNS Host", "netlogon.secchan.nl_auth_message.message_flags.dns_host", FT_BOOLEAN, 32,
            NULL, 0x00000008, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_message_flags_nb_host_utf8,
          { "NetBios Host(UTF8)", "netlogon.secchan.nl_auth_message.message_flags.nb_host_utf8", FT_BOOLEAN, 32,
            NULL, 0x00000010, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_nb_domain,
          { "NetBios Domain", "netlogon.secchan.nl_auth_message.nb_domain", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_nb_host,
          { "NetBios Host", "netlogon.secchan.nl_auth_message.nb_host", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_nb_host_utf8,
          { "NetBios Host(UTF8)", "netlogon.secchan.nl_auth_message.nb_host_utf8", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_dns_domain,
          { "DNS Domain", "netlogon.secchan.nl_auth_message.dns_domain", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_secchan_nl_dns_host,
          { "DNS Host", "netlogon.secchan.nl_auth_message.dns_host", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_data_length,
          { "Length of Data", "netlogon.data.length", FT_UINT32, BASE_DEC,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_package_name,
          { "SSP Package Name", "netlogon.data.package_name", FT_STRING, BASE_NONE,
            NULL, 0, NULL, HFILL }},

        { &hf_netlogon_secchan_verf,
          { "Secure Channel Verifier", "netlogon.secchan.verifier", FT_NONE, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},

        { &hf_netlogon_secchan_verf_signalg,
          { "Sign algorithm", "netlogon.secchan.signalg", FT_UINT16, BASE_HEX,
            VALS(sign_algs), 0, NULL, HFILL }},

        { &hf_netlogon_secchan_verf_sealalg,
          { "Seal algorithm", "netlogon.secchan.sealalg", FT_UINT16, BASE_HEX,
            VALS(seal_algs), 0, NULL, HFILL }},

        { &hf_netlogon_secchan_verf_flag,
          { "Flags", "netlogon.secchan.flags", FT_BYTES, BASE_NONE, NULL,
            0x0, NULL, HFILL }},

        { &hf_netlogon_secchan_verf_digest,
          { "Packet Digest", "netlogon.secchan.digest", FT_BYTES, BASE_NONE, NULL,
            0x0, NULL, HFILL }},

        { &hf_netlogon_secchan_verf_seq,
          { "Sequence No", "netlogon.secchan.seq", FT_BYTES, BASE_NONE, NULL,
            0x0, NULL, HFILL }},

        { &hf_netlogon_secchan_verf_nonce,
          { "Nonce", "netlogon.secchan.nonce", FT_BYTES, BASE_NONE, NULL,
            0x0, NULL, HFILL }},

        { &hf_netlogon_user_flags_extra_sids,
          { "Extra SIDs", "netlogon.user.flags.extra_sids",
            FT_BOOLEAN, 32, TFS(&user_flags_extra_sids), 0x00000020,
            "The user flags EXTRA_SIDS", HFILL }},

        { &hf_netlogon_user_flags_resource_groups,
          { "Resource Groups", "netlogon.user.flags.resource_groups",
            FT_BOOLEAN, 32, TFS(&user_flags_resource_groups), 0x00000200,
            "The user flags RESOURCE_GROUPS", HFILL }},

        { &hf_netlogon_user_account_control_dont_require_preauth,
          { "Don't Require PreAuth", "netlogon.user.account_control.dont_require_preauth",
            FT_BOOLEAN, 32, TFS(&user_account_control_dont_require_preauth), 0x00010000,
            "The user account control DONT_REQUIRE_PREAUTH flag", HFILL }},

        { &hf_netlogon_user_account_control_use_des_key_only,
          { "Use DES Key Only", "netlogon.user.account_control.use_des_key_only",
            FT_BOOLEAN, 32, TFS(&user_account_control_use_des_key_only), 0x00008000,
            "The user account control use_des_key_only flag", HFILL }},

        { &hf_netlogon_user_account_control_not_delegated,
          { "Not Delegated", "netlogon.user.account_control.not_delegated",
            FT_BOOLEAN, 32, TFS(&user_account_control_not_delegated), 0x00004000,
            "The user account control not_delegated flag", HFILL }},

        { &hf_netlogon_user_account_control_trusted_for_delegation,
          { "Trusted For Delegation", "netlogon.user.account_control.trusted_for_delegation",
            FT_BOOLEAN, 32, TFS(&user_account_control_trusted_for_delegation), 0x00002000,
            "The user account control trusted_for_delegation flag", HFILL }},

        { &hf_netlogon_user_account_control_smartcard_required,
          { "SmartCard Required", "netlogon.user.account_control.smartcard_required",
            FT_BOOLEAN, 32, TFS(&user_account_control_smartcard_required), 0x00001000,
            "The user account control smartcard_required flag", HFILL }},

        { &hf_netlogon_user_account_control_encrypted_text_password_allowed,
          { "Encrypted Text Password Allowed", "netlogon.user.account_control.encrypted_text_password_allowed",
            FT_BOOLEAN, 32, TFS(&user_account_control_encrypted_text_password_allowed), 0x00000800,
            "The user account control encrypted_text_password_allowed flag", HFILL }},

        { &hf_netlogon_user_account_control_account_auto_locked,
          { "Account Auto Locked", "netlogon.user.account_control.account_auto_locked",
            FT_BOOLEAN, 32, TFS(&user_account_control_account_auto_locked), 0x00000400,
            "The user account control account_auto_locked flag", HFILL }},

        { &hf_netlogon_user_account_control_dont_expire_password,
          { "Don't Expire Password", "netlogon.user.account_control.dont_expire_password",
            FT_BOOLEAN, 32, TFS(&user_account_control_dont_expire_password), 0x00000200,
            "The user account control dont_expire_password flag", HFILL }},

        { &hf_netlogon_user_account_control_server_trust_account,
          { "Server Trust Account", "netlogon.user.account_control.server_trust_account",
            FT_BOOLEAN, 32, TFS(&user_account_control_server_trust_account), 0x00000100,
            "The user account control server_trust_account flag", HFILL }},

        { &hf_netlogon_user_account_control_workstation_trust_account,
          { "Workstation Trust Account", "netlogon.user.account_control.workstation_trust_account",
            FT_BOOLEAN, 32, TFS(&user_account_control_workstation_trust_account), 0x00000080,
            "The user account control workstation_trust_account flag", HFILL }},

        { &hf_netlogon_user_account_control_interdomain_trust_account,
          { "Interdomain trust Account", "netlogon.user.account_control.interdomain_trust_account",
            FT_BOOLEAN, 32, TFS(&user_account_control_interdomain_trust_account), 0x00000040,
            "The user account control interdomain_trust_account flag", HFILL }},

        { &hf_netlogon_user_account_control_mns_logon_account,
          { "MNS Logon Account", "netlogon.user.account_control.mns_logon_account",
            FT_BOOLEAN, 32, TFS(&user_account_control_mns_logon_account), 0x00000020,
            "The user account control mns_logon_account flag", HFILL }},

        { &hf_netlogon_user_account_control_normal_account,
          { "Normal Account", "netlogon.user.account_control.normal_account",
            FT_BOOLEAN, 32, TFS(&user_account_control_normal_account), 0x00000010,
            "The user account control normal_account flag", HFILL }},

        { &hf_netlogon_user_account_control_temp_duplicate_account,
          { "Temp Duplicate Account", "netlogon.user.account_control.temp_duplicate_account",
            FT_BOOLEAN, 32, TFS(&user_account_control_temp_duplicate_account), 0x00000008,
            "The user account control temp_duplicate_account flag", HFILL }},

        { &hf_netlogon_user_account_control_password_not_required,
          { "Password Not Required", "netlogon.user.account_control.password_not_required",
            FT_BOOLEAN, 32, TFS(&user_account_control_password_not_required), 0x00000004,
            "The user account control password_not_required flag", HFILL }},

        { &hf_netlogon_user_account_control_home_directory_required,
          { "Home Directory Required", "netlogon.user.account_control.home_directory_required",
            FT_BOOLEAN, 32, TFS(&user_account_control_home_directory_required), 0x00000002,
            "The user account control home_directory_required flag", HFILL }},

        { &hf_netlogon_user_account_control_account_disabled,
          { "Account Disabled", "netlogon.user.account_control.account_disabled",
            FT_BOOLEAN, 32, TFS(&user_account_control_account_disabled), 0x00000001,
            "The user account control account_disabled flag", HFILL }},

#if 0
        { &hf_netlogon_dnsdomaininfo,
          { "DnsDomainInfo", "netlogon.dnsdomaininfo", FT_NONE, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
#endif

        { &hf_dns_domain_info_sid,
          { "Sid", "netlogon.lsa_DnsDomainInfo.sid", FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_domain_info_sid,
          { "Sid", "netlogon.lsa_DomainInfo.sid", FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_dns_domain_info_domain_guid,
          { "Domain Guid", "netlogon.lsa_DnsDomainInfo.domain_guid", FT_GUID, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_dns_domain_info_dns_forest,
          { "Dns Forest", "netlogon.lsa_DnsDomainInfo.dns_forest", FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_dns_domain_info_dns_domain,
          { "Dns Domain", "netlogon.lsa_DnsDomainInfo.dns_domain", FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_dns_domain_info_name,
          { "Name", "netlogon.lsa_DnsDomainInfo.name", FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_netlogon_s4u2proxytarget,
          { "S4U2proxyTarget", "netlogon.s4u2proxytarget", FT_STRING, BASE_NONE,
            NULL, 0, "Target for constrained delegation using s4u2proxy", HFILL }},
        { &hf_netlogon_transitedlistsize,
          { "TransitedListSize", "netlogon.transited_list_size", FT_UINT32, BASE_HEX,
            NULL, 0x0, "Number of elements in the TransitedServices array.", HFILL }},
        { &hf_netlogon_transited_service,
          { "Transited Service", "netlogon.transited_service", FT_STRING, BASE_NONE,
            NULL, 0, "S4U2 Transited Service name", HFILL }},
        { &hf_netlogon_logon_duration,
          { "Duration", "netlogon.logon_duration", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_time_created,
          { "Time Created", "netlogon.time_created", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claims_set_size,
          { "Claims Set Size", "netlogon.claims_set_size", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claims_compression_format,
          { "Claims Compression Format", "netlogon.claims_compression_format", FT_UINT1632, BASE_DEC,
            VALS(netlogon_claims_compression_format_vals), 0, NULL, HFILL }},
        { &hf_netlogon_claims_set_uncompressed_size,
          { "Claims Set Uncompressed Size", "netlogon.claims_set_uncompressed_size", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claims_reserved_type,
          { "Claims Reserved Type", "netlogon.claims_reserved_type", FT_UINT16, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claims_reserved_field_size,
          { "Claims Reserved Field Size", "netlogon.claims_reserved_field_size", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claims_source_type,
          { "Claims Source Type", "netlogon.claims_source_type", FT_UINT1632, BASE_DEC,
            VALS(hf_netlogon_claims_source_type_vals), 0, NULL, HFILL }},
        { &hf_netlogon_claims_count,
          { "Claims Count", "netlogon.claims_count", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claim_id,
          { "Claim ID", "netlogon.claim_id", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claim_type,
          { "Claim Type", "netlogon.claim_type", FT_UINT1632, BASE_DEC,
            VALS(netlogon_claim_type_vals), 0, NULL, HFILL }},
        { &hf_netlogon_claim_value_count,
          { "Claim Value Count", "netlogon.claim_value_count", FT_UINT32, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claim_int64_value,
          { "Claim INT64 Value", "netlogon.claim_int64_value", FT_INT64, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claim_uint64_value,
          { "Claim UINT64 Value", "netlogon.claim_uint64_value", FT_UINT64, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claim_string_value,
          { "Claim STRING Value", "netlogon.claim_string_value", FT_STRING, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_claim_boolean_value,
          { "Claim BOOLEAN Value", "netlogon.claim_boolean_value", FT_UINT64, BASE_DEC,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_options, {
          "Request Options",
          "netlogon.ticket_logon_options",
          FT_UINT64, BASE_HEX, NULL, 0x0, "Requested Options", HFILL }},
        { &hf_netlogon_ticket_logon_options_0000000000000001, {
          "No Authorization Data",
          "netlogon.ticket_logon_options.no_authorization_data",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000000000001, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_options_0000000000010000, {
          "Skip Resource Groups",
          "netlogon.ticket_logon_options.skip_resource_groups",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000000010000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_options_0000000000020000, {
          "Skip A2A Checks",
          "netlogon.ticket_logon_options.skip_a2a_checks",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000000020000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_options_0000000100000000, {
          "Skip SID Filtering",
          "netlogon.ticket_logon_options.skip_sid_filter",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000100000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_options_0000000200000000, {
          "Skip Namespace Filtering",
          "netlogon.ticket_logon_options.skip_namespace_filter",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000200000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_options_0001000000000000, {
          "Skip PAC Signatures",
          "netlogon.ticket_logon_options.skip_pac_signatures",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0001000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_options_0002000000000000, {
          "Remove Resource Groups",
          "netlogon.ticket_logon_options.remove_resource_groups",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0002000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_service_ticket_size,
          { "Service Ticket Size", "netlogon.ticket_logon_service_ticket_size",
            FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_additional_ticket_size,
          { "Additional Ticket Size", "netlogon.ticket_logon_additional_ticket_size",
            FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results, {
          "Results",
          "netlogon.ticket_logon_results",
          FT_UINT64, BASE_HEX, NULL, 0x0, "Request Results", HFILL }},
        { &hf_netlogon_ticket_logon_results_0000000000000001, {
          "Failed_Logon",
          "netlogon.ticket_logon_results.failed_logon",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000000000001, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0000000100000000, {
          "Ticket Decryption Failed",
          "netlogon.ticket_logon_results.ticket_decryption_failed",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000100000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0000000200000000, {
          "PAC Validation Failed",
          "netlogon.ticket_logon_results.pac_validation_failed",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000200000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0000000400000000, {
          "Compound Source",
          "netlogon.ticket_logon_results.compound_source",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000400000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0000000800000000, {
          "Source User Claims",
          "netlogon.ticket_logon_results.source_user_claims",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000000800000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0000001000000000, {
          "Source Device Claims",
          "netlogon.ticket_logon_results.source_device_claims",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000001000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0000002000000000, {
          "Full Signature Present",
          "netlogon.ticket_logon_results.full_signature_present",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000002000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0000004000000000, {
          "Resource Groups Removed",
          "netlogon.ticket_logon_results.resource_groups_removed",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0000004000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0001000000000000, {
          "User SIDS Failed",
          "netlogon.ticket_logon_results.user_sids_failed",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0001000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0002000000000000, {
          "User Namespace Failed",
          "netlogon.ticket_logon_results.user_namespace_failed",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0002000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0004000000000000, {
          "User Failed A2A",
          "netlogon.ticket_logon_results.user_failed_a2a",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0004000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0008000000000000, {
          "Device SIDS Failed",
          "netlogon.ticket_logon_results.device_sids_failed",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0008000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0010000000000000, {
          "Device Namespace Failed",
          "netlogon.ticket_logon_results.device_namespace_failed",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0010000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0020000000000000, {
          "User SIDS Filtered",
          "netlogon.ticket_logon_results.user_sids_filtered",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0020000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_results_0040000000000000, {
          "Device SIDS Filtered",
          "netlogon.ticket_logon_results.device_sids_filtered",
          FT_BOOLEAN, 64, TFS(&tfs_set_notset),
          0x0040000000000000, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_kerberos_status,
          { "Kerberos NTSTATUS", "netlogon.ticket_logon_kerberos_status",
            FT_UINT32, BASE_HEX|BASE_EXT_STRING, &NT_errors_ext, 0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_netlogon_status,
          { "Netlogon NTSTATUS", "netlogon.ticket_logon_netlogon_status",
            FT_UINT32, BASE_HEX|BASE_EXT_STRING, &NT_errors_ext, 0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_source_of_status,
          { "Source Of Status", "netlogon.ticket_logon_source_of_status",
            FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_user_claims_size,
          { "User Claims Size", "netlogon.ticket_logon_user_claims_size",
            FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_device_claims_size,
          { "Device Claims Size", "netlogon.ticket_logon_device_claims_size",
            FT_UINT32, BASE_DEC, NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_ticket_logon_claims,
          { "Claims", "netlogon.ticket_logon_claims", FT_BYTES, BASE_NONE,
            NULL, 0x0, NULL, HFILL }},
        { &hf_netlogon_forest_trust_info_flags, {
          "Flags",
          "netlogon.forest_trust_info_flags",
          FT_UINT32, BASE_HEX, NULL, 0x0, "Forest Trust Info Flags", HFILL }},
        { &hf_netlogon_forest_trust_info_flags_00000001, {
          "Update Trusted Domain Object",
          "netlogon.forest_trust_info_flags.update_tdo",
          FT_BOOLEAN, 32, TFS(&tfs_set_notset),
          0x00000001, NULL, HFILL }},
        { &hf_netlogon_forest_trust_info,
          { "Forest Trust Info", "netlogon.forest_trust_info",
            FT_NONE, BASE_NONE, NULL, 0, NULL, HFILL }},

    };

    static int *ett[] = {
        &ett_dcerpc_netlogon,
        &ett_authenticate_flags,
        &ett_CYPHER_VALUE,
        &ett_QUOTA_LIMITS,
        &ett_IDENTITY_INFO,
        &ett_DELTA_ENUM,
        &ett_UNICODE_MULTI,
        &ett_DOMAIN_CONTROLLER_INFO,
        &ett_netr_CryptPassword,
        &ett_NL_PASSWORD_VERSION,
        &ett_NL_GENERIC_RPC_DATA,
        &ett_TYPE_50,
        &ett_TYPE_52,
        &ett_DELTA_ID_UNION,
        &ett_CAPABILITIES,
        &ett_DELTA_UNION,
        &ett_LM_OWF_PASSWORD,
        &ett_NT_OWF_PASSWORD,
        &ett_GROUP_MEMBERSHIP,
        &ett_DS_DOMAIN_TRUSTS,
        &ett_BLOB,
        &ett_DOMAIN_TRUST_INFO,
        &ett_LSA_POLICY_INFO,
        &ett_trust_flags,
        &ett_trust_attribs,
        &ett_get_dcname_request_flags,
        &ett_dc_flags,
        &ett_secchan_nl_auth_message,
        &ett_secchan_nl_auth_message_flags,
        &ett_secchan_verf,
        &ett_group_attrs,
        &ett_user_flags,
        &ett_nt_counted_longs_as_string,
        &ett_user_account_control,
        &ett_wstr_LOGON_IDENTITY_INFO_string,
        &ett_domain_group_memberships,
        &ett_domains_group_memberships,
        &ett_netlogon_ticket_logon_options,
        &ett_netlogon_ticket_logon_results,
        &ett_netlogon_ticket_logon_claims,
        &ett_netlogon_forest_trust_info_flags,
    };
    static ei_register_info ei[] = {
     { &ei_netlogon_auth_nthash, {
       "netlogon.authenticated", PI_SECURITY, PI_CHAT,
       "Authenticated NTHASH", EXPFILL
     }},
     { &ei_netlogon_session_key, {
       "netlogon.sessionkey", PI_SECURITY, PI_CHAT,
       "SessionKey", EXPFILL
     }},
    };
    expert_module_t* expert_netlogon;

    proto_dcerpc_netlogon = proto_register_protocol("Microsoft Network Logon", "RPC_NETLOGON", "rpc_netlogon");

    proto_register_field_array(proto_dcerpc_netlogon, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
    expert_netlogon = expert_register_protocol(proto_dcerpc_netlogon);
    expert_register_field_array(expert_netlogon, ei, array_length(ei));

    netlogon_auths = wmem_map_new_autoreset(wmem_epan_scope(), wmem_file_scope(), netlogon_auth_hash, netlogon_auth_equal);
    schannel_auths = wmem_map_new_autoreset(wmem_epan_scope(), wmem_file_scope(), dcerpc_auth_schannel_key_hash, dcerpc_auth_schannel_key_equal);
}

static dcerpc_auth_subdissector_fns secchan_auth_fns = {
    dissect_secchan_nl_auth_message,    /* Bind */
    dissect_secchan_nl_auth_message,    /* Bind ACK */
    NULL,                               /* AUTH3 */
    dissect_request_secchan_verf,       /* Request verifier */
    dissect_response_secchan_verf,      /* Response verifier */
    dissect_request_data,               /* Request data */
    dissect_response_data               /* Response data */
};

void
proto_reg_handoff_dcerpc_netlogon(void)
{
    /* Register protocol as dcerpc */
    seen.isseen = false;
    seen.num = 0;
    dcerpc_init_uuid(proto_dcerpc_netlogon, ett_dcerpc_netlogon,
                     &uuid_dcerpc_netlogon, ver_dcerpc_netlogon,
                     dcerpc_netlogon_dissectors, hf_netlogon_opnum);


    register_dcerpc_auth_subdissector(DCE_C_AUTHN_LEVEL_PKT_INTEGRITY,
                                      DCE_C_RPC_AUTHN_PROTOCOL_SEC_CHAN,
                                      &secchan_auth_fns);
    register_dcerpc_auth_subdissector(DCE_C_AUTHN_LEVEL_PKT_PRIVACY,
                                      DCE_C_RPC_AUTHN_PROTOCOL_SEC_CHAN,
                                      &secchan_auth_fns);
}

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
