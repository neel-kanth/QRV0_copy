/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_FIRA_H
#define CHERRY_FIRA_H

#include <cherry/cherry.h>
#include <cherry/cherry_common.h>
#include <cherry/cherry_session.h>
#include <stdbool.h>
#include <stdint.h>

#define CHERRY_AOA_AZIMUTH 0
#define CHERRY_AOA_ELEVATION 1
#define CHERRY_AOA_NB_MAX 2
#define CHERRY_FIRA_MAX_RESPONDERS 8
#define CHERRY_FIRA_MAX_ACTIVE_RR_REPORT 16
#define CHERRY_CFO_INVALID 0x8000
#define CHERRY_STATIC_STS_SIZE 6
#define CHERRY_VENDOR_ID_SIZE 2

/**
 * DOC: struct cherry_fira_session
 *
 * FiRa session object.
 *
 * All sessions are associated with a session object which is given to every
 * functions working with a session.
 */
struct cherry_fira_session;

/**
 * struct cherry_fira_capabilities - Device capability parameters.
 *
 * From FiRa UCI Technical specification v3.0.0.
 */
struct cherry_fira_capabilities {
	/**
	 * @max_data_message_size: Maximum byte size of UCI data messages the UWBS can receive.
	 */
	uint16_t max_data_message_size;
	/**
	 * @max_data_packet_payload_size: Maximum UCI data packet payload byte size the UWBS can send or receive.
	 */
	uint16_t max_data_packet_payload_size;
	/**
	 * @phy_version_range: FiRa PHY version range supported.
	 *
	 * - b0-b7 = lower bound major version.
	 * - b8-b15 = lower bound minor version.
	 * - b16-b23 = higher bound major version.
	 * - b24-b31 = higher bound minor version.
	 */
	uint32_t phy_version_range;
	/**
	 * @mac_version_range: FiRa MAC version range supported.
	 *
	 * - b0-b7 = lower bound major version.
	 * - b8-b15 = lower bound minor version.
	 * - b16-b23 = higher bound major version.
	 * - b24-b31 = higher bound minor version.
	 */
	uint32_t mac_version_range;
	/**
	 * @device_type: Supported device types.
	 *
	 * - b0 = controller.
	 * - b1 = controlee.
	 * - b2-b3 = CCC.
	 */
	uint8_t device_type;
	/**
	 * @device_roles: Supported device roles.
	 *
	 * - b0 = responder.
	 * - b1 = initiator.
	 * - b5 = advertiser.
	 * - b6 = observer.
	 * - b7 = OWR DL-TDoA anchor.
	 * - b8 = OWR DL-TDoA tag.
	 */
	uint16_t device_roles;
	/**
	 * @ranging_methods: Supported ranging methods.
	 *
	 * - b1 = SS-TWR with deferred mode.
	 * - b2 = DS-TWR with deferred mode.
	 * - b3 = SS-TWR with non-deferred mode.
	 * - b4 = DS-TWR with non-deferred mode.
	 * - b5 = OWR-DL-TDoA.
	 * - b6 = OWR for AoA measurement.
	 * - b7 = eSS-TWR with non-deferred mode for contention based ranging.
	 * - b8 = aDS-TWR for contention based ranging.
	 * - b9 = Data Transfer Mode.
	 * - b10 = CCC.
	 */
	uint16_t ranging_methods;
	/**
	 * @sts_config: Supported Scrambled Timestamp Sequence (STS) configurations.
	 *
	 * - b0 = static STS.
	 * - b1 = dynamic STS.
	 * - b2 = dynamic STS for Responder Specific Sub-Session Key.
	 * - b3 = provisioned STS.
	 * - b4 = provisioned STS for Responder Specific Sub-Session Key.
	 */
	uint8_t sts_config;
	/**
	 * @multi_node_mode: Supported multinode modes.
	 *
	 * - b0 = one to one.
	 * - b1 = one to many.
	 * - b2 = Data Transfer.
	 */
	uint8_t multi_node_mode;
	/**
	 * @ranging_time_struct: Supported time structure.
	 *
	 * - b1 = block Based Scheduling.
	 */
	uint8_t ranging_time_struct;
	/**
	 * @schedule_mode: Supported schedule modes.
	 *
	 * - b0 = contention based ranging.
	 * - b1 = time scheduled ranging.
	 * - b2 = Hybrid based ranging.
	 */
	uint8_t schedule_mode;
	/**
	 * @hopping_mode: Preference of hopping.
	 *
	 * - b0 = boolean value.
	 */
	uint8_t hopping_mode;
	/**
	 * @block_striding: Preference of Block Striding.
	 *
	 * - b0 = boolean value.
	 */
	uint8_t block_striding;
	/**
	 * @uwb_initiation_time: Support flag for Initiation time.
	 *
	 * - b0 = boolean value.
	 */
	uint8_t uwb_initiation_time;
	/**
	 * @channels: Supported channels.
	 *
	 * - b0 = channel 5.
	 * - b1 = channel 6.
	 * - b2 = channel 8.
	 * - b3 = channel 9.
	 * - b4 = channel 10.
	 * - b5 = channel 12.
	 * - b6 = channel 13.
	 * - b7 = channel 14.
	 */
	uint8_t channels;
	/**
	 * @rframe_config: Supported STS Packet configurations.
	 *
	 * - b0 = SP0 (Sync, SFD, PHR, PHY Payload).
	 * - b1 = SP1 (Sync, SFD, STS, PHR, PHY Payload).
	 * - b2 = SP2 Deprecated
	 * - b3 = SP3 (Sync, SFD, STS).
	 */
	uint8_t rframe_config;
	/**
	 * @cc_constraint_length: Specifies the constraint length of the convolutional code preferred.
	 *
	 * - b0 = k3.
	 * - b1 = k7.
	 */
	uint8_t cc_constraint_length;
	/**
	 * @bprf_parameter_sets: Bitmap of BPRF set.
	 *
	 * Bits 0 to 5 represents boolean values for BPRF sets from 1 to 6.
	 */
	uint8_t bprf_parameter_sets;
	/**
	 * @hprf_parameter_sets: Bitmap of HPRF set.
	 *
	 * Bits 0 to 34 represents boolean values for HPRF sets from 1 to 35.
	 */
	uint64_t hprf_parameter_sets;
	/**
	 * @aoa_support: Supported AoA measurement values.
	 *
	 * - b0 = azimuth A0A in range -90 to +90 degrees.
	 * - b1 = azimuth A0A in range -180 to +180 degrees.
	 * - b2 = elevation.
	 * - b3 = FOM (Figure Of Merit: reliability of an estimated measured value,
	 *     from 0 to 100).
	 */
	uint8_t aoa_support;
	/**
	 * @extended_mac_address: Support flag for extended MAC addresses.
	 *
	 * - b0 = boolean value.
	 */
	uint8_t extended_mac_address;
	/**
	 * @session_key_length: Supported key bit sizes.
	 *
	 * - b0 = 256 bits for dynamic STS.
	 * - b1 = 256 bits for provisioned STS.
	 */
	uint8_t session_key_length;
	/**
	 * @dt_anchor_max_active_rr: Maximum number of active ranging rounds supported by the DT-Anchor.
	 *
	 * Range from 0 to 255. 0 means DT-Anchor role not supported and shall be consistent with
	 * DEVICE_ROLES capacity.
	 */
	uint8_t dt_anchor_max_active_rr;
	/**
	 * @dt_tag_max_active_rr: Maximum number of active ranging rounds supported by the DT-Tag.
	 *
	 * Range from 0 to 255. 0 means DT-Tag role not supported and shall be consistent with DEVICE_ROLES
	 * capacity.
	 */
	uint8_t dt_tag_max_active_rr;
	/**
	 * @dt_tag_block_skipping: Support flag for DL-TDoA block skipping.
	 *
	 * - b0 = boolean value.
	 */
	uint8_t dt_tag_block_skipping;
	/**
	 * @psdu_length_support: Supported PSDU byte sizes.
	 *
	 * - b0 = 2047 bytes.
	 * - b1 = 4095 bytes.
	 */
	uint8_t psdu_length_support;
	/**
	 * @ll_capability_params: Supported Logical Link feature and parameters.
	 *
	 * - b0 = feature support boolean value.
	 * - b1 = Logical Links Aggregated Frame support.
	 * - b8-b11 = Maximum number of Logical Links supported in UWBS.
	 * - b12-b15: Maximum number of Logical Links support per session.
	 */
	uint16_t ll_capability_params;
	/**
	 * @bypass_mode_support: Support flag for bypass mode.
	 *
	 * - b0 = boolean value.
	 */
	uint8_t bypass_mode_support;
};

/**
 * enum cherry_fira_event_type - Type of event for FiRa session.
 */
enum cherry_fira_event_type {
	/**
	 * @CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS: FiRa session state has changed.
	 */
	CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS,
	/**
	 * @CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR: An error occurred with a session.
	 */
	CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR,
	/**
	 * @CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT: Report of FiRa
	 * session ranging result for TWR and DT-Anchor.
	 */
	CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT,
	/**
	 * @CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT: Report of FiRa
	 * DT-Tag ranging result.
	 */
	CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT,
	/**
	 * @CHERRY_FIRA_EVENT_TYPE_SESSION_DIAGNOSTIC_REPORT: Report of FiRa
	 * session diagnostic.
	 */
	CHERRY_FIRA_EVENT_TYPE_SESSION_DIAGNOSTIC_REPORT,
};

/**
 * enum cherry_fira_session_state - State of a FiRa session
 */
enum cherry_fira_session_state {
	/**
	 * @CHERRY_FIRA_SESSION_STATE_INIT: The session is created but not
	 * yet started
	 */
	CHERRY_FIRA_SESSION_STATE_INIT,
	/**
	 * @CHERRY_FIRA_SESSION_STATE_DEINIT: The session is destroyed, it can
	 * no longer be used.
	 */
	CHERRY_FIRA_SESSION_STATE_DEINIT,
	/**
	 * @CHERRY_FIRA_SESSION_STATE_ACTIVE: The session is started.
	 */
	CHERRY_FIRA_SESSION_STATE_ACTIVE,
	/**
	 * @CHERRY_FIRA_SESSION_STATE_IDLE: The session is stopped.
	 *
	 * Either because the max number of ranging as been reached,
	 * or because the session stop was requested.
	 */
	CHERRY_FIRA_SESSION_STATE_IDLE,
};

/**
 * enum cherry_fira_state_change_reason - Reason why the session state has
 * changed
 */
enum cherry_fira_state_change_reason {
	/**
	 * @CHERRY_FIRA_STATE_CHANGE_REASON_MGMT_CMD: Due to a session management
	 * command
	 */
	CHERRY_FIRA_STATE_CHANGE_REASON_MGMT_CMD,
	/**
	 * @CHERRY_FIRA_STATE_CHANGE_REASON_MAX_RETRY: The number of consecutive
	 * failed round has reached the max retry value defined
	 */
	CHERRY_FIRA_STATE_CHANGE_REASON_MAX_RETRY,
	/**
	 * @CHERRY_FIRA_STATE_CHANGE_REASON_MAX_MEASUREMENT: The number of
	 * successful ranging has reached the max value defined
	 */
	CHERRY_FIRA_STATE_CHANGE_REASON_MAX_MEASUREMENT,
	/**
	 * @CHERRY_FIRA_STATE_CHANGE_REASON_UNKNOWN: The reason is unknown
	 */
	CHERRY_FIRA_STATE_CHANGE_REASON_UNKNOWN,
	/**
	 * @CHERRY_FIRA_STATE_CHANGE_REASON_FORCE_STOPPED: The session
	 * has been force stopped
	 */
	CHERRY_FIRA_STATE_CHANGE_REASON_FORCE_STOPPED,
};

/**
 * struct cherry_fira_session_event_session_status - Data format for the
 * CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS event.
 */
struct cherry_fira_session_event_session_status {
	/**
	 * @session: Session context.
	 */
	struct cherry_fira_session *session;
	/**
	 * @session_state: New state of the session.
	 */
	enum cherry_fira_session_state session_state;
	/**
	 * @reason_code: Reason for the session state change
	 */
	enum cherry_fira_state_change_reason reason_code;
};

/**
 * struct cherry_fira_session_event_error - Data format for the
 * CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR event.
 */
struct cherry_fira_session_event_error {
	/**
	 * @session: Session context.
	 */
	struct cherry_fira_session *session;
	/**
	 * @status_err: Error code, see the definition of cherry_err.
	 */
	enum cherry_err status_err;
};

/**
 * DOC: Warning CHERRY_FIRA_FRAME_X has been renamed CHERRY_COMMON_FRAME_X.
 * The following define are for backward compatibility.
 * They will be removed in next major release
 */
#define CHERRY_FIRA_FRAME_STATUS_OK CHERRY_COMMON_FRAME_STATUS_OK
#define CHERRY_FIRA_FRAME_STATUS_UNKNOWN CHERRY_COMMON_FRAME_STATUS_UNKNOWN
#define CHERRY_FIRA_FRAME_STATUS_TX_FAILED CHERRY_COMMON_FRAME_STATUS_TX_FAILED
#define CHERRY_FIRA_FRAME_STATUS_RX_TIMEOUT \
	CHERRY_COMMON_FRAME_STATUS_RX_TIMEOUT
#define CHERRY_FIRA_FRAME_STATUS_RX_PHY_DEC_FAILED \
	CHERRY_COMMON_FRAME_STATUS_RX_PHY_DEC_FAILED
#define CHERRY_FIRA_FRAME_STATUS_RX_PHY_TOA_FAILED \
	CHERRY_COMMON_FRAME_STATUS_RX_PHY_TOA_FAILED
#define CHERRY_FIRA_FRAME_STATUS_RX_PHY_STS_FAILED \
	CHERRY_COMMON_FRAME_STATUS_RX_PHY_STS_FAILED
#define CHERRY_FIRA_FRAME_STATUS_RX_MAC_DEC_FAILED \
	CHERRY_COMMON_FRAME_STATUS_RX_MAC_DEC_FAILED
#define CHERRY_FIRA_FRAME_STATUS_RX_MAC_IE_DEC_FAILED \
	CHERRY_COMMON_FRAME_STATUS_RX_MAC_IE_DEC_FAILED
#define CHERRY_FIRA_FRAME_STATUS_RX_MAC_IE_MISSING \
	CHERRY_COMMON_FRAME_STATUS_RX_MAC_IE_MISSING

/**
 * struct cherry_fira_aoa_measurements - Fira Angle of Arrival measurements.
 *
 * Contains the different results of the AOA measurements.
 */
struct cherry_fira_aoa_measurements {
	/**
	 * @aoa_fom: Estimation of local AoA reliability.
	 */
	uint8_t aoa_fom;
	/**
	 * @aoa: Estimation of reception angle in degrees (encoded as Q9.7).
	 */
	int16_t aoa;
};

/**
 * struct cherry_fira_session_twr_measurements - TWR measurements.
 */
struct cherry_fira_session_twr_measurements {
	/**
	 * @short_addr: Address of the participating device.
	 */
	uint16_t short_addr;
	/**
	 * @frame_status: Zero if OK, or error reason.
	 *
	 * See enum cherry_common_frame_status for all error codes.
	 */
	enum cherry_common_frame_status frame_status;
	/**
	 * @slot_index: In case of error, slot index where the error was
	 * detected.
	 */
	uint8_t slot_index;
	/**
	 * @distance_mm: Distance in mm.
	 */
	int32_t distance_mm;
	/**
	 * @remote_aoa: Reception angle measured on the participating
	 * device in degrees (encoded as Q9.7).
	 */
	struct cherry_fira_aoa_measurements remote_aoa[CHERRY_AOA_NB_MAX];
	/**
	 * @aoa: Reception angle measured on the device in degrees (encoded as Q9.7).
	 */
	struct cherry_fira_aoa_measurements aoa[CHERRY_AOA_NB_MAX];
	/**
	 * @nlos: Bitmap of the different NLOS issue detected.
	 *
	 * See the CHERRY_NLOS_*_MASK.
	 */
	uint8_t nlos;
	/**
	 * @rssi: computed rssi
	 */
	uint8_t rssi;
};

/**
 * struct cherry_fira_session_twr_ranging_report - Data format for
 * CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT event.
 */
struct cherry_fira_session_twr_ranging_report {
	/**
	 * @session: Session context.
	 */
	struct cherry_fira_session *session;
	/**
	 * @sequence_number: Session notification counter.
	 */
	uint32_t sequence_number;
	/**
	 * @n_measurements: Number of measurements stored in the measurements
	 * table.
	 */
	int n_measurements;
	/**
	 * @measurements: Ranging measurements information.
	 */
	struct cherry_fira_session_twr_measurements measurements[];
};

/**
 * enum cherry_fira_anchor_location_type - Type of DL-TDoA Anchor location used.
 *
 * @CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE: Location not configured.
 * @CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84: Location is in WGS-84 format.
 * @CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL: Location is in relative format.
 * @CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84_Z_EXT: Location is in WGS-84 format with Z element.
 * @CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT: Location is in relative format with Z element.
 * @CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_GRAVITY_ALIGNED: Location is in relative format with Z element.
 * @CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT_GRAVITY_ALIGNED: Location is in relative format with Z element.
 */
enum cherry_fira_anchor_location_type {
	CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE = 0x00,
	CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84 = 0x01,
	CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL = 0x03,
	CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84_Z_EXT = 0x05,
	CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT = 0x07,
	CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_GRAVITY_ALIGNED = 0x09,
	CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT_GRAVITY_ALIGNED = 0x0B,
};

/**
 * struct cherry_fira_anchor_wgs84_location - DL-TDoA Anchor geographic location in
 * WGS-84 coordinates.
 */
struct cherry_fira_anchor_wgs84_location {
	/**
	 * @latitude: Latitude value of the geographical position of the DT-Anchor.
	 *
	 * Signed value in Q9.24 format between -90 and 90.
	 */
	uint64_t latitude;
	/**
	 * @longitude: Longitude value of the geographical position of the DT-Anchor.
	 *
	 * Signed value in Q9.24 format between -180 and 180.
	 */
	uint64_t longitude;
	/**
	 * @altitude: Altitude value of the geographical position of the DT-Anchor.
	 *
	 * Signed value in Q9.21, the unit is 1 kilometer.
	 */
	uint32_t altitude;
};

/**
 * struct cherry_fira_anchor_relative_location - DL-TDoA Anchor geographic location in
 * relative coordinates.
 */
struct cherry_fira_anchor_relative_location {
	/**
	 * @x: X value of the DT-Anchor coordinates in millimeter.
	 *
	 * A signed value between -134217728 and 134217727.
	 */
	int32_t x;
	/**
	 * @y: Y value of the DT-Anchor coordinates in millimeter.
	 *
	 * A signed value between -134217728 and 134217727.
	 */
	int32_t y;
	/**
	 * @z: Z value of the DT-Anchor coordinates in millimeter.
	 *
	 * A signed value between -8388608 and 8388607
	 */
	int32_t z;
};

/**
 * enum cherry_fira_anchor_movable - Indicate if the anchor is movable.
 *
 * @CHERRY_FIRA_ANCHOR_MOVABLE_NO: Anchor location is not expecting to change.
 * @CHERRY_FIRA_ANCHOR_MOVABLE_YES: Anchor location is expecting to change.
 * @CHERRY_FIRA_ANCHOR_MOVABLE_UNKOWN: Anchor movability is unknown.
 * @CHERRY_FIRA_ANCHOR_MOVABLE_RESERVED: Reserved value.
 */
enum cherry_fira_anchor_movable {
	CHERRY_FIRA_ANCHOR_MOVABLE_NO = 0,
	CHERRY_FIRA_ANCHOR_MOVABLE_YES = 1,
	CHERRY_FIRA_ANCHOR_MOVABLE_UNKOWN = 2,
	CHERRY_FIRA_ANCHOR_MOVABLE_RESERVED = 3,
};

/**
 * struct cherry_fira_anchor_relative_location_z_ext - DL-TDoA Anchor geographic location in
 * relative coordinates with Z element for the floor indication.
 */
struct cherry_fira_anchor_relative_location_z_ext {
	/**
	 * @x: X value of the DT-Anchor coordinates in millimeter.
	 *
	 * A signed value between -134217728 and 134217727.
	 */
	int32_t x;
	/**
	 * @y: Y value of the DT-Anchor coordinates in millimeter.
	 *
	 * A signed value between -134217728 and 134217727.
	 */
	int32_t y;
	/**
	 * @z: Z value of the DT-Anchor coordinates in millimeter.
	 *
	 * A signed value between -8388608 and 8388607
	 */
	int32_t z;
	/**
	 * @floor: Floor number of the DT-Anchor.
	 *
	 * Signed value Q10.4 format
	 */
	int16_t floor;
	/**
	 * @moveable: Indicate if the anchor is movable.
	 */
	enum cherry_fira_anchor_movable moveable;
	/**
	 * @height: Height of the DT-Anchor above floor.
	 *
	 * Signed value in Q12.12 format.
	 */
	int32_t height;
	/**
	 * @height_uncertainty: Height uncertainty of the DT-Anchor above floor.
	 *
	 * The 0 value indicates an unknown uncertainty value.
	 */
	int8_t height_uncertainty;
};

/**
 * struct cherry_fira_anchor_wgs84_location_z_ext - DL-TDoA Anchor geographic location in
 * WGS-84 coordinates with Z element for the floor indication.
 */
struct cherry_fira_anchor_wgs84_location_z_ext {
	/**
	 * @latitude: Latitude value of the geographical position of the DT-Anchor.
	 *
	 * Signed value in Q9.24 format between -90 and 90.
	 */
	uint64_t latitude;
	/**
	 * @longitude: Longitude value of the geographical position of the DT-Anchor.
	 *
	 * Signed value in Q9.24 format between -180 and 180.
	 */
	uint64_t longitude;
	/**
	 * @altitude: Altitude value of the geographical position of the DT-Anchor.
	 *
	 * Signed value in Q9.21, the unit is 1 kilometer.
	 */
	uint32_t altitude;
	/**
	 * @floor: Floor number of the DT-Anchor.
	 *
	 * Signed value Q10.4 format
	 */
	int16_t floor;
	/**
	 * @moveable: Indicate if the anchor is movable.
	 */
	enum cherry_fira_anchor_movable moveable;
	/**
	 * @height: Height of the DT-Anchor above floor.
	 *
	 * Signed value in Q12.12 format.
	 */
	int32_t height;
	/**
	 * @height_uncertainty: Height uncertainty of the DT-Anchor above floor.
	 *
	 * The 0 value indicates an unknown uncertainty value.
	 */
	int8_t height_uncertainty;
};

/**
 * struct cherry_fira_anchor_location - DL-TDoA Anchor location
 */
struct cherry_fira_anchor_location {
	/**
	 * @type: Type of location provided for the Anchor
	 */
	enum cherry_fira_anchor_location_type type;
	/**
	 * @data: Geographic location of the Anchor in the format of %type.
	 */
	union {
		/**
		 * @data.wgs84: DT-Anchor location in WGS-84 format when %type is set
		 * to %CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84.
		 */
		struct cherry_fira_anchor_wgs84_location wgs84;
		/**
		 * @data.relative: DT-Anchor location in relative format when %type is set
		 * to %CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL.
		 */
		struct cherry_fira_anchor_relative_location relative;
		/**
		 * @data.wgs84_z_ext: DT-Anchor location in WGS-84 format when %type is set
		 * to %CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84_Z_EXT.
		 */
		struct cherry_fira_anchor_wgs84_location_z_ext wgs84_z_ext;
		/**
		 * @data.relative_z_ext: DT-Anchor location in relative format when %type is set
		 * to %CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT.
		 */
		struct cherry_fira_anchor_relative_location_z_ext relative_z_ext;
		/**
		 * @data.relative_gravity_aligned: DT-Anchor location in relative format when
		 * %type is set to %CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_GRAVITY_ALIGNED.
		 */
		struct cherry_fira_anchor_relative_location
			relative_gravity_aligned;
		/**
		 * @data.relative_z_ext_gravity_aligned: DT-Anchor location in relative format
		 * when %type is set to %CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL_Z_EXT_GRAVITY_ALIGNED.
		 */
		struct cherry_fira_anchor_relative_location_z_ext
			relative_z_ext_gravity_aligned;
	} data;
};

/**
 * enum cherry_fira_dltdoa_msg - Type of DL-TDoA message
 *
 * @CHERRY_FIRA_DLTDOA_MSG_POLL_DTM: Poll DTM sent by the round initiator
 * @CHERRY_FIRA_DLTDOA_MSG_RESP_DTM: Response DTM sent by round responders
 * @CHERRY_FIRA_DLTDOA_MSG_FINAL_DTM: Final DTM sent by the round initiator
 */
enum cherry_fira_dltdoa_msg {
	CHERRY_FIRA_DLTDOA_MSG_POLL_DTM = 0x00,
	CHERRY_FIRA_DLTDOA_MSG_RESP_DTM,
	CHERRY_FIRA_DLTDOA_MSG_FINAL_DTM,
};

/**
 * struct cherry_fira_session_dt_tag_measurements - DT-Tag measurements.
 */
struct cherry_fira_session_dt_tag_measurements {
	/**
	 * @short_addr: Address of the participating device.
	 */
	uint16_t short_addr;
	/**
	 * @frame_status: Zero if OK, or error reason.
	 *
	 * See enum cherry_common_frame_status for all error codes.
	 */
	enum cherry_common_frame_status frame_status;
	/**
	 * @msg_type: message type of the packet received that is associated to the current
	 * measurement.
	 */
	enum cherry_fira_dltdoa_msg msg_type;
	/**
	 * @n_rr_indexes: Number of ranging round on which the Anchor is
	 * participating. It also gives the number of round index reported into
	 * %rr_index.
	 */
	uint8_t n_rr_indexes;
	/**
	 * @rr_indexes: List of round index on which the Anchor is participating.
	 */
	uint8_t rr_indexes[CHERRY_FIRA_MAX_ACTIVE_RR_REPORT];
	/**
	 * @location: anchor's location reported by the anchor itself.
	 */
	struct cherry_fira_anchor_location location;
	/**
	 * @block_index: block index of the current ranging block.
	 */
	uint16_t block_index;
	/**
	 * @round_index: round index of the current ranging block.
	 */
	uint8_t round_index;
	/**
	 * @aoa: reception angle measured on the device.
	 */
	struct cherry_fira_aoa_measurements aoa[CHERRY_AOA_NB_MAX];
	/**
	 * @nlos: Bitmap of the different NLOS issue detected.
	 * See the CHERRY_NLOS_*_MASK.
	 */
	uint8_t nlos;
	/**
	 * @rssi: RSSI measured by the DT-Tag. 0x00 when not available.
	 *
	 * Absolute value in Q7.1 fixed point format in dBm.
	 */
	uint8_t rssi;
	/**
	 * @common_time_base: true when %tx_timestamp and %rx_timestamp are
	 * reported in the common time base.
	 */
	bool common_time_base;
	/**
	 * @tx_timestamp: message transmission timestamp.
	 *
	 * The unit is in ranging ticks (~15.65 ps).
	 */
	uint64_t tx_timestamp;
	/**
	 * @rx_timestamp: message reception timestamp.
	 *
	 * The unit is in ranging ticks (~15.65 ps).
	 */
	uint64_t rx_timestamp;
	/**
	 * @cfo: Clock Frequency Offset measure by the device.
	 *
	 * Signed value in q6.10 fixed point format in ppm unit.
	 * Set to %CHERRY_CFO_INVALID if not supported.
	 */
	uint16_t cfo;
	/**
	 * @anchor_cfo: Clock Frequency Offset reported by the DT-Anchor with
	 * respect to the initiator.
	 *
	 * Signed value in q6.10 fixed point format in ppm unit.
	 * Set to %CHERRY_CFO_INVALID if not supported.
	 */
	uint16_t anchor_cfo;
	/**
	 * @initiator_reply_time: Time difference measured by the initiator
	 * between the Rx timestamp of the Response DTM and Tx timestamp of the
	 * Final DTM.
	 *
	 * The unit is in ranging ticks (~15.65 ps).
	 * 0 when not available.
	 */
	uint32_t initiator_reply_time;
	/**
	 * @responder_reply_time: Time difference measured by the responder
	 * between the Rx timestamp of the Poll DTM and Tx timestamp of the
	 * Response DTM.
	 *
	 * The unit is in ranging ticks (~15.65 ps).
	 * 0 when not available.
	 */
	uint32_t responder_reply_time;
	/**
	 * @tof: Time of Flight between the Responder and the Initiator.
	 *
	 * The unit is in ranging ticks (~15.65 ps).
	 * 0 when not available.
	 */
	uint16_t tof;
};

/**
 * struct cherry_fira_session_dt_tag_ranging_report - data format for
 * CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT event.
 */
struct cherry_fira_session_dt_tag_ranging_report {
	/**
	 * @session: session context.
	 */
	struct cherry_fira_session *session;
	/**
	 * @sequence_number: session notification counter.
	 */
	uint32_t sequence_number;
	/**
	 * @n_measurements: number of measurements stored in the measurements
	 * table.
	 */
	int n_measurements;
	/**
	 * @measurements: ranging measurements information.
	 */
	struct cherry_fira_session_dt_tag_measurements measurements[];
};

/**
 * struct cherry_fira_event - Cherry FiRa event message.
 */
struct cherry_fira_event {
	/**
	 * @type: Event that occurred.
	 */
	enum cherry_fira_event_type type;
	/**
	 * @data: Pointer to the event data. Each event as it own data format
	 * and size.
	 */
	union {
		/**
		 * @data.status: data for the
		 * CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS event.
		 */
		struct cherry_fira_session_event_session_status *status;
		/**
		 * @data.error: data for the
		 * CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR event.
		 */
		struct cherry_fira_session_event_error *error;
		/**
		 * @data.twr_ranging: data for the
		 * CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT event.
		 */
		struct cherry_fira_session_twr_ranging_report *twr_ranging;
		/**
		 * @data.dt_tag_ranging: data for the
		 * CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT event.
		 */
		struct cherry_fira_session_dt_tag_ranging_report *dt_tag_ranging;
		/**
		 * @data.diagnostics: data for the
		 * CHERRY_FIRA_EVENT_TYPE_SESSION_DIAGNOSTIC_REPORT event.
		 */
		struct cherry_common_diag_report *diagnostics;
	} data;
};

/**
 * enum cherry_fira_anchor_role - DL-TDoA Anchor role
 */
enum cherry_fira_anchor_role {
	/**
	 * @CHERRY_FIRA_ANCHOR_ROLE_RESPONDER: The device behaves as a responder
	 * within the round.
	 */
	CHERRY_FIRA_ANCHOR_ROLE_RESPONDER = 0,
	/**
	 * @CHERRY_FIRA_ANCHOR_ROLE_INITIATOR: The device behaves as an initiator
	 * within the round.
	 */
	CHERRY_FIRA_ANCHOR_ROLE_INITIATOR = 1
};

/**
 * struct cherry_fira_anchor_round_config - Round configuration for a DL-TDoA
 * anchor session.
 *
 * There should be at least one device configured as the root initiator;
 * initiator role within the round 0. As long as this root initiator is not
 * started, all other devices will not be able to start their sessions, including
 * the initiator of other rounds.
 */
struct cherry_fira_anchor_round_config {
	/**
	 * @round_idx: Index of the round.
	 */
	uint8_t round_idx;
	/**
	 * @role: Role of the device within the round.
	 */
	enum cherry_fira_anchor_role role;
	/**
	 * @n_responders: Number of responder in the round and into responders table.
	 *
	 * Should be 0 in case of %role set to %CHERRY_FIRA_ANCHOR_ROLE_RESPONDER.
	 */
	uint8_t n_responders;
	/**
	 * @responders_addr: list of responder's addresses in the round.
	 *
	 * Implicit scheduling is used; therefore, responder slots are obtain
	 * from the order of this responders address lists.
	 */
	uint16_t responders_addr[CHERRY_FIRA_MAX_RESPONDERS];
};
/**
 * struct cherry_fira_phy_params - Cherry FiRa PHY parameters.
 */
struct cherry_fira_phy_params {
	/**
	 * @rframe_config: STS Packet Configuration.
	 */
	enum cherry_common_rframe_config rframe_config;
	/**
	 * @prf_mode: Mean Pulse Repetition Frequency.
	 */
	enum cherry_common_prf_mode prf_mode;
	/**
	 * @sfd_id: Identifier for Start of Frame Delimiter (SFD) sequence.
	 */
	uint8_t sfd_id;
	/**
	 * @preamble_duration: Preamble Symbol Repetitions (PSR).
	 */
	enum cherry_common_preamble_duration preamble_duration;
	/**
	 * @nb_sts_segments: Number of STS segments in the frame.
	 */
	uint8_t nb_sts_segments;
	/**
	 * @sts_length: Number of symbols in the STS segment.
	 */
	enum cherry_common_sts_length sts_length;
	/**
	 * @psdu_data_rate: Data rate for PHY service Data Unit.
	 */
	enum cherry_common_psdu_data_rate psdu_data_rate;
};

#define CHERRY_FIRA_PHY_PARAMS(A, B, C, D, E, F, G)                       \
	{                                                                 \
		.rframe_config = CHERRY_COMMON_RFRAME_CONFIG_##A,         \
		.prf_mode = CHERRY_COMMON_PRF_MODE_##B, .sfd_id = C,      \
		.preamble_duration = CHERRY_COMMON_PREAMBLE_DURATION_##D, \
		.nb_sts_segments = E,                                     \
		.sts_length = CHERRY_COMMON_STS_LENGTH_##F,               \
		.psdu_data_rate = CHERRY_COMMON_PSDU_DATA_RATE_##G,       \
	}

/**
 * typedef cherry_fira_cb_t - Type for Cherry FiRa notification callback.
 * @event: The event type and data.
 * @user_data: Pointer to client data defined at the session creation.
 *
 * The event is allocated by cherry and has to be freed by the application
 * using cherry_fira_event_free().
 *
 * The callback implementation should return immediately and should not be
 * blocking.
 */
typedef void (*cherry_fira_cb_t)(struct cherry_fira_event *event,
				 void *user_data);

/**
 * cherry_fira_session_create_twr_controller() - Setup a session for two way
 * ranging for a controller.
 * @ctx: Cherry context.
 * @callback: Callback function to be call for session events.
 * @user_data: Optional pointer to data element managed by the Cherry's client.
 *             It's the pointer that will be put back as argument of fira event callback.
 * @session_id: Session identifier, any non-zero value that must match with
 *              peers.
 * @interval_ms: Interval between ranging, in milliseconds.
 * @controlee_addresses: Array of peers addresses (valid for the duration of the
 *                       call).
 * @n_controlee_addresses: Number of peers addresses in array.
 * @short_addr: UWB device address.
 *
 * This function allocates the session context which must be released using
 * cherry_fira_session_destroy(). Then it setups every needed parameters for a
 * two way ranging session as a controller.
 *
 * This function returns immediately.
 *
 * Returns: Newly allocated session or NULL on error.
 */
struct cherry_fira_session *cherry_fira_session_create_twr_controller(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, int interval_ms,
	const uint16_t *controlee_addresses, int n_controlee_addresses,
	uint16_t short_addr);

/**
 * cherry_fira_session_create_twr_controlee() - Setup a session for two way
 * ranging for a controlee.
 * @ctx: Cherry context.
 * @callback: Callback function to be call for session events.
 * @user_data: Optional pointer to data element managed by the Cherry's client.
 *             It's the pointer that will be put back as argument of fira event callback.
 * @session_id: Session identifier, any non-zero value that must match with
 *              peers.
 * @interval_ms: Interval between ranging, in milliseconds, must match
 * controller configuration.
 * @controller_address: Controller address.
 * @short_addr: UWB device address.
 *
 * This function allocates the session context which must be released using
 * cherry_fira_session_destroy(). Then it setups every needed parameters for a
 * two way ranging session as a controlee.
 *
 * This function returns immediately.
 *
 * Returns: Newly allocated session or NULL on error.
 */
struct cherry_fira_session *cherry_fira_session_create_twr_controlee(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, int interval_ms, uint16_t controller_address,
	uint16_t short_addr);

/**
 * cherry_fira_session_create_dt_anchor() - Setup a session for DL-TDoA Anchor.
 * @ctx: Cherry context.
 * @callback: Callback function to be call for session events.
 * @user_data: Optional pointer to data element managed by the Cherry's client.
 *             It's the pointer that will be put back as argument of fira event callback.
 * @session_id: Session identifier, any non-zero value that must match with peers.
 * @interval_ms: Interval between ranging, in milliseconds (aka ranging duration). It must
 *               match other DT-Anchor configuration.
 * @slot_duration: Specifies duration of a ranging slot in the unit of RSTU
 *                 (1200 is equal to 1ms).
 * @short_addr: UWB device address.
 * @n_rounds: Number of rounds in which the device is participating.
 * @rounds_config: Round configuration table, should contain %n_rounds.
 * @location: Device location. Can be %NULL if location should not be set.
 * @time_reference_anchor: 1 if sesssion is the time reference for the whole cluster network.
 *
 * This function allocates the session context which must be released using
 * cherry_fira_session_destroy(). Then it setups every needed parameters for DL-TDoA ranging session
 * as an Anchor.
 *
 * This function returns immediately.
 *
 * Returns: Newly allocated session or NULL on error.
 */
struct cherry_fira_session *cherry_fira_session_create_dt_anchor(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, uint32_t interval_ms, uint16_t slot_duration,
	uint16_t short_addr, int n_rounds,
	const struct cherry_fira_anchor_round_config *rounds_config,
	const struct cherry_fira_anchor_location *location,
	uint8_t time_reference_anchor);

/**
 * cherry_fira_session_create_dt_tag() - Setup a session for DL-TDoA Tag.
 * @ctx: Cherry context.
 * @callback: Callback function to be call for session events.
 * @user_data: Optional pointer to data element managed by the Cherry's client.
 *             It's the pointer that will be put back as argument of fira event callback.
 * @session_id: Session identifier, any non-zero value that must match the
 *              DT-Anchors configuration.
 * @interval_ms: Interval between ranging, in milliseconds (aka ranging
 *               duration). It must match the DT-Anchor configuration.
 * @slot_duration: Specifies duration of a ranging slot in the unit of RSTU
 *                 (1200 is equal to 1ms). It must match the DT-Anchor configuration.
 * @block_skipping: Number of block to be skipped between two active ranging
 *                  blocks.
 * @n_rounds: Number of rounds in which the device is participating.
 * @round_indexes: Table of active round indexes with %n_rounds elements.
 *
 * This function allocates the session context which must be released using
 * cherry_fira_session_destroy(). Then it setups every needed parameters for
 * DL-TDoA ranging session as a Tag.
 *
 * This function returns immediately.
 *
 * Returns: Newly allocated session or NULL on error.
 */
struct cherry_fira_session *cherry_fira_session_create_dt_tag(
	struct cherry *ctx, cherry_fira_cb_t callback, void *user_data,
	uint32_t session_id, uint32_t interval_ms, uint16_t slot_duration,
	uint8_t block_skipping, int n_rounds, const uint8_t *round_indexes);

/**
 * cherry_fira_session_to_base() - Return the base session object
 * associated to a FiRa one.
 * @session: FiRa session object.
 *
 * Returns: Generic session object.
 */
struct cherry_session *
cherry_fira_session_to_base(struct cherry_fira_session *session);

/**
 * cherry_fira_session_get_user_data() - Get the user_data of the session
 * @session: Session object.
 *
 * Returns: The user_data pointer given at the session creation.
 */
static inline void *
cherry_fira_session_get_user_data(struct cherry_fira_session *session)
{
	return cherry_session_get_user_data(
		cherry_fira_session_to_base(session));
}

/**
 * cherry_fira_event_free() - Release the event allocated memory.
 * @event: Pointer to the event to be freed.
 *
 * After each call to cherry_fira_cb_t, this function has to be used to free the
 * corresponding event data. It lets the upper layer decide either to fully
 * handle the event inside the callback or to defer some processing outside
 * without temporary data copy.
 */
void cherry_fira_event_free(struct cherry_fira_event *event);

/**
 * cherry_fira_session_destroy() - Release a session.
 * @session: Session object.
 *
 * This can be used on any session, active or not. The session will be stopped
 * if needed and removed from the UWB subsystem. The session is not destroyed
 * immediately, it is marked as destructed and can no longer be used by the API
 * client. The session context will be freed after being removed from the UWB
 * subsystem.
 *
 * The session object must not be used after this call.
 *
 * This function returns immediately.
 */
static inline void
cherry_fira_session_destroy(struct cherry_fira_session *session)
{
	cherry_session_destroy(cherry_fira_session_to_base(session));
}

/**
 * cherry_fira_session_start() - Start a session.
 * @session: Session object.
 *
 * Request a session to be started. This is done asynchronously and the session
 * state callback will be called once the session changes state, or if there is
 * an error.
 *
 * While a session is active, the ranging notification callback is called
 * regularly when results are available.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_SESSION_CONFIG if session is missing some key configuration
 */
static inline enum cherry_err
cherry_fira_session_start(struct cherry_fira_session *session)
{
	return cherry_session_start(cherry_fira_session_to_base(session));
}

/**
 * cherry_fira_session_stop() - Stop a session.
 * @session: Session object.
 *
 * Request a session to be stopped. This is done asynchronously and the session
 * state callback will be called once the session changes state, or if there is
 * an error.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
static inline enum cherry_err
cherry_fira_session_stop(struct cherry_fira_session *session)
{
	return cherry_session_stop(cherry_fira_session_to_base(session));
}

/* Extra APIs for fine session configuration */

/**
 * cherry_fira_session_set_rr_retry() - Set number of failed ranging round
 * attempts before stopping the session.
 * @session: Session object.
 * @max_rr_retry: Max retry before stopping the session.
 *
 * When the max ranging round retry is set to 0, the session will retry
 * infinitely otherwise it will automatically stopped when the retry value is
 * reached.
 *
 * By default, if this function is not called, the value is set to 0.
 *
 * This function may be called at any time, as long as the session is not
 * active.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err
cherry_fira_session_set_rr_retry(struct cherry_fira_session *session,
				 uint16_t max_rr_retry);

/**
 * cherry_fira_session_set_nb_measurement() - Set the number of ranging measurement
 * before stopping the session
 * @session: Session object.
 * @max_nb_measurements: Max measurements before stopping the session.
 *
 * When the max number of ranging measurement is set to 0, the session will not
 * stop until the cherry_fira_session_stop() is called. Otherwise it will
 * automatically stop when it reached the number of measurements requested.
 *
 * By default, if this function is not called, the value is set to 0.
 *
 * This function may be called at any time, as long as the session is not
 * active.
 *
 * This function returns immediately.
 *
 * returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err
cherry_fira_session_set_nb_measurement(struct cherry_fira_session *session,
				       uint16_t max_nb_measurements);

/**
 * cherry_fira_session_set_channel() - Set the RF channel for a session.
 * @session: Session object.
 * @channel: Channel number.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
static inline enum cherry_err
cherry_fira_session_set_channel(struct cherry_fira_session *session,
				int channel)
{
	return cherry_session_set_channel(cherry_fira_session_to_base(session),
					  channel);
}

/**
 * cherry_fira_session_set_phy() - Set PHY parameters for a session.
 * @session: Session object.
 * @phy_params: PHY parameters configuration to be applied.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 * By default, if this function is not called the PHY parameters are not set by cherry,
 * so the UWBS uses the default one specified by FiRa.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err
cherry_fira_session_set_phy(struct cherry_fira_session *session,
			    const struct cherry_fira_phy_params *phy_params);

/**
 * cherry_fira_session_set_preamble_code_index() - Sets preamble code index for a session.
 * @session: Session object.
 * @preamble_code_index: preamble_code_index.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 * By default, if this function is not called, the value is set to 10.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
static inline enum cherry_err
cherry_fira_session_set_preamble_code_index(struct cherry_fira_session *session,
					    uint8_t preamble_code_index)
{
	return cherry_session_set_preamble_code_index(
		cherry_fira_session_to_base(session), preamble_code_index);
}

/**
 * cherry_fira_session_set_report_rssi() - Sets the report rssi for a session.
 * @session: Session object.
 * @report_rssi: report_rssi.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 * When set to 1 the RSSI is reported on the ranging report event.
 * By default, if this function is not called, the value is set to 0.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err
cherry_fira_session_set_report_rssi(struct cherry_fira_session *session,
				    uint8_t report_rssi);

/**
 * cherry_fira_session_set_priority() - Sets the session priority for a session.
 * @session: Session object.
 * @session_priority: session_priority.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 * By default, if this function is not called, the value is set to 50.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
static inline enum cherry_err
cherry_fira_session_set_priority(struct cherry_fira_session *session,
				 uint8_t session_priority)
{
	return cherry_session_set_priority(cherry_fira_session_to_base(session),
					   session_priority);
}

/**
 * cherry_fira_session_set_result_report_config() - Sets the result report config for a session.
 * @session: Session object.
 * @result_report_config: result_report_config.
 *
 * b0 = TOF report (0: Disable, 1: Enable)
 * b1 = AOA Azimuth report (0: Disable, 1: Enable)
 * b2 = AOA elevation report (0: Disable, 1: Enable)
 * b3 = AOA FOM report (0: Disable, 1: Enable)
 * This configuration parameter is only applicable when the Controlee is intended to transmit
 * a RRRM or MRM Type 3 message.
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 * By default, if this function is not called, the value is set to 0x01.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err cherry_fira_session_set_result_report_config(
	struct cherry_fira_session *session, uint8_t result_report_config);

/**
 * cherry_fira_session_set_hopping_mode() - Sets the hopping mode for a session.
 * @session: Session object.
 * @hopping_mode: hopping_mode.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 * When set to 1, hopping mode is enabled.
 * By default, if this function is not called, the value is set to 0.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err
cherry_fira_session_set_hopping_mode(struct cherry_fira_session *session,
				     uint8_t hopping_mode);

/**
 * cherry_fira_session_set_static_sts() - Set the Static STS IV.
 * @session: Session object.
 * @sts_iv: Arbitrary 48 bit value used for Static STS encryption.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 *
 * Static STS IV is used to generate encryption parameters. All peers must use
 * the same value.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err cherry_fira_session_set_static_sts(
	struct cherry_fira_session *session,
	const uint8_t sts_iv[CHERRY_STATIC_STS_SIZE]);

/**
 * cherry_fira_session_set_vendor_id() - Set the Vendor ID.
 * @session: Session object.
 * @vendor_id:  Unique ID for vendor. This parameter is used to set vUpper64[15:0] for static STS
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err cherry_fira_session_set_vendor_id(
	struct cherry_fira_session *session,
	const uint8_t vendor_id[CHERRY_VENDOR_ID_SIZE]);

/**
 * cherry_fira_session_set_prov_sts() - Set the Provisioned STS security mode.
 * @session: Session object.
 * @sts_key_len: Key length in bytes, only 16 and 32 are supported.
 * @sts_key: STS root key.
 * @key_rotation_rate: defines n, with 2^n being the rotation rate of some
 *                     keys used during Provisioned STS Ranging.
 *                     If set to 0, no rotation will be done.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err
cherry_fira_session_set_prov_sts(struct cherry_fira_session *session,
				 uint8_t sts_key_len, const uint8_t *sts_key,
				 uint8_t key_rotation_rate);

/**
 * cherry_fira_session_set_diagnostics() - Enable or disable session
 * diagnostics.
 * @session: Session object.
 * @config: Define which kind of diagnostics to enable.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter or not supported
 *    diagnostics
 */
enum cherry_err
cherry_fira_session_set_diagnostics(struct cherry_fira_session *session,
				    struct cherry_common_diag_cfg config);

/**
   * cherry_fira_session_set_antenna() - Select the Antenna Set to be used for the session
   * @session: Session object.
   * @antenna_set: Antenna Set to be used for the session.
   *
   * By default, if this function is not called, the value is set to 0.
   *
   * This function may be called at any time, as long as the session is not
   * active.
   *
   * This function returns immediately.
   *
   * Returns:
   *  - &CHERRY_ERR_NONE if accepted
   *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
   */
enum cherry_err
cherry_fira_session_set_antenna(struct cherry_fira_session *session,
				uint8_t antenna_set);

/**
 * cherry_fira_session_update_dt_tag_active_rounds() - Update the DT-Tag active rounds.
 * @session: Session object.
 * @n_rounds: Number of rounds in which the device is participating.
 * @round_indexes: Table of active round indexes with %n_rounds elements.
 *
 * This function update the list of active rounds a DT-Tag must participate.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err cherry_fira_session_update_dt_tag_active_rounds(
	struct cherry_fira_session *session, int n_rounds,
	const uint8_t *round_indexes);

/**
 * cherry_fira_session_set_slots_per_ranging_round() - Set the number of slots per ranging round.
 * @session: Session object.
 * @slots_per_rr: Number of slots per ranging round.
 *
 * This function updates the number of slot per ranging round.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err cherry_fira_session_set_slots_per_ranging_round(
	struct cherry_fira_session *session, uint8_t slots_per_rr);

/**
 * cherry_fira_session_set_ref_time_base() - Configure the session time
 * reference.
 * @session: Session object.
 * @session_reference: Session object to be used as time reference.
 * @offset_us: Offset, in microseconds, between the @session_reference and
 *             @session round start.
 *
 * This function may be called at any time, as long as the session is not
 * active.
 *
 * It allows to synchronize the session start with another session.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
static inline enum cherry_err
cherry_fira_session_set_ref_time_base(struct cherry_fira_session *session,
				      struct cherry_session *session_reference,
				      uint32_t offset_us)
{
	return cherry_session_set_ref_time_base(
		cherry_fira_session_to_base(session), session_reference,
		offset_us);
}

/**
 * cherry_fira_session_set_dltdoa_tx_active_rr() - Enable or not the transmission of active ranging round.
 * @session: Session object.
 * @tx_active_rr: Enable transmission of the active ranging rounds list.
 *
 * This function may be called at any time, as long as the session is not
 * active. It does not have to be called if no change is needed from default
 * parameters.
 * This function can only be used on DL-TDoA Anchor sessions. When set to 1,
 * the DL-TDoA anchor will transmit the list of ranging rounds it is participating.
 * By default, if this function is not called, the value is set to 0.
 *
 * This function returns immediately.
 *
 * Returns:
 *  - &CHERRY_ERR_NONE if accepted
 *  - &CHERRY_ERR_INVALID_PARAMETER on any invalid parameter
 */
enum cherry_err
cherry_fira_session_set_dltdoa_tx_active_rr(struct cherry_fira_session *session,
					    uint8_t tx_active_rr);

#endif /* CHERRY_FIRA_H */
