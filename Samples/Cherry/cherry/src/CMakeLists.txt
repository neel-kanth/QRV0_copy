# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

set(CHERRY_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_subdirectory(uci_client)

if(CONFIG_CHERRY_ZEPHYR)

  # Zephyr build.

  zephyr_library_named(cherry)
  zephyr_library_sources(
    cherry.c
    cherry_calib_mut.c
    cherry_ccc.c
    cherry_fira.c
    cherry_session.c
    cherry_session_manager.c
    cherry_thread.c
    cherry_radar.c
    cherry_proxy.c
  )
  if (CONFIG_UCI_TRANSPORT_USE_QMUTILS)
	zephyr_library_sources(cherry_uci_transport_qmutils.c)
  else()
    zephyr_library_sources(cherry_uci_transport_hsspi.c)
  endif()

  zephyr_include_directories(
	${CHERRY_INCLUDE_DIRECTORIES}
  )
  zephyr_library_link_libraries(qosal cherry_uci_client)

else()

  # Linux user mode build.

  add_library(cherry
	cherry.c
	cherry_calib_mut.c
	cherry_ccc.c
	cherry_fira.c
	cherry_proxy.c
	cherry_radar.c
	cherry_session.c
	cherry_session_manager.c
	cherry_thread.c
	cherry_uci_transport.c
  )
  target_include_directories(cherry PUBLIC ${CHERRY_INCLUDE_DIRECTORIES})
  target_link_libraries(cherry PUBLIC qosal PRIVATE cherry_uci_client)
  if (CONFIG_CHERRY_CALIB_FOLDER)
	target_sources(cherry PRIVATE cherry_calib_folder.c)
	target_link_libraries(cherry PRIVATE uwbs_config)
	target_compile_definitions(cherry PUBLIC CONFIG_CHERRY_CALIB_FOLDER)
  endif()
  if (CONFIG_UCI_TRANSPORT_USE_QMUTILS)
	target_compile_definitions(cherry PRIVATE CONFIG_USE_QMUTILS)
  endif()

endif()
if(BUILD_SHARED_LIBS)
  install(TARGETS cherry LIBRARY)
endif()
