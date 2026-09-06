/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define VECTOR_INC_STEP 8

struct uwbs_config_vector {
	void *arr;
	size_t size;
	size_t capacity;
};

bool uwbs_config_vector_reserve(struct uwbs_config_vector *v,
				const size_t elem_size, size_t new_capacity);

typedef void (*uwbs_config_vector_item_ctr)(void *);
typedef void (*uwbs_config_vector_item_dtr)(void *);

bool uwbs_config_vector_resize(struct uwbs_config_vector *v,
			       const size_t elem_size, size_t new_size,
			       uwbs_config_vector_item_ctr constructor);
void uwbs_config_vector_destructor(struct uwbs_config_vector *v,
				   const size_t elem_size,
				   uwbs_config_vector_item_dtr destructor);

#define VECTOR_INIT_STATIC \
	{                  \
		NULL, 0, 0 \
	}

#define VECTOR_INIT(v) ((typeof(v))VECTOR_INIT_STATIC)

#define VECTOR_DESTROY(v, destructor)                                    \
	uwbs_config_vector_destructor((struct uwbs_config_vector *)&(v), \
				      sizeof(*((v).arr)),                \
				      (uwbs_config_vector_item_dtr)destructor)

#define VECTOR_SIZE(v) ((v).size)
#define VECTOR_CAPACITY(v) ((v).capacity)
#define VECTOR_EMPTY(v) (VECTOR_SIZE(v) == 0)
#define VECTOR_FIRST(v) ((v).arr)
#define VECTOR_BEGIN(v) VECTOR_FIRST(v)
#define VECTOR_END(v) (VECTOR_FIRST(v) + VECTOR_SIZE(v))

#define VECTOR_RESERVE(v, new_capacity, ret)                                \
	ret = uwbs_config_vector_reserve((struct uwbs_config_vector *)&(v), \
					 sizeof(*((v).arr)), new_capacity)

#define VECTOR_RESIZE(v, new_size, constructor, ret)                   \
	ret = uwbs_config_vector_resize(                               \
		(struct uwbs_config_vector *)&(v), sizeof(*((v).arr)), \
		new_size, (uwbs_config_vector_item_ctr)constructor)

#define VECTOR_INC_SIZE(v, inc, constructor, ret)             \
	do {                                                  \
		size_t new_size = VECTOR_SIZE(v) + inc;       \
		VECTOR_RESIZE(v, new_size, constructor, ret); \
	} while (1 == 2)

#define VECTOR_FOREACH(v, cbk, ...)                        \
	do {                                               \
		size_t i;                                  \
		for (i = 0; i < VECTOR_SIZE(v); ++i) {     \
			(cbk)(&(v).arr[i], ##__VA_ARGS__); \
		}                                          \
	} while (' ' > ' ')

#define VECTOR_CLEAR(v, destructor)            \
	do {                                   \
		VECTOR_FOREACH(v, destructor); \
		VECTOR_SIZE(v) = 0;            \
	} while ('a' == '\0')

#define VECTOR_AT(v, idx) &(v).arr[idx]
#define VECTOR_BACK(v) VECTOR_AT(v, VECTOR_SIZE(v) - 1)

#define VECTOR(type, name)       \
	struct {                 \
		type *arr;       \
		size_t size;     \
		size_t capacity; \
	} name
