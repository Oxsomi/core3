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

# The built-in HLSL headers are ordinary HLSL, so an editor treats them as shaders rather than as the string
# literals they used to be, and the compiler still resolves #include "@types.hlsli" out of its own binary.
# Generating them into a translation unit of their own is what keeps editing one header from recompiling
# compiler.cpp and relinking the whole module.

file(GLOB builtInHlsli CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../include/shader_compiler/shaders/*.hlsli")

if(NOT Python3_EXECUTABLE)
	find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

set(builtInHlsliC "${CMAKE_CURRENT_BINARY_DIR}/oxc3_builtin_includes.c")
set(embedScript "${CMAKE_CURRENT_SOURCE_DIR}/../tools/embed_hlsli.py")

add_custom_command(
	OUTPUT "${builtInHlsliC}"
	COMMAND ${Python3_EXECUTABLE} "${embedScript}" -output "${builtInHlsliC}" ${builtInHlsli}
	DEPENDS ${builtInHlsli} "${embedScript}"
	COMMENT "Embedding the built-in HLSL headers"
	VERBATIM
)

# HEADER_FILE_ONLY so they show up to edit without the build trying to compile HLSL as C.

set_source_files_properties(${builtInHlsli} PROPERTIES HEADER_FILE_ONLY ON)
source_group("shaders" FILES ${builtInHlsli})

add_library(
	OxC3_shader_compiler
	${shaderCompilerLinkage}
	${compilerHeaders}
	${compilerSources}
	${builtInHlsli}
	"${builtInHlsliC}"
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

	# The HLSL corpus is test data, not compiled by the build; mark it header-only so it shows up
	# in the IDE (under an "hlsl" group) without CMake trying to invoke a shader compiler on it.
	file(GLOB_RECURSE shaderCompilerTestHlsl CONFIGURE_DEPENDS "test/hlsl/*.hlsl" "test/hlsl/*.hlsli")
	set_source_files_properties(${shaderCompilerTestHlsl} PROPERTIES HEADER_FILE_ONLY ON)
	source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/test/hlsl" PREFIX "hlsl" FILES ${shaderCompilerTestHlsl})

	# Everything the corpus test is built FROM, none of which is a source it links against: the HLSL corpus, the reference
	# oiSH it compares against, and the includes embedded into the compiler itself (types.hlsli and friends).
	# Editing any of them has to re-run the test, and none of them relinks the executable.

	file(GLOB shaderCompilerGoldens CONFIGURE_DEPENDS "test/*.oiSH")
	file(GLOB_RECURSE shaderCompilerEmbedded CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../include/shader_compiler/shaders/*.hlsli")

	# The corpus test reads/writes shaders + their reference oiSH relative to the working directory, so
	# run it from the corpus folder (OxC3 restricts file access to the working dir).

	include(../../cmake/oxc3.cmake)

	oxc3_add_test(
		NAME        OxC3_shader_compiler_test
		FOLDER      Oxsomi/test/graphics
		LIBS        OxC3_shader_compiler OxC3_platforms OxC3_types_test
		SOURCES     ${shaderCompilerTestHlsl}
		WORKING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test"
		DATA        ${shaderCompilerTestHlsl} ${shaderCompilerGoldens} ${shaderCompilerEmbedded}
	)

endif()
