# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

project(cherry)

include(CMakeDependentOption)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(PkgConfig REQUIRED)

option(CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT "Enable getopt support in Cherry sample apps" ON)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(CONFIG_UCI_TRANSPORT_USE_QMUTILS "Enable use of UCI transport based on qm-utils library" ON)

set(CONFIG_QOSAL_IMPL_LINUX 1)
set(CONFIG_QLOG_LEVEL QLOG_LEVEL_DEBUG) # QLOG_LEVEL_DEBUG = 4

option(CONFIG_UWBS_CONFIGURATION "Enable UWBS configuration APIs, requires file system support" ON)
cmake_dependent_option(
  CONFIG_CHERRY_CALIB_FOLDER
    "Enable cherry calibration folder APIs, requires file system support" ON
    "CONFIG_UWBS_CONFIGURATION" OFF)

  if (CONFIG_CHERRY_CALIB_FOLDER AND NOT CONFIG_UWBS_CONFIGURATION)
  message(FATAL_ERROR "CONFIG_CHERRY_CALIB_FOLDER can not be enabled without CONFIG_UWBS_CONFIGURATION")
endif()


option(CHERRY_PEDANTIC_BUILD "Enaable pedantic build" OFF)

if(CHERRY_PEDANTIC_BUILD)
  add_compile_options(-Wpedantic)
endif()

include(qbuild)

add_subdirectory(qosal)
if(CONFIG_UCI_TRANSPORT_USE_QMUTILS)
# Must stay before uci subdirectory.
add_subdirectory(qm-utils/qmutils)
endif()
add_subdirectory(uci)

if (CONFIG_UWBS_CONFIGURATION)
  pkg_check_modules(LIBYAML yaml-0.1)
  if(NOT LIBYAML_FOUND)
    message(FATAL_ERROR "libyaml not found!")
  endif()
endif()

option(CHERRY_GTESTS "Enable build of Cherry test suites for target" OFF)
if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/cherry)
  add_subdirectory(cherry)
  if(CONFIG_UWBS_CONFIGURATION)
    add_subdirectory(uwbs_config)
  endif()
  if(CHERRY_GTESTS)
    add_subdirectory(cherry/utest)
  endif()
endif()

option(UCI_GTESTS "Enable build of UCI test suites for target" OFF)
if(UCI_GTESTS)
  add_subdirectory(uci/utest)
endif()

add_subdirectory(examples)
add_subdirectory(cherry/ftest)
