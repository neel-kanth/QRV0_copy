/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "qmchannel_bypass.h"
}

struct qm3x_bypass_channel {
	int dummy;
};

class MockBypass {
    public:
	MOCK_METHOD(int, qm3x_bypass_bound,
		    (qm3x_bypass_handle hnd,
		     enum qm3x_transport_msg_type *out));
	MOCK_METHOD(qm3x_bypass_handle, qm3x_bypass_open,
		    (struct qm3x * qm35, qm3x_bypass_listener_cb cb,
		     void *priv_data));
	MOCK_METHOD(int, qm3x_bypass_queue_check, (qm3x_bypass_handle hnd));
	MOCK_METHOD(int, qm3x_bypass_close, (qm3x_bypass_handle hnd));
	MOCK_METHOD(int, qm3x_bypass_send,
		    (qm3x_bypass_handle hnd, void *buffer, size_t len));
	MOCK_METHOD(int, qm3x_bypass_recv,
		    (qm3x_bypass_handle hnd, void *buffer, size_t len,
		     enum qm3x_transport_msg_type *type, int *flags));
	MOCK_METHOD(int, qm3x_bypass_control,
		    (qm3x_bypass_handle hnd, enum qm3x_bypass_actions action,
		     long *param));

    public:
	MockBypass();
	virtual ~MockBypass();

    public:
	static MockBypass *get_singleton();
	void store_callback(qm3x_bypass_listener_cb cb, void *priv_data);
	int call_callback(enum qm3x_bypass_events evt);

    private:
	static MockBypass *instance_;
	qm3x_bypass_listener_cb cb;
	void *priv_data;

    public:
	struct qm3x_bypass_channel bpc;
};
