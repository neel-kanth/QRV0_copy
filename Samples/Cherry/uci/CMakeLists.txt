# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

set(UCI_INCLUDE_DIRECTORIES uci_core/include uci_core/src uci_transport/include)

if(CONFIG_UCI_ZEPHYR)

  # Zephyr build.

  zephyr_library_named(uci)
  zephyr_library_sources(
    uci_core/src/uci.c
    uci_core/src/uci_message.c
  )
  if(CONFIG_UCI_TRANSPORT_USE_QMUTILS)
    zephyr_library_sources(
      uci_transport/src/uci_transport_qmutils.c uci_transport/src/uci_transport_uart.c
    )
    zephyr_library_compile_definitions(USE_QMUTILS)
  else()
    zephyr_library_sources(uci_transport/src/uci_transport_hsspi.c)
  endif()
  zephyr_include_directories(${UCI_INCLUDE_DIRECTORIES})
  zephyr_library_link_libraries(qosal)

else()
  # Linux user mode build.
  add_library(uci uci_core/src/uci.c
		uci_core/src/uci_message.c
		uci_transport/src/uci_transport_fd.c
		uci_transport/src/uci_transport_chardev.c
		uci_transport/src/uci_transport_serial.c
  )
  target_include_directories(uci PUBLIC ${UCI_INCLUDE_DIRECTORIES})
  target_link_libraries(uci PUBLIC qosal)
  if(CONFIG_UCI_TRANSPORT_USE_QMUTILS)
    target_sources(uci PRIVATE uci_transport/src/uci_transport_qmutils.c)
    target_link_libraries(uci PUBLIC qmutils_multichan)
  endif()
endif()
if(BUILD_SHARED_LIBS)
  install(TARGETS uci LIBRARY)
endif()
