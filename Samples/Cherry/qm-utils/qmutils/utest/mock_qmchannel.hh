/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

#include "qmutils/qmchannel.h"

extern "C" {
int qmchannel_init_mock(qmhandle hnd, enum qmchannel_type type,
			qmchannel_callback cb, void *cb_data);
int qmchannel_close_mock(qmhandle hnd);
int qmchannel_getfd_mock(qmhandle hnd);
int qmchannel_getpollevent_mock(qmhandle hnd);
int qmchannel_setbuf_mock(qmhandle hnd, void *buf, size_t len);
int qmchannel_read_mock(qmhandle hnd);
int qmchannel_write_mock(qmhandle hnd, void *buf, size_t len);
int qmchannel_ioctl_mock(qmhandle hnd, unsigned long request, char *argp);
}

class MockQmchannel {
    public:
	/* clang-format off */
	MOCK_METHOD(int, qmchannel_init,
		    (qmhandle hnd, enum qmchannel_type type,
		     qmchannel_callback cb, void *cb_data));
	MOCK_METHOD(int, qmchannel_close, (qmhandle hnd));
	MOCK_METHOD(int, qmchannel_getfd, (qmhandle hnd));
	MOCK_METHOD(int, qmchannel_getpollevent, (qmhandle hnd));
	MOCK_METHOD(int, qmchannel_setbuf,
		    (qmhandle hnd, void *buf, size_t len));
	MOCK_METHOD(int, qmchannel_read, (qmhandle hnd));
	MOCK_METHOD(int, qmchannel_write,
		    (qmhandle hnd, void *buf, size_t len));
	MOCK_METHOD(int, qmchannel_ioctl,
		    (qmhandle hnd, unsigned long request, char *argp));
	/* clang-format on */

    public:
	MockQmchannel();
	virtual ~MockQmchannel();
	void implicit_call()
	{
		active = false;
	}
	void explicit_call()
	{
		active = true;
	}
	bool mocked()
	{
		return active;
	}

    public:
	static MockQmchannel *get_singleton();

    private:
	static MockQmchannel *instance_;
	bool active = true;
};
