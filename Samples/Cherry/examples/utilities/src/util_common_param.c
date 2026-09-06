/*
 * Definition of global parameters for cherry examples.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <qlog.h>
#include <util_calib_qm357_sip.h>
#include <util_calib_qm35822.h>
#include <util_calib_qm35825.h>
#include <util_calib_soc.h>
#include <util_common_param.h>

const uint8_t static_sts_iv[CHERRY_STATIC_STS_SIZE] = { 0x01, 0x02, 0x03,
							0x04, 0x05, 0x06 };
const uint8_t vendor_id[CHERRY_VENDOR_ID_SIZE] = { 0x07, 0x08 };

enum device_chip get_device_chip(uint32_t device_id, uint8_t device_soi)
{
	if (device_id == DEVICE_ID_QM357) {
		return QM357;
	} else if (device_id == DEVICE_ID_QM358) {
		if (device_soi == SOI_VARIANT_QM35825)
			return QM35825;
		else if (device_soi == SOI_VARIANT_QM35822)
			return QM35822;
		/* Use this device chip as default if SOI_VARIANT missing. */
		else
			return QM358;
	}
	return NONE;
}

void choose_calib(const struct cherry_calib **calib, enum device_chip chip,
		  bool is_sip)
{
	if (is_sip) {
		switch (chip) {
		case QM358:
		case QM35822:
			*calib = &util_calib_qm35822;
			break;
		case QM35825:
			*calib = &util_calib_qm35825;
			break;
		case QM357:
			*calib = &util_calib_qm357_sip;
			break;
		default:
			QLOGW("Unkown chip type, using default SIP calibration");
			*calib = &util_calib_qm357_sip;
			break;
		}
	} else {
		*calib = &util_calib_soc;
	}
	QLOGD("Apply calibration for %s device and %s chip",
	      is_sip ? "SIP" : "SOC",
	      chip == QM35822 ? "QM35822" :
	      chip == QM35825 ? "QM35825" :
	      chip == QM357   ? "QM357" :
	      chip == QM358   ? "QM358" :
				"Unknown");
}
