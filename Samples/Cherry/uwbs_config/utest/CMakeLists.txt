cmake_minimum_required(VERSION 3.13)
project(test_uwbs_config)

set(CHERRY_TOP ${PROJECT_SOURCE_DIR}/../..)
set(CMAKE_MODULE_PATH ${CHERRY_TOP}/tools/cmake)

option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)

find_package(PkgConfig REQUIRED)

set(CONFIG_QOSAL_IMPL_LINUX 1)

add_subdirectory(${CHERRY_TOP}/deps/googletest gtest)

pkg_check_modules(LIBYAML yaml-0.1)
if(NOT LIBYAML_FOUND)
  message(FATAL_ERROR "Yaml not found!")
endif()

include(qbuild)

# If not included from ${CHERRY_TOP}/CMakeList.txt, need to include
# required subdirectory here. This allow independent build of tests.

add_subdirectory(${CHERRY_TOP}/qosal qosal)
add_subdirectory(../src uwbs_config)

add_executable(
    test_uwbs_config
      test_uwbs_config_model.cc
      test_uwbs_config.cc
      test_uwbs_region.cc
      test_uwbs_config_folder.cc
  )

target_include_directories(test_uwbs_config PRIVATE ../src)

target_link_libraries(test_uwbs_config PUBLIC uwbs_config gmock_main)
target_compile_definitions(test_uwbs_config PRIVATE UTEST_RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources")

add_test(NAME test_uwbs_config COMMAND test_uwbs_config)

if(ENABLE_TEST_COVERAGE)
  include(Coverage)
  target_coverage(uwbs_config)
  add_coverage(NAME test_uwbs_config GTEST_JUNIT)
else()
  include(Sanitize)
  target_sanitize(test_uwbs_config)
endif()
