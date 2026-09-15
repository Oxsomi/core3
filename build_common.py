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
import re
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.realpath(__file__))

HASH_CACHE_FILE = os.path.join(ROOT, ".dep_hashes.json")

ALL_MODES = [ "Release", "Debug", "RelWithDebInfo", "MinSizeRel" ]

# Matches the floor documented in README.md.
# dirs_exist_ok (the apk step) is what actually needs 3.8
MIN_PYTHON = (3, 8)

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

	# Inside a flatpak sandbox /etc/os-release reflects the container, not the host.
	# Use flatpak-spawn --host to read the real host OS file instead.

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

def ensurePythonVersion():
	"""Refuse to start on an interpreter older than the scripts need.

	Checked up front because the failure otherwise arrives very late and unrecognisably. Everything here
	parses on 3.7 and the only newer thing we use is shutil.copytree's dirs_exist_ok, in the apk step - so
	an old interpreter cheerfully runs the entire cross build, then dies minutes later on a bare TypeError
	that says nothing about python versions. Windows makes this easy to hit by accident: Visual Studio puts
	a 3.7 on PATH as `python`, ahead of whatever newer one is installed as `python3`.
	"""

	if sys.version_info >= MIN_PYTHON:
		return

	wanted  = ".".join(str(v) for v in MIN_PYTHON)
	current = ".".join(str(v) for v in sys.version_info[:3])

	print(f"-- Python {wanted}+ required, this is {current} ({sys.executable})", file=sys.stderr)

	# Usually the machine already has a new enough one under a different name, so name it rather than
	# leaving the reader to go looking

	for name in ("python3", "py"):

		found = shutil.which(name)

		if not found or os.path.realpath(found) == os.path.realpath(sys.executable):
			continue

		version = subprocess.run(
			[ found, "-c", "import sys; print('.'.join(str(v) for v in sys.version_info[:3]))" ],
			capture_output=True, text=True
		)

		if version.returncode or tuple(int(v) for v in version.stdout.strip().split(".")) < MIN_PYTHON:
			continue

		print(f"-- {name} is {version.stdout.strip()}, run: {name} {os.path.basename(sys.argv[0])} ...", file=sys.stderr)
		break

	sys.exit(1)

def ensureCorrectEnvironment(script):
	"""On SteamOS, re-exec `script` inside the steamrt4-sdk distrobox, escaping the flatpak sandbox
	first if VSCode itself is sandboxed. No-op on Windows / macOS / plain Linux."""

	ensurePythonVersion()

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

# Compiler each platform uses when -compiler isn't given.
# These are the combinations that have always been built; the alternatives exist so CI can cover a second toolchain,
# not to change anyone's default.
DEFAULT_COMPILERS = { "Windows": "msvc", "Darwin": "clang", "Linux": "gcc" }

# What each platform can actually be built with.
# Deliberately excludes combinations that would be a new target rather than a new compiler:
# MinGW gcc doesn't share Windows' CRT or ABI, and `gcc` on macOS is a clang symlink,
# so a gcc job there would silently retest clang.
SUPPORTED_COMPILERS = { "Windows": ("msvc", "clang"), "Darwin": ("clang",), "Linux": ("gcc", "clang") }

def defaultCompiler():
	return DEFAULT_COMPILERS[hostSystem()]

# The executable each profile's compiler is detected through. Has to match what the profile template
# asks detect_api for, or the guard below would check a different compiler than the build uses.

PROFILE_COMPILER_EXE = {
	("Windows", "msvc"):  "cl",
	("Windows", "clang"): "clang-cl",
	("Darwin",  "clang"): "clang",
	("Linux",   "gcc"):   "gcc",
	("Linux",   "clang"): "clang"
}

def resolveCompilerExe(exe):
	"""`exe` as something runnable, or None.

	cl and clang-cl only reach PATH inside a developer command prompt, and the Visual Studio generator finds
	its own toolchain regardless, so a check that insisted on PATH would fail on machines, and on CI runners,
	where the build itself works perfectly well.
	Conan's own detection is split on this: detect_msvc_compiler goes through vswhere and needs nothing on
	PATH, while detect_clang_compiler simply runs the executable (see ensureCompilerOnPath below).
	"""

	found = shutil.which(exe)

	if found or hostSystem() != "Windows" or exe not in ("cl", "clang-cl"):
		return found

	vswhere = os.path.join(
		os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"),
		"Microsoft Visual Studio", "Installer", "vswhere.exe"
	)

	if not os.path.isfile(vswhere):
		return None

	try:
		install = capture('"' + vswhere + '" -latest -products * -property installationPath').strip()
	except Exception:
		return None

	if not install:
		return None

	# Visual Studio lays both toolchains out per host architecture, so an arm64 install has no x64
	# directory at all and a hardcoded one finds nothing on it. The host's own comes first and the other
	# is a fallback rather than an alternative: arm64 Windows runs x64 binaries under emulation, so an
	# x64 toolchain there works and is better than failing, while the reverse never resolves.

	hostFirst = ( "ARM64", "x64" ) if hostArch()[0] == "aarch64" else ( "x64", "ARM64" )

	if exe == "clang-cl":

		for arch in hostFirst:
			candidate = os.path.join(install, "VC", "Tools", "Llvm", arch, "bin", "clang-cl.exe")
			if os.path.isfile(candidate):
				return candidate

		return None

	toolsets = os.path.join(install, "VC", "Tools", "MSVC")

	if not os.path.isdir(toolsets):
		return None

	# cl is additionally split by what it targets, and only the native pair is looked for: a cross
	# compiler would report the same version while building for the wrong architecture.

	for version in sorted(os.listdir(toolsets), reverse=True):
		for arch in hostFirst:
			candidate = os.path.join(toolsets, version, "bin", f"Host{arch}", arch, "cl.exe")
			if os.path.isfile(candidate):
				return candidate

	return None

def detectedCompilerVersion(exe, compiler = None):
	"""The version `exe` actually reports, or None if it isn't there.

	Only the major matters, which is what conan keys package ids on for every compiler this repo uses.
	"""

	resolved = resolveCompilerExe(exe)

	if not resolved:
		return None

	for args in ( [ resolved, "-dumpfullversion" ], [ resolved, "--version" ] ):

		try:
			probe = subprocess.run(args, capture_output=True, text=True, timeout=10)
		except Exception:
			continue

		# cl has no version flag and writes its banner to stderr, so both streams are read and a non zero
		# return code is not taken as "no answer".

		text = (probe.stdout or "") + (probe.stderr or "")

		if compiler == "msvc":

			# conan spells msvc as major*10 + minor/10, so 19.38 is 193 rather than 19.

			found = re.search(r"Version\s+([0-9]+)\.([0-9]+)", text)

			if found:
				return str(int(found.group(1)) * 10 + int(found.group(2)) // 10)

			continue

		if probe.returncode:
			continue

		found = re.search(r"([0-9]+)(\.[0-9]+)*", text)

		if found:
			return found.group(1)

	return None

def ensureCompilerOnPath(exe):
	"""Put `exe` on PATH when it was only found through a Visual Studio install.

	The Windows clang profiles detect their version with detect_api.detect_clang_compiler("clang-cl"), which
	runs the executable and so only ever looks at PATH. resolveCompilerExe finds the Visual Studio copy as
	well, so without this the guard below passes on a machine that has clang-cl while RENDERING the profile
	dies with "No version provided to 'detect_api.default_compiler_version()' for clang compiler".
	It is also what lets CMAKE_C_COMPILER resolve, since tools.build:compiler_executables names clang-cl
	rather than a full path.

	Only clang needs it: detect_msvc_compiler goes through vswhere, and prepending the MSVC bin directory
	would shadow tools that share a name with one of its own (link, most of all) for the rest of the process.
	"""

	if exe != "clang-cl" or shutil.which(exe):
		return

	resolved = resolveCompilerExe(exe)

	if resolved:
		os.environ["PATH"] = os.path.dirname(resolved) + os.pathsep + os.environ.get("PATH", "")

def verifyHostCompiler(compiler, system):
	"""Fail early when the profile can't describe this machine.

	The profiles detect their compiler's version rather than pinning one, so a machine that lacks that
	compiler, or has one newer than conan understands, produces a conan error deep into the first
	dependency build. Both cases are diagnosable up front and the fix differs, so say which it is.
	"""

	exe = PROFILE_COMPILER_EXE.get(( system, compiler ))

	if not exe:
		return

	ensureCompilerOnPath(exe)

	version = detectedCompilerVersion(exe, compiler)

	if version is None:
		print(
			f"Profile compiler '{compiler}' selected, but '{exe}' was not found on PATH or in a Visual Studio install.\n"
			f"Install it, or pick another with -compiler=<{'|'.join(SUPPORTED_COMPILERS[system])}>.",
			file=sys.stderr
		)
		sys.exit(1)

	known = conanKnownCompilerVersions(compiler)

	if known and version not in known:
		print(
			f"{exe} reports version {version}, which this conan doesn't accept for '{compiler}' "
			f"(it knows up to {known[-1]}).\n"
			f"Upgrade conan, or widen compiler.version in settings_user.yml.",
			file=sys.stderr
		)
		sys.exit(1)

def hostProfileBase(compiler = None):

	arch = hostArch()[0]
	system = hostSystem()
	compiler = compiler or defaultCompiler()

	supported = SUPPORTED_COMPILERS[system]

	if compiler not in supported:
		print(f"Unsupported compiler '{compiler}' on {system}; expected one of {', '.join(supported)}", file=sys.stderr)
		sys.exit(1)

	verifyHostCompiler(compiler, system)

	if system == "Windows":
		return f"packages/conan/profiles/windows_{compiler}_{arch}"

	if system == "Darwin":
		return f"packages/conan/profiles/osx_{compiler}_{arch}"

	return f"packages/conan/profiles/linux_{compiler}_{arch}"

def hostProfileForMode(mode, compiler = None):
	"""On Windows the profile is per-mode (the debug/release CRT differs), elsewhere one covers all."""

	base = hostProfileBase(compiler)
	return f"{base}_{mode}" if hostSystem() == "Windows" else base

def visualStudioNinjaConf(compiler):
	"""Conf a Ninja build on Windows needs, and an early check that it can work at all.

	The Visual Studio generator never runs vcvars; it sets its own environment up. Ninja does run it, which
	surfaces two things that generator hid:

	  - vswhere lives in the VS Installer folder and is usually not on PATH, so vcvars cannot find the
	    install. Pointing conan straight at the installation removes the need for it.
	  - conan derives -vcvars_ver from the profile's pinned toolset (v144 -> 14.4). If that toolset is not
	    installed, vcvars fails with a message that says nothing about profiles, so it is checked here.
	"""

	if hostSystem() != "Windows":
		return ""

	vswhere = os.path.join(
		os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
		"Microsoft Visual Studio", "Installer", "vswhere.exe"
	)

	if not os.path.isfile(vswhere):
		print("-- Error: -generator ninja needs Visual Studio; vswhere.exe was not found", file=sys.stderr)
		sys.exit(1)

	install = capture(f'"{vswhere}" -latest -products * -property installationPath').strip()

	if not install:
		print("-- Error: -generator ninja needs Visual Studio; vswhere reported no installation", file=sys.stderr)
		sys.exit(1)

	# The profiles pin a runtime_version, and conan turns that into the toolset vcvars is asked for.

	wanted = "14.4"                              #v144, what both windows profiles pin
	toolsets = os.path.join(install, "VC", "Tools", "MSVC")
	have = sorted(os.listdir(toolsets)) if os.path.isdir(toolsets) else []

	if not any(v.startswith(wanted) for v in have):
		print(
			f"-- Error: -generator ninja needs MSVC toolset {wanted}x, which the profiles pin through\n"
			f"   compiler.runtime_version. Installed: {', '.join(have) or 'none'}.\n"
			f"   Install it through the VS Installer, or build without -generator ninja; the Visual Studio\n"
			f"   generator does not use vcvars and is unaffected.",
			file=sys.stderr
		)
		sys.exit(1)

	return f'-c tools.microsoft.msbuild:installation_path="{install}" '

def hostProfileArgs(mode, compiler = None):
	p = hostProfileForMode(mode, compiler)
	return f"--profile:build={p} --profile:host={p}"

def crossBuildProfileArgs(compiler = None):
	"""Profile args for the *build* context of a cross build (android).

	conan derives a tool_requires' package id from the consumer's build profile, so this has to be exactly
	the profile buildHostToolPackage used, otherwise conan decides the prebuilt host package is a different
	binary and rebuilds it from source. Without this it silently falls back to the auto-detected `default`
	profile, whose cppstd/runtime differ from the checked-in ones.
	"""

	return f"--profile:build={hostProfileForMode(HOST_TOOL_MODE, compiler)} -s:b build_type={HOST_TOOL_MODE}"

def ensureDefaultProfile():
	"""Make sure conan has a `default` profile.

	The checked-in host profiles are fully specified so they don't need one, but the android profile
	starts with include(default) to pick up the host compiler for the build context. `conan profile
	detect` errors out when the profile already exists (and --exist-ok only landed in 2.1), so check first.
	"""

	default = os.path.join(capture("conan config home"), "profiles", "default")

	if not os.path.isfile(default):
		run("conan profile detect")

def conanKnownCompilerVersions(compiler):
	"""The compiler.version values this conan install accepts, oldest first.

	Read out of the cache's settings.yml rather than asked of conan, which only exposes [conf] through
	`conan config`. settings_user.yml is read after it because it *replaces* the list it names rather than
	extending it (the `version+:` append form isn't in conan 2.11), so a user who already widened the list
	by hand is respected. Returns [] if either file isn't there or doesn't parse, which callers treat as
	"don't second guess conan".
	"""

	versions = []

	# `^\s+clang:$` can't match `apple-clang:`, since a '-' isn't whitespace.
	# version is the first key under the compiler, so the next one in the file is the one we're after; it wraps across lines.

	header = re.compile(r"^\s+" + re.escape(compiler) + r":\s*$", re.MULTILINE)
	values = re.compile(r"version:\s*\[(.*?)\]", re.DOTALL)

	for name in ("settings.yml", "settings_user.yml"):

		path = os.path.join(capture("conan config home"), name)

		if not os.path.isfile(path):
			continue

		with open(path, encoding="utf-8") as f:
			settings = f.read()

		start = header.search(settings)

		if not start:
			continue

		found = values.search(settings, start.end())

		if not found:
			continue

		versions = [ v.strip().strip("\"'") for v in found.group(1).split(",") if v.strip() ]

	return versions

def conanCompilerVersion(compiler, version):
	"""`version`, or the newest one conan accepts if it's never heard of it.

	NDKs ship clang well ahead of conan's settings.yml (NDK 29 is clang 20, which conan only learned in
	2.12), and an unknown value is a hard error rather than a warning - "Invalid setting '20' is not a
	valid 'settings.compiler.version' value" - which kills the first dependency build. Nothing here reads
	the number back, so reporting the newest conan knows keeps a current NDK working on an older conan
	instead of making every NDK bump a conan bump too.

	The alternative, widening the list in settings_user.yml, is worse: it has to restate every version
	conan already ships (there's no append), and it edits global config to fix one repo's build.
	"""

	known = conanKnownCompilerVersions(compiler)

	if not known or version in known:
		return version

	def asTuple(value):
		return tuple(int(part) for part in value.split(".") if part.isdigit())

	older = [ v for v in known if asTuple(v) and asTuple(v) <= asTuple(version) ]

	if not older:
		return version                                     # nothing sane to fall back to, let conan object

	fallback = max(older, key=asTuple)

	print(f"-- conan doesn't know {compiler} {version}, building as {compiler} {fallback}")
	print(f"-- Upgrade conan if you want the real version in the package id")

	return fallback

# ---------------------------------------------------------------------------------------------------
# Recipe introspection
#
# Read constants straight out of conanfile.py rather than duplicating them here.
# Importing it isn't an option (it needs the conan python package, which may live in a different interpreter than ours),
# so parse the AST instead; these are plain literals.
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

# Recipes shared through python_requires rather than living in the package folder itself.
# They have to be part of every dependent's hash, otherwise editing the shared logic leaves the cache thinking
# nothing changed and the packages that consume it are never rebuilt.

SHARED_RECIPES = ( "packages/sanitizers", )

def hashPackage(packagePath, profilePath, mode):

	h = hashlib.sha256()
	h.update(mode.encode())

	if os.path.isfile(profilePath):
		with open(profilePath, "rb") as f:
			h.update(f.read())

	folders = [ packagePath ]

	for shared in SHARED_RECIPES:

		resolved = shared if os.path.isabs(shared) else os.path.join(ROOT, shared)

		# A package is only rehashed for a shared recipe it doesn't already contain, so hashing
		# packages/sanitizers itself doesn't fold it in twice.

		if os.path.isdir(resolved) and os.path.normpath(resolved) != os.path.normpath(packagePath):
			folders.append(resolved)

	for folder in folders:
		for root, _, files in os.walk(folder):
			for fname in sorted(files):
				fpath = os.path.join(root, fname)
				h.update(fpath.encode())
				with open(fpath, "rb") as f:
					h.update(f.read())

	return h.hexdigest()

_sharedRecipesExported = False

def exportSharedRecipes():
	"""conan export the python_requires recipes that the dependency recipes consume.

	A python_requires has to be resolvable from the local cache before any consumer is created, otherwise the
	consumer's recipe fails to load. Export doesn't build anything, so this is cheap; it still only runs once
	per process since nothing changes underneath it mid-build.
	"""

	global _sharedRecipesExported

	if _sharedRecipesExported:
		return

	for shared in SHARED_RECIPES:

		resolved = shared if os.path.isabs(shared) else os.path.join(ROOT, shared)

		if os.path.isdir(resolved):
			run(f"conan export \"{resolved}\"", cwd=ROOT)

	_sharedRecipesExported = True

def loadHashCache():

	if os.path.isfile(HASH_CACHE_FILE):
		with open(HASH_CACHE_FILE, "r") as f:
			return json.load(f)

	return {}

def saveHashCache(cache):
	with open(HASH_CACHE_FILE, "w") as f:
		json.dump(cache, f, indent=2)

def pruneDeadGenerators(buildDir):
	"""Drop CMakeDeps files pointing at a package folder that is no longer in the conan cache.

	conan writes one set of generator files per build type into a shared folder,
	and <pkg>Targets.cmake globs every one of them back in,
	so a config this run never installs is still included.
	Evict that config's package and the configure dies on a library it cannot find,
	for a build type nobody asked for.
	A generator file whose package folder is gone has nothing left to offer, so it goes.
	"""

	generators = os.path.join(buildDir, "build", "generators")

	if not os.path.isdir(generators):
		return

	for name in sorted(os.listdir(generators)):

		# conan names these <package>-<config>-<arch>-data.cmake, alongside <package>-Target-<config>.cmake

		match = re.fullmatch(r"(.+)-([^-]+)-[^-]+-data\.cmake", name)

		if not match:
			continue

		path = os.path.join(generators, name)

		with open(path, "r", encoding="utf-8", errors="ignore") as f:
			folder = re.search(r'set\([A-Za-z0-9_]*PACKAGE_FOLDER[A-Za-z0-9_]*\s+"([^"]+)"\)', f.read())

		if not folder or os.path.isdir(folder.group(1)):
			continue

		package, config = match.group(1), match.group(2)

		print(f"-- Dropping stale {package} {config} generator files, its package left the conan cache")

		for dead in [ path, os.path.join(generators, f"{package}-Target-{config}.cmake") ]:
			if os.path.isfile(dead):
				os.remove(dead)

def conanCreateIfChanged(packagePath, profile, mode, profileArgs, cache, key=None, options="", force=False):
	"""conan create a dependency, skipping it when neither the recipe nor the profile changed.

	`key` disambiguates the same recipe built for different targets (host vs android) in one cache.
	Paths are resolved against the repo so this works no matter what the caller's cwd is.
	`force` builds even on a hash hit, for a caller that found the binary itself gone from the conan cache.
	"""

	# options is part of the identity: the same recipe and profile built with and without a sanitizer are
	# different binaries, and without this the cache would hand back the wrong one.

	key         = f"{key or packagePath}::{mode}{('::' + options) if options else ''}"
	resolved    = packagePath if os.path.isabs(packagePath) else os.path.join(ROOT, packagePath)
	profilePath = profile if os.path.isabs(profile) else os.path.join(ROOT, profile)
	currentHash = hashPackage(resolved, profilePath, mode)

	if not force and cache.get(key) == currentHash:
		print(f"-- Skipping {packagePath} ({mode}), unchanged")
		return

	run(f"conan create \"{resolved}\" {profileArgs} {options} -s build_type={mode} --build=missing", cwd=ROOT)

	cache[key] = currentHash

def hostTablegenDir(mode=HOST_TOOL_MODE, compiler=None):
	"""Where a host build of packages/dxc left llvm-tblgen / clang-tblgen.

	A cross build can't run the tablegens it would produce itself, and DXC's NATIVE sub build can't
	compile them either (see the LLVM_USE_HOST_TOOLS note in packages/dxc/conanfile.py). The host
	package ships them, so this finds that package and hands back its bin/.

	Returns None when there's no host dxc in the cache yet; the caller then says nothing and the build
	falls back to whatever it did before rather than failing on a missing path.
	"""

	profile = hostProfileForMode(mode, compiler)
	profile = profile if os.path.isabs(profile) else os.path.join(ROOT, profile)

	result = subprocess.run(
		f"conan graph info --requires=dxc/{dxcVersion()} "
		f"--profile:host=\"{profile}\" --profile:build=\"{profile}\" "
		f"-s build_type={mode} --format=json",
		shell=True, capture_output=True, text=True, cwd=ROOT
	)

	if result.returncode:
		return None

	try:
		graph = json.loads(result.stdout)
	except json.JSONDecodeError:
		return None

	# graph info reports what the binary *is* but not where it lives, so resolve the folder separately.
	# ref already carries the recipe revision, and package_id + prev pin the exact binary, which is what
	# `conan cache path` wants.

	for node in graph.get("graph", {}).get("nodes", {}).values():

		ref = str(node.get("ref", ""))

		if not ref.startswith("dxc/") or not node.get("package_id") or not node.get("prev"):
			continue

		located = subprocess.run(
			f"conan cache path {ref}:{node['package_id']}#{node['prev']}",
			shell=True, capture_output=True, text=True, cwd=ROOT
		)

		if located.returncode:
			continue

		folder = os.path.join(located.stdout.strip(), "bin")

		if os.path.isdir(folder):
			return folder

	return None

def dxcVersion():
	"""The dxc version the root recipe pins, so the two can't drift."""

	text = open(os.path.join(ROOT, "conanfile.py"), encoding="utf-8").read()
	found = re.search(r'"dxc/([^"]+)"', text)

	return found.group(1) if found else None

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

def buildHostDependencies(modes, cache, debugShaderCompiler=False, compiler=None, asan=False, ubsan=False):
	"""Create everything oxc3 needs for a *host* build (which includes the OxC3_package tool build)."""

	system = hostSystem()

	# dxc, spirv_reflect and openal_soft share their sanitizer wiring through a python_requires, which has to
	# be in the cache before any of them is created.

	exportSharedRecipes()

	# Only the packages that actually compile C/C++ take these; the rest are headers or prebuilt binaries.
	# A sanitized consumer can't link unsanitized dependencies: MSVC's STL records its ASan container
	# annotation state per object and lld-link rejects the mix, and ASan has to own operator new, which
	# DXC otherwise replaces.

	sanitizerOptions = ""

	if asan or ubsan:
		sanitizerOptions = f"-o enableASAN={asan} -o enableUBSAN={ubsan}"

	for mode in modes:

		profile     = hostProfileForMode(mode, compiler)
		profileArgs = hostProfileArgs(mode, compiler)

		if system == "Windows":
			conanCreateIfChanged("packages/amd_ags",        profile, mode, profileArgs, cache)

		conanCreateIfChanged("packages/agility_sdk",        profile, mode, profileArgs, cache)
		conanCreateIfChanged("packages/nvapi",              profile, mode, profileArgs, cache)

		# RGA is a tool package (settings = os/arch only), so it builds once regardless of mode.
		# The hash cache makes the repeat calls free.
		# Windows/Linux x64 only, since it vendors AMD's compilers.

		if system in ("Windows", "Linux") and hostArch()[1] == "x86_64":
			conanCreateIfChanged("packages/radeon_gpu_analyzer", profile, mode, profileArgs, cache)

		conanCreateIfChanged("packages/vulkan_headers",     profile, mode, profileArgs, cache)
		# These two follow shaderCompilerDepMode rather than the requested mode

		shaderMode = shaderCompilerDepMode(mode, debugShaderCompiler)
		shaderProfile = hostProfileForMode(shaderMode, compiler)
		shaderArgs = hostProfileArgs(shaderMode, compiler)

		# On Windows a sanitized DXC can't build its own tablegen: the instrumented llvm-tblgen/clang-tblgen
		# abort mid-build for want of the sanitizer runtime (STATUS_ENTRYPOINT_NOT_FOUND). Do it the way the
		# android/web cross builds do (see build_android.py): build an UNSANITIZED host DXC purely for its
		# tablegen binaries, then point the sanitized build at them with user.dxc:tablegen_dir, which makes the
		# recipe set LLVM_USE_HOST_TOOLS=OFF and consume those instead of building its own. Unsanitized tools
		# need no runtime, so they run fine. Linux/macOS build tablegen inline without trouble and skip this.

		dxcTablegenConf = ""

		# DXC is ASan-sanitized on Windows (never UBSan), so the trigger is `asan`, not `asan or ubsan`.

		if asan and system == "Windows":

			conanCreateIfChanged(
				"packages/dxc", shaderProfile, shaderMode, shaderArgs, cache,
				key="packages/dxc::host_tablegen", options=""
			)

			# hostTablegenDir resolves the graph with default (unsanitized) options, so it finds the host DXC
			# just built rather than the sanitized one. The conf is not part of the package id.

			tablegenDir = hostTablegenDir(mode=shaderMode, compiler=compiler)

			# The create above skips on an unchanged recipe and profile,
			# which is a claim about what this script built, not about what is still in the conan cache.
			# A cache clean evicts the binary while the local hash goes on claiming it is there,
			# and the host package is nothing's dependency, so no --build=missing ever brings it back.
			# Build it again ignoring that claim rather than leaving the graph Missing.

			if not tablegenDir:

				print("-- Host DXC tablegen is gone from the conan cache, rebuilding it")

				conanCreateIfChanged(
					"packages/dxc", shaderProfile, shaderMode, shaderArgs, cache,
					key="packages/dxc::host_tablegen", options="", force=True
				)

				tablegenDir = hostTablegenDir(mode=shaderMode, compiler=compiler)

			# Going on without it is not a degraded build but a guaranteed failure:
			# DXC compiles its own tablegens with ASan and then RUNS them mid-build,
			# where they abort with STATUS_ENTRYPOINT_NOT_FOUND for want of the sanitizer runtime.
			# Say so here rather than half an hour later inside someone else's CMake.

			if not tablegenDir:
				raise RuntimeError(
					"No host DXC tablegen available and a sanitized Windows DXC cannot build its own. "
					"Check that packages/dxc creates cleanly unsanitized for this profile."
				)

			dxcTablegenConf = f' -c:h user.dxc:tablegen_dir="{tablegenDir}"'

		for package in SHADER_COMPILER_DEPS:
			depOptions = (sanitizerOptions + dxcTablegenConf) if package == "packages/dxc" else sanitizerOptions
			conanCreateIfChanged(package, shaderProfile, shaderMode, shaderArgs, cache, options=depOptions.strip())
		conanCreateIfChanged("packages/openal_soft",        profile, mode, profileArgs, cache, options=sanitizerOptions)

		if system == "Linux":
			conanCreateIfChanged("packages/xdg_shell",      profile, mode, profileArgs, cache)
			conanCreateIfChanged("packages/xdg_decoration", profile, mode, profileArgs, cache)

# ---------------------------------------------------------------------------------------------------
# The host tool package
# ---------------------------------------------------------------------------------------------------

def buildHostToolPackage(mode=HOST_TOOL_MODE, forceDeps=False, compiler=None):
	"""Build and export oxc3 for the host with the shader compiler + CLI enabled, so that a build which
	can't run its own packager (android, or any cross build) can tool_requires it.

	Deliberately built from this working tree via `conan build` + `conan export-pkg` rather than
	`conan create`: the recipe's source() clones core3 from github, which would package whatever is on
	main instead of what's checked out here.
	"""

	if forceDeps and os.path.isfile(HASH_CACHE_FILE):
		os.remove(HASH_CACHE_FILE)

	cache = loadHashCache()
	buildHostDependencies([ mode ], cache, compiler=compiler)
	saveHashCache(cache)

	profileArgs = hostProfileArgs(mode, compiler)
	options     = hostToolOptionArgs()
	reference   = f"{recipeName()}/{recipeVersion()}"

	# A CMake cache is tied to the compiler that configured it, the same reason build.py gives each
	# toolchain its own tree.
	# Sharing one folder here means -host_compiler silently reuses whatever configured it last, and the
	# cache keeps the old toolset while the new profile picks the flags: MSVC then gets handed clang's
	# -Wno-* and dies on D8021, or the reverse.
	# The default compiler keeps `build` so existing trees stay valid; anything else nests beside it.

	suffix       = "" if (compiler or defaultCompiler()) == defaultCompiler() else f"/host_{compiler}"
	outputFolder = f"build{suffix}"

	print(f"-- Building {reference} for the host ({hostPlatformName()}/{hostArch()[0]}, {mode}) to package virtual files")

	# export-pkg packages whatever is sitting in bin/, and CMakeLists pins the output directory to a source-relative path,
	# so every configuration shares it regardless of the build folder.
	# A normal build.py run leaves OxC3_shader_compiler.dll there; the host tool is static (HOST_TOOL_OPTIONS) and would ship a
	# ~29 MB DLL it never built, and that nothing loads.
	# Clear it first so what gets exported is a function of the options rather than of whatever was built here last.

	# Matches the suffix CMakeLists puts on the output directory for a non default toolchain; without it
	# this would clean the default compiler's bin/ and leave the real stale DLL in place.

	archName = hostArch()[0]

	if (compiler or defaultCompiler()) != defaultCompiler():
		archName += f"_{compiler}"

	binDir = os.path.join(ROOT, "build", mode, hostPlatformName(), archName, "bin")

	for name in ("OxC3_shader_compiler.dll", "libOxC3_shader_compiler.so", "libOxC3_shader_compiler.dylib"):

		stale = os.path.join(binDir, name)

		if os.path.isfile(stale):
			print(f"-- Removing stale {name} before packaging the static host tool")
			os.remove(stale)

	run(f"conan build . -of {outputFolder} {profileArgs} -s build_type={mode} {options} --build=missing", cwd=ROOT)
	run(f"conan export-pkg . -of {outputFolder} {profileArgs} -s build_type={mode} {options}", cwd=ROOT)
