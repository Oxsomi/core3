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

add_subdirectory(types)
add_subdirectory(formats)

if(EnableTests AND OxC3TestExecutables)
	add_subdirectory(tools/package_simple)		# Required to create oiCAs to attach as virtual files
endif()

add_subdirectory(platforms)

if(EnableShaderCompiler)
	add_subdirectory(shader_compiler)
	add_subdirectory(tools/package_cli)			# OxC3_graphics uses the packager
endif()

add_subdirectory(graphics)
add_subdirectory(audio)
add_subdirectory(tools)

# Test bundles for platforms that have no exec, so the per-suite executables the other platforms run
# through ctest can't be used there. Android today; wasm will want the same shape.

if(EnableTests AND NOT OxC3TestExecutables)
	if(ANDROID)
		add_subdirectory(test/android)
	else()
		message(FATAL_ERROR "No test bundle for this platform yet, add one under src/test/")
	endif()
endif()
