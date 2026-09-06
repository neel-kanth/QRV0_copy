# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-twr-app twr_app.c twr_app_params.c)
target_link_libraries(cherry-twr-app PUBLIC cherry utilities)

install(TARGETS cherry-twr-app RUNTIME)
