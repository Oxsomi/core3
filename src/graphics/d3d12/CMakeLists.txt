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

message("-- Enabling Direct3D12 support...")

file(GLOB_RECURSE graphicsApiHeaders "../../../inc/graphics/d3d12/*.h")
file(GLOB_RECURSE graphicsApiSources "generic/*.c")

set(TARGET_NAME OxC3_graphics_d3d12)

if(NOT DynamicLinkingGraphics)

	set(TARGET_NAME OxC3_graphics)
	message("-- Targeting graphics api: ${graphicsApi} on platform ${platform}")

	target_compile_definitions(OxC3_graphics PUBLIC -D_GRAPHICS_API=GRAPHICS_API_D3D12)

	target_sources(OxC3_graphics PUBLIC ${graphicsApiHeaders})
	target_sources(OxC3_graphics PRIVATE ${graphicsApiSources})
	
	source_group("Source Files (${graphicsApi})" FILES ${graphicsApiSources})
	source_group("Header Files (${graphicsApi})" FILES ${graphicsApiSources})

else()
	add_library(
		OxC3_graphics_d3d12
		SHARED
		${graphicsApiHeaders}
		${graphicsApiSources}
		CMakeLists.txt
	)
endif()

target_link_libraries(${TARGET_NAME} PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container)

if(DynamicLinkingGraphics)
	target_link_libraries(OxC3_graphics_d3d12 PUBLIC OxC3_graphics)
endif()

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER Oxsomi/graphics)

find_package(agility_sdk REQUIRED)

target_link_libraries(${TARGET_NAME} PUBLIC agility_sdk::agility_sdk)
target_include_directories(${TARGET_NAME} PUBLIC "${CMAKE_BINARY_DIR}/external/agility/build/native/include")

if(NOT ${ARM})

	# AMD AGS and NVAPI are required because Direct3D12 was apparently not good enough :/
	# These are not supported for ARM, so don't include them there

	# NVAPI, used for NV extensions and getting driver version

	find_package(ags REQUIRED)
	find_package(nvapi REQUIRED)

	target_link_libraries(${TARGET_NAME} PUBLIC ags::ags nvapi::nvapi)

endif()

