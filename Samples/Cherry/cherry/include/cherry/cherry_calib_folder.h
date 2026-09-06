/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_CALIB_FOLDER_H
#define CHERRY_CALIB_FOLDER_H

#include <cherry/cherry.h>

#ifdef CONFIG_CHERRY_CALIB_FOLDER
/**
 * cherry_calib_folder_load() - Load calibration keys from folder based on country code.
 * @folder: path to calibration folder.
 * @country_code: Current country code (optional).
 *
 * The following files are read:
 * - <path>/schema.json: settings definitions specific to firmware.
 * - <path>/definition.yml [optional]: system definitions settings.
 * - <path>/configuration.yml [optional]: system configuration settings.
 * - <path>/calibration.yml [optional]: system calibration settings.
 * - <path>/regions.yml: mapping between country codes and world regions. For example:
 *             Region1:
 *               - country1
 *               - country2
 *             Region2:
 *               - country3
 *               - country4
 * - <path>/<region>/configuration.yml [optional]: region system configuration settings.
 *
 * Returns: pointer to loaded calibration, must be freed using cherry_calib_destroy()
 */
struct cherry_calib *cherry_calib_folder_load(const char *folder,
					      const char *country_code);

#endif /* CONFIG_CHERRY_CALIB_FOLDER */

#endif /* CHERRY_CALIB_FOLDER_H */
