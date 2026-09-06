/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_calib_mut.h"

void cherry_calib_mut_destroy(struct cherry_calib_mut *calib_mut)
{
	struct cherry_calib_mut_allocation *alloc_iter;
	struct cherry_calib_mut_allocation *alloc_next;

	for (alloc_iter = calib_mut->allocation_list,
	    alloc_next = alloc_iter ? alloc_iter->next : NULL;
	     alloc_iter; alloc_iter = alloc_next,
	    alloc_next = alloc_iter != NULL ? alloc_iter->next : NULL) {
		qfree(alloc_iter);
	}

	qfree(calib_mut->mut_keys);
}

void *cherry_calib_mut_alloc(struct cherry_calib_mut *calib_mut, size_t size)
{
	struct cherry_calib_mut_allocation *alloc =
		qmalloc(size + sizeof(struct cherry_calib_mut_allocation));
	if (!alloc)
		return NULL;
	alloc->next = calib_mut->allocation_list;
	calib_mut->allocation_list = alloc;
	return alloc->data;
}
