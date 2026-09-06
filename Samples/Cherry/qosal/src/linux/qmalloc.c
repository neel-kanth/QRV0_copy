/**
 * @file      qmalloc.c
 *
 * @brief     Implementation for Qorvo dynamic allocation functions
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include "qmalloc.h"

#include <stdlib.h>

uint32_t allocation_quotas[] = {
	~0,
#ifdef CONFIG_MEM_QUOTA_ID1
	CONFIG_MEM_QUOTA_ID1,
#endif
#ifdef CONFIG_MEM_QUOTA_ID2
	CONFIG_MEM_QUOTA_ID2,
#endif
#ifdef CONFIG_MEM_QUOTA_ID3
	CONFIG_MEM_QUOTA_ID3,
#endif
#ifdef CONFIG_MEM_QUOTA_ID4
	CONFIG_MEM_QUOTA_ID4,
#endif
};

void *qmalloc_internal(size_t size)
{
	return malloc(size);
}

void *qrealloc_internal(void *ptr, size_t new_size)
{
	return realloc(ptr, new_size);
}

void qfree_internal(void *ptr)
{
	free(ptr);
}
