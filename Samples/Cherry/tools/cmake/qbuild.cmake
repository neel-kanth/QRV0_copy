# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

include_guard()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Defines common compilation options
add_compile_options(
  -Werror
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Wno-missing-field-initializers
  -Wno-error=deprecated-declarations
  -Wundef
  $<$<COMPILE_LANGUAGE:C>:-Wdeclaration-after-statement>
)

# Declare custom linker snippets using the appropriate section identifier, DATA_SECTIONS for RAM
# structures and SECTIONS for ROM ones
function(qlinker_sources)
  message(FATAL_ERROR "No linker sources defined for your project!")
endfunction()
