/*
 * Header file for for uart uci transport.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <cherry/cherry_proxy.h>
#include <uci/uci.h>

struct uci_transport *
uci_transport_uart_create(struct cherry_proxy *cherry_proxy_context);
void uci_transport_uart_destroy(struct uci_transport *transport);

struct uci_transport_uart {
	struct uci *uci;
	struct uci_transport base;
	struct cherry_proxy *cherry_proxy_context;
	struct uci_blk *uci_pkt;
	struct uci_blk *uci_pkt_ready;
#ifdef CONFIG_UCI_ZEPHYR
	struct k_work work_uart_packet_received;
	const struct device *uart;
	uint16_t uart_data_len;
	bool exit;
#endif
};
