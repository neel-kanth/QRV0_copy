# ~~~
# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2
# ~~~

cmake_minimum_required(VERSION 3.13)

set(COMMON_SOURCES
    src/common.c
    src/uci.c
    src/hsspi.c
    src/coredump.c
    src/misc.c
    src/log.c
    src/qtrace.c
)

if(CONFIG_QMUTILS_ZEPHYR)

  # Zephyr build.

  zephyr_library_named(qmutils)
  zephyr_library_sources(src/bypass.c ${COMMON_SOURCES})
  zephyr_include_directories(include)
  zephyr_compile_definitions(QMCHANNEL_USE_BYPASS)
  zephyr_library_link_libraries(qosal)

elseif(NOT ZEPHYR_BASE)

  # Linux user mode build.

  set(UCI_DEV_PATH CACHE STRING "Path to UCI device")
  add_library(qmutils_multichan STATIC src/multichan.c ${COMMON_SOURCES})
  target_include_directories(
    qmutils_multichan
    PUBLIC include
    PRIVATE src
  )
  target_compile_definitions(qmutils_multichan PUBLIC QMCHANNEL_USE_MULTICHAN)
  if(UCI_DEV_PATH)
    target_compile_definitions(qmutils_multichan PRIVATE UCI_DEV_PATH="${UCI_DEV_PATH}")
  endif()
  target_link_libraries(qmutils_multichan PRIVATE qosal)

  add_library(qmutils_bypass STATIC src/bypass.c ${COMMON_SOURCES})
  target_include_directories(
    qmutils_bypass
    PUBLIC include
    PRIVATE src
  )
  target_compile_definitions(qmutils_bypass PUBLIC QMCHANNEL_USE_BYPASS)
  target_link_libraries(qmutils_bypass PRIVATE qosal)

  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../tools/cmake)
  include(helpers)
  qorvo_install_target(qmutils_multichan)
  qorvo_install_target(qmutils_bypass)

endif()
