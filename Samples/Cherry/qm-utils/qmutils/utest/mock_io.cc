/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_io.hh"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <poll.h>
#include <sys/ioctl.h>

MockIO *MockIO::instance_ = nullptr;

int open_mock(const char *pathname, int flags)
{
	auto mock = MockIO::get_singleton();
	if (mock && mock->mocked())
		return mock->open(pathname, flags);
	/* Call real open(). */
	return open(pathname, flags);
}

int close_mock(int fd)
{
	auto mock = MockIO::get_singleton();
	if (mock && mock->mocked())
		return mock->close(fd);
	/* Call real close(). */
	return close(fd);
}

ssize_t read_mock(int fd, void *buf, size_t count)
{
	auto mock = MockIO::get_singleton();
	if (mock && mock->mocked())
		return mock->read(fd, buf, count);
	/* Call real read(). */
	return read(fd, buf, count);
}

ssize_t write_mock(int fd, const void *buf, size_t count)
{
	auto mock = MockIO::get_singleton();
	if (mock && mock->mocked())
		return mock->write(fd, buf, count);
	/* Call real write(). */
	return write(fd, buf, count);
}

int ioctl_mock(int fd, unsigned long request, void *arg)
{
	auto mock = MockIO::get_singleton();
	if (mock && mock->mocked())
		return mock->ioctl(fd, request, arg);
	/* Call real ioctl(). */
	return ioctl(fd, request, arg);
}

int poll_mock(struct pollfd *fds, nfds_t nfds, int timeout)
{
	auto mock = MockIO::get_singleton();
	if (mock && mock->mocked())
		return mock->poll(fds, nfds, timeout);
	/* Call real poll(). */
	return poll(fds, nfds, timeout);
}

MockIO::MockIO()
{
	assert(!MockIO::instance_);
	MockIO::instance_ = this;
}

MockIO *MockIO::get_singleton()
{
	return MockIO::instance_;
}

MockIO::~MockIO()
{
	MockIO::instance_ = nullptr;
}
