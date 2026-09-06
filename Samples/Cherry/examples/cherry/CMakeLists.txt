# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

# Zephyr build.
if(CONFIG_CHERRY_EXAMPLES_ZEPHYR)
	zephyr_library_named(cherry_examples)
	zephyr_library_sources(
		twr_app/twr_app.c
		twr_app/twr_app_params.c
		device_app/device_app.c
		device_app/device_app_params.c
		dl_tdoa_app/dl_tdoa_app.c
		dl_tdoa_app/dl_tdoa_app_params.c
		get_calib_app/get_calib_app.c
		get_calib_app/get_calib_app_params.c
		multi_session_app/multi_session_app.c
		multi_session_app/multi_session_app_params.c
		radar_app/radar_app.c
		radar_app/radar_app_params.c
	)
	zephyr_include_directories(
		twr_app
		device_app
		dl_tdoa_app
		get_calib_app
		radar_app
		multi_session_app
	)
	zephyr_library_link_libraries(qosal cherry)

# Linux user mode build.
else()
	if (CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT)
		add_definitions(-DCONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT)
	endif ()
	add_subdirectory(device_app)
	add_subdirectory(dl_tdoa_app)
	add_subdirectory(get_calib_app)
	add_subdirectory(multi_session_app)
	add_subdirectory(radar_app)
	add_subdirectory(twr_app)
endif()
