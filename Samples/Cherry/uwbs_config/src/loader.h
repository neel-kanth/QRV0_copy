/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include "uwbs_config/uwbs_config.h"

#include <stdint.h>
#include <yaml.h>

#define NODE_STACK_SIZE 10

struct uwbs_config_yaml_ctx;
typedef enum uwbs_config_status (*uwbs_config_yaml_consume_event_t)(
	struct uwbs_config_yaml_ctx *ctx, const yaml_event_t *event,
	void *data);

struct uwbs_config_yaml_node {
	uwbs_config_yaml_consume_event_t consume_event;
	void *data;
};

struct uwbs_config_yaml_ctx {
	struct uwbs_config_yaml_node node_stack[NODE_STACK_SIZE];
	size_t node_stack_size;
	size_t skip_depth;
	uwbs_config_yaml_consume_event_t root_consume_event;
};

static inline void uwbs_config_yaml_init(struct uwbs_config_yaml_ctx *ctx)
{
	*ctx = (struct uwbs_config_yaml_ctx){ 0 };
}

static inline void uwbs_config_yaml_destroy(struct uwbs_config_yaml_ctx *ctx)
{
}

enum uwbs_config_status
uwbs_config_yaml_enter_node(struct uwbs_config_yaml_ctx *ctx,
			    uwbs_config_yaml_consume_event_t consume_event,
			    void *data);

void uwbs_config_yaml_leave_node(struct uwbs_config_yaml_ctx *ctx);
enum uwbs_config_status
uwbs_config_yaml_consume_event(struct uwbs_config_yaml_ctx *ctx,
			       const yaml_event_t *event);
enum uwbs_config_status
uwbs_config_yaml_skip_node(struct uwbs_config_yaml_ctx *ctx);
enum uwbs_config_status
uwbs_config_yaml_skip_scalar(struct uwbs_config_yaml_ctx *ctx);
enum uwbs_config_status
uwbs_config_yaml_enter_string_node(struct uwbs_config_yaml_ctx *ctx,
				   char **str);
enum uwbs_config_status
uwbs_config_yaml_enter_uint8_node(struct uwbs_config_yaml_ctx *ctx, uint8_t *n);
enum uwbs_config_status
uwbs_config_yaml_enter_uint32_node(struct uwbs_config_yaml_ctx *ctx,
				   uint32_t *n);
enum uwbs_config_status
uwbs_config_yaml_enter_int64_node(struct uwbs_config_yaml_ctx *ctx, int64_t *n);

enum uwbs_config_status uwbs_config_yaml_load_str(
	const char *str, size_t size, struct uwbs_config_yaml_ctx *ctx,
	uwbs_config_yaml_consume_event_t root_consume_event, void *root);
enum uwbs_config_status
uwbs_config_yaml_load_file(const char *path, struct uwbs_config_yaml_ctx *ctx,
			   uwbs_config_yaml_consume_event_t root_consume_event,
			   void *root);
