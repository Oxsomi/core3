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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

import argparse
import os
import sys

import build_common as common

def main():

	common.ensureCorrectEnvironment(__file__)

	parser = argparse.ArgumentParser(description="Build OxC3 for the host platform")

	parser.add_argument("-mode", type=str, default=None, choices=common.ALL_MODES, help="Build type (optional on Windows; defaults to all configs)")
	parser.add_argument("-simd",            type=str, default="True",  choices=["True", "False"], help="Enable SIMD")
	parser.add_argument("-tests",           type=str, default="False", choices=["True", "False"], help="Enable tests")
	parser.add_argument("-dynamic_linking", type=str, default="False", choices=["True", "False"], help="Dynamic linking graphics")
	parser.add_argument(
		"-dynamic_linking_shader_compiler", type=str, default="True", choices=["True", "False"],
		help="Build the shader compiler as a shared library, so DXC lives in one module instead of "
		     "in every executable that links it. On by default; independent of -dynamic_linking"
	)

	parser.add_argument(
		"-debug_shader_compiler", type=str, default="False", choices=["True", "False"],
		help="Build and consume DXC/SPIRV-Reflect in the current mode instead of Release. Off by default: "
		     "they dominate build time and are rarely the thing being debugged"
	)

	parser.add_argument(
		"-compiler", type=str, default=None,
		help="Toolchain to build with; defaults to the platform's usual one (msvc on Windows, clang on macOS, "
		     "gcc on Linux). 'clang' on Windows means clang-cl, which uses MSVC's ABI and CRT"
	)

	parser.add_argument(
		"-asan", type=str, default="False", choices=["True", "False"],
		help="Build with AddressSanitizer. clang/gcc only, and RelWithDebInfo is the mode you want: "
		     "ASan needs optimized code with frame pointers and symbols"
	)

	parser.add_argument(
		"-ubsan", type=str, default="False", choices=["True", "False"],
		help="Build with UndefinedBehaviorSanitizer. clang/gcc only. On Windows it traps into the crash "
		     "handler rather than using UBSan's own reporting runtime"
	)

	parser.add_argument("--force_deps", action="store_true", help="Ignore hash cache and rebuild all dependencies")

	args, remainder = parser.parse_known_args()

	system        = common.hostSystem()
	arch          = common.hostArch()[0]
	platform_name = common.hostPlatformName()

	# On non-Windows, mode is required

	if system != "Windows" and args.mode is None:
		print("-- Error: -mode is required on non-Windows platforms", file=sys.stderr)
		sys.exit(1)

	# Decide which modes to build deps for

	# A CMake cache is tied to the compiler that configured it, so a non-default toolchain gets its own
	# tree rather than fighting over the default one's.

	compiler = args.compiler or common.defaultCompiler()
	suffix   = "" if compiler == common.defaultCompiler() else f"_{compiler}"

	if system == "Windows":
		dep_modes = common.ALL_MODES if args.mode is None else [ args.mode ]
		build_dir = f"build/{platform_name}/{arch}{suffix}"
	else:
		dep_modes = [ args.mode ]
		build_dir = f"build/{args.mode}/{platform_name}/{arch}{suffix}"

	# Build dependencies (skipped when hash unchanged)

	if args.force_deps and os.path.isfile(common.HASH_CACHE_FILE):
		os.remove(common.HASH_CACHE_FILE)

	cache = common.loadHashCache()
	debugShaderCompiler = args.debug_shader_compiler == "True"

	common.buildHostDependencies(
		dep_modes, cache, debugShaderCompiler, compiler, args.asan == "True", args.ubsan == "True"
	)
	common.saveHashCache(cache)

	# Build project, use the mode that was requested, or Release as the
	# Conan install step for a multi-config VS project (VS itself builds all configs)

	extra = " ".join(remainder)

	build_modes = common.ALL_MODES if (system == "Windows" and args.mode is None) else [ args.mode or "Release" ]

	for mode in build_modes:
		common.run(
			f"conan build . "
			f"-of {build_dir} "
			f"{common.hostProfileArgs(mode, compiler)} "
			f"-s build_type={mode} "
			f"-o enableSIMD={args.simd} "
			f"-o enableTests={args.tests} "
			f"-o dynamicLinkingGraphics={args.dynamic_linking} "
			f"-o dynamicLinkingShaderCompiler={args.dynamic_linking_shader_compiler} "
			f"-o debugShaderCompiler={args.debug_shader_compiler} "
			f"-o enableASAN={args.asan} "
			f"-o enableUBSAN={args.ubsan} "
			f"{common.shaderCompilerDepArgs(debugShaderCompiler)} "
			f"{extra}"
		)

	# Run tests

	if args.tests == "True":

		test_mode = args.mode if args.mode is not None else "Release"

		# Per-test wall clock cap. The slowest test (the shader compiler corpus, in Debug on a CI runner)
		# lands well under a minute, so anything reaching this is wedged rather than slow, and without the
		# cap a deadlocked JobQueue just burns the whole job's time budget before anyone finds out.

		test_timeout = 300

		flags = f"-C {test_mode} --output-on-failure --timeout {test_timeout}"

		if system != "Windows":
			common.run(f"ctest --test-dir {build_dir}/build/{test_mode} {flags}", cwd=os.getcwd())
		else:
			common.run(f"ctest --test-dir {build_dir}/build {flags}", cwd=os.getcwd())

if __name__ == "__main__":
	main()
