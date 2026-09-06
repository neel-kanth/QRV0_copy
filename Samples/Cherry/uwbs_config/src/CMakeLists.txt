# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

cmake_minimum_required(VERSION 3.13)

add_library(uwbs_config
                config_loader.c
                loader.c
                model.c
                model_loader.c
                region_loader.c
                uwbs_config.c
                vector.c
            )

target_include_directories(uwbs_config PUBLIC ../include)

target_link_libraries(uwbs_config PUBLIC qosal yaml)
if(BUILD_SHARED_LIBS)
  install(TARGETS uwbs_config LIBRARY)
endif()
