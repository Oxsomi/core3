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

find_package(dxc REQUIRED)
find_package(spirv_reflect REQUIRED)

# Our parser & calling DXC and spirv-opt, spirv-reflect, DXIL reflect, etc.

file(GLOB_RECURSE compilerHeaders CONFIGURE_DEPENDS "../../include/shader_compiler/*.h")
file(GLOB_RECURSE compilerSources CONFIGURE_DEPENDS "*.c" "*.cpp")	# Some CPP to interface with DXC
list(FILTER compilerSources EXCLUDE REGEX "/test/")	# Test TUs build into their own target, not the lib

add_library(
	OxC3_shader_compiler
	STATIC
	${compilerHeaders}
	${compilerSources}
	CMakeLists.txt
)

set_target_properties(OxC3_shader_compiler PROPERTIES FOLDER Oxsomi/graphics)
target_link_libraries(OxC3_shader_compiler PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_platforms OxC3_formats_oiSH OxC3_formats_oiSR)
target_link_libraries(OxC3_shader_compiler PUBLIC dxc::dxc spirv_reflect::spirv_reflect)

# On Windows d3d12shader.h (included by DXC's dxcreflect.h) comes from the Windows SDK.
# Off Windows there is no such SDK, so pull it in headers-only from agility_sdk
# (an include-only package there), matching conanfile.py.
if(NOT WIN32)
	find_package(agility_sdk REQUIRED)
	target_link_libraries(OxC3_shader_compiler PUBLIC agility_sdk::agility_sdk)
endif()

if(EnableTests)

	# Compiles HLSL through the real DXC/SPIRV pipeline and validates parsing, reflection,
	# oxc:: annotations, extensions and disassembly. The test/hlsl corpus is exposed to the
	# test at build time via OXC3_SHADER_COMPILER_TEST_HLSL_DIR.

	file(GLOB shaderCompilerTestsC CONFIGURE_DEPENDS "test/*.c")

	# The HLSL corpus is test data, not compiled by the build; mark it header-only so it shows up
	# in the IDE (under an "hlsl" group) without CMake trying to invoke a shader compiler on it.
	file(GLOB_RECURSE shaderCompilerTestHlsl CONFIGURE_DEPENDS "test/hlsl/*.hlsl" "test/hlsl/*.hlsli")
	set_source_files_properties(${shaderCompilerTestHlsl} PROPERTIES HEADER_FILE_ONLY ON)
	source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl" PREFIX "hlsl" FILES ${shaderCompilerTestHlsl})

	add_executable(
		OxC3_shader_compiler_test
		${shaderCompilerTestsC}
		${shaderCompilerTestHlsl}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_shader_compiler_test PUBLIC OxC3_shader_compiler OxC3_platforms OxC3_types_test)

	# The corpus test reads/writes shaders + their reference oiSH relative to the working directory, so
	# run it from the corpus folder (OxC3 restricts file access to the working dir).
	# Set it for both the VS debugger and ctest.

	set_target_properties(OxC3_shader_compiler_test PROPERTIES
		FOLDER Oxsomi/test/graphics
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test"
	)

	add_test(
		NAME OxC3_shader_compiler_test
		COMMAND OxC3_shader_compiler_test
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test"
	)

endif()
