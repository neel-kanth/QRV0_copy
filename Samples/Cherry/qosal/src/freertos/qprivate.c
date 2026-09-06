/*
 * Copyright (c) 2023 Qorvo, Inc
 *
 * All rights reserved.
 *
 * NOTICE: All information contained herein is, and remains the property
 * of Qorvo, Inc. and its suppliers, if any. The intellectual and technical
 * concepts herein are proprietary to Qorvo, Inc. and its suppliers, and
 * may be covered by patents, patent applications, and are protected by
 * trade secret and/or copyright law. Dissemination of this information
 * or reproduction of this material is strictly forbidden unless prior written
 * permission is obtained from Qorvo, Inc.
 *
 */

#include "FreeRTOS.h"
#include "qirq.h"
#include "task.h"

#include <stdbool.h>

bool qprivate_is_in_isr()
{
	uint32_t ipsr_register;

	/* Read IPSR register. */
	__asm__ volatile("MRS %0, ipsr" : "=r"(ipsr_register));

	return !!ipsr_register;
}
