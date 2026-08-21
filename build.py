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
import shutil
import sys

import build_common as common

def main():

	common.ensureCorrectEnvironment(__file__)

	parser = argparse.ArgumentParser(description="Build OxC3 for the host platform")

	parser.add_argument("-mode", type=str, default=None, choices=common.ALL_MODES, help="Build type (optional on Windows; defaults to all configs)")
	parser.add_argument("-simd",            type=str, default="True",  choices=["True", "False"], help="Enable SIMD")
	parser.add_argument("-tests",           type=str, default="False", choices=["True", "False"], help="Enable tests")
	parser.add_argument(
		"-ctest", default="False", choices=["True", "False"],
		help="Run the full suite through ctest after building. Off by default: each suite already runs right "
		     "after it is built, and only when one of its inputs changed. CI turns this on to always run everything."
	)
	parser.add_argument(
		"-generator", type=str, default=None, choices=["ninja", "default"],
		help="CMake generator. 'default' is whatever the profile picks (Visual Studio on Windows). 'ninja' "
		     "builds with Ninja instead, which is what produces a compile_commands.json for clangd and VS "
		     "Code. It gets its own build tree, since a CMake cache belongs to the generator that made it"
	)
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

	parser.add_argument(
		"-deploy", type=str, default=None,
		help="After building, export the conan package and lay it out in this folder. Use it to produce a "
		     "prebuilt without restating what belongs in one: the contents come from conanfile.py's "
		     "package(), so a deployed folder and a conan install give the same thing"
	)

	args, remainder = parser.parse_known_args()

	system        = common.hostSystem()
	arch          = common.hostArch()[0]
	platform_name = common.hostPlatformName()

	# On non-Windows, mode is required

	if system != "Windows" and args.mode is None:
		print("-- Error: -mode is required on non-Windows platforms", file=sys.stderr)
		sys.exit(1)

	# MSVC's own sanitizers are deliberately unused here (see windows_clang_sanitizers.yml), and asking for
	# them anyway doesn't fail cleanly: cl ignores -fsanitize=undefined with a D9002 warning and carries on
	# unsanitized, while its ASan pulls in a runtime the clang flags don't match, which surfaces as unresolved
	# __imp___asan_* symbols deep inside a dependency build rather than as anything pointing back here.

	if args.compiler == "msvc" and (args.asan == "True" or args.ubsan == "True"):
		print("-- Error: -asan/-ubsan require -compiler clang, MSVC's sanitizers aren't used here", file=sys.stderr)
		sys.exit(1)

	# Decide which modes to build deps for

	# A CMake cache is tied to the compiler that configured it, so a non-default toolchain gets its own
	# tree rather than fighting over the default one's.

	compiler = args.compiler or common.defaultCompiler()
	suffix   = "" if compiler == common.defaultCompiler() else f"_{compiler}"

	# A CMake cache belongs to its generator just as much as to its compiler, so Ninja gets its own tree
	# rather than making every switch a full reconfigure.

	# Ninja Multi-Config on Windows rather than plain Ninja, so one tree still holds every config the way the
	# Visual Studio generator does and -mode stays optional.
	# Elsewhere -mode is always given and the build folder already names it, so single config Ninja is right.
	# Checked here rather than at the conan call, so an unusable toolchain fails before anything is built.

	generatorConf = ""

	if args.generator == "ninja":

		if shutil.which("ninja") is None:
			print("-- Error: -generator ninja needs ninja on PATH", file=sys.stderr)
			sys.exit(1)

		suffix += "_ninja"

		ninjaGenerator = "Ninja Multi-Config" if system == "Windows" else "Ninja"
		generatorConf = f'-c tools.cmake.cmaketoolchain:generator="{ninjaGenerator}" '
		generatorConf += common.visualStudioNinjaConf(compiler)

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

	# One options string for both the build and the export below: export-pkg has to select the very package
	# id the build produced, so any drift between the two would silently export a different configuration.

	options = (
		f"{generatorConf}"
		f"-o enableSIMD={args.simd} "
		f"-o enableTests={args.tests} "

		# Either the build runs each suite as it is built, or ctest runs them all at the end.
		# Both means every suite runs twice.

		f"-o testAutoRun={'False' if args.ctest == 'True' else 'True'} "
		f"-o dynamicLinkingGraphics={args.dynamic_linking} "
		f"-o dynamicLinkingShaderCompiler={args.dynamic_linking_shader_compiler} "
		f"-o debugShaderCompiler={args.debug_shader_compiler} "
		f"-o enableASAN={args.asan} "
		f"-o enableUBSAN={args.ubsan} "
		f"{common.shaderCompilerDepArgs(debugShaderCompiler)} "
		f"{extra}"
	)

	for mode in build_modes:
		common.run(
			f"conan build . "
			f"-of {build_dir} "
			f"{common.hostProfileArgs(mode, compiler)} "
			f"-s build_type={mode} "
			f"{options}"
		)

		# Packaging is conanfile.py's job, so a prebuilt never has to restate which files belong in one.
		# direct_deploy copies just this package (not its dependencies) out of the cache into a plain
		# folder, which is what makes the result usable as a downloadable archive.

		if args.deploy:

			reference = f"{common.recipeName()}/{common.recipeVersion()}"
			profile   = common.hostProfileArgs(mode, compiler)

			common.run(f"conan export-pkg . -of {build_dir} {profile} -s build_type={mode} {options}")

			common.run(
				f"conan install --requires={reference} "
				f"{profile} "
				f"-s build_type={mode} "
				f"{options} "
				f"--deployer=direct_deploy "
				f"--deployer-folder=\"{args.deploy}/{mode}\""
			)

	# Run tests.
	#
	# Normally there is nothing to do here: each suite runs behind a stamp file that depends on everything the
	# suite was built from, so one whose inputs did not change does not re-run and the build graph does the
	# dirty tracking (see oxc3_add_test in cmake/oxc3.cmake).
	# CI passes -ctest to run the whole suite through ctest regardless, which always runs everything and
	# reports per test rather than as a single build failure.

	if args.tests == "True" and args.ctest == "True":

		test_mode = args.mode if args.mode is not None else "Release"

		# Per-test wall clock cap.
		# It used to be sized around the shader compiler corpus, which lands well under a minute, but the
		# graphics suite now reaches a real device on CI: lavapipe on linux, WARP on windows.
		# Both are CPU rasterizers on a four vCPU runner, and that suite goes on to create a device, buffers,
		# command lists and load prebuilt oiSH, so it is now the slowest thing here by some margin.
		# Anything reaching the cap is still wedged rather than slow, which is the point: without it a
		# deadlocked JobQueue burns the whole job's time budget before anyone finds out.

		test_timeout = 900

		flags = f"-C {test_mode} --output-on-failure --timeout {test_timeout}"

		if system != "Windows":
			common.run(f"ctest --test-dir {build_dir}/build/{test_mode} {flags}", cwd=os.getcwd())
		else:
			common.run(f"ctest --test-dir {build_dir}/build {flags}", cwd=os.getcwd())

if __name__ == "__main__":
	main()
