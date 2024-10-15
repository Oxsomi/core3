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

set(AllowOxC3PlatformsSH ON)

file(GLOB formatsSHHeaders "../../../inc/formats/oiSH/*.h")
file(GLOB formatsSHSources "*.c")

add_library(
	OxC3_formats_oiSH
	STATIC
	${formatsSHHeaders}
	${formatsSHSources}
	CMakeLists.txt
)

set_target_properties(OxC3_formats_oiSH PROPERTIES FOLDER Oxsomi/formats)
target_link_libraries(OxC3_formats_oiSH PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_formats_oiSB)

if(AllowOxC3PlatformsSH)
	target_link_libraries(OxC3_formats_oiSH PUBLIC OxC3_platforms)
else()
	target_compile_definitions(OxC3_formats_oiSH PUBLIC -DDISALLOW_SH_OXC3_PLATFORMS)
endif()
