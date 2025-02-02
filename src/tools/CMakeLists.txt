# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

file(GLOB headers "../../include/tools/*.h")
file(GLOB sources "*.c")

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
	OxC3_audio
)

if(CLIGraphics)
	target_link_libraries(OxC3CLI PUBLIC OxC3_graphics_target)
	target_compile_definitions(OxC3CLI PUBLIC -DCLI_GRAPHICS)
endif()

if(EnableShaderCompiler)
	target_link_libraries(OxC3CLI PUBLIC OxC3_shader_compiler)
	target_compile_definitions(OxC3CLI PUBLIC -DCLI_SHADER_COMPILER)
endif()

target_include_directories(OxC3CLI PUBLIC inc/tools)

# OxC3 executable, for easily executing OxC3CLI commands

file(GLOB sources "executable/*.c")

add_executable(
	OxC3
	${sources}
	CMakeLists.txt
)

set_target_properties(OxC3 PROPERTIES FOLDER Oxsomi/CLI)
target_link_libraries(OxC3 PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3CLI)

include(../../cmake/oxc3.cmake)
configure_icon(OxC3 "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
configure_virtual_files(OxC3)
apply_dependencies(OxC3)

# Done automatically by apply_dependencies in things that use OxC3 as a conan package
# However, OxC3 itself has to manually copy .so file to avoid this problem

if(UNIX AND TARGET OxC3_graphics_vk)

	add_custom_command(
		TARGET OxC3 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:OxC3_graphics_vk> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	)

endif()

