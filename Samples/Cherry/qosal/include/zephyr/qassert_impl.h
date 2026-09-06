/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */
#pragma once

#ifndef QASSERT
#include <zephyr/kernel.h>

#define QASSERT(cond)              \
	do {                       \
		if (!(cond)) {     \
			k_panic(); \
		}                  \
	} while (0)
#endif /* QASSERT */
