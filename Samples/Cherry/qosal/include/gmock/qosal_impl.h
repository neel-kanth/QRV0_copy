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

#ifndef QOSAL_IMPL_H
#define QOSAL_IMPL_H

#define QOSAL_IMPL_THREAD_MAX_NAME_LEN 16

#define QOSAL_IMPL_THREAD_STACK_DEFINE(name, stack_size) static void *name = NULL

#define QOSAL_IMPL_LOG_INFO(...)
#define QOSAL_IMPL_LOG_ERR(...)
#define QOSAL_IMPL_LOG_WARN(...)
#define QOSAL_IMPL_LOG_DEBUG(...)

#define QOSAL_IMPL_IRQ_CONNECT(irqn, prio, handler)

/* The following functions are valid on all ARM Cortex-M. */
#define QOSAL_IMPL_IRQ_ENABLE(irqn)
#define QOSAL_IMPL_IRQ_DISABLE(irqn)
#define QOSAL_IMPL_IRQ_CLEAR_PENDING(irqn)
#define QOSAL_IMPL_IRQ_GET(irqn) 0

#define QOSAL_PRINT_TRACE(...)

#endif /* QOSAL_IMPL_H */
