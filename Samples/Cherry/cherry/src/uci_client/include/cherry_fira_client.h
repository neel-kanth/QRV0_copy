/*
 * Header file for uci fira client
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_FIRA_CLIENT_H
#define CHERRY_FIRA_CLIENT_H

#include "cherry_session_client.h"

#include <cherry/cherry_fira.h>
#include <qerr.h>
#include <stdbool.h>
#include <stdint.h>
#include <uci/uci.h>

/*
 * In BPRF, frame is at most 127
 * 127 - (MHR + HIE + HT + PIE_Header + V_OUI + MIC + CRC)
 */
#define FIRA_DATA_PAYLOAD_SIZE_MAX 84

/*
 * This limitation comes from the maximum size of PSDUs
 * in BPRF (calculated for Poll DTM).
 */
#define FIRA_DT_TAG_MAX_ACTIVE_RR 33
#define FIRA_DT_ANCHOR_MAX_ACTIVE_RR 33

#define FIRA_DL_TDOA_MAX_ROUNDS_PER_BLOCK 6

/**
 * enum fira_dl_tdoa_message_type - Type of the current DL-TDoA Message.
 * @FIRA_DL_TDOA_MESSAGE_TYPE_DT_POLL: DL-TDOA Poll Message.
 * @FIRA_DL_TDOA_MESSAGE_TYPE_DT_RESPONSE: DL-TDOA Response Message.
 * @FIRA_DL_TDOA_MESSAGE_TYPE_DT_FINAL: DL-TDOA Final Message.
 * @FIRA_DL_TDOA_MESSAGE_TYPE_AOA_MEASUREMENT: AoA Measurement Message.
 */
enum fira_dl_tdoa_message_type {
	FIRA_DL_TDOA_MESSAGE_TYPE_DT_POLL = 0x00,
	FIRA_DL_TDOA_MESSAGE_TYPE_DT_RESPONSE = 0x01,
	FIRA_DL_TDOA_MESSAGE_TYPE_DT_FINAL = 0x02,
	FIRA_DL_TDOA_MESSAGE_TYPE_AOA_MEASUREMENT = 0x3,
};

/**
 * enum fira_owr_dtm_timestamp_type -- DTM TX Timestamp type.
 * @FIRA_OWR_DTM_TIMESTAMP_LOCAL_TIME_BASE: timestamp in local time base.
 * @FIRA_OWR_DTM_TIMESTAMP_COMMON_TIME_BASE: timestamp in common time base
 * of the Initiator DT-Anchor.
 */
enum fira_owr_dtm_timestamp_type {
	FIRA_OWR_DTM_TIMESTAMP_LOCAL_TIME_BASE = 0,
	FIRA_OWR_DTM_TIMESTAMP_COMMON_TIME_BASE = 1,
};

/**
 * enum fira_owr_dtm_timestamp_len -- DTM TX Timestamp length.
 * @FIRA_OWR_DTM_TIMESTAMP_40BITS: 40 bits timestamp.
 * @FIRA_OWR_DTM_TIMESTAMP_64BITS: 64 bits timestamp.
 */
enum fira_owr_dtm_timestamp_len {
	FIRA_OWR_DTM_TIMESTAMP_40BITS = 0,
	FIRA_OWR_DTM_TIMESTAMP_64BITS = 1,
};

/**
 * enum aoa_measurements_index - AOA measurements.
 * @CHERRY_FIRA_CLIENT_AOA_AZIMUTH: Retrieve AOA azimuth.
 * @CHERRY_FIRA_CLIENT_AOA: Retrieve AOA (same as azimuth).
 * @CHERRY_FIRA_CLIENT_AOA_ELEVATION: Retrieve AOA elevation.
 * @CHERRY_FIRA_CLIENT_AOA_NB: Enum members number.
 */
enum aoa_measurements_index {
	CHERRY_FIRA_CLIENT_AOA_AZIMUTH,
	CHERRY_FIRA_CLIENT_AOA = CHERRY_FIRA_CLIENT_AOA_AZIMUTH,
	CHERRY_FIRA_CLIENT_AOA_ELEVATION,
	CHERRY_FIRA_CLIENT_AOA_NB
};

/**
 * struct aoa_measurements - Fira Angle of Arrival measurements.
 *
 * Contains the different results of the AOA measurements.
 */
struct aoa_measurements {
	/**
	 * @rx_antenna_pair: Antenna pair index.
	 */
	uint8_t rx_antenna_pair;
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
 * struct twr_ranging_measurements - Fira ranging measurements.
 */
struct twr_ranging_measurements {
	/**
	 * @short_addr: Address of the participating device.
	 */
	uint16_t short_addr;
	/**
	 * @status: Zero if ok, or error reason.
	 * See enum fira_status for all error codes.
	 */
	enum fira_status status;
	/**
	 * @nlos: Non Line Of Sight indicator.
	 */
	uint8_t nlos;
	/**
	 * @slot_index: in case of error, slot index where the error was detected.
	 */
	uint8_t slot_index;
	/**
	 * @stopped: Ranging was stopped as requested [controller only].
	 */
	bool stopped;
	/**
	 * @distance_mm: Distance in mm.
	 */
	int32_t distance_mm;
	/**
	 * @remote_aoa_azimuth: Estimation of reception angle in the azimuth
	 * of the participating device.
	 */
	int16_t remote_aoa_azimuth;
	/**
	 * @remote_aoa_elevation: Estimation of reception angle in the
	 * elevation of the participating device.
	 */
	int16_t remote_aoa_elevation;
	/**
	 * @remote_aoa_azimuth_fom: Estimation of azimuth reliability of the
	 * participating device.
	 */
	uint8_t remote_aoa_azimuth_fom;
	/**
	 * @remote_aoa_elevation_fom: Estimation of elevation of the
	 * participating device.
	 */
	uint8_t remote_aoa_elevation_fom;
	/**
	 * @local_aoa_measurements: Table of estimations of local measurements.
	 */
	struct aoa_measurements
		local_aoa_measurements[CHERRY_FIRA_CLIENT_AOA_NB];
	/**
	 * @sp1_data: SP1 received data payload
	 */
	uint8_t sp1_data[FIRA_DATA_PAYLOAD_SIZE_MAX];
	/**
	 * @sp1_data_len: Length of received data.
	 */
	int sp1_data_len;
	/**
	 * @rssi: computed rssi
	 */
	uint8_t rssi;
};

/**
 * struct dl_tdoa_measurements - DL-TDOA ranging measurements.
 */
struct dl_tdoa_measurements {
	/**
	 * @short_addr: Address of the participating device.
	 */
	uint16_t short_addr;
	/**
	 * @status: Zero if ok, or error reason.
	 * See enum fira_status for all error codes.
	 */
	enum fira_status status;
	/**
	 * @message_type: Type of the message which has been received.
	 */
	enum fira_dl_tdoa_message_type message_type;
	/**
	 * @tx_timestamp_type: Type of the TX timestamp (local time base vs common time base)
	 * included in the received message.
	 */
	enum fira_owr_dtm_timestamp_type tx_timestamp_type;
	/**
	 * @tx_timestamp_len: Length of the TX timestamp (40-bit vs 64-bit)
	 * included in the received message.
	 */
	enum fira_owr_dtm_timestamp_len tx_timestamp_len;
	/**
	 * @rx_timestamp_len: Length of the TX timestamp (40-bit vs 64-bit)
	 * calculated during the reception of the received message.
	 */
	enum fira_owr_dtm_timestamp_len rx_timestamp_len;
	/**
	 * @anchor_location_type: Type of the coordinate system of DT-Anchor location
	 * (0: WGS84, 1: relative) (if included).
	 */
	enum session_dt_location_coord_system_type anchor_location_type;
	/**
	 * @anchor_location_present: True when the information about DT-Anchor location
	 * is included in the measurement, false otherwise.
	 */
	bool anchor_location_present;
	/**
	 * @active_ranging_round_indexes_len: Number of active ranging round indexes
	 * included in the measurement.
	 */
	uint8_t active_ranging_round_indexes_len;
	/**
	 * @round_index: Index of the current ranging round.
	 */
	uint8_t round_index;
	/**
	 * @block_index: Index of the current ranging block.
	 */
	uint16_t block_index;
	/**
	 * @local_aoa_azimuth: AoA Azimuth in degrees measured by the DT-Tag
	 * during the reception (encoded as Q9.7).
	 */
	int16_t local_aoa_azimuth;
	/**
	 * @local_aoa_elevation: AoA Elevation in degrees measured by the DT-Tag
	 * during the reception (encoded as Q9.7).
	 */
	int16_t local_aoa_elevation;
	/**
	 * @local_aoa_azimuth_fom: Reliability of the estimated AoA Azimuth measured by the DT-Tag
	 * during the reception (range: 0-100).
	 */
	uint8_t local_aoa_azimuth_fom;
	/**
	 * @local_aoa_elevation_fom: Reliability of the estimated AoA Elevation measured by the DT-Tag
	 * during the reception (range: 0-100).
	 */
	uint8_t local_aoa_elevation_fom;
	/**
	 * @rx_rssi: RSSI measured by the DT-Tag during the reception (encoded as Q7.1).
	 */
	uint8_t rx_rssi;
	/**
	 * @nlos: Indicates if the reception of the message was in Line of Sight or not.
	 * Value 0xFF means that it was unable to be determined (or not supported at all).
	 */
	uint8_t nlos;
	/**
	 * @local_cfo: Clock frequency offset measured locally with respect to the DT-Anchor
	 * that sent the message received (encoded as Q6.10).
	 */
	uint16_t local_cfo;
	/**
	 * @remote_cfo: Clock frequency offset of a Responder DT-Anchor with respect to the Initiator DT-Anchor
	 * of the ranging round as included in the received message (encoded as Q6.10).
	 */
	uint16_t remote_cfo;
	/**
	 * @tx_timestamp_rctu: TX timestamp included in the received message (unit: RCTU).
	 */
	uint64_t tx_timestamp_rctu;
	/**
	 * @rx_timestamp_rctu: RX timestamp calculated during the reception of the received message (unit: RCTU).
	 */
	uint64_t rx_timestamp_rctu;
	/**
	 * @initiator_reply_time_rctu: Reply time of the Initiator DT-Anchor measured between the reception of Response DTM
	 * and the transmission of Final DTM (used only in DS-TWR, unit: RCTU).
	 */
	uint32_t initiator_reply_time_rctu;
	/**
	 * @responder_reply_time_rctu: Reply time of the Responder DT-Anchor measured between the reception of Poll DTM
	 * and the transmission of Response DTM (unit: RCTU).
	 */
	uint32_t responder_reply_time_rctu;
	/**
	 * @anchor_location: Location coordinates of DT-Anchor that sent the message received.
	 */
	uint8_t anchor_location[FIRA_DL_TDOA_ANCHOR_LOCATION_SIZE_MAX];
	/**
	 * @active_ranging_round_indexes: List of active ranging round indexes in which the DT-Anchor
	 * that sent the message received participates.
	 */
	uint8_t active_ranging_round_indexes[FIRA_DL_TDOA_MAX_ROUNDS_PER_BLOCK];
	/**
	 * @initiator_responder_tof_rctu: Time of Flight measured between the Initiator DT-Anchor and the Responder DT-Anchor
	 * (for SS-TWR it's calculated by Initiator DT-Anchor and included in Poll DTM and
	 * for DS-TWR it's calculated by Responder DT-Anchor and included in Response DTM,
	 * unit: RCTU)
	 */
	uint16_t initiator_responder_tof_rctu;
};

/**
 * struct twr_ranging_results - Fira TWR ranging results.
 */
struct twr_ranging_results {
	/**
	 * @session_handle: Session id of the ranging result.
	 */
	uint32_t session_handle;
	/**
	 * @sequence_number: Session notification counter.
	 */
	uint32_t sequence_number;
	/**
	 * @ranging_interval_ms: Current ranging interval in unit of ms.
	 * formula: (block size * (stride + 1))
	 */
	uint32_t ranging_interval_ms;
	/**
	 * @n_measurements:
	 * Number of measurements stored in the measurements
	 * table.
	 */
	int n_measurements;
	/**
	 * @measurements: Ranging measurements information.
	 */
	struct twr_ranging_measurements measurements[FIRA_CONTROLEES_MAX];
};

/**
 * struct dl_tdoa_ranging_results - Fira TWR ranging results.
 */
struct dl_tdoa_ranging_results {
	/**
	 * @session_handle: Session id of the ranging result.
	 */
	uint32_t session_handle;
	/**
	 * @sequence_number: Session notification counter.
	 */
	uint32_t sequence_number;
	/**
	 * @block_index: Current block index.
	 */
	uint32_t block_index;
	/**
	 * @n_measurements:
	 * Number of measurements stored in the measurements
	 * table.
	 */
	int n_measurements;
	/**
	 * @measurements: Linked list of the DL-TDOA measurements or NULL.
	 */
	struct dl_tdoa_measurements *measurements;
};

struct cir {
	uint8_t receiver_segment;
	int16_t fpath_tap_offset;
	uint16_t n_taps;
	uint8_t tap_size;
	uint8_t *taps;
};

struct aoa_measurement {
	int16_t tdoa;
	int16_t pdoa;
	int16_t aoa;
	uint8_t fom;
	uint8_t type;
};

struct segment_metrics {
	int16_t noise_value;
	uint16_t rsl_q8;
	uint16_t fp_index;
	uint16_t fp_rsl_q8;
	uint16_t fp_ns_q6;
	uint16_t pp_index;
	uint16_t pp_rsl_q8;
	uint16_t pp_ns_q6;
	uint8_t receiver_segment;
};

struct frame_report {
	struct segment_metrics *seg_metrics;
	struct aoa_measurement *aoas;
	struct cir *cirs;
	int32_t cfo_q26;
	uint16_t emitter_short_addr;
	uint16_t extra_status;
	uint8_t nb_seg_metrics;
	uint8_t nb_aoa;
	uint8_t nb_cir;
	bool cfo_present;
	bool emitter_short_addr_present;
	bool extra_status_present;
	uint8_t msg_id;
	uint8_t action;
	uint8_t antenna_set;
};

struct diagnostic_info {
	uint32_t session_handle;
	uint32_t sequence_number;
	uint32_t nb_reports;
	struct frame_report *reports;
};

/**
 * typedef cherry_uci_client_fira_diag_notification_cb_t - Diagnostics notification callback type.
 *
 * @results: Diagnostics results.
 * @user_data: User data pointer given to cherry_uci_client_fira_open.
 *
 * Return: Nothing.
 */
typedef void (*cherry_uci_client_fira_diag_notification_cb_t)(
	struct diagnostic_info *results, void *user_data);

/*
 * typedef struct cherry_fira_context - Fira context
 */
struct cherry_fira_context;

/**
 * cherry_uci_client_fira_open() - Initialize the internal resources of the client.
 *
 * @context: Fira context to initialize.
 * @uci: UCI Core context.
 * @user_data: User data pointer to give back in callback.
 * @diag_cb: Callback to use to notify twr range measurements.
 *
 * NOTE: This function must be called first. @cherry_uci_client_fira_close must be called
 * at the end of the application to ensure resources are freed.
 * The channel will be managed by the client, this means you should neither use
 * uwbmac_channel_create nor uwbmac_channel_release.
 *
 * Return: QERR_SUCCESS or error.
 */
enum qerr cherry_uci_client_fira_open(
	struct cherry_fira_context **context, struct uci *uci, void *user_data,
	cherry_uci_client_fira_diag_notification_cb_t diag_cb);

/**
 * cherry_uci_client_fira_close() - Free all internal resources of the client.
 *
 * @context: Fira context to free.
 */
void cherry_uci_client_fira_close(struct cherry_fira_context *context);

/**
 * cherry_uci_client_fira_update_dt_anchor_ranging_rounds() - Sets the dl tdoa anchor ranging rounds used in this session.
 * @context: Fira context.
 * @session_id: Session identifier.
 * @number_of_active_ranging_rounds: Number of ranging rounds in which a UWBS is active as DT-Anchor.
 * @round_indexes: Ranging Round Index to activate
 * @ranging_role: Represents the ranging role within the ranging round.
 * @number_of_responders: Number of Responder MAC Addresses
 * @responder_address_list: Responder MAC Address List for the specified ranging round as Initiator DT-Anchor.
 * @responder_slot_scheduling: RResponder slot presence.
 * @responder_slots: Slot indexes assigned for Responder transmissions.
 * @dl_tdoa_update_ranging_round_array: Variable to store the response value.
 * @dl_tdoa_update_ranging_round_array_size: Variable to store the response size.
 *
 * Return: UCI_STATUS_OK or error.
 */

enum uci_status_code cherry_uci_client_fira_update_dt_anchor_ranging_rounds(
	struct cherry_fira_context *context, uint32_t session_id,
	uint8_t number_of_active_ranging_rounds, uint8_t *round_indexes,
	uint8_t *ranging_role, uint8_t *number_of_responders,
	uint16_t responder_address_list[][8],
	uint8_t *responder_slot_scheduling, uint8_t responder_slots[][8],
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
	uint8_t *dl_tdoa_update_ranging_round_array_size);

/**
 * cherry_uci_client_fira_update_dt_tag_ranging_rounds() - Sets the dl tdoa tag ranging rounds used in this session.
 * @context: Fira context.
 * @session_id: Session identifier.
 * @number_of_active_ranging_rounds: Number of ranging rounds in which a UWBS is active as DT-Anchor.
 * @ranging_round_indexes: Ranging Round Index where the tag listen.
 * @dl_tdoa_update_ranging_round_array: Variable to store the response value.
 * @dl_tdoa_update_ranging_round_array_size: Variable to store the response size.
 *
 * Return: UCI_STATUS_OK or error.
 */

enum uci_status_code cherry_uci_client_fira_update_dt_tag_ranging_rounds(
	struct cherry_fira_context *context, uint32_t session_id,
	uint8_t number_of_active_ranging_rounds, uint8_t *ranging_round_indexes,
	uint8_t dl_tdoa_update_ranging_round_array
		[DL_TDOA_UPDATE_RANGING_ROUND_RSP_MAX_SIZE],
	uint8_t *dl_tdoa_update_ranging_round_array_size);

/**
 * cherry_uci_client_fira_free_twr_results() - Free memory allocated in twr ranging notification.
 * @results: Pointer to twr ranging notifification data to free.
 */
void cherry_uci_client_fira_free_twr_results(
	struct twr_ranging_results *results);

/**
 * cherry_uci_client_fira_free_diag() - Free memory allocated in diagnostic notification.
 * @diag: Pointer to diagnostic notification data to free.
 */
void cherry_uci_client_fira_free_diag(struct diagnostic_info *diag);

/**
 * cherry_uci_client_parse_twr_measurements() - Parse TWR measurements.
 * @data: Notification genereic data.
 * @twr_results: Pointer to structure to fill in.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum qerr cherry_uci_client_parse_twr_measurements(
	const struct session_ranging_data *data,
	struct twr_ranging_results *twr_results);

/**
 * cherry_uci_client_parse_dltdoa_measurements() - Parse dl-tdoa measurements.
 * @data: Notification genereic data.
 * @dl_tdoa_results: Pointer to structure to fill in.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum qerr cherry_uci_client_parse_dltdoa_measurements(
	const struct session_ranging_data *data,
	struct cherry_fira_session_dt_tag_ranging_report *dl_tdoa_results);

/**
 * cherry_uci_client_parse_dltdoa_measurements_v2() - Parse dl-tdoa measurements.
 * @data: Notification genereic data.
 * @dl_tdoa_results: Pointer to structure to fill in.
 *
 * Return: UCI_STATUS_OK or error.
 */
enum qerr cherry_uci_client_parse_dltdoa_measurements_v2(
	const struct session_ranging_data *data,
	struct cherry_fira_session_dt_tag_ranging_report *dl_tdoa_results);

#endif /* CHERRY_FIRA_CLIENT_H */
