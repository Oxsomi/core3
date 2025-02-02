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

file(GLOB_RECURSE typesContainerHeaders "../../../include/types/container/*.h")
file(GLOB typesContainerSources "*.c")
file(GLOB_RECURSE typesContainerPlatformSources "platforms/${platform}/*.c")
file(GLOB_RECURSE typesContainerSIMDSources "simd/${simd}/*.c")

if(UNIX)
	file(GLOB_RECURSE typesContainerUnixSources "platforms/unix/*.c")
endif()

add_library(
	OxC3_types_container
	STATIC
	${typesContainerHeaders}
	${typesContainerSources}
	${typesContainerPlatformSources}
	${typesContainerSIMDSources}
	${typesContainerUnixSources}
	CMakeLists.txt
)

source_group("Source Files (${platform})" FILES ${typesContainerPlatformSources})
source_group("Source Files (simd: ${simd})" FILES ${typesContainerSIMDSources})
source_group("Source Files (unix)" FILES ${typesContainerUnixSources})

set_target_properties(OxC3_types_container PROPERTIES FOLDER Oxsomi/types)
target_link_libraries(OxC3_types_container PUBLIC OxC3_types_base OxC3_types_math)

target_compile_definitions(OxC3_types_container PUBLIC -D_ENABLE_SIMD=${SIMD})

if(WIN32)
	target_link_libraries(OxC3_types_container PUBLIC Bcrypt.lib)
elseif(APPLE)
	target_link_libraries(OxC3_types_container PUBLIC "-framework Security")
elseif(ANDROID)
	target_link_libraries(OxC3_types_container PUBLIC android log)
endif()
