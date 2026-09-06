/*
 * Public header for utilities for converting different units.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once
#include <cherry/cherry.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef CONFIG_CHERRY_EXAMPLES_UTILITIES_ZEPHYR
#include <termios.h>
#endif

/**
 * util_convert_x_to_int() - Convert x to int.
 * @x: Value to convert.
 *
 * Return: Result of the conversion.
 */
int32_t util_convert_x_to_int(int32_t x);

/**
 * util_convert_y_to_int() - Convert y to int.
 * @x: Value to convert.
 *
 * Return: Result of the conversion.
 */
int32_t util_convert_y_to_int(int32_t y);

/**
 * util_convert_z_to_int() - Convert z to int.
 * @x: Value to convert.
 *
 * Return: Result of the conversion.
 */
int32_t util_convert_z_to_int(int32_t z);

/**
 * util_convert_latitude_to_float() - Convert latitude to float.
 * @latitude: Value to convert.
 *
 * Return: Result of the conversion.
 */
double util_convert_latitude_to_float(uint64_t latitude);

/**
 * util_convert_longitude_to_float() - Convert longitude to float.
 * @longitude: Value to convert.
 *
 * Return: Result of the conversion.
 */
double util_convert_longitude_to_float(uint64_t longitude);

/**
 * util_convert_altitude_to_float() - Convert altitude to float.
 * @altitude: Value to convert.
 *
 * Return: Result of the conversion.
 */
double util_convert_altitude_to_float(uint32_t altitude);

/**
 * util_convert_aoa_q_to_float() - Convert Q notation to float for aoa.
 * @aoa_q7: Value to convert of 16bits.
 *
 * Converts fixed point number in Q notation to floating point number.
 *
 * Return: Result of the conversion.
 */
float util_convert_aoa_q_to_float(int16_t aoa_q7);

/**
 * util_convert_rssi_q_to_float() - Convert Q notation to float.
 * @int_value: Value to convert of 8bits.
 *
 * Converts fixed point number in Q notation to floating point number.
 *
 * Return: Result of the conversion.
 */
float util_convert_rssi_q_to_float(uint8_t int_value);

/**
 * util_convert_aoa_2pi_to_deg() - Convert 2pi range in Q9.7 to degree.
 * @aoa_2pi_q16: Value to convert.
 *
 * Return: Result of the conversion.
 */
int16_t util_convert_aoa_2pi_to_deg(int32_t aoa_2pi_q16);

/**
 * util_convert_aoa_pi_to_deg() - Convert pi range in Q9.7 to degree.
 * @aoa_pi_q16: Value to convert.
 *
 * Return: Result of the conversion.
 */
int16_t util_convert_aoa_pi_to_deg(int32_t aoa_pi_q16);

/**
 * util_convert_cfo_to_float() - Convert cfo to float.
 * @cfo_q16: Value to convert.
 *
 * Return: Result of the conversion.
 */
float util_convert_cfo_to_float(int16_t cfo_q16);

/**
 * util_convert_arg_to_uint64() - Convert string option argument to uint64_t.
 * @str: Str to convert.
 * @out: Pointer to the output value.
 *
 * Return: True if succesful conversion else false.
 */
bool util_convert_arg_to_uint64(const char *str, uint64_t *out);

/**
 * util_convert_arg_to_uint32() - Convert string option argument to uint32_t.
 * @str: Str to convert.
 * @out: Pointer to the output value.
 *
 * Return: True if succesful conversion else false.
 */
bool util_convert_arg_to_uint32(const char *str, uint32_t *out);

/**
 * util_convert_arg_to_int32() - Convert string option argument to int32_t.
 * @str: Str to convert.
 * @out: Pointer to the output value.
 *
 * Return: True if succesful conversion else false.
 */
bool util_convert_arg_to_int32(const char *str, int32_t *out);

/**
 * util_convert_arg_to_uint16() - Convert string option argument to uint16_t.
 * @str: Str to convert.
 * @out: Pointer to the output value.
 *
 * Return: True if succesful conversion else false.
 */
bool util_convert_arg_to_uint16(const char *str, uint16_t *out);

/**
 * util_convert_arg_to_int16() - Convert string option argument to int16_t.
 * @str: Str to convert.
 * @out: Pointer to the output value.
 *
 * Return: True if succesful conversion else false.
 */
bool util_convert_arg_to_int16(const char *str, int16_t *out);

/**
 * util_convert_arg_to_uint8() - Convert string option argument to uint8_t.
 * @str: Str to convert.
 * @out: Pointer to the output value.
 *
 * Return: True if succesful conversion else false.
 */
bool util_convert_arg_to_uint8(const char *str, uint8_t *out);

#ifndef CONFIG_CHERRY_EXAMPLES_UTILITIES_ZEPHYR
/**
 * util_convert_long_to_speed_t() - Convert int to speed_t.
 * @value: Value to convert.
 *
 * Return: Result of the conversion.
 */
speed_t util_conver_long_to_speed_t(long value);
#endif

/**
 * util_convert_arg_to_log_level() - Convert string option argument to enum cherry_log_level.
 * @str: Str to convert.
 * @out: Pointer to the output value.
 *
 * Return: True if succesful conversion else false.
 */
bool util_convert_arg_to_log_level(const char *str, enum cherry_log_level *out);
