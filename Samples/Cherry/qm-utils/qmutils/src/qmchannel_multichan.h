/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <stddef.h>

#include "qmutils/qmchannel.h"

struct qmchannel {
	enum qmchannel_type type;
	qmchannel_callback cb;
	void *cb_data;
	char *buffer;
	size_t buffer_size;
	int fd;
};
