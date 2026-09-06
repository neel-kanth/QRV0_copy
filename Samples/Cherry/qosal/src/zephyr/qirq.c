/*
 * SPDX-FileCopyrightText: Copyright (c) 2022 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "qirq.h"

#include <zephyr/irq.h>

unsigned int qirq_lock(void)
{
	return irq_lock();
}

void qirq_unlock(unsigned int key)
{
	irq_unlock(key);
}
