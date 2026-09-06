# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)
project(ftest_cherry)

set(CHERRY_TOP ${PROJECT_SOURCE_DIR}/../..)
set(CMAKE_MODULE_PATH ${CHERRY_TOP}/tools/cmake)

include(qbuild)

add_subdirectory(test_reload_calibration)
add_subdirectory(test_session_deinit)
add_subdirectory(test_session_unsupported)
