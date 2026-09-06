/*
 * Utilities for converting different units.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "util_convert.h"

static int32_t q28_to_int(int32_t q28_value)
{
	int32_t val = q28_value;
	int32_t sign = val & 0x8000000;

	/* Q28 format. */
	if (sign) {
		val |= 0xF0000000;
		val = ~val + 1;
		val = -val;
	}
	return val;
}

static int32_t q24_to_int(int32_t q24_value)
{
	int32_t val = q24_value;
	int32_t sign = val & 0x800000;

	/* Q24 format. */
	if (sign) {
		val |= 0xFF000000;
		val = ~val + 1;
		val = -val;
	}
	return val;
}

static double q9_24_to_float(const int64_t q9_24_value)
{
	/* Check if the number is negative. */
	int64_t value = q9_24_value;
	int64_t is_negative = value & 0x100000000;
	double result;

	/* If negative, convert to positive using two's complement. */
	if (is_negative) {
		value |= 0xFFFFFFFE00000000;
		value = ~value + 1;
	}
	/* Convert to floating-point. */
	result = value / (double)(1 << 24);
	/* Apply the sign. */
	if (is_negative) {
		result = -result;
	}
	return result;
}

static double q9_21_to_float(const int32_t q9_21_value)
{
	/* Check if the number is negative. */
	int64_t value = q9_21_value;
	int64_t is_negative = value & 0x20000000;
	double result;

	/* If negative, convert to positive using two's complement. */
	if (is_negative) {
		value |= 0xFFFFFFFFC0000000;
		value = ~value + 1;
	}
	/* Convert to floating-point. */
	result = value / (double)(1 << 21);
	/* Apply the sign. */
	if (is_negative) {
		result = -result;
	}
	return result;
}

int32_t util_convert_x_to_int(int32_t x)
{
	return q28_to_int(x);
}

int32_t util_convert_y_to_int(int32_t y)
{
	return q28_to_int(y);
}

int32_t util_convert_z_to_int(int32_t z)
{
	return q24_to_int(z);
}

double util_convert_latitude_to_float(uint64_t latitude)
{
	return (q9_24_to_float(latitude));
}

double util_convert_longitude_to_float(uint64_t longitude)
{
	return (q9_24_to_float(longitude));
}

double util_convert_altitude_to_float(uint32_t altitude)
{
	return (q9_21_to_float(altitude));
}

float util_convert_aoa_q_to_float(int16_t aoa_q7)
{
	return ((float)aoa_q7 / (float)(1 << 7));
}

float util_convert_rssi_q_to_float(uint8_t int_value)
{
	return (float)(int_value) / (float)(1 << 1);
}

/* AoA azimuth is 2pi range encoded as Q9.7, we convert it in degree. */
int16_t util_convert_aoa_2pi_to_deg(int32_t aoa_2pi_q16)
{
	/* Store into 32 bits to avoid overflow during computation. */
	int32_t aoa_deg_q7 = aoa_2pi_q16;
	int32_t sign;
	/* Round(aoa_2pi_q16 * 360 / (1 << 9)). */
	aoa_deg_q7 *= 360;
	if (aoa_2pi_q16 > 0)
		aoa_deg_q7 += ((1 << 9) / 2);
	else
		aoa_deg_q7 -= ((1 << 9) / 2);
	aoa_deg_q7 /= (1 << 9);

	sign = aoa_deg_q7 & 0x8000;
	aoa_deg_q7 = 0x7FFF & (aoa_deg_q7 >> 7);
	aoa_deg_q7 |= sign;
	return (0xFFFF & aoa_deg_q7);
}

/* AoA elevation is pi range encoded as Q9.7, we convert it in degree. */
int16_t util_convert_aoa_pi_to_deg(int32_t aoa_pi_q16)
{
	int16_t aoa_deg_q7 = aoa_pi_q16 * 180 / (1 << 9);
	int32_t sign;

	sign = aoa_deg_q7 & 0x8000;
	aoa_deg_q7 = 0x7FFF & (aoa_deg_q7 >> 7);
	aoa_deg_q7 |= sign;
	return (0xFFFF & aoa_deg_q7);
}

/* Cfo is encoded as Q6.10, we convert it. */
float util_convert_cfo_to_float(int16_t cfo_q16)
{
	return ((float)cfo_q16 / (float)(1 << 10));
}

bool util_convert_arg_to_uint64(const char *str, uint64_t *out)
{
	char *endptr;
	unsigned long long int value = strtoull(str, &endptr, 0);

	if (*endptr != '\0') {
		return false;
	}
	*out = (uint64_t)value;
	return true;
}

bool util_convert_arg_to_uint32(const char *str, uint32_t *out)
{
	char *endptr;
	unsigned long int value = strtoul(str, &endptr, 0);

	if (*endptr != '\0' || value > 0xFFFFFFFF) {
		return false;
	}
	*out = (uint32_t)value;
	return true;
}

bool util_convert_arg_to_int32(const char *str, int32_t *out)
{
	char *endptr;
	long int value = strtoul(str, &endptr, 0);

	if (*endptr != '\0' || value > 0xFFFFFFFF) {
		return false;
	}
	*out = (int32_t)value;
	return true;
}

bool util_convert_arg_to_uint16(const char *str, uint16_t *out)
{
	char *endptr;
	unsigned long int value = strtoul(str, &endptr, 0);

	if (*endptr != '\0' || value > 0xFFFF) {
		return false;
	}
	*out = (uint16_t)value;
	return true;
}

bool util_convert_arg_to_int16(const char *str, int16_t *out)
{
	char *endptr;
	long int value = strtoul(str, &endptr, 0);

	if (*endptr != '\0' || value > 0xFFFF) {
		return false;
	}
	*out = (int16_t)value;
	return true;
}

bool util_convert_arg_to_uint8(const char *str, uint8_t *out)
{
	char *endptr;
	unsigned long int value = strtoul(str, &endptr, 0);

	if (*endptr != '\0' || value > 0xFF) {
		return false;
	}
	*out = (uint8_t)value;
	return true;
}

#ifndef CONFIG_CHERRY_EXAMPLES_UTILITIES_ZEPHYR
speed_t util_conver_long_to_speed_t(long value)
{
	switch (value) {
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 500000:
		return B500000;
	case 576000:
		return B576000;
	case 921600:
		return B921600;
	case 1000000:
		return B1000000;
	case 1152000:
		return B1152000;
	case 1500000:
		return B1500000;
	case 2000000:
		return B2000000;
	case 2500000:
		return B2500000;
	case 3000000:
		return B3000000;
	case 3500000:
		return B3500000;
	case 4000000:
		return B4000000;
	default:
		return B0;
	}
}
#endif

bool util_convert_arg_to_log_level(const char *str, enum cherry_log_level *out)
{
	char *endptr;
	unsigned long int value = strtoul(str, &endptr, 0);

	if (*endptr != '\0' || value > CHERRY_LOG_LEVEL_DEBUG) {
		return false;
	}
	*out = (enum cherry_log_level)value;
	return true;
}
