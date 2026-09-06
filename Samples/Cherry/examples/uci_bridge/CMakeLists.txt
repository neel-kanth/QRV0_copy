# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

# Zephyr build.
if(CONFIG_CHERRY_BRIDGE_ZEPHYR)
  zephyr_library_named(cherry_bridge)
  zephyr_library_sources(uci_bridge_app.c)
  zephyr_include_directories(.)
  zephyr_library_link_libraries(qosal cherry)

  # Linux user mode build.
else()
  if(CONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT)
    add_definitions(-DCONFIG_CHERRY_EXAMPLES_GETOPT_SUPPORT)
  endif()
  add_executable(uci-bridge uci_bridge_app.c)
  target_link_libraries(uci-bridge PUBLIC cherry utilities)

  install(TARGETS uci-bridge RUNTIME DESTINATION bin)

  include(GNUInstallDirs)

  install(
    FILES uci-bridge.service
    DESTINATION /lib/systemd/system
  )
  install(
    FILES uci-bridge.default
    DESTINATION /etc/default
    RENAME uci-bridge
  )
endif()
