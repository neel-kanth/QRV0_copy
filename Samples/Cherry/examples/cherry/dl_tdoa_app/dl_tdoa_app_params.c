/*
 * This file contains parameters' parsing for dl_tdoa_app sample.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "dl_tdoa_app_params.h"

#include "util_dltdoa_config.h"

#include <cherry/cherry_fira.h>
#include <qmalloc.h>
#include <string.h>
#include <unistd.h>
#include <util_bprf.h>
#include <util_convert.h>
#include <util_hprf.h>
#include <util_log.h>

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
#include <getopt.h>
#endif

#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
#ifdef CONFIG_CHERRY_CALIB_FOLDER
#define OPTSTR "i:l:n:ts:rTd:v:p:c:x:a:R:I:H:B:hAY:Z:"
#else
#define OPTSTR "i:l:n:ts:rTd:v:p:c:x:a:R:I:H:B:hA"
#endif

static void dl_tdoa_app_usage(void)
{
	QLOGD("%s", DL_TDOA_APP_USAGE_HELP);
}

static bool valueinarray(uint16_t val, const uint16_t *arr, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (arr[i] == val)
			return true;
	}
	return false;
}

/**
 * alloc_memory_round_config - Get the number of substring separated by a space in the string.
 * @params: Pointer to the parameters struct to fill.
 *
 * Allocate the memory for the round configuration base on the number of rounds to configure.
 *
 * Returns :
 * 	- True if succeed.
 *  - False for any error.
 */
static bool alloc_memory_round_config(struct app_dltdoa_parameter *params)
{
	params->light_conf.round_config_custom =
		(struct cherry_fira_anchor_round_config *)qmalloc(
			params->light_conf.n_rounds *
			sizeof(struct cherry_fira_anchor_round_config));
	if (!params->light_conf.round_config_custom)
		return false;
	/* Init values for the round_config_custom. This allow to check if we do not set those value twice later on. */
	for (int config_index = 0; config_index < params->light_conf.n_rounds;
	     config_index++) {
		params->light_conf.round_config_custom[config_index]
			.n_responders = 0;
		params->light_conf.round_config_custom[config_index].round_idx =
			-1;
	}
	return true;
}

/**
 * check_round - Get the number of substring separated by a space in the string.
 * @arg: Pointer to the string to parse.
 *
 * Parse through the string, ensure the responder address are valid
 * and update the destination addresses parameter from the struct.
 *
 * Returns :
 *  - Number of substring present in the string.
 */
static int check_round(char *arg)
{
	int n_round = 0;
	char *tok, *save_arg;

	tok = strtok_r(arg, " ", &save_arg);
	while (tok != NULL) {
		tok = strtok_r(NULL, " ", &save_arg);
		n_round++;
	}
	return n_round;
}

/**
 * fetch_responders_addresses - Get the responders' address from a string pointer.
 * @optarg: Pointer to string containing the responders' addresses.
 * @params: Pointer to the parameters struct to fill.
 * @index_in_arg: Index of the sub string in the argument.
 *
 * Parse through the string, ensure the responder address are valid
 * and update the destination addresses parameter from the struct.
 *
 * Returns :
 * 	- True if succeed.
 *  - False for any error.
 */
static bool fetch_responders_addresses(char *slot_descriptor,
				       struct app_dltdoa_parameter *params,
				       int index_in_arg)
{
	char *address, *save_addresses;
	int nb_address = 0;

	if (params->light_conf.round_config_custom[index_in_arg].n_responders) {
		QLOGE("The responder(s) on round %d are specified more than once.",
		      params->light_conf.round_config_custom[index_in_arg]
			      .round_idx);
		return false;
	}

	address = strtok_r(slot_descriptor, "/", &save_addresses);

	while (address != NULL) {
		uint16_t address_value;

		/* Convert argument into address value. */
		if (!util_convert_arg_to_uint16(address, &address_value)) {
			QLOGE("Invalid address for the responder number %d of round %d."
			      "Check if the parameter respect the required pattern :"
			      "'index,A1/A2/A3 index2,A4/A5/A6 with ' ' between round,"
			      "',' between index and adress list and '/' between addresses.",
			      nb_address + 1,
			      params->light_conf
				      .round_config_custom[index_in_arg]
				      .round_idx);
			return false;
		}
		/* Check if the address value already exist, if not add it to the destination address list. */
		if (!valueinarray(address_value, params->dst_address.addresses,
				  params->dst_address.n_addresses)) {
			if (params->dst_address.n_addresses == MAX_NB_ADDRESS) {
				QLOGE("Too many responder addresses. The maximum unique address is %d",
				      MAX_NB_ADDRESS);
				return false;
			}
			if (params->light_conf.device_mac_address ==
			    address_value) {
				QLOGE("The responder address has to be different from the address of the initiator.");
				return false;
			}
			/* Set the value in the dst_address struct common to every round
			and increment the number of address listed. */
			params->dst_address
				.addresses[params->dst_address.n_addresses] =
				address_value;
			params->dst_address.n_addresses++;
		}
		if (valueinarray(address_value,
				 params->light_conf
					 .round_config_custom[index_in_arg]
					 .responders_addr,
				 nb_address)) {
			QLOGE("Same responder address provided twice for the same round.");
			return false;
		}

		/* Set the value in this specific round configuration. */
		params->light_conf.round_config_custom[index_in_arg]
			.responders_addr[nb_address] = address_value;
		address = strtok_r(NULL, "/", &save_addresses);
		nb_address++;
	}
	/* Update number of responders for this round configuration. */
	params->light_conf.round_config_custom[index_in_arg].n_responders =
		nb_address;

	return true;
}

/**
 * fetch_round_config - Get the round configuration from a string pointer.
 * @optarg: Pointer to string containing the round configuration.
 * @params: Pointer to the parameters struct to fill.
 * @index_in_arg: Index of the sub string in the argument.
 *
 * Parse through the string and fill the parameters struct.
 *
 * Returns :
 * 	- True if succeed.
 *  - False for any error.
 */
static bool fetch_round_config(char *round_tok,
			       struct app_dltdoa_parameter *params,
			       int index_in_arg)
{
	/* This pointers allow us to loop through each values in the string. */
	char *round_descriptor, *save_descriptor;
	struct cherry_fira_anchor_round_config *round_configuration =
		&params->light_conf.round_config_custom[index_in_arg];

	/* Every value fetched with strtok_r() will be refenced to as a token. */
	/* Check the first token, expected to be the round index. */
	if ((round_descriptor = strtok_r(round_tok, ",", &save_descriptor))) {
		uint8_t local_index;
		int config_index;
		if (!util_convert_arg_to_uint8(round_descriptor,
					       &local_index)) {
			QLOGE("Invalid value for index of round config number : %d",
			      index_in_arg);
			return false;
		}
		/* Ensure the round for this index has not been already defined. */
		for (config_index = 0; config_index < index_in_arg;
		     config_index++) {
			if (params->light_conf.round_config_custom[config_index]
				    .round_idx == local_index) {
				QLOGE("Round role for round index %d is defined more than once.",
				      local_index);
				return false;
			}
		}
		round_configuration->round_idx = local_index;
	}
	/* Check the second token, expected to be the anchor role. */
	if ((round_descriptor = strtok_r(NULL, ",", &save_descriptor))) {
		if (!strcmp("init", round_descriptor))
			round_configuration->role =
				CHERRY_FIRA_ANCHOR_ROLE_INITIATOR;
		else if (!strcmp("respf", round_descriptor))
			round_configuration->role =
				CHERRY_FIRA_ANCHOR_ROLE_RESPONDER;
		else {
			QLOGE("test %s", round_descriptor);
			QLOGE("Role can only be set as 'init' or 'respf'");
			return false;
		}
	}
	/* Check the third token, expected to be NULL if the role is RESPONDER
	or expected to be the list of the reponders' addresses if the role is INITIATOR. */
	round_descriptor = strtok_r(NULL, ",", &save_descriptor);
	if (round_configuration->role == CHERRY_FIRA_ANCHOR_ROLE_RESPONDER) {
		if (round_descriptor) {
			QLOGE("Too many parameters for this responder round descriptor."
			      "Check if the parameter respect the required pattern :"
			      "'index,respf index2,respf with ' ' between round, ',' between index and role and no address list.");
			return false;
		}
		return true;
	}
	if (round_descriptor) {
		/* Get the responders' addresses for this round. */
		if (!fetch_responders_addresses(round_descriptor, params,
						index_in_arg))
			return false;
	} else {
		QLOGE("Not enough parameters for this initiator round descriptor."
		      "Check if the parameter respect the required pattern :"
		      "'index,init,A1/A2/A3 index2,respf with ' ' between round, ','"
		      "between index, role and adress list and '/' between addresses.");
		return false;
	}
	/* Check the fourth token, expected to be NULL. */
	if ((round_descriptor = strtok_r(NULL, ",", &save_descriptor))) {
		QLOGE("Too many parameters for this round descriptor."
		      "Check if the parameter respect the required pattern :"
		      "'index,init,A1/A2/A3 index2,respf with ' ' between round, ','"
		      "between index, role and adress list and '/' between addresses.");
		return false;
	}
	return true;
}

/**
 * get_round_configurations - Get all the round configurations from a string pointer.
 * @optarg: Pointer to string containing the round configurations.
 * @params: Pointer to the parameters struct to fill.
 *
 * Parse through the string and fetch the configuration for each round described.
 *
 * Returns :
 * 	- True if succeed.
 *  - False for any error.
 */
static bool get_round_configurations(char *optarg,
				     struct app_dltdoa_parameter *params)
{
	/* This pointers allow us to loop through each the string */
	char *save_arg, *round_descriptor;
	int round_number = 0;
	char save_optarg[strlen(optarg) + 1];

	/* A round descriptor is defined as the sub string between two spaces. */

	/* Loop through the string once to check how many round descriptor there are.
	Then allocate the corresponding memory to the cherry_fira_anchor_round_config pointer. */
	strncpy(save_optarg, optarg, sizeof(save_optarg));
	params->light_conf.n_rounds = check_round(save_optarg);
	if (!params->light_conf.n_rounds) {
		QLOGE("At least one round is required.");
		return false;
	}
	if (!alloc_memory_round_config(params)) {
		QLOGE("Round configuration parameter could not be allocated.");
		return false;
	}

	/* Loop through all the round descriptors in the string. */
	round_descriptor = strtok_r(optarg, " ", &save_arg);
	while (round_descriptor != NULL) {
		/* For each round descriptor, get the cherry_fira_anchor_round_config provided. */
		if (!fetch_round_config(round_descriptor, params, round_number))
			return false;
		round_descriptor = strtok_r(NULL, " ", &save_arg);
		round_number++;
	}
	return true;
}

/**
 * apply_config - Apply parameters.
 * @params: Pointer to the struct app_dltdoa_parameter.
 * @light_conf: Pointer to the light DLTDOA configuration to apply.

 * Apply the DLTDOA config on the application parameters.
 */
static void apply_config(struct app_dltdoa_parameter *params,
			 const struct dl_tdoa_config_light *light_conf)
{
	params->light_conf = *light_conf;
}

/**
 * apply_config_multi - Apply parameters for a multi cluster configuration.
 * @params: Pointer to the struct app_dltdoa_parameter.
 *
 * Fetch parameters for the multi cluster configuration and set them in the struct app_dltdoa_parameter.
 */
static bool apply_config_multi(struct app_dltdoa_parameter *params)
{
	size_t max_index = sizeof(round_conf_7_clusters) /
			   sizeof(round_conf_7_clusters[0]);
	if (params->config_choice > max_index) {
		QLOGE("Config number chosen : %d is not available, configuration available goes from 1 to %d.",
		      params->config_choice, max_index);
		return false;
	}
	apply_config(params, &dltdoa_multi_configs[params->config_choice - 1]);
	return true;
}
#endif

/**
 * free_runtime_dltdoa_app_param - Free the memory allocated for parameters.
 * @params: Pointer to the struct app_dltdoa_parameter.
 *
 * Only free the round_config_custom if no pre defined configuration selected.
 * Otherwise no memory was allocated.
 */
void free_runtime_dltdoa_app_param(struct app_dltdoa_parameter *params)
{
#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
	if (params->light_conf.round_config_custom && !params->config_choice)
		qfree(params->light_conf.round_config_custom);
#endif
}

bool get_runtime_dltdoa_app_param(int argc, char *argv[],
				  struct app_dltdoa_parameter *params)
{
#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
	int opt;
	uint16_t index_conf = 0;

#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	/* On zephyr, getopt must be reseted between multiple calls. */
	getopt_init();
#endif
	while ((opt = getopt(argc, argv, OPTSTR)) != -1) {
		switch (opt) {
		case 't':
			params->is_anchor = false;
			break;
		case 'i':
			if (!util_convert_arg_to_uint32(optarg,
							&params->interval_ms)) {
				QLOGE("Invalid number for the ranging interval.");

				goto cleanup;
			}
			break;
		case 'l':
			if (!util_convert_arg_to_log_level(
				    optarg, &params->log_level)) {
				QLOGE("Invalid value for the log level.");
				goto cleanup;
			}
			break;
		case 'n':
			if (!util_convert_arg_to_uint16(
				    optarg, &params->max_nb_measurements)) {
				QLOGE("Invalid number for the number of measurement.");

				goto cleanup;
			}
			break;
		case 's':
			if (!util_convert_arg_to_uint16(
				    optarg, &params->slot_duration)) {
				QLOGE("Invalid value for slot duration.");

				goto cleanup;
			}
			break;
		case 'T':
			params->is_time_ref = true;
			break;
		case 'r':
			params->report_rssi = 0;
			break;
		case 'd':
			params->device = optarg;
			break;
		case 'v':
			if (!util_convert_arg_to_uint8(optarg,
						       &params->sts_config)) {
				QLOGE("Invalid number for the STS config.");

				goto cleanup;
			}
			break;
		case 'p':
			if (!util_convert_arg_to_uint8(
				    optarg, &params->preamble_code_index)) {
				QLOGE("Invalid number for the preamble index.");

				goto cleanup;
			}
			break;
		case 'c':
			if (params->light_conf.round_config_custom) {
				QLOGE("A round configuration as already been set using -R and -S options"
				      ", you cannot use a pre defined one now.");
				goto cleanup;
			}
			if (!util_convert_arg_to_uint8(
				    optarg, &params->config_choice)) {
				QLOGE("Invalid value for the config choice.");
				goto cleanup;
			}
			if (!params->config_choice) {
				QLOGE("Config number chosen : %d is not available, configuration's numbers start at 1.",
				      params->config_choice);
				goto cleanup;
			}
			break;
		case 'x':
			if (!util_convert_arg_to_uint8(optarg,
						       &params->slots_per_rr)) {
				QLOGE("Invalid number for the slots per round.");

				goto cleanup;
			}
			break;
		case 'a':
			if (!util_convert_arg_to_uint16(
				    optarg,
				    &params->light_conf.device_mac_address)) {
				QLOGE("Invalid value for the device mac address.");

				goto cleanup;
			}
			break;
		case 'R': {
			if (params->config_choice) {
				QLOGE("A pre defined round configuration was already selected,"
				      "it is not modifiable with -R option.");
				goto cleanup;
			}
			if (!get_round_configurations(optarg, params)) {
				goto cleanup;
			}
			break;
		}
		case 'I': {
			int n_index = 0;
			char *index;
			if (params->is_anchor) {
				QLOGE("Only set the list of index for the tag and not the anchor.");
				goto cleanup;
			}
			if (params->config_choice) {
				QLOGE("Only select a config for anchors, not for tags.");
				goto cleanup;
			}
			index = strtok(optarg, " ");
			while (index != NULL) {
				if (n_index == MAX_NB_INDEX) {
					QLOGE("Invalid number of index, maximum is %d.",
					      MAX_NB_INDEX);

					goto cleanup;
				};
				if (!util_convert_arg_to_uint8(
					    index,
					    &params->index_tag[n_index])) {
					QLOGE("Invalid index. Check if the parameters respect the required pattern :"
					      "'I1 I2 I3' with spaces between indexes");

					goto cleanup;
				}
				n_index++;
				index = strtok(NULL, " ");
			}
			params->light_conf.n_rounds = n_index;
			break;
		}
		case 'H':
			if (index_conf) {
				QLOGE("It is not possible to set a HPRF and BPRF set up at the same time.");
				goto cleanup;
			}
			if (!util_convert_arg_to_uint16(optarg, &index_conf)) {
				QLOGE("Invalid value for the HPRF config");
				goto cleanup;
			}
			if (index_conf == 0 || index_conf > MAX_HPRF_SET_ID) {
				QLOGE("Unaccepcted value for the HPRF config, must be between 1 and %d.",
				      MAX_HPRF_SET_ID);
				goto cleanup;
			}
			params->phy_params = &hprf_settings[index_conf - 1];
			break;
		case 'B':
			if (index_conf) {
				QLOGE("It is not possible to set a HPRF and BPRF set up at the same time.");
				goto cleanup;
			}
			if (!util_convert_arg_to_uint16(optarg, &index_conf)) {
				QLOGE("Invalid value for the BPRF config");
				goto cleanup;
			}
			if (index_conf == 0 || index_conf > MAX_BPRF_SET_ID) {
				QLOGE("Unaccepcted value for the BPRF config, must be between 1 and %d.",
				      MAX_BPRF_SET_ID);
				goto cleanup;
			}
			params->phy_params = &bprf_settings[index_conf - 1];
			break;
		case 'A':
			params->tx_active_rr = true;
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
			dl_tdoa_app_usage();
			goto cleanup;
		}
	}
	if (params->config_choice) {
		if (!apply_config_multi(params))
			goto cleanup;
	}
#endif
	return true;
#ifdef CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT
cleanup:
	free_runtime_dltdoa_app_param(params);
	return false;
#endif
}
