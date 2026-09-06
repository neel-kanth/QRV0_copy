/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "qrand.h"

#include <zephyr/random/random.h>

void qrand_seed(uint32_t seed)
{
	/* no implementation */
}

uint32_t qrand_rand(void)
{
	return sys_rand32_get();
}
