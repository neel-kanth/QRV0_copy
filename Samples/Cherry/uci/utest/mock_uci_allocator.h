/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gmock/gmock.h>

extern "C" {
#include "uci/uci.h"
}

struct MockUciAllocator : public uci_allocator {
	MockUciAllocator()
	{
		uci_allocator::ops = new uci_allocator_ops{
			.alloc = MockUciAllocator::static_alloc,
			.free = MockUciAllocator::static_free
		};
	}

	~MockUciAllocator()
	{
		delete uci_allocator::ops;
		uci_allocator::ops = nullptr;
	}

	MOCK_METHOD((struct uci_blk *), alloc,
		    (size_t size_hint, uint8_t flags_hint));
	MOCK_METHOD(void, free, (struct uci_blk *));

	static struct uci_blk *static_alloc(struct uci_allocator *a,
					    size_t size_hint,
					    uint8_t flags_hint)
	{
		return static_cast<MockUciAllocator *>(a)->alloc(size_hint,
								 flags_hint);
	}

	static void static_free(struct uci_allocator *a, struct uci_blk *p)
	{
		static_cast<MockUciAllocator *>(a)->free(p);
	}

	void SetDefaultBehavior()
	{
		ON_CALL(*this, alloc)
			.WillByDefault([](size_t size_hint,
					  uint8_t flags_hint) {
				uci_blk *p = new uci_blk();
				if (p) {
					p->data = new uint8_t[size_hint];
					p->size = size_hint;
					p->len = 0;
					p->next = nullptr;
					p->flags = 0;
					p->total_len = 0;
				}
				return p;
			});
		ON_CALL(*this, free).WillByDefault([](struct uci_blk *p) {
			delete[] p->data;
			delete p;
		});
	}
};
