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

#pragma once

/* TODO: Only used for profiling and not implemented for FreeRTOS.
 * Disabled for now to avoid contamination with included headers
 * on FreeRtos builds.
 */
#define QOSAL_IMPL_THREAD_MAX_NAME_LEN 0

#define QOSAL_IMPL_THREAD_STACK_DEFINE(name, stack_size) static uint8_t name[stack_size]

#ifdef CONFIG_LOG
#include <qlog_impl.h>
#else
#define QOSAL_IMPL_LOG_INFO(...)
#define QOSAL_IMPL_LOG_ERR(...)
#define QOSAL_IMPL_LOG_WARN(...)
#define QOSAL_IMPL_LOG_DEBUG(...)
#endif

#define QOSAL_IMPL_IRQ_CONNECT(irqn, prio, handler)

/* The following functions are valid on all ARM Cortex-M. */
#define QOSAL_IMPL_IRQ_ENABLE(irqn) NVIC_EnableIRQ(irqn)
#define QOSAL_IMPL_IRQ_DISABLE(irqn) NVIC_DisableIRQ(irqn)
#define QOSAL_IMPL_IRQ_CLEAR_PENDING(irqn) NVIC_ClearPendingIRQ(irqn)
#define QOSAL_IMPL_IRQ_GET(irqn) NVIC_GetEnableIRQ(irqn)
