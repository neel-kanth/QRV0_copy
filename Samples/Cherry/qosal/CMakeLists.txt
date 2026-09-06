# ~~~
# SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2
# ~~~

cmake_minimum_required(VERSION 3.13)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../tools/cmake)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../tools/cmake)
elseif(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../../tools/cmake)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../../tools/cmake)
endif()
include(helpers)

# Declares the interface alone.
add_library(qosal_itf INTERFACE)
target_include_directories(qosal_itf INTERFACE include)
if(CONFIG_QLOG_LEVEL)
  target_compile_definitions(qosal_itf INTERFACE CONFIG_QLOG_LEVEL=${CONFIG_QLOG_LEVEL})
endif()

if(CONFIG_Q_QHAL)
  # Zephyr qpm.c implem depend on QHAL module (from uwb-stack)
  set(ZEPHYR_QPM zephyr/qpm.c)
else()
  # freertos/qpm.c is an empty dummy implementation for now
  set(ZEPHYR_QPM freertos/qpm.c)
endif()

if(CONFIG_QOSAL_IMPL_ZEPHYR)
  target_include_directories(qosal_itf INTERFACE include/zephyr)
  target_compile_definitions(qosal_itf INTERFACE QOSAL_ENABLED)

  add_library(
    qosal STATIC EXCLUDE_FROM_ALL
    src/qerr_common.c
    src/zephyr/qerr.c
    src/zephyr/qirq.c
    src/zephyr/qlog.c
    src/zephyr/qmalloc.c
    src/zephyr/qprofiling.c
    src/zephyr/qmsg_queue.c
    src/zephyr/qmutex.c
    src/${ZEPHYR_QPM}
    src/zephyr/qrand.c
    src/zephyr/qsemaphore.c
    src/zephyr/qsignal.c
    src/zephyr/qthread.c
    src/zephyr/qtime.c
    src/zephyr/qworkqueue.c
    src/zephyr/qtracing.c
  )
  target_link_libraries(qosal PUBLIC qosal_itf)
  target_link_libraries(
    qosal
    PUBLIC zephyr_interface
    # TODO kernel dependency is required as qosal is not exactly a zephyr library and must depends
    # on zephyr_generated_headers library for offsets.h
    PRIVATE kernel
  )
  if(CONFIG_Q_QHAL)
    target_link_libraries(qosal PRIVATE qhal)
  endif()
  target_compile_options(qosal PRIVATE -Wno-error=type-limits)

elseif(CONFIG_QOSAL_IMPL_FREERTOS)
  target_include_directories(qosal_itf INTERFACE include/freertos)
  target_compile_definitions(qosal_itf INTERFACE QOSAL_ENABLED)

  add_library(
    qosal STATIC EXCLUDE_FROM_ALL
    src/qerr_common.c
    src/freertos/qerr.c
    src/freertos/qirq.c
    src/freertos/qmalloc.c
    src/freertos/qprofiling.c
    src/freertos/qmsg_queue.c
    src/freertos/qmutex.c
    src/freertos/qpm.c
    src/freertos/qprivate.c
    src/freertos/qrand.c
    src/freertos/qsemaphore.c
    src/freertos/qsignal.c
    src/freertos/qthread.c
    src/freertos/qtime.c
    src/freertos/qworkqueue.c
    src/freertos/qtracing.c
  )
  target_link_libraries(qosal PUBLIC qosal_itf)
  target_include_directories(qosal PRIVATE src/freertos)

elseif(CONFIG_QOSAL_IMPL_LINUX)
  # needed for qlog stub
  target_include_directories(qosal_itf INTERFACE include/linux include/dummy)
  target_compile_definitions(qosal_itf INTERFACE QOSAL_ENABLED)

  add_library(
    qosal EXCLUDE_FROM_ALL
    src/qerr_common.c
    src/linux/qerr.c
    src/linux/qlog.c
    src/linux/qmalloc.c
    src/linux/qmutex.c
    src/linux/qprofiling.c
    src/linux/qsemaphore.c
    src/linux/qthread.c
    src/linux/qtime.c
  )
  target_link_libraries(qosal PUBLIC qosal_itf)
  if(BUILD_SHARED_LIBS)
    install(TARGETS qosal LIBRARY)
  endif()

elseif(CONFIG_QOSAL_IMPL_GMOCK)
  target_include_directories(qosal_itf INTERFACE include/gmock)

  add_library(
    qosal STATIC EXCLUDE_FROM_ALL
    src/qerr_common.c
    src/linux/qerr.c
    src/gmock/mock_qmalloc.cc
    src/gmock/fake_qosal.cc
    src/gmock/mock_qassert.cc
    src/gmock/mock_qsemaphore.cc
    src/gmock/mock_qosal.cc
    src/gmock/mock_qmutex.cc
  )
  target_link_libraries(qosal PUBLIC qosal_itf gmock)

else()
  target_include_directories(qosal_itf INTERFACE include/dummy)

  add_library(qosal INTERFACE)
  target_link_libraries(qosal INTERFACE qosal_itf)

endif()

qorvo_install_target(qosal_itf)
qorvo_install_target(qosal)
