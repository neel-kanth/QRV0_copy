# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-dl-tdoa-app dl_tdoa_app.c dl_tdoa_app_params.c)
target_link_libraries(cherry-dl-tdoa-app PUBLIC cherry utilities)

install(TARGETS cherry-dl-tdoa-app RUNTIME)
