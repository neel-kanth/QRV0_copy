/*
 * Public header for definition of common parameters in cherry examples.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry_fira.h>

/* Device ID Const */
#define DEVICE_ID_QM357 0xdeca0430
#define DEVICE_ID_QM358 0xdeca0440

#define SOI_VARIANT_QM35825 1
#define SOI_VARIANT_QM35822 3

/* Constant parameters */
extern const uint8_t static_sts_iv[CHERRY_STATIC_STS_SIZE];
extern const uint8_t vendor_id[CHERRY_VENDOR_ID_SIZE];

enum device_chip {
	QM357,
	QM358,
	QM35825,
	QM35822,
	NONE,
};

enum device_chip get_device_chip(uint32_t device_id, uint8_t device_soi);

void choose_calib(const struct cherry_calib **calib, enum device_chip chip,
		  bool is_sip);
