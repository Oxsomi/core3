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

find_package(dxc REQUIRED)
find_package(nvapi REQUIRED)
find_package(spirv_reflect REQUIRED)

# Our parser & calling DXC and spirv-opt, spirv-reflect, DXIL reflect, etc.

file(GLOB_RECURSE compilerHeaders "../../include/shader_compiler/*.h")
file(GLOB_RECURSE compilerSources "*.c" "*.cpp")	# Some CPP to interface with DXC

add_library(
	OxC3_shader_compiler
	STATIC
	${compilerHeaders}
	${compilerSources}
	CMakeLists.txt
)

set_target_properties(OxC3_shader_compiler PROPERTIES FOLDER Oxsomi/graphics)
target_link_libraries(OxC3_shader_compiler PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_platforms OxC3_formats_oiSH)
target_link_libraries(OxC3_shader_compiler PUBLIC dxc::dxc spirv_reflect::spirv_reflect nvapi::nvapi)
