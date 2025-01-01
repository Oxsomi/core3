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

message("-- Targeting audio API: OpenAL")
find_package(openal_soft REQUIRED)

file(GLOB_RECURSE graphicsHeaders "../../inc/audio/*.h")
file(GLOB graphicsSources "*.c")

file(GLOB_RECURSE apiPlatformSources "openal_soft/*.c")
file(GLOB_RECURSE apiPlatformIncludes "../../inc/audio/openal_soft/*.h")

add_library(
	OxC3_audio
	STATIC
	${graphicsHeaders}
	${graphicsSources}
	${apiPlatformSources}
	${apiPlatformIncludes}
	CMakeLists.txt
)

source_group("Source Files (openal_soft)" FILES ${apiPlatformSources})
source_group("Header Files (openal_soft)" FILES ${apiPlatformIncludes})

set_target_properties(OxC3_audio PROPERTIES FOLDER Oxsomi)
target_link_libraries(OxC3_audio PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_platforms openal_soft::openal_soft OxC3_formats_wav)
