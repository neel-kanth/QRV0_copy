# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

set(SANITIZE -fsanitize=undefined -fsanitize=leak -fsanitize=address
	-fno-sanitize-recover=all)

# Add sanitize build flags to a target.
function(target_sanitize target)
	if(NOT DISABLE_SANITIZE)
		target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:${SANITIZE}>)
		target_link_options(${target} PUBLIC $<$<CONFIG:Debug>:${SANITIZE}>)
	endif()
endfunction()
