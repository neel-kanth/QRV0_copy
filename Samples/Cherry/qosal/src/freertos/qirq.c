/*
 * Copyright (c) 2022-2024 Qorvo, Inc
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

#include "qirq.h"

#include "FreeRTOS.h"
#include "qprivate.h"
#include "task.h"

unsigned int qirq_lock(void)
{
	vPortEnterCritical();
	return 0;
}

void qirq_unlock(unsigned int key)
{
	vPortExitCritical();
}
