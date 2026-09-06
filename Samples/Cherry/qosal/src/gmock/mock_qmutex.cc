/*
 * This file is part of the UWB stack for linux.
 *
 * Copyright (c) 2020-2021 Qorvo US, Inc.
 *
 * This software is provided under the GNU General Public License, version 2
 * (GPLv2), as well as under a Qorvo commercial license.
 *
 * You may choose to use this software under the terms of the GPLv2 License,
 * version 2 ("GPLv2"), as published by the Free Software Foundation.
 * You should have received a copy of the GPLv2 along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * This program is distributed under the GPLv2 in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GPLv2 for more
 * details.
 *
 * If you cannot meet the requirements of the GPLv2, you may not use this
 * software for any purpose without first obtaining a commercial license from
 * Qorvo. Please contact Qorvo to inquire about licensing terms.
 */

#include <gtest/gtest.h>
#include <string>

extern "C" {
#include <qerr.h>
#include <qmutex.h>
#include <stdint.h>
#include <stdlib.h>
}

struct qmutex {
	int val;
	int init;
};

extern "C" struct qmutex *qmutex_init()
{
	struct qmutex *lock = (struct qmutex *)malloc(sizeof(qmutex));
	lock->val = 0;
	lock->init = 1;
	return lock;
}

extern "C" qerr qmutex_lock(struct qmutex *lock, uint32_t timeout_ms)
{
	EXPECT_TRUE(lock->init == 1) << "qmutex lock called without init";
	EXPECT_TRUE(lock->val == 0) << "qmutex lock already locked ";
	lock->val = 1;
	return QERR_SUCCESS;
}

extern "C" qerr qmutex_unlock(struct qmutex *lock)
{
	EXPECT_TRUE(lock->init == 1) << "qmutex lock called without init";
	EXPECT_TRUE(lock->val == 1) << "qmutex unlock called without lock";
	lock->val = 0;
	return QERR_SUCCESS;
}

extern "C" void qmutex_deinit(struct qmutex *lock)
{
	EXPECT_TRUE(lock->init == 1) << "qmutex lock called without init";
	EXPECT_TRUE(lock->val == 0) << "qmutex lock already locked ";
	free(lock);
}
