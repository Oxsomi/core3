# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/rt_core/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

# CLI, useful for manually calling CLI utility functions

file(GLOB headers CONFIGURE_DEPENDS "../.../../include/tools/oxc3_cli/*.h")
file(GLOB sources CONFIGURE_DEPENDS "*.c")

add_library(
	OxC3CLI STATIC
	${headers}
	${sources}
	CMakeLists.txt
)

set_target_properties(OxC3CLI PROPERTIES FOLDER Oxsomi/CLI)
target_link_libraries(
	OxC3CLI PUBLIC
	OxC3_types_base OxC3_types_math OxC3_types_container
	OxC3_platforms
	OxC3_formats_oiSH OxC3_formats_oiSB OxC3_formats_oiCA OxC3_formats_oiDL OxC3_formats_wav
	OxC3_formats_bmp OxC3_formats_dds
	OxC3_audio
)

if(CLIGraphics)
	target_link_libraries(OxC3CLI PUBLIC OxC3_graphics_target)
	target_compile_definitions(OxC3CLI PUBLIC -DCLI_GRAPHICS)
endif()

if(EnableShaderCompiler)
	target_link_libraries(OxC3CLI PUBLIC OxC3_shader_compiler OxC3_package_CLI)
	target_compile_definitions(OxC3CLI PUBLIC -DCLI_SHADER_COMPILER)
endif()

target_include_directories(OxC3CLI PUBLIC include/tools/oxc3_cli)

# OxC3 executable, for easily executing OxC3CLI commands

file(GLOB sources CONFIGURE_DEPENDS "executable/*.c")

add_executable(
	OxC3
	${sources}
	CMakeLists.txt
)

set_target_properties(OxC3 PROPERTIES FOLDER Oxsomi/CLI)
target_link_libraries(OxC3 PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3CLI)

include(../../../cmake/oxc3.cmake)
configure_icon(OxC3 "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
add_virtual_dependencies(TARGET OxC3 DEPENDENCIES OxC3_graphics)
apply_dependencies(OxC3)

# Done automatically by apply_dependencies in things that use OxC3 as a conan package
# However, OxC3 itself has to manually copy .so file to avoid this problem

if(UNIX AND TARGET OxC3_graphics_vk)

	# $<TARGET_FILE_DIR:OxC3> instead of CMAKE_RUNTIME_OUTPUT_DIRECTORY; the latter can be empty when only the
	# per-config variables are set, which turned the command into "cmake -E copy <file>" (no destination).
	add_custom_command(
		TARGET OxC3 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:OxC3_graphics_vk> $<TARGET_FILE_DIR:OxC3>
	)

endif()

# Functional CLI test (ctest): drives the built OxC3 binary through tools/test.py,
# asserting on real command output (known hashes, round-trips, virtual "//" files, -oiCA operations, negative cases).
# Runs from the binary directory so embedded virtual sections resolve.

if(EnableTests AND OxC3TestExecutables)

	if(NOT Python3_EXECUTABLE)
		find_package(Python3 COMPONENTS Interpreter)
	endif()

	if(Python3_EXECUTABLE)

		include(../../../cmake/oxc3.cmake)

		oxc3_add_test_command(
			NAME        OxC3_cli_test
			TARGET      OxC3
			FOLDER      Oxsomi/test/tools
			COMMAND     ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test.py $<TARGET_FILE_DIR:OxC3>
			WORKING_DIR $<TARGET_FILE_DIR:OxC3>
			DATA        ${CMAKE_SOURCE_DIR}/tools/test.py
		)

		# Drives DXC through the CLI, so it hits the same CGFunctionInfo new/delete size mismatch the
		# shader compiler test documents.

		if(EnableASAN)
			oxc3_test_env(OxC3_cli_test
				"ASAN_OPTIONS=string_append:,new_delete_type_mismatch=0"
				"LSAN_OPTIONS=string_append:,suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan_suppressions.txt"
			)
		endif()

		# Drives the graphics device commands too, so it needs the same driver accommodations.

		oxc3SanitizerDeviceTestEnv(OxC3_cli_test)
	else()
		message(WARNING "OxC3_cli_test skipped: Python3 interpreter not found")
	endif()

endif()

