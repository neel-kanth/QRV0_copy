/*
 * Copyright (c) 2024 Qorvo, Inc
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

/* IWYU pragma: private, include "qatomic.h" */

#ifndef QATOMIC_IMPL_H
#define QATOMIC_IMPL_H

#ifdef __cplusplus
#include <atomic>
using namespace std;
#else
#include <stdatomic.h>
#endif

#define QATOMIC

#define qatomic_int atomic_int
#define qatomic_bool atomic_bool

#define qatomic_init(x, value) atomic_init(x, value)
#define qatomic_load(x) atomic_load(x)
#define qatomic_store(x, value) atomic_store(x, value)
#define qatomic_exchange(x, value) atomic_exchange(x, value)
#define qatomic_fetch_add(x, value) atomic_fetch_add(x, value)
#define qatomic_fetch_sub(x, value) atomic_fetch_sub(x, value)

/* No specific implementation. */

#endif /* QATOMIC_IMPL_H */
