# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_executable(cherry-test-reload-calibration test_reload_calibration.c)
target_link_libraries(cherry-test-reload-calibration  PUBLIC cherry)

install(TARGETS cherry-test-reload-calibration RUNTIME)
