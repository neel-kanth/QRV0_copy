/*
 * Header file for uci internal
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include "uci/uci.h"

static inline bool is_control_mt(uint8_t mt)
{
	return mt == UCI_MESSAGE_TYPE_COMMAND ||
	       mt == UCI_MESSAGE_TYPE_RESPONSE ||
	       mt == UCI_MESSAGE_TYPE_NOTIFICATION;
}

static inline bool is_data_mt(uint8_t mt)
{
	return mt == UCI_MESSAGE_TYPE_DATA;
}

static inline bool is_se_testing_mt(uint8_t mt)
{
	return mt == UCI_MESSAGE_TYPE_SE_TESTING_COMMAND ||
	       mt == UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE;
}

/**
 * uci_blk_put_control_header() - Add header to a block, specific for Control
 * Packet.
 * @blk: The block in which the header will be written.
 * @mt_gid_oid: Union of MT, GID and OID to put in the header.
 * @remaining: payload length remaining.
 */
static inline void uci_blk_put_control_header(struct uci_blk *blk,
					      uint16_t mt_gid_oid,
					      uint16_t remaining)
{
	bool segment = remaining > UCI_MAX_CONTROL_PAYLOAD_SIZE;
	uint8_t pbf = (uint8_t)segment << 4;

	blk->data[0] = ((mt_gid_oid >> 8) | pbf) & 0xff;
	blk->data[1] = mt_gid_oid & 0xff;
	blk->data[2] = 0;
	/* This data[3] is remaining or UCI_MAX_CONTROL_PAYLOAD_SIZE if segment
	 * is true. */
	blk->data[3] = segment ? UCI_MAX_CONTROL_PAYLOAD_SIZE : remaining;
}

/**
 * uci_blk_put_se_testing_control_header() - Add header to a block, specific for
 * SE testing Control Packet.
 *
 * @blk: The block in which the header will be written.
 * @mt_gid_oid: Union of MT, GID and OID to put in the header.
 * @remaining: payload length remaining.
 */
static inline void uci_blk_put_se_testing_control_header(struct uci_blk *blk,
							 uint16_t mt_gid_oid,
							 uint16_t remaining)
{
	blk->data[0] = (mt_gid_oid >> 8) & 0xff;
	blk->data[1] = 0;
	blk->data[2] = (uint8_t)(remaining & 0xff);
	blk->data[3] = (uint8_t)(remaining >> 8 & 0xff);
}

/**
 * uci_blk_put_data_header() - Add header to a block, specific for Data Packet.
 *
 * @blk: The block in which the header will be written.
 * @mt_dpf: Union of MT and DPF to put in the header.
 * @remaining: payload length remaining.
 */
static inline void uci_blk_put_data_header(struct uci_blk *blk, uint16_t mt_dpf,
					   uint16_t remaining)
{
	bool segment = remaining > UCI_MAX_DATA_PAYLOAD_SIZE;
	uint16_t cur_packet_len = segment ? UCI_MAX_DATA_PAYLOAD_SIZE :
					    remaining;
	uint8_t pbf = (uint8_t)segment << 4;

	blk->data[0] = ((mt_dpf >> 8) | pbf) & 0xff;
	blk->data[1] = 0;
	blk->data[2] = (uint8_t)(cur_packet_len & 0xff);
	blk->data[3] = (uint8_t)(cur_packet_len >> 8 & 0xff);
}

/**
 * uci_blk_has_header() - Check if a block has a header.
 * @blk: The block being checked.
 *
 * Return: true if a header is present.
 */
static inline bool uci_blk_has_header(const struct uci_blk *blk)
{
	return blk ? blk->flags & UCI_BLK_FLAGS_HEADER_RESERVED : false;
}

/**
 * uci_blk_get_mt() - Get the MT. Returned value can be directly maped
 * to &enum uci_message_type.
 * @blk: The block from which m is extracted.
 *
 * .. note::
 *    Presence of a header must be checked before this call.
 *
 * Return: Extracted MT from blk.
 */
static inline uint8_t uci_blk_get_mt(const struct uci_blk *blk)
{
	return blk->data[0] >> 5;
}

/**
 * uci_blk_get_mt_dpf() - Get the MT and DPF in format mt_dpf.
 * It's compatible with mt_gid_oid format.
 * This function should be applied to UCI Data Packets only.
 * @blk: The block from which mt_gid_oid is extracted.
 *
 * .. note::
 *    Presence of a header must be checked before this call.
 *
 * Return: Extracted mt_gid_oid from blk.
 */
static inline uint16_t uci_blk_get_mt_dpf(const struct uci_blk *blk)
{
	return (blk->data[0] & 0xEF) << 8;
}

/**
 * uci_blk_get_mt_gid_oid() - Get the MT, GID and OID in format mt_gid_oid.
 * This function should be applied to UCI Control Packets only.
 * @blk: The block from which mt_gid_oid is extracted.
 *
 * .. note::
 *    Presence of a header must be checked before this call.
 *
 * Return: Extracted mt_gid_oid from blk.
 */
static inline uint16_t uci_blk_get_mt_gid_oid(const struct uci_blk *blk)
{
	return blk->data[1] | ((blk->data[0] & 0xEF) << 8);
}

/**
 * uci_blk_get_uci_message_id() - Get the uci_message_id. It is:
 * - for UCI Control Packet - MT, GID and OID in format mt_gid_oid.
 * - for UCI Data Packet - MT and DPF in format mt_dpf.
 * - for SE Testing Packet - MT format compatible with mt_gid_oid and mt_dpf.
 * @blk: The block from which mt_gid_oid is extracted.
 * @mt: Message Type. Could be extracted from blk with uci_blk_get_mt() prior
 * the function call.
 *
 * .. note::
 *    Presence of a header must be checked before this call.
 *
 * Return: Extracted uci_message_id from blk.
 */
static inline uint16_t uci_blk_get_uci_message_id(const struct uci_blk *blk,
						  uint8_t mt)
{
	switch (mt) {
	case UCI_MESSAGE_TYPE_DATA:
		return uci_blk_get_mt_dpf(blk);
	case UCI_MESSAGE_TYPE_COMMAND:
	case UCI_MESSAGE_TYPE_RESPONSE:
	case UCI_MESSAGE_TYPE_NOTIFICATION:
		return uci_blk_get_mt_gid_oid(blk);
	case UCI_MESSAGE_TYPE_SE_TESTING_COMMAND:
	case UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE:
	default:
		return mt << 13;
	}
}

/**
 * uci_blk_is_segment() - Indicates whether the blk is a segment
 * @blk: The block to test.
 *
 * Return: true if block represent a segment.
 */
static inline bool uci_blk_is_segment(const struct uci_blk *blk)
{
	return blk->data[0] & 0x10 ? true : false;
}

/**
 * uci_packet_hdr_get_msg_max_size() - Get the maximum msg size from the UCI
 * block
 * @blk: The block from which the max message size is extracted.
 *
 * Return: the maximum size of the message.
 */
static inline uint32_t
uci_packet_hdr_get_msg_max_size(const struct uci_blk *blk)
{
	enum uci_message_type mt = (enum uci_message_type)uci_blk_get_mt(blk);
	switch (mt) {
	case UCI_MESSAGE_TYPE_DATA:
		return UCI_MAX_DATA_PACKET_SIZE;
	case UCI_MESSAGE_TYPE_COMMAND:
	case UCI_MESSAGE_TYPE_RESPONSE:
	case UCI_MESSAGE_TYPE_NOTIFICATION:
		return UCI_MAX_CONTROL_PACKET_SIZE;
	case UCI_MESSAGE_TYPE_SE_TESTING_COMMAND:
	case UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE:
		return UCI_MAX_SE_TESTING_PACKET_SIZE;
	}
	/* Unknown message type, returning 0 will make UCI log the error and
	 * drop the packet */
	return 0;
}
