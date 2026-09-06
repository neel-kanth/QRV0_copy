/*
 * Header file for uci core client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_CORE_CLIENT_H
#define CHERRY_CORE_CLIENT_H

#include <cherry/cherry.h>
#include <qerr.h>
#include <uci/uci.h>

#define CHERRY_DEV_INFO_FW_VERSION_SIZE 32

/*
 * struct fira_capabilities - FiRa capabilities
 *
 * This structure contains the FiRa capabilities as reported by the MAC
 */
struct fira_capabilities {
#define F(name) bool has_##name

#define P(name, type) \
	F(name);      \
	type name

	P(fira_phy_version_range, uint32_t);
	P(fira_mac_version_range, uint32_t);
	P(device_class, uint8_t);
	F(device_type_controlee_responder);
	F(device_type_controlee_initiator);
	F(device_type_controller_responder);
	F(device_type_controller_initiator);
	F(device_type_controller);
	F(device_type_controlee);
	F(device_role_responder);
	F(device_role_initiator);
	F(device_role_ut_sync_anchor);
	F(device_role_ut_anchor);
	F(device_role_ut_tag);
	F(device_role_advertiser);
	F(device_role_observer);
	F(device_role_dt_anchor);
	F(device_role_dt_tag);
	F(ranging_time_struct_block_based_scheduling);
	F(multi_node_mode_unicast);
	F(multi_node_mode_one_to_many);
	F(multi_node_mode_many_to_many);
	F(ranging_round_usage_ss_twr);
	F(ranging_round_usage_ds_twr);
	F(ranging_method_ss_twr_deferred);
	F(ranging_method_ss_twr_non_deferred);
	F(ranging_method_ds_twr_deferred);
	F(ranging_method_ds_twr_non_deferred);
	F(ranging_method_owr_dl_tdoa);
	F(ranging_method_owr_aoa);
	F(ranging_method_ess_twr_non_deferred_for_cbr);
	P(number_of_controlees_max, uint32_t);
	/* Behaviour. */
	F(schedule_mode_contention_based);
	F(schedule_mode_time_scheduled);
	F(schedule_mode_hybrid);
	F(round_hopping);
	F(block_striding);
	F(uwb_initiation_time);
	F(extended_mac_address);
	F(suspend_ranging);
	/* Radio. */
	P(channel_number, uint16_t);
	F(rframe_config_sp0);
	F(rframe_config_sp1);
	F(rframe_config_sp3);
	F(convolutional_encoding_systematic);
	F(convolutional_encoding_non_systematic);
	F(prf_mode_bprf);
	F(prf_mode_hprf);
	F(preamble_duration_64);
	F(preamble_duration_32);
	F(sfd_id_0);
	F(sfd_id_1);
	F(sfd_id_2);
	F(sfd_id_3);
	F(sfd_id_4);
	F(number_of_sts_segments_0);
	F(number_of_sts_segments_1);
	F(number_of_sts_segments_2);
	F(number_of_sts_segments_3);
	F(number_of_sts_segments_4);
	F(psdu_data_rate_6m81);
	F(psdu_data_rate_7m80);
	F(psdu_data_rate_27m2);
	F(psdu_data_rate_31m2);
	F(bprf_phr_data_rate_850k);
	F(bprf_phr_data_rate_6m81);
	F(mac_fcs_type_crc32);
	F(tx_adaptive_payload_power);
	/* Antenna. */
	P(rx_antenna_pairs, uint32_t);
	P(tx_antennas, uint32_t);
	/* STS and crypto capabilities. */
	F(sts_static);
	F(sts_dynamic);
	F(sts_dynamic_individual_key);
	F(sts_provisioned);
	F(sts_provisioned_individual_key);
	P(session_dynamic_key_lengths, uint8_t);
	P(session_provisioned_key_lengths, uint8_t);
	F(ppdu_format_sp0);
	F(ppdu_format_sp1);
	F(ppdu_format_sp3);
	F(sts_segment_length_32);
	F(sts_segment_length_64);
	F(sts_segment_length_128);
	/* Report. */
	F(aoa_azimuth);
	F(aoa_azimuth_full);
	F(aoa_elevation);
	F(aoa_fom);
	/* Specific for DL-TDoA. */
	P(dt_anchor_max_active_rr, uint8_t);
	P(dt_tag_max_active_rr, uint8_t);
	P(dt_tag_block_skipping, uint8_t);

#undef P
#undef F
};

/**
 * typedef cherry_uci_client_core_device_status_cb_t - Type for device state NTF callback.
 * @new_state: The new state of the device.
 * @user_data: User data pointer given to cherry_uci_client_core_open.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_core_device_status_cb_t)(
	const enum uci_device_state new_state, void *user_data);

/**
 * typedef cherry_uci_client_core_boot_cb_t - Type for boot NTF callback.
 * @reason: The new state of the device.
 * @user_data: User data pointer given to core_client_open.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_core_boot_cb_t)(
	const enum uci_qorvo_boot_reason reason, void *user_data);

/*
 * typedef struct cherry_core_context - Core client context
 *
 */
struct cherry_core_context;

/**
 * cherry_uci_client_core_open() - Initialize the internal resources of the client
 *
 * @context: Core context to initialize
 * @uci: UCI context to use for serialization
 * @user_data: Application context to pass along for the callback
 * @device_status_cb: Callback to use to notify each device status NTF.
 * @boot_cb: Callback to use to notify each boot NTF.
 *
 * Return: QERR_SUCCESS or error
 */
enum qerr cherry_uci_client_core_open(
	struct cherry_core_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_core_device_status_cb_t device_status_cb,
	cherry_uci_client_core_boot_cb_t boot_cb);

/**
 * cherry_uci_client_core_close() - Free all internal resources of the client
 *
 * @context: Core context to free
 *
 */
void cherry_uci_client_core_close(struct cherry_core_context *context);

/**
 * cherry_uci_client_core_device_reset() - Reset the device
 * @context: FiRa context
 * @reset: Device Reset
 *
 * Return: UCI_STATUS_OK or error
 */
enum uci_status_code
cherry_uci_client_core_device_reset(struct cherry_core_context *context,
				    uint8_t reset);

/**
 * cherry_uci_client_core_get_device_info() - Get the device information.
 * @context: FiRa context.
 * @device_info: FiRa device info
 *
 * Return: UCI_STATUS_OK or error
 */
enum uci_status_code cherry_uci_client_core_get_device_info(
	struct cherry_core_context *context,
	struct cherry_core_event_device_info *device_info);

/**
 * cherry_uci_client_core_get_capabilities() - Get device capabilities.
 * @context: FiRa context
 * @device_caps: Device capabilities
 *
 * Return: UCI_STATUS_OK or error
 */
enum uci_status_code cherry_uci_client_core_get_capabilities(
	struct cherry_core_context *context,
	struct cherry_core_event_device_capabilities *device_caps);

/**
 * cherry_uci_client_core_get_uwb_device_stats() - Get the UWBS device stats.
 * @context: Core context.
 * @stats: UWBS device stats.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_core_get_uwb_device_stats(
	struct cherry_core_context *context,
	struct cherry_core_event_device_stats *stats);

/**
 * cherry_uci_client_core_get_device_timestamp() - Get the device timestamp.
 * @context: Core context.
 * @device_timestamp: Device timestamp.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_core_get_device_timestamp(
	struct cherry_core_context *context,
	struct cherry_core_event_device_timestamp *device_timestamp);

/**
 * cherry_uci_client_core_get_uwbs_state() - Get the UWBS state.
 * @context: Core context.
 * @uwbs_state: UWBS state.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_core_get_uwbs_state(struct cherry_core_context *context,
				      enum uci_device_state *uwbs_state);

/**
 * cherry_uci_client_core_set_gpio_toggle_mode() - Set mode and get the GPIO toggle timestamp.
 * @context: Core context.
 * @mode: Mode to use for GPIO toggle.
 * @gpio_toggle: GPIO toggle timestamp.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_core_set_gpio_toggle_mode(
	struct cherry_core_context *context, uint8_t mode,
	struct cherry_core_event_gpio_toggle *gpio_toggle);

void cherry_uci_client_core_capabilities_free(
	struct cherry_core_event_device_capabilities *device_caps);

#endif /* CHERRY_CORE_CLIENT_H */
