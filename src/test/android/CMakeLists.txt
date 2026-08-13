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

# One .so holding every unit test suite, loaded by a NativeActivity.
#
# Android has no exec, so the per-suite executables ctest runs elsewhere are useless here.
# This target compiles the same test sources a second time with _OXC3_TEST_BUNDLED,
# which turns each suite's main() into a named entry (see OXC3_TEST_MAIN / OXC3_TEST_ENTRY in types/test/test.h)
# that atest_main.c calls in turn.
# Compiling them twice is deliberate: every module's own test target stays untouched, so this can't affect the host build.
#
# shader_compiler is bundled only when it was built (-shader_compiler True), since DXC is off by default on android.
# Its corpus test skips itself there, see test_shader_compiler_main.c.
#
# The functional suites ARE bundled: they need a human at the device,
# so atest_main only runs them when debug.oxc3.interactive is set.

set(testRoot ${CMAKE_CURRENT_SOURCE_DIR}/../..)

file(GLOB_RECURSE androidTestSources CONFIGURE_DEPENDS
	"${testRoot}/types/base/test/*.c"
	"${testRoot}/types/math/test/*.c"
	"${testRoot}/types/math/test/*.cpp"
	"${testRoot}/types/container/test/*.c"
	"${testRoot}/types/container/test/*.cpp"
	"${testRoot}/formats/*/test/*.c"
	"${testRoot}/audio/test/interface/*.c"
	"${testRoot}/audio/test/functional/*.c"
	"${testRoot}/platforms/test/functional/*.c"
	"${testRoot}/graphics/test/interface/*.c"
	"${testRoot}/platforms/test/interface/*.c"
)

if(EnableShaderCompiler)
	file(GLOB shaderCompilerTestSources CONFIGURE_DEPENDS "${testRoot}/shader_compiler/test/*.c")
	list(APPEND androidTestSources ${shaderCompilerTestSources})
endif()

add_library(OxC3_atest SHARED
	${androidTestSources}
	atest_main.c
	CMakeLists.txt
)

# _OXC3_TEST_VFS_TARGET: platforms_interface reaches its test data through the virtual file system, and a
# section is namespaced by the target that packaged it. add_virtual_files below registers it under
# OxC3_atest, not OxC3_plinttst as the desktop build does, so the suite is told which name to expect.

target_compile_definitions(OxC3_atest PRIVATE _OXC3_TEST_BUNDLED _OXC3_TEST_VFS_TARGET="OxC3_atest")

if(EnableShaderCompiler)
	target_link_libraries(OxC3_atest PUBLIC OxC3_shader_compiler)
	target_compile_definitions(OxC3_atest PRIVATE _OXC3_TEST_SHADER_COMPILER)
endif()

target_link_libraries(OxC3_atest PUBLIC
	OxC3_platforms
	OxC3_graphics
	OxC3_audio
	OxC3_formats_bmp OxC3_formats_dds OxC3_formats_oiBC OxC3_formats_oiCA
	OxC3_formats_oiDL OxC3_formats_oiSB OxC3_formats_oiSH OxC3_formats_wav
	OxC3_types_test
	OxC3_types_container_test_util
)

# Test_dynamicLibrary dlopens this by bare name; it only has to end up in the apk's lib/<abi>/, which
# build_android.py fills from every .so the build produced, so a dependency is all that's needed.

add_dependencies(OxC3_atest OxC3_platforms_interface_dylib_test)

# platforms_interface reads these through the virtual file system, same as OxC3_plinttst does

include(${testRoot}/../cmake/oxc3.cmake)

add_virtual_files(
	TARGET  OxC3_atest
	NAME    testdata
	ROOT    ${testRoot}/platforms/test/interface/res
	SELF    ${testRoot}/..
	FORCE_PACKAGER
)

# shader_compiler reads its HLSL off disk when ctest runs it from src/shader_compiler/test; an apk has no
# such working directory, so the same tree is bundled and the suite reads it back through //OxC3_atest.
# -raw is what keeps it readable: the packager otherwise compiles .hlsl into oiSH, and these tests want
# the source (they drive the compiler themselves).
# The .c/.h next to it aren't staged, since add_virtual_files packages a whole directory.

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
		TARGET  OxC3_atest
		NAME    shaderdata
		ROOT    ${shaderRes}
		SELF    ${testRoot}/..
		FORCE_PACKAGER
		ARGS    -raw
	)

endif()

# audio_functional reads its wavs through the virtual file system here, since an apk has no working directory to fopen out of.
# Only the short_* clips are bundled, the long_* ones are 79 of the 88 MB and would turn a ~11 MB test apk into a ~99 MB one.
# The sweeps fall back to a looping short clip, only the tests that genuinely need a long one are skipped (and say so).
# They're staged into a separate folder because add_virtual_files packages a whole directory, and theirs holds .c/.h too.

set(audioRes ${CMAKE_CURRENT_BINARY_DIR}/audiodata)
file(GLOB audioShortWavs CONFIGURE_DEPENDS "${testRoot}/audio/test/functional/short_*.wav")
file(COPY ${audioShortWavs} DESTINATION ${audioRes})

add_virtual_files(
	TARGET  OxC3_atest
	NAME    audiodata
	ROOT    ${audioRes}
	SELF    ${testRoot}/..
	FORCE_PACKAGER
)

set_target_properties(OxC3_atest PROPERTIES FOLDER Oxsomi/test)

apply_dependencies(OxC3_atest)
