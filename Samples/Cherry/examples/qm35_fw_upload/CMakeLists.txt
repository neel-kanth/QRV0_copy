# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

list(APPEND EXAMPLE_SOURCES main.c)

add_executable(qm35-fw-upload ${EXAMPLE_SOURCES})
target_link_libraries(qm35-fw-upload PUBLIC uci)

install(TARGETS qm35-fw-upload RUNTIME)
