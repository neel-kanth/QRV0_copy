# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)
project(test_uci)

set(CHERRY_TOP ${PROJECT_SOURCE_DIR}/../..)
set(CMAKE_MODULE_PATH ${CHERRY_TOP}/tools/cmake)
option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)

set(CONFIG_QOSAL_IMPL_GMOCK 1)

add_subdirectory(${CHERRY_TOP}/deps/googletest gtest)

include(qbuild)

# If not included from ${CHERRY_TOP}/CMakeList.txt, need to include
# required subdirectory here. This allow independent build of tests.

if(NOT TARGET qosal)
add_subdirectory(${CHERRY_TOP}/qosal qosal)
endif()
if(NOT TARGET uci)
add_subdirectory(.. uci)
endif()

add_executable(test_uci test_uci_core.cc)

target_include_directories(test_uci PRIVATE ../src)
target_link_libraries(test_uci PUBLIC uci gmock_main)

add_test(NAME test_uci COMMAND test_uci)

if(ENABLE_TEST_COVERAGE)
  include(Coverage)
  target_coverage(test_uci)
  target_coverage(uci)
  add_coverage(NAME test_uci GTEST_JUNIT)
else()
  include(Sanitize)
  target_sanitize(test_uci)
endif()
