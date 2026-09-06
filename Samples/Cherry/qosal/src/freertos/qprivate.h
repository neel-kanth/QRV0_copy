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

#ifndef QPRIVATE_H
#define QPRIVATE_H

#include <stdbool.h>

/**
 * qprivate_is_in_isr() - Indicate if execution is in IRQ or not.
 *
 * Return: true if execution is in IRQ.
 */
bool qprivate_is_in_isr(void);

#endif /* QPRIVATE_H */
