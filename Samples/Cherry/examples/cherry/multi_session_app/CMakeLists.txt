# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-multi-session-app multi_session_app.c multi_session_app_params.c)
target_link_libraries(cherry-multi-session-app PUBLIC cherry utilities)

install(TARGETS cherry-multi-session-app RUNTIME)
