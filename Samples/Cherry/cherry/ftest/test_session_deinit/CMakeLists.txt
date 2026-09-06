# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-test-session-deinit test_session_deinit.c)
target_link_libraries(cherry-test-session-deinit PUBLIC cherry)

install(TARGETS cherry-test-session-deinit RUNTIME)
