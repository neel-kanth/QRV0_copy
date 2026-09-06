/*
 * Copyright (c) 2022 Qorvo, Inc
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

#include "qerr.h"

#include "FreeRTOS.h"

enum qerr qerr_convert_os_to_qerr(int error)
{
	return (error == pdFAIL) ? QERR_EIO : QERR_SUCCESS;
}

int qerr_convert_qerr_to_os(enum qerr error)
{
	return (error == QERR_SUCCESS) ? pdPASS : pdFAIL;
}
