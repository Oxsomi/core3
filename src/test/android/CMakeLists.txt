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

# Extra suites android bundles on top of the shared set: it has a GPU, and its functional suites are
# gated behind debug.oxc3.interactive rather than dropped.

file(GLOB_RECURSE androidExtraSources CONFIGURE_DEPENDS
	"${testRoot}/audio/test/functional/*.c"
	"${testRoot}/audio/test/functional/*.cpp"
	"${testRoot}/platforms/test/functional/*.c"
	"${testRoot}/platforms/test/functional/*.cpp"
	"${testRoot}/graphics/test/interface/*.c"
	"${testRoot}/graphics/test/interface/*.cpp"
)

set(androidExtraLibs OxC3_graphics)

if(EnableShaderCompiler)

	file(GLOB shaderCompilerTestSources CONFIGURE_DEPENDS
		"${testRoot}/shader_compiler/test/*.c"
		"${testRoot}/shader_compiler/test/*.cpp"
	)

	list(APPEND androidExtraSources ${shaderCompilerTestSources})
	list(APPEND androidExtraLibs OxC3_shader_compiler)
endif()

include(${testRoot}/../cmake/oxc3.cmake)

oxc3_add_bundled_test(
	TARGET  OxC3_atest
	ROOT    ${testRoot}
	SELF    ${testRoot}/..
	ENTRY   atest_main.c
	SHARED
	SOURCES ${androidExtraSources}
	LIBS    ${androidExtraLibs}
)

if(EnableShaderCompiler)
	target_compile_definitions(OxC3_atest PRIVATE _OXC3_TEST_SHADER_COMPILER)
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

# The graphics shader execution tests read //OxC3_gtest/test_shaders; the dependency is what makes the
# package build at all, build_android.py then sweeps every packages/ directory into the apk's assets.

if(TARGET OxC3_gtest)
	add_virtual_dependencies(TARGET OxC3_atest DEPENDENCIES OxC3_gtest)
endif()

apply_dependencies(OxC3_atest)
