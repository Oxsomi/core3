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

set(graphicsAllowD3D12 OFF)
set(graphicsAllowVk OFF)

if(WIN32)
	if(DynamicLinkingGraphics)
		set(graphicsAllowD3D12 ON)
		if(DEFINED ENV{VULKAN_SDK})
			set(graphicsAllowVk ON)
		endif()
	elseif(NOT ForceVulkan)
		set(graphicsAllowD3D12 ON)
	elseif(DEFINED ENV{VULKAN_SDK})
		set(graphicsAllowVk ON)
	endif()
elseif(DEFINED ENV{VULKAN_SDK})
	set(graphicsAllowVk ON)
endif()

if(NOT graphicsAllowVk AND NOT graphicsAllowD3D12)
	message(FATAL_ERROR "-- No valid graphics API found, aborting...")
endif()

file(GLOB_RECURSE graphicsHeaders "../../inc/graphics/generic/*.h")
file(GLOB_RECURSE graphicsSources "generic/*.c")

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
target_link_libraries(OxC3_graphics PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_platforms OxC3_formats_oiSH)

source_group("Shaders" FILES ${shaders})

add_library(OxC3_graphics_target INTERFACE)

if(graphicsAllowVk)
    add_subdirectory(vulkan)
endif()

if(graphicsAllowD3D12)
    add_subdirectory(d3d12)
endif()
