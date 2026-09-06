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
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

# Package CLI, useful for manually calling CLI utility functions

file(GLOB headers CONFIGURE_DEPENDS "../../../include/tools/package_cli/*.h")
file(GLOB sources CONFIGURE_DEPENDS "*.c")

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

# OxC3_package executable, for easily executing OxC3_package_CLI commands.
#
# Not on android: Platform_defineEntrypoint gives android_main rather than main there, so an executable
# doesn't link (ld.lld: undefined symbol: main), and an apk couldn't run one anyway.
# Not on web either: it links, but the build machine can't exec the resulting .js, and node users get the
# same commands from OxC3.js (which links OxC3_package_CLI), so the executable would only repeat the DXC
# sized link with nobody to run it.
# Nothing is lost by skipping it, since a cross build packages its virtual files with the host
# OxC3_package it tool_requires; the library above is still built and linked into the app.

if(NOT ANDROID AND NOT EMSCRIPTEN)

	file(GLOB sources CONFIGURE_DEPENDS "executable/*.c")

	add_executable(
		OxC3_package
		${sources}
		CMakeLists.txt
	)

	set_target_properties(OxC3_package PROPERTIES FOLDER Oxsomi/CLI)
	target_link_libraries(OxC3_package PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_package_CLI)

	include(../../../cmake/oxc3.cmake)
	configure_icon(OxC3_package "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
	apply_dependencies(OxC3_package)

endif()
