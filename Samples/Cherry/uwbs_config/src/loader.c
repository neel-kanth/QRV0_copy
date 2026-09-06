/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "loader.h"

#include "uwbs_config/uwbs_config.h"

#include <assert.h>
#include <qtils.h>
#include <stdbool.h>
#include <yaml.h>

/* #define LOADER_DEBUG */

enum uwbs_config_status
uwbs_config_yaml_enter_node(struct uwbs_config_yaml_ctx *ctx,
			    uwbs_config_yaml_consume_event_t consume_event,
			    void *data)
{
	struct uwbs_config_yaml_node *n;

	if (ctx->node_stack_size == qarray_size(ctx->node_stack)) {
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
	}

	n = &ctx->node_stack[ctx->node_stack_size++];
	n->consume_event = consume_event;
	n->data = data;

	return UWBS_CONFIG_STATUS_OK;
}

void uwbs_config_yaml_leave_node(struct uwbs_config_yaml_ctx *ctx)
{
	assert(ctx->node_stack_size > 0);
	--ctx->node_stack_size;
}

enum uwbs_config_status
uwbs_config_yaml_consume_event(struct uwbs_config_yaml_ctx *ctx,
			       const yaml_event_t *event)
{
	struct uwbs_config_yaml_node *n;
	assert(ctx->node_stack_size > 0);
	if (ctx->node_stack_size == 0)
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

	n = &ctx->node_stack[ctx->node_stack_size - 1];
	return n->consume_event(ctx, event, n->data);
}

static enum uwbs_config_status
uwbs_config_yaml_skip_node_consume_event(struct uwbs_config_yaml_ctx *ctx,
					 const yaml_event_t *event, void *data)
{
	switch (event->type) {
	case YAML_SEQUENCE_START_EVENT:
	case YAML_MAPPING_START_EVENT:
		++ctx->skip_depth;
		break;
	case YAML_SEQUENCE_END_EVENT:
	case YAML_MAPPING_END_EVENT:
		if (--ctx->skip_depth == 0) {
			uwbs_config_yaml_leave_node(ctx);
		}
		break;
	default:
		break;
	}
	return UWBS_CONFIG_STATUS_OK;
}

enum uwbs_config_status
uwbs_config_yaml_skip_node(struct uwbs_config_yaml_ctx *ctx)
{
	ctx->skip_depth = 0;
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_yaml_skip_node_consume_event, NULL);
}

static enum uwbs_config_status uwbs_config_yaml_skip_scalar_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	switch (event->type) {
	case YAML_SCALAR_EVENT:
		uwbs_config_yaml_leave_node(ctx);
		break;
	default:
		break;
	}
	return UWBS_CONFIG_STATUS_OK;
}

enum uwbs_config_status
uwbs_config_yaml_skip_scalar(struct uwbs_config_yaml_ctx *ctx)
{
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_yaml_skip_scalar_consume_event, NULL);
}

static enum uwbs_config_status uwbs_config_yaml_string_node_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	char **str = data;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		*str = strdup((char *)event->data.scalar.value);
		uwbs_config_yaml_leave_node(ctx);
		return (*str ? UWBS_CONFIG_STATUS_OK :
			       UWBS_CONFIG_STATUS_INTERNAL_ERROR);
	default:
		break;
	}

	return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
}

enum uwbs_config_status
uwbs_config_yaml_enter_string_node(struct uwbs_config_yaml_ctx *ctx, char **str)
{
	if (*str) {
		/* Do not allow to use a pointer pointing a string. */
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
	}
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_yaml_string_node_consume_event, str);
}

enum uwbs_config_status
uwbs_config_yaml_uint8_node_consume_event(struct uwbs_config_yaml_ctx *ctx,
					  const yaml_event_t *event, void *data)
{
	uint32_t *n = data;
	const char *str;
	char *endptr;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		str = (char *)event->data.scalar.value;
		*n = strtoul(str, &endptr, 0);
		if (*n > 255)
			return UWBS_CONFIG_STATUS_INVALID_FMT;
		uwbs_config_yaml_leave_node(ctx);
		if (str != endptr && *endptr == '\0')
			return UWBS_CONFIG_STATUS_OK;
		break;
	default:
		break;
	}
	return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
}

enum uwbs_config_status
uwbs_config_yaml_enter_uint8_node(struct uwbs_config_yaml_ctx *ctx, uint8_t *n)
{
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_yaml_uint8_node_consume_event, n);
}

enum uwbs_config_status uwbs_config_yaml_uint32_node_consume_event(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event, void *data)
{
	uint32_t *n = data;
	const char *str;
	char *endptr;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		str = (char *)event->data.scalar.value;
		*n = strtoul(str, &endptr, 0);
		uwbs_config_yaml_leave_node(ctx);
		if (str != endptr && *endptr == '\0')
			return UWBS_CONFIG_STATUS_OK;
		break;
	default:
		break;
	}
	return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
}

enum uwbs_config_status
uwbs_config_yaml_enter_uint32_node(struct uwbs_config_yaml_ctx *ctx,
				   uint32_t *n)
{
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_yaml_uint32_node_consume_event, n);
}

static enum uwbs_config_status
uwbs_config_yaml_int64_node_consume_event(struct uwbs_config_yaml_ctx *ctx,
					  const yaml_event_t *event, void *data)
{
	int64_t *n = data;
	const char *str;
	char *endptr;

	switch (event->type) {
	case YAML_SCALAR_EVENT:
		str = (char *)event->data.scalar.value;
		*n = strtoll(str, &endptr, 0);
		uwbs_config_yaml_leave_node(ctx);
		if (str != endptr && *endptr == '\0')
			return UWBS_CONFIG_STATUS_OK;
		break;
	default:
		break;
	}
	return UWBS_CONFIG_STATUS_INTERNAL_ERROR;
}

enum uwbs_config_status
uwbs_config_yaml_enter_int64_node(struct uwbs_config_yaml_ctx *ctx, int64_t *n)
{
	return uwbs_config_yaml_enter_node(
		ctx, uwbs_config_yaml_int64_node_consume_event, n);
}

static enum uwbs_config_status
uwbs_config_yaml_stream_consume_event(struct uwbs_config_yaml_ctx *ctx,
				      const yaml_event_t *event, void *root)
{
	enum uwbs_config_status st = UWBS_CONFIG_STATUS_OK;
	switch (event->type) {
	case YAML_STREAM_START_EVENT:
		break;
	case YAML_STREAM_END_TOKEN:
		uwbs_config_yaml_leave_node(ctx);
		break;
	case YAML_DOCUMENT_START_EVENT:
		st = uwbs_config_yaml_enter_node(ctx, ctx->root_consume_event,
						 root);
		break;
	case YAML_DOCUMENT_END_EVENT:
		break;
	default:
		st = UWBS_CONFIG_STATUS_INTERNAL_ERROR;
		break;
	}
	return st;
}

#ifdef LOADER_DEBUG
static const char *event2str(enum yaml_event_type_e t)
{
	switch (t) {
	case YAML_NO_EVENT:
		return "No event";
	case YAML_STREAM_START_TOKEN:
		return "Stream start";
	case YAML_STREAM_END_EVENT:
		return "Stream end";
	case YAML_DOCUMENT_START_EVENT:
		return "Doc start";
	case YAML_DOCUMENT_END_EVENT:
		return "Doc end";
	case YAML_ALIAS_EVENT:
		return "Alias";
	case YAML_SCALAR_EVENT:
		return "Scalar";
	case YAML_SEQUENCE_START_EVENT:
		return "Seq start";
	case YAML_SEQUENCE_END_EVENT:
		return "Seq end";
	case YAML_MAPPING_START_EVENT:
		return "Map start";
	case YAML_MAPPING_END_EVENT:
		return "Map end";
	}
	return "UNKNOWN";
};
#endif

enum uwbs_config_status uwbs_config_yaml_load_loop(
	yaml_parser_t *parser, struct uwbs_config_yaml_ctx *ctx,
	uwbs_config_yaml_consume_event_t root_consume_event, void *root)
{
	enum uwbs_config_status st = UWBS_CONFIG_STATUS_OK;
	bool ret;

	ctx->root_consume_event = root_consume_event;
	uwbs_config_yaml_enter_node(ctx, uwbs_config_yaml_stream_consume_event,
				    root);

	while (st == UWBS_CONFIG_STATUS_OK) {
		yaml_event_t event;

		ret = yaml_parser_parse(parser, &event);
		if (!ret) {
			printf("Failed to parse YAML (line: %zu, column: %zu): %s\n",
			       parser->problem_mark.line + 1,
			       parser->problem_mark.column + 1,
			       parser->problem);
			return UWBS_CONFIG_STATUS_INVALID_FMT;
		}

		/* Parsing done. */
		if (event.type == YAML_NO_EVENT)
			break;

#ifdef LOADER_DEBUG
		printf("[IN ] yaml event type %s, scalar: %s\n",
		       event2str(event.type),
		       event.type == YAML_SCALAR_EVENT ?
			       (char *)event.data.scalar.value :
			       "");
#endif

		st = uwbs_config_yaml_consume_event(ctx, &event);

#ifdef LOADER_DEBUG
		printf("[OUT] yaml event type %s st = %d\n",
		       event2str(event.type), st);
#endif

		if (st != UWBS_CONFIG_STATUS_OK) {
			printf("Failed to interpret YAML (line: %zu, column: %zu)\n",
			       event.start_mark.line + 1,
			       event.start_mark.column + 1);
		}

		yaml_event_delete(&event);
	};

	return st;
}

enum uwbs_config_status uwbs_config_yaml_load_str(
	const char *str, size_t size, struct uwbs_config_yaml_ctx *ctx,
	uwbs_config_yaml_consume_event_t root_consume_event, void *root)
{
	yaml_parser_t parser;
	enum uwbs_config_status st = UWBS_CONFIG_STATUS_OK;

	if (!yaml_parser_initialize(&parser))
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

	yaml_parser_set_input_string(&parser, (const unsigned char *)str, size);

	st = uwbs_config_yaml_load_loop(&parser, ctx, root_consume_event, root);

	yaml_parser_delete(&parser);
	return st;
}

enum uwbs_config_status
uwbs_config_yaml_load_file(const char *path, struct uwbs_config_yaml_ctx *ctx,
			   uwbs_config_yaml_consume_event_t root_consume_event,
			   void *root)
{
	yaml_parser_t parser;
	FILE *input;
	enum uwbs_config_status ret = UWBS_CONFIG_STATUS_OK;

	if (!yaml_parser_initialize(&parser))
		return UWBS_CONFIG_STATUS_INTERNAL_ERROR;

	input = fopen(path, "rb");
	if (!input) {
		yaml_parser_delete(&parser);
		return UWBS_CONFIG_STATUS_INVALID_ARG;
	}

	yaml_parser_set_input_file(&parser, input);

	ret = uwbs_config_yaml_load_loop(&parser, ctx, root_consume_event,
					 root);

	yaml_parser_delete(&parser);
	fclose(input);
	return ret;
}
