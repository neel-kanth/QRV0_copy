/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "cherry_calib_mut.h"

#include <cherry/cherry_calib_folder.h>
#include <uwbs_config/uwbs_config.h>

struct cherry_calib_folder {
	/**
	 * @error: indicate if an error occured in callback/
	 */
	bool error;
	/**
	 * @calib_mut: pointer to mutamique calibration struct.
	 */
	struct cherry_calib_mut *calib_mut;
};

static void cherry_calib_folder_on_new_key(const char *keyname,
					   enum uwbs_config_value_type vtype,
					   enum uwbs_config_number_type ntype,
					   const void *value, size_t size,
					   void *user_data)
{
	struct cherry_calib_folder *calib_folder = user_data;
	struct cherry_calib_mut *calib_mut = calib_folder->calib_mut;
	struct cherry_calib_key *tmp;
	struct cherry_calib_key *key;

	/* Ignore event when error occured. */
	if (calib_folder->error)
		return;

	if (!calib_mut->mut_keys)
		calib_mut->calib.n_keys = 0;

	/* Extend array if required. */
	if (calib_mut->mut_keys_capacity == 0 ||
	    calib_mut->mut_keys_capacity == calib_mut->calib.n_keys) {
		calib_mut->mut_keys_capacity += 64;
		tmp = qrealloc(calib_mut->mut_keys,
			       calib_mut->mut_keys_capacity *
				       sizeof(*calib_mut->mut_keys));
		if (!tmp) {
			goto err;
		}
		calib_mut->mut_keys = tmp;
	}

	key = &calib_mut->mut_keys[calib_mut->calib.n_keys++];
	key->name = cherry_calib_mut_strdup(calib_mut, keyname);
	if (!key->name)
		goto err;

	/* Rely on compilation to detect missing cases. */
	switch (vtype) {
	case UWBS_CONFIG_VTYPE_BLOB:
		key->type = CHERRY_CALIB_VALUE_DATA;
		key->data = cherry_calib_mut_memdup(calib_mut, value, size);
		if (!key->data)
			goto err;
		break;

	case UWBS_CONFIG_VTYPE_NUMBER:
		key->type = CHERRY_CALIB_VALUE_NUMBER;
		/* Copy number value */
		switch (ntype) {
		case UWBS_CONFIG_NTYPE_INT8:
			key->number = *(int8_t *)value;
			break;

		case UWBS_CONFIG_NTYPE_UINT8:
			key->number = *(uint8_t *)value;
			break;

		case UWBS_CONFIG_NTYPE_INT16:
			key->number = *(int16_t *)value;
			break;

		case UWBS_CONFIG_NTYPE_UINT16:
			key->number = *(uint16_t *)value;
			break;

		case UWBS_CONFIG_NTYPE_INT32:
			key->number = *(int32_t *)value;
			break;

		case UWBS_CONFIG_NTYPE_UINT32:
			key->number = *(uint32_t *)value;
			break;
		}
		break;

	case UWBS_CONFIG_VTYPE_ARRAY_NUMBER:
		key->type = CHERRY_CALIB_VALUE_NUMBER_ARRAY;

		/* Get item count in array. */
		switch (ntype) {
		case UWBS_CONFIG_NTYPE_INT8:
		case UWBS_CONFIG_NTYPE_UINT8:
			key->nb_array_items = size / sizeof(int8_t);
			break;

		case UWBS_CONFIG_NTYPE_INT16:
		case UWBS_CONFIG_NTYPE_UINT16:
			key->nb_array_items = size / sizeof(int16_t);
			break;

		case UWBS_CONFIG_NTYPE_INT32:
		case UWBS_CONFIG_NTYPE_UINT32:
			key->nb_array_items = size / sizeof(int32_t);
			break;
		}

		key->data = cherry_calib_mut_memdup(calib_mut, value, size);
		if (!key->data)
			goto err;

		break;
	}

	key->size = size;

	return;

err:
	calib_folder->error = true;
	return;
}

struct cherry_calib *cherry_calib_folder_load(const char *folder,
					      const char *country_code)
{
	struct cherry_calib_folder calib_folder;
	enum uwbs_config_status st;

	calib_folder.error = false;
	calib_folder.calib_mut = qmalloc(sizeof(*calib_folder.calib_mut));
	if (!calib_folder.calib_mut)
		return NULL;
	*calib_folder.calib_mut = CHERRY_CALIB_DYN_INIT_STATIC;

	st = uwbs_config_load(folder, country_code,
			      cherry_calib_folder_on_new_key, &calib_folder);
	if ((st != UWBS_CONFIG_STATUS_OK &&
	     st != UWBS_CONFIG_STATUS_COUNTRY_CODE_NOT_FOUND) ||
	    calib_folder.error) {
		cherry_calib_mut_destroy(calib_folder.calib_mut);
		qfree(calib_folder.calib_mut);
		return NULL;
	}

	calib_folder.calib_mut->calib.keys = calib_folder.calib_mut->mut_keys;

	return &calib_folder.calib_mut->calib;
}
