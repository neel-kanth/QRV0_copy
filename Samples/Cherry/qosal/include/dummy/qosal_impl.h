/**
 * @file      qosal_impl.h
 *
 * @brief     Header file for qosal implementation
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#ifndef QOSAL_IMPL_H
#define QOSAL_IMPL_H

#define QOSAL_IMPL_THREAD_MAX_NAME_LEN 0

#define QOSAL_IMPL_THREAD_STACK_DEFINE(name, stack_size) static void *name = NULL

#define QOSAL_IMPL_LOG_INFO(...)
#define QOSAL_IMPL_LOG_ERR(...)
#define QOSAL_IMPL_LOG_WARN(...)
#define QOSAL_IMPL_LOG_DEBUG(...)

#define QOSAL_IMPL_IRQ_CONNECT(irqn, prio, handler)

/* The following functions are valid on all ARM Cortex-M. */
#define QOSAL_IMPL_IRQ_ENABLE(irqn) NVIC_EnableIRQ(irqn)
#define QOSAL_IMPL_IRQ_DISABLE(irqn) NVIC_DisableIRQ(irqn)
#define QOSAL_IMPL_IRQ_CLEAR_PENDING(irqn) NVIC_ClearPendingIRQ(irqn)
#define QOSAL_IMPL_IRQ_GET(irqn) NVIC_GetEnableIRQ(irqn)

#endif /* QOSAL_IMPL_H */
