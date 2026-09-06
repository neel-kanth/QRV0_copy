# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

if(!CONFIG_UCI_TRANSPORT_USE_QMUTILS)
  message(FATAL_ERROR "The use of qm-utils library is mandatory now! Aborting...")
endif()

zephyr_library_named(hsspi)

zephyr_library_sources(
  bypass.c
  hsspi.c
  hsspi_helpers.c
  $<$<BOOL:${CONFIG_SHELL}>:shell.c>
)
zephyr_library_link_libraries(qosal)
