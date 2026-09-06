# SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
# SPDX-License-Identifier: LicenseRef-QORVO-2

find_program(LCOV lcov REQUIRED)
find_program(GENHTML genhtml REQUIRED)
set(COVERAGE_EXCLUDE /usr/* */deps/* */utest/*)

# find configuration file path
find_path(COVERAGE_DIR "lcovrc" HINTS ${CMAKE_CURRENT_LIST_DIR})
if (${COVERAGE_DIR} MATCHES COVERAGE_DIR-NOTFOUND)
	message(FATAL_ERROR "Couldn't locate lcov configuration file")
endif()

# Add coverage build flags to a target.
function(target_coverage target)
	target_compile_options(${target} PRIVATE --coverage)
	target_link_options(${target} PUBLIC --coverage)
endfunction()

# Add a coverage target.
function(add_coverage)
	set(options GTEST_JUNIT)
	set(oneValueArgs NAME)
	set(multiValueArgs COMMAND EXCLUDE)
	cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")
	if(NOT DEFINED ARG_NAME)
		message(FATAL_ERROR "no NAME given")
	endif()
	if(NOT DEFINED ARG_COMMAND)
		set(ARG_COMMAND ${ARG_NAME})
	endif()
	if(DEFINED ARG_GTEST_JUNIT)
		set(ARG_COMMAND ${ARG_COMMAND}
			--gtest_output=xml:${ARG_NAME}_report.xml)
	endif()
	set(CONFIGFILE --config-file ${COVERAGE_DIR}/lcovrc)
	set(LCOVFLAGS ${CONFIGFILE})
	set(EXCLUDE ${COVERAGE_EXCLUDE} ${ARG_EXCLUDE} --ignore-errors unused)
	add_custom_command(OUTPUT ${ARG_NAME}.info
		COMMAND ${LCOV} ${LCOVFLAGS} --directory . --zerocounters
		COMMAND ${LCOV} ${LCOVFLAGS} --directory . --capture --initial --output-file ${ARG_NAME}_base.info
		COMMAND ${ARG_COMMAND}
		COMMAND ${LCOV} ${LCOVFLAGS} --directory . --capture --output-file ${ARG_NAME}_test.info
		COMMAND ${LCOV} ${LCOVFLAGS} -q --add-tracefile ${ARG_NAME}_base.info --add-tracefile ${ARG_NAME}_test.info --output-file ${ARG_NAME}_total.info
		COMMAND ${LCOV} ${LCOVFLAGS} --remove ${ARG_NAME}_total.info ${EXCLUDE} --output-file ${ARG_NAME}.info
		BYPRODUCTS
			${ARG_NAME}_base.info
			${ARG_NAME}_test.info
			${ARG_NAME}_total.info
			${ARG_NAME}_report.xml
		DEPENDS ${ARG_NAME}
		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
		VERBATIM
		)
	add_custom_command(OUTPUT ${ARG_NAME}_coverage_html.stamp
		COMMAND ${GENHTML} ${CONFIGFILE} -q --output-directory ${ARG_NAME}_coverage_html ${ARG_NAME}.info
		COMMAND touch ${ARG_NAME}_coverage_html.stamp
		BYPRODUCTS
			${ARG_NAME}_coverage_html
		DEPENDS ${ARG_NAME}.info
		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
		VERBATIM
		COMMENT "Generating HTML coverage output"
		)
	add_custom_target(${ARG_NAME}_coverage
		DEPENDS ${ARG_NAME}_coverage_html.stamp
		)
endfunction()
