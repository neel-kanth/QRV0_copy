# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

# Zephyr build.
if(CONFIG_CHERRY_ZEPHYR)
  zephyr_library_named(cherry_uci_client)
  zephyr_library_sources(
    cherry_calib_client.c
    cherry_ccc_client.c
    cherry_core_client.c
    cherry_fira_client.c
    cherry_session_client.c
    cherry_radar_client.c
    cherry_proxy_client.c
  )
  zephyr_include_directories(include ..)
  zephyr_library_link_libraries(qosal uci)

# Linux user mode build.
else()
	add_library(cherry_uci_client
		cherry_calib_client.c
		cherry_ccc_client.c
		cherry_core_client.c
		cherry_fira_client.c
		cherry_proxy_client.c
		cherry_session_client.c
		cherry_radar_client.c
	)
	target_include_directories(cherry_uci_client PUBLIC ${CHERRY_INCLUDE_DIRECTORIES} include PRIVATE ..)
	target_link_libraries(cherry_uci_client PUBLIC qosal uci)
endif()
if(BUILD_SHARED_LIBS)
  install(TARGETS cherry_uci_client LIBRARY)
endif()
