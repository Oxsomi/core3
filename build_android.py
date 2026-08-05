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

"""Cross build OxC3 for Android (and optionally wrap it in an apk).

Runs on Windows, Linux and macOS; everything host specific goes through build_common, which build.py
uses too, so a host build made here is the same one a normal build.py run would produce.

An android build can't run its own packager (the shader compiler is off, and the binaries wouldn't be
runnable on the host anyway), so it tool_requires a host oxc3 that has OxC3_package in it. That host
package is built first, from this working tree; see build_common.buildHostToolPackage.
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

import build_common as common

ANDROID_ARCHES = { "x64": "x86_64", "arm64": "armv8" }

def archName(conanArch):
	return "x64" if conanArch == "x86_64" else "arm64"

# ---------------------------------------------------------------------------------------------------
# NDK
# ---------------------------------------------------------------------------------------------------

def ndkToolchainBin():
	"""<ndk>/toolchains/llvm/prebuilt/<host tag>/bin; the host tag is the only entry in there."""

	root = os.path.join(os.environ["ANDROID_NDK"], "toolchains", "llvm", "prebuilt")

	if not os.path.isdir(root):
		print(f"-- ANDROID_NDK doesn't look like an NDK: {root} missing", file=sys.stderr)
		sys.exit(1)

	for entry in sorted(os.listdir(root)):
		candidate = os.path.join(root, entry, "bin")
		if os.path.isdir(candidate):
			return candidate

	print(f"-- No prebuilt toolchain found under {root}", file=sys.stderr)
	sys.exit(1)

def clangMajorVersion(binDir):
	"""Major version of the NDK's clang, which is what conan wants for compiler.version.

	Queried from the plain driver rather than the per-target wrappers: those have no extension on posix
	but are .cmd files on Windows, while clang(.exe) is spelled the same everywhere.
	"""

	clang = os.path.join(binDir, "clang.exe" if os.name == "nt" else "clang")

	if not os.path.isfile(clang):
		clang = os.path.join(binDir, "clang")

	version = common.capture(f"\"{clang}\" --version")
	found   = re.search(r"clang version ([0-9]+)\.[0-9]+\.[0-9]+", version)

	if not found:
		print(f"-- Couldn't parse clang version out of:\n{version}", file=sys.stderr)
		sys.exit(1)

	# An NDK's clang is routinely newer than the conan you have knows about, and conan rejects a version
	# it doesn't list outright rather than warning, so ask what it'll actually accept.

	return common.conanCompilerVersion("clang", found.group(1))

def profileName(conanArch, level, generator):
	return "android_" + conanArch + "_" + str(level) + "_" + generator.replace(" ", "_")

def makeProfile(conanHome, binDir, conanArch, level, generator):

	outputPath = os.path.join(conanHome, "profiles", profileName(conanArch, level, generator))
	inputPath  = os.path.join(common.ROOT, "src", "platforms", "android", "android_profile")

	profile = Path(inputPath).read_text()
	profile = profile.format(
		level=level,
		arch=conanArch,
		clangVersion=clangMajorVersion(binDir),
		androidNdk=os.environ["ANDROID_NDK"].replace("\\", "/"),
		generator=generator
	)

	os.makedirs(os.path.join(conanHome, "profiles"), exist_ok=True)

	with open(outputPath, "w") as output:
		output.write(profile)

	return outputPath

def ensureProfile(conanHome, binDir, conanArch, level, generator):

	# Always rewritten rather than reused when present. The name only encodes arch/level/generator, but the
	# contents also depend on the NDK path, its clang version and which versions conan accepts - all of
	# which change under an existing profile (an NDK upgrade, a conan upgrade), leaving a stale one that
	# fails in ways pointing nowhere near the profile. Writing a small file every build is cheaper than that.

	return makeProfile(conanHome, binDir, conanArch, level, generator)

# ---------------------------------------------------------------------------------------------------
# Cross build
# ---------------------------------------------------------------------------------------------------

def androidOptionArgs(simd, tests=False):
	"""Options for the android target itself.

	enableShaderCompiler=False is what makes conanfile.py tool_requires the host packager; DXC doesn't
	cross compile to android and we wouldn't be able to run it there anyway.

	enableTests builds OxC3_atest, one .so holding every suite (see src/CMakeLists.txt); android
	has no exec, so that's the only way to run them on device.
	"""

	options = {
		"cliGraphics": "False",
		"enableOxC3CLI": "False",
		"forceFloatFallback": "False",
		"enableTests": "True" if tests else "False",
		"dynamicLinkingGraphics": "False",
		"enableShaderCompiler": "False",
		"enableSIMD": simd
	}

	return " ".join(f"-o \"&:{k}={v}\"" for k, v in options.items())

def doBuild(mode, conanHome, binDir, conanArch, level, generator, generatorValidationLayer, simd, doInstall, tests, cache):

	profile     = profileName(conanArch, level, generator)
	profilePath = ensureProfile(conanHome, binDir, conanArch, level, generator)

	# --profile:host targets android, --profile:build stays on the host toolchain so the OxC3_package
	# tool_requires resolves to the package build_common already exported (see crossBuildProfileArgs).

	buildProfile = common.crossBuildProfileArgs()
	profileArgs  = f"--profile:host={profile} {buildProfile}"

	# Build dependencies for the target. Keyed per profile so x64 and arm64 don't invalidate each other.

	for package in ("packages/openal_soft", "packages/vulkan_headers"):
		common.conanCreateIfChanged(
			package, profilePath, mode, profileArgs, cache, key=f"{package}::{profile}"
		)

	if mode == "Debug":

		profile2      = profileName(conanArch, level, generatorValidationLayer)
		profile2Path  = ensureProfile(conanHome, binDir, conanArch, level, generatorValidationLayer)
		profile2Args  = f"--profile:host={profile2} {buildProfile}"

		common.conanCreateIfChanged(
			"packages/vulkan_validation_layers", profile2Path, mode, profile2Args, cache,
			key=f"packages/vulkan_validation_layers::{profile2}"
		)

	outputFolder = f"\"build/{mode}/android/{archName(conanArch)}\""
	options      = androidOptionArgs(simd, tests)

	common.run(
		f"conan build . -of {outputFolder} {options} -s build_type={mode} {profileArgs} --build=missing"
	)

	if doInstall:
		common.run(
			f"conan export-pkg . -of {outputFolder} {options} -s build_type={mode} {profileArgs}"
		)

# ---------------------------------------------------------------------------------------------------
# APK
# ---------------------------------------------------------------------------------------------------

def buildToolsDir():
	"""Newest build-tools install; they're named by version so sorting picks the latest."""

	candidates = sorted(glob.glob(os.path.join(os.environ["ANDROID_SDK"], "build-tools", "*")))

	if not candidates:
		print("-- No android build-tools found, install them through sdkmanager", file=sys.stderr)
		sys.exit(1)

	return candidates[-1]

def buildTool(name):
	"""Build-tools ship posix shell scripts plus .bat wrappers; pick whichever exists for this host."""

	base = os.path.join(buildToolsDir(), name)

	for candidate in ((base + ".bat", base + ".exe", base) if os.name == "nt" else (base, base + ".sh")):
		if os.path.isfile(candidate):
			return f"\"{candidate}\""

	return f"\"{base}\""

def d8Command():
	"""d8 run straight from its jar, rather than through the build-tools wrapper.

	build-tools' d8.bat/d8 sources find_java from the legacy `tools` package, which cmdline-tools
	replaced and which a modern SDK install doesn't have. When it's missing the wrapper hits
	`if not defined java_exe goto :EOF`, so it exits 0 having produced nothing, and the failure only
	surfaces later as aapt reporting "Unable to add 'classes.dex': file not found".
	The jar is the same compiler without the host-specific wrapper, so this also drops the .bat/.sh split.
	apksigner is fine as-is: its wrapper tries java from PATH before falling back to JAVA_HOME.
	"""

	jar = os.path.join(buildToolsDir(), "lib", "d8.jar")

	if not os.path.isfile(jar):
		print(f"-- No d8.jar in {buildToolsDir()}/lib, install build-tools through sdkmanager", file=sys.stderr)
		sys.exit(1)

	java = os.path.join(os.environ["JAVA_HOME"], "bin", "java") if "JAVA_HOME" in os.environ else "java"
	return f"\"{java}\" -cp \"{jar}\" com.android.tools.r8.D8"

def makeApk(args, packageDirs):

	if args.package is None or args.version is None or args.lib is None or args.name is None:
		print(
			"APK requires the following arguments (example): "
			"-package net.osomi.test -version 0.1.0 -lib test -name \"Test app\"", file=sys.stderr
		)
		return

	androidJar = "\"" + os.path.join(os.environ["ANDROID_SDK"], "platforms", f"android-{args.api}", "android.jar") + "\""

	# Make manifest

	inputPath = os.path.join(common.ROOT, "src", "platforms", "android", "AndroidManifest.xml")
	manifest  = Path(inputPath).read_text()

	manifest = manifest.format(
		APP_PACKAGE=args.package, APP_VERSION=args.version, APP_API_LEVEL=str(args.api), APP_NAME=args.name,
		APP_DEBUGGABLE="false" if args.mode == "Release" else "true", APP_CATEGORY=args.category,
		APP_LIB_NAME=args.lib,
		APP_EXPORTED="true" if (args.mode != "Release" or args.tests == "True") else "false"
	)

	apkFolder = f"build/{args.mode}/android/apk"
	shutil.rmtree(apkFolder, ignore_errors=True)
	Path(apkFolder).mkdir(parents=True, exist_ok=True)

	with open(apkFolder + "/AndroidManifest.xml", "w") as output:
		output.write(manifest)

	# Copy so file(s)

	libFolder = apkFolder + "/lib"
	Path(libFolder).mkdir(parents=True, exist_ok=True)

	for arch, abi in (("arm64", "arm64-v8a"), ("x64", "x86_64")):

		if args.arch != arch and args.arch != "all":
			continue

		Path(libFolder + "/" + abi).mkdir(parents=True, exist_ok=True)

		# Shared objects land in bin/, lib/ holds the static .a archives. Both are globbed because which
		# one a .so ends up in depends on the generator, and a missing .so here shows up as a
		# dlopen/UnsatisfiedLinkError on device rather than as a build failure.

		found = set()

		for folder in ("bin", "lib"):
			for f in glob.glob(f"build/{args.mode}/android/{arch}/{folder}/*.so"):
				if os.path.basename(f) not in found:
					found.add(os.path.basename(f))
					shutil.copy2(f, libFolder + "/" + abi)

		if not found:
			print(f"-- Warning: no .so found for {abi}, the apk will have no native code", file=sys.stderr)

	# Copy packages (the oiCA archives add_virtual_files produced)

	assetsFolder = apkFolder + "/assets"
	outputPath   = assetsFolder + "/packages"
	Path(outputPath).mkdir(parents=True, exist_ok=True)

	for packageDir in packageDirs:

		if not os.path.isdir(packageDir):
			print(f"-- No packages at {packageDir}, skipping")
			continue

		for f in sorted(glob.glob(packageDir + "/*")):

			if not os.path.isdir(f):
				continue

			# We need to make a file named section_ here, because AAssetManager can't iterate directories

			sectionName = Path(f).name
			open(outputPath + "/section_" + sectionName, 'a').close()

			# Copy folder

			Path(outputPath + "/" + sectionName).mkdir(parents=True, exist_ok=True)

			for fc in glob.glob(f + "/*"):
				shutil.copy2(fc, outputPath + "/" + sectionName + "/" + Path(fc).name)

	# Copy into /res (and provide -S res to aapt package)

	inputPath = os.path.join(common.ROOT, "src", "platforms", "android", "res")
	resFolder = apkFolder + "/res"
	Path(resFolder).mkdir(parents=True, exist_ok=True)
	shutil.copytree(inputPath, resFolder, dirs_exist_ok=True)

	# Compile .java files.
	# Wildcards are expanded here rather than handed to the shell: cmd.exe doesn't glob, so passing
	# "*.java" / "*.class" through only ever worked on a posix shell.

	print("-- Compiling .java file")
	javaFiles = " ".join(f"\"{f}\"" for f in sorted(glob.glob(os.path.join(common.ROOT, "src", "platforms", "android", "*.java"))))
	# --release pins the class file version so the installed JDK doesn't decide it. Without it javac
	# targets its own latest, and d8 rejects anything newer than it understands ("Unsupported class
	# file major version"), which made the build depend on which JDK happened to be on PATH.
	# 11 is the floor every d8 accepts, and our .java is plain Java 8 anyway.

	common.run(f"javac --release 11 -Xlint:deprecation -cp {androidJar} -d \"{apkFolder}/bin/\" {javaFiles}")

	# Compile .class files to .dex

	print("-- Compiling .class file")
	cwd = os.getcwd()
	os.chdir(apkFolder)

	try:
		classFiles = " ".join(f"\"{f}\"" for f in sorted(glob.glob("bin/net/osomi/nativeactivity/*.class")))
		common.run(f"{d8Command()} {classFiles} --min-api {args.api}")
	finally:
		os.chdir(cwd)

	# Make unsigned apk

	print("-- Creating apk")
	common.run(
		f"{buildTool('aapt')} package -f -I {androidJar} -M \"{apkFolder}/AndroidManifest.xml\" "
		f"-A \"{assetsFolder}\" -S \"{resFolder}\" -m -F \"{apkFolder}/app-unsigned.apk\""
	)

	print("-- Adding libs to apk")

	# Gotta temporary move to android/apk to avoid names from getting messed up by aapt

	cwd = os.getcwd()
	os.chdir(apkFolder)

	try:
		for f in glob.glob("lib/*"):
			for f0 in glob.glob(f + "/*.so"):
				# aapt stores the path verbatim, and an apk's entries are always /-separated
				entry = f0.replace(os.sep, "/")
				common.run(f"{buildTool('aapt')} add \"app-unsigned.apk\" \"{entry}\"")

		common.run(f"{buildTool('aapt')} add \"app-unsigned.apk\" classes.dex")
	finally:
		os.chdir(cwd)

	# Zip align and sign

	print("-- Zipalign apk")
	common.run(f"{buildTool('zipalign')} -v -f 4 \"{apkFolder}/app-unsigned.apk\" \"{apkFolder}/app.apk\"")

	if args.sign:
		signApk(args, apkFolder)

def signApk(args, apkFolder):

	# Resolved before the keystore is generated, because generating one needs the same password.

	if args.keystore_password is None:
		print("-- Sign apk, please enter keystore password")
		pwd = str(input())
	else:
		pwd = args.keystore_password

	if len(pwd) < 6:
		print("-- Keystore password has to be at least 6 characters", file=sys.stderr)
		sys.exit(1)

	if args.keystore is None:

		args.keystore = apkFolder + "/.keystore"

		if not os.path.isfile(args.keystore):

			print("-- Creating keystore")

			if "JAVA_HOME" not in os.environ:
				print("JAVA_HOME not found", file=sys.stderr)
				return

			# -storepass/-keypass/-dname are passed explicitly: keytool prompts for all of them
			# otherwise, so an unattended --sign would silently produce no keystore at all and only
			# fail later, in apksigner, as a confusing FileNotFoundException.

			keytool = os.path.join(os.environ["JAVA_HOME"], "bin", "keytool")
			common.run(
				f"\"{keytool}\" -genkeypair -v -keystore \"{args.keystore}\" -keyalg RSA -keysize 2048 "
				f"-validity 10000 -alias oxc3 -storepass \"{pwd}\" -keypass \"{pwd}\" "
				f"-dname \"CN=OxC3, OU=Dev, O=Oxsomi, C=NL\""
			)

	common.run(
		f"{buildTool('apksigner')} sign --ks \"{args.keystore}\" --ks-pass \"pass:{pwd}\" "
		f"--key-pass \"pass:{pwd}\" --min-sdk-version {args.api} \"{apkFolder}/app.apk\""
	)

	print("-- Successfully signed apk")

def runApk(args):

	apkFile = f"\"build/{args.mode}/android/apk/app.apk\""

	if args.package is None or args.lib is None:
		print("apk run requires the following arguments (example): -package net.osomi.test -lib test", file=sys.stderr)
		return

	if not args.sign and args.apk:
		print("apk run requires apk to be signed", file=sys.stderr)
		return

	adb = "\"" + os.path.join(os.environ["ANDROID_SDK"], "platform-tools", "adb") + "\""

	# -ip targets a device over the network, which frees the usb port for a keyboard/mouse hub during the
	#  interactive suites.
	# connect is idempotent (it just reports "already connected"), and -s is needed either way: adb refuses every
	#  command with "more than one device/emulator" as soon as a usb and a wireless transport are both attached.

	if args.ip:
		common.run(f"{adb} connect {args.ip}")
		adb = f"{adb} -s {args.ip}"

	print(f"-- Installing apk file ({apkFile}) using adb ({adb})")

	if args.mode == "Release":
		common.run(f"{adb} install -r {apkFile}")
	else:
		common.run(f"{adb} install -t -r {apkFile}")

	if args.mode == "Release" and args.tests != "True":
		print("-- Installed apk file, but Release mode has android:exported turned off for security reasons, please manually launch the app")
		return

	# The two functional suites need someone watching the device, so OxC3_atest skips them unless asked.
	# A system property rather than an intent extra, so it needs no JNI and no OxC3Activity change.

	if args.tests == "True":
		common.run(f"{adb} shell setprop debug.oxc3.interactive {'1' if args.interactive else '0'}")

	print("-- Running apk file")
	common.run(f"{adb} logcat -c")		# Clear log first
	subprocess.run(f"{adb} shell am start -n {args.package}/net.osomi.nativeactivity.OxC3Activity", shell=True)

	if args.tests != "True":
		print("-- Starting logcat")
		print(f"-- Failed, run it manually: {adb} logcat -s OxC3")
		return

	return tailTestRun(adb, args)

def tailTestRun(adb, args):
	"""Stream the app's log until OxC3_atest reports it's finished, and turn that into an exit code.

	`am start` doesn't give one back, so atest_main brackets the run with OXC3_TEST_BEGIN/OXC3_TEST_END
	and this looks for the latter. Interactive runs get no timeout, a human is driving them.
	"""

	print("-- Waiting for the test run (OXC3_TEST_END)")

	timeout = None if args.interactive else args.test_timeout
	deadline = None if timeout is None else time.monotonic() + timeout

	process = subprocess.Popen(
		f"{adb} logcat -s OxC3:V", shell=True,
		stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1
	)

	result = None

	try:
		for line in process.stdout:

			line = line.rstrip()

			if line:
				print(line)

			if "OXC3_TEST_END" in line:
				result = "result=PASSED" in line
				break

			if deadline is not None and time.monotonic() > deadline:
				print(f"-- No result after {timeout}s, giving up", file=sys.stderr)
				break

	finally:
		process.kill()

	if result is None:
		print("-- Test run didn't report a result (crash, or the app never started)", file=sys.stderr)
		sys.exit(1)

	if not result:
		print("-- Tests FAILED", file=sys.stderr)
		sys.exit(1)

	print("-- Tests passed")

# ---------------------------------------------------------------------------------------------------

def main():

	common.ensureCorrectEnvironment(__file__)

	parser = argparse.ArgumentParser(description="Build a lib or apk via OxC3")

	parser.add_argument("-mode", type=str, default="Release", choices=common.ALL_MODES, help="Build mode")
	parser.add_argument("-api", type=int, default=31, help="Android api level (e.g. 31 = Android 12)")
	parser.add_argument("-arch", type=str, default="all", choices=["arm64", "x64", "all"], help="Architecture")
	parser.add_argument("-simd", type=str, default="True", choices=["True", "False"], help="EnableSIMD (True by default)")
	parser.add_argument(
		"--interactive", action="store_true",
		help="With --run -tests True, also run the functional suites (they need a human watching the device)"
	)
	parser.add_argument(
		"-test_timeout", type=int, default=600,
		help="Seconds to wait for a non-interactive test run to report a result (default 600)"
	)
	parser.add_argument(
		"-tests", type=str, default="False", choices=["True", "False"],
		help="Build OxC3_atest, one .so with every unit test suite (android has no exec to run them separately)"
	)
	parser.add_argument("-generator", type=str, help="CMake Generator")
	parser.add_argument("--skip_build", help="Run full build, if false, can be used to package an already built project", action="store_true")

	parser.add_argument("-package", type=str, help="APK package name (required if -apk)")
	parser.add_argument("-version", type=str, help="APK version (required if -apk)")
	parser.add_argument("-lib", type=str, help="Final name of lib built for the APK (required if -apk)")
	parser.add_argument("-name", type=str, help="Display name of the app itself (required if -apk)")
	parser.add_argument("-category", type=str, default="game", help="APK category", choices=[ "accessibility", "audio", "game", "image", "maps", "news", "productivity", "social", "video"])
	parser.add_argument("-packages", type=str, action="append", help="Extra folder of oiCA packages to put in the apk (repeatable)")

	parser.add_argument("-keystore", type=str, help="Sign an apk with a certain keystore, if empty, will require JAVA_HOME to be set (will generate)", default=None)
	parser.add_argument("-keystore_password", type=str, help="Keystore password, if not entered, will ask it when signing", default=None)

	parser.add_argument(
		"-ip", type=str, default=None,
		help="Run on a device over the network rather than usb, e.g. 192.168.2.93 (port defaults to 5555). "
			"Frees the usb port for a keyboard/mouse hub during --interactive. "
			"Enable it first with `adb tcpip 5555` over usb, or android's Wireless debugging"
	)

	parser.add_argument("--sign", help="Sign apk if built", action="store_true")
	parser.add_argument("--run", help="Run apk if built", action="store_true")
	parser.add_argument("--apk", help="Build apk (requires -package, -version, -lib and -name)", action="store_true")

	parser.add_argument("--install", help="Install package rather than build", action="store_true")

	# The android build can't produce its own OxC3_package (no shader compiler, and it wouldn't run on the host),
	# so it tool_requires a host build of oxc3. Built here by default so a clean checkout works;
	# CI splits it out into its own step so the three android configs share one host package.

	parser.add_argument("--skip_host_package", help="Assume a host oxc3 with OxC3_package is already in the conan cache", action="store_true")
	parser.add_argument("--host_package_only", help="Only build + export the host oxc3 used for packaging, then stop", action="store_true")
	parser.add_argument("--force_deps", help="Ignore hash cache and rebuild all dependencies", action="store_true")

	args = parser.parse_args()

	# adb wants host:port, but the port is 5555 for `adb tcpip` in practice, so only the address is worth typing.
	# Wireless debugging picks a random port instead, and that one has to be given in full.

	if args.ip and ":" not in args.ip:
		args.ip += ":5555"

	# Grab generator; on Windows, this is likely MinGW Makefiles while otherwise it's probably Unix Makefiles

	if args.generator is None:
		args.generator = "MinGW Makefiles" if os.name == "nt" else "Unix Makefiles"
		generatorValidationLayer = "Ninja" if os.name == "nt" else "Unix Makefiles"
	else:
		generatorValidationLayer = args.generator

	# Host tooling. Doesn't need the NDK, so it comes before the environment check.

	if args.host_package_only:
		common.buildHostToolPackage(forceDeps=args.force_deps)
		return

	# Vulkan headers now come from the conan vulkan_headers package (see doBuild), which masquerades as
	# find_package(Vulkan), so a system VULKAN_SDK is no longer required.
	# The NDK is what compiles; the SDK only shows up for apk packaging (aapt/d8/zipalign) and adb,
	# so a libs-only build doesn't need it installed.

	required = ([] if args.skip_build else [ "ANDROID_NDK" ]) + ([ "ANDROID_SDK" ] if args.apk or args.run else [])

	for var in required:
		if var not in os.environ:
			print(f"{var} not found", file=sys.stderr)
			sys.exit(1)

	# Build

	if not args.skip_build:

		if not args.skip_host_package:
			common.buildHostToolPackage(forceDeps=args.force_deps)

		common.ensureDefaultProfile()

		conanHome = common.capture("conan config home")
		binDir    = ndkToolchainBin()

		if args.force_deps and os.path.isfile(common.HASH_CACHE_FILE):
			os.remove(common.HASH_CACHE_FILE)

		cache = common.loadHashCache()

		for arch, conanArch in ANDROID_ARCHES.items():

			if args.arch not in ("all", arch):
				continue

			doBuild(
				args.mode, conanHome, binDir, conanArch, str(args.api), args.generator,
				generatorValidationLayer, args.simd, args.install, args.tests == "True", cache
			)

		common.saveHashCache(cache)

	# Make APK

	if args.apk:
		packageDirs = [ f"build/{args.mode}/android/packages" ] + (args.packages or [])
		makeApk(args, packageDirs)

	# Run

	if args.run:
		runApk(args)

if __name__ == "__main__":
	main()
