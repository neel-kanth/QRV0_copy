# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

set(UTILITIES_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Zephyr build.
if(CONFIG_CHERRY_EXAMPLES_UTILITIES_ZEPHYR)
	zephyr_library_named(cherry_examples_utilities)
	zephyr_library_sources(
		src/util_calib_default.c
		src/util_common_param.c
		src/util_convert.c
        src/util_dltdoa_config.c
		src/util_dump.c
		src/util_log.c
	)
	zephyr_include_directories(
		${UTILITIES_INCLUDE_DIRECTORIES}
	)
	zephyr_library_link_libraries(qosal cherry)

# Linux user mode build.
else()
	add_library(utilities
		src/util_calib_default.c
		src/util_common_param.c
		src/util_convert.c
        src/util_dltdoa_config.c
		src/util_dump.c
		src/util_log.c
	)
	target_include_directories(utilities PUBLIC
		${UTILITIES_INCLUDE_DIRECTORIES}
		${CHERRY_INCLUDE_DIRECTORIES}
	)
	target_link_libraries(utilities PUBLIC cherry qosal)
endif()
if(BUILD_SHARED_LIBS)
  install(TARGETS utilities LIBRARY)
endif()
