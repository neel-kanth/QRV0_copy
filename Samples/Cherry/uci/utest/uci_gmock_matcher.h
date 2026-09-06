/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef UCI_GMOCK_MATCHER_H
#define UCI_GMOCK_MATCHER_H

#include <cstring>
#include <gmock/gmock.h>
#include <iomanip>

extern "C" {
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>
#include <uci_internal.h>
}

static bool is_single_packet(const uci_blk *arg, int ex_mt, int ex_gid,
			     int ex_oid,
			     ::testing::MatchResultListener *result_listener)
{
	if (!arg) {
		*result_listener << "Null response";
		return false;
	}

	if (!uci_blk_has_header(arg)) {
		*result_listener << "packet without header";
		return false;
	}

	uint16_t mt_gid_oid = uci_blk_get_mt_gid_oid(arg);
	int mt = UCI_MT(mt_gid_oid);
	int gid = UCI_GID(mt_gid_oid);
	int oid = UCI_OID(mt_gid_oid);
	if (mt != ex_mt || gid != ex_gid || oid != ex_oid) {
		*result_listener << "Incorrect (MT,GID,OID), got" << mt << ","
				 << gid << "," << oid << ") instead of ("
				 << ex_mt << "," << ex_gid << "," << ex_oid
				 << ")";
		return false;
	}

	if (uci_blk_is_segment(arg)) {
		*result_listener << "Received message is segmented";
		return false;
	}
	return true;
}

MATCHER_P3(IsSinglePacketMtGidOid, ex_mt, ex_gid, ex_oid,
	   "Received packet doesn't match expected MT/GID/OID")
{
	return is_single_packet(arg, ex_mt, ex_gid, ex_oid, result_listener);
}

MATCHER_P3(IsResponseStatus, ex_gid, ex_oid, ex_status,
	   "Received packet doesn't match expected status")
{
	if (!is_single_packet(arg, UCI_MESSAGE_TYPE_RESPONSE, ex_gid, ex_oid,
			      result_listener)) {
		return false;
	}

	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(arg);
	if (uci_message_remaining(&parser) < 1) {
		*result_listener << "Received packet is missing status";
		return false;
	}

	int status = uci_message_get_8bit(&parser);
	if (status != ex_status) {
		*result_listener << "Incorrect status received, got " << status
				 << " instead of " << ex_status;
		return false;
	}
	return true;
}

static inline bool
is_status_with_size(struct uci_message_parser *parser,
		    enum uci_status_code ex_status, uint8_t ex_tlv_length,
		    ::testing::MatchResultListener *result_listener)
{
	if (uci_message_remaining(parser) < 1) {
		*result_listener << "Received packet is missing status";
		return false;
	}

	int status = uci_message_get_8bit(parser);
	if (status != ex_status) {
		*result_listener << "Incorrect status received, got " << status
				 << " instead of " << ex_status;
		return false;
	}

	if (uci_message_remaining(parser) < 1) {
		*result_listener << "Received packet is missing tlv length";
		return false;
	}

	int tlv_length = uci_message_get_8bit(parser);
	if (tlv_length != ex_tlv_length) {
		*result_listener << "Incorrect tlv length received, got "
				 << tlv_length << " instead of "
				 << ex_tlv_length;
		return false;
	}
	return true;
}

MATCHER_P4(IsResponseStatusWithTLV, ex_gid, ex_oid, ex_status, ex_tlv_length,
	   "Received packet doesn't match expected status and/or length")
{
	if (!is_single_packet(arg, UCI_MESSAGE_TYPE_RESPONSE, ex_gid, ex_oid,
			      result_listener)) {
		return false;
	}
	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(arg);
	return is_status_with_size(&parser, (enum uci_status_code)ex_status,
				   ex_tlv_length, result_listener);
}

struct SetAppConfigErrorInfo {
	uint8_t type;
	enum uci_status_code error;
};

MATCHER_P2(IsSetAppResponseError, ex_status, errors,
	   "Received packet didn't match expected response to set app config")
{
	if (!is_single_packet(
		    arg, UCI_MESSAGE_TYPE_RESPONSE, UCI_GID_SESSION_CONFIG,
		    UCI_OID_SESSION_SET_APP_CONFIG, result_listener)) {
		return false;
	}
	int ex_size = errors.size();
	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(arg);

	if (!is_status_with_size(&parser, (enum uci_status_code)ex_status,
				 ex_size, result_listener))
		return false;
	uint8_t type;
	uint8_t error;
	for (int i = 0; i < ex_size; ++i) {
		type = uci_message_get_8bit(&parser);
		const struct SetAppConfigErrorInfo &info = errors[i];
		if (type != info.type) {
			*result_listener << "Incorrect type at index " << i
					 << ", got " << static_cast<int>(type)
					 << " instead of "
					 << static_cast<int>(info.type);
			return false;
		}
		error = uci_message_get_8bit(&parser);
		if (error != info.error) {
			*result_listener << "Incorrect error code at index "
					 << i << ", got "
					 << static_cast<int>(error)
					 << " instead of "
					 << static_cast<int>(info.error);
			return false;
		}
	}

	return true;
}

MATCHER_P3(IsSessionStateNotif, ex_session_id, ex_state, ex_code,
	   "Received packet doesn't match expected session state notification")
{
	if (!is_single_packet(arg, UCI_MESSAGE_TYPE_NOTIFICATION,
			      UCI_GID_SESSION_CONFIG, UCI_OID_SESSION_STATUS,
			      result_listener)) {
		return false;
	}

	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(arg);
	if (uci_message_remaining(&parser) < 6) {
		*result_listener << "Received packet is too small";
		return false;
	}

	uint32_t session_id = uci_message_get_32bit(&parser);
	if (ex_session_id != session_id) {
		*result_listener << "Incorrect session id received, got "
				 << session_id << " instead of "
				 << ex_session_id;
		return false;
	}

	uint8_t state = uci_message_get_8bit(&parser);
	if (ex_state != state) {
		*result_listener << "Incorrect session state received, got "
				 << state << " instead of " << ex_state;
		return false;
	}

	uint8_t code = uci_message_get_8bit(&parser);
	if (code != ex_code) {
		*result_listener << "Incorrect code received, got " << code
				 << " instead of " << ex_code;
		return false;
	}
	return true;
}

MATCHER_P(IsDeviceStateNotif, ex_state,
	  "Received packet doesn't match expected device state notification")
{
	if (!is_single_packet(arg, UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_CORE,
			      UCI_OID_CORE_DEVICE_STATUS, result_listener)) {
		return false;
	}

	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(arg);
	if (uci_message_remaining(&parser) < 1) {
		*result_listener << "Received packet is missing state";
		return false;
	}

	uint8_t state = uci_message_get_8bit(&parser);
	if (ex_state != state) {
		*result_listener << "Incorrect session state received, got "
				 << state << " instead of " << ex_state;
		return false;
	}

	return true;
}

MATCHER_P2(UciEq, expected_raw, expected_size,
	   "Received message doesn't match expected raw packet")
{
	struct uci_message_parser parser = UCI_MESSAGE_PARSER_INITIALIZER(arg);

	uint16_t actual_hdr = uci_blk_get_mt_gid_oid(arg);
	uint16_t expected_hdr = ((uint16_t)expected_raw[0]) << 8 |
				expected_raw[1];

	if (actual_hdr != expected_hdr) {
		*result_listener << std::setfill('0') << std::setw(4)
				 << std::hex << "Header is 0x"
				 << (int)actual_hdr << ", expected 0x"
				 << (int)expected_hdr;
		return false;
	}

	auto actual_size = uci_message_remaining(&parser) + 4;

	if (actual_size != expected_size) {
		*result_listener << "Message size is " << actual_size
				 << ", expected " << expected_size;
		return false;
	}

	for (size_t i = 4; i < expected_size; ++i) {
		uint8_t expected = expected_raw[i];
		uint8_t actual = uci_message_get_8bit(&parser);
		if (actual != expected) {
			*result_listener << "Message byte " << i
					 << std::setfill('0') << std::setw(2)
					 << std::hex << " is 0x" << (int)actual
					 << ", expected 0x" << (int)expected;
			return false;
		}
	}
	return true;
}

// Check that pointed data is the same than the one embedded in the given vector
MATCHER_P(BytesEq, expected_vector, "Unexpected pointed data")
{
	// do not check anything when comparing to an empty vector
	if (expected_vector.size() == 0) {
		return true;
	}
	return 0 == std::memcmp(expected_vector.data(), arg,
				expected_vector.size() *
					sizeof(expected_vector.back()));
}

#endif // UCI_GMOCK_MATCHER_H
