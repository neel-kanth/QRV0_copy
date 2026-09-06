/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "vector.h"

#include <stdlib.h>

bool uwbs_config_vector_reserve(struct uwbs_config_vector *v,
				const size_t elem_size, size_t new_capacity)
{
	void *tmp;

	if (new_capacity <= v->capacity)
		return true;

	new_capacity =
		((new_capacity + VECTOR_INC_STEP - 1) / VECTOR_INC_STEP) *
		VECTOR_INC_STEP;
	tmp = realloc(v->arr, elem_size * new_capacity);
	if (!tmp)
		return false;

	v->arr = tmp;
	v->capacity = new_capacity;
	return true;
}

bool uwbs_config_vector_resize(struct uwbs_config_vector *v,
			       const size_t elem_size, size_t new_size,
			       uwbs_config_vector_item_ctr constructor)
{
	bool ret;
	char *iter;
	char *last;
	ret = uwbs_config_vector_reserve(v, elem_size, new_size);
	if (!ret)
		return ret;

	for (iter = (char *)(v->arr) + v->size * elem_size,
	    last = (char *)(v->arr) + new_size * elem_size;
	     iter < last; iter += elem_size) {
		(constructor)(iter);
	}
	v->size = new_size;
	return true;
}

static void uwbs_config_vector_clear(struct uwbs_config_vector *v,
				     const size_t elem_size,
				     uwbs_config_vector_item_dtr destructor)
{
	char *iter;
	char *last;
	for (iter = v->arr, last = (char *)v->arr + elem_size * v->size;
	     iter < last; iter += elem_size)
		destructor(iter);
	v->size = 0;
}

void uwbs_config_vector_destructor(struct uwbs_config_vector *v,
				   const size_t elem_size,
				   uwbs_config_vector_item_dtr destructor)
{
	uwbs_config_vector_clear(v, elem_size, destructor);
	free(v->arr);
	v->arr = NULL;
	v->capacity = 0;
}
