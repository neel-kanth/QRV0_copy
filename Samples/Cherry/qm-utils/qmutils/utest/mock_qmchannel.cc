/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_qmchannel.hh"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/ioctl.h>

MockQmchannel *MockQmchannel::instance_ = nullptr;

int qmchannel_init_mock(qmhandle hnd, enum qmchannel_type type,
			qmchannel_callback cb, void *cb_data)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_init(hnd, type, cb, cb_data);
	/* Call real qmchannel_init(). */
	return qmchannel_init(hnd, type, cb, cb_data);
}

int qmchannel_close_mock(qmhandle hnd)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_close(hnd);
	/* Call real qmchannel_close(). */
	return qmchannel_close(hnd);
}

int qmchannel_getfd_mock(qmhandle hnd)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_getfd(hnd);
	/* Call real qmchannel_getfd(). */
	return qmchannel_getfd(hnd);
}

int qmchannel_getpollevent_mock(qmhandle hnd)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_getpollevent(hnd);
	/* Call real qmchannel_getpollevent(). */
	return qmchannel_getpollevent(hnd);
}

int qmchannel_setbuf_mock(qmhandle hnd, void *buf, size_t len)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_setbuf(hnd, buf, len);
	/* Call real qmchannel_setbuf(). */
	return qmchannel_setbuf(hnd, buf, len);
}

int qmchannel_read_mock(qmhandle hnd)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_read(hnd);
	/* Call real qmchannel_read(). */
	return qmchannel_read(hnd);
}

int qmchannel_write_mock(qmhandle hnd, void *buf, size_t len)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_write(hnd, buf, len);
	/* Call real qmchannel_write(). */
	return qmchannel_write(hnd, buf, len);
}

int qmchannel_ioctl_mock(qmhandle hnd, unsigned long request, char *argp)
{
	auto mock = MockQmchannel::get_singleton();
	if (mock && mock->mocked())
		return mock->qmchannel_ioctl(hnd, request, argp);
	/* Call real qmchannel_ioctl(). */
	return qmchannel_ioctl(hnd, request, argp);
}

MockQmchannel::MockQmchannel()
{
	assert(!MockQmchannel::instance_);
	MockQmchannel::instance_ = this;
}

MockQmchannel *MockQmchannel::get_singleton()
{
	return MockQmchannel::instance_;
}

MockQmchannel::~MockQmchannel()
{
	MockQmchannel::instance_ = nullptr;
}
