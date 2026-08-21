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

# SHARED folds DXC and SPIRV-Tools into one module instead of into every executable that links this.
# There's no runtime selection here (unlike graphics, which loads a backend and asks it for a table), so consumers link it the
# same way either way and nothing above this line changes.

if(DynamicLinkingShaderCompiler)
	set(shaderCompilerLinkage SHARED)
else()
	set(shaderCompilerLinkage STATIC)
endif()

add_library(
	OxC3_shader_compiler
	${shaderCompilerLinkage}
	${compilerHeaders}
	${compilerSources}
	CMakeLists.txt
)

set_target_properties(OxC3_shader_compiler PROPERTIES FOLDER Oxsomi/graphics)

if(DynamicLinkingShaderCompiler)

	# The public API is ~40 free functions and no exported data, so letting CMake write the .def is enough
	# and keeps __declspec out of the headers.
	# Everything else already defaults to visible; nothing here sets -fvisibility=hidden.

	set_target_properties(OxC3_shader_compiler PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
	target_compile_definitions(OxC3_shader_compiler PUBLIC -DSHADER_COMPILER_DYNAMIC)

	if(MSVC)

		# WINDOWS_EXPORT_ALL_SYMBOLS reads the .obj files to write the .def,
		# and neither MSVC's /GL nor clang-cl's -flto leaves COFF there ("unrecognized file format"),
		# so whole program optimisation has to go for this one target.
		# The root CMakeLists reads this property and simply doesn't add the flag,
		# rather than us overriding it afterwards with /GL-, which earns a D9025 per TU.
		# The cost is small: it only ever applied to our ~16 wrapper TUs, never to prebuilt DXC.

		set_target_properties(OxC3_shader_compiler PROPERTIES OXC3_NO_WHOLE_PROGRAM ON)
	endif()

endif()
target_link_libraries(OxC3_shader_compiler PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_platforms OxC3_formats_oiSH)
target_link_libraries(OxC3_shader_compiler PUBLIC dxc::dxc spirv_reflect::spirv_reflect)

# On Windows d3d12shader.h (included by DXC's dxcreflect.h) comes from the Windows SDK.
# Off Windows there is no such SDK, so pull it in headers-only from agility_sdk
# (an include-only package there), matching conanfile.py.
if(NOT WIN32)
	find_package(agility_sdk REQUIRED)
	target_link_libraries(OxC3_shader_compiler PUBLIC agility_sdk::agility_sdk)
endif()

if(EnableTests AND OxC3TestExecutables)

	# Compiles HLSL through the real DXC/SPIRV pipeline and validates parsing, reflection,
	# oxc:: annotations, extensions and disassembly.
	# The test/hlsl corpus is exposed to the test at build time via OXC3_SHADER_COMPILER_TEST_HLSL_DIR.

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

	# DXC's vendored clang allocates CGFunctionInfo with a trailing objects layout, one operator new
	# sized for the arguments that follow the object, and then destroys it through a plain delete on
	# the smaller declared type. ASan sees 128 bytes allocated against 32 deallocated and aborts.
	# The mismatch is real but it is theirs, and it cannot be suppressed by symbol the way a leak or an
	# odr violation can, so the check is turned off for the tests that link DXC and stays on everywhere
	# else. Every other ASan check remains active here.

	# The leak suppressions are the same ones the build uses, DXC and SPIRV-Reflect leaving allocations
	# to process exit, suppressed by library entry point so a leak in OxC3's own code still reports.

	# Appended rather than assigned: the caller sets options of its own that the whole suite depends on,
	# and a plain ENVIRONMENT would replace them instead of adding to them.

	if(EnableASAN)
		set_tests_properties(OxC3_shader_compiler_test PROPERTIES ENVIRONMENT_MODIFICATION
			"ASAN_OPTIONS=string_append:,new_delete_type_mismatch=0;LSAN_OPTIONS=string_append:,suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan_suppressions.txt")
	endif()

endif()
