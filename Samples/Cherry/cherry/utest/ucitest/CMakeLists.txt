# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)
project(test_cherry_client)

set(CHERRY_TOP ${PROJECT_SOURCE_DIR}/../../..)
set(CMAKE_MODULE_PATH ${CHERRY_TOP}/tools/cmake)
option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)

# If not included from ${CHERRY_TOP}/CMakeList.txt, need to include
# required subdirectory here. This allow independent build of tests.

set(CONFIG_QOSAL_IMPL_GMOCK 1)

add_subdirectory(${CHERRY_TOP}/deps/googletest gtest)

include(qbuild)

add_subdirectory(${CHERRY_TOP}/qosal qosal)
add_subdirectory(${CHERRY_TOP}/uci uci)
add_subdirectory(${CHERRY_TOP}/cherry/src cherry)

add_executable(test_cherry_client
	test_fira_client.cc
	test_calib_client.cc
	test_core_client.cc
	test_proxy_client.cc
	test_session_client.cc
	test_radar_client.cc
	test_ccc_client.cc
)

target_include_directories(test_cherry_client PRIVATE ../src)
target_link_libraries(test_cherry_client PUBLIC gmock_main cherry_uci_client)

add_test(NAME test_cherry_client COMMAND test_cherry_client)

if(ENABLE_TEST_COVERAGE)
  include(Coverage)
  target_coverage(test_cherry_client)
  target_coverage(cherry_uci_client)
  add_coverage(NAME test_cherry_client GTEST_JUNIT)
else()
  include(Sanitize)
  target_sanitize(test_cherry_client)
endif()
