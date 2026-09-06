# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

set(flag_for_qram_report ram)
set(flag_for_qrom_report rom)
set(flag_for_qfootprint all -q)
set(report_depth 99)

foreach(report qram_report qrom_report qfootprint)
  add_custom_target(
    ${report}
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_BASE}/scripts/footprint/size_report
    -k ${ZEPHYR_BINARY_DIR}/${KERNEL_ELF_NAME}
    -z ${CMAKE_CURRENT_LIST_DIR}/../../
    -o ${CMAKE_BINARY_DIR}
    --workspace=${ROOT_DIR}
    -d ${report_depth}
    -q
    --json ${report}.json
    ${flag_for_${report}}
    DEPENDS ${logical_target_for_zephyr_elf}
    $<TARGET_PROPERTY:zephyr_property_target,${report}_DEPENDENCIES>
    USES_TERMINAL
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endforeach()


add_custom_target(
  qpuncover
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/../../deps/puncover/runner.py
  --elf_file       ${ZEPHYR_BINARY_DIR}/${KERNEL_ELF_NAME}
  --gcc_tools_base ${CROSS_COMPILE}
  --src_root       ${CMAKE_CURRENT_LIST_DIR}/../../
  --build_dir      ${CMAKE_BINARY_DIR}
  --json           puncover.json
  DEPENDS ${logical_target_for_zephyr_elf}
  $<TARGET_PROPERTY:zephyr_property_target,${report}_DEPENDENCIES>
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
)

find_program(PAHOLE pahole)

if(NOT ${PAHOLE} STREQUAL PAHOLE-NOTFOUND)
  add_custom_target(
    qpahole
    ${PAHOLE}
    --anon_include
    --nested_anon_include
    --show_decl_info
    $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:--verbose>
    ${ZEPHYR_BINARY_DIR}/${KERNEL_ELF_NAME}
    ">pahole.txt"
    DEPENDS ${logical_target_for_zephyr_elf}
            $<TARGET_PROPERTY:zephyr_property_target,${report}_DEPENDENCIES>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
    )
endif()

add_custom_target(
  qmemreport
  ${PYTHON_EXECUTABLE}
  ${CMAKE_CURRENT_LIST_DIR}/../qmemreport/qmemreport.py
  --rom=qrom_report.json
  --ram=qram_report.json
  --puncover=puncover.json
  "$<$<TARGET_EXISTS:qpahole>:--pahole=pahole.txt>"
  DEPENDS qrom_report qram_report qpuncover $<TARGET_NAME_IF_EXISTS:qpahole>
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
)
