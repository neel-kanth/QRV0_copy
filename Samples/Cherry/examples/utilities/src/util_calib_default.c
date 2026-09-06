/*
 * Utilities file to compare calibration to default chip calibration.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <qlog.h>
#include <string.h>
#include <util_calib_default.h>

static bool comp_calib(const struct cherry_calib *calib,
		       const struct cherry_calib *default_calib,
		       bool *different_calib)
{
	const struct cherry_calib_key *key, *default_key;
	struct cherry_calib *calib_tmp = (struct cherry_calib *)calib;
	struct cherry_calib *default_calib_tmp =
		(struct cherry_calib *)default_calib;

	for (int i = 0; i < (int)default_calib_tmp->n_keys; i++) {
		int j = 0;
		uint8_t *data, *default_data;
		default_key = &default_calib_tmp->keys[i];
		while (j < (int)calib_tmp->n_keys &&
		       strcmp(default_key->name, (&calib_tmp->keys[j])->name)) {
			j++;
		}
		/* If the key is not part of the default ones, not in the default case. */
		if (j == (int)calib_tmp->n_keys) {
			return false;
		}
		key = &calib_tmp->keys[j];
		data = (uint8_t *)key->data;
		default_data = (uint8_t *)&default_calib_tmp->keys[i].data;

		if (data[0] != default_data[0]) {
			*different_calib = true;
			return true;
		}
	}
	return true;
}

bool check_calib(struct cherry_calib *calib, bool *has_ant_set,
		 bool *has_lut_set)
{
	*has_ant_set = false;
	*has_lut_set = false;
	if (calib->n_keys != util_calib_default.n_keys) {
		return false;
	}
	if (!comp_calib(calib, &util_calib_default_ant, has_ant_set))
		return false;
	if (!*has_ant_set)
		return true;
	if (!comp_calib(calib, &util_calib_default_lut, has_lut_set))
		return false;
	return true;
}
