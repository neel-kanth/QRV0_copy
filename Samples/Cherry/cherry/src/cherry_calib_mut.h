/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <cherry/cherry.h>
#include <qmalloc.h>
#include <string.h>

struct cherry_calib_mut_allocation {
	struct cherry_calib_mut_allocation *next;
	char data[];
};

struct cherry_calib_mut {
	/**
	 * @allocations: dumb allocation references.
	 */
	struct cherry_calib_mut_allocation *allocation_list;
	/**
	 * @mut_keys: mutable keys array. Array size is available in calib.
	 */
	struct cherry_calib_key *mut_keys;
	/**
	 * @mut_keys_capacity: Store allocated mut_keys array size.
	 */
	size_t mut_keys_capacity;
	/**
	 * @calib: calibration output. All pointers in this member are valid
	 * when context open is done and until context is closed.
	 */
	struct cherry_calib calib;
};

#define CHERRY_CALIB_DYN_INIT_STATIC \
	(struct cherry_calib_mut)    \
	{                            \
		0                    \
	}

void cherry_calib_mut_destroy(struct cherry_calib_mut *calib_mut);

void *cherry_calib_mut_alloc(struct cherry_calib_mut *calib_mut, size_t size);

static inline void *cherry_calib_mut_memdup(struct cherry_calib_mut *calib_mut,
					    const void *in, const size_t size)
{
	void *out = cherry_calib_mut_alloc(calib_mut, size);
	if (out)
		memcpy(out, in, size);
	return out;
}

static inline char *cherry_calib_mut_strdup(struct cherry_calib_mut *calib_mut,
					    const char *in)
{
	const size_t len = strlen(in) + 1;
	return (char *)cherry_calib_mut_memdup(calib_mut, (void *)in, len);
}
