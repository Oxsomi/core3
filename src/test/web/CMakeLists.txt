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

# One wasm64 module holding every unit test suite, run under node.
#
# Same recompile-with-_OXC3_TEST_BUNDLED trick as test/android (see that CMakeLists for the why).
# But node has a real exec: this is an ordinary executable with an exit code,
# wired into ctest through CMAKE_CROSSCOMPILING_EMULATOR (the emscripten toolchain sets it to node).
#
# Bundled here but not on android: shader_compiler - DXC does build for wasm64.
# Not bundled: graphics_interface (no GPU under node) and the functional suites (need a human).

set(testRoot ${CMAKE_CURRENT_SOURCE_DIR}/../..)

file(GLOB_RECURSE webTestSources CONFIGURE_DEPENDS
	"${testRoot}/types/base/test/*.c"
	"${testRoot}/types/math/test/*.c"
	"${testRoot}/types/math/test/*.cpp"
	"${testRoot}/types/container/test/*.c"
	"${testRoot}/types/container/test/*.cpp"
	"${testRoot}/formats/*/test/*.c"
	"${testRoot}/audio/test/interface/*.c"
	"${testRoot}/platforms/test/interface/*.c"
)

file(GLOB webShaderTestSources CONFIGURE_DEPENDS "${testRoot}/shader_compiler/test/*.c")

add_executable(OxC3_wtest
	${webTestSources}
	${webShaderTestSources}
	wtest_main.c
	CMakeLists.txt
)

# _OXC3_TEST_VFS_TARGET: platforms_interface reaches its test data through the virtual file system,
# namespaced by the target that packaged it (see test/android/CMakeLists.txt).

target_compile_definitions(OxC3_wtest PRIVATE _OXC3_TEST_BUNDLED _OXC3_TEST_VFS_TARGET="OxC3_wtest")

target_link_libraries(OxC3_wtest PUBLIC
	OxC3_platforms
	OxC3_audio
	OxC3_shader_compiler
	OxC3_formats_bmp OxC3_formats_dds OxC3_formats_oiBC OxC3_formats_oiCA
	OxC3_formats_oiDL OxC3_formats_oiSB OxC3_formats_oiSH OxC3_formats_wav
	OxC3_types_test
	OxC3_types_container_test_util
)

# Node-targeted link: NODERAWFS mounts the real filesystem,
# so packages/ resolves through plain POSIX paths from the working directory.
# The memory/stack settings are hard requirements (DXC recursion overflows the 64KB default stack silently).
# INITIAL_MEMORY is what the module commits at instantiation, before it does any work.
# Keep it small and let ALLOW_MEMORY_GROWTH size it to the workload:
# a phone should not have to hand over half a gigabyte just to load the module.
# DXC grows this a lot while compiling, but it grows on demand and only for the run that needs it.

target_link_options(OxC3_wtest PRIVATE
	"-sENVIRONMENT=node"
	"-sNODERAWFS=1"
	"-sEXIT_RUNTIME=1"
	"-sALLOW_MEMORY_GROWTH=1"
	"-sINITIAL_MEMORY=32MB"
	"-sMAXIMUM_MEMORY=16GB"
	"-sSTACK_SIZE=8MB"
)

# Test_dynamicLibrary dlopens this by bare name on other platforms.
# The web run expects the load to fail (no dlopen without MAIN_MODULE),
# but the target must still exist for the dependency.

add_dependencies(OxC3_wtest OxC3_platforms_interface_dylib_test)

include(${testRoot}/../cmake/oxc3.cmake)

add_virtual_files(
	TARGET  OxC3_wtest
	NAME    testdata
	ROOT    ${testRoot}/platforms/test/interface/res
	SELF    ${testRoot}/..
	FORCE_PACKAGER
)

# The shader corpus, reached through the virtual file system rather than a working directory.
# TEST_SHADER_ROOT (shader_compiler/test/test_shader_compiler_shared.h) resolves to //<vfs target>/shaderdata/
# whenever the suites are bundled, so a bundled build MUST package this section
# or every compileFileShader() call fails to read its source.
# -raw keeps the .hlsl files as they are instead of compiling them during packaging.
# Same shape as test/android.

if(EnableShaderCompiler)

	set(shaderRes ${CMAKE_CURRENT_BINARY_DIR}/shaderdata)

	file(GLOB_RECURSE shaderTestData CONFIGURE_DEPENDS
		"${testRoot}/shader_compiler/test/*/*.hlsl"
		"${testRoot}/shader_compiler/test/*/*.hlsli"
		"${testRoot}/shader_compiler/test/*.oiSH"
	)

	foreach(f ${shaderTestData})
		file(RELATIVE_PATH rel "${testRoot}/shader_compiler/test" "${f}")
		get_filename_component(relDir "${rel}" DIRECTORY)
		file(COPY "${f}" DESTINATION "${shaderRes}/${relDir}")
	endforeach()

	add_virtual_files(
		TARGET  OxC3_wtest
		NAME    shaderdata
		ROOT    ${shaderRes}
		SELF    ${testRoot}/..
		FORCE_PACKAGER
		ARGS    -raw
	)

endif()

set_target_properties(OxC3_wtest PROPERTIES FOLDER Oxsomi/test)

apply_dependencies(OxC3_wtest)

# ctest: CMAKE_CROSSCOMPILING_EMULATOR (node) runs the module.
# Suites that read data get their working directory from -packages:
# packages/ lives under <SELF>/build/web (add_virtual_files, no config component for single-config generators).
# The shader corpus is reached through the virtual file system rather than a source dir,
# so it needs the same directory, not its own.

add_test(
	NAME OxC3_wtest
	COMMAND OxC3_wtest
		-packages "${testRoot}/../build/$<CONFIG>/web"
)
