/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_bypass.hh"

#include "qmchannel_bypass.h"
#include <gtest/gtest.h>

MockBypass *MockBypass::instance_ = nullptr;

int qm3x_bypass_bound(qm3x_bypass_handle hnd, enum qm3x_transport_msg_type *out)
{
	auto mock = MockBypass::get_singleton();
	if (mock)
		return mock->qm3x_bypass_bound(hnd, out);
	return -1;
}

qm3x_bypass_handle qm3x_bypass_open(struct qm3x *qm35,
				    qm3x_bypass_listener_cb cb, void *priv_data)
{
	auto mock = MockBypass::get_singleton();
	if (mock) {
		mock->store_callback(cb, priv_data);
		return mock->qm3x_bypass_open(qm35, cb, priv_data);
	}
	return NULL;
}

int qm3x_bypass_queue_check(qm3x_bypass_handle hnd)
{
	auto mock = MockBypass::get_singleton();
	if (mock)
		return mock->qm3x_bypass_queue_check(hnd);
	return 0;
}

int qm3x_bypass_close(qm3x_bypass_handle hnd)
{
	auto mock = MockBypass::get_singleton();
	if (mock)
		return mock->qm3x_bypass_close(hnd);
	return 0;
}

int qm3x_bypass_send(qm3x_bypass_handle hnd, void *buffer, size_t len)
{
	auto mock = MockBypass::get_singleton();
	if (mock)
		return mock->qm3x_bypass_send(hnd, buffer, len);
	return 0;
}

int qm3x_bypass_recv(qm3x_bypass_handle hnd, void *buffer, size_t len,
		     enum qm3x_transport_msg_type *type, int *flags)
{
	auto mock = MockBypass::get_singleton();
	if (mock)
		return mock->qm3x_bypass_recv(hnd, buffer, len, type, flags);
	return 0;
}
int qm3x_bypass_control(qm3x_bypass_handle hnd, enum qm3x_bypass_actions action,
			long *param)
{
	auto mock = MockBypass::get_singleton();
	if (mock)
		return mock->qm3x_bypass_control(hnd, action, param);
	return 0;
}

MockBypass::MockBypass()
{
	assert(!MockBypass::instance_);
	MockBypass::instance_ = this;
}

MockBypass *MockBypass::get_singleton()
{
	return MockBypass::instance_;
}

MockBypass::~MockBypass()
{
	MockBypass::instance_ = nullptr;
}

void MockBypass::store_callback(qm3x_bypass_listener_cb cb, void *priv_data)
{
	this->priv_data = priv_data;
	this->cb = cb;
}

int MockBypass::call_callback(enum qm3x_bypass_events evt)
{
	if (cb)
		return cb(priv_data, evt);
	else
		return -EINVAL;
}
