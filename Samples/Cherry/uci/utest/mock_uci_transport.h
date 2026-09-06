/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include <uci/uci.h>
}

class MockUciTransport : public uci_transport {
    public:
	MockUciTransport()
	{
		uci_transport::ops = new uci_transport_ops{
			.attach = MockUciTransport::static_attach,
			.detach = MockUciTransport::static_detach,
			.packet_send_ready =
				MockUciTransport::static_packet_send_ready
		};
	}

	~MockUciTransport()
	{
		delete uci_transport::ops;
		uci_transport::ops = nullptr;
	}

	static void static_attach(struct uci_transport *uci_tr, struct uci *uci)
	{
		MockUciTransport *self =
			static_cast<MockUciTransport *>(uci_tr);
		ASSERT_TRUE(self);
		self->uci_ = uci;
	}

	static void static_detach(struct uci_transport *uci_tr)
	{
		MockUciTransport *self =
			static_cast<MockUciTransport *>(uci_tr);
		ASSERT_TRUE(self);
		self->uci_ = nullptr;
	}

	static void static_packet_send_ready(struct uci_transport *uci_tr)
	{
		MockUciTransport *self =
			static_cast<MockUciTransport *>(uci_tr);
		ASSERT_TRUE(self);
		struct uci_blk *p;
		while ((p = uci_packet_send_get_ready(self->uci_))) {
			auto status = self->Out(p);
			uci_packet_send_done(self->uci_, p, status);
		}
		/* We want to be able to:
		 * - Simulate multiple packet received for one command sent (the
		 * loop).
		 * - Be able to prepare TWO reply for two commands sent by one
		 * API (a second uci_send_message() call in one API).
		 * - Allow an Out() lambda to queue one more reply.
		 */
		/* This loop assume NO reply are split into multiple uci_blk
		 * chain. So ensure ALL forged replies with
		 * uci_message_builder() have enough length.
		 */
		while (self->reply_) {
			struct uci_blk *n = self->reply_->next;
			self->reply_->next = nullptr;
			uci_packet_recv(self->uci_, self->reply_);
			self->reply_ = n;
		}
		if (self->second_reply_) {
			/* Prepare next call by uci_send_message(). */
			self->reply_ = self->second_reply_;
			self->second_reply_ = nullptr;
		}
	}

	MOCK_METHOD(int, Out, (struct uci_blk * p));

	void SetReply(struct uci_blk *reply)
	{
		if (!reply_) {
			reply_ = reply;
			second_reply_ = nullptr;
		} else {
			second_reply_ = reply;
		}
	}

	void SendNotif(struct uci_blk *notif)
	{
		notif_ = notif;
		if (notif_) {
			uci_packet_recv(this->uci_, this->notif_);
		}
	}

    private:
	struct uci *uci_{ nullptr };
	struct uci_blk *reply_{ nullptr };
	struct uci_blk *second_reply_{ nullptr };
	struct uci_blk *notif_{ nullptr };
};
