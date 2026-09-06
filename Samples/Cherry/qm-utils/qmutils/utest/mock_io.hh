/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>
#include <poll.h>

extern "C" {
int open_mock(const char *pathname, int flags);
int close_mock(int fd);
ssize_t read_mock(int fd, void *buf, size_t count);
ssize_t write_mock(int fd, const void *buf, size_t count);
int ioctl_mock(int fd, unsigned long request, void *arg);
int poll_mock(struct pollfd *fds, nfds_t nfds, int timeout);
}

class MockIO {
    public:
	/* clang-format off */
	MOCK_METHOD(int, open, (const char *pathname, int flags));
	MOCK_METHOD(int, close, (int fd));
	MOCK_METHOD(ssize_t, read, (int fd, void *buf, size_t count));
	MOCK_METHOD(ssize_t, write, (int fd, const void *buf, size_t count));
	MOCK_METHOD(int, ioctl, (int fd, unsigned long request, void *arg));
	MOCK_METHOD(int, poll, (struct pollfd *fds, nfds_t nfds, int timeout));
	/* clang-format on */

    public:
	MockIO();
	virtual ~MockIO();
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
	static MockIO *get_singleton();

    private:
	static MockIO *instance_;
	bool active = true;
};
