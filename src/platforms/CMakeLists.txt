# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

message("-- Targeting platform: ${platform}")

file(GLOB platformsHeaders "../../inc/platforms/*.h" "../../inc/platforms/ext/*.h")

file(GLOB_RECURSE platformsPlatformSources "${platform}/*.c")
file(GLOB_RECURSE platformsPlatformIncludes "../../inc/platforms/${platform}/*.h")
file(GLOB_RECURSE platformsSources "generic/*.c")
file(GLOB_RECURSE platformsSIMDSources "simd/${simd}/*.c")

if(UNIX)
	file(GLOB_RECURSE platformsUnixSources "unix/*.c")
	list(APPEND platformsSources ${platformsUnixSources})
endif()

add_library(
	OxC3_platforms
	STATIC
	${platformsHeaders}
	${platformsPlatformSources}
	${platformsPlatformIncludes}
	${platformsSources}
	${platformsSIMDSources}
	CMakeLists.txt
)

source_group("Source Files (${platform})" FILES ${platformsPlatformSources})
source_group("Header Files (${platform})" FILES ${platformsPlatformIncludes})
source_group("Source Files (simd: ${simd})" FILES ${platformsSIMDSources})

set_target_properties(OxC3_platforms PROPERTIES FOLDER Oxsomi)

target_link_libraries(
	OxC3_platforms PUBLIC
	OxC3_types_base OxC3_types_math OxC3_types_container
	OxC3_formats_oiXX OxC3_formats_oiCA OxC3_formats_oiDL OxC3_formats_dds
)

if(APPLE)
	target_link_libraries(OxC3_platforms PUBLIC "-framework CoreFoundation" "-framework ApplicationServices" "-framework AppKit")
elseif(UNIX AND NOT APPLE)
	find_package(xdg_shell REQUIRED)
	find_package(xdg_decoration REQUIRED)
	target_link_libraries(OxC3_platforms PUBLIC xdg_shell::xdg_shell xdg_decoration::xdg_decoration)
endif()
