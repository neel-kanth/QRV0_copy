/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <stddef.h>

/**
 * enum uwbs_config_status - Status code returned by uwbs_config APIs.
 */
enum uwbs_config_status {
	/**
	 * @UWBS_CONFIG_STATUS_OK: No error.
	 */
	UWBS_CONFIG_STATUS_OK,
	/**
	 * @UWBS_CONFIG_STATUS_INVALID_ARG: function arguments are invalid or paths does not exists.
	 */
	UWBS_CONFIG_STATUS_INVALID_ARG,
	/**
	 * @UWBS_CONFIG_STATUS_INTERNAL_ERROR: Unexpected state.
	 */
	UWBS_CONFIG_STATUS_INTERNAL_ERROR,
	/**
	 * @UWBS_CONFIG_STATUS_INVALID_KEY: Key from yaml is not defined in schema.
	 */
	UWBS_CONFIG_STATUS_INVALID_KEY,
	/**
	 * @UWBS_CONFIG_STATUS_INVALID_FMT: Invalid format.
	 */
	UWBS_CONFIG_STATUS_INVALID_FMT,
	/**
	 * @UWBS_CONFIG_STATUS_OOM: Out of memory.
	 */
	UWBS_CONFIG_STATUS_OOM,
	/**
	 * UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND: Country code has no associated region but all
	 * generic keys has been loaded. This status can be ignored if country code presence is
	 * optional.
	 */
	UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND,
};

/**
 * enum uwbs_config_value_type - Setting value type.
 */
enum uwbs_config_value_type {
	/**
	 * @UWBS_CONFIG_VTYPE_BLOB: Value is binary buffer.
	 */
	UWBS_CONFIG_VTYPE_BLOB,
	/**
	 * @UWBS_CONFIG_VTYPE_NUMBER: Value is a number.
	 */
	UWBS_CONFIG_VTYPE_NUMBER,
	/**
	 * @UWBS_CONFIG_VTYPE_ARRAY_NUMBER: Value is a number array.
	 */
	UWBS_CONFIG_VTYPE_ARRAY_NUMBER,
};

/**
 * enum uwbs_config_number_type - Setting number type, apply only with
 * UWBS_CONFIG_VTYPE_NUMBER.
 */
enum uwbs_config_number_type {
	/**
	 * @UWBS_CONFIG_NTYPE_INT8: value is int8_t.
	 */
	UWBS_CONFIG_NTYPE_INT8,
	/**
	 * @UWBS_CONFIG_NTYPE_INT8: value is uint8_t.
	 */
	UWBS_CONFIG_NTYPE_UINT8,
	/**
	 * @UWBS_CONFIG_NTYPE_INT16: value is int16_t.
	 */
	UWBS_CONFIG_NTYPE_INT16,
	/**
	 * @UWBS_CONFIG_NTYPE_INT16: value is uint16_t.
	 */
	UWBS_CONFIG_NTYPE_UINT16,
	/**
	 * @UWBS_CONFIG_NTYPE_INT32: value is int32_t.
	 */
	UWBS_CONFIG_NTYPE_INT32,
	/**
	 * @UWBS_CONFIG_NTYPE_INT32: value is uint32_t.
	 */
	UWBS_CONFIG_NTYPE_UINT32,
};

/**
 * typedef uwbs_config_on_new_key_cb - Type for keys callback.
 * @keyname: The key name.
 * @vtype: The key's value type.
 * @ntype: The number type when key's value type is UWBS_CONFIG_VTYPE_NUMBER or
 *         UWBS_CONFIG_VTYPE_ARRAY_NUMBER. Otherwise value is undefined.
 * @value: Pointer to key's value.
 * @size: Size of value.
 * @user_data: pointer provided by caller.
 */
typedef void (*uwbs_config_on_new_key_cb)(const char *keyname,
					  enum uwbs_config_value_type vtype,
					  enum uwbs_config_number_type ntype,
					  const void *value, size_t size,
					  void *user_data);

/**
 * uwbs_config_load() - Load configuration from folder.
 * @path: Settings folder path.
 * @country_code: Country code string, can be NULL. Country code is used to retrieve region
 *                specific settings. When country code is not found in region.yml, it does
 *                not prevent global configuration to be loaded.
 * @user_cb: User callback used called for each key loaded.
 * @user_data: Data provided to user callback.
 *
 * The following files are read:
 *  - <path>/schema.json: configuration definitions specific to firmware.
 *  - <path>/definition.yml [optional]: system definitions.
 *  - <path>/settings.yml [optional]: system settings.
 *  - <path>/calibration.yml [optional]: system calibration.
 *  - <path>/regions.yml: mapping between country codes and world regions. For example:
 *             Region1:
 *               - country1
 *               - country2
 *             Region2:
 *               - country3
 *               - country4
 * - <path>/<region>/settings.yml [optional]: regional system settings.
 *
 * When a key is present multiple files, the callback is called for each occurence.
 *
 * Returns:
 *  - UWBS_CONFIG_STATUS_OK when configuration have been read correctly
 *  - UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND when country code is not defined in regions.yml
 *    but global configurations have been loaded successfully.
 *  - Any other error.
 */
enum uwbs_config_status
uwbs_config_load(const char *path, const char *country_code,
		 const uwbs_config_on_new_key_cb user_cb, void *user_data);
