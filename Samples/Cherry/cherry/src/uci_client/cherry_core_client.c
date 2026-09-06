/*
 * Implemention of uci core client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#include "cherry_core_client.h"

#include "cherry/cherry.h"
#include "cherry/cherry_ccc.h"
#include "cherry/cherry_fira.h"
#include "cherry/cherry_radar.h"
#define LOG_TAG "cherry_uci_client"
#include "cherry_log.h"

#include <inttypes.h>
#include <qmalloc.h>
#include <qsemaphore.h>
#include <qtils.h>
#include <stdio.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>

#define UCI_GPIO_TOGGLE_TIMESTAMP_RSP_SIZE 13

struct cherry_core_context {
	struct uci *uci;
	cherry_uci_client_core_device_status_cb_t device_status_cb;
	cherry_uci_client_core_boot_cb_t boot_cb;
	void *user_data;
	int response_status;
	struct qsemaphore *sem_response;
	void *cmd_data;
};

static inline int cherry_core_wait_rsp(struct cherry_core_context *context)
{
	int r = qsemaphore_take(context->sem_response, 1000);

	/* Error or timeout. */
	if (r)
		return 0;

	/* Completed in time. */
	return 1;
}

static inline void cherry_core_set_rsp(struct cherry_core_context *context)
{
	qsemaphore_give(context->sem_response);
}

static enum qerr uci_reset_rsp_handler(struct uci *uci, uint16_t mt_gid_oid,
				       const struct uci_blk *payload,
				       void *user_data)
{
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	struct cherry_core_context *context = user_data;

	if (uci_message_remaining(&parser) < 1)
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
	else
		context->response_status = uci_message_get_8bit(&parser);

	cherry_core_set_rsp(context);

	return QERR_SUCCESS;
}

static enum qerr uci_device_status_handler(struct uci *uci, uint16_t mt_gid_oid,
					   const struct uci_blk *payload,
					   void *user_data)
{
	struct cherry_core_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	enum uci_device_state state;

	if (uci_message_remaining(&parser) < 1) {
		state = UCI_DEVICE_STATE_ERROR;
	} else {
		state = uci_message_get_8bit(&parser);
	}

	if (context->device_status_cb)
		context->device_status_cb(state, context->user_data);

	return QERR_SUCCESS;
}

static enum qerr uci_boot_handler(struct uci *uci, uint16_t mt_gid_oid,
				  const struct uci_blk *payload,
				  void *user_data)
{
	struct cherry_core_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	enum uci_qorvo_boot_reason reason;

	if (uci_message_remaining(&parser) < 1) {
		reason = UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET;
	} else {
		reason = uci_message_get_8bit(&parser);
	}

	if (context->boot_cb)
		context->boot_cb(reason, context->user_data);

	return QERR_SUCCESS;
}

static enum qerr uci_rsp_handler_get_device_info(struct uci *uci,
						 uint16_t mt_gid_oid,
						 const struct uci_blk *payload,
						 void *user_data)
{
	struct cherry_core_context *context = user_data;
	struct cherry_core_event_device_info *cherry_device_info =
		context->cmd_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	uint8_t vendor_specific_length;

	/* Message must at least contain generic device info. */
	if (uci_message_remaining(&parser) < 10) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto err;
	}

	context->response_status = uci_message_get_8bit(&parser);

	cherry_device_info->uci_version = uci_message_get_16bit(&parser);
	cherry_device_info->mac_version = uci_message_get_16bit(&parser);
	cherry_device_info->phy_version = uci_message_get_16bit(&parser);
	cherry_device_info->uci_test_version = uci_message_get_16bit(&parser);
	vendor_specific_length = uci_message_get_8bit(&parser);
	if (uci_message_remaining(&parser) != (size_t)vendor_specific_length) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto err;
	}

	if (vendor_specific_length >= 52) {
		/* Vendor data format provided by qm-firmware. */
		uint8_t fw_major = uci_message_get_8bit(&parser);
		uint8_t fw_minor = uci_message_get_8bit(&parser);
		uint8_t fw_patch = uci_message_get_8bit(&parser);
		uint8_t fw_rc = uci_message_get_8bit(&parser);
		uint64_t build_job = uci_message_get_64bit(&parser);
		snprintf(cherry_device_info->fw_version,
			 CHERRY_DEV_INFO_FW_VERSION_SIZE,
			 "%u.%u.%urc%u_%" PRIu64, fw_major, fw_minor, fw_patch,
			 fw_rc, build_job);
		/* Skip OEM version. */
		uci_message_skip(&parser, 1);
		uci_message_skip(&parser, 1);
		uci_message_skip(&parser, 1);
		for (int i = 0; i < CHERRY_DEV_INFO_SOC_ID_LEN; i++)
			cherry_device_info->soc_id[i] =
				uci_message_get_8bit(&parser);
		cherry_device_info->device_id = uci_message_get_32bit(&parser);
		cherry_device_info->package_id = uci_message_get_8bit(&parser);
		if (vendor_specific_length >= 76) {
			for (int i = 0; i < CHERRY_DEV_INFO_FLAVOR_LEN; i++)
				cherry_device_info->flavor[i] =
					uci_message_get_8bit(&parser);
		}
		if (vendor_specific_length >= 80) {
			cherry_device_info->product_id =
				uci_message_get_32bit(&parser);
		}
		if (vendor_specific_length >= 84) {
			cherry_device_info->soi_variant =
				uci_message_get_32bit(&parser);
		}
		if (vendor_specific_length >= 86) {
			cherry_device_info->rom_revision =
				uci_message_get_16bit(&parser);
		}
	} else {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		QLOGE("Unsupported vendor_data format provided");
	}
err:
	cherry_core_set_rsp(context);

	return QERR_SUCCESS;
}

static void parse_fira_capabilities(struct uci_message_parser *parser,
				    struct cherry_fira_capabilities *fira_caps,
				    uint8_t type, uint8_t length)
{
	switch (type) {
	case UCI_CAP_MAX_DATA_MESSAGE_SIZE:
		fira_caps->max_data_message_size =
			uci_message_get_16bit(parser);
		break;
	case UCI_CAP_MAX_DATA_PACKET_PAYLOAD_SIZE:
		fira_caps->max_data_packet_payload_size =
			uci_message_get_16bit(parser);
		break;
	case UCI_CAP_FIRA_PHY_VERSION_RANGE:
		fira_caps->phy_version_range = uci_message_get_32bit(parser);
		break;
	case UCI_CAP_FIRA_MAC_VERSION_RANGE:
		fira_caps->mac_version_range = uci_message_get_32bit(parser);
		break;
	case UCI_CAP_DEVICE_TYPES:
		fira_caps->device_type = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_DEVICE_ROLES:
		fira_caps->device_roles = uci_message_get_16bit(parser);
		break;
	case UCI_CAP_RANGING_METHOD:
		fira_caps->ranging_methods = uci_message_get_16bit(parser);
		break;
	case UCI_CAP_STS_CONFIG:
		fira_caps->sts_config = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_MULTI_NODE_MODE:
		fira_caps->multi_node_mode = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_RANGING_TIME_STRUCT:
		fira_caps->ranging_time_struct = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_SCHEDULE_MODE:
		fira_caps->schedule_mode = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_HOPPING_MODE:
		fira_caps->hopping_mode = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_BLOCK_STRIDING:
		fira_caps->block_striding = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_UWB_INITIATION_TIME:
		fira_caps->uwb_initiation_time = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_CHANNELS:
		fira_caps->channels = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_RFRAME_CONFIG:
		fira_caps->rframe_config = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_CC_CONSTRAINT_LENGTH:
		fira_caps->cc_constraint_length = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_BPRF_PARAMETER_SETS:
		fira_caps->bprf_parameter_sets = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_HPRF_PARAMETER_SETS: {
		uint32_t first_block = uci_message_get_32bit(parser);
		uint8_t second_block = uci_message_get_8bit(parser);
		fira_caps->hprf_parameter_sets =
			first_block + (second_block << 4);
		break;
	}
	case UCI_CAP_AOA_SUPPORT:
		fira_caps->aoa_support = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_EXTENDED_MAC_ADDRESS:
		fira_caps->extended_mac_address = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_SESSION_KEY_LENGTH:
		fira_caps->session_key_length = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_DT_ANCHOR_MAX_ACTIVE_RR:
		fira_caps->dt_anchor_max_active_rr =
			uci_message_get_8bit(parser);
		break;
	case UCI_CAP_DT_TAG_MAX_ACTIVE_RR:
		fira_caps->dt_tag_max_active_rr = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_DT_TAG_BLOCK_SKIPPING:
		fira_caps->dt_tag_block_skipping = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_PSDU_LENGTH_SUPPORT:
		fira_caps->psdu_length_support = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_LL_CAPABILITY_PARAM:
		fira_caps->ll_capability_params = uci_message_get_16bit(parser);
		break;
	case UCI_CAP_BYPASS_MODE_SUPPORT:
		fira_caps->bypass_mode_support = uci_message_get_8bit(parser);
		break;
	default:
		QLOGD("Unknown FiRa capability type %#02x, length %d.", type,
		      length);
		uci_message_skip(parser, length);
		break;
	}
}

static bool parse_ccc_capabilities(struct uci_message_parser *parser,
				   struct cherry_ccc_capabilities *ccc_caps,
				   uint8_t type, uint8_t length)
{
	switch (type) {
	case UCI_CAP_CCC_SLOT_BITMASK:
		ccc_caps->slot_bitmask = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_CCC_SYNC_CODE_INDEX_BITMASK:
		ccc_caps->sync_code_index_bitmask =
			uci_message_get_32bit(parser);
		break;
	case UCI_CAP_CCC_HOPPING_CONFIG_BITMASK:
		ccc_caps->hopping_config_bitmask = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_CCC_CHANNEL_BITMASK:
		ccc_caps->channel_bitmask = uci_message_get_8bit(parser);
		break;
	case UCI_CAP_CCC_SUPPORTED_PROTOCOL_VERSION: {
		int nb_element = length / 2;
		ccc_caps->protocol_versions.len = nb_element;
		ccc_caps->protocol_versions.items =
			(uint16_t *)qmalloc(sizeof(uint16_t) * nb_element);
		if (!ccc_caps->protocol_versions.items) {
			QLOGE("Failed to allocate memory for protocol versions.");
			return false;
		}
		for (uint8_t i = 0; i < nb_element; i++) {
			ccc_caps->protocol_versions.items[i] =
				uci_message_get_16bit(parser);
		}
		break;
	}
	case UCI_CAP_CCC_SUPPORTED_UWB_CONFIG_ID: {
		int nb_element = length / 2;
		ccc_caps->uwb_configs.len = nb_element;
		ccc_caps->uwb_configs.items =
			(uint16_t *)qmalloc(sizeof(uint16_t) * nb_element);
		if (!ccc_caps->uwb_configs.items) {
			QLOGE("Failed to allocate memory for uwb configs.");
			return false;
		}
		for (uint8_t i = 0; i < nb_element; i++) {
			ccc_caps->uwb_configs.items[i] =
				uci_message_get_16bit(parser);
		}
		break;
	}
	case UCI_CAP_CCC_SUPPORTED_PULSE_SHAPE_COMBO:
		ccc_caps->pulse_shape_combos.len = length;
		ccc_caps->pulse_shape_combos.items =
			(uint8_t *)qmalloc(sizeof(uint8_t) * length);
		if (!ccc_caps->pulse_shape_combos.items) {
			QLOGE("Failed to allocate memory for pulse shape combos.");
			return false;
		}
		for (uint8_t i = 0; i < length; i++) {
			ccc_caps->pulse_shape_combos.items[i] =
				uci_message_get_8bit(parser);
		}
		break;
	case UCI_CAP_CCC_MINIMUM_RAN_MULTIPLIER:
		ccc_caps->minimum_ran_multiplier = uci_message_get_8bit(parser);
		break;
	default:
		QLOGD("Unknown CCC capability type %#02x, length %d.", type,
		      length);
		uci_message_skip(parser, length);
		break;
	}
	return true;
}

static void
parse_radar_capabilities(struct uci_message_parser *parser,
			 struct cherry_radar_capabilities *radar_caps,
			 uint8_t type, uint8_t length)
{
	radar_caps->support = uci_message_get_8bit(parser);
}

static bool alloc_fira_capabilities(
	struct cherry_core_event_device_capabilities *device_caps)
{
	device_caps->fira_capabilities =
		(struct cherry_fira_capabilities *)qmalloc(
			sizeof(struct cherry_fira_capabilities));
	if (!device_caps->fira_capabilities) {
		QLOGE("Failed to allocate FiRa capabilities.");
		return false;
	}

	return true;
}

static bool alloc_ccc_capabilities(
	struct cherry_core_event_device_capabilities *device_caps)
{
	if (!device_caps->ccc_capabilities) {
		device_caps->ccc_capabilities =
			(struct cherry_ccc_capabilities *)qmalloc(
				sizeof(struct cherry_ccc_capabilities));
		if (!device_caps->ccc_capabilities) {
			QLOGE("Failed to allocate CCC capabilities.");
			return false;
		}
		device_caps->ccc_capabilities->pulse_shape_combos.items = NULL;
		device_caps->ccc_capabilities->pulse_shape_combos.len = 0;
		device_caps->ccc_capabilities->uwb_configs.items = NULL;
		device_caps->ccc_capabilities->uwb_configs.len = 0;
		device_caps->ccc_capabilities->protocol_versions.items = NULL;
		device_caps->ccc_capabilities->protocol_versions.len = 0;
	}
	return true;
}

static bool alloc_radar_capabilities(
	struct cherry_core_event_device_capabilities *device_caps)
{
	if (!device_caps->radar_capabilities) {
		device_caps->radar_capabilities =
			(struct cherry_radar_capabilities *)qmalloc(
				sizeof(struct cherry_radar_capabilities));
		if (!device_caps->radar_capabilities) {
			QLOGE("Failed to allocate radar capabilities.");
			return false;
		}
	}
	return true;
}

static enum qerr
uci_rsp_handler_get_capabilities_info(struct uci *uci, uint16_t mt_gid_oid,
				      const struct uci_blk *payload,
				      void *user_data)
{
	struct cherry_core_context *context = user_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	struct cherry_core_event_device_capabilities *device_caps =
		context->cmd_data;
	size_t remaining, parsed_length;

	uint8_t total_number;
	int params_count = 0;

	if (uci_message_remaining(&parser) < 2) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto err;
	}

	context->response_status = uci_message_get_8bit(&parser);
	total_number = uci_message_get_8bit(&parser);

	if (context->response_status != UCI_STATUS_OK)
		goto err;

	while (uci_message_remaining(&parser) >= 3) {
		uint8_t type = uci_message_get_8bit(&parser);
		uint8_t length = uci_message_get_8bit(&parser);
		remaining = uci_message_remaining(&parser);

		if (uci_message_remaining(&parser) < length) {
			context->response_status = UCI_STATUS_SYNTAX_ERROR;
			goto err;
		}

		if (type <= UCI_CAP_BYPASS_MODE_SUPPORT) {
			if (!device_caps->fira_capabilities) {
				if (!alloc_fira_capabilities(device_caps)) {
					context->response_status =
						UCI_STATUS_UNKNOWN;
					goto err;
				}
			}
			parse_fira_capabilities(&parser,
						device_caps->fira_capabilities,
						type, length);
		} else if ((UCI_CAP_CCC_SLOT_BITMASK <= type) &&
			   (type <= UCI_CAP_CCC_MINIMUM_RAN_MULTIPLIER)) {
			if (!device_caps->ccc_capabilities) {
				if (!alloc_ccc_capabilities(device_caps)) {
					context->response_status =
						UCI_STATUS_UNKNOWN;
					goto err;
				}
			}
			if (!parse_ccc_capabilities(
				    &parser, device_caps->ccc_capabilities,
				    type, length)) {
				context->response_status = UCI_STATUS_UNKNOWN;
				goto err;
			}
		} else if (type == UCI_CAP_ANDROID_RADAR_SUPPORT) {
			if (!device_caps->radar_capabilities) {
				if (!alloc_radar_capabilities(device_caps)) {
					context->response_status =
						UCI_STATUS_UNKNOWN;
					goto err;
				}
			}
			parse_radar_capabilities(
				&parser, device_caps->radar_capabilities, type,
				length);
		} else {
			QLOGD("Unexpected capability: identifier %#02x, length %d.",
			      type, length);
			uci_message_skip(&parser, length);
		}

		parsed_length = remaining - uci_message_remaining(&parser);
		if (length != parsed_length) {
			QLOGE("Error reading capability type %#x, parsed length %u/%u.",
			      type, parsed_length, length);
			context->response_status = UCI_STATUS_UNKNOWN;
			goto err;
		}

		params_count++;
	}

	if (params_count != total_number) {
		QLOGE("Only %u/%u capabilities have been received.",
		      params_count, total_number);
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
	}
err:
	cherry_core_set_rsp(context);

	return QERR_SUCCESS;
}

static enum qerr
uci_rsp_handler_get_uwb_device_stats(struct uci *uci, uint16_t mt_gid_oid,
				     const struct uci_blk *payload,
				     void *user_data)
{
	struct cherry_core_context *context = user_data;
	struct cherry_core_event_device_stats *stats = context->cmd_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	/* Message must at least contain device stats. */
	if (uci_message_remaining(&parser) < 3) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto err;
	}

	context->response_status = uci_message_get_8bit(&parser);

	stats->temperature_100th_celsius = uci_message_get_16bit(&parser);

err:
	cherry_core_set_rsp(context);

	return QERR_SUCCESS;
}

static enum qerr uci_rsp_handler_get_config(struct uci *uci,
					    uint16_t mt_gid_oid,
					    const struct uci_blk *payload,
					    void *user_data)
{
	struct cherry_core_context *context = user_data;
	enum uci_device_state *uwbs_state = context->cmd_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);
	uint8_t nb_parameter;
	uint8_t type;
	uint8_t length;

	/* Message must at least contain status. */
	if (uci_message_remaining(&parser) < 1) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto err;
	}

	context->response_status = uci_message_get_8bit(&parser);

	if (context->response_status != UCI_STATUS_OK)
		goto err;

	nb_parameter = uci_message_get_8bit(&parser);

	for (; nb_parameter > 0; --nb_parameter) {
		if (!uci_message_get_8bit_no_assert(&parser, &type) ||
		    !uci_message_get_8bit_no_assert(&parser, &length)) {
			context->response_status = UCI_STATUS_SYNTAX_ERROR;
			goto err;
		}

		/* Only Device state is processed. */
		switch (type) {
		case UCI_DEVICE_PARAMETER_DEVICE_STATE:
			if (!uci_message_get_8bit_no_assert(
				    &parser, (uint8_t *)uwbs_state)) {
				context->response_status =
					UCI_STATUS_SYNTAX_ERROR;
				goto err;
			}
			break;
		default:
			if (length)
				uci_message_skip(&parser, length);
			break;
		}
	}

err:
	cherry_core_set_rsp(context);

	return QERR_SUCCESS;
}

static enum qerr
uci_rsp_handler_get_device_timestamp(struct uci *uci, uint16_t mt_gid_oid,
				     const struct uci_blk *payload,
				     void *user_data)
{
	struct cherry_core_context *context = user_data;
	struct cherry_core_event_device_timestamp *timestamp =
		context->cmd_data;
	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	/* Message must contain status and device timestamp. */
	if (uci_message_remaining(&parser) < 9) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto err;
	}

	context->response_status = uci_message_get_8bit(&parser);
	timestamp->timestamp_us = uci_message_get_64bit(&parser);

err:
	cherry_core_set_rsp(context);

	return QERR_SUCCESS;
}

static enum qerr uci_rsp_handler_toggle_gpio_timesync_handler(
	struct uci *uci, uint16_t mt_gid_oid, const struct uci_blk *payload,
	void *user_data)
{
	struct cherry_core_context *context = user_data;
	struct cherry_core_event_gpio_toggle *gpio_toggle_timestamp =
		context->cmd_data;

	struct uci_message_parser parser =
		UCI_MESSAGE_PARSER_INITIALIZER(payload);

	/* Message must contain 1 byte for status, 8 bytes for timestamp followed by 4 RFUs bytes. */
	if (uci_message_remaining(&parser) <
	    UCI_GPIO_TOGGLE_TIMESTAMP_RSP_SIZE) {
		context->response_status = UCI_STATUS_SYNTAX_ERROR;
		goto err;
	}

	context->response_status = uci_message_get_8bit(&parser);
	gpio_toggle_timestamp->timestamp_us = uci_message_get_64bit(&parser);

	uci_message_skip(&parser, 4); /* Skip RFU bytes. */

err:
	cherry_core_set_rsp(context);

	return QERR_SUCCESS;
}

/* Handlers in table need to have their OID's ordered ascending. */
static const struct uci_message_handler uci_rsp_core_handlers[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET),
		.handler = uci_reset_rsp_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_STATUS),
		.handler = uci_device_status_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_DEVICE_INFO),
		.handler = uci_rsp_handler_get_device_info,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_CAPS_INFO),
		.handler = uci_rsp_handler_get_capabilities_info,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_CONFIG),
		.handler = uci_rsp_handler_get_config,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_RESPONSE,
					     UCI_GID_CORE,
					     UCI_OID_CORE_QUERY_UWBS_TIMESTAMP),
		.handler = uci_rsp_handler_get_device_timestamp,
	},
};

static struct uci_message_handlers uci_rsp_core_handlers_list = {
	.next = NULL,
	.handlers = uci_rsp_core_handlers,
	.n_handlers = qarray_size(uci_rsp_core_handlers),
	.user_data = NULL,
};

static const struct uci_message_handler uci_qorvo_cmd_handlers[] = {
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_GET_DEVICE_STATS),
		.handler = uci_rsp_handler_get_uwb_device_stats,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					     UCI_GID_QORVO_EXT2,
					     UCI_OID_QORVO_CORE_DEVICE_BOOT),
		.handler = uci_boot_handler,
	},
	{
		.mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_QORVO_EXT2,
			UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC),
		.handler = uci_rsp_handler_toggle_gpio_timesync_handler,
	},
};

static struct uci_message_handlers uci_rsp_qorvo_handlers_list = {
	.next = NULL,
	.handlers = uci_qorvo_cmd_handlers,
	.n_handlers = qarray_size(uci_qorvo_cmd_handlers),
	.user_data = NULL,
};

enum qerr cherry_uci_client_core_open(
	struct cherry_core_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_core_device_status_cb_t device_status_cb,
	cherry_uci_client_core_boot_cb_t boot_cb)

{
	if (!device_status_cb || !boot_cb)
		return QERR_EINVAL;

	*context = (struct cherry_core_context *)qmalloc(
		sizeof(struct cherry_core_context));

	if (!context)
		return QERR_EINVAL;

	(*context)->uci = uci;
	(*context)->user_data = user_data;
	(*context)->device_status_cb = device_status_cb;
	(*context)->boot_cb = boot_cb;

	(*context)->sem_response = qsemaphore_init(0, 1);

	/* setup core handlers */
	uci_rsp_core_handlers_list.user_data = (*context);
	uci_message_handlers_register((*context)->uci,
				      &uci_rsp_core_handlers_list);

	/* setup qorvo handlers */
	uci_rsp_qorvo_handlers_list.user_data = (*context);
	uci_message_handlers_register((*context)->uci,
				      &uci_rsp_qorvo_handlers_list);

	return QERR_SUCCESS;
}

void cherry_uci_client_core_close(struct cherry_core_context *context)
{
	if (!context)
		return;

	context->user_data = NULL;
	qsemaphore_deinit(context->sem_response);

	uci_rsp_core_handlers_list.user_data = NULL;
	uci_message_handlers_unregister(context->uci,
					&uci_rsp_core_handlers_list);

	uci_rsp_qorvo_handlers_list.user_data = NULL;
	uci_message_handlers_unregister(context->uci,
					&uci_rsp_qorvo_handlers_list);
	qfree(context);
}

enum uci_status_code
cherry_uci_client_core_device_reset(struct cherry_core_context *context,
				    uint8_t reset)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_DEVICE_RESET);
	struct uci_message_builder builder;
	int ret;

	if (!context) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_8bit(&builder, reset);
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_core_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code cherry_uci_client_core_get_device_info(
	struct cherry_core_context *context,
	struct cherry_core_event_device_info *device_info)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_DEVICE_INFO);
	struct uci_message_builder builder;
	int ret;

	if (!context || !device_info) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	context->cmd_data = device_info;
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_core_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code cherry_uci_client_core_get_capabilities(
	struct cherry_core_context *context,
	struct cherry_core_event_device_capabilities *device_caps)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_CAPS_INFO);
	struct uci_message_builder builder;
	int ret;

	if (!context || !device_caps) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	context->cmd_data = device_caps;

	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_core_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code cherry_uci_client_core_get_uwb_device_stats(
	struct cherry_core_context *context,
	struct cherry_core_event_device_stats *stats)
{
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_QORVO_EXT2,
			       UCI_OID_QORVO_CORE_GET_DEVICE_STATS);
	struct uci_message_builder builder;
	int ret;

	if (!context || !stats) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	context->cmd_data = stats;
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_core_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code cherry_uci_client_core_get_device_timestamp(
	struct cherry_core_context *context,
	struct cherry_core_event_device_timestamp *device_timestamp)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_QUERY_UWBS_TIMESTAMP);
	struct uci_message_builder builder;
	int ret;

	if (!context || !device_timestamp) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	context->cmd_data = device_timestamp;
	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_core_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code
cherry_uci_client_core_get_uwbs_state(struct cherry_core_context *context,
				      enum uci_device_state *uwbs_state)
{
	uint16_t mt_gid_oid = UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
					     UCI_GID_CORE,
					     UCI_OID_CORE_GET_CONFIG);
	struct uci_message_builder builder;
	int ret;

	if (!context || !uwbs_state) {
		return UCI_STATUS_INVALID_PARAM;
	}

	/* Initialize the state to be sure to return coherent data. */
	*uwbs_state = 0;
	context->cmd_data = uwbs_state;
	uci_message_builder_init(&builder, context->uci);
	uci_message_put_8bit(&builder, 1);
	uci_message_put_8bit(&builder, UCI_DEVICE_PARAMETER_DEVICE_STATE);

	/* Send message */
	uci_send_message(context->uci, mt_gid_oid, builder.message);
	/* Wait for response */
	ret = cherry_core_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

enum uci_status_code cherry_uci_client_core_set_gpio_toggle_mode(
	struct cherry_core_context *context, uint8_t mode,
	struct cherry_core_event_gpio_toggle *gpio_toggle)
{
	uint16_t mt_gid_oid =
		UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND, UCI_GID_QORVO_EXT2,
			       UCI_OID_QORVO_CORE_TOGGLE_GPIO_TIMESYNC);
	struct uci_message_builder builder;
	int ret;

	if (!context || !gpio_toggle) {
		return UCI_STATUS_INVALID_PARAM;
	}

	uci_message_builder_init(&builder, context->uci);
	uci_message_put_8bit(&builder, mode);
	context->cmd_data = gpio_toggle;

	/* Send message. */
	uci_send_message(context->uci, mt_gid_oid, builder.message);

	/* Wait for response. */
	ret = cherry_core_wait_rsp(context);

	return ret ? context->response_status : UCI_STATUS_UCI_MESSAGE_RETRY;
}

void cherry_uci_client_core_capabilities_free(
	struct cherry_core_event_device_capabilities *device_caps)
{
	if (!device_caps)
		return;
	if (device_caps->ccc_capabilities) {
		if (device_caps->ccc_capabilities->pulse_shape_combos.items)
			qfree(device_caps->ccc_capabilities->pulse_shape_combos
				      .items);
		if (device_caps->ccc_capabilities->uwb_configs.items)
			qfree(device_caps->ccc_capabilities->uwb_configs.items);
		if (device_caps->ccc_capabilities->protocol_versions.items)
			qfree(device_caps->ccc_capabilities->protocol_versions
				      .items);
		qfree(device_caps->ccc_capabilities);
	}
	if (device_caps->fira_capabilities)
		qfree(device_caps->fira_capabilities);
	if (device_caps->radar_capabilities)
		qfree(device_caps->radar_capabilities);
}
