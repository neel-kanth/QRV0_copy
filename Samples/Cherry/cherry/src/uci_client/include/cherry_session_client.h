/*
 * Header file for uci session client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_SESSION_CLIENT_H
#define CHERRY_SESSION_CLIENT_H

#include <cherry/cherry_common.h>
#include <cherry/cherry_fira.h>
#include <qerr.h>
#include <stdbool.h>
#include <stdint.h>
#include <uci/uci.h>

#define FIRA_CONTROLEES_MAX 8
#define STATIC_STS_IV_SIZE 6
#define VENDOR_ID_SIZE 2

#define DL_TDOA_ANCHOR_LOCATION_MAX_SIZE 13
#define DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE 257
#define FIRA_DL_TDOA_ANCHOR_LOCATION_SIZE_MAX 19

/**
 * enum fira_status - FiRa-UCI-v2.0.0_0.9r11 8.5 Status Code
 * @FIRA_STATUS_OK: Success.
 * @FIRA_STATUS_REJECTED: Intended operation is not supported in the current state.
 * @FIRA_STATUS_FAILED: Intended operation is failed to complete.
 * @FIRA_STATUS_SYNTAX_ERROR: UCI packet structure is not per spec.
 * @FIRA_STATUS_INVALID_PARAM: Config ID is not correct, and it is not present in UWBS.
 * @FIRA_STATUS_INVALID_RANGE: Config ID is correct, and value is not in proper range
 * @FIRA_STATUS_INVALID_MESSAGE_SIZE: UCI packet payload size is not as per spec.
 * @FIRA_STATUS_UNKNOWN_GID: UCI Group ID is not per spec.
 * @FIRA_STATUS_UNKNOWN_OID: UCI Opcode ID is not per spec.
 * @FIRA_STATUS_READ_ONLY: Config ID is read-only.
 * @FIRA_STATUS_UCI_MESSAGE_RETRY: UWBS request retransmission from Host
 * @FIRA_STATUS_UNKNOWN: It is not known whether the intended operation was failed or successful.
 * @FIRA_STATUS_NOT_APPLICABLE: The parameter ID is not applicable for the selected operation
 * @FIRA_STATUS_ERROR_SESSION_NOT_EXIST: Session is not existing or not created
 * @FIRA_STATUS_RFU_1: Reserved for future use
 * @FIRA_STATUS_ERROR_SESSION_ACTIVE: Session is active.
 * @FIRA_STATUS_ERROR_MAX_SESSION_EXCEDEED: Max. number of sessions already created.
 * @FIRA_STATUS_ERROR_SESSION_NOT_CONFIGURED: Session is not configured with required
 *   app configurations
 * @FIRA_STATUS_ERROR_ACTIVE_SESSIONS_ONGOING: Sessions are actively running in UWBS.
 * @FIRA_STATUS_ERROR_MULTICAST_LIST_FULL: Indicates when multicast list is full during
 *   one-to-many ranging
 * @FIRA_STATUS_ERROR_UWB_INITIATION_TIME_TOO_OLD: The current UWBS time has gone past
 *   the configured UWB_INITIATION_TIME.
 * @FIRA_STATUS_OK_NEGATIVE_DISTANCE_REPORT: Success. A negative distance was measured:
 *   Distance is the absolute value of the measurement
 * @FIRA_STATUS_RANGING_TX_FAILED: Failed to transmit UWB packet.
 * @FIRA_STATUS_RANGING_RX_TIMEOUT: No UWB packet detected by the receiver.
 * @FIRA_STATUS_RANGING_RX_PHY_DEC_FAILED: UWB packet channel decoding error.
 * @FIRA_STATUS_RANGING_RX_PHY_TOA_FAILED: Failed to detect time of arrival of
 * the UWB packet from CIR samples.
 * @FIRA_STATUS_RANGING_RX_PHY_STS_FAILED: UWB packet STS segment mismatch.
 * @FIRA_STATUS_RANGING_RX_MAC_DEC_FAILED: MAC CRC or syntax error.
 * @FIRA_STATUS_RANGING_RX_MAC_IE_DEC_FAILED: IE syntax error.
 * @FIRA_STATUS_RANGING_RX_MAC_IE_MISSING: Expected IE missing in the packet.
 * @FIRA_STATUS_ERROR_ROUND_INDEX_NOT_ACTIVATED: Configured DL-TDoA ranging round
 * could not be activated.
 * @FIRA_STATUS_ERROR_NUMBER_OF_ACTIVE_RANGING_ROUNDS_EXCEEDED: Number of active
 * ranging rounds exceeds the maximum number of ranging rounds supported.
 * @FIRA_STATUS_ERROR_DL_TDOA_DEVICE_ADDRESS_NOT_MATCHING_IN_REPLY_TIME_LIST:
 * Received DL-TDoA Reply Time List does not contain the Initiator Reply Time
 * associated to the MAC address in RDM List (can happen only in DS-TWR).
 * @FIRA_STATUS_RANGING_INTERNAL_ERROR: Implementation specific error.
 */
enum fira_status {
	FIRA_STATUS_OK = 0x00,
	FIRA_STATUS_REJECTED = 0x01,
	FIRA_STATUS_FAILED = 0x02,
	FIRA_STATUS_SYNTAX_ERROR = 0x03,
	FIRA_STATUS_INVALID_PARAM = 0x04,
	FIRA_STATUS_INVALID_RANGE = 0x05,
	FIRA_STATUS_INVALID_MESSAGE_SIZE = 0x06,
	FIRA_STATUS_UNKNOWN_GID = 0x07,
	FIRA_STATUS_UNKNOWN_OID = 0x08,
	FIRA_STATUS_READ_ONLY = 0x09,
	FIRA_STATUS_UCI_MESSAGE_RETRY = 0x0A,
	FIRA_STATUS_UNKNOWN = 0x0B,
	FIRA_STATUS_NOT_APPLICABLE = 0x0C,
	/* UWB Session Specific Status Codes. */
	FIRA_STATUS_ERROR_SESSION_NOT_EXIST = 0x11,
	FIRA_STATUS_RFU_1 = 0x12,
	FIRA_STATUS_ERROR_SESSION_ACTIVE = 0x13,
	FIRA_STATUS_ERROR_MAX_SESSION_EXCEDEED = 0x14,
	FIRA_STATUS_ERROR_SESSION_NOT_CONFIGURED = 0x15,
	FIRA_STATUS_ERROR_ACTIVE_SESSIONS_ONGOING = 0x16,
	FIRA_STATUS_ERROR_MULTICAST_LIST_FULL = 0x17,
	FIRA_STATUS_ERROR_UWB_INITIATION_TIME_TOO_OLD = 0x1a,
	FIRA_STATUS_OK_NEGATIVE_DISTANCE_REPORT = 0x1B,
	/* UWB Ranging Session Specific Status Codes. */
	FIRA_STATUS_RANGING_TX_FAILED = 0x20,
	FIRA_STATUS_RANGING_RX_TIMEOUT = 0x21,
	FIRA_STATUS_RANGING_RX_PHY_DEC_FAILED = 0x22,
	FIRA_STATUS_RANGING_RX_PHY_TOA_FAILED = 0x23,
	FIRA_STATUS_RANGING_RX_PHY_STS_FAILED = 0x24,
	FIRA_STATUS_RANGING_RX_MAC_DEC_FAILED = 0x25,
	FIRA_STATUS_RANGING_RX_MAC_IE_DEC_FAILED = 0x26,
	FIRA_STATUS_RANGING_RX_MAC_IE_MISSING = 0x27,
	FIRA_STATUS_ERROR_ROUND_INDEX_NOT_ACTIVATED = 0x28,
	FIRA_STATUS_ERROR_NUMBER_OF_ACTIVE_RANGING_ROUNDS_EXCEEDED = 0x29,
	FIRA_STATUS_ERROR_DL_TDOA_DEVICE_ADDRESS_NOT_MATCHING_IN_REPLY_TIME_LIST =
		0x2A,
	/* Proprietary status code. */
	FIRA_STATUS_RANGING_INTERNAL_ERROR = 0xFF,
};

/**
 * enum fira_ranging_data_attrs_ranging_measurement_type - Values for
 * FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE attribute.
 *
 * @FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_TWR:
 * 	Two Way Ranging Measurement (SS-TWR, DS-TWR).
 * @FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_DL_TDOA:
 * 	DL-TDoA Ranging Measurement (OWR DL-TDoA).
 * @FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_OWR_AOA:
 * 	OWR for AoA Measurement.
 */
enum fira_ranging_data_attrs_ranging_measurement_type {
	FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_TWR = 1,
	FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_DL_TDOA = 2,
	FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_OWR_AOA = 3,
	FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER = 4,
	FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE = 5,
	FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_DL_TDOA_V2 = 6,
};

/**
 * enum session_dt_location_coord_system_type -- Coordinate System Type
 * of a DT-Anchor.
 * @FIRA_DT_LOCATION_COORD_WGS84: The location is given in WGS84 coordinate
 * system (longitude, latitude, altitude,(see &struct session_wgs84_location).
 * @FIRA_DT_LOCATION_COORD_RELATIVE: The location is given in relative
 * coordinates system (see &struct session_relative_location).
 * @FIRA_DT_LOCATION_COORD_INVALID: is a value in RSU range for test.
 */
enum session_dt_location_coord_system_type {
	FIRA_DT_LOCATION_COORD_WGS84 = 0,
	FIRA_DT_LOCATION_COORD_RELATIVE = 1,
	FIRA_DT_LOCATION_COORD_WGS84_WITH_Z = 2,
	FIRA_DT_LOCATION_COORD_RELATIVE_WITH_Z = 3,
	FIRA_DT_LOCATION_COORD_RELATIVE_GRAVITY_ALIGNED = 4,
	FIRA_DT_LOCATION_COORD_RELATIVE_GRAVITY_ALIGNED_WITH_Z = 5,
	FIRA_DT_LOCATION_COORD_INVALID = 0xFF,
};

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
	uint16_t addresses[FIRA_CONTROLEES_MAX];
};

/*
 * struct dl_tdoa_anchor_location - FiRa DT-Anchor location
 */
struct dl_tdoa_anchor_location {
	/**
	 * @info: Presence of DT-Anchor location
	 */
	bool location_presence;
	/**
	 * @info: Type of coordinates system (WGS-84/relative)
	 */
	uint8_t coordinates_system;
	/**
	 * @info: x coordinate in relative coordinates system / latitude in WGS-84
	 */
	uint64_t location_x;
	/**
	 * @info: y coordinate in relative coordinates system / longitude in WGS-84
	 */
	uint64_t location_y;
	/**
	 * @info: z coordinate in relative coordinates system / altitude in WGS-84
	 */
	uint64_t location_z;
};

/*
 * typedef struct cherry_session_context - Session context
 */
struct cherry_session_context;

/*
 * struct session_status_ntf - Content of a SESSION_STATUS_NTF.
 *
 * Use the session_status_ntf_get_xxx functions to access to the content.
 */
struct session_status_ntf {
	uint32_t session_handle;
	enum uci_session_state session_state;
	enum uci_session_reason_code reason_code;
};

/**
 * session_status_ntf_get_session_handle - Gets the id of the session.
 *
 * @ntf: The notification containing the information
 *
 * Return: The id of the session.
 */
uint32_t
session_status_ntf_get_session_handle(const struct session_status_ntf *ntf);

/**
 * session_status_ntf_get_session_state - Gets the state of the session.
 *
 * @ntf: The notification containing the information.
 *
 * Return: The state of the session.
 */
enum uci_session_state
session_status_ntf_get_session_state(const struct session_status_ntf *ntf);

/**
 * session_status_ntf_get_reason_code - Gets the reason code of the change.
 *
 * @ntf: The notification containing the information.
 *
 * Return: The reason code of the change.
 */
enum uci_session_reason_code
session_status_ntf_get_reason_code(const struct session_status_ntf *ntf);

/**
 * cherry_session_frame_status_to_cherry_format() - Convert frame status into Cherry format.
 * @code: UCI value.
 *
 * Return: The converted frame status value in Cherry format.
 */
enum cherry_common_frame_status
cherry_session_frame_status_to_cherry_format(enum fira_status code);

/**
 * typedef cherry_uci_client_session_status_cb_t - Type for session status NTF cb.
 *
 * @ntf: Sessions status notification.
 * @user_data: User data pointer given to cherry_uci_client_session_open.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_session_status_cb_t)(
	const struct session_status_ntf *ntf, void *user_data);

struct uci_message_parser;

struct session_ranging_data {
	uint32_t session_handle;
	uint32_t sequence_number;
	uint32_t ranging_interval_ms;
	uint8_t type;
	uint8_t n_measurements;
	struct uci_message_parser *parser;
	void *user_data;
};

/**
 * typedef cherry_uci_client_session_ranging_ntf_cb_t - Ranging notification callback type.
 *
 * @data: Ranging data structure.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_session_ranging_ntf_cb_t)(
	const struct session_ranging_data *data);

/**
 * cherry_uci_client_session_open() - Initialize the internal resources of the client.
 *
 * @context: Session context to initialize.
 * @uci: UCI Core context.
 * @user_data: User data pointer to give back in callback.
 * @session_status_cb: Callback to use to notify each session status NTF.
 * @ranging_ntf_cb: Callback to ranging NTF.
 *
 * NOTE: This function must be called first. @cherry_uci_client_session_close must be called
 * at the end of the application to ensure resources are freed.
 * The channel will be managed by the client, this means you should neither use
 * uwbmac_channel_create nor uwbmac_channel_release.
 *
 * Return: QERR_SUCCESS or error.
 */
enum qerr cherry_uci_client_session_open(
	struct cherry_session_context **context, struct uci *uci,
	void *user_data,
	cherry_uci_client_session_status_cb_t session_status_cb,
	cherry_uci_client_session_ranging_ntf_cb_t ranging_ntf_cb);

/**
 * cherry_uci_client_session_close() - Free all internal resources of the client.
 *
 * @context: Session context to free.
 */
void cherry_uci_client_session_close(struct cherry_session_context *context);

/**
 * cherry_uci_client_session_init_session() - Initialize a session.
 *
 * @context: Session context.
 * @session_id: Session identifier.
 * @session_type: Session type.
 * @session_handle: Session handle.
 *
 * This function must be called first to create and initialize the session session.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_init_session(
	struct cherry_session_context *context, uint32_t session_id,
	uint8_t session_type, uint32_t *session_handle);

/**
 * cherry_uci_client_session_start_session() - Start a session session.
 * @context: Session context.
 * @session_handle: Session handle.
 *
 * This function must be called after session session was initialized.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_start_session(struct cherry_session_context *context,
					uint32_t session_handle);

/**
 * cherry_uci_client_session_stop_session() - Stop a session session.
 * @context: Session context.
 * @session_handle: Session handle.
 *
 * This function stop the session ranging.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_stop_session(struct cherry_session_context *context,
				       uint32_t session_handle);

/**
 * cherry_uci_client_session_deinit_session() - Deinitialize a session session.
 * @context: Session context.
 * @session_handle: Session handle.
 *
 * This function is called to free all memory allocated by the session.
 * This function must be called when the session is stopped.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_deinit_session(struct cherry_session_context *context,
					 uint32_t session_handle);

/**
 * cherry_uci_client_session_set_app_config_cmd_create() - Create command to set session
 * configuration.
 * @context: FiRa context.
 *
 * Return: new command or NULL if error.
 */
struct cherry_uci_client_session_set_app_config_cmd *
cherry_uci_client_session_set_app_config_cmd_create(
	struct cherry_session_context *context);

/**
 * cherry_uci_client_session_set_app_config_cmd_put() - Put parameter value to set session
 * configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @data: Session parameter value pointer. Pointed value should have little endian format.
 * @size: Session parameter value size in bytes.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_set_app_config_cmd_put(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const void *data, uint8_t size);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_uint8() - Put uint8_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: uint8_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_uint8(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const uint8_t v)
{
	return cherry_uci_client_session_set_app_config_cmd_put(cmd, param_id,
								&v, sizeof(v));
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_int8() - Put int8_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: int8_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_int8(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const int8_t v)
{
	return cherry_uci_client_session_set_app_config_cmd_put(cmd, param_id,
								&v, sizeof(v));
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_uint16() - Put uint16_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: uint16_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_uint16(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const uint16_t v)
{
	uint8_t buf[] = {
		(uint8_t)v,
		(uint8_t)(v >> 8),
	};
	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, param_id, &buf, sizeof(buf));
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_int16() - Put int16_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: int16_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_int16(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const int16_t v)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, param_id, (uint16_t)v);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_uint32() - Put uint32_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: uint32_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_uint32(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const uint32_t v)
{
	uint8_t buf[] = {
		(uint8_t)v,
		(uint8_t)(v >> 8),
		(uint8_t)(v >> 16),
		(uint8_t)(v >> 24),
	};
	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, param_id, &buf, sizeof(buf));
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_int32() - Put int32_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: int32_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_int32(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const int32_t v)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint32(
		cmd, param_id, (uint32_t)v);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_uint64() - Put uint64_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: uint64_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_uint64(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const uint64_t v)
{
	uint8_t buf[] = {
		(uint8_t)v,	    (uint8_t)(v >> 8),	(uint8_t)(v >> 16),
		(uint8_t)(v >> 24), (uint8_t)(v >> 32), (uint8_t)(v >> 40),
		(uint8_t)(v >> 48), (uint8_t)(v >> 56),
	};
	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, param_id, &buf, sizeof(buf));
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_int64() - Put int64_t parameter value to set
 * session configuration command.
 * @cmd: Session set application configuration command.
 * @param_id: Session parameter ID.
 * @v: int64_t value.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_int64(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t param_id, const int64_t v)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint64(
		cmd, param_id, (uint64_t)v);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_send() - Send set session configuration command and
 * free associated memory.
 * @cmd: Session set application configuration command.
 * @session_handle: Session handle.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_set_app_config_cmd_send(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint32_t session_handle);

/**
 * cherry_uci_client_session_set_app_config_cmd_abort() - Abort pending session configuration
 * command.
 * @cmd: Session set application configuration command.
 */
void cherry_uci_client_session_set_app_config_cmd_abort(
	struct cherry_uci_client_session_set_app_config_cmd *cmd);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_interval_ms() - Sets the ranging interval.
 * @cmd: Session set application configuration command.
 * @interval_ms: Interval between ranging, in milliseconds.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_interval_ms(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	int interval_ms)
{
	return cherry_uci_client_session_set_app_config_cmd_put_int32(
		cmd, UCI_APPLICATION_PARAMETER_RANGING_INTERVAL, interval_ms);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_device_type() - Sets the device type.
 * @cmd: Session set application configuration command.
 * @device_type: 0 - CONTROLEE, 1 - CONTROLLER,
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_device_type(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t device_type)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DEVICE_TYPE, device_type);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage() - Sets ranging round usage.
 * @cmd: Session set application configuration command.
 * @ranging_round_usage:
 *  possible values:
 *  One Way Ranging mode (unused, not in FiRa 1.1).
 *  Single-Sided Two Way Ranging mode.
 *  Dual-Sided Two Way Ranging mode.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_ranging_round_usage(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t ranging_round_usage)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_RANGING_ROUND_USAGE,
		ranging_round_usage);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_device_role()- Sets the device role
 * @cmd: Session set application configuration command.
 * @device_role: Role played by the device.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_device_role(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t device_role)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DEVICE_ROLE, device_role);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sts_config() - scrambled timestamp sequence configuration.
 * @cmd: Session set application configuration command.
 * @sts_config:
 *  Possible values:
 *   0x00: Static STS (default).
 *   0x01: Dynamic STS.
 *   0x02: Dynamic STS - Responder Specific Sub-session key.
 *   0x03: Provisioned STS.
 *   0x04: Provisioned STS - Responder Specific Sub-session key.
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_sts_config(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t sts_config)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_STS_CONFIG, sts_config);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode() - The multi-node mode used during a round.
 * @cmd: Session set application configuration command.
 * @multi_node_mode:
 * 	FIRA_MULTI_NODE_MODE_UNICAST,
 *      FIRA_MULTI_NODE_MODE_ONE_TO_MANY,
 *	FIRA_MULTI_NODE_MODE_MANY_TO_MANY,
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_multi_node_mode(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t multi_node_mode)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_MULTI_NODE_MODE,
		multi_node_mode);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_number_of_sts_segments() - Sets the number of STS segments.
 * @cmd: Session set application configuration command.
 * @sts_segment: STS segment.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_number_of_sts_segments(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t sts_segment)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_NUMBER_OF_STS_SEGMENTS,
		sts_segment);
}

/**
 * cherry_uci_client_session_get_app_config_number_of_sts_segments() - Gets the number of STS segments.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @sts_segment: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_number_of_sts_segments(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sts_segment);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_device_mac_address() - Sets the device mac address.
 * @cmd: Session set application configuration command.
 * @dev_mac_addr: Device mac address.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_device_mac_address(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t dev_mac_addr)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS,
		dev_mac_addr);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_mac_address_mode() - Sets the mac address mode.
 * @cmd: Session set application configuration command.
 * @mac_address_mode: Mac addres mode.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_mac_address_mode(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t mac_address_mode)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_MAC_ADDRESS_MODE,
		mac_address_mode);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address() - Sets the destination mac address.
 * @cmd: Session set application configuration command.
 * @dst_mac_address: Destination mac address.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dest_mac_address(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const struct dst_mac_addresses *dest_mac_add);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_no_of_controlees() - Sets the number of controlees.
 * @cmd: Session set application configuration command.
 * @no_of_controlees: Number of controlees.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_no_of_controlees(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t no_of_controlees)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_NUMBER_OF_CONTROLEES,
		no_of_controlees);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_result_report_config() - Sets the result report config.
 * @cmd: Session set application configuration command.
 * @result_report_config: result_report_config.
 * b0 = TOF report (0: Disable, 1: Enable)
 * b1 = AOA Azimuth report (0: Disable, 1: Enable)
 * b2 = AOA elevation report (0: Disable, 1: Enable)
 * b3 = AOA FOM report (0: Disable, 1: Enable)
 * This configuration parameter is only applicable when the Controlee is intended to transmit
 * a RRRM or MRM Type 3 message
 * default = 0x01
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_result_report_config(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t result_report_config)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_RESULT_REPORT_CONFIG,
		result_report_config);
}

/**
 * cherry_uci_client_session_get_app_config_device_mac_address() - Gets the device mac address.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @device_mac_address: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_device_mac_address(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *device_mac_address);

/**
 * @context: Session context.
 * @session_handle: Session identifier.
 * @no_of_controlees: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_no_of_controlees(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *no_of_controlees);

/**
 * cherry_uci_client_session_get_app_config_dest_mac_address() - Gets dstination mac address.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @dst_mac_address: variable to sore the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_dest_mac_address(
	struct cherry_session_context *context, uint32_t session_handle,
	struct dst_mac_addresses *dst_mac_address);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_short_addr() - Sets short address.
 * @cmd: Session set application configuration command.
 * @short_addr: short_addr.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_short_addr(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t short_addr)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_DEVICE_MAC_ADDRESS, short_addr);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_destination_short_address() - Sets destination short address.
 * @cmd: Session set application configuration command.
 * @dest_short_addr: dest_short_addr.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_destination_short_address(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t dest_short_addr)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_DST_MAC_ADDRESS,
		dest_short_addr);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_initiation_time_us() - Sets initiation time [us].
 * @cmd: Session set application configuration command.
 * @initiation_time_us: initiation_time_us.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_initiation_time_us(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint64_t initiation_time_us)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint64(
		cmd, UCI_APPLICATION_PARAMETER_UWB_INITIATION_TIME,
		initiation_time_us);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_slot_duration_rstu() - Sets slot duration rstu.
 * @cmd: Session set application configuration command.
 * @slot_duration_rstu: slot_duration_rstu. - Duration of a slot in RSTU (1200RSTU=1ms)
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_slot_duration_rstu(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t slot_duration_rstu)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_SLOT_DURATION,
		slot_duration_rstu);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_slots_per_ranging_round() - Sets round duration slots.
 * @cmd: Session set application configuration command.
 * @slots_per_rr: Number of slots per ranging round.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_slots_per_ranging_round(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t slots_per_rr)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_SLOTS_PER_RR, slots_per_rr);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_block_stride_length() - Sets block stride length.
 * @cmd: Session set application configuration command.
 * @block_stride_length: Number of blocks to stride.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_block_stride_length(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t block_stride_length)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_BLOCK_STRIDE_LENGTH,
		block_stride_length);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_hopping_mode() - Enables or disable hopping
 * @cmd: Session set application configuration command.
 * @hopping_mode:
 * 	0x00 = Hopping Disable
 *	0x01 = Session Hopping Enable
 *	Values 0x02 to 0x9F = RFU
 * 	Values 0xA0 to 0xFF = Vendor specific modes
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_hopping_mode(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t hopping_mode)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_HOPPING_MODE, hopping_mode);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_priority() - sets the priority.
 * @cmd: Session set application configuration command.
 * @priority: priority of the session.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_priority(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t priority)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_SESSION_PRIORITY, priority);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_schedule_mode() - Sets schedule mode parameter.
 * @cmd: Session set application configuration command.
 * @schedule_mode:
 * - 0x00 - Contention-based ranging.
 * - 0x01 - Time-scheduled ranging.
 * - 0x02 - Hybrid-based ranging (not supported).
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_schedule_mode(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t schedule_mode)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_SCHEDULE_MODE, schedule_mode);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_max_number_of_measurements() - Sets the max number of measurements.
 * @cmd: Session set application configuration command.
 * @max_number_of_measurements: max_number_of_measurements.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_max_number_of_measurements(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t max_number_of_measurements)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_MAX_NUMBER_OF_MEASUREMENTS,
		max_number_of_measurements);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_max_rr_retry() - Sets the max rr retry.
 * @cmd: Session set application configuration command.
 * @max_rr_retry: max_rr_retry.
 * Number of failed ranging round attempts before stopping the session.
 * The value zero disables the feature.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_max_rr_retry(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t max_rr_retry)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_MAX_RR_RETRY, max_rr_retry);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_channel_number() - Sets the channel number.
 * @cmd: Session set application configuration command.
 * @channel_number: channel_number.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_channel_number(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t channel_number)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_CHANNEL_NUMBER, channel_number);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index() - Sets preamble code index.
 * @cmd: Session set application configuration command.
 * @preamble_code_index: preamble_code_index.
 * Possible values:
 *  9-12: BPRF
 *  25-32: HPRF
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_preamble_code_index(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t preamble_code_index)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_PREAMBLE_CODE_INDEX,
		preamble_code_index);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_rframe_config() - Sets rframe_config.
 * @cmd: Session set application configuration command.
 * @rframe_config: rframe_config.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_rframe_config(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t rframe_config)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_RFRAME_CONFIG, rframe_config);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_preamble_duration() - Sets preamble duration.
 * @cmd: Session set application configuration command.
 * @preamble_duration: 	- 0x00: 32 symbols or 0x01: 64 symbols (default)
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_preamble_duration(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t preamble_duration)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_PREAMBLE_DURATION,
		preamble_duration);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sfd_id() - Sets sfd_id.
 * @cmd: Session set application configuration command.
 * @sfd_id: 0 or 2 in BPRF, 1-4 in HPRF
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_sfd_id(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t sfd_id)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_SFD_ID, sfd_id);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_psdu_data_rate() - Sets psdu data rate.
 * @cmd: Session set application configuration command.
 * @psdu_data_rate:
 * Possible values:
 *  0: 6.81Mbps (default)
 *  1: 7.80 Mbps
 *  2: 27.2 Mbps
 *  3: 31.2 Mbps
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_psdu_data_rate(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t psdu_data_rate)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_PSDU_DATA_RATE, psdu_data_rate);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_key_rotation() - Enable/disable key rotation.
 * @cmd: Session set application configuration command.
 * @key_rotation: 0 to disable key rotation, 1 to enable it.
 * Enable/disable key rotation during Dynamic or Provisioned STS ranging.
 * If enable the period will be set with
 * cherry_uci_client_session_set_app_config_cmd_put_key_rotation_rate.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_key_rotation(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t key_rotation)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_KEY_ROTATION, key_rotation);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_key_rotation_rate() - Sets key rotation rate.
 * @cmd: Session set application configuration command.
 * @key_rotation_rate: defines n, with 2^n being the rotation rate of some
 * keys used during Dynamic or Provisioned STS Ranging,
 * n shall be in the range of 0<=n<=15.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_key_rotation_rate(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t key_rotation_rate)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_KEY_ROTATION_RATE,
		key_rotation_rate);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_info_ntf_config() - Sets ntf config.
 * @cmd: Session set application configuration command.
 * @session_info_ntf_config: values;
 *  0x00 = Disable range data notification (ntf)
 *  0x01 = Enable range data notification (default)
 *  0x02 = Enable range data ntf while inside proximity range
 *  0x03 = Enable range data ntf while inside AoA upper and lower bounds
 *  0x04 = Enable range data ntf while inside AoA upper and lower bounds as well as inside proximity range
 *  0x05 = Enable range data ntf only when entering or leaving proximity range
 *  0x06 = Enable range data ntf only when entering or leaving AoA upper and lower bounds
 *  0x07 = Enable range data ntf only when entering or leaving AoA upper and lower bounds as well as entering or leaving proximity range
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_info_ntf_config(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t session_info_ntf_config)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_SESSION_INFO_NTF_CONFIG,
		session_info_ntf_config);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_report_rssi() - Sets the report rssi.
 * @cmd: Session set application configuration command.
 * @report_rssi: report_rssi false - no report, true report
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_report_rssi(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t report_rssi)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_RSSI_REPORTING, report_rssi);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_prf_mode() - Sets prf mode.
 * @cmd: Session set application configuration command.
 * @prf_mode: prf_mode pulse repetition frequency.
 *  0x00: 62.4 MHz PRF. BPRF mode (default)
 *  0x01: 124.8 MHz PRF. HPRF mode.
 *  0x02: 249.6 MHz PRF. HPRF mode with data rate 27.2 and 31.2 Mbps
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_prf_mode(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t prf_mode)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_PRF_MODE, prf_mode);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sts_length() - Sets sts length.
 * @cmd: Session set application configuration command.
 * @sts_length: values
 *  0x00: 32 symbols
 *  0x01: 64 symbols (default)
 *  0x02: 128 symbols
 *  Values 0x03 to 0xFF: RFU
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_sts_length(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t sts_length)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_STS_LENGTH, sts_length);
}

/**
 * cherry_uci_client_session_get_app_config_device_type() - Gets the device type.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @device_type: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_device_type(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *device_type);

/**
 * cherry_uci_client_session_get_app_config_ranging_round_usage() - Gets the ranging round usage.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @ranging_round_usage: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_ranging_round_usage(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *ranging_round_usage);

/**
 * cherry_uci_client_session_get_app_config_device_role() - Gets device role.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @device_role: device_role.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_device_role(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *device_role);

/**
 * cherry_uci_client_session_get_app_config_short_sts_config() - Gets the sts config.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @sts_config: variable to store the value
 *  Possible values:
 *   0x00: Static STS (default).
 *   0x01: Dynamic STS.
 *   0x02: Dynamic STS - Responder Specific Sub-session key.
 *   0x03: Provisioned STS.
 *   0x04: Provisioned STS - Responder Specific Sub-session key.
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_short_sts_config(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sts_config);

/**
 * cherry_uci_client_session_get_app_config_multi_node_mode() - Gets the multi node mode.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @multi_node_mode: variable to store the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_multi_node_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *multi_node_mode);

/**
 * cherry_uci_client_session_get_app_config_short_address() - Gets the short address.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @short_addr: variable to store the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_short_address(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *short_addr);

/**
 * cherry_uci_client_session_get_app_config_destination_short_address() - Gets the destination short address.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @dest_short_addr: variable to store the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_destination_short_address(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *dest_short_addr);

/**
 * cherry_uci_client_session_get_app_config_initiation_time_ms() - Gets the initiation time.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @initiation_time_ms: variable to store the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_initiation_time_ms(
	struct cherry_session_context *context, uint32_t session_handle,
	uint32_t *initiation_time_ms);

/**
 * cherry_uci_client_session_get_app_config_slot_duration_rstu() - Gets the slot duration rstu.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @slot_duration_rstu: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_slot_duration_rstu(
	struct cherry_session_context *context, uint32_t session_handle,
	uint32_t *slot_duration_rstu);

/**
 * cherry_uci_client_session_get_app_config_slots_per_ranging_round() - Gets the number of round duration slots
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @slots_per_rr: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_slots_per_ranging_round(
	struct cherry_session_context *context, uint32_t session_handle,
	uint32_t *slots_per_rr);

/**
 * cherry_uci_client_session_get_app_config_block_duration_ms() - Gets the block duration.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @block_duration_ms: variable to store the value (Block size in unit of 1200 RSTU (same as ms))
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_block_duration_ms(
	struct cherry_session_context *context, uint32_t session_handle,
	uint32_t *block_duration_ms);

/**
 * cherry_uci_client_session_get_app_config_block_stride_length() - Gets the block stride length.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @block_stride_length: variable to store the number of blocks to stride.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_block_stride_length(
	struct cherry_session_context *context, uint32_t session_handle,
	uint32_t *block_stride_length);

/**
 * cherry_uci_client_session_get_app_config_hopping_mode() - Gets the hopping mode 0 - disabled, 1 enabled.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @hopping_mode: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_hopping_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *hopping_mode);

/**
 * cherry_uci_client_session_get_app_config_priority() - Gets the priority.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @priority: variable to store the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_priority(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *priority);

/**
 * cherry_uci_client_session_get_app_config_skip_ranging_control_phase() - Gets the RCP setting for non-deferred mode (included/skipped).
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @skip_ranging_control_phase: Variable to store the value (0 included, 1 skipped).
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_skip_ranging_control_phase(
	struct cherry_session_context *context, uint32_t session_handle,
	bool *skip_ranging_control_phase);

/**
 * cherry_uci_client_session_get_app_config_result_report_phase() - Gets the result report phase.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @result_report_phase: variable to store the value 0 disabled, 1 enabled.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_result_report_phase(
	struct cherry_session_context *context, uint32_t session_handle,
	bool *result_report_phase);

/**
 * cherry_uci_client_session_get_app_config_schedule_mode() - Gets schedule mode parameter.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @schedule_mode: variable to store the value; .
 * - 0x00 - Contention-based ranging.
 * - 0x01 - Time-scheduled ranging.
 * - 0x02 - Hybrid-based ranging (not supported).
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_schedule_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *schedule_mode);

/**
 * cherry_uci_client_session_get_app_config_embedded_mode() - Gets the embedded mode.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @embedded_mode: variable to store the value; .
 *  0: MODE_DEFERRED - Ranging messages do not embed control messages. Additional messages are required.
 *  1: MODE_NON_DEFERRED - Ranging messages embed control messages
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_embedded_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *embedded_mode);

/**
 * cherry_uci_client_session_get_app_config_max_number_of_measurements() - Gets the number of measurements.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_number_of_measurements: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_max_number_of_measurements(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *max_number_of_measurements);

/**
 * cherry_uci_client_session_get_app_config_max_rr_retry() - Gets the maximum rangring rounds retry.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @max_rr_retry: max_rr_retry. variable to store the value
 * Number of failed ranging round attempts before stopping the session.
 * The value zero disables the feature.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_max_rr_retry(
	struct cherry_session_context *context, uint32_t session_handle,
	uint16_t *max_rr_retry);

/**
 * cherry_uci_client_session_get_app_config_channel_number() - Gets the channel used in this session.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @channel_number: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_channel_number(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *channel_number);

/**
 * cherry_uci_client_session_get_app_config_preamble_code_index() - Gets preamble code index.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_code_index: variable to store the value.
 * Possible values:
 * 9-12: BPRF
 * 25-32: HPRF
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_preamble_code_index(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *preamble_code_index);

/**
 * cherry_uci_client_session_get_app_config_rframe_config() - Gets the rframe_config.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @rframe_config: variable to store the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_rframe_config(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *rframe_config);

/**
 * cherry_uci_client_session_get_app_config_preamble_duration() - Gets the preamble duration.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @preamble_duration: variable to store the value. 0x00: 32 symbols or 0x01: 64 symbols (default)
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_preamble_duration(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *preamble_duration);

/**
 * cherry_uci_client_session_get_app_config_sfd_id() - Gets sfd_id.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @sfd_id: sfd_id. variable to store the value
 * possible values 0 or 2 in BPRF, 1-4 in HPRF
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_sfd_id(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sfd_id);

/**
 * cherry_uci_client_session_get_app_config_psdu_data_rate() - Gets the psdu data rate.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @psdu_data_rate: psdu_data_rate.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_psdu_data_rate(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *psdu_data_rate);

/**
 * cherry_uci_client_session_get_app_config_sub_session_handle() - Gets controlee' sub-session id.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @sub_session_handle: controlee' sub-session id used during Dynamic or Provisioned STS
 * for Responder Specific Sub-session Key.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_sub_session_id(
	struct cherry_session_context *context, uint32_t session_id,
	uint32_t *sub_session_id);

/**
 * cherry_uci_client_session_get_app_config_key_rotation() - Gets the key rotation.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation: false - no rotation, true rotation
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_key_rotation(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *key_rotation);

/**
 * cherry_uci_client_session_get_app_config_key_rotation_rate() - Gets the key rotation rate.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @key_rotation_rate: value to store the variable which.
 * defines n, with 2^n being the rotation rate of some
 * keys used during Dynamic or Provisioned STS Ranging,
 * n shall be in the range of 0<=n<=15.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_key_rotation_rate(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *key_rotation_rate);

/**
 * cherry_uci_client_session_get_app_config_report_rssi() - Gets rssi report.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_rssi: variable to store; false no report, true report.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_report_rssi(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *report_rssi);

/**
 * cherry_uci_client_session_get_app_config_report_tof() - Gets the report tof
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_tof: variable to store; false no report, true report.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_report_tof(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *report_tof);

/**
 * cherry_uci_client_session_get_app_config_report_aoa_azimuth() - report aoa azimuth
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_aoa_azimuth: variable to store; false no report, true report.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_report_aoa_azimuth(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *report_aoa_azimuth);

/**
 * cherry_uci_client_session_get_app_config_report_aoa_elevation() - gets the aoa elevation.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_aoa_elevation: variable to store; false no report, true report.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_report_aoa_elevation(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *report_aoa_elevation);

/**
 * cherry_uci_client_session_get_app_config_report_aoa_fom() - gets the aoa fom.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @report_aoa_fom: variable to store; false no report, true report.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_report_aoa_fom(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *report_aoa_fom);

/**
 * cherry_uci_client_session_get_app_config_prf_mode() - gets the prf mode.
 * @context: Session context.
 * @session_handle: Session identifier.
 * @prf_mode: prf_mode. pulse repetition frequency
 * variable to store the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_prf_mode(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *prf_mode);

/**
 * cherry_uci_client_session_get_app_config_cap_size_min() - Gets cap size min.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @cap_size_min: variable to sore the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_cap_size_min(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *cap_size_min);

/**
 * cherry_uci_client_session_get_app_config_cap_size_max() - Get cap size max.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @cap_size_max: variable to sore the value.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_cap_size_max(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *cap_size_max);

/**
 * cherry_uci_client_session_get_app_config_sts_length() - gets sts length.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @sts_length: variable to store the value
 * 0x00: 32 symbols
 * 0x01: 64 symbols (default)
 * 0x02: 128 symbols
 * Values 0x03 to 0xFF: RFU
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_sts_length(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *sts_length);

/**
 * cherry_uci_client_session_get_app_config_info_ntf_config() - Gets range notification.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @session_info_ntf_config: variable to store the value;
 * 0x00 = Disable range data notification (ntf)
 * 0x01 = Enable range data notification (default)
 * 0x02 = Enable range data ntf while inside proximity range
 * 0x03 = Enable range data ntf while inside AoA upper and lower bounds
 * 0x04 = Enable range data ntf while inside AoA upper and lower bounds as well as inside proximity range
 * 0x05 = Enable range data ntf only when entering or leaving proximity range
 * 0x06 = Enable range data ntf only when entering or leaving AoA upper and lower bounds
 * 0x07 = Enable range data ntf only when entering or leaving AoA upper and lower bounds as well as entering or leaving proximity range
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_info_ntf_config(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *session_info_ntf_config);

/**
 * cherry_uci_client_session_get_app_config_range_data_ntf_proximity_near_mm() - Gets ntf proximity near.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @range_data_ntf_proximity_near_mm: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_range_data_ntf_proximity_near_mm(
	struct cherry_session_context *context, uint32_t session_handle,
	uint32_t *range_data_ntf_proximity_near_mm);

/**
 * cherry_uci_client_session_get_app_config_range_data_ntf_proximity_far_mm() - Gets ntf proximity
 * far.
 *
 * @context: Session context. @session_handle: Session identifier. @range_data_ntf_proximity_far_mm:
 * variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_range_data_ntf_proximity_far_mm(
	struct cherry_session_context *context, uint32_t session_handle,
	uint32_t *range_data_ntf_proximity_far_mm);

/**
 * cherry_uci_client_session_get_app_config_range_data_ntf_lower_bound_aoa_azimuth() - Gets ntf
 * lower bound aoa azimuth.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @range_data_ntf_lower_bound_aoa_azimuth_2pi: Lower bound in rad_2pi_q16
 * for AOA azimuth, applicable when range_data_ntf_config is set to 0x03, 0x04, 0x06 or 0x07.
 * It is a signed value on 16 bits (rad_2pi_q16). Allowed values range from -180° to +180°.
 * should be less than or equal to RANGE_DATA_NTF_UPPER_BOUND_AOA_AZIMUTH value. (default = -180).
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_range_data_ntf_lower_bound_aoa_azimuth(
	struct cherry_session_context *context, uint32_t session_handle,
	int16_t *range_data_ntf_lower_bound_aoa_azimuth_2pi);

/**
 * cherry_uci_client_session_get_app_config_range_data_ntf_upper_bound_aoa_azimuth() - Gets ntf
 * upper bound aoa azimuth.
 *
 * @context: Session context. @session_handle: Session identifier.
 * @data_ntf_upper_bound_aoa_azimuth_2pi: Upper bound in rad_2pi_q16 for AOA azimuth, applicable
 * when range_data_ntf_config is set to 0x03, 0x04, 0x06 or 0x07. It is a signed value on 16 bits
 * (rad_2pi_q16). Allowed values range from -180° to +180°. Should be greater than or equal to
 * RANGE_DATA_NTF_LOWER_BOUND_AOA_AZIMUTH value. (default = +180).
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_range_data_ntf_upper_bound_aoa_azimuth(
	struct cherry_session_context *context, uint32_t session_handle,
	int16_t *data_ntf_upper_bound_aoa_azimuth_2pi);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_rx_antenna_selection() - Sets antenna set to use for RX.
 * @cmd: Session set application configuration command.
 * @ant_set: The antenna set to use for RX for this session.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_rx_antenna_selection(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const uint8_t ant_set)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_RX_ANTENNA_SELECTION, ant_set);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_tx_antenna_selection() - Sets antenna set to use for TX.
 * @cmd: Session set application configuration command.
 * @ant_set: The antenna set to use for TX for this session.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_tx_antenna_selection(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const uint8_t ant_set)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_TX_ANTENNA_SELECTION, ant_set);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_round2_rx_antenna_selection() - Sets antenna set to use for RX on Aliro ranging round 2.
 * @cmd: Session set application configuration command.
 * @ant_set: The antenna set to use for RX for this session.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_round2_rx_antenna_selection(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const uint8_t ant_set)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_ROUND2_RX_ANTENNA_SELECTION,
		ant_set);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_round2_tx_antenna_selection() - Sets antenna set to use for TX on Aliro ranging round 2.
 * @cmd: Session set application configuration command.
 * @ant_set: The antenna set to use for TX for this session.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_round2_tx_antenna_selection(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const uint8_t ant_set)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_ROUND2_TX_ANTENNA_SELECTION,
		ant_set);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_antenna_selection() - Sets antenna set to use for RX and TX.
 * @cmd: Session set application configuration command.
 * @ant_set: The antenna set to use for RX and TX for this session.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_antenna_selection(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const uint8_t ant_set)
{
	enum uci_status_code st;
	st = cherry_uci_client_session_set_app_config_cmd_put_rx_antenna_selection(
		cmd, ant_set);
	if (st != UCI_STATUS_OK)
		return st;

	return cherry_uci_client_session_set_app_config_cmd_put_tx_antenna_selection(
		cmd, ant_set);
}

/**
 * cherry_uci_client_session_get_app_config_range_data_ntf_lower_bound_aoa_elevation() - Gets ntf lower bound aoa elevation.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @data_ntf_lower_bound_aoa_elevation_2pi: Lower bound in rad_2pi_q16
 * for AOA elevation, applicable when range_data_ntf_config is set to 0x03, 0x04, 0x06 or 0x07.
 * It is a signed value on 16 bits (rad_2pi_q16). Allowed values range from -90° to +90°.
 * Should be less than or equal to RANGE_DATA_NTF_PROXIMITY_UPPER_BOUND_A_ELEVATION value. (default = -90).
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_range_data_ntf_lower_bound_aoa_elevation(
	struct cherry_session_context *context, uint32_t session_handle,
	int16_t *data_ntf_lower_bound_aoa_elevation_2pi);

/**
 * cherry_uci_client_session_get_app_config_vendor_id() - Gets vendor id.
 * @context: Session context.
 * @session_id: Session identifier.
 * @vendor_id: Vendor identifier.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_vendor_id(
	struct cherry_session_context *context, uint32_t session_id,
	uint8_t vendor_id[VENDOR_ID_SIZE]);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_vendor_id() - Sets vendor id.
 * @cmd: Session set application configuration command.
 * @vendor_id: Vendor identifier.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_vendor_id(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const uint8_t vendor_id[VENDOR_ID_SIZE])
{
	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, UCI_APPLICATION_PARAMETER_VENDOR_ID, vendor_id,
		VENDOR_ID_SIZE);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_time_base() - Sync sessions.
 * @cmd: Session set application configuration command.
 * @enable: Enable feature.
 * @continue_session: Continue session if reference session is not active.
 * @resync: Resync time grid in case the ref session will become active again.
 * @session_handle: Session handle of reference session.
 * @offset_us: Session offset time in microseconds.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_session_time_base(
	struct cherry_uci_client_session_set_app_config_cmd *cmd, bool enable,
	bool continue_session, bool resync, uint32_t session_handle,
	uint32_t offset_us);

/**
 * cherry_uci_client_session_get_app_config_static_sts_IV() - Gets static sts IV.
 * @context: Session context.
 * @session_id: Session identifier.
 * @static_sts_IV: Static STS IV.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_static_sts_IV(
	struct cherry_session_context *context, uint32_t session_id,
	uint8_t static_sts_IV[STATIC_STS_IV_SIZE]);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_static_sts_iv() - Sets static sts IV.
 * @cmd: Session set application configuration command.
 * @static_sts_IV: Static STS IV.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_static_sts_iv(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const uint8_t static_sts_IV[STATIC_STS_IV_SIZE])
{
	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, UCI_APPLICATION_PARAMETER_STATIC_STS_IV, static_sts_IV,
		STATIC_STS_IV_SIZE);
}

/**
 * cherry_uci_client_session_get_app_config_range_data_ntf_upper_bound_aoa_elevation() - Gets ntf
 * upper bound aoa elevation.
 *
 * @context: Session context.
 * @session_handle: Session identifier.
 * @data_ntf_upper_bound_aoa_elevation_2pi: Upper bound in rad_2pi_q16
 * for AOA elevation, applicable when range_data_ntf_config is set to 0x03, 0x04, 0x06 or 0x07.
 * It is a signed value on 16 bits (rad_2pi_q16). Allowed values range from -90° to +90°.
 * Should be greater than or equal to RANGE_DATA_NTF_LOWER_BOUND_AOA_ELEVATION value. (default = +90).
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_range_data_ntf_upper_bound_aoa_elevation(
	struct cherry_session_context *context, uint32_t session_handle,
	int16_t *data_ntf_upper_bound_aoa_elevation_2pi);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_session_key() - Sets this key for the session.
 * @cmd: Session set application configuration command.
 * @key: pointer to the session key
 * @size: length of the session key in bytes, can be 16 or 32.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_session_key(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const void *key, uint8_t size)
{
	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, UCI_APPLICATION_PARAMETER_SESSION_KEY, key, size);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_diags_frame_report_fields - Sets the fields to show in diags.
 * This does not activate the Diagnostics.
 *
 * @cmd: Session set application configuration command.
 * @fields: mask of fields to activate
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_diags_frame_report_fields(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t fields)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DIAGS_FRAME_REPORTS_FIELDS,
		fields);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_enable_diags() - activate or deactivate the diagnotics.
 * @cmd: Session set application configuration command.
 * @enabled: 1 to activate and 0 to deactivate
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_enable_diags(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t enabled)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_ENABLE_DIAGNOSTICS, enabled);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sub_session_key() - Sets key for the current controlee' sub-session.
 * @cmd: Session set application configuration command.
 * @key: pointer to the sub-session key
 * @size: length of the sub-session key, can be 128 or 256 bits.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_sub_session_key(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const void *key, uint8_t size)
{
	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, UCI_APPLICATION_PARAMETER_SUB_SESSION_KEY, key, size);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_data_repetition_count() - Sets the data repetition count.
 * @cmd: Session set application configuration command.
 * @data_repetition_count: data repetition count.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_data_repetition_count(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t data_repetition_count)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DATA_REPETITION_COUNT,
		data_repetition_count);
}

/**
 * cherry_uci_client_session_get_app_config_data_repetition_count() - Gets the data repetition count used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @data_repetition_count: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_data_repetition_count(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *data_repetition_count);

/**
 * cherry_uci_client_set_app_config_cmd_put_dl_tdoa_ranging_method() - Sets the dl tdoa ranging method.
 * @cmd: Session set application configuration command.
 * @dl_tdoa_ranging_method: dl tdoa ranging method.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_ranging_method(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_ranging_method)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_RANGING_METHOD,
		dl_tdoa_ranging_method);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_ranging_method() - Gets the dl tdoa ranging method used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_ranging_method: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_ranging_method(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_ranging_method);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_conf() - Sets the dl tdoa tx timestamp conf
 * @cmd: Session set application configuration command.
 * @dl_tdoa_tx_timestamp_conf: dl tdoa tx timestamp conf.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_tx_timestamp_conf(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_tx_timestamp_conf)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_TX_TIMESTAMP_CONF,
		dl_tdoa_tx_timestamp_conf);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_conf() - Gets the dl tdoa tx timestamp conf used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_tx_timestamp_conf: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_tx_timestamp_conf(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_tx_timestamp_conf);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_count() - Sets the dl tdoa hop count
 * @cmd: Session set application configuration command.
 * @dl_tdoa_hop_count: dl tdoa hop count.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_hop_count(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_hop_count)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_HOP_COUNT,
		dl_tdoa_hop_count);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_hop_count() - Gets the dl tdoa hop count used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_hop_count: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code cherry_uci_client_session_get_app_config_dl_tdoa_hop_count(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_hop_count);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfo() - Sets the dl tdoa anchor cfo.
 * @cmd: Session set application configuration command.
 * @dl_tdoa_anchor_cfo: dl tdoa anchor cfo.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_cfo(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_anchor_cfo)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_ANCHOR_CFO,
		dl_tdoa_anchor_cfo);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfo() - Gets the dl tdoa anchor cfo used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_anchor_cfo: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_anchor_cfo(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_anchor_cfo);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_rounds() - Sets the dl tdoa tx active ranging rounds.
 * @cmd: Session set application configuration command.
 * @dl_tdoa_tx_active_ranging_rounds: dl tdoa tx active ranging rounds.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tx_active_ranging_rounds(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_tx_active_ranging_rounds)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_TX_ACTIVE_RANGING_ROUNDS,
		dl_tdoa_tx_active_ranging_rounds);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tx_active_ranging_rounds() - Gets the dl tdoa tx active ranging rounds used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_tx_active_ranging_rounds: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tx_active_ranging_rounds(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_tx_active_ranging_rounds);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skipping() - Sets the dl tdoa block skipping.
 * @cmd: Session set application configuration command.
 * @dl_tdoa_block_skipping: dl tdoa block skipping.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_block_skipping(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_block_skipping)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_BLOCK_SKIPPING,
		dl_tdoa_block_skipping);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_block_skipping() - Gets the dl tdoa block skipping used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_block_skipping: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_block_skipping(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_block_skipping);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchor() - Sets the dl tdoa time reference anchor.
 * @cmd: Session set application configuration command.
 * @dl_tdoa_time_reference_anchor: dl tdoa time reference anchor.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_time_reference_anchor(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_time_reference_anchor)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_TIME_REFERENCE_ANCHOR,
		dl_tdoa_time_reference_anchor);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchor() - Gets the dl tdoa time reference anchor used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_time_reference_anchor: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_time_reference_anchor(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_time_reference_anchor);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tof() - Sets the dl tdoa responder tof.
 * @cmd: Session set application configuration command.
 * @dl_tdoa_responder_tof: dl tdoa responder tof.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_responder_tof(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t dl_tdoa_responder_tof)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_DL_TDOA_RESPONDER_TOF,
		dl_tdoa_responder_tof);
}

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_responder_tof() - Gets the dl tdoa responder tof used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_responder_tof: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_responder_tof(
	struct cherry_session_context *context, uint32_t session_handle,
	uint8_t *dl_tdoa_responder_tof);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location() - Sets the dl tdoa anchor location infos.
 * @cmd: Session set application configuration command.
 * @dl_tdoa_anchor_location: Parameters of DL-TDoA anchor location.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_dl_tdoa_anchor_location(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	const struct cherry_fira_anchor_location *dl_tdoa_anchor_location);

/**
 * cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location() - Gets the dl tdoa anchor location used in this session.
 * @context: Session context.
 * @session_handle: Session handle.
 * @dl_tdoa_anchor_location: variable to store the value
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_app_config_dl_tdoa_anchor_location(
	struct cherry_session_context *context, uint32_t session_handle,
	struct dl_tdoa_anchor_location *dl_tdoa_anchor_location);

/**
 * cherry_uci_client_session_free_status_ntf() - Free memory allocated in session status notification.
 * @ntf: Pointer to status notification data to free.
 */
void cherry_uci_client_session_free_status_ntf(struct session_status_ntf *ntf);

/**
 * cherry_uci_client_session_get_count() - Get sessions count.
 * @context: Session context.
 * @count: Session count.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_count(struct cherry_session_context *context,
				    int *count);

/**
 * cherry_uci_client_session_get_state() - Get session state.
 * @context: Session context.
 * @session_handle: Session ID.
 * @state: Session state.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum uci_status_code
cherry_uci_client_session_get_state(struct cherry_session_context *context,
				    uint32_t session_handle, int *state);

/**
 * cherry_uci_client_session_set_app_config_cmd_put_radar_timing_params() - Sets radar timing params.
 * @cmd: Session set application configuration command.
 * @burst_period_ms: Duration between the start of two consecutive Radar bursts in ms.
 * @sweep_period_rstu: Duration between the start of two consecutive Radar sweeps in RSTU.
 * @sweeps_per_burst: Number of Radar sweeps within the Radar.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_radar_timing_params(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint32_t burst_period_ms, uint16_t sweep_period_rstu,
	uint8_t sweeps_per_burst)
{
	uint8_t data[7];

	/*
	 * BURST_PERIOD (bytes 0-3): Duration between the start of two consecutive Radar bursts in ms.
	 * SWEEP_PERIOD (bytes 4-5): Duration between the start of two consecutive Radar sweeps in
	 * RSTU.
	 * SWEEPS_PER_BURST (byte 6): Number of Radar sweeps within the Radar.
	 */
	memcpy(data, &burst_period_ms, 4);
	memcpy(data + 4, &sweep_period_rstu, 2);
	memcpy(data + 6, &sweeps_per_burst, 1);

	return cherry_uci_client_session_set_app_config_cmd_put(
		cmd, UCI_APPLICATION_PARAMETER_RADAR_TIMING_PARAMS, data,
		sizeof(data));
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_radar_samples_per_sweep() - Sets radar samples per sweep.
 * @cmd: Session set application configuration command.
 * @samples_per_sweep: Number of samples per sweep. Possible values are {1 to 255} (Default = 64).
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_radar_samples_per_sweep(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t samples_per_sweep)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_RADAR_SAMPLES_PER_SWEEP,
		samples_per_sweep);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_radar_antenna_set_id() - Sets radar antenna set id.
 * @cmd: Session set application configuration command.
 * @antenna_set: Antenna set ID used by MAC_SET_CALIBRATIONS_CMD. Possible values are {0 to 3}
 * (Default: 0)
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_radar_antenna_set_id(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t antenna_set_id)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_ANTENNA_SET_ID, antenna_set_id);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_radar_max_burst() - Sets radar max bursts.
 * @cmd: Session set application configuration command.
 * @max_burst: maximum number of Radar bursts to be executed in a session.
 *
 * Configuration parameter to set maximum number of Radar bursts to be executed in a session.
 * The session is stopped and moved to SESSION_STATE_IDLE Session State when configured Radar bursts are elapsed.
 * Values can be:
 * 0x00: Unlimited (Default)
 * 0x01-0xFF: Number of bursts
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_radar_max_burst(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t max_burst)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_RADAR_NUMBER_OF_BURSTS,
		max_burst);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_radar_sweep_offset() - Sets sweep offset.
 * @cmd: Session set application configuration command.
 * @offset: Number of samples offset before First Path. Possible values are {-32768 to 32767}
 * (Default = -10)
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_radar_sweep_offset(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	int16_t offset)
{
	return cherry_uci_client_session_set_app_config_cmd_put_int16(
		cmd, UCI_APPLICATION_PARAMETER_RADAR_SWEEP_OFFSET, offset);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_radar_tx_profile_idx() - Sets tx profile idx.
 * @cmd: Session set application configuration command.
 * @idx: Radar Tx profile index
 * 0x00 = TX_PROFILE_HIGH (Default)
 * 0x01 = TX_PROFILE_LOW
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_radar_tx_profile_idx(
	struct cherry_uci_client_session_set_app_config_cmd *cmd, uint8_t idx)
{
	return cherry_uci_client_session_set_app_config_cmd_put_int8(
		cmd, UCI_APPLICATION_PARAMETER_TX_PROFILE_IDX, idx);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_uwb_config_id() - Sets the UWB
 * configuration identifier.
 * @cmd: Session set application configuration command.
 * @uwb_config_id: Configuration identifier.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_uwb_config_id(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint16_t uwb_config_id)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint16(
		cmd, UCI_APPLICATION_PARAMETER_SELECTED_UWB_CONFIG_ID,
		uwb_config_id);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_pulse_shape_combo() - Sets the pulse shape combinations.
 * @cmd: Session set application configuration command.
 * @pulse_shape_combo: Pulse shape combination.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_pulse_shape_combo(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t pulse_shape_combo)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_SELECTED_PULSE_SHAPE_COMBO,
		pulse_shape_combo);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sync_code_index() - Sets ync code index.
 * @cmd: Session set application configuration command.
 * @sync_code_index: Sync code index.
 * Possible values:
 *  9-12: BPRF
 *  25-32: HPRF
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_sync_code_index(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t sync_code_index)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_SYNC_CODE_INDEX,
		sync_code_index);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_sts_index0() - Sets the starting STS Index.
 * @cmd: Session set application configuration command.
 * @sts_index:  Starting STS Index.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_sts_index0(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	int32_t sts_index)
{
	return cherry_uci_client_session_set_app_config_cmd_put_int32(
		cmd, UCI_APPLICATION_PARAMETER_STS_INDEX0, sts_index);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_mac_mode() - Sets the mac mode.
 * @cmd: Session set application configuration command.
 * @sts_index:  Specify if one or multiple ranging round are used in a ranging block.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_mac_mode(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t mac_mode)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_MAC_MODE, mac_mode);
}

/**
 * cherry_uci_client_session_set_app_config_cmd_put_responder_slot_index() - Choose responder index.
 * @cmd: Session set application configuration command.
 * @responder_slot_index: Responder slot index.
 *
 * Return: UCI_STATUS_OK or error.
 */
static inline enum uci_status_code
cherry_uci_client_session_set_app_config_cmd_put_responder_slot_index(
	struct cherry_uci_client_session_set_app_config_cmd *cmd,
	uint8_t responder_slot_index)
{
	return cherry_uci_client_session_set_app_config_cmd_put_uint8(
		cmd, UCI_APPLICATION_PARAMETER_RESPONDER_SLOT_INDEX,
		responder_slot_index);
}

#endif /* CHERRY_SESSION_CLIENT_H */
