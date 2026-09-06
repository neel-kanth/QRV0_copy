/*
 * Implementation of uci message handling functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "uci/uci_message.h"

#include "uci/uci.h"
#include "uci/uci_spec_fira.h"
#include "uci_internal.h"

#ifdef __KERNEL__
#undef current
#endif

static struct uci_blk *
alloc_blk_with_header(struct uci_message_builder *builder, size_t data_len)
{
	size_t size_needed = UCI_PACKET_HEADER_SIZE + data_len;
	size_t size_ask = builder->expected_packet_size > size_needed ?
				  builder->expected_packet_size :
				  size_needed;

	struct uci_blk *b =
		uci_blk_alloc(builder->uci, size_ask, builder->alloc_flags);
	if (b) {
		if (b->size < UCI_PACKET_HEADER_SIZE) {
			/* not enougth room for header or more */
			uci_blk_free_all(builder->uci, b);
			return NULL;
		}

		b->flags |= UCI_BLK_FLAGS_HEADER_RESERVED;
		b->len = UCI_PACKET_HEADER_SIZE;
		b->total_len = 0;
	}
	return b;
}

enum uci_status_code
uci_message_validate_builder(struct uci_message_builder *builder)
{
	if (builder->first_errno != UCI_STATUS_OK)
		return builder->first_errno;

	if (builder->message)
		/* A block has already been allocated, and it should have
		 * reverved place for the header
		 */
		return UCI_STATUS_OK;

	builder->message = alloc_blk_with_header(builder, 0);
	if (!builder->message)
		return UCI_STATUS_FAILED;
	return UCI_STATUS_OK;
}

static size_t get_remaining(struct uci_message_builder *builder,
			    size_t data_len)
{
	size_t remaining;
	size_t total_len = builder->message ? builder->message->total_len : 0;
	size_t packet_len = total_len % UCI_MAX_CONTROL_PAYLOAD_SIZE;
	struct uci_blk *current_block = builder->current_block;
	if (packet_len == 0) {
		/* current packet is full or not initialized */
		current_block = alloc_blk_with_header(
			builder,
			builder->expected_packet_size - UCI_PACKET_HEADER_SIZE);
		if (!current_block)
			return 0;

		if (!builder->message)
			builder->message = current_block;
		else
			builder->current_block->next = current_block;

		builder->current_block = current_block;
	} else if (current_block->size == current_block->len) {
		/* current block is full */
		current_block = uci_blk_alloc(builder->uci,
					      builder->expected_packet_size,
					      builder->alloc_flags);
		if (!current_block)
			return 0;

		builder->current_block->next = current_block;
		builder->current_block = current_block;
	}

	remaining = current_block->size - current_block->len;
	if (UCI_MAX_CONTROL_PAYLOAD_SIZE - packet_len < remaining)
		return UCI_MAX_CONTROL_PAYLOAD_SIZE - packet_len;
	return remaining;
}

void uci_message_start_nest(struct uci_message_builder *builder,
			    struct uci_message_builder *payload_builder)
{
	// Reserve space for nested length
	uci_message_put_8bit(builder, 0);
	/* Because we only wrote one byte, current still points to the block we
	 * wrote to, and we can get the pointer to it using len - 1.
	 */
	*payload_builder = *builder;
	payload_builder->elems_nr =
		builder->current_block->data + builder->current_block->len - 1;
}

void uci_message_end_nest(struct uci_message_builder *builder,
			  const struct uci_message_builder *payload_builder)
{
	/*
	 * We basically want builder to get all of payload_builder's
	 * information, but keep the nested length pointer intact.
	 */
	uint8_t *elems_nr_saved = builder->elems_nr;
	*builder = *payload_builder;
	builder->elems_nr = elems_nr_saved;
}

int uci_message_put(struct uci_message_builder *builder, const uint8_t *data,
		    size_t data_len)
{
	uint8_t *current = NULL;
	int reserved;

	while (data_len > 0) {
		reserved = uci_message_put_nocopy(builder, data_len, &current);
		if (reserved <= 0) {
			return builder->first_errno;
		}

		memcpy(current, data, reserved);

		data += reserved;
		data_len -= reserved;
	}

	if (builder->elems_nr) {
		(*builder->elems_nr)++;
	}
	return 0;
}

size_t uci_message_get(struct uci_message_parser *parser, uint8_t *data,
		       size_t data_len)
{
	uint8_t *start = data;
	uint8_t *chunk = NULL;
	size_t read;

	while (uci_message_remaining(parser) > 0 && data_len > 0) {
		read = uci_message_get_nocopy(parser, data_len, &chunk);
		memcpy(data, chunk, read);
		data += read;
		data_len -= read;
	}

	return data - start;
}

size_t uci_message_skip(struct uci_message_parser *parser, size_t len)
{
	uint8_t *chunk = NULL;
	size_t read;
	size_t rem = len;

	while (uci_message_remaining(parser) > 0 && rem > 0) {
		read = uci_message_get_nocopy(parser, rem, &chunk);
		rem -= read;
	}

	return len - rem;
}

int uci_message_put_nocopy(struct uci_message_builder *builder, size_t size,
			   uint8_t **data)
{
	size_t remaining;

	if (!builder) {
		return UCI_STATUS_INVALID_PARAM;
	}

	if (builder->first_errno) {
		return -builder->first_errno;
	}

	if (!data || size == 0) {
		builder->first_errno = UCI_STATUS_INVALID_PARAM;
		return -builder->first_errno;
	}

	remaining = get_remaining(builder, size);
	if (remaining == 0) {
		builder->first_errno = UCI_STATUS_FAILED;
		return -builder->first_errno;
	}

	if (size > remaining) {
		size = remaining;
	}

	*data = builder->current_block->data + builder->current_block->len;
	builder->message->total_len += size;
	builder->current_block->len += size;
	return size;
}

void uci_message_put_blk(struct uci_message_builder *builder,
			 struct uci_blk *blk)
{
	if (!builder || !blk) {
		return;
	}

	/* is this block the first one ? */
	if (!builder->message) {
		builder->message = blk;
		builder->current_block = blk;
		return;
	}

	builder->current_block->next = blk;
	builder->current_block = blk;
	builder->message->total_len += blk->len;
}

size_t uci_message_get_nocopy(struct uci_message_parser *parser, size_t size,
			      uint8_t **data)
{
	const struct uci_blk *current_block;
	size_t blk_remaining;

	if (!parser || size == 0 || !data) {
		return 0;
	}

	current_block = parser->current_block;
	if (!current_block) {
		return 0;
	}

	/* no space left */
	if (parser->offset >= current_block->len) {
		parser->current_remaining_len -= current_block->len;
		parser->previous = current_block;
		parser->current_block = current_block->next;
		parser->offset = 0;
		current_block = parser->current_block;
	}

	if (!current_block) {
		return 0;
	}

	/* skip the header if needed */
	if (parser->offset == 0 && uci_blk_has_header(current_block)) {
		parser->offset = UCI_PACKET_HEADER_SIZE;
		parser->current_remaining_len += UCI_PACKET_HEADER_SIZE;
	}

	blk_remaining = current_block->len - parser->offset;
	if (size > blk_remaining) {
		size = blk_remaining;
	}

	*data = parser->current_block->data + parser->offset;
	parser->offset += size;
	return size;
}

enum uci_status_code uci_message_get_bytes(struct uci_message_parser *parser,
					   uint8_t *dst, uint8_t *length,
					   size_t capacity)
{
	size_t count;
	if (uci_message_remaining(parser) < 1) {
		return UCI_STATUS_SYNTAX_ERROR;
	}

	*length = uci_message_get_8bit(parser);

	if (*length > capacity || uci_message_remaining(parser) < *length) {
		return UCI_STATUS_SYNTAX_ERROR;
	}

	count = uci_message_get(parser, dst, *length);

	if (count != *length) {
		return UCI_STATUS_SYNTAX_ERROR;
	}

	return UCI_STATUS_OK;
}

void uci_message_put_bytes(struct uci_message_builder *builder,
			   const uint8_t *data, size_t length)
{
	if (length > 255) {
		builder->first_errno = UCI_STATUS_FAILED;
		return;
	}
	uci_message_put_8bit(builder, length);
	uci_message_put(builder, data, length);
}

enum uci_status_code uci_message_get_string(struct uci_message_parser *parser,
					    char *dst, size_t capacity)
{
	uint8_t length;
	enum uci_status_code ret = uci_message_get_bytes(parser, (uint8_t *)dst,
							 &length, capacity - 1);
	if (ret != 0) {
		length = 0;
	}
	dst[length] = '\0';
	if (dst[0] == '\0') {
		length = 0;
		ret = UCI_STATUS_SYNTAX_ERROR;
	}
	return ret;
}

void uci_message_put_string(struct uci_message_builder *builder,
			    const char *str, size_t maxlen)
{
	const char *end = memchr(str, '\0', maxlen);
	size_t length = end ? (size_t)(end - str) : maxlen;
	uci_message_put_bytes(builder, (const uint8_t *)str, length);
}

int uci_read_tlv(struct uci_message_parser *parser, uint8_t *type, uint8_t *len)
{
	if (uci_message_remaining(parser) < 2)
		return UCI_STATUS_SYNTAX_ERROR;
	*type = uci_message_get_8bit(parser);
	/* Check for extended type */
	if (*type >= 0xe0 && *type <= 0xe2) {
		if (uci_message_remaining(parser) < 2)
			return UCI_STATUS_SYNTAX_ERROR;
		uci_message_skip(parser, 1);
	}
	*len = uci_message_get_8bit(parser);
	if (uci_message_remaining(parser) < *len)
		return UCI_STATUS_SYNTAX_ERROR;

	if (*type >= 0xe0 && *type <= 0xe2) {
		return UCI_STATUS_INVALID_PARAM;
	}

	return UCI_STATUS_OK;
}
