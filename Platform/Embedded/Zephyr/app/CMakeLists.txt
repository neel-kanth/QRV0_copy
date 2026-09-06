# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13.1)

list(APPEND ZEPHYR_EXTRA_MODULES
	$ENV{ZEPHYR_BASE}/../../../../Samples/Cherry/uci
	$ENV{ZEPHYR_BASE}/../../../../Samples/Cherry/qosal
	$ENV{ZEPHYR_BASE}/../../../../Samples/Cherry/cherry
	$ENV{ZEPHYR_BASE}/../../../../Samples/Cherry/examples/utilities
	$ENV{ZEPHYR_BASE}/../../../../Samples/Cherry/examples/cherry
	$ENV{ZEPHYR_BASE}/../../../../Samples/Cherry/qm-utils
	$ENV{ZEPHYR_BASE}/../../../../Samples/Cherry/examples/uci_bridge
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
zephyr_library_link_libraries(qosal)

project(app LANGUAGES C)

target_sources(app PRIVATE
	src/main.c
	src/main_qtrace.c
	src/qsoc_flash_bridge.c
	src/shell_utils.c
	src/uci_bridge.c
	src/user_button.c
)
