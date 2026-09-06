/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include "mock_channel_callback.hh"

MockChannelCallback::MockChannelCallback()
{
}

MockChannelCallback::~MockChannelCallback()
{
}

int MockChannelCallback::cb(void *user, void *buf, size_t len)
{
	MockChannelCallback *priv = static_cast<MockChannelCallback *>(user);
	return priv->callback(buf, len);
}

int MockChannelCallback::end_cb(void *user, bool status)
{
	MockChannelCallback *priv = static_cast<MockChannelCallback *>(user);
	return priv->end_callback(status);
}
