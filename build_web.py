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
import shutil
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

def webFlavorSuffix(threads, asan=False, ubsan=False):
	"""Name of one flavor, as a suffix on wasm64.

	Everything that forks the package set forks this name.
	-pthread is ABI, and a sanitized build links sanitized dependencies,
	so none of these share a binary with the plain flavor.
	CMakeLists.txt's EMSCRIPTEN block is authoritative for the name and derives the artifact directory
	(archDir) from it: wasm64, then _mt when the compile flags carry -pthread, then _asan, then _ubsan.
	conanfile.py's package() rebuilds the same name to find what CMake wrote.
	All three have to agree, otherwise a build writes its binaries into another flavor's bin and the
	test bundle is looked for in a directory nothing ever writes.
	"""

	return ("_mt" if threads else "") + ("_asan" if asan else "") + ("_ubsan" if ubsan else "")

def ensureEmsdk():

	emsdk = os.environ.get("EMSDK", os.path.join(os.path.expanduser("~"), "emsdk"))

	if not os.path.isfile(os.path.join(emsdk, "upstream", "emscripten", "emcc")):
		print(f"-- No emsdk at {emsdk} (set EMSDK or install to ~/emsdk)", file=sys.stderr)
		sys.exit(1)

	os.environ["EMSDK"] = emsdk
	return emsdk

def webOptionArgs(tests, hostCrypto=False, asan=False, ubsan=False):
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
		"enableHostCrypto": "True" if hostCrypto else "False",

		# The emscripten linker adds the ASan shadow region (an eighth of MAXIMUM_MEMORY) to INITIAL_MEMORY
		# itself whenever ALLOW_MEMORY_GROWTH is on, which apply_web_link_options (cmake/oxc3.cmake) sets,
		# so the 32MB the module starts with covers a sanitized build and nothing here has to raise it.
		# Raising it from here is not possible anyway: those are target_link_options,
		# which land after anything conan can put in the linker flags, and emcc takes the last -s setting.

		"enableASAN": "True" if asan else "False",
		"enableUBSAN": "True" if ubsan else "False"
	}

	return " ".join(f"-o \"&:{k}={v}\"" for k, v in options.items())

def doBuild(
	mode, doInstall, tests, cache, hostCrypto=False, threads=False, asan=False, ubsan=False, singleFile=False,
	dxcSource=None
):

	profile      = webProfile(threads)
	buildProfile = common.crossBuildProfileArgs()

	# The dependency hash cache is keyed per flavor, because the flavors share recipes but not binaries.
	# A key collision would report the threaded packages as unchanged and reuse the single threaded ones,
	# and would hand a sanitized build the unsanitized dependencies in the same way.

	cacheSuffix  = webFlavorSuffix(threads, asan, ubsan)

	# Separate output folder per flavor, not just a separate profile.
	# CMake seeds CMAKE_<LANG>_FLAGS from the toolchain's _INIT variables on the first configure only,
	# so reusing a folder configured for the other flavor keeps the cached flags and drops -pthread.
	# The toolchain file then says one thing and CMakeCache.txt another,
	# and the build links a single threaded binary that passes every test.

	target       = f"wasm64{cacheSuffix}"
	profileArgs  = f"--profile:host=\"{profile}\" {buildProfile}"

	# The dependencies that compile C/C++ have to be built the way we are.
	# conanfile.py's requirements() propagates enableASAN/enableUBSAN to dxc and spirv_reflect, so the graph
	# asks for sanitized packages; creating them without these options leaves it resolving a binary that
	# nothing ever built, which is a "Missing binary" error at install time rather than a link failure later.

	sanitizerOptions = ""

	if asan or ubsan:
		sanitizerOptions = f"-o enableASAN={asan} -o enableUBSAN={ubsan}"

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

	# Building the fork from a working tree rather than from the pin in packages/dxc/conandata.yml, so a
	# change to it can be tested before it is pushed. The package this produces is not reproducible, which
	# is why it takes an explicit path rather than defaulting to a sibling checkout.

	if dxcSource:
		tablegenConf += f" -c:h user.dxc:source_dir=\"{os.path.abspath(dxcSource)}\""

	shaderMode = common.shaderCompilerDepMode(mode, False)

	# Both of them take their sanitizer wiring from a python_requires, which has to be resolvable from the
	# local cache before either recipe is loaded.

	common.exportSharedRecipes()

	for package in common.SHADER_COMPILER_DEPS:
		common.conanCreateIfChanged(
			package, profile, shaderMode, profileArgs, cache, key=f"{package}::web{cacheSuffix}",
			options=f"{tablegenConf} {sanitizerOptions}".strip()
		)

	outputFolder = f"\"build/{mode}/web/{target}\""
	options      = webOptionArgs(tests, hostCrypto, asan, ubsan)
	shaderArgs   = common.shaderCompilerDepArgs(False)

	# How the frontend module ships its wasm is a link choice on one target, not an ABI one, so it rides
	# in as a cmake variable rather than as a conan option: an option would fork package_id and rebuild
	# every dependency for something no consumer links against.

	frontendArgs = ""

	if singleFile:
		frontendArgs = "-c tools.cmake.cmaketoolchain:extra_variables=\"{'WebFrontendSingleFile': 'ON'}\""

	common.run(
		f"conan build . -of {outputFolder} {options} -s build_type={mode} {profileArgs} {shaderArgs} "
		f"{frontendArgs} --build=missing"
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

def runTests(mode, suite=None, threads=False, asan=False, ubsan=False):
	"""Run the bundle under node directly (what ctest would do), streaming output; exit code is the result."""

	flavor = f"wasm64{webFlavorSuffix(threads, asan, ubsan)}"
	binDir = os.path.join(common.ROOT, "build", mode, "web", flavor, "bin")
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

WEB_FRONTEND = "web"
WEB_MODULE = "OxC3_wasm"

def stageFrontend(mode, singleFile=False):
	"""Copy the module the page loads into web/wasm/.

	Two flavors coexist so a rebuild can never break the other one's page: the two file build stages as
	OxC3_wasm.js + .wasm (what js/wasm_rpc.js's worker asks for on http), the embedded build as
	OxC3_wasm_sf.js (what js/wasmload.js asks for on file://, where the sidecar .wasm can't be fetched).
	They're build artifacts (21+ MB), hence copied rather than committed; .gitignore covers the folder.
	"""

	binDir = os.path.join(common.ROOT, "build", mode, "web", "wasm64", "bin")
	outDir = os.path.join(common.ROOT, WEB_FRONTEND, "wasm")

	stagedName = f"{WEB_MODULE}_sf.js" if singleFile else f"{WEB_MODULE}.js"
	moduleJs = os.path.join(binDir, f"{WEB_MODULE}.js")

	if not os.path.isfile(moduleJs):
		print(f"-- No {WEB_MODULE}.js at {binDir} (the module only builds with the shader compiler on)", file=sys.stderr)
		sys.exit(1)

	os.makedirs(outDir, exist_ok=True)
	shutil.copy2(moduleJs, os.path.join(outDir, stagedName))
	staged = [stagedName]

	if singleFile:
		return print(f"-- Staged {stagedName} into {os.path.relpath(outDir, common.ROOT)}")

	# Precompressed siblings belong to the module they were made from: serveFrontend (and a real host)
	# prefers them, so a stale one would silently shadow the fresh module. --precompress recreates them.
	for leftover in (f"{WEB_MODULE}.js.br", f"{WEB_MODULE}.wasm.br"):
		if os.path.isfile(os.path.join(outDir, leftover)):
			os.remove(os.path.join(outDir, leftover))

	# A single file build embeds the wasm and emits none, so a leftover one from an earlier build has to
	# go: it would sit there as a stale 21 MB file the page never reads.
	# Which build this is comes from the module itself, since it names the file it will ask for and a
	# single file one names nothing. Timestamps can't tell them apart: emcc writes the wasm first, so it
	# is always fractionally older than the js beside it, even when both are fresh.

	moduleWasm = os.path.join(binDir, f"{WEB_MODULE}.wasm")
	stagedWasm = os.path.join(outDir, f"{WEB_MODULE}.wasm")

	with open(moduleJs, "r", encoding="utf-8", errors="ignore") as f:
		needsWasm = f"{WEB_MODULE}.wasm" in f.read()

	if needsWasm:

		if not os.path.isfile(moduleWasm):
			print(f"-- {WEB_MODULE}.js asks for a .wasm that isn't in {binDir}", file=sys.stderr)
			sys.exit(1)

		shutil.copy2(moduleWasm, stagedWasm)
		staged.append(f"{WEB_MODULE}.wasm")

	elif os.path.isfile(stagedWasm):
		os.remove(stagedWasm)

	print(f"-- Staged {', '.join(staged)} into {os.path.relpath(outDir, common.ROOT)}")

def precompressFrontend():
	"""Write .br siblings next to the staged module, which serveFrontend and a real host serve.

	Brotli turns the ~21 MB module into ~5.5 MB, the difference between a painful first load and an ok
	one. Node's zlib does the compressing because the emsdk already ships node; no extra tool needed.
	"""

	outDir = os.path.join(common.ROOT, WEB_FRONTEND, "wasm")
	node = emsdkNode()

	for name in (f"{WEB_MODULE}.js", f"{WEB_MODULE}.wasm"):

		src = os.path.join(outDir, name)

		if not os.path.isfile(src):
			continue

		subprocess.run(
			[
				node, "-e",
				"const z=require('zlib'),f=require('fs'),p=process.argv[1];"
				"f.writeFileSync(p+'.br',z.brotliCompressSync(f.readFileSync(p),"
				"{params:{[z.constants.BROTLI_PARAM_QUALITY]:11,"
				"[z.constants.BROTLI_PARAM_SIZE_HINT]:f.statSync(p).size}}))",
				src
			],
			check=True
		)

		print(f"-- {name}: {os.path.getsize(src):,} -> {os.path.getsize(src + '.br'):,} bytes (.br)")

def runFrontendTests(mode):
	"""Drive the staged module through the frontend's own boundary (web/js/wasm.js), headless.

	This is the regression net for the boundary rather than for the page: the call frame, wasm64
	pointer marshalling, the project tree #includes resolve against, and every document serializer.
	"""

	smoke = os.path.join(common.ROOT, WEB_FRONTEND, "dev", "wasm_smoke.js")
	workerSmoke = os.path.join(common.ROOT, WEB_FRONTEND, "dev", "worker_smoke.js")
	module = os.path.join(common.ROOT, "build", mode, "web", "wasm64", "bin", f"{WEB_MODULE}.js")

	if not os.path.isfile(module):
		print(f"-- No module at {module} (build with --frontend first)", file=sys.stderr)
		sys.exit(1)

	for suite in (smoke, workerSmoke):

		result = subprocess.run(f"\"{emsdkNode()}\" \"{suite}\" \"{module}\"", shell=True)

		if result.returncode:
			print("-- Frontend tests FAILED", file=sys.stderr)
			sys.exit(result.returncode)

def serveFrontend(port):
	"""Serve web/ over http, which loading a .wasm needs (file:// blocks the fetch).

	Cross origin isolation is sent even though the single threaded module doesn't need it: it costs
	nothing here and it's what a threaded flavor would require, so a page that works under this server
	works under the one that has to ship those headers.
	"""

	import functools
	import http.server

	class Handler(http.server.SimpleHTTPRequestHandler):

		def send_head(self):

			# A precompressed sibling (--precompress) is served the way a real host serves it, so first
			# load measured here matches the deployed site rather than the 21 MB raw module.
			if "br" in self.headers.get("Accept-Encoding", ""):

				path = self.translate_path(self.path)

				if os.path.isfile(path + ".br"):
					f = open(path + ".br", "rb")
					self.send_response(200)
					self.send_header("Content-Type", self.guess_type(path))
					self.send_header("Content-Encoding", "br")
					self.send_header("Content-Length", str(os.fstat(f.fileno()).st_size))
					self.end_headers()
					return f

			return super().send_head()

		def end_headers(self):
			self.send_header("Cross-Origin-Opener-Policy", "same-origin")
			self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
			self.send_header("Cache-Control", "no-store")
			super().end_headers()

	Handler.extensions_map[".wasm"] = "application/wasm"

	root = os.path.join(common.ROOT, WEB_FRONTEND)
	handler = functools.partial(Handler, directory=root)

	print(f"-- Serving {os.path.relpath(root, common.ROOT)} on http://localhost:{port} (ctrl+c to stop)")

	with http.server.ThreadingHTTPServer(("localhost", port), handler) as server:
		try:
			server.serve_forever()
		except KeyboardInterrupt:
			print()

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
	parser.add_argument(
		"-asan", type=str, default="False", choices=["True", "False"],
		help="Build with AddressSanitizer. Its own flavor, in its own output folder: the dependencies that "
		     "compile C/C++ are rebuilt sanitized, so nothing is shared with the plain build"
	)
	parser.add_argument(
		"-ubsan", type=str, default="False", choices=["True", "False"],
		help="Build with UndefinedBehaviorSanitizer. Its own flavor as well, for the same reason as -asan"
	)
	parser.add_argument(
		"--frontend", action="store_true",
		help="Stage OxC3_wasm into web/wasm/ so the web frontend runs against the real compiler instead of its mocks"
	)
	parser.add_argument(
		"-dxc_source", type=str, default=None, metavar="PATH",
		help="Build DXC from this checkout instead of the pinned clone, for iterating on the fork"
	)
	parser.add_argument(
		"--single_file", action="store_true",
		help="Embed the wasm in the frontend module's js, so web/index.html runs from file:// with no server"
	)
	parser.add_argument(
		"--run_frontend_tests", action="store_true",
		help="Run web/dev/wasm_smoke.js + worker_smoke.js against the built module (the boundary's regression nets)"
	)
	parser.add_argument(
		"--precompress", action="store_true",
		help="Write brotli .br siblings next to the staged module; --serve and real hosts prefer them"
	)
	parser.add_argument(
		"--serve", nargs="?", type=int, const=8000, default=None, metavar="PORT",
		help="Serve web/ on http://localhost:PORT (default 8000) with COOP/COEP; a .wasm can't load from file://"
	)
	parser.add_argument("--run_tests", help="Run the test bundle under node after building", action="store_true")
	parser.add_argument("--skip_build", help="Skip the build (e.g. to only run tests)", action="store_true")
	parser.add_argument("--install", help="conan export-pkg the result", action="store_true")

	parser.add_argument("--skip_host_package", help="Assume a host oxc3 with OxC3_package is already in the conan cache", action="store_true")
	parser.add_argument("--host_package_only", help="Only build + export the host oxc3 used for packaging, then stop", action="store_true")
	parser.add_argument("--force_deps", help="Ignore hash cache and rebuild all dependencies", action="store_true")

	args = parser.parse_args()

	if args.host_package_only:
		common.buildHostToolPackage(forceDeps=args.force_deps, dxcSource=args.dxc_source)
		return

	ensureEmsdk()

	if not args.skip_build:

		if not args.skip_host_package:
			common.buildHostToolPackage(forceDeps=args.force_deps, dxcSource=args.dxc_source)

		common.ensureDefaultProfile()

		if args.force_deps and os.path.isfile(common.HASH_CACHE_FILE):
			os.remove(common.HASH_CACHE_FILE)

		cache = common.loadHashCache()
		# The threaded flavor already needs a cross origin isolated page for its shared memory, which is
		# the same requirement host crypto has, so it routes by default. --host_crypto still forces it on
		# for the single threaded flavor.

		threads = args.threads == "True"

		doBuild(
			args.mode, args.install, args.tests == "True", cache, args.host_crypto or threads, threads,
			args.asan == "True", args.ubsan == "True", args.single_file, args.dxc_source
		)

		# The frontend ships both flavors, and the embed switch is a cmake variable on one target, so
		# the second flavor is a reconfigure + relink of the same objects, not a rebuild. Staging runs
		# between the two because both land at the same build path. --single_file skips the pair for a
		# quick local iteration and stages only the embedded flavor.

		if args.frontend and not args.single_file:

			stageFrontend(args.mode, singleFile=False)

			doBuild(
				args.mode, False, args.tests == "True", cache, args.host_crypto or threads, threads,
				args.asan == "True", args.ubsan == "True", True, args.dxc_source
			)

			stageFrontend(args.mode, singleFile=True)

		elif args.frontend:
			stageFrontend(args.mode, singleFile=True)

		common.saveHashCache(cache)

	if args.frontend and args.skip_build:
		stageFrontend(args.mode, singleFile=args.single_file)

	if args.precompress:
		precompressFrontend()

	if args.run_frontend_tests:
		runFrontendTests(args.mode)

	if args.run_tests:
		runTests(args.mode, args.suite, args.threads == "True", args.asan == "True", args.ubsan == "True")

	if args.serve is not None:
		serveFrontend(args.serve)

if __name__ == "__main__":
	main()
