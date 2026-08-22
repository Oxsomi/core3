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

find_package(Vulkan QUIET)

set(graphicsAllowD3D12 OFF)
set(graphicsAllowVk OFF)

if(WIN32)
	if(DynamicLinkingGraphics)
		set(graphicsAllowD3D12 ON)
		if(Vulkan_FOUND)
			set(graphicsAllowVk ON)
		endif()
	elseif(NOT ForceVulkan)
		set(graphicsAllowD3D12 ON)
	elseif(Vulkan_FOUND)
		set(graphicsAllowVk ON)
	endif()
elseif(Vulkan_FOUND)
	set(graphicsAllowVk ON)
endif()

if(NOT graphicsAllowVk AND NOT graphicsAllowD3D12)
	message(FATAL_ERROR "-- No valid graphics API found, aborting...")
endif()

file(GLOB_RECURSE graphicsHeaders CONFIGURE_DEPENDS "../../include/graphics/generic/*.h")
file(GLOB_RECURSE graphicsSources CONFIGURE_DEPENDS "generic/*.c")

add_library(
	OxC3_graphics
	STATIC
	${graphicsHeaders}
	${graphicsSources}
	${shaders}
	CMakeLists.txt
)

if(DynamicLinkingGraphics)
	target_compile_definitions(OxC3_graphics PUBLIC -DGRAPHICS_API_DYNAMIC)
endif()

set_target_properties(OxC3_graphics PROPERTIES FOLDER Oxsomi/graphics)
target_link_libraries(OxC3_graphics PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_platforms OxC3_formats_oiSH OxC3_formats_oiSP)

include(../../cmake/oxc3.cmake)
add_virtual_files(
	TARGET OxC3_graphics NAME shaders
	ROOT ${CMAKE_CURRENT_SOURCE_DIR}/generic/shaders
	SELF ${CMAKE_CURRENT_SOURCE_DIR}/../..
	FORCE_PACKAGER
)

source_group("Shaders" FILES ${shaders})

add_library(
	OxC3_graphics_target		# Since dynamic linking might not define dependencies correctly
	INTERFACE
)

if(NOT DynamicLinkingGraphics)
	target_link_libraries(OxC3_graphics_target INTERFACE OxC3_graphics)
endif()

if(graphicsAllowVk)
	add_subdirectory(vulkan)
endif()

if(graphicsAllowD3D12)
	add_subdirectory(d3d12)
endif()

if(EnableTests)

	# Test shaders ship as their own package instead of polluting //OxC3_graphics.
	# OxC3_gtest is a plain custom target that only exists to own the package; target and name are capped at 15 chars.
	# Not gated on OxC3TestExecutables, since the android test app consumes this package too.

	add_custom_target(OxC3_gtest)
	set_target_properties(OxC3_gtest PROPERTIES FOLDER Oxsomi/test/graphics)

	add_virtual_files(
		TARGET OxC3_gtest NAME test_shaders
		ROOT ${CMAKE_CURRENT_SOURCE_DIR}/test/shaders
		SELF ${CMAKE_CURRENT_SOURCE_DIR}/../..
		FORCE_PACKAGER
	)

endif()

if(EnableTests AND OxC3TestExecutables)

	# Interface test: headless-safe checks always run, device tests skip without an adapter.
	# add_virtual_dependencies is required so device creation can load the //OxC3_graphics prebuilt shaders.

	include(../../cmake/oxc3.cmake)
	file(GLOB testsInterfaceC CONFIGURE_DEPENDS "test/interface/*.c")

	add_executable(
		OxC3_graphics_interface_test
		${testsInterfaceC}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_graphics_interface_test PUBLIC OxC3_graphics_target OxC3_platforms OxC3_types_test)
	set_target_properties(OxC3_graphics_interface_test PROPERTIES FOLDER Oxsomi/test/graphics)

	add_virtual_dependencies(TARGET OxC3_graphics_interface_test DEPENDENCIES OxC3_graphics OxC3_gtest)

	apply_dependencies(OxC3_graphics_interface_test)
	add_test(NAME OxC3_graphics_interface_test COMMAND OxC3_graphics_interface_test)

endif()
