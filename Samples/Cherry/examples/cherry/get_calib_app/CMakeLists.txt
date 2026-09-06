# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-get-calib-app get_calib_app.c get_calib_app_params.c)
target_link_libraries(cherry-get-calib-app  PUBLIC cherry utilities)

install(TARGETS cherry-get-calib-app RUNTIME)
