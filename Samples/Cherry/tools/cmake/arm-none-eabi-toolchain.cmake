# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs" CACHE INTERNAL "")
set(CMAKE_C_FLAGS "-mcpu=${TARGET} -mthumb -mfloat-abi=${ARCH_FPOINT}")
string(APPEND CMAKE_C_FLAGS_RELEASE_INIT " -s")
string(APPEND CMAKE_CXX_FLAGS_RELEASE_INIT " -s")
