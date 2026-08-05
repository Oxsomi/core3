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

"""Shared plumbing for build.py (host builds) and build_android.py (cross builds).

Everything in here is host-oriented: process helpers, host detection, conan profiles and the
dependency cache. build_android.py adds the NDK/APK specific parts on top; the host half of an
android build (the OxC3_package tool it needs to package virtual files) goes through the exact
same code path as a normal host build.
"""

import ast
import hashlib
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.realpath(__file__))

HASH_CACHE_FILE = os.path.join(ROOT, ".dep_hashes.json")

ALL_MODES = [ "Release", "Debug", "RelWithDebInfo", "MinSizeRel" ]

# The host OxC3_package tool is always built optimized; nothing about the target changes it, and a cross
# build's *build* context has to be pinned to this same mode or conan computes a different package id for
# it and rebuilds instead of reusing what we exported.
HOST_TOOL_MODE = "Release"

# ---------------------------------------------------------------------------------------------------
# Process helpers
# ---------------------------------------------------------------------------------------------------

def run(cmd, **kwargs):
	"""Run a shell command, streaming its output, and abort the script if it fails.

	shell=True is required: the commands are written as single strings and posix would otherwise try to
	exec the whole string as one filename.
	"""

	result = subprocess.run(cmd, shell=True, **kwargs)

	if result.returncode != 0:
		print(f"-- Command failed: {cmd}", file=sys.stderr)
		sys.exit(result.returncode)

def capture(cmd, **kwargs):
	"""Run a shell command and return its stdout, aborting the script if it fails."""

	result = subprocess.run(cmd, shell=True, capture_output=True, text=True, **kwargs)

	if result.returncode != 0:
		print(f"-- Command failed: {cmd}", file=sys.stderr)
		print(result.stderr, file=sys.stderr)
		sys.exit(result.returncode)

	return result.stdout.strip()

def python():
	"""The interpreter running us; "python3" doesn't exist on a default Windows install."""
	return f"\"{sys.executable}\""

# ---------------------------------------------------------------------------------------------------
# SteamOS: the toolchain only exists inside the steamrt4-sdk distrobox
# ---------------------------------------------------------------------------------------------------

def isSteamOS():

	# Inside a flatpak sandbox /etc/os-release reflects the container, not the
	# host. Use flatpak-spawn --host to read the real host OS file instead.

	if shutil.which("flatpak-spawn"):
		try:
			result = subprocess.run(
				[ "flatpak-spawn", "--host", "cat", "/etc/os-release" ],
				capture_output=True, text=True
			)
			return "steamos" in result.stdout.lower()
		except Exception:
			return False

	# Non-flatpak: read directly

	try:
		with open("/etc/os-release") as f:
			return "steamos" in f.read().lower()
	except FileNotFoundError:
		return False

def ensureCorrectEnvironment(script):
	"""On SteamOS, re-exec `script` inside the steamrt4-sdk distrobox, escaping the flatpak sandbox
	first if VSCode itself is sandboxed. No-op on Windows / macOS / plain Linux."""

	if "--already-escaped" in sys.argv:
		sys.argv.remove("--already-escaped")
		return

	if platform.system() != "Linux" or not isSteamOS():
		return

	script    = os.path.abspath(script)
	forwarded = " ".join(shlex.quote(a) for a in sys.argv[1:])
	inner_cmd = (
		f"distrobox enter steamrt4-sdk -- python3 {shlex.quote(script)} "
		f"{forwarded} --already-escaped"
	)

	if shutil.which("flatpak-spawn"):
		cmd = [ "flatpak-spawn", "--host", "/bin/bash", "-l", "-c", inner_cmd ]
	else:
		cmd = [ "/bin/bash", "-l", "-c", inner_cmd ]

	sys.exit(subprocess.run(cmd).returncode)

# ---------------------------------------------------------------------------------------------------
# Host detection + conan profiles
# ---------------------------------------------------------------------------------------------------

def hostSystem():
	return platform.system()				# Windows / Linux / Darwin

def hostArch():
	"""Returns (arch, conanArch); arch is the name OxC3's build/ tree uses."""

	machine = platform.machine()

	if machine in ("AMD64", "x86_64"):
		return "x64", "x86_64"

	if machine in ("aarch64", "arm64", "ARM64"):
		return "aarch64", "armv8"

	print(f"Unsupported architecture: {machine}", file=sys.stderr)
	sys.exit(1)

def hostPlatformName():

	system = hostSystem()

	if system == "Windows":
		return "windows"

	if system == "Darwin":
		return "osx"

	return "linux"

def hostProfileBase():

	arch = hostArch()[0]
	system = hostSystem()

	if system == "Windows":
		return f"packages/conan/profiles/windows_msvc_{arch}"

	if system == "Darwin":
		return f"packages/conan/profiles/osx_clang_{arch}"

	return f"packages/conan/profiles/linux_gcc_{arch}"

def hostProfileForMode(mode):
	"""On Windows the profile is per-mode (the MSVC debug/release CRT differs), elsewhere one covers all."""

	base = hostProfileBase()
	return f"{base}_{mode}" if hostSystem() == "Windows" else base

def hostProfileArgs(mode):
	p = hostProfileForMode(mode)
	return f"--profile:build={p} --profile:host={p}"

def crossBuildProfileArgs():
	"""Profile args for the *build* context of a cross build (android).

	conan derives a tool_requires' package id from the consumer's build profile, so this has to be exactly
	the profile buildHostToolPackage used, otherwise conan decides the prebuilt host package is a different
	binary and rebuilds it from source. Without this it silently falls back to the auto-detected `default`
	profile, whose cppstd/runtime differ from the checked-in ones.
	"""

	return f"--profile:build={hostProfileForMode(HOST_TOOL_MODE)} -s:b build_type={HOST_TOOL_MODE}"

def ensureDefaultProfile():
	"""Make sure conan has a `default` profile.

	The checked-in host profiles are fully specified so they don't need one, but the android profile
	starts with include(default) to pick up the host compiler for the build context. `conan profile
	detect` errors out when the profile already exists (and --exist-ok only landed in 2.1), so check first.
	"""

	default = os.path.join(capture("conan config home"), "profiles", "default")

	if not os.path.isfile(default):
		run("conan profile detect")

# ---------------------------------------------------------------------------------------------------
# Recipe introspection
#
# Read constants straight out of conanfile.py rather than duplicating them here. Importing it isn't an
# option (it needs the conan python package, which may live in a different interpreter than ours), so
# parse the AST instead; these are plain literals.
# ---------------------------------------------------------------------------------------------------

def _recipeConstant(name, insideClass):

	with open(os.path.join(ROOT, "conanfile.py"), encoding="utf-8") as f:
		tree = ast.parse(f.read())

	if insideClass:
		nodes = [ n for c in tree.body if isinstance(c, ast.ClassDef) for n in c.body ]
	else:
		nodes = tree.body

	for node in nodes:
		if isinstance(node, ast.Assign):
			for target in node.targets:
				if isinstance(target, ast.Name) and target.id == name:
					return ast.literal_eval(node.value)

	print(f"-- Couldn't find '{name}' in conanfile.py", file=sys.stderr)
	sys.exit(1)

def recipeName():
	return _recipeConstant("name", True)

def recipeVersion():
	return _recipeConstant("version", True)

def hostToolOptions():
	"""The options oxc3 is built with when it's consumed as a host *tool* (the OxC3_package packager).

	conanfile.py's build_requirements() hands the exact same dict to tool_requires, so a build that
	can't run the packager itself (android: enableShaderCompiler=False) resolves the package we
	produce here. They have to match exactly or conan considers it a different binary and falls back
	to rebuilding oxc3 from git rather than from this working tree.
	"""

	return _recipeConstant("HOST_TOOL_OPTIONS", False)

def hostToolOptionArgs():
	# & is the consumer pattern; it has to be quoted or cmd.exe treats it as a command separator.
	return " ".join(f"-o \"&:{k}={v}\"" for k, v in hostToolOptions().items())

# ---------------------------------------------------------------------------------------------------
# Dependency building (hash cached, so unchanged recipes don't get re-created every run)
# ---------------------------------------------------------------------------------------------------

def hashPackage(packagePath, profilePath, mode):

	h = hashlib.sha256()
	h.update(mode.encode())

	if os.path.isfile(profilePath):
		with open(profilePath, "rb") as f:
			h.update(f.read())

	for root, _, files in os.walk(packagePath):
		for fname in sorted(files):
			fpath = os.path.join(root, fname)
			h.update(fpath.encode())
			with open(fpath, "rb") as f:
				h.update(f.read())

	return h.hexdigest()

def loadHashCache():

	if os.path.isfile(HASH_CACHE_FILE):
		with open(HASH_CACHE_FILE, "r") as f:
			return json.load(f)

	return {}

def saveHashCache(cache):
	with open(HASH_CACHE_FILE, "w") as f:
		json.dump(cache, f, indent=2)

def conanCreateIfChanged(packagePath, profile, mode, profileArgs, cache, key=None):
	"""conan create a dependency, skipping it when neither the recipe nor the profile changed.

	`key` disambiguates the same recipe built for different targets (host vs android) in one cache.
	Paths are resolved against the repo so this works no matter what the caller's cwd is.
	"""

	key         = f"{key or packagePath}::{mode}"
	resolved    = packagePath if os.path.isabs(packagePath) else os.path.join(ROOT, packagePath)
	profilePath = profile if os.path.isabs(profile) else os.path.join(ROOT, profile)
	currentHash = hashPackage(resolved, profilePath, mode)

	if cache.get(key) == currentHash:
		print(f"-- Skipping {packagePath} ({mode}), unchanged")
		return

	run(f"conan create \"{resolved}\" {profileArgs} -s build_type={mode} --build=missing", cwd=ROOT)

	cache[key] = currentHash

# DXC and SPIRV-Reflect dominate a from-scratch build and are almost never the thing being debugged,
# so they're pinned to Release unless -debug_shader_compiler asks otherwise.
# Both the package build and the consuming build have to agree, hence shaderCompilerDepArgs() below.

SHADER_COMPILER_DEPS = ( "packages/dxc", "packages/spirv_reflect" )

def shaderCompilerDepMode(mode, debugShaderCompiler):
	return mode if debugShaderCompiler else "Release"

def shaderCompilerDepArgs(debugShaderCompiler):
	"""Per-package settings so a Debug consumer still resolves the Release dxc/spirv_reflect packages."""

	if debugShaderCompiler:
		return ""

	args = []

	for package in ("dxc", "spirv_reflect"):

		args.append(f"-s {package}/*:build_type=Release")

		# build_type alone doesn't move compiler.runtime_type, so the package_id still wouldn't match the Release build.
		# Pinning it is safe rather than an ABI mismatch because CMakeLists.txt sets CMAKE_MSVC_RUNTIME_LIBRARY to the static
		# release CRT for every config anyway.

		if hostSystem() == "Windows":
			args.append(f"-s {package}/*:compiler.runtime_type=Release")

	return " ".join(args)

def buildHostDependencies(modes, cache, debugShaderCompiler=False):
	"""Create everything oxc3 needs for a *host* build (which includes the OxC3_package tool build)."""

	system = hostSystem()

	for mode in modes:

		profile     = hostProfileForMode(mode)
		profileArgs = hostProfileArgs(mode)

		if system == "Windows":
			conanCreateIfChanged("packages/amd_ags",        profile, mode, profileArgs, cache)

		conanCreateIfChanged("packages/agility_sdk",        profile, mode, profileArgs, cache)
		conanCreateIfChanged("packages/nvapi",              profile, mode, profileArgs, cache)
		conanCreateIfChanged("packages/vulkan_headers",     profile, mode, profileArgs, cache)
		# These two follow shaderCompilerDepMode rather than the requested mode

		shaderMode = shaderCompilerDepMode(mode, debugShaderCompiler)
		shaderProfile = hostProfileForMode(shaderMode)
		shaderArgs = hostProfileArgs(shaderMode)

		for package in SHADER_COMPILER_DEPS:
			conanCreateIfChanged(package, shaderProfile, shaderMode, shaderArgs, cache)
		conanCreateIfChanged("packages/openal_soft",        profile, mode, profileArgs, cache)

		if system == "Linux":
			conanCreateIfChanged("packages/xdg_shell",      profile, mode, profileArgs, cache)
			conanCreateIfChanged("packages/xdg_decoration", profile, mode, profileArgs, cache)

# ---------------------------------------------------------------------------------------------------
# The host tool package
# ---------------------------------------------------------------------------------------------------

def buildHostToolPackage(mode=HOST_TOOL_MODE, forceDeps=False):
	"""Build and export oxc3 for the host with the shader compiler + CLI enabled, so that a build which
	can't run its own packager (android, or any cross build) can tool_requires it.

	Deliberately built from this working tree via `conan build` + `conan export-pkg` rather than
	`conan create`: the recipe's source() clones core3 from github, which would package whatever is on
	main instead of what's checked out here.
	"""

	if forceDeps and os.path.isfile(HASH_CACHE_FILE):
		os.remove(HASH_CACHE_FILE)

	cache = loadHashCache()
	buildHostDependencies([ mode ], cache)
	saveHashCache(cache)

	profileArgs = hostProfileArgs(mode)
	options     = hostToolOptionArgs()
	reference   = f"{recipeName()}/{recipeVersion()}"

	print(f"-- Building {reference} for the host ({hostPlatformName()}/{hostArch()[0]}, {mode}) to package virtual files")

	# export-pkg packages whatever is sitting in bin/, and CMakeLists pins the output directory to a source-relative path,
	# so every configuration shares it regardless of the build folder.
	# A normal build.py run leaves OxC3_shader_compiler.dll there; the host tool is static (HOST_TOOL_OPTIONS) and would ship a
	#~29 MB DLL it never built, and that nothing loads.
	# Clear it first so what gets exported is a function of the options rather than of whatever was built here last.

	binDir = os.path.join(ROOT, "build", mode, hostPlatformName(), hostArch()[0], "bin")

	for name in ("OxC3_shader_compiler.dll", "libOxC3_shader_compiler.so", "libOxC3_shader_compiler.dylib"):

		stale = os.path.join(binDir, name)

		if os.path.isfile(stale):
			print(f"-- Removing stale {name} before packaging the static host tool")
			os.remove(stale)

	run(f"conan build . {profileArgs} -s build_type={mode} {options} --build=missing", cwd=ROOT)
	run(f"conan export-pkg . {profileArgs} -s build_type={mode} {options}", cwd=ROOT)
