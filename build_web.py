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

"""Cross build OxC3 for the web (emscripten, wasm64) and optionally run the test bundle under node.

Same shape as build_android.py, minus the apk machinery: node has a real exec, so tests are an
ordinary executable (OxC3_wtest.js + .wasm) with an exit code.

The shader compiler is on by default here, where android leaves it off. That is a default, not a
capability: DXC cross compiles for both, but it more than doubles the build, and compiling HLSL in
the browser is the point of this port. The host packager is still tool_required, because the cross
build's own OxC3_package is a .js the build machine can't find_program, which is why
conanfile.build_requirements() also fires on cross_building.

Needs a local emsdk (env EMSDK, default ~/emsdk); --run_tests uses the node the SDK ships and only
falls back to PATH, because emsdk_env is what puts that node on PATH and a plain shell hasn't sourced it.
Single-threaded wasm for now: no -pthread anywhere, Platform_getThreads() returns 1 and JobQueue
runs inline (see include/types/container/job_queue.h). EH (-fwasm-exceptions) and -m64 are pinned
in packages/conan/profiles/emscripten_wasm64 and must match in every consumer link.
"""

import argparse
import os
import subprocess
import sys

import build_common as common

WEB_PROFILE = os.path.join("packages", "conan", "profiles", "emscripten_wasm64")
WEB_PROFILE_MT = os.path.join("packages", "conan", "profiles", "emscripten_wasm64_mt")

def webProfile(threads):
	"""Profile for the requested flavor.

	The two are separate profiles rather than one templated profile so the flag lists that fork
	package_id are visible in the file that causes the fork. -pthread is ABI, so every dependency is
	rebuilt for the threaded flavor; nothing is shared with the single threaded one but the recipes.
	"""
	return WEB_PROFILE_MT if threads else WEB_PROFILE

def ensureEmsdk():

	emsdk = os.environ.get("EMSDK", os.path.join(os.path.expanduser("~"), "emsdk"))

	if not os.path.isfile(os.path.join(emsdk, "upstream", "emscripten", "emcc")):
		print(f"-- No emsdk at {emsdk} (set EMSDK or install to ~/emsdk)", file=sys.stderr)
		sys.exit(1)

	os.environ["EMSDK"] = emsdk
	return emsdk

def webOptionArgs(tests, hostCrypto=False):
	"""Options for the web target itself.

	enableShaderCompiler=True: the point of the port.
	SIMD on: SIMD_WASM covers the vector math and leaves crypto and hash scalar, which CMake derives
	from EnableSIMD; spelling it out keeps the conan package id honest.
	No dynamic linking: wasm dlopen needs MAIN_MODULE.
	"""

	options = {
		"cliGraphics": "False",
		"enableOxC3CLI": "True",
		"forceFloatFallback": "False",
		"enableTests": "True" if tests else "False",
		"dynamicLinkingGraphics": "False",
		"dynamicLinkingShaderCompiler": "False",
		"enableShaderCompiler": "True",
		# SIMD_WASM: vector math on native SIMD128 (vec4*_wasm.inc.h), crypto and hash still scalar because
		# wasm has no AES-NI or SHA equivalent.
		# Emscripten's SSE shims are deliberately unused: they misparse under -fms-extensions, which
		# OxC3's C++ TUs require.
		"enableSIMD": "True",
		"enableHostCrypto": "True" if hostCrypto else "False"
	}

	return " ".join(f"-o \"&:{k}={v}\"" for k, v in options.items())

def doBuild(mode, doInstall, tests, cache, hostCrypto=False, threads=False):

	profile      = webProfile(threads)
	buildProfile = common.crossBuildProfileArgs()

	# The dependency hash cache is keyed per flavor, because the two share recipes but not binaries.
	# A key collision would report the threaded packages as unchanged and reuse the single threaded ones.

	cacheSuffix  = "_mt" if threads else ""

	# Separate output folder per flavor, not just a separate profile.
	# CMake seeds CMAKE_<LANG>_FLAGS from the toolchain's _INIT variables on the first configure only,
	# so reusing a folder configured for the other flavor keeps the cached flags and drops -pthread.
	# The toolchain file then says one thing and CMakeCache.txt another,
	# and the build links a single threaded binary that passes every test.

	target       = "wasm64_mt" if threads else "wasm64"
	profileArgs  = f"--profile:host=\"{profile}\" {buildProfile}"

	# Target dependencies.
	# Graphics is off (no WebGPU backend yet) so no vulkan_headers; agility_sdk ships d3d12shader.h for DXC's dxcreflect.h.
	# No openal_soft: emscripten has its own OpenAL.

	common.conanCreateIfChanged(
		"packages/agility_sdk", profile, mode, profileArgs, cache, key=f"packages/agility_sdk::web{cacheSuffix}"
	)

	# The shader compiler's dependencies.
	# DXC needs a tablegen that runs on the machine doing the building, which only a host build of the same recipe has;
	# see build_common.hostTablegenDir.
	# Same wiring android uses (build_android.py), rather than bootstrapping a second native LLVM.
	# Pinned Release for the same reason the host build pins them.

	tablegenDir = common.hostTablegenDir()

	if not tablegenDir:
		print(
			"-- No host dxc package to take llvm-tblgen/clang-tblgen from; build the host tooling "
			"first (build.py, or build_web.py --host_package_only)", file=sys.stderr
		)
		sys.exit(1)

	tablegenConf = f"-c:h user.dxc:tablegen_dir=\"{tablegenDir}\""

	shaderMode = common.shaderCompilerDepMode(mode, False)

	for package in common.SHADER_COMPILER_DEPS:
		common.conanCreateIfChanged(
			package, profile, shaderMode, profileArgs, cache, key=f"{package}::web{cacheSuffix}",
			options=tablegenConf
		)

	outputFolder = f"\"build/{mode}/web/{target}\""
	options      = webOptionArgs(tests, hostCrypto)
	shaderArgs   = common.shaderCompilerDepArgs(False)

	common.run(
		f"conan build . -of {outputFolder} {options} -s build_type={mode} {profileArgs} {shaderArgs} --build=missing"
	)

	if doInstall:
		common.run(
			f"conan export-pkg . -of {outputFolder} {options} -s build_type={mode} {profileArgs} {shaderArgs}"
		)

def emsdkNode():
	"""The node the emsdk ships, falling back to PATH.

	emsdk installs node under $EMSDK/node/<version>/bin and only puts it on PATH through emsdk_env, which a
	plain shell (and CI) hasn't sourced. Resolving it here means --run_tests works from a bare checkout with
	nothing but EMSDK set, instead of failing with command-not-found that reads like a broken test bundle.
	"""

	emsdk = os.environ.get("EMSDK")

	if emsdk:

		nodeRoot = os.path.join(emsdk, "node")

		if os.path.isdir(nodeRoot):
			for entry in sorted(os.listdir(nodeRoot), reverse=True):
				candidate = os.path.join(nodeRoot, entry, "bin", "node")
				if os.path.isfile(candidate):
					return candidate

	return "node"

def runTests(mode, suite=None, threads=False):
	"""Run the bundle under node directly (what ctest would do), streaming output; exit code is the result."""

	binDir = os.path.join(common.ROOT, "build", mode, "web", "wasm64_mt" if threads else "wasm64", "bin")
	bundle = os.path.join(binDir, "OxC3_wtest.js")

	if not os.path.isfile(bundle):
		print(f"-- No test bundle at {bundle} (build with -tests True first)", file=sys.stderr)
		sys.exit(1)

	cmd = (
		f"\"{emsdkNode()}\" \"{bundle}\" "
		f"-packages \"{os.path.join(common.ROOT, 'build', mode, 'web')}\""
	)

	if suite:
		cmd += f" {suite}"

	result = subprocess.run(cmd, shell=True)

	if result.returncode:
		print("-- Tests FAILED", file=sys.stderr)
		sys.exit(result.returncode)

	print("-- Tests passed")

def main():

	common.ensureCorrectEnvironment(__file__)

	parser = argparse.ArgumentParser(description="Build OxC3 for the web (emscripten wasm64)")

	parser.add_argument("-mode", type=str, default="Release", choices=common.ALL_MODES, help="Build mode")
	parser.add_argument(
		"-tests", type=str, default="True", choices=["True", "False"],
		help="Build OxC3_wtest, one wasm module with every unit test suite (run it with --run_tests)"
	)
	parser.add_argument("-suite", type=str, default=None, help="With --run_tests: run a single suite by name")
	parser.add_argument(
		"--host_crypto", action="store_true",
		help="Route SHA-256/AES-GCM to the host's crypto (needs a cross origin isolated page + Worker in browsers)"
	)
	parser.add_argument(
		"-threads", type=str, default="False", choices=["True", "False"],
		help="Build the -pthread flavor (needs cross origin isolation in a browser; rebuilds every dependency)"
	)
	parser.add_argument("--run_tests", help="Run the test bundle under node after building", action="store_true")
	parser.add_argument("--skip_build", help="Skip the build (e.g. to only run tests)", action="store_true")
	parser.add_argument("--install", help="conan export-pkg the result", action="store_true")

	parser.add_argument("--skip_host_package", help="Assume a host oxc3 with OxC3_package is already in the conan cache", action="store_true")
	parser.add_argument("--host_package_only", help="Only build + export the host oxc3 used for packaging, then stop", action="store_true")
	parser.add_argument("--force_deps", help="Ignore hash cache and rebuild all dependencies", action="store_true")

	args = parser.parse_args()

	if args.host_package_only:
		common.buildHostToolPackage(forceDeps=args.force_deps)
		return

	ensureEmsdk()

	if not args.skip_build:

		if not args.skip_host_package:
			common.buildHostToolPackage(forceDeps=args.force_deps)

		common.ensureDefaultProfile()

		if args.force_deps and os.path.isfile(common.HASH_CACHE_FILE):
			os.remove(common.HASH_CACHE_FILE)

		cache = common.loadHashCache()
		# The threaded flavor already needs a cross origin isolated page for its shared memory, which is
		# the same requirement host crypto has, so it routes by default. --host_crypto still forces it on
		# for the single threaded flavor.

		threads = args.threads == "True"
		doBuild(args.mode, args.install, args.tests == "True", cache, args.host_crypto or threads, threads)
		common.saveHashCache(cache)

	if args.run_tests:
		runTests(args.mode, args.suite, args.threads == "True")

if __name__ == "__main__":
	main()
