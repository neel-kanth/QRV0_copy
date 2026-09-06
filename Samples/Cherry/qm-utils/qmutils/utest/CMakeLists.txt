cmake_minimum_required(VERSION 3.13)
project(test_qmutils LANGUAGES C CXX)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
list(APPEND CMAKE_MODULE_PATH ${ROOT_DIR}/tools/cmake)

add_subdirectory(${ROOT_DIR}/deps/googletest gtest)

include(qbuild)
include(qtest)

set(CMAKE_BUILD_TYPE Debug)

set(CONFIG_QOSAL_IMPL_GMOCK 1)
set(CONFIG_QLOG_LEVEL QLOG_LEVEL_DEBUG)

add_subdirectory(${ROOT_DIR}/deps/qosal qosal) # needed by qmutils

add_subdirectory(${ROOT_DIR}/qmutils qmutils)

# Ensure library build using mock functions.
target_compile_definitions(qmutils_multichan PRIVATE USE_MOCK)
target_compile_definitions(qmutils_bypass PRIVATE USE_MOCK)

add_executable(
  test_qmutils_multichan
  test_multichan.cc
  test_hsspi.cc
  test_uci.cc
  test_coredump.cc
  test_log.cc
  test_qtrace.cc
  test_misc.cc
  mock_channel_callback.cc
  mock_io.cc
  mock_pthread.cc
  mock_qmchannel.cc
)
target_include_directories(test_qmutils_multichan PRIVATE ../src)
target_link_libraries(test_qmutils_multichan PUBLIC qmutils_multichan gmock_main qosal)

add_executable(test_qmutils_bypass test_bypass.cc mock_bypass.cc mock_channel_callback.cc)
target_include_directories(test_qmutils_bypass PRIVATE ../src)
target_link_libraries(test_qmutils_bypass PUBLIC qmutils_bypass gmock_main qosal)

add_test(NAME test_qmutils_multichan COMMAND test_qmutils_multichan)
add_test(NAME test_qmutils_bypass COMMAND test_qmutils_bypass)

target_activate_test_mode(qmutils_multichan)
target_activate_test_mode(qmutils_bypass)

if(QTEST_MODE STREQUAL "COVERAGE")
  add_coverage(NAME test_qmutils_multichan)
  add_coverage(NAME test_qmutils_bypass INCLUDE ../src/bypass.c)
endif()
