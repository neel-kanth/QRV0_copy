/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef UCI_UNIT_CONVERTOR_H
#define UCI_UNIT_CONVERTOR_H

#include <limits.h>
#include <math.h>

static inline uint16_t uci_convert_aoa_2pi_q16_to_deg_q7(int32_t aoa_2pi_q16)
{
	/* Store into 32 bits to avoid overflow during computation. */
	int32_t aoa_deg_q7 = aoa_2pi_q16;

	/* Round(aoa_2pi_q16 * 360 / (1 << 9)). */
	aoa_deg_q7 *= 360;
	if (aoa_2pi_q16 > 0)
		aoa_deg_q7 += ((1 << 9) / 2);
	else
		aoa_deg_q7 -= ((1 << 9) / 2);
	aoa_deg_q7 /= (1 << 9);

	return (0xFFFF & aoa_deg_q7);
}

static inline int32_t uci_convert_aoa_deg_q7_to_2pi_q16(int16_t aoa_deg_q7)
{
	int32_t aoa_2pi_q16 = (int32_t)aoa_deg_q7;

	/* Round(aoa_deg_q7 * (1 << 9) / 360). */
	aoa_2pi_q16 /= 360;
	if (aoa_deg_q7 > 0)
		aoa_2pi_q16 += (2 / (1 << 9));
	else
		aoa_2pi_q16 -= (2 / (1 << 9));
	aoa_2pi_q16 *= (1 << 9);

	return aoa_2pi_q16;
}

static inline int16_t uci_convert_aoa_pi_q16_to_deg_q7(int32_t aoa_pi_q16)
{
	return aoa_pi_q16 * 180 / (1 << 9);
}

static inline int32_t uci_convert_deg_q7_to_2pi(int16_t aoa_deg_q7)
{
	/* Round(aoa_deg_q7 * (1 << 9) / 360). */
	int32_t aoa_2pi = (int32_t)aoa_deg_q7 * (1 << 9);

	if (aoa_deg_q7 > 0)
		aoa_2pi += 180;
	else
		aoa_2pi -= 180;
	aoa_2pi /= 360;

	return aoa_2pi;
}

static inline int32_t uci_convert_aoa_deg_q7_to_pi_q16(int16_t aoa_deg_q7)
{
	return aoa_deg_q7 * (1 << 9) / 180;
}

static inline int64_t float_to_q(float float_value, int8_t fractionnal_bits)
{
	int64_t q_value;

	float_value *= 1 << fractionnal_bits;

	if (float_value < (float)LONG_MIN || float_value >= (float)LONG_MAX) {
		return -1;
	}

	// +-0.5 for rounding
	q_value = (int64_t)(float_value + (float_value > 0 ? 0.5 : -0.5));

	return q_value;
}

static inline float q_to_float(uint8_t int_value, uint8_t fractionnal_bits)
{
	float float_value;

	float_value = (float)(int_value) / (float)(1 << fractionnal_bits);

	return float_value;
}
#endif // UCI_UNIT_CONVERTOR_H
