/*
 * Utilities for dumping different structures to a terminal.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "util_dump.h"

#include "qmalloc.h"
#include "qtime.h"
#include "util_convert.h"
#include "util_log.h"

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char *frame_status_to_str(enum cherry_common_frame_status status)
{
	switch (status) {
	case CHERRY_COMMON_FRAME_STATUS_OK:
		return "OK";
	case CHERRY_COMMON_FRAME_STATUS_TX_FAILED:
		return "TX FAILED";
	case CHERRY_COMMON_FRAME_STATUS_RX_TIMEOUT:
		return "RX TIMEOUT";
	case CHERRY_COMMON_FRAME_STATUS_RX_PHY_DEC_FAILED:
		return "RX PHY DEC FAILED";
	case CHERRY_COMMON_FRAME_STATUS_RX_PHY_TOA_FAILED:
		return "RX PHY TOA FAILED";
	case CHERRY_COMMON_FRAME_STATUS_RX_PHY_STS_FAILED:
		return "RX PHY STS FAILED";
	case CHERRY_COMMON_FRAME_STATUS_RX_MAC_DEC_FAILED:
		return "RX MAC DEC FAILED";
	case CHERRY_COMMON_FRAME_STATUS_RX_MAC_IE_DEC_FAILED:
		return "RX MAC IE DEC FAILED";
	case CHERRY_COMMON_FRAME_STATUS_RX_MAC_IE_MISSING:
		return "RX MAC IE MISSING";
	default:
		break;
	}
	return "UNKNOWN";
}

static void util_dump_nlos(uint8_t nlos)
{
	QLOGI("\tNLOS: 0x%02X", nlos & CHERRY_NLOS_MASK);
}

static void util_dump_rssi(uint8_t rssi)
{
	QLOGI("\tRSSI: %.1f", -1 * util_convert_rssi_q_to_float(rssi));
}

/* Angles are encoded as Q9.7, we convert them in float. */
void util_dump_twr_range_report(
	const struct cherry_fira_session_twr_measurements *cur_meas)
{
	QLOGI("\tRanging OK");
	util_dump_nlos(cur_meas->nlos);
	QLOGI("\tDistance in mm: %d", cur_meas->distance_mm);
	QLOGI("\tLocal AOA measurements");
	QLOGI("\tEstimation of local reception angle in the azimuth: %.7f deg",
	      util_convert_aoa_q_to_float(
		      cur_meas->aoa[CHERRY_AOA_AZIMUTH].aoa));
	QLOGI("\tEstimation of local azimuth reliability: %u",
	      cur_meas->aoa[CHERRY_AOA_AZIMUTH].aoa_fom);
	QLOGI("\tEstimation of local reception angle in the elevation: %.7f deg",
	      util_convert_aoa_q_to_float(
		      cur_meas->aoa[CHERRY_AOA_ELEVATION].aoa));
	QLOGI("\tEstimation of local elevation reliability: %u",
	      cur_meas->aoa[CHERRY_AOA_ELEVATION].aoa_fom);
	QLOGI("\tRemote AOA measurements");
	QLOGI("\tEstimation of reception angle in the azimuth: %.7f deg",
	      util_convert_aoa_q_to_float(
		      cur_meas->remote_aoa[CHERRY_AOA_AZIMUTH].aoa));
	QLOGI("\tEstimation of azimuth reliability: %u",
	      cur_meas->remote_aoa[CHERRY_AOA_AZIMUTH].aoa_fom);
	QLOGI("\tEstimation of reception angle in the elevation: %.7f deg",
	      util_convert_aoa_q_to_float(
		      cur_meas->remote_aoa[CHERRY_AOA_ELEVATION].aoa));
	QLOGI("\tEstimation of elevation reliability: %u",
	      cur_meas->remote_aoa[CHERRY_AOA_ELEVATION].aoa_fom);
	util_dump_rssi(cur_meas->rssi);
}

void util_dump_dt_tag_report(
	const struct cherry_fira_session_dt_tag_measurements *cur_meas)
{
	QLOGI("\tRanging OK");
	QLOGI("\tMessage type: %d", cur_meas->msg_type);
	util_dump_nlos(cur_meas->nlos);
	for (int i = 0; i < cur_meas->n_rr_indexes; i++) {
		QLOGI("\t\tRanging round %d with index %u", i,
		      cur_meas->rr_indexes[i]);
	}
	switch (cur_meas->location.type) {
	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_NONE:
		QLOGI("\tNo location present");
		break;
	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_REL:
		QLOGI("\tRelative location present");
		QLOGI("\t\tx = %d",
		      util_convert_x_to_int(
			      cur_meas->location.data.relative.x));
		QLOGI("\t\ty = %d",
		      util_convert_y_to_int(
			      cur_meas->location.data.relative.y));
		QLOGI("\t\tz = %d",
		      util_convert_z_to_int(
			      cur_meas->location.data.relative.z));
		break;
	case CHERRY_FIRA_ANCHOR_LOCATION_TYPE_WGS84:
		QLOGI("\t\tlatitude = %.24lf",
		      util_convert_latitude_to_float(
			      cur_meas->location.data.wgs84.latitude));
		QLOGI("\t\tlongitude = %.24lf",
		      util_convert_longitude_to_float(
			      cur_meas->location.data.wgs84.longitude));
		QLOGI("\t\taltitude = %.21lf",
		      util_convert_altitude_to_float(
			      cur_meas->location.data.wgs84.altitude));
		break;
	default:
		break;
	}
	QLOGI("\tBlock index: %u", cur_meas->block_index);
	QLOGI("\tRound index: %u", cur_meas->round_index);
	QLOGI("\tAoas:");
	QLOGI("\t\tAzimuth: %.7f deg with fom %u",
	      util_convert_aoa_q_to_float(
		      cur_meas->aoa[CHERRY_AOA_AZIMUTH].aoa),
	      cur_meas->aoa[CHERRY_AOA_AZIMUTH].aoa_fom);
	QLOGI("\t\tElevation: %.7f deg with fom %u",
	      util_convert_aoa_q_to_float(
		      cur_meas->aoa[CHERRY_AOA_ELEVATION].aoa),
	      cur_meas->aoa[CHERRY_AOA_ELEVATION].aoa_fom);
	util_dump_rssi(cur_meas->rssi);
	if (cur_meas->common_time_base) {
		QLOGI("Timestamps are in common time base");
	} else {
		QLOGI("Timestamps are in local time base");
	}
	QLOGI("\tTx timestamp: %" PRIu64 "", cur_meas->tx_timestamp);
	QLOGI("\tRx timestamp: %" PRIu64 "", cur_meas->rx_timestamp);
	QLOGI("\tCfo: %.6f ppm", util_convert_cfo_to_float(cur_meas->cfo));
	QLOGI("\tAnchor cfo: %6f ppm",
	      util_convert_cfo_to_float(cur_meas->anchor_cfo));
	QLOGI("\tInitiator reply time: %" PRIu32 "",
	      cur_meas->initiator_reply_time);
	QLOGI("\tResponder reply time: %" PRIu32 "",
	      cur_meas->responder_reply_time);
	QLOGI("\tTof: %u", cur_meas->tof);
}

static void util_dump_diagnostic_frame_segment_metrics(
	const struct cherry_common_segment_metrics *seg_metrics)
{
	QLOGD("\t\t\tReceiver segment: 0x%02X", seg_metrics->receiver_segment);
	QLOGD("\t\t\tNoise: %ddBm", seg_metrics->noise_value);
	QLOGD("\t\t\tRSL: 0x%04X", seg_metrics->rsl_q8);
	QLOGD("\t\t\tFirst Path index: %u", seg_metrics->fp_index);
	QLOGD("\t\t\tFirst Path offset: 0x%04X", seg_metrics->fp_ns_q6);
	QLOGD("\t\t\tFirst Path RSL: 0x%04X", seg_metrics->fp_rsl_q8);
	QLOGD("\t\t\tPeak Path index: %u", seg_metrics->pp_index);
	QLOGD("\t\t\tFirst Path offset: 0x%04X", seg_metrics->pp_ns_q6);
	QLOGD("\t\t\tPeak Path RSL: 0x%04X", seg_metrics->pp_rsl_q8);
}

static void
util_dump_diagnostic_frame_aoa(const struct cherry_common_aoa_measurement *aoa)
{
	QLOGD("\t\t\tTDoA: %d", aoa->tdoa);
	QLOGD("\t\t\tPDoA: 0x%04X", aoa->pdoa);
	QLOGD("\t\t\tAoA: 0x%04X", aoa->aoa);
	QLOGD("\t\t\tFOM: 0x%02X", aoa->fom);
	QLOGD("\t\t\tType: 0x%02X", aoa->type);
}

static void util_dump_diagnostic_frame_cir(const struct cherry_common_cir *cir)
{
	uint8_t sizeof_half_tap;

	QLOGD("\t\t\tReceiver segment: 0x%02X", cir->receiver_segment);
	QLOGD("\t\t\tFirst Path offset: %d", cir->fpath_tap_offset);
	QLOGD("\t\t\tSize of tap: %u", cir->tap_size);
	QLOGD("\t\t\tNumber of taps: %u", cir->n_taps);

	if (!cir->taps) {
		QLOGD("\t\t\tTaps empty");
		return;
	}

	QLOGD("\t\t\tTaps:");
	/*
	 * Tap size describe a sum of sizes of real and imaginary parts in a tap.
	 * Real and Imaginary parts equally take half of this value.
	 * Here we map 16-bit or 24-bit or 32-bit value on 32 bit unsigned value.
	 */
	sizeof_half_tap = cir->tap_size / 2;
	for (uint16_t i = 0; i < cir->n_taps * 2; i += sizeof_half_tap) {
		uint8_t *half_tap = cir->taps + i;
		uint32_t val = 0;
		uint32_t sign_bit = 1 << (8 * sizeof_half_tap - 1);
		uint32_t sign_ext = 0xffffffff << (8 * sizeof_half_tap);

		for (uint16_t k = 0; k < sizeof_half_tap; k++)
			val |= half_tap[k] << (8 * k);
		/*
		 * When 16-bit or 24-bit negative representation is stored on 32-bit,
		 * need to extend the sign for 2's complement storage.
		 */
		if (val & sign_bit)
			val |= sign_ext;

		if (i % 2)
			QLOGD("\t\t\t\tIm: %d", (int32_t)val);
		else
			QLOGD("\t\t\t\tRe: %d", (int32_t)val);
	}
}

void util_dump_diagnostic(const struct cherry_common_diag_report *diagnostic)
{
	if (!diagnostic) {
		QLOGD("No diagnostic report available");
		return;
	}

	QLOGD("Diagnostic report contains %d frame reports",
	      diagnostic->n_frame_report);

	for (uint16_t i = 0; i < diagnostic->n_frame_report; i++) {
		const struct cherry_common_diag_frame *frame =
			&diagnostic->frame_report[i];

		QLOGD("\tFrame %u:", i + 1);
		QLOGD("\t\tMSG identifier: 0x%02X", frame->msg_id);
		QLOGD("\t\tAction: %s", frame->action == 0 ? "Rx" : "Tx");
		QLOGD("\t\tAntenna set: %u", frame->antenna_set);

		if (frame->extra_status_present)
			QLOGD("\t\tExtra status: 0x%04X", frame->extra_status);

		if (frame->cfo_present)
			QLOGD("\t\tCFO: 0x%08X", frame->cfo_q26);

		if (frame->emitter_short_addr_present)
			QLOGD("\t\tEmitter addr: 0x%04X",
			      frame->emitter_short_addr);

		if (frame->n_seg_metrics) {
			if (!frame->seg_metrics) {
				QLOGW("\t\tUnexpected empty segment metrics");
			} else {
				for (uint16_t j = 0; j < frame->n_seg_metrics;
				     j++) {
					struct cherry_common_segment_metrics
						*seg_metrics =
							&frame->seg_metrics[j];
					QLOGD("\t\tSegment %d: ", j + 1);
					util_dump_diagnostic_frame_segment_metrics(
						seg_metrics);
				}
			}
		} else {
			QLOGD("\t\tNo segment metrics available");
		}

		if (frame->n_aoa) {
			if (!frame->aoas) {
				QLOGW("\t\tUnexpected empty AoA metrics");
			} else {
				for (uint16_t j = 0; j < frame->n_aoa; j++) {
					struct cherry_common_aoa_measurement
						*aoa = &frame->aoas[j];
					QLOGD("\t\tAoA %d: ", j + 1);
					util_dump_diagnostic_frame_aoa(aoa);
				}
			}
		} else {
			QLOGD("\t\tNo AoA metrics available");
		}

		if (frame->n_cir) {
			if (!frame->cirs) {
				QLOGW("\t\tUnexpected empty CIR");
			} else {
				for (uint16_t j = 0; j < frame->n_cir; j++) {
					struct cherry_common_cir *cir =
						&frame->cirs[j];
					QLOGD("\t\tCIR %d: ", j + 1);
					util_dump_diagnostic_frame_cir(cir);
				}
			}
		} else {
			QLOGD("\t\tNo CIR available");
		}
	}
}

static const char *device_state_to_str(enum cherry_core_device_state state)
{
	switch (state) {
	case CHERRY_CORE_DEVICE_STATE_READY:
		return "READY";
	case CHERRY_CORE_DEVICE_STATE_ACTIVE:
		return "ACTIVE";
	case CHERRY_CORE_DEVICE_STATE_ERROR:
		return "ERROR";
	}
	return "UNKNOWN";
}

static const char *
device_reason_to_str(enum cherry_core_state_change_reason reason)
{
	switch (reason) {
	case CHERRY_CORE_STATE_CHANGE_ACTIVITY:
		return "ACTIVITY";
	case CHERRY_CORE_STATE_CHANGE_BOOT_REASON_UNKNOWN:
		return "BOOT REASON UNKNOWN";
	case CHERRY_CORE_STATE_CHANGE_BOOT_REASON_FATAL:
		return "BOOT REASON FATAL";
	case CHERRY_CORE_STATE_CHANGE_SOFT_RESET:
		return "SOFT RESET";
	}
	return "UNKNOWN";
}

static void free_calib_log_buf(char *buf)
{
	if (buf)
		qfree(buf);
	return;
}

static bool alloc_calib_log_buf(const struct cherry_calib_key *key, char **str,
				size_t *size)
{
	uint16_t key_size;

	*str = NULL;
	*size = 0;

	switch (key->type) {
	case CHERRY_CALIB_VALUE_NUMBER:
		if (key->size != 1 && key->size != 2 && key->size != 4) {
			QLOGE("Invalid key size %u for %s.", key->size,
			      key->name);
			return false;
		}
		*size = (key->size * 2 + 2);
		break;
	case CHERRY_CALIB_VALUE_NUMBER_ARRAY:
		key_size = key->size / key->nb_array_items;
		if (key_size != 1 && key_size != 2 && key_size != 4) {
			QLOGE("Invalid key size %u for %s.", key->size,
			      key->name);
			return false;
		}
		*size = ((key_size * 2 + 3) * key->nb_array_items);
		break;
	case CHERRY_CALIB_VALUE_DATA:
		*size = (5 * key->size);
		break;
	default:
		QLOGE("Invalid key type %u for %s.", key->type, key->name);
		return false;
	}

	if (!*size)
		return false;

	*size += 64 + 16; /* Size of key name + padding. */

	*str = qmalloc(*size);
	if (*str) {
		*str[0] = '\0';
		snprintf(str[strlen(*str)], 64 + 16, "\tkey %s: ", key->name);
		return true;
	}

	return false;
}

void util_dump_calib(const struct cherry_calib *calib)
{
	uint32_t index, index_array, index_data;
	char *str;
	const struct cherry_calib_key *key;
	size_t size;

	QLOGD("Calibration data:");

	for (index = 0; index < calib->n_keys; index++) {
		key = &calib->keys[index];
		if (!alloc_calib_log_buf(key, &str, &size))
			return;

		switch (key->type) {
		case CHERRY_CALIB_VALUE_NUMBER:
			switch (key->size) {
			case 1:
				snprintf(&str[strlen(str)], size, "%#01x",
					 key->number);
				break;
			case 2:
				snprintf(&str[strlen(str)], size, "%#02x",
					 key->number);
				break;
			case 4:
				snprintf(&str[strlen(str)], size, "%#04x",
					 key->number);
				break;
			default:
				/* Checked in alloc_calib_log_buf(). */
				break;
			}
			break;

		case CHERRY_CALIB_VALUE_NUMBER_ARRAY:
			switch (key->size / key->nb_array_items) {
			case 1: {
				uint8_t *number = (uint8_t *)key->data;
				for (index_array = 0;
				     index_array < key->nb_array_items;
				     index_array++)
					snprintf(&str[strlen(str)], size,
						 "%#01x ", number[index_array]);
			} break;
			case 2: {
				uint16_t *number = (uint16_t *)key->data;
				for (index_array = 0;
				     index_array < key->nb_array_items;
				     index_array++)
					snprintf(&str[strlen(str)], size,
						 "%#02x ", number[index_array]);
			} break;
			case 4: {
				uint32_t *number = (uint32_t *)key->data;
				for (index_array = 0;
				     index_array < key->nb_array_items;
				     index_array++)
					snprintf(&str[strlen(str)], size,
						 "%#04x ", number[index_array]);

			} break;
			default:
				/* Checked in alloc_calib_log_buf(). */
				break;
			}
			break;

		case CHERRY_CALIB_VALUE_DATA: {
			uint8_t *data = (uint8_t *)key->data;
			for (index_data = 0; index_data < key->size;
			     index_data++)
				snprintf(&str[strlen(str)], size, "%#01x ",
					 data[index_data]);
		} break;
		default:
			/* Checked in alloc_calib_log_buf(). */
			break;
		}
		QLOGD("%s", str);
#ifdef CONFIG_CHERRY_EXAMPLES_ZEPHYR
		/* This sleep allows to avoid to drop messages. */
		qtime_msleep(8);
#endif
		free_calib_log_buf(str);
	}
}

void util_dump_fira_caps(struct cherry_fira_capabilities *fira_caps)
{
	if (!fira_caps)
		return;

	QLOGI("Device FiRa Capabilities:");
	QLOGI("\tMax Data Message Size: %u", fira_caps->max_data_message_size);
	QLOGI("\tMax Data Packet Payload Size: %u",
	      fira_caps->max_data_packet_payload_size);
	QLOGI("\tPHY Version Range: 0x%08X", fira_caps->phy_version_range);
	QLOGI("\tMAC Version Range: 0x%08X", fira_caps->mac_version_range);
	QLOGI("\tDevice Type: 0x%02X", fira_caps->device_type);
	QLOGI("\tDevice Roles: 0x%04X", fira_caps->device_roles);
	QLOGI("\tRanging Methods: 0x%04X", fira_caps->ranging_methods);
	QLOGI("\tSTS Config: 0x%02X", fira_caps->sts_config);
	QLOGI("\tMulti Node Mode: 0x%02X", fira_caps->multi_node_mode);
	QLOGI("\tRanging Time Struct: 0x%02X", fira_caps->ranging_time_struct);
	QLOGI("\tSchedule Mode: 0x%02X", fira_caps->schedule_mode);
	QLOGI("\tHopping Mode: 0x%02X", fira_caps->hopping_mode);
	QLOGI("\tBlock Striding: 0x%02X", fira_caps->block_striding);
	QLOGI("\tUWB Initiation Time: 0x%02X", fira_caps->uwb_initiation_time);
	QLOGI("\tChannels: 0x%02X", fira_caps->channels);
	QLOGI("\tRFrame Config: 0x%02X", fira_caps->rframe_config);
	QLOGI("\tCC Constraint Length: 0x%02X",
	      fira_caps->cc_constraint_length);
	QLOGI("\tBPRF Parameter Sets: 0x%02X", fira_caps->bprf_parameter_sets);
	QLOGI("\tHPRF Parameter Sets: 0x%016" PRIx64,
	      fira_caps->hprf_parameter_sets);
	QLOGI("\tAoA Support: 0x%02X", fira_caps->aoa_support);
	QLOGI("\tExtended MAC Address: 0x%02X",
	      fira_caps->extended_mac_address);
	QLOGI("\tSession Key Length: 0x%02X", fira_caps->session_key_length);
	QLOGI("\tDT Anchor Max Active RR: %u",
	      fira_caps->dt_anchor_max_active_rr);
	QLOGI("\tDT Tag Max Active RR: %u", fira_caps->dt_tag_max_active_rr);
	QLOGI("\tDT Tag Block Skipping: 0x%02X",
	      fira_caps->dt_tag_block_skipping);
	QLOGI("\tPSDU Length Support: 0x%02X", fira_caps->psdu_length_support);
	QLOGI("\tLL Capability Params: 0x%04X",
	      fira_caps->ll_capability_params);
	QLOGI("\tBypass Mode Support: 0x%02X", fira_caps->bypass_mode_support);
}

void util_dump_radar_caps(struct cherry_radar_capabilities *radar_caps)
{
	if (!radar_caps)
		return;

	QLOGI("Device Radar Capabilities :");
	QLOGI("\tRadar function is %s",
	      radar_caps->support ? "supported" : "not supported");
}

void util_dump_ccc_caps(struct cherry_ccc_capabilities *ccc_caps)
{
	if (!ccc_caps)
		return;

	QLOGD("Device CCC Capabilities:");
	QLOGD("\tSlot Bitmask: 0x%02X", ccc_caps->slot_bitmask);
	QLOGD("\tSync Code Index Bitmask: 0x%08X",
	      ccc_caps->sync_code_index_bitmask);
	QLOGD("\tHopping Config Bitmask: 0x%02X",
	      ccc_caps->hopping_config_bitmask);
	QLOGD("\tChannel Bitmask: 0x%02X", ccc_caps->channel_bitmask);

	QLOGD("\tProtocol Versions:");
	for (size_t i = 0; i < ccc_caps->protocol_versions.len; i++) {
		QLOGD("\t\tVersion: 0x%04X",
		      ccc_caps->protocol_versions.items[i]);
	}

	QLOGD("\tUWB Configs:");
	for (size_t i = 0; i < ccc_caps->uwb_configs.len; i++) {
		QLOGD("\t\tConfig: 0x%04X", ccc_caps->uwb_configs.items[i]);
	}

	QLOGD("\tPulse Shape Combos:");
	for (size_t i = 0; i < ccc_caps->pulse_shape_combos.len; i++) {
		QLOGD("\t\tCombo: 0x%02X",
		      ccc_caps->pulse_shape_combos.items[i]);
	}

	QLOGD("\tMinimum RAN Multiplier: %u", ccc_caps->minimum_ran_multiplier);
}

void util_dump_caps(
	const struct cherry_core_event_device_capabilities *device_caps)
{
	util_dump_fira_caps(device_caps->fira_capabilities);
	util_dump_radar_caps(device_caps->radar_capabilities);
	util_dump_ccc_caps(device_caps->ccc_capabilities);
}

void util_dump_core_event(const struct cherry_core_event *event)
{
	switch (event->type) {
	case CHERRY_CORE_EVENT_TYPE_DEVICE_STATUS:
		QLOGI("Device status changed: %s",
		      device_state_to_str(event->data.device_status->state));
		QLOGI("Change reason: %s",
		      device_reason_to_str(event->data.device_status->reason));
		break;
	case CHERRY_CORE_EVENT_TYPE_ERROR:
		QLOGE("Error core event: %s",
		      cherry_err_str(event->data.device_error->status_err));
		break;
	case CHERRY_CORE_EVENT_TYPE_CALIB_UPDATE:
		QLOGD("CALIB UPDATE EVENT received with: %s",
		      cherry_err_str(event->data.device_error->status_err));
		break;
	case CHERRY_CORE_EVENT_TYPE_DEVICE_INFO: {
		char str[CHERRY_DEV_INFO_SOC_ID_LEN * 2 + 1] = "";
		if (event->data.device_info->status_err == CHERRY_ERR_NONE) {
			QLOGI("UCI Version = %#04x",
			      event->data.device_info->uci_version);
			QLOGI("mac version = %#04x",
			      event->data.device_info->mac_version);
			QLOGI("phy version = %#04x",
			      event->data.device_info->phy_version);
			QLOGI("uci test version = %#04x",
			      event->data.device_info->uci_test_version);
			QLOGI("fw version = %s",
			      event->data.device_info->fw_version);
			for (int i = 0; i < CHERRY_DEV_INFO_SOC_ID_LEN; i++) {
				snprintf(&str[strlen(str)], sizeof(str), "%02x",
					 event->data.device_info->soc_id[i]);
			}
			QLOGI("SOC_ID = %s", str);
			QLOGI("device_id = %x",
			      event->data.device_info->device_id);
			if (event->data.device_info->package_id) {
				QLOGI("package_id = sip");
			} else {
				QLOGI("package_id = soc");
			}
			QLOGI("flavor = %s", event->data.device_info->flavor);
			QLOGI("product id = %08x",
			      event->data.device_info->product_id);
			QLOGI("SOI variant = %d",
			      event->data.device_info->soi_variant);
			QLOGI("ROM revision = %d",
			      event->data.device_info->rom_revision);
		} else
			QLOGE("Device info event received with error: %s",
			      cherry_err_str(
				      event->data.device_stats->status_err));
		break;
	}
	case CHERRY_CORE_EVENT_TYPE_DEVICE_STATS:
		if (event->data.device_stats->status_err == CHERRY_ERR_NONE) {
			QLOGI("temperature = %.2lf °C",
			      (double)(event->data.device_stats
					       ->temperature_100th_celsius) /
				      100);
		} else
			QLOGE("Device stats event received with error: %s",
			      cherry_err_str(
				      event->data.device_stats->status_err));
		break;
	case CHERRY_CORE_EVENT_TYPE_GET_CALIB:
		if (event->data.get_calib->status_err != CHERRY_ERR_NONE)
			QLOGE("Get calib event received with error %d",
			      event->data.get_calib->status_err);
		break;
	case CHERRY_CORE_EVENT_TYPE_TIMESTAMP:
		if (event->data.get_calib->status_err == CHERRY_ERR_NONE) {
			QLOGD("timestamp = %016x",
			      (uint64_t)(event->data.device_timestamp
						 ->timestamp_us));
		} else
			QLOGE("Get timestamp event received with error %d",
			      event->data.device_timestamp->status_err);
		break;
	case CHERRY_CORE_EVENT_TYPE_DEVICE_CAPS:
		if (event->data.device_caps->status_err == CHERRY_ERR_NONE)
			util_dump_caps(event->data.device_caps);
		else
			QLOGE("Device capabilities event received with error %d",
			      event->data.device_caps->status_err);
		break;
	case CHERRY_CORE_EVENT_TYPE_GPIO_TOGGLE:
		if (event->data.gpio_toggle->status_err == CHERRY_ERR_NONE) {
			QLOGD("timestamp = %016x",
			      (uint64_t)(event->data.gpio_toggle->timestamp_us));
		} else
			QLOGE("Get gpio toggle timestamp event received with error %d",
			      event->data.gpio_toggle->status_err);
		break;
	default:
		break;
	}
}

void util_dump_session_fira(const struct cherry_fira_event *event,
			    uint32_t session_id)
{
	QLOGD("Event received for session id %d", session_id);

	switch (event->type) {
	case CHERRY_FIRA_EVENT_TYPE_SESSION_STATUS:
		switch (event->data.status->session_state) {
		case CHERRY_FIRA_SESSION_STATE_INIT:
			QLOGD("FiRa status changed: CHERRY_FIRA_SESSION_STATE_INIT");
			break;
		case CHERRY_FIRA_SESSION_STATE_DEINIT:
			QLOGD("FiRa status changed: CHERRY_FIRA_SESSION_STATE_DEINIT");
			break;
		case CHERRY_FIRA_SESSION_STATE_ACTIVE:
			QLOGD("FiRa status changed: CHERRY_FIRA_SESSION_STATE_ACTIVE");
			break;
		case CHERRY_FIRA_SESSION_STATE_IDLE:
			QLOGD("FiRa status changed: CHERRY_FIRA_SESSION_STATE_IDLE");
			break;
		default:
			QLOGD("FiRa status changed: unknown state %d",
			      event->data.status->session_state);
			break;
		}
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_ERROR:
		QLOGE("FiRa error event: %s",
		      cherry_err_str(event->data.error->status_err));
		break;
	case CHERRY_FIRA_EVENT_TYPE_SESSION_TWR_RANGING_REPORT: {
		int index;
		const struct cherry_fira_session_twr_ranging_report *results =
			event->data.twr_ranging;

		QLOGI("Ranging report:\n");
		QLOGI("Ranging measurements data:");
		QLOGI("Session sequence counter: %u\n",
		      results->sequence_number);

		for (index = 0; index < results->n_measurements; index++) {
			const struct cherry_fira_session_twr_measurements
				*cur_meas = &results->measurements[index];
			QLOGI("for device short address %u:",
			      cur_meas->short_addr);
			if (!cur_meas->frame_status) {
				util_dump_twr_range_report(cur_meas);
			} else {
				QLOGI("\tRanging error detected with error status: %s on slot index %u\n",
				      frame_status_to_str(
					      cur_meas->frame_status),
				      cur_meas->slot_index);
			}
		}
		break;
	}
	case CHERRY_FIRA_EVENT_TYPE_SESSION_DT_TAG_RANGING_REPORT: {
		const struct cherry_fira_session_dt_tag_ranging_report *results =
			event->data.dt_tag_ranging;
		int index;

		QLOGI("DT-Tag report:\n");
		QLOGI("Session sequence counter: %u\n",
		      results->sequence_number);

		for (index = 0; index < results->n_measurements; index++) {
			const struct cherry_fira_session_dt_tag_measurements
				*cur_meas = &results->measurements[index];
			QLOGI("for device short address %u:",
			      cur_meas->short_addr);
			if (!cur_meas->frame_status) {
				util_dump_dt_tag_report(cur_meas);
			} else {
				QLOGI("\tRanging error detected with error status: %s on slot index %u\n",
				      frame_status_to_str(
					      cur_meas->frame_status),
				      cur_meas->block_index);
			}
		}
		break;
	}
	case CHERRY_FIRA_EVENT_TYPE_SESSION_DIAGNOSTIC_REPORT:
		QLOGI("Ranging Diagnostic report:");
		util_dump_diagnostic(event->data.diagnostics);
		break;
	default:
		break;
	}
}

const char *radar_state_to_str(enum cherry_radar_session_state state)
{
	switch (state) {
	case CHERRY_RADAR_SESSION_STATE_INIT:
		return "INIT";
	case CHERRY_RADAR_SESSION_STATE_DEINIT:
		return "DEINIT";
	case CHERRY_RADAR_SESSION_STATE_ACTIVE:
		return "ACTIVE";
	case CHERRY_RADAR_SESSION_STATE_IDLE:
		return "IDLE";
	}
	return "UNKNOWN";
}

const char *radar_reason_to_str(enum cherry_radar_state_change_reason reason)
{
	switch (reason) {
	case CHERRY_RADAR_STATE_CHANGE_REASON_MGMT_CMD:
		return "MNGT CMD";
	case CHERRY_RADAR_STATE_CHANGE_REASON_MAX_MEASUREMENT:
		return "MAX MEASUREMENT";
	case CHERRY_RADAR_STATE_CHANGE_REASON_UNKNOWN:
		return "UNKNOWN";
	case CHERRY_RADAR_STATE_CHANGE_REASON_FORCE_STOPPED:
		return "FORCE STOPPED";
	}
	return "UNKNOWN";
}

static int radar_two_complement(int value)
{
	if ((value & (1 << (24 - 1))) != 0)
		value = value - (1 << 24);

	return value;
}

static void parse_radar_sweep(struct cherry_radar_sweep *sweeps,
			      enum cherry_radar_sample_size sample_size,
			      const char *file, char *buffer)
{
	int num_of_bytes = sample_size * 2 + 4;
	int real, img;
	FILE *fptr = stdout;
	char *current_buffer = buffer;
	int count;

#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	if (file) {
		fptr = fopen(file, "a");
		if (!fptr) {
			QLOGE("Can not open file %s", file);
			fptr = stdout;
		}
	}
#endif

	count = sprintf(current_buffer, "CIR:");
	current_buffer += count;
	for (int j = 0; j < sweeps->n_data_fragments; j++) {
		uint8_t *data = sweeps->data_fragments[j].data;
		for (int k = 0; k < sweeps->data_fragments[j].size;
		     k += num_of_bytes) {
			switch (sample_size) {
			case CHERRY_RADAR_SAMPLE_SIZE_32_BITS:
				break;
			case CHERRY_RADAR_SAMPLE_SIZE_48_BITS:
				real = (data[k]) + (data[k + 1] << 8) +
				       (data[k + 2] << 16);
				img = (data[k + (num_of_bytes / 2)]) +
				      (data[k + 1 + (num_of_bytes / 2)] << 8) +
				      (data[k + 2 + (num_of_bytes / 2)] << 16);

				real = radar_two_complement(real);
				img = radar_two_complement(img);

				count = sprintf(current_buffer, "%d%+dj,", real,
						img);
				current_buffer += count;
				break;
			case CHERRY_RADAR_SAMPLE_SIZE_64_BITS:
				break;
			default:
				break;
			}
		}
	}
	if (fptr == stdout) {
		QLOGI("%s", buffer);
	}
#ifndef CONFIG_CHERRY_EXAMPLES_ZEPHYR
	else {
		fprintf(fptr, "%s\n", buffer);
		fclose(fptr);
	}
#endif
}

static void parse_radar_data(struct cherry_radar_session_report *report,
			     const char *file, char *buffer, bool first_frame)
{
	if (report->status != 0)
		QLOGI("Radar frame received with an error");
	else {
		if (first_frame) {
			QLOGI("Radar frame:");
			QLOGI("Number of sweeps: %u", report->n_sweeps);
			QLOGI("Number of samples per sweep: %u",
			      report->samples_per_sweep);
			QLOGI("Size in bits of the samples: %u",
			      report->sample_size);
		}

		for (int i = 0; i < report->n_sweeps; i++) {
			QLOGI("Sweep %u: Sequence number %u, Timestamp (in RSTU): %" PRIu64
			      "",
			      i, report->sweeps[i].timestamp,
			      report->sweeps[i].timestamp);
			parse_radar_sweep(&report->sweeps[i],
					  report->sample_size, file, buffer);
		}
	}
}

void util_dump_session_radar(const struct cherry_radar_event *event,
			     const char *file, char *buffer, bool first_frame)
{
	switch (event->type) {
	case CHERRY_RADAR_EVENT_TYPE_SESSION_STATUS:
		QLOGD("RADAR status changed: %s",
		      radar_state_to_str(event->data.status->session_state));
		QLOGD("Change reason: %s",
		      radar_reason_to_str(event->data.status->reason_code));
		break;
	case CHERRY_RADAR_EVENT_TYPE_SESSION_ERROR:
		QLOGD("Error RADAR event: %s",
		      cherry_err_str(event->data.error->status_err));
		break;
	case CHERRY_RADAR_EVENT_TYPE_SESSION_REPORT:
		parse_radar_data(event->data.report, file, buffer, first_frame);
		break;
	default:
		break;
	}
}

void util_dump_anchor_parameters(
	const struct cherry_fira_anchor_round_config *round_conf, int n_rounds)
{
	QLOGD("     ====================");
	QLOGD("     ANCHOR CONFIGURATION");
	QLOGD("     ====================");
	for (int i = 0; i < n_rounds; i++) {
		const char *role;
		struct cherry_fira_anchor_round_config config = round_conf[i];
		if (!config.role) {
			role = "responder";
		} else {
			role = "initiator";
		}
		QLOGD("     --- Round index %d ---", config.round_idx);
		QLOGD("Anchor role is %s on index %d", role, config.round_idx);
		if (config.role) {
			QLOGD("n responder : %d", config.n_responders);
			for (int j = 0; j < config.n_responders; j++)
				QLOGD("Responder address : %d",
				      config.responders_addr[j]);
		}
	}
	QLOGD("===================================");
}
