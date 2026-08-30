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
# The bundle itself is oxc3_add_bundled_test (cmake/oxc3.cmake), shared with test/android; that is where the
# recompile-with-_OXC3_TEST_BUNDLED trick and the suites every bundle takes are explained.
# What is web's own is here: node has a real exec, so this is an ordinary executable with an exit code,
# wired into ctest through CMAKE_CROSSCOMPILING_EMULATOR (the emscripten toolchain sets it to node).
#
# shader_compiler is bundled unconditionally here, unlike android where it is bundled only when
# -shader_compiler True asked for it.
# That is a default, not a capability: DXC cross compiles for both, but it more than doubles the build, so
# android leaves it off while the web target treats it as the point of the port.
# Not bundled: graphics_interface (no GPU under node) and the functional suites (need a human).
# Test_dynamicLibrary is bundled and its dlopen is expected to fail, since there is no dlopen without
# MAIN_MODULE.

set(testRoot ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include(${testRoot}/../cmake/oxc3.cmake)

# The shader suites are passed in rather than shared, since bundling them is a per platform decision:
# web always does, android only when EnableShaderCompiler is on.

file(GLOB webShaderTestSources CONFIGURE_DEPENDS "${testRoot}/shader_compiler/test/*.c")

oxc3_add_bundled_test(
	TARGET  OxC3_wtest
	ROOT    ${testRoot}
	SELF    ${testRoot}/..
	ENTRY   wtest_main.c
	SOURCES ${webShaderTestSources}
	LIBS    OxC3_shader_compiler
)

apply_web_link_options(OxC3_wtest)

apply_dependencies(OxC3_wtest)

# ctest: CMAKE_CROSSCOMPILING_EMULATOR (node) runs the module.
# Suites that read data get their working directory from -packages:
# packages/ lives under <SELF>/build/<config>/web: conan's toolchain sets CMAKE_CONFIGURATION_TYPES even
# for single-config generators, so add_virtual_files keeps the config component (see cmake/oxc3.cmake).
# The shader corpus is reached through the virtual file system rather than a source dir,
# so it needs the same directory, not its own.

add_test(
	NAME OxC3_wtest
	COMMAND OxC3_wtest
		-packages "${testRoot}/../build/$<CONFIG>/web"
)
