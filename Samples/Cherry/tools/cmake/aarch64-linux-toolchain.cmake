# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Make pkg-config look in the right place
find_program(CROSS_PKG_CONFIG aarch64-linux-gnu-pkg-config)
if(CROSS_PKG_CONFIG)
  # for UsePkgConfig
  set(PKGCONFIG_EXECUTABLE ${CROSS_PKG_CONFIG})
  # for FindPkgConfig
  set(PKG_CONFIG_EXECUTABLE ${CROSS_PKG_CONFIG})
else()
  set(ENV{PKG_CONFIG_LIBDIR} /usr/lib/aarch64-linux-gnu/pkgconfig)
endif()
