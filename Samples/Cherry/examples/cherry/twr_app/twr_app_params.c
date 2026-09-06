/*
 * This file contains parameters' parsing for twr_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "twr_app_params.h"

#include <unistd.h>
#include <util_convert.h>
#include <util_log.h>
#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#include <getopt.h>
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#ifdef CONFIG_CHERRY_CALIB_FOLDER
#define OPTSTR "CA:Dan:i:p:P:Rs:S:m:e:v:hl:d:Y:Z:"
#else
#define OPTSTR "CA:Dan:i:p:P:Rs:S:m:e:v:hl:d:"
#endif
#endif

const struct cherry_fira_phy_params phy_params_set[] = {
	/* rframe / prf mode / sfd id / preamble duration / nb sts segments / sts lentgh / psdu data rate */
	/* HPRF Set 6 */
	CHERRY_FIRA_PHY_PARAMS(SP1, HPRF_124_8M, 2, 64, 1, 64, 6_81M),
	/* HPRF Set 20 */
	CHERRY_FIRA_PHY_PARAMS(SP3, HPRF_124_8M, 3, 64, 1, 128, NA),
	/* BPRF Set 3 */
	CHERRY_FIRA_PHY_PARAMS(SP1, BPRF_62_4M, 2, 64, 1, 64, 6_81M),
	/* BPRF Set 4 (Default) */
	CHERRY_FIRA_PHY_PARAMS(SP3, BPRF_62_4M, 2, 64, 1, 64, NA),
	/* BPRF Set 5 */
	CHERRY_FIRA_PHY_PARAMS(SP1, BPRF_62_4M, 0, 64, 1, 64, 6_81M),
	/* BPRF Set 6 */
	CHERRY_FIRA_PHY_PARAMS(SP3, BPRF_62_4M, 0, 64, 1, 64, NA),
};

#define MAX_PRF_SET_ID sizeof(phy_params_set) / sizeof(phy_params_set[0])
#define MAX_HPRF_SET_ID 2

const uint16_t controlee_addresses[] = { CONTROLEE_1_MAC_ADDRESS,
					 CONTROLEE_2_MAC_ADDRESS };

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
static void twr_app_usage(void)
{
	QLOGD("%s", TWR_APP_USAGE_HELP);
}
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
static bool valueinarray(uint16_t val, const uint16_t *arr, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (arr[i] == val)
			return true;
	}
	return false;
}
#endif

bool get_runtime_twr_app_param(int argc, char *argv[],
			       struct app_twr_parameter *params)
{
#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
	int opt;
	bool is_hprf = false;

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	/* On zephyr, getopt must be reseted between multiple calls. */
	getopt_init();
#endif
	while ((opt = getopt(argc, argv, OPTSTR)) != -1)
		switch (opt) {
		case 'C':
			/* Setup address for first controlee. */
			params->is_controller = false;
			break;
		case 'A':
			if (!params->is_controller) {
				if (!util_convert_arg_to_uint16(
					    optarg, &params->short_addr)) {
					QLOGE("Invalid number for the controlee address.");
					return false;
				}
				if (!valueinarray(params->short_addr,
						  controlee_addresses, 2)) {
					QLOGE("Invalid adress : %u for the controlee. Default adress %u will be set",
					      params->short_addr,
					      controlee_addresses[0]);
					params->short_addr =
						controlee_addresses[0];
				}
			} else {
				QLOGD("Cannot change adress of the controller.");
			}
			break;
		case 'D':
			params->diagnostic_enabled = true;
			break;
		case 'a':
			params->aoa_360_enabled = true;
			break;
		case 'n':
			if (!util_convert_arg_to_uint16(
				    optarg, &params->max_nb_measurements)) {
				QLOGE("Invalid number for the number of measurement.");
				return false;
			}
			break;
		case 'i':
			if (!util_convert_arg_to_int32(optarg,
						       &params->interval_ms)) {
				QLOGE("Invalid number for the ranging interval.");
				return false;
			}
			break;
		case 'p':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->preamble_code_index)) {
				QLOGE("Invalid number for the preamble code index.");
				return false;
			}
			break;
		case 'P': {
			uint8_t i;
			if (!util_convert_arg_to_uint8(optarg, &i) || i == 0 ||
			    i > MAX_PRF_SET_ID) {
				QLOGE("Invalid number for the PHY parameter set."
				      " Should be between 1 and %d.",
				      MAX_PRF_SET_ID);
				return false;
			}
			params->phy_params = &phy_params_set[i - 1];
			if (i <= MAX_HPRF_SET_ID)
				is_hprf = true;

			break;
		}
		case 'R':
			params->report_rssi = 1;
			break;
		case 's':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->session_priority)) {
				QLOGE("Invalid number for the session priority.");
				return false;
			}
			break;
		case 'S':
			if (!util_convert_arg_to_uint8(optarg,
						       &params->ant_set_id)) {
				QLOGE("Invalid number for the antenna set.");
				return false;
			}
			break;
		case 'm':
			if (!util_convert_arg_to_uint16(
				    optarg, &params->max_rr_retry)) {
				QLOGE("Invalid number for the max ranging round retry.");
				return false;
			}
			break;
		case 'e':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->result_report_config)) {
				QLOGE("Invalid number for the result report config.");
				return false;
			}
			break;
		case 'v':
			if (!util_convert_arg_to_uint8(optarg,
						       &params->sts_config)) {
				QLOGE("Invalid number for the STS config.");
				return false;
			}
			break;
		case 'l':
			if (!util_convert_arg_to_log_level(
				    optarg, &params->log_level)) {
				QLOGE("Invalid log level.");
				return false;
			}
			break;
		case 'd':
			params->device = optarg;
			break;
#ifdef CONFIG_CHERRY_CALIB_FOLDER
		case 'Y': {
			params->country_code = optarg;
			break;
		}
		case 'Z': {
			params->config_path = optarg;
			break;
		}
#endif

		case 'h':
		default:
			twr_app_usage();
			return false;
		}

	if (params->phy_params && is_hprf) {
		if (params->preamble_code_index < 25 ||
		    params->preamble_code_index > 32) {
			QLOGE("For HPRF mode preamble code index has to be in [25-32]");
			return false;
		}
	}
	if (params->ant_set_id > 1 && params->aoa_360_enabled) {
		QLOGE("Antenna set 1 is forced for 360 AoA, antenna set %d will not be used.",
		      params->ant_set_id);
	}
#endif
	return true;
}
