/*
 * This file contains parameters' structure and define for dl_tdoa_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry_fira.h>
#include <stdbool.h>
#include <stdint.h>
#include <util_common_param.h>
#include <util_dltdoa_config.h>

#define MAX_NB_INDEX 33
#define MAX_NB_ADDRESS 8
#define ANCHOR_MAC_ADDRESS_1 161
#define ANCHOR_MAC_ADDRESS_2 162

/**
 * struct dst_mac_addresses- List of dest mac addresses.
 */
struct dst_mac_addresses {
	/**
	 * @n_addresses: Number of addresses to consider.
	 */
	int n_addresses;
	/**
	 * @addresses: array of dest mac addresses.
	 */
	uint16_t addresses[MAX_NB_ADDRESS];
};

#define DLTDOA_APP_NAME "cherry-dl-tdoa-app"

#ifndef CONFIG_CHERRY_CALIB_FOLDER
#define CALIB_APP_USAGE_HELP_COMMAND_EXT ""
#define CALIB_APP_USAGE_HELP_EXT ""
#else
#define CALIB_APP_USAGE_HELP_COMMAND_EXT \
	" [-Y country_code] [-Z configuration_path]"
#define CALIB_APP_USAGE_HELP_EXT                                                             \
	"\t-Y CODE\t\t\tSelect the country code. Configuration path needds to be set too\n " \
	"\t-Z PATH\t\t\tSelect the path of the config folder\n "
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#define DL_TDOA_APP_USAGE_HELP                                                                                                     \
	"\nUsage: " DLTDOA_APP_NAME                                                                                                \
	"\t[-T] [-i INTERVAL_MS] [-l LOG_LEVEL] [-n N_MEASUREMENTS] [-t] [-s SLOT_DURATION]\n"                                     \
	"\t\t\t\t[-r] [-d DEVICE_PATH] [-v CONFIG_STS] [-p PREAMBLE_INDEX] [-x SLOTS_PER_RR] [-a ADDRESS]\n"                       \
	"\t\t\t\t[-R ROLE_PER_ROUND][-N NUMBER_OF_ROUNDS] [-H HPRF_CONFIG] [-B BPRF_CONFIG] [-A]" CALIB_APP_USAGE_HELP_COMMAND_EXT \
	"\n"                                                                                                                       \
	"\n"                                                                                                                       \
	"\nOptions:\n"                                                                                                             \
	"\t-i INTERVAL_MS\t\tSet the interval duration.\n"                                                                         \
	"\t-l LOG_LEVEL\t\tSet cherry's log level (default is 2 for warning).\n"                                                   \
	"\t-n N_MEASUREMENTS\tSet the number of measurements.\n"                                                                   \
	"\t-t\t\t\tSet the device role to dt-tag (default dt-anchor).\n"                                                           \
	"\t-s SLOT_DURATION\tSet slot duration.\n"                                                                                 \
	"\t-T\t\t\tSet DL_TDOA_TIME_REFERENCE to 1.\n"                                                                             \
	"\t-r\t\t\tdisable rssi report.\n"                                                                                         \
	"\t-d PATH\t\t\tset the device path (UCI or TTY), default is /dev/uci0.\n"                                                 \
	"\t-v CONFIG_STS\t\tSet the STS mode.\n"                                                                                   \
	"\t-p PREAMBLE_INDEX\tSet preamble code index value.\n"                                                                    \
	"\t-c config\t\tUse a predifined round configuration, 6 configurations available from 1 to 6\n"                            \
	"\t-x SLOTS_PER_RR\t\tSet the number of slots per ranging round.\n"                                                        \
	"\t-a ADDRESS\t\tSet DEVICE_MAC_ADRESS value.\n"                                                                           \
	"\t-R ROLE_PER_ROUND\tAs an anchor, this parameter set the role for each active round between 'init' and 'respf'.\n"       \
	"\t\t\t\tRequired pattern : \n"                                                                                            \
	"\t\t\t\tround_index,[init,address1/address2/..|respf] round_index2,[init,addressX/addressY/..|respf] ... \n"              \
	"\t-I INDEX_ACTIVE_ROUND\tAs a tag, this parameter set the indexes of the round in which the tag is involved.\n"           \
	"\t\t\t\tRequired pattern : 'I1 I2 I3'\n"                                                                                  \
	"\t-H HPRF_CONFIG\t\tSet the required HPRF config.\n"                                                                      \
	"\t-B BPRF_CONFIG\t\tSet the required BPRF config.\n"                                                                      \
	"\t-A\t\t\tEnable transmission of the active ranging rounds list (disabled by default).\n" CALIB_APP_USAGE_HELP_EXT        \
	"\n"                                                                                                                       \
	"\nUsual mono cluster examples:\n"                                                                                         \
	"\tAnchor Iniator time ref\t\tcherry-dl-tdoa-app -a 12 -R '0,init,13/14' -T\n"                                             \
	"\tAnchor Responder\t\tcherry-dl-tdoa-app -a 13 -R '0,respf'\n"                                                            \
	"\tAnchor Responder 2\t\tcherry-dl-tdoa-app -a 14 -R '0,respf'\n"                                                          \
	"\tTag		\t\tcherry-dl-tdoa-app -t -a 24 -I '0'\n"                                                                          \
	"\n\nUsual multi cluster examples:\n"                                                                                      \
	"\tAnchor Iniator time ref\t\tcherry-dl-tdoa-app -a 12 -R '0,init,13/14/15' -T\n"                                          \
	"\tAnchor 2\t\t\tcherry-dl-tdoa-app -a 13 -R '0,respf'\n"                                                                  \
	"\tAnchor 3\t\t\tcherry-dl-tdoa-app -a 14 -R '0,respf 1,init,15/16'\n"                                                     \
	"\tAnchor 4\t\t\tcherry-dl-tdoa-app -a 15 -R '1,respf'\n"                                                                  \
	"\tAnchor 5\t\t\tcherry-dl-tdoa-app -a 16 -R '1,respf 2,init,17' \n"                                                       \
	"\tAnchor 6\t\t\tcherry-dl-tdoa-app -a 17 -R '2,respf'\n"                                                                  \
	"\tTag\t\t\t\tcherry-dl-tdoa-app -t -a 24 -I '0 1 2'\n"                                                                    \
	"\nConfigurations description:\n\n"                                                                                        \
	"\tMulti Cluster :\n"                                                                                                      \
	"\t\t[Config 1] Initiator/Responder and Time reference\n"                                                                  \
	"\t\t[Config 2-6] Initiator/Responder\n"                                                                                   \
	"\tRound configuration :\n"                                                                                                \
	"\t\t R0: Anchor 1 (initiator, Time Ref), Anchor 2,3,4,5,6 (Responder)\n"                                                  \
	"\t\t R1: Anchor 2 (initiator), Anchor 1,3,4,5,6 (Responder)\n"                                                            \
	"\t\t R2: Anchor 3 (initiator), Anchor 1,2,4,5,6 (Responder)\n"                                                            \
	"\t\t R3: Anchor 4 (initiator), Anchor 1,2,3,5,6 (Responder)\n"                                                            \
	"\t\t R7: Anchor 5 (initiator), Anchor 1,2,3,4,6 (Responder)\n"                                                            \
	"\t\t R8: Anchor 6 (initiator), Anchor 1,2,3,4,5 (Responder)\n"                                                            \
	"\t\t R9: Anchor 1 (initiator), Anchor 2,3,4,5,6 (Responder)\n"                                                            \
	"\tCommand lines :\n"                                                                                                      \
	"\t\tAnchor 1\t\tcherry-dl-tdoa-app -c 1 -T\n"                                                                             \
	"\t\tAnchor 2\t\tcherry-dl-tdoa-app -c 2\n"                                                                                \
	"\t\tAnchor [3-6]\t\tcherry-dl-tdoa-app -c [3-6]\n"                                                                        \
	"\nThis example uses the antenna set 0 by default.\n"
#endif

struct app_dltdoa_parameter {
	struct dl_tdoa_config_light light_conf;
	uint8_t block_skipping;
	uint8_t config_choice;
	uint8_t index_tag[MAX_NB_INDEX];
	uint8_t preamble_code_index;
	uint8_t report_rssi;
	uint8_t slots_per_rr;
	uint8_t sts_config;
	uint16_t max_nb_measurements;
	uint16_t slot_duration;
	uint32_t interval_ms;
	uint32_t session_id;
	bool is_anchor;
	bool is_multi_cluster;
	bool is_time_ref;
	const char *device;
	const struct cherry_fira_phy_params *phy_params;
	enum cherry_log_level log_level;
	struct dst_mac_addresses dst_address;
	bool tx_active_rr;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
	const char *config_path;
	const char *country_code;
#endif
};

void free_runtime_dltdoa_app_param(struct app_dltdoa_parameter *params);

bool get_runtime_dltdoa_app_param(int argc, char *argv[],
				  struct app_dltdoa_parameter *params);
