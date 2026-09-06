/*
 * Implementation of uci functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "uci/uci.h"

#include "uci/uci_message.h"
#include "uci/uci_spec_fira.h"
#include "uci_internal.h"

#include <qmutex.h>
#include <qsemaphore.h>
#include <string.h>

/* Log level to Warning by default. */
unsigned int uci_log_level = 2;

#define QLOG_CURRENT_LEVEL uci_log_level
#define LOG_TAG "qorvo.uwb.uci"
#include <qlog.h>

#define RSP(mt_gid_oid) (((mt_gid_oid)&0xfff) | 0x4000)

#if (defined(__linux__) || defined(__ANDROID__) || defined(__ZEPHYR__))
#define UCI_DUMP_ACTIVATED
#endif

#ifdef UCI_DUMP_ACTIVATED
#ifndef __KERNEL__
#include <stdio.h>
#endif
static void check_print_msg(struct uci *uci, struct uci_blk *packet,
			    bool sending);
#else
static void check_print_msg(struct uci *uci, struct uci_blk *packet,
			    bool sending)
{
}
#endif

#ifdef __KERNEL__
#undef current
#endif

static struct uci_blk *uci_alloc_status(struct uci *uci)
{
	struct uci_blk *blk = uci_blk_alloc(uci, UCI_STATUS_PACKET_SIZE, 0);
	if (!blk) {
		QLOGE("%s: Failed to allocate uci block for status", __func__);
		return NULL;
	}

	if (blk->size < UCI_STATUS_PACKET_SIZE) {
		uci_blk_free_all(uci, blk);
		QLOGE("%s: Failed to allocate uci block big enough", __func__);
		return NULL;
	}

	blk->flags = UCI_BLK_FLAGS_HEADER_RESERVED;
	blk->len = UCI_PACKET_HEADER_SIZE;
	blk->total_len = 0;
	return blk;
}

static inline struct uci_blk *uci_get_prealloc_status(struct uci *uci)
{
	struct uci_blk *blk;
	if (uci->rsp_blk) {
		/* block is already pre-allocated */
		blk = uci->rsp_blk;
		uci->rsp_blk = NULL;
		return blk;
	}

	return uci_alloc_status(uci);
}

void clear_uci_msg_info(struct uci *uci)
{
	uci->data_msg_info.destination_address = 0;
	uci->data_msg_info.sequence_number = 0;
	uci->data_msg_info.app_data_len = 0;
	uci->data_msg_info.app_data_rem_len = 0;
	uci->data_msg_info.is_last_packet = true;
	uci->data_msg_info.is_last_packet_prev = true;
}

enum qerr uci_init(struct uci *uci, struct uci_allocator *allocator,
		   bool is_client)
{
	if (!allocator)
		return QERR_ENOENT;

	uci->mutex = qmutex_init();
	if (!uci->mutex)
		return QERR_ENOMEM;

#ifdef UCI_DUMP_ACTIVATED
	uci->sem_print = qsemaphore_init(1, 1);
	if (!uci->sem_print) {
		qmutex_deinit(uci->mutex);
		return QERR_ENOMEM;
	}
#endif
	uci->allocator = allocator;
	uci->handlers_head = NULL;
	uci->generic_handler = NULL;
	uci->rx = NULL;
	uci->tx = NULL;
	uci->tr = NULL;
	uci->is_client = is_client;
	uci->known_gid = 0;
	uci->device_state = UCI_DEVICE_STATE_READY;
	clear_uci_msg_info(uci);
	/* Pre-allocate a rsp message to reduce latency. */
	uci->rsp_blk = uci_alloc_status(uci);
	return QERR_SUCCESS;
}

void uci_set_log_level(uint8_t level)
{
	uci_log_level = level;
}

void uci_uninit(struct uci *uci)
{
	if (uci->rsp_blk)
		uci->allocator->ops->free(uci->allocator, uci->rsp_blk);

	uci->handlers_head = NULL;

	uci->allocator = NULL;
	uci->rsp_blk = NULL;

#ifdef UCI_DUMP_ACTIVATED
	qsemaphore_deinit(uci->sem_print);
#endif

	qmutex_deinit(uci->mutex);
}

struct uci_blk *uci_blk_alloc(struct uci *uci, size_t size_hint,
			      uint8_t flags_hint)
{
	struct uci_blk *b;

	b = uci->allocator->ops->alloc(uci->allocator, size_hint, flags_hint);
	if (b) {
		b->next = NULL;
		b->len = 0;
		b->total_len = 0;
		b->flags = flags_hint;
	}

	return b;
}

void uci_blk_free_all(struct uci *uci, struct uci_blk *block)
{
	struct uci_blk *next;

	while (block) {
		next = block->next;

		if (block->flags & UCI_BLK_FLAGS_DESTRUCTIBLE) {
			struct uci_blk_destructible *blk_dest =
				(struct uci_blk_destructible *)block;
			blk_dest->destructor(blk_dest->destructor_arg,
					     blk_dest);
		} else if (!(block->flags & UCI_BLK_FLAGS_STATIC)) {
			uci->allocator->ops->free(uci->allocator, block);
		}

		block = next;
	}
}

void uci_message_handlers_unregister(struct uci *uci,
				     struct uci_message_handlers *handlers)
{
	struct uci_message_handlers *cur, **next;

	/* Search handlers in chain. */
	next = &uci->handlers_head;
	cur = uci->handlers_head;
	while (cur) {
		if (cur == handlers)
			break;
		next = &cur->next;
		cur = cur->next;
	}
	if (!cur)
		return;
	/* Remove specifed handlers from chain. */
	*next = cur->next;
	/* Recalc known_gid from scratch. */
	uci->known_gid = 0;
	cur = uci->handlers_head;
	while (cur) {
		uint8_t first;
		first = UCI_GID(cur->handlers[0].mt_gid_oid);
		uci->known_gid |= 1u << first;
		cur = cur->next;
	}
}

void uci_message_handlers_register(struct uci *uci,
				   struct uci_message_handlers *handlers)
{
	uint8_t first;
	size_t i;

	if (handlers == NULL)
		return;

	first = UCI_GID(handlers->handlers[0].mt_gid_oid);

	/* Check handler table consistency */
	for (i = 1; i < handlers->n_handlers; i++) {
		uint8_t next = UCI_GID(handlers->handlers[i].mt_gid_oid);
		if (next != first) {
			QLOGE("%s: Mixed GID in table ([%zu]%u != [0]%u)!!!",
			      __func__, i, next, first);
			QLOGE("Not allowed.");
			return;
		}
	}
	/* Add it to linked list */
	handlers->next = uci->handlers_head;
	uci->handlers_head = handlers;
	uci->known_gid |= 1u << first;
}

void uci_message_generic_handler_unregister(struct uci *uci)
{
	uci->generic_handler = NULL;
}

void uci_message_generic_handler_register(struct uci *uci,
					  struct uci_generic_handler *handler)
{
	uci->generic_handler = handler;
}

static bool gid_is_known(struct uci *uci, uint16_t mt_gid_oid)
{
	uint8_t gid = UCI_GID(mt_gid_oid);
	return !!(uci->known_gid & (1u << gid));
}

struct uci_blk *uci_packet_recv_alloc(struct uci *uci, uint16_t size_hint)
{
	return uci_blk_alloc(uci, size_hint, 0);
}

void uci_packet_recv_free_all(struct uci *uci, struct uci_blk *blks)
{
	uci_blk_free_all(uci, blks);
}

static void handle_unknown_control_packet(struct uci *uci, uint16_t mt_gid_oid,
					  bool gid_is_known)
{
	uint8_t status;

	if (uci->is_client) {
		int mt = UCI_MT(mt_gid_oid);
		if (mt == UCI_MESSAGE_TYPE_COMMAND) {
			QLOGW("%s: client received a command with header=(%d,%d,%d)",
			      __func__, UCI_MT(mt_gid_oid), UCI_GID(mt_gid_oid),
			      UCI_OID(mt_gid_oid));
			return;
		}
		/* Client is not supposed to be missing a handler */
		QLOGD("%s: header=(%d,%d,%d)", __func__, UCI_MT(mt_gid_oid),
		      UCI_GID(mt_gid_oid), UCI_OID(mt_gid_oid));
		/* Client is not supposed to answer */
		return;
	} else {
		int mt = UCI_MT(mt_gid_oid);
		if (mt == UCI_MESSAGE_TYPE_RESPONSE ||
		    mt == UCI_MESSAGE_TYPE_NOTIFICATION) {
			QLOGW("%s: server received a response or notif with header=(%d,%d,%d)",
			      __func__, UCI_MT(mt_gid_oid), UCI_GID(mt_gid_oid),
			      UCI_OID(mt_gid_oid));
			return;
		}
	}
	status = gid_is_known ? UCI_STATUS_UNKNOWN_OID : UCI_STATUS_UNKNOWN_GID;
	QLOGD("%s: header=(%d,%d,%d) status=%d", __func__, UCI_MT(mt_gid_oid),
	      UCI_GID(mt_gid_oid), UCI_OID(mt_gid_oid), status);
	uci_send_status(uci, mt_gid_oid, status);
}

static bool search_call_handler(struct uci *uci,
				struct uci_message_handlers *current,
				uint16_t uci_message_id, int *error)
{
	size_t i;

	/* This assumes all the handlers in the table are for THE SAME GID ONLY! */
	uint8_t gid = UCI_GID(uci_message_id),
		hgid = UCI_GID(current->handlers[0].mt_gid_oid),
		oid = UCI_OID(uci_message_id);

	if (gid != hgid)
		return false;

	for (i = 0; i < current->n_handlers; i++) {
		const struct uci_message_handler *hdlr;
		hdlr = &current->handlers[i];

		if (oid < UCI_OID(hdlr->uci_message_id)) {
			/* OID are ordered in handlers table so
			no need to check till the end */
			break;
		}

		if (hdlr->uci_message_id == uci_message_id) {
			*error = hdlr->handler(uci, uci_message_id, uci->rx,
					       current->user_data);
			return true;
		}
	}
	return false;
}

static inline int check_device_state(struct uci *uci, uint16_t uci_message_id)
{
	uint8_t mt = UCI_MT(uci_message_id);
	if (!uci->is_client) {
		uint8_t device_status = uci_get_device_state(uci);
		if (is_control_mt(mt)) {
			/* If device state is 'broken' and this is Control
			 * Packet, but not the reset command, reject it */
			if (device_status == UCI_DEVICE_STATE_ERROR &&
			    uci_message_id !=
				    UCI_MT_GID_OID(UCI_MESSAGE_TYPE_COMMAND,
						   UCI_GID_CORE,
						   UCI_OID_CORE_DEVICE_RESET)) {
				uci_send_status(uci, uci_message_id,
						UCI_STATUS_REJECTED);
				return -1;
			}
		}
	}
	return 0;
}

static inline void handle_full_message(struct uci *uci, uint16_t uci_message_id)
{
	struct uci_message_handlers *current;
	bool handled = false;
	uint8_t mt = UCI_MT(uci_message_id);
	int error;

	if (check_device_state(uci, uci_message_id))
		return;

	/* Search and call the corresponding handler */
	for (current = uci->handlers_head; current; current = current->next) {
		/* Search in current handlers table and call it if found. */
		if (search_call_handler(uci, current, uci_message_id, &error)) {
			handled = true;
			break;
		}
	}

	/* Check if generic handler has been registered. */
	if (!handled && uci->generic_handler) {
		handled = true;
		error = uci->generic_handler->handler(
			uci, uci_message_id, uci->rx,
			uci->generic_handler->user_data);
	}

	if (handled) {
		if (error) {
			QLOGE("%s: error %d occurred while processing "
			      "message with header 0x%04X",
			      __func__, error, uci_message_id);
			if (!uci->is_client && is_control_mt(mt))
				uci_send_status(uci, uci_message_id,
						UCI_STATUS_FAILED);
		}
		/* Handled. */
		return;
	}

	if (is_control_mt(mt)) {
		handle_unknown_control_packet(
			uci, uci_message_id, gid_is_known(uci, uci_message_id));
	} else {
		QLOGE("%s: Packet with uci_message_id (mt_gid_oid / mt_dpf / mt ) %u was not handled",
		      __func__, uci_message_id);
	}
}

static inline bool uci_pbf_packet_recv(struct uci *uci, struct uci_blk *packet,
				       uint16_t uci_message_id)
{
	/* Get info from packet header */
	bool segmented = uci_blk_is_segment(packet);

	/* Check that this packet matches the current rx, if any */
	if (uci->rx) {
		/* Compare without PBF */
		uint16_t stored_uci_message_id = uci_blk_get_uci_message_id(
			uci->rx, uci_blk_get_mt(uci->rx));
		if (uci_message_id != stored_uci_message_id) {
			QLOGE("%s: previous segmented packet with uci_message_id (mt_gid_oid / mt_dpf / mt ) %u was not finished, we drop it",
			      __func__, stored_uci_message_id);
			uci_blk_free_all(uci, uci->rx);
			uci->rx = packet;
			uci->rx_last = packet;
		} else {
			/* Additional segment received. */
			uci->rx_last->next = packet;
			uci->rx_last = packet;
			/* Update full chain total_len now to avoid doing it in
			   a loop after. */
			uci->rx->total_len += packet->total_len;
			/* Reset total_len of non-first segment. */
			packet->total_len = 0;
			/* Let the UCI parser deal with the header itself.
			 * TODO: It may be better to remove header here. */
		}
	} else {
		/* First segment received. */
		uci->rx = packet;
		uci->rx_last = packet;
	}
	/* We may receive an uci_blk chain, so ensure last is really last one. */
	while (uci->rx_last->next) {
		uci->rx_last = uci->rx_last->next;
		check_print_msg(uci, uci->rx_last, false);
	}

	/* Message not completed yet */
	return segmented;
}

static inline void uci_pbf_packet_data_recv(struct uci *uci,
					    struct uci_blk *packet)
{
	if (uci->rx != NULL) {
		QLOGE("%s: previous segmented packet with uci_message_id "
		      "(mt_gid_oid/mt_dpf/mt) %u was not finished, we drop it",
		      __func__,
		      uci_blk_get_uci_message_id(uci->rx,
						 uci_blk_get_mt(uci->rx)));
		uci_blk_free_all(uci, uci->rx);
	}

	uci->data_msg_info.is_last_packet_prev =
		uci->data_msg_info.is_last_packet;

	/* Get info from packet header about PBF field. */
	uci->data_msg_info.is_last_packet = !uci_blk_is_segment(packet);

	/* In case of DATA we always want to pass Data Packet to uwbmac->mac
	in order to get DATA_CREDIT_NTF, even if it is not the last segment in
	message. */
	uci->rx = packet;
	uci->rx_last = packet;
	check_print_msg(uci, uci->rx_last, false);
}

void uci_packet_recv(struct uci *uci, struct uci_blk *packet)
{
	uint16_t uci_message_id;
	enum uci_message_type mt;

	if (packet->len < UCI_PACKET_HEADER_SIZE ||
	    packet->len > uci_packet_hdr_get_msg_max_size(packet)) {
		/* Bad form packet. */
		QLOGE("%s: error bad packet len given: %d", __func__,
		      packet->len);
		uci_blk_free_all(uci, packet);
		return;
	}

	/* The received uci_blk chain always starts with the header. */
	packet->flags |= UCI_BLK_FLAGS_HEADER_RESERVED;
	check_print_msg(uci, packet, false);

	mt = uci_blk_get_mt(packet);
	uci_message_id = uci_blk_get_uci_message_id(packet, mt);
	packet->total_len = uci_packet_hdr_get_payload_size(packet->data);

	switch (mt) {
	case UCI_MESSAGE_TYPE_DATA:
		if (uci_blk_get_mt_dpf(packet) & 0x0F00) {
			/*
			 * If DPF is RADAR type the message is processed as NTF
			 * to allow to receive all segments before handle it.
			 */
			clear_uci_msg_info(uci);
			if (uci_pbf_packet_recv(uci, packet, uci_message_id))
				return;
		} else {
			uci_pbf_packet_data_recv(uci, packet);
		}
		break;
	case UCI_MESSAGE_TYPE_COMMAND:
	case UCI_MESSAGE_TYPE_RESPONSE:
	case UCI_MESSAGE_TYPE_NOTIFICATION:
		clear_uci_msg_info(uci);
		if (uci_pbf_packet_recv(uci, packet, uci_message_id))
			return;
		break;

	case UCI_MESSAGE_TYPE_SE_TESTING_COMMAND:
	case UCI_MESSAGE_TYPE_SE_TESTING_RESPONSE:
		clear_uci_msg_info(uci);
		if (uci->rx != NULL) {
			QLOGE("%s: previous segmented packet with uci_message_id (mt_gid_oid / mt_dpf / mt ) %u was not finished, we drop it",
			      __func__,
			      uci_blk_get_uci_message_id(
				      uci->rx, uci_blk_get_mt(uci->rx)));
			uci_blk_free_all(uci, uci->rx);
		}
		uci->rx = packet;
		uci->rx_last = packet;
		break;
		/* Don't need default. A message with incorrect MT will be dropped
		 * because uci_packet_hdr_get_msg_max_size() returns 0. */
	}

	handle_full_message(uci, uci_message_id);
	uci_blk_free_all(uci, uci->rx);
	uci->rx = NULL;
	return;
}

enum qerr uci_transport_attach(struct uci *uci, struct uci_transport *uci_tr)
{
	if (uci->tr)
		return QERR_EBUSY;

	uci->tr = uci_tr;
	uci_tr->ops->attach(uci_tr, uci);

	return QERR_SUCCESS;
}

enum qerr uci_transport_detach(struct uci *uci)
{
	if (!uci->tr)
		return QERR_EINVAL;

	uci->tr->ops->detach(uci->tr);
	uci->tr = NULL;

	uci_blk_free_all(uci, uci->rx);
	uci->rx = NULL;

	uci_blk_free_all(uci, uci->tx);
	uci->tx = NULL;

	return QERR_SUCCESS;
}

static void queue_packet(struct uci *uci, struct uci_blk *first,
			 struct uci_blk *last)
{
	if (uci->tx)
		/* Add this packet to the end if some packets are still
		 * pending. */
		uci->tx_last->next = first;
	else
		/* Queue empty*/
		uci->tx = first;
	/* Update last packet of queue */
	uci->tx_last = last;
}

static inline struct uci_blk *uci_put_headers_to_blocks(uint16_t uci_message_id,
							struct uci_blk *payload)
{
	uint8_t mt = UCI_MT(uci_message_id);

	struct uci_blk *current, *last;
	uint16_t remaining;

	last = current = payload;
	remaining = current->total_len;
	while (current) {
		if (uci_blk_has_header(current)) {
			/* Update header. */
			if (is_control_mt(mt)) {
				uci_blk_put_control_header(
					current, uci_message_id, remaining);
			} else if (is_data_mt(mt)) {
				uci_blk_put_data_header(current, uci_message_id,
							remaining);
			} else {
				QLOGE("%s: Incorrect message type.", __func__);
				UCI_ASSERT(0);
			}
			/* Adjust remaining according to payload len of current */
			remaining -= (current->len - UCI_PACKET_HEADER_SIZE);
		} else {
			remaining -= current->len;
		}
		last = current;
		current = current->next;
		if (!remaining && current) {
			QLOGE("%s: Extra uci_blk in chain while remaining is 0!!!",
			      __func__);
		}
	}
	return last;
}

void uci_send_se_message(struct uci *uci, enum uci_message_type mt,
			 struct uci_blk *payload)
{
	/*
	 * For Message type SE testing, there is no segmentation.
	 * We can use multiple blocks internally but the total lenth will only
	 * be stored on the first block.
	 */
	UCI_ASSERT(uci_blk_has_header(payload));

	/* Set specific header without gid/oid and with length on 2 bytes */
	payload->data[0] = ((uint8_t)mt) << 5;
	payload->data[1] = 0;
	payload->data[2] = payload->total_len;
	payload->data[3] = payload->total_len >> 8;

	qmutex_lock(uci->mutex, QOSAL_WAIT_FOREVER);

	/* Add new chain to TX queue */
	queue_packet(uci, payload, payload);

	qmutex_unlock(uci->mutex);

	/* Finally, let transport send stored packets. */
	uci->tr->ops->packet_send_ready(uci->tr);
}

void uci_send_message(struct uci *uci, uint16_t uci_message_id,
		      struct uci_blk *payload)
{
	struct uci_blk *last_blk;

	qmutex_lock(uci->mutex, QOSAL_WAIT_FOREVER);

	/* no transport attached, just drop the message */
	if (!uci->tr) {
		uci_blk_free_all(uci, payload);
		qmutex_unlock(uci->mutex);
		return;
	}

	if (!payload) {
		payload = uci_get_prealloc_status(uci);
		if (!payload) {
			QLOGE("%s: Not enough memory to send %x", __func__,
			      uci_message_id);
			qmutex_unlock(uci->mutex);
			return;
		}
	}

	/* Put headers in each uci_blk in the chain. */
	last_blk = uci_put_headers_to_blocks(uci_message_id, payload);

	/* Add new chain to TX queue */
	queue_packet(uci, payload, last_blk);

	/* Pre-allocate for next RSP message if necessary */
	if (!uci->rsp_blk)
		uci->rsp_blk = uci_alloc_status(uci);

	qmutex_unlock(uci->mutex);

	/* Finally, let transport send stored packets. */
	uci->tr->ops->packet_send_ready(uci->tr);
}

void uci_send_status(struct uci *uci, uint16_t gid_oid, uint8_t status)
{
	struct uci_blk *blk;

	qmutex_lock(uci->mutex, QOSAL_WAIT_FOREVER);

	/* No transport attached, just drop the message */
	if (!uci->tr) {
		qmutex_unlock(uci->mutex);
		return;
	}

	blk = uci_get_prealloc_status(uci);
	if (!blk) {
		QLOGE("%s: Not enough memory to send status for %x", __func__,
		      gid_oid);
		qmutex_unlock(uci->mutex);
		return;
	}
	blk->data[UCI_PACKET_HEADER_SIZE] = status;
	blk->len += 1;
	blk->total_len += 1;

	qmutex_unlock(uci->mutex);

	uci_send_message(uci, RSP(gid_oid), blk);
}

struct uci_blk *uci_packet_send_get_ready(struct uci *uci)
{
	struct uci_blk *current, *ret;

	qmutex_lock(uci->mutex, QOSAL_WAIT_FOREVER);

	current = ret = uci->tx;
	uci->tx = NULL;

	while (current) {
		check_print_msg(uci, current, true);
		if (current->next && uci_blk_has_header(current->next)) {
			/* beginning of the next packet, cut here */
			uci->tx = current->next;
			current->next = NULL;
			break;
		}
		current = current->next;
	}

	qmutex_unlock(uci->mutex);

	return ret;
}

uint8_t uci_get_device_state(struct uci *uci)
{
	if (uci->is_client) {
		QLOGE("Only server should call %s", __func__);
	}
	return uci->device_state;
}

void uci_set_device_state_notification(struct uci *uci, uint8_t state)
{
	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER_WITH_SIZE(uci, 5);
	if (uci->is_client) {
		QLOGE("Only server can call %s", __func__);
		return;
	}
	if (uci->device_state == UCI_DEVICE_STATE_ERROR)
		return;

	if (uci->device_state == state)
		return;

	uci->device_state = state;

	uci_message_put_8bit(&builder, state);
	uci_send_message(uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_STATUS),
			 builder.message);
}

void uci_reset_device_state(struct uci *uci)
{
	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER_WITH_SIZE(uci, 5);
	if (uci->is_client) {
		QLOGE("Only server can call %s", __func__);
		return;
	}
	uci->device_state = UCI_DEVICE_STATE_READY;

	uci_message_put_8bit(&builder, UCI_DEVICE_STATE_READY);
	uci_send_message(uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					UCI_GID_CORE,
					UCI_OID_CORE_DEVICE_STATUS),
			 builder.message);
}

void uci_send_device_boot_notification(struct uci *uci,
				       enum uci_qorvo_boot_reason boot_reason)
{
	struct uci_message_builder builder =
		UCI_MESSAGE_BUILDER_INITIALIZER_WITH_SIZE(uci, 5);
	if (uci->is_client) {
		QLOGE("Only server can call %s", __func__);
		return;
	}

	uci_message_put_8bit(&builder, (uint8_t)boot_reason);
	uci_send_message(uci,
			 UCI_MT_GID_OID(UCI_MESSAGE_TYPE_NOTIFICATION,
					UCI_GID_QORVO_EXT2,
					UCI_OID_QORVO_CORE_DEVICE_BOOT),
			 builder.message);
}

void uci_packet_send_done(struct uci *uci, struct uci_blk *packet, int status)
{
	uci_blk_free_all(uci, packet);
	//TODO(Guillaume): is this what we want when transmission fails ?
	if (status != 0) {
		uci_blk_free_all(uci, uci->tx);
		uci->tx = NULL;
	}
}

void uci_send_message_raw(struct uci *uci, struct uci_blk *payload)
{
	check_print_msg(uci, payload, true);

	uci->tr->ops->packet_send_raw(uci->tr, payload);
}

#ifdef UCI_DUMP_ACTIVATED
static void print_msg(struct uci_blk *packet, bool sending)
{
	uint16_t i;
#define BUFFER_LEN (16 * 3)
	size_t buffer_len = BUFFER_LEN;
	char buffer[BUFFER_LEN + 1];
	char *dest = buffer;
	const char *max = buffer + buffer_len;

	memset(buffer, '\0', buffer_len + 1);
	if (uci_blk_has_header(packet)) {
		QLOGD("%s: header=(%d,%d,%d) len=%d", __func__,
		      UCI_MT(uci_blk_get_mt_gid_oid(packet)),
		      UCI_GID(uci_blk_get_mt_gid_oid(packet)),
		      UCI_OID(uci_blk_get_mt_gid_oid(packet)), packet->len);
	} else {
		QLOGD("%s: len=%d", __func__, packet->len);
	}
	QLOGD("------------ BEGIN UCI MESSAGE ------------");
	for (i = 0; i < packet->len; ++i) {
		dest += sprintf(dest, "%02x ", packet->data[i]);
		if (dest >= max) {
			// the line is full, flush.
			QLOGD("%s: %s", sending ? "Sending" : "Received",
			      buffer);
			dest = buffer;
		}
	}
	if (dest != buffer) {
		// Flush incomplete line.
		QLOGD("%s: %s", sending ? "Sending" : "Received", buffer);
	}
	QLOGD("------------ END UCI MESSAGE --------------");
}

static inline void check_print_msg(struct uci *uci, struct uci_blk *packet,
				   bool sending)
{
	if (uci_log_level == QLOG_LEVEL_DEBUG) {
		qsemaphore_take(uci->sem_print, 50);
		print_msg(packet, sending);
		qsemaphore_give(uci->sem_print);
	}
}
#endif
