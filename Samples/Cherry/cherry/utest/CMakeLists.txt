# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)
project(test_cherry)

set(CHERRY_TOP ${PROJECT_SOURCE_DIR}/../..)
set(CMAKE_MODULE_PATH ${CHERRY_TOP}/tools/cmake)

option(ENABLE_TEST_COVERAGE "Enable test coverage" ON)

# If not included from ${CHERRY_TOP}/CMakeList.txt, need to include
# required subdirectory here. This allow independent build of tests.

find_package(PkgConfig REQUIRED)

set(CONFIG_QOSAL_IMPL_GMOCK 1)

pkg_check_modules(LIBYAML yaml-0.1)
if(NOT LIBYAML_FOUND)
  message(FATAL_ERROR "Yaml not found!")
endif()

add_subdirectory(${CHERRY_TOP}/deps/googletest gtest)

include(qbuild)

add_subdirectory(${CHERRY_TOP}/qosal qosal)
add_subdirectory(${CHERRY_TOP}/uci uci)
add_subdirectory(${CHERRY_TOP}/uwbs_config uwbs_config)
add_subdirectory(../src cherry)

add_executable(
	test_cherry
	test_cherry.cc
	mock_cherry_calib_client.cc
	test_cherry_calib_folder.cc
	mock_cherry_ccc_client.cc
	mock_cherry_core_client.cc
	mock_cherry_fira_client.cc
	mock_cherry_radar_client.cc
	mock_cherry_session_client.cc
	mock_cherry_thread.cc
	mock_cherry_uci_transport.cc
	mock_uci.cc
	../src/cherry.c
	../src/cherry_calib_folder.c
        ../src/cherry_calib_mut.c
        ../src/cherry_ccc.c
	../src/cherry_fira.c
	../src/cherry_radar.c
	../src/cherry_session.c
	../src/cherry_session_manager.c
)

target_include_directories(test_cherry PRIVATE ../src ../src/uci_client/include ${CHERRY_TOP}/uci/uci_transport/include ../include ${CHERRY_TOP}/uci/uci_core/include)
target_compile_definitions(test_cherry PRIVATE CONFIG_CHERRY_CALIB_FOLDER UTEST_RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources" )

target_link_libraries(test_cherry PUBLIC gmock_main qosal uwbs_config)

add_test(NAME test_cherry COMMAND test_cherry)

if(ENABLE_TEST_COVERAGE)
  include(Coverage)
  target_coverage(test_cherry)
  target_coverage(cherry)
  add_coverage(NAME test_cherry GTEST_JUNIT)
else()
  include(Sanitize)
  target_sanitize(test_cherry)
endif()
