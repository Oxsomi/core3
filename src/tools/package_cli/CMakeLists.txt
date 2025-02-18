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

# Package CLI, useful for manually calling CLI utility functions

file(GLOB headers "../../../include/tools/package_cli/*.h")
file(GLOB sources "*.c")

add_library(
	OxC3_package_CLI STATIC
	${headers}
	${sources}
	CMakeLists.txt
)

set_target_properties(OxC3_package_CLI PROPERTIES FOLDER Oxsomi/CLI)
target_link_libraries(
	OxC3_package_CLI PUBLIC
	OxC3_types_base OxC3_types_math OxC3_types_container
	OxC3_platforms
	OxC3_formats_oiCA
)

if(EnableShaderCompiler)
	target_link_libraries(OxC3_package_CLI PUBLIC OxC3_shader_compiler)
	target_compile_definitions(OxC3_package_CLI PUBLIC -DCLI_SHADER_COMPILER)
endif()

if(DynamicLinkingGraphics)
	target_compile_definitions(OxC3_package_CLI PUBLIC -DGRAPHICS_API_DYNAMIC)
else()
	if(WIN32 AND ForceVulkan)
	target_compile_definitions(OxC3_package_CLI PUBLIC -DFORCE_VULKAN)
	endif()
endif()

target_include_directories(OxC3_package_CLI PUBLIC include/tools/package_cli)

# OxC3_package executable, for easily executing OxC3_package_CLI commands

file(GLOB sources "executable/*.c")

add_executable(
	OxC3_package
	${sources}
	CMakeLists.txt
)

set_target_properties(OxC3_package PROPERTIES FOLDER Oxsomi/CLI)
target_link_libraries(OxC3_package PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_package_CLI)

include(../../../cmake/oxc3.cmake)
configure_icon(OxC3_package "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
configure_virtual_files(OxC3_package)
apply_dependencies(OxC3_package)
