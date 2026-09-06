# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-test-session-unsupported test_session_unsupported.c)
target_link_libraries(cherry-test-session-unsupported PUBLIC cherry)

install(TARGETS cherry-test-session-unsupported RUNTIME)
