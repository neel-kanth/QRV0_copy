# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-device-app device_app.c device_app_params.c)
target_link_libraries(cherry-device-app PUBLIC cherry utilities)

install(TARGETS cherry-device-app RUNTIME)
