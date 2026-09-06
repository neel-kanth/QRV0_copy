/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2 OR GPL-2.0
 */

#pragma once

#define UCI_FIRA_PARAM_ID(param) QUWBS_FIRA_SESSION_PARAM_UID_##param
#define UCI_CCC_PARAM_ID(param) QUWBS_CCC_SESSION_PARAM_UID_##param
#define UCI_RADAR_PARAM_ID(param) QUWBS_RADAR_SESSION_PARAM_UID_##param

#define UCI_FIRA_TEST_VERSION_MAJOR 1
#define UCI_FIRA_TEST_VERSION_MINOR 1

/*
 * According to Table 35 - Fira Consortium UWB Command Interface Generic
 * Technical Specification 2.0.0
 */
#define UCI_FIRA_TWR_MEASUREMENT_DISTANCE_INVALID 0xffff

/**
 * enum uci_common_packet_header - Common Packet Header layout description
 * @UCI_COMMON_PACKET_HEADER_MT_OFFSET:
 *     Offset in the Common Packet Header Octet 0 of the MT field.
 * @UCI_COMMON_PACKET_HEADER_MT_MASK:
 *     Mask of the MT field once shifted down.
 * @UCI_COMMON_PACKET_HEADER_PBF_OFFSET:
 *     Offset in the Common Packet Header Octet 0 of the PBF field.
 * @UCI_COMMON_PACKET_HEADER_PBF_MASK:
 *     Mask of the PBF field once shifted down.
 * @UCI_COMMON_PACKET_HEADER_INFO_OFFSET:
 *     Offset in the Common Packet Header Octet 0 of the INFO field.
 * @UCI_COMMON_PACKET_HEADER_INFO_MASK:
 *     Mask of the INFO field once shifted down.
 */
enum uci_common_packet_header {
	UCI_COMMON_PACKET_HEADER_MT_OFFSET = 5,
	UCI_COMMON_PACKET_HEADER_MT_MASK = 0b111,
	UCI_COMMON_PACKET_HEADER_PBF_OFFSET = 4,
	UCI_COMMON_PACKET_HEADER_PBF_MASK = 1,
	UCI_COMMON_PACKET_HEADER_INFO_OFFSET = 0,
	UCI_COMMON_PACKET_HEADER_INFO_MASK = 0b1111,
};

/**
 * enum uci_message_pbf - PBF flag definition.
 *
 * @UCI_MESSAGE_PBF_COMPLETE:
 *     If the Packet contains a complete Message, the PBF SHALL be set to 0b0.
 *     If the Packet contains the last segment of a segmented Message, the PBF
 *     SHALL be set to 0b0.
 * @UCI_MESSAGE_PBF_SEGMENT:
 *     If the packet does not contain the last segment of a segmented Message,
 * the PBF SHALL be set to 0b1.
 */
enum uci_message_pbf {
	UCI_MESSAGE_PBF_COMPLETE = 0b0,
	UCI_MESSAGE_PBF_SEGMENT = 0b1,
};

/**
 * enum uci_message_type - Message type definition.
 *
 * @UCI_MESSAGE_TYPE_DATA:
 *     The message is a data packet
 * @UCI_MESSAGE_TYPE_COMMAND:
 *     The message is a command.
 * @UCI_MESSAGE_TYPE_RESPONSE:
 *     The message is a response to a command.
 * @UCI_MESSAGE_TYPE_NOTIFICATION:
 *     The message is a notification.
 * @UCI_MESSAGE_TYPE_SE_TESTING_COMMAND:
 *     The message is a test command for the Secure Element.
 * @UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE:
 *     The message is a test response from the Secure Element.
 */
enum uci_message_type {
	UCI_MESSAGE_TYPE_DATA = 0b000,
	UCI_MESSAGE_TYPE_COMMAND = 0b001,
	UCI_MESSAGE_TYPE_RESPONSE = 0b010,
	UCI_MESSAGE_TYPE_NOTIFICATION = 0b011,
	UCI_MESSAGE_TYPE_SE_TESTING_COMMAND = 0b100,
	UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE = 0b101,
	/* RFU 0b110 - 0b111 */
};
/**
 * enum uci_message_dpf - Message Data Packet Format (DPF) definition.
 *
 * @UCI_MESSAGE_DPF_SEND:
 *     Endpoint Format send (DATA_MESSAGE_SND)
 * @UCI_MESSAGE_DPF_RECEIVE:
 *     Endpoint Format receive (DATA_MESSAGE_RCV)
 * @UCI_MESSAGE_DPF_LL_SEND:
 *     Endpoint Format send (LL_DATA_MESSAGE_SND)
 * @UCI_MESSAGE_DPF_LL_RECEIVE:
 *     Endpoint Format receive (LL_DATA_MESSAGE_RCV)
 */
enum uci_message_dpf {
	/* UCI_MESSAGE_DPF_RFU = 0b0000,  */
	UCI_MESSAGE_DPF_SEND = 0b0001,
	UCI_MESSAGE_DPF_RECEIVE = 0b0010,
	UCI_MESSAGE_DPF_LL_SEND = 0b0011,
	UCI_MESSAGE_DPF_LL_RECEIVE = 0b0100,
	/* RFU 0b0101 - 0b1111 */
};

/**
 * enum uci_message_gid - Group identifiers definition.
 *
 * @UCI_GID_CORE:
 *     GID used for generic uci command on device.
 * @UCI_GID_SESSION_CONFIG:
 *     GID used for fira session command.
 * @UCI_GID_SESSION_CONTROL:
 *     GID used for starting stopping a session.
 * @UCI_GID_QORVO_EXT1:
 *     Secure Element group.
 * @UCI_GID_QORVO_EXT2:
 *     GID used for qorvo's additional physical tests and diagnostics.
 * @UCI_GID_ANDROID:
 *     GID used in Android HAL.
 * @UCI_GID_TEST:
 *     Test session group.
 * @UCI_GID_QORVO_MAC:
 *     GID used for uwbmac over uci feature.
 * @UCI_GID_QORVO_CALIB:
 *     Calibration reset tool.
 */
enum uci_message_gid {
	UCI_GID_CORE = 0b0000,
	UCI_GID_SESSION_CONFIG = 0b0001,
	UCI_GID_SESSION_CONTROL = 0b0010,
	/* RFU 0b0011 - 0b1000 */
	/* Vendor 0b1001 - 0b1100 */
	UCI_GID_QORVO_EXT1 = 0b1001,
	UCI_GID_QORVO_EXT2 = 0b1011,
	UCI_GID_ANDROID = 0b1100,
	/* spec PCTT */
	UCI_GID_TEST = 0b1101,
	/* Vendor 0b1110 - 0b1111 */
	UCI_GID_QORVO_MAC = 0b1110,
	/* TODO: to be removed and merged with UCI_GID_QORVO_MAC. */
	UCI_GID_QORVO_CALIB = 0b1111,
};

/**
 * enum uci_oid_core - Opcode identifiers for UCI_GID_CORE.
 *
 * @UCI_OID_CORE_DEVICE_RESET:
 *     Reset device.
 * @UCI_OID_CORE_DEVICE_STATUS:
 *     Device state notification (ready, active, error).
 * @UCI_OID_CORE_GET_DEVICE_INFO:
 *     Get UCI version information as well as device specific ones.
 * @UCI_OID_CORE_GET_CAPS_INFO:
 *     Get device capabilities.
 * @UCI_OID_CORE_SET_CONFIG:
 *     Set device configuration.
 * @UCI_OID_CORE_GET_CONFIG:
 *     Get device configuration.
 * @UCI_OID_CORE_GENERIC_ERROR:
 *     Error notification.
 * @UCI_OID_CORE_QUERY_UWBS_TIMESTAMP:
 *     Get timestamp.
 */
enum uci_oid_core {
	UCI_OID_CORE_DEVICE_RESET = 0b000000,
	UCI_OID_CORE_DEVICE_STATUS = 0b000001,
	UCI_OID_CORE_GET_DEVICE_INFO = 0b000010,
	UCI_OID_CORE_GET_CAPS_INFO = 0b000011,
	UCI_OID_CORE_SET_CONFIG = 0b000100,
	UCI_OID_CORE_GET_CONFIG = 0b000101,
	/* RFU = 0b000110 */
	UCI_OID_CORE_GENERIC_ERROR = 0b000111,
	UCI_OID_CORE_QUERY_UWBS_TIMESTAMP = 0b001000,
	/* RFU = 0b111000 - 0b111111 */
};

/**
 * enum uci_oid_session_config - Opcode identifiers for UCI_GID_SESSION_CONFIG.
 *
 * @UCI_OID_SESSION_CONFIG_SESSION_INIT:
 *     Init session.
 * @UCI_OID_SESSION_CONFIG_SESSION_DEINIT:
 *     Deinit session.
 * @UCI_OID_SESSION_CONFIG_SESSION_STATUS:
 *     Session state notification (init, idle, active, deinit).
 * @UCI_OID_SESSION_CONFIG_SESSION_SET_APP_CONFIG:
 *     Set session configuration.
 * @UCI_OID_SESSION_CONFIG_SESSION_GET_APP_CONFIG:
 *     Get session configuration.
 * @UCI_OID_SESSION_CONFIG_SESSION_GET_COUNT:
 *     Get the number of session in use.
 * @UCI_OID_SESSION_CONFIG_SESSION_GET_STATE:
 *     Get the state of one session.
 * @UCI_OID_SESSION_CONFIG_SESSION_UPDATE_CONTROLLER_MULTICAST_LIST:
 *     Update the list of controlee of a session.
 * @UCI_OID_SESSION_CONFIG_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS:
 *     Configure the active ranging rounds of DT-Anchor, ranging
 *     roles for each active round and lists of Responders for each
 *     active round in which DT-Anchor acts as Initiator.
 * @UCI_OID_SESSION_CONFIG_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS:
 *     Configure the active ranging rounds of DT-Tag.
 * @UCI_OID_SESSION_CONFIG_SESSION_QUERY_DATA_SIZE_IN_RANGING:
 *     Get maximum Application Data size in Ranging Round.
 * @UCI_OID_SESSION_CONFIG_SESSION_SET_HUS_CONTROLLER_CONFIG:
 *     Configure phases of a HUS ranging round on a Controller device.
 * @UCI_OID_SESSION_CONFIG_SESSION_SET_HUS_CONTROLEE_CONFIG:
 *     Configure phases of a HUS ranging round on a Controlee device.
 * @UCI_OID_SESSION_CONFIG_SESSION_DATA_TRANSFER_PHASE_CONFIG:
 *     Configure slots allocations for data transfer.
 */
enum uci_oid_session_config {
	UCI_OID_SESSION_CONFIG_SESSION_INIT = 0b000000,
	UCI_OID_SESSION_CONFIG_SESSION_DEINIT = 0b000001,
	UCI_OID_SESSION_CONFIG_SESSION_STATUS = 0b000010,
	UCI_OID_SESSION_CONFIG_SESSION_SET_APP_CONFIG = 0b000011,
	UCI_OID_SESSION_CONFIG_SESSION_GET_APP_CONFIG = 0b000100,
	UCI_OID_SESSION_CONFIG_SESSION_GET_COUNT = 0b000101,
	UCI_OID_SESSION_CONFIG_SESSION_GET_STATE = 0b000110,
	UCI_OID_SESSION_CONFIG_SESSION_UPDATE_CONTROLLER_MULTICAST_LIST =
		0b000111,
	UCI_OID_SESSION_CONFIG_SESSION_UPDATE_DT_ANCHOR_RANGING_ROUNDS =
		0b001000,
	UCI_OID_SESSION_CONFIG_SESSION_UPDATE_DT_TAG_RANGING_ROUNDS = 0b001001,
	UCI_OID_SESSION_CONFIG_SESSION_QUERY_DATA_SIZE_IN_RANGING = 0b001011,
	UCI_OID_SESSION_CONFIG_SESSION_SET_HUS_CONTROLLER_CONFIG = 0b001100,
	UCI_OID_SESSION_CONFIG_SESSION_SET_HUS_CONTROLEE_CONFIG = 0b001101,
	UCI_OID_SESSION_CONFIG_SESSION_DATA_TRANSFER_PHASE_CONFIG = 0b001110,
	/* RFU 0b001111 - 0b111111 */
};

/**
 * enum uci_oid_session_control - Opcode identifiers for UCI_GID_SESSION_CONTROL.
 *
 * @UCI_OID_SESSION_START:
 *     Start session.
 * @UCI_OID_SESSION_INFO:
 *     Ranging session notification.
 * @UCI_OID_SESSION_STOP:
 *     Stop session.
 * @UCI_OID_SESSION_GET_RANGING_COUNT:
 *     Get the number of ranging done by an active session.
 * @UCI_OID_SESSION_DATA_CREDIT:
 *     Notification to indicate that data packet is successfully received by UWBS.
 * @UCI_OID_SESSION_DATA_TRANSFER_STATUS:
 *     Notification to indicate the status of in-band data transfer.
 * @UCI_OID_LOGICAL_LINK_CREATE:
 *     Create a logical link for data exchange.
 * @UCI_OID_LOGICAL_LINK_CLOSE:
 *     Close a logical link for data exchange.
 * @UCI_OID_LOGICAL_LINK_UWBS_CLOSE:
 *     Notification sent by UWBS to indicate release of a logical link.
 * @UCI_OID_LOGICAL_LINK_UWBS_CREATE:
 *     Notification sent by UWBS to indicate creation of a logical link.
 * @UCI_OID_LOGICAL_LINK_GET_PARAM:
 *     Get logical link layer configuration parameters.
 */
enum uci_oid_session_control {
	UCI_OID_SESSION_START = 0b000000,
	UCI_OID_SESSION_INFO = UCI_OID_SESSION_START,
	UCI_OID_SESSION_STOP = 0b000001,
	UCI_OID_SESSION_GET_RANGING_COUNT = 0b000011,
	UCI_OID_SESSION_DATA_CREDIT = 0b000100,
	UCI_OID_SESSION_DATA_TRANSFER_STATUS = 0b000101,
	UCI_OID_LOGICAL_LINK_CREATE = 0b000111,
	UCI_OID_LOGICAL_LINK_CLOSE = 0b001000,
	UCI_OID_LOGICAL_LINK_UWBS_CLOSE = 0b001001,
	UCI_OID_LOGICAL_LINK_UWBS_CREATE = 0b001010,
	UCI_OID_LOGICAL_LINK_GET_PARAM = 0b001011,
	/* RFU 0b001100 - 0b111111 */
};

/**
 * enum uci_oid_test - Opcode identifiers for UCI_GID_TEST.
 *
 * @UCI_OID_TEST_CONFIG_SET:
 *     Set configuration specific to testing.
 * @UCI_OID_TEST_CONFIG_GET:
 *     Get test configuration.
 * @UCI_OID_TEST_PERIODIC_TX:
 *     Execute periodic tx test.
 * @UCI_OID_TEST_PER_RX:
 *     Execute per rx test.
 * @UCI_OID_TEST_RX:
 *     Execute rx test.
 * @UCI_OID_TEST_LOOPBACK:
 *     Execute loopback test.
 * @UCI_OID_TEST_STOP_SESSION:
 *     Stop a test.
 * @UCI_OID_TEST_SS_TWR:
 *     Execute ss twr test.
 * @UCI_OID_TEST_SR_RX:
 *     Execute sr rx test.
 */
enum uci_oid_test {
	UCI_OID_TEST_CONFIG_SET = 0b000000,
	UCI_OID_TEST_CONFIG_GET = 0b000001,
	UCI_OID_TEST_PERIODIC_TX = 0b000010,
	UCI_OID_TEST_PER_RX = 0b000011,
	UCI_OID_TEST_RX = 0b000101,
	UCI_OID_TEST_LOOPBACK = 0b000110,
	UCI_OID_TEST_STOP_SESSION = 0b000111,
	UCI_OID_TEST_SS_TWR = 0b001000,
	UCI_OID_TEST_SR_RX = 0b001001,
	/* RFU 0b000100 */
	/* RFU 0b001010 - 0b111111 */
};

/**
 * enum uci_status_code - Status of a command.
 *
 * @UCI_STATUS_OK:
 *     Success.
 * @UCI_STATUS_REJECTED:
 *     Rejected.
 * @UCI_STATUS_FAILED:
 *     Unknown failure.
 * @UCI_STATUS_SYNTAX_ERROR:
 *     Syntax error in the UCI command.
 * @UCI_STATUS_INVALID_PARAM:
 *     A parameter was not valid.
 * @UCI_STATUS_INVALID_RANGE:
 *     A known parameter value was out of possible range.
 * @UCI_STATUS_INVALID_MESSAGE_SIZE:
 *     The size of the command message was wrong.
 * @UCI_STATUS_UNKNOWN_GID:
 *     Unknow GID.
 * @UCI_STATUS_UNKNOWN_OID:
 *     Unknow OID.
 * @UCI_STATUS_READ_ONLY:
 *     Tried to set a read only value.
 * @UCI_STATUS_UCI_MESSAGE_RETRY:
 *     UWBS request retransmission from Host.
 * @UCI_STATUS_UNKNOWN:
 *     It is not known whether the intended operation was failed or successful.
 * @UCI_STATUS_NOT_APPLICABLE:
 *     The parameter is not applicable for the requested operation.
 * @UCI_STATUS_ERROR_SESSION_NOT_EXIST:
 *     Given session does not exist.
 * @STATUS_ERROR_INVALID_PHASE_PARTICIPATION:
 *    Invalid phase participation values during primary/secondary sessions binding.
 * @UCI_STATUS_ERROR_SESSION_ACTIVE:
 *     Session is active.
 * @UCI_STATUS_ERROR_MAX_SESSIONS_EXCEEDED:
 *     Max. number of sessions already created.
 * @UCI_STATUS_ERROR_SESSION_NOT_CONFIGURED:
 *     Session is not configured with required app configurations.
 * @UCI_STATUS_ERROR_ACTIVE_SESSIONS_ONGOING:
 *     Sessions are actively running in UWBS.
 * @UCI_STATUS_ERROR_MULTICAST_LIST_FULL:
 *     Indicates when multicast list is full during one to many ranging.
 * @UCI_STATUS_ERROR_ADDRESS_NOT_FOUND:
 *     FiRa 1.3 only, RFU on newer version.
 *     Indicate one controlee of the update multicast list command was not
 *     found.
 * @UCI_STATUS_ERROR_UWB_INITIATION_TIME_TOO_OLD:
 *     The current UWBS time has gone past the configured UWB_INITIATION_TIME.
 * @UCI_STATUS_OK_NEGATIVE_DISTANCE_REPORT:
 *     Success.A negative distance was measured.
 * @UCI_STATUS_RANGING_TX_FAILED:
 *     Failed to transmit UWB packet.
 * @UCI_STATUS_RANGING_RX_TIMEOUT:
 *     No UWB packet detected by the receiver.
 * @UCI_STATUS_RANGING_RX_PHY_DEC_FAILED:
 *     UWB packet channel decoding error.
 * @UCI_STATUS_RANGING_RX_PHY_TOA_FAILED:
 *     Failed to detect time of arrival of the UWB packet from CIR samples.
 * @UCI_STATUS_RANGING_RX_PHY_STS_FAILED:
 *     UWB packet STS segment mismatch.
 * @UCI_STATUS_RANGING_RX_MAC_DEC_FAILED:
 *     MAC CRC or syntax error.
 * @UCI_STATUS_RANGING_RX_MAC_IE_DEC_FAILED:
 *     IE syntax error.
 * @UCI_STATUS_RANGING_RX_MAC_IE_MISSING:
 *     Expected IE missing in the packet.
 * @UCI_STATUS_ERROR_ROUND_INDEX_NOT_ACTIVATED:
 *     Configured DL-TDoA Ranging Round index could not be activated.
 * @UCI_STATUS_ERROR_NUMBER_OF_ACTIVE_RANGING_ROUNDS_EXCEEDED:
 *     Number of active ranging rounds exceeds the maximum number of ranging
 *     rounds supported.
 * @UCI_STATUS_ERROR_DL_TDOA_DEVICE_ADDRESS_NOT_MATCHING_IN_REPLY_TIME_LIST:
 *     Received DL-TDoA Reply Time List does not contain the Initiator Reply
 *     Time associated to the MAC address in RDM List.
 * @UCI_STATUS_ERROR_SE_BUSY:
 *     Proprietary CCC error: Secure Element is busy.
 * @UCI_STATUS_ERROR_CCC_LIFECYCLE:
 *     Proprietary CCC error: lifecycle has not been respected.
 * @UCI_STATUS_LAST:
 *     Internal use.
 */
enum uci_status_code {
	/* Generic Status Codes */
	UCI_STATUS_OK = 0x00,
	UCI_STATUS_REJECTED = 0x01,
	UCI_STATUS_FAILED = 0x02,
	UCI_STATUS_SYNTAX_ERROR = 0x03,
	UCI_STATUS_INVALID_PARAM = 0x04,
	UCI_STATUS_INVALID_RANGE = 0x05,
	UCI_STATUS_INVALID_MESSAGE_SIZE = 0x06,
	UCI_STATUS_UNKNOWN_GID = 0x07,
	UCI_STATUS_UNKNOWN_OID = 0x08,
	UCI_STATUS_READ_ONLY = 0x09,
	UCI_STATUS_UCI_MESSAGE_RETRY = 0x0a,
	UCI_STATUS_UNKNOWN = 0x0b,
	UCI_STATUS_NOT_APPLICABLE = 0x0c,
	/* RFU 0x0b - 0x0f */

	/* UWB Session Specific Status Codes */
	/* Note: 0x10 is missing from the documentation */
	UCI_STATUS_ERROR_SESSION_NOT_EXIST = 0x11,
	STATUS_ERROR_INVALID_PHASE_PARTICIPATION = 0x12,
	UCI_STATUS_ERROR_SESSION_ACTIVE = 0x13,
	UCI_STATUS_ERROR_MAX_SESSIONS_EXCEEDED = 0x14,
	UCI_STATUS_ERROR_SESSION_NOT_CONFIGURED = 0x15,
	UCI_STATUS_ERROR_ACTIVE_SESSIONS_ONGOING = 0x16,
	UCI_STATUS_ERROR_MULTICAST_LIST_FULL = 0x17,
	/* 0x18 is defined in 1.3, but RFU starting 2.0 */
	UCI_STATUS_ERROR_ADDRESS_NOT_FOUND = 0x18,
	/* RFU 0x18 - 0x19 */
	UCI_STATUS_ERROR_UWB_INITIATION_TIME_TOO_OLD = 0x1a,
	UCI_STATUS_OK_NEGATIVE_DISTANCE_REPORT = 0x1b,
	/* RFU 0x1c - 0x1f */

	/* UWB Ranging Session Specific Status Codes */
	UCI_STATUS_RANGING_TX_FAILED = 0x20,
	UCI_STATUS_RANGING_RX_TIMEOUT = 0x21,
	UCI_STATUS_RANGING_RX_PHY_DEC_FAILED = 0x22,
	UCI_STATUS_RANGING_RX_PHY_TOA_FAILED = 0x23,
	UCI_STATUS_RANGING_RX_PHY_STS_FAILED = 0x24,
	UCI_STATUS_RANGING_RX_MAC_DEC_FAILED = 0x25,
	UCI_STATUS_RANGING_RX_MAC_IE_DEC_FAILED = 0x26,
	UCI_STATUS_RANGING_RX_MAC_IE_MISSING = 0x27,
	UCI_STATUS_ERROR_ROUND_INDEX_NOT_ACTIVATED = 0x28,
	UCI_STATUS_ERROR_NUMBER_OF_ACTIVE_RANGING_ROUNDS_EXCEEDED = 0x29,
	UCI_STATUS_ERROR_DL_TDOA_DEVICE_ADDRESS_NOT_MATCHING_IN_REPLY_TIME_LIST =
		0x2a,
	/* RFU 0x2c - 0x4f */

	/* Proprietary Status Codes 0x50 - 0xff */
	UCI_STATUS_ERROR_SE_BUSY = 0x50,
	UCI_STATUS_ERROR_CCC_LIFECYCLE = 0x51,
	UCI_STATUS_LAST,
};

/**
 * enum uci_data_transfer_status_code - Status Code values in the
 * SESSION_DATA_TRANSFER_STATUS_NTF
 *
 * @UCI_DATA_TRANSFER_STATUS_REPETITION_OK:
 *     When DATA_REPETITION_COUNT > 0 and
 *     SESSION_DATA_TRANSFER_STATUS_NTF_CONFIG = Enable it indicates that one
 *     data transmission is completed in a RR.
 * @UCI_DATA_TRANSFER_STATUS_OK:
 *     Complete Application Data has been fully transmitted over UWB.
 * @UCI_DATA_TRANSFER_STATUS_ERROR_DATA_TRANSFER:
 *     Application Data couldn’t be sent due to an unrecoverable error.
 * @UCI_DATA_TRANSFER_STATUS_ERROR_NO_CREDIT_AVAILABLE:
 *     DATA_MESSAGE_SND is not accepted as no credit is available.
 * @UCI_DATA_TRANSFER_STATUS_ERROR_REJECTED:
 *     DATA_MESSAGE_SND packet sent in wrong state or Application Data Size
 *     exceeds the maximum size that can be sent in one Ranging Round.
 * @UCI_DATA_TRANSFER_STATUS_SESSION_TYPE_NOT_SUPPORTED:
 *     Data transfer is not supported for given session type.
 * @UCI_DATA_TRANSFER_STATUS_ERROR_DATA_TRANSFER_IS_ONGOING:
 *     Application Data is being transmitted and the number of configured
 *     DATA_REPETITION_COUNT transmissions is not yet completed.
 * @UCI_DATA_TRANSFER_STATUS_INVALID_FORMAT:
 *     The format of the command DATA_MESSAGE_SND associated with this
 * notification is incorrect (e.g, a parameter is missing, a parameter value is
 * invalid).
 * @UCI_DATA_TRANSFER_STATUS_LAST:
 *     Internal use.
 */
enum uci_data_transfer_status_code {
	UCI_DATA_TRANSFER_STATUS_REPETITION_OK = 0x00,
	UCI_DATA_TRANSFER_STATUS_OK = 0x01,
	UCI_DATA_TRANSFER_STATUS_ERROR_DATA_TRANSFER = 0x02,
	UCI_DATA_TRANSFER_STATUS_ERROR_NO_CREDIT_AVAILABLE = 0x03,
	UCI_DATA_TRANSFER_STATUS_ERROR_REJECTED = 0x04,
	UCI_DATA_TRANSFER_STATUS_SESSION_TYPE_NOT_SUPPORTED = 0x05,
	UCI_DATA_TRANSFER_STATUS_ERROR_DATA_TRANSFER_IS_ONGOING = 0x06,
	UCI_DATA_TRANSFER_STATUS_INVALID_FORMAT = 0x07,
	/* RFU 0x08 - 0x1f */
	UCI_DATA_TRANSFER_STATUS_LAST,
};

/**
 * enum uci_device_state - Device state.
 *
 * @UCI_DEVICE_STATE_READY:
 *     Device is ready.
 * @UCI_DEVICE_STATE_ACTIVE:
 *     Device is performing a communication or ranging.
 * @UCI_DEVICE_STATE_ERROR:
 *     Device needs to be reset.
 */
enum uci_device_state {
	/* RFU 0x00 */
	UCI_DEVICE_STATE_READY = 0x01,
	UCI_DEVICE_STATE_ACTIVE = 0x02,
	/* RFU 0x03 - 0xfe */
	UCI_DEVICE_STATE_ERROR = 0xff,
};

/**
 * enum uci_device_configuration_parameters - Device configuration.
 *
 * @UCI_DEVICE_PARAMETER_DEVICE_STATE:
 *     Read only device state.
 * @UCI_DEVICE_PARAMETER_LOW_POWER_MODE:
 *     Low power mode.
 *
 * For MCPS specific parameters, see ``enum device_config`` in
 * ``uci_spec_mcps.h``.
 */
enum uci_device_configuration_parameters {
	UCI_DEVICE_PARAMETER_DEVICE_STATE = 0x00,
	UCI_DEVICE_PARAMETER_LOW_POWER_MODE = 0x01,
	/* RFU 0x02 - 0x9f */
	/* Reserved for Vendor Specific 0xa0 - 0xdf */
	/* RFU 0xe0 - 0xe2 */
	/* Vendor Specific parameters 0xe3 - 0xff */
};

/**
 * enum uci_device_type - Device type.
 *
 * @UCI_DEVICE_TYPE_CONTROLEE:
 *     Controlee.
 * @UCI_DEVICE_TYPE_CONTROLLER:
 *     Controller.
 */
enum uci_device_type {
	UCI_DEVICE_TYPE_CONTROLEE = 0x00,
	UCI_DEVICE_TYPE_CONTROLLER = 0x01,
	/* RFU 0x02 - 0xff */
};

/**
 * enum uci_ranging_round_usage - Ranging Round usage
 *
 * @UCI_SSTWR_DEFERRED:
 *     SS-TWR with Deferred Mode.
 * @UCI_DSTWR_DEFERRED:
 *     DS-TWR with Deferred Mode.
 * @UCI_SSTWR_NON_DEFERRED:
 *     SS-TWR with Non-Deferred mode.
 * @UCI_DSTWR_NON_DEFERRED:
 *     DS-TWR with Non-Deferred mode.
 * @UCI_OWR_DL_TDOA:
 *     One Way Ranging DL-TDoA.
 * @UCI_OWR_AOA:
 *     OWR for AoA measurement.
 * @UCI_ESS_TWR_NON_DEFERRED_CONTENTION_BASED:
 *     eSS-TWR with Non-deferred Mode for Contention-based ranging.
 * @UCI_ADS_TWR_CONTENTION_BASED:
 *     aDS-TWR for Contention-based ranging.
 */
enum uci_ranging_round_usage {
	UCI_SSTWR_DEFERRED = 0x01,
	UCI_DSTWR_DEFERRED = 0x02,
	UCI_SSTWR_NON_DEFERRED = 0x03,
	UCI_DSTWR_NON_DEFERRED = 0x04,
	UCI_OWR_DL_TDOA = 0x05,
	UCI_OWR_AOA = 0x06,
	UCI_ESS_TWR_NON_DEFERRED_CONTENTION_BASED = 0x07,
	UCI_ADS_TWR_CONTENTION_BASED = 0x08,
	/* RFU 0x09 - 0xff */
};

/**
 * enum uci_ranging_round_control - Ranging Round Control
 *	b0: Send Ranging Result Report Message (RRRM).
 *	b1: Send Control Message in-band (currently always set).
 *	b2: Skip Ranging Control Phase (RCP) in non-deferred mode.
 *
 * @UCI_RRRM_DISABLED_RCP_ENABLED:
 *	Ranging Result Report Message disabled,
 *	Ranging Control Phase included in non-deferred mode.
 * @UCI_RRRM_ENABLED_RCP_ENABLED:
 *	Ranging Result Report Message enabled,
 *	Ranging Control Phase included in non-deferred mode.
 * @UCI_RRRM_DISABLED_RCP_DISABLED:
 *	Ranging Result Report Message disabled,
 *	Ranging Control Phase skipped in non-deferred mode.
 * @UCI_RRRM_ENABLED_RCP_DISABLED:
 *	Ranging Result Report Message enabled,
 *	Ranging Control Phase skipped in non-deferred mode.
 */
enum uci_ranging_round_control {
	UCI_RRRM_DISABLED_RCP_ENABLED = 0x02,
	UCI_RRRM_ENABLED_RCP_ENABLED = 0x03,
	UCI_RRRM_DISABLED_RCP_DISABLED = 0x06,
	UCI_RRRM_ENABLED_RCP_DISABLED = 0x07,
};

/**
 * enum uci_device_role - Device role.
 *
 * @UCI_DEVICE_ROLE_RESPONDER:
 *     Responder.
 * @UCI_DEVICE_ROLE_INITIATOR:
 *     Initiator.
 * @UCI_DEVICE_ROLE_ADVERTISER:
 *     Advertiser.
 * @UCI_DEVICE_ROLE_OBSERVER:
 *     Observer.
 */
enum uci_device_role {
	UCI_DEVICE_ROLE_RESPONDER = 0x00,
	UCI_DEVICE_ROLE_INITIATOR = 0x01,
	/* RFU 0x02 - 0x04 */
	UCI_DEVICE_ROLE_ADVERTISER = 0x05,
	UCI_DEVICE_ROLE_OBSERVER = 0x06,
	/* RFU 0x07 - 0xff */
};

/**
 * enum uci_test_configuration_parameters - Test configuration.
 *
 * @UCI_TEST_PARAMETER_NUM_PACKETS:
 *     Number of packets.
 * @UCI_TEST_PARAMETER_T_GAP:
 *     T gap.
 * @UCI_TEST_PARAMETER_T_START:
 *     T start.
 * @UCI_TEST_PARAMETER_T_WIN:
 *     T win.
 * @UCI_TEST_PARAMETER_RANDOMIZE_PSDU:
 *     Randomize psdu.
 * @UCI_TEST_PARAMETER_PHR_RANGING_BIT:
 *     PHR ranging bit.
 * @UCI_TEST_PARAMETER_RMARKER_TX_START:
 *     R marker tx start.
 * @UCI_TEST_PARAMETER_RMARKER_RX_START:
 *     R marker rx start.
 * @UCI_TEST_PARAMETER_STS_INDEX_AUTO_INCR:
 *     STS index auto incr.
 * @UCI_TEST_PARAMETER_STS_DETECT_BITMAP_EN:
 *     STS detection bitmap enable.
 * @UCI_TEST_PARAMETER_RSSI_OUTLIERS:
 *     Number of outliers to remove.
 */
enum uci_test_configuration_parameters {
	UCI_TEST_PARAMETER_NUM_PACKETS = 0x00,
	UCI_TEST_PARAMETER_T_GAP = 0x01,
	UCI_TEST_PARAMETER_T_START = 0x02,
	UCI_TEST_PARAMETER_T_WIN = 0x03,
	UCI_TEST_PARAMETER_RANDOMIZE_PSDU = 0x04,
	UCI_TEST_PARAMETER_PHR_RANGING_BIT = 0x05,
	UCI_TEST_PARAMETER_RMARKER_TX_START = 0x06,
	UCI_TEST_PARAMETER_RMARKER_RX_START = 0x07,
	UCI_TEST_PARAMETER_STS_INDEX_AUTO_INCR = 0x08,
	UCI_TEST_PARAMETER_STS_DETECT_BITMAP_EN = 0x09,
	/* RFU 0x09 - 0xff */
	/* Reserved for extension of ID 0xe0 - 0xe4 */
	/* Proprietary 0xe5 - 0xff */
	UCI_TEST_PARAMETER_RSSI_OUTLIERS = 0xeb
};

enum uci_device_capability_parameters {
	UCI_CAP_MAX_DATA_MESSAGE_SIZE = 0x00,
	UCI_CAP_MAX_DATA_PACKET_PAYLOAD_SIZE = 0x01,
	UCI_CAP_FIRA_PHY_VERSION_RANGE = 0x02,
	UCI_CAP_FIRA_MAC_VERSION_RANGE = 0x03,
	UCI_CAP_DEVICE_TYPES = 0x04,
	UCI_CAP_DEVICE_ROLES = 0x05,
	UCI_CAP_RANGING_METHOD = 0x06,
	UCI_CAP_STS_CONFIG = 0x07,
	UCI_CAP_MULTI_NODE_MODE = 0x08,
	UCI_CAP_RANGING_TIME_STRUCT = 0x09,
	UCI_CAP_SCHEDULE_MODE = 0x0a,
	UCI_CAP_HOPPING_MODE = 0x0b,
	UCI_CAP_BLOCK_STRIDING = 0x0c,
	UCI_CAP_UWB_INITIATION_TIME = 0x0d,
	UCI_CAP_CHANNELS = 0x0e,
	UCI_CAP_RFRAME_CONFIG = 0x0f,
	UCI_CAP_CC_CONSTRAINT_LENGTH = 0x10,
	UCI_CAP_BPRF_PARAMETER_SETS = 0x11,
	UCI_CAP_HPRF_PARAMETER_SETS = 0x12,
	UCI_CAP_AOA_SUPPORT = 0x13,
	UCI_CAP_EXTENDED_MAC_ADDRESS = 0x14,
	UCI_CAP_SUSPEND_RANGING_SUPPORT = 0x15,
	UCI_CAP_SESSION_KEY_LENGTHS = 0x16,
	UCI_CAP_DT_ANCHOR_MAX_ACTIVE_RR = 0x17,
	UCI_CAP_DT_TAG_MAX_ACTIVE_RR = 0x18,
	UCI_CAP_DL_TDOA_BLOCK_SKIPPING = 0x19,
	UCI_CAP_PSDU_LENGTH_SUPPORT = 0x1a,
	UCI_CAP_LL_CAPABILITY_PARAM = 0x1b,
	UCI_CAP_BYPASS_MODE_SUPPORT = 0x1c,
	/* RFU 0x1d - 0x9f */
	/* VENDOR SPECIFIC 0xa0 - 0xdf */
	/* RESERVED FOR EXTENSION OF IDS 0xe0 - 0xe2 */
	/* VENDOR SPECIFIC APP CONFIG 0xe3 - 0xff */
};

/**
 * enum uci_device_capability_aosp - Vendor device capabilities for Android.
 *
 * Up-to-date documentation and definition is available in
 * hardware/uwb/fira_android/UwbVendorCapabilityTlvTypes.aidl.
 *
 * All values are prefixed with AOSP_CAPS compared to their original definition.
 */
enum uci_device_capability_aosp {
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_CHAPS_PER_SLOT: 1 byte bitmask with a list
	 *of supported chaps per slot
	 *
	 * Bitmap of supported values of Slot durations as a multiple of TChap,
	 * NChap_per_Slot as defined in CCC Specification.
	 * Each “1” in this bit map corresponds to a specific
	 * value of NChap_per_Slot where:
	 * 0x01 = “3”,
	 * 0x02 = “4”,
	 * 0x04= “6”,
	 * 0x08 =“8”,
	 * 0x10 =“9”,
	 * 0x20 = “12”,
	 * 0x40 = “24”,
	 * 0x80 is reserved.
	 */
	AOSP_CAPS_CCC_SUPPORTED_CHAPS_PER_SLOT = 0xa0,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_SYNC_CODES: 4 byte bitmask with a list of
	 *		supported sync codes.
	 *
	 * Bitmap of SYNC code indices that can be used.
	 * The position of each “1” in this bit pattern
	 * corresponds to the index of a SYNC code that
	 * can be used, where:
	 * 0x00000001 = “1”,
	 * 0x00000002 = “2”,
	 * 0x00000004 = “3”,
	 * 0x00000008 = “4”,
	 * ….
	 * 0x40000000 = “31”,
	 * 0x80000000 = “32”
	 * Refer to IEEE 802.15.4-2015 and CCC
	 * Specification for SYNC code index definition
	 */
	AOSP_CAPS_CCC_SUPPORTED_SYNC_CODES = 0xa1,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_HOPPING_CONFIG_MODES_AND_SEQUENCES: 1 byte
	 * bitmask with a list of supported hopping config modes
	 * and sequences.
	 *
	 * **Legacy Android CCC:**
	 *
	 * [b7 b6 b5] : bitmask of hopping modes the
	 * device offers to use in the ranging session
	 * 100 - No Hopping
	 * 010 - Continuous Hopping
	 * 001 - Adaptive Hopping
	 * [b4 b3 b2 b1 b0] : bit mask of hopping
	 * sequences the device offers to use in the
	 * ranging session
	 * b4=1 is always set because of the default
	 * hopping sequence. Support for it is mandatory.
	 * b3=1 is set when the optional AES based
	 * hopping sequence is supported.
	 *
	 * **JumpWG:**
	 *
	 * [b0]    : AES-based hopping sequence
	 * [b1]    : Default hopping sequence (always set)
	 * [b2]    : Adaptive hopping mode
	 * [b3]    : Continuous hopping mode (always set)
	 * [b4]    : No hopping
	 * [b5-b7] : Reserved for future use (RFU)
	 *
	 * For more details about hopping modes and sequences, see :cite:`2022:ccc_specification`.
	 */
	AOSP_CAPS_CCC_SUPPORTED_HOPPING_CONFIG_MODES_AND_SEQUENCES = 0xa2,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_CHANNELS: 1 byte bitmask with list of
	 *supported channels.
	 *
	 * Bitmap of supported UWB channels. Each “1” in
	 * this bit map corresponds to a specific value of
	 * UWB channel where:
	 * 0x01 = "Channel 5"
	 * 0x02 = "Channel 9"
	 */
	AOSP_CAPS_CCC_SUPPORTED_CHANNELS = 0xa3,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_VERSIONS: Supported CCC version.
	 *
	 * 2 byte tuple {major_version (1 byte), minor_version (1 byte)} array
	 * with list of supported CCC versions
	 */
	AOSP_CAPS_CCC_SUPPORTED_VERSIONS = 0xa4,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_UWB_CONFIGS: A byte array in `legacy_android_ccc` and a short
	 * array in `JumpWG`, containing a list of supported UWB configs.
	 *
	 * UWB configurations are defined in chapter
	 * "21.4 UWB Frame Elements" of the CCC
	 * specification. Configuration 0x0000 is
	 * mandatory for device and vehicle, configuration
	 * 0x0001 is mandatory for the device, optional for
	 * the vehicle.
	 */
	AOSP_CAPS_CCC_SUPPORTED_UWB_CONFIGS = 0xa5,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_PULSE_SHAPE_COMBOS: supported CCC pulse
	 *shape combo.
	 *
	 * 1 byte tuple {initiator_tx (4 left-most bits), responder_tx (4 right-most bits)} array
	 *with list of supported pulse shape combos
	 *
	 * Values:
	 *  PULSE_SHAPE_SYMMETRICAL_ROOT_RAISED_COSINE = 0
	 *  PULSE_SHAPE_PRECURSOR_FREE = 1
	 *  PULSE_SHAPE_PRECURSOR_FREE_SPECIAL = 2
	 */
	AOSP_CAPS_CCC_SUPPORTED_PULSE_SHAPE_COMBOS = 0xa6,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_RAN_MULTIPLIER: A 4-byte value in `legacy_android_ccc` and a
	 * 1-byte value in `JumpWG`, indicating the supported RAN multiplier.
	 */
	AOSP_CAPS_CCC_SUPPORTED_RAN_MULTIPLIER = 0xa7,
	/**
	 * @AOSP_CAPS_CCC_SUPPORTED_MAX_RANGING_SESSION_NUMBER: 4 byte value to
	 * 		indicate the maximum number of CCC ranging sessions
	 * 		supported.
	 */
	AOSP_CAPS_CCC_SUPPORTED_MAX_RANGING_SESSION_NUMBER = 0xa8,
	/**
	 * @AOSP_CAPS_SUPPORTED_POWER_STATS_QUERY: byte value for indicating
	 *		supported (1 == supported)
	 */
	AOSP_CAPS_SUPPORTED_POWER_STATS_QUERY = 0xc0,
	/**
	 * @AOSP_CAPS_SUPPORTED_AOA_RESULT_REQ_ANTENNA_INTERLEAVING: 1 byte
	 *value to indicate support for antenna interleaving feature.
	 *
	 * Values:
	 *  1 - Feature supported.
	 *  0 - Feature not supported.
	 */
	AOSP_CAPS_SUPPORTED_AOA_RESULT_REQ_ANTENNA_INTERLEAVING = 0xe3,
	/**
	 * @AOSP_CAPS_SUPPORTED_SESSION_INFO_NTF_CONFIG: 4 bytes bitmask to
	 *		indicate the supported SESSION_INFO_NTF_CONFIG values.
	 */
	AOSP_CAPS_SUPPORTED_SESSION_INFO_NTF_CONFIG = 0xe5,
	/**
	 * @AOSP_CAPS_SUPPORTED_MIN_RANGING_DURATION_MS: 4 byte value to
	 * 		indicate the min ranging interval supported in ms .
	 */
	AOSP_CAPS_SUPPORTED_MIN_RANGING_DURATION_MS = 0xe4,
	/**
	 * @AOSP_CAPS_SUPPORTED_RSSI_REPORTING: 1 byte bitmask to
	 *		indicate the supported RSSI_REPORTING values.
	 *
	 * Values:
	 *  1 - Feature supported.
	 *  0 - Feature not supported.
	 */
	AOSP_CAPS_SUPPORTED_RSSI_REPORTING = 0xe6,
	/**
	 * @AOSP_CAPS_SUPPORTED_DIAGNOSTICS: 1 byte value to indicate support
	 *		for diagnostics feature.
	 * Values:
	 *  1 - Feature supported.
	 *  0 - Feature not supported.
	 */
	AOSP_CAPS_SUPPORTED_DIAGNOSTICS = 0xe7,
	/**
	 * @AOSP_CAPS_SUPPORTED_MIN_SLOT_DURATION_RSTU: 4 byte value to indicate
	 * 		the min slot duration supported in RSTU.
	 */
	AOSP_CAPS_SUPPORTED_MIN_SLOT_DURATION_RSTU = 0xe8,
	/**
	 * @AOSP_CAPS_SUPPORTED_MAX_RANGING_SESSION_NUMBER: 4 byte value to
	 * 		indicate the maximum number of FiRa ranging sessions
	 * 		supported.
	 */
	AOSP_CAPS_SUPPORTED_MAX_RANGING_SESSION_NUMBER = 0xe9,
};

/**
 * enum uci_diagnostic_notif_frame_report_field - Report fields included
 * in vendor specific Range Diagnostics Notification.
 *
 * @UCI_DIAG_REPORT_AOAS: AoAs report field.
 * @UCI_DIAG_REPORT_EXTRA_STATUS: Extra Status report field.
 * @UCI_DIAG_REPORT_CFO_Q26: CFO report field.
 * @UCI_DIAG_REPORT_EMITTER_SHORT_ADDR: Emitter short address report field.
 * @UCI_DIAG_REPORT_SEGMENT_METRICS: Segment metrics report field.
 * @UCI_DIAG_REPORT_CIRS: CIRs report field.
 * @UCI_DIAG_REPORT_CONFIDENCE_METRICS: Confidence metrics report field.
 * @UCI_DIAG_REPORT_SEGMENT_CONFIDENCE_RAW_DATA: Segment confidence raw data report field.
 *
 */
enum uci_diagnostic_notif_frame_report_field {
	/* Flag 0x0 is deprecated. */
	UCI_DIAG_REPORT_AOAS = 0x01,
	/* Flag 0x2 is deprecated. */
	UCI_DIAG_REPORT_EXTRA_STATUS = 0x03,
	UCI_DIAG_REPORT_CFO_Q26 = 0x04,
	UCI_DIAG_REPORT_EMITTER_SHORT_ADDR = 0x05,
	UCI_DIAG_REPORT_SEGMENT_METRICS = 0x06,
	UCI_DIAG_REPORT_CIRS = 0x07,
	UCI_DIAG_REPORT_CONFIDENCE_METRICS = 0x08,
	UCI_DIAG_REPORT_SEGMENT_CONFIDENCE_RAW_DATA = 0x09,
};
