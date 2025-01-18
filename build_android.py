# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
# along with this program. If not, see https://github.com/Oxsomi/rt_core/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

import argparse
import os
import sys
import re
import subprocess
import shutil
import glob
from pathlib import Path

def makeProfile(conanHome, llvmRootDir, arch, level, generator):

	# Get clang version

	if arch == "x86_64":
		llvmRootDir += "/x86_64"
	else:
		llvmRootDir += "/aarch64"

	llvmRootDir += "-linux-android" + level + "-clang"
	
	clangVersionStr = subprocess.check_output("\"" + llvmRootDir + "\" --version", shell=True, stderr=subprocess.DEVNULL).decode("utf-8").strip()
	regexFind = re.search("clang version ([0-9]+).[0-9]+.[0-9]+ ", clangVersionStr)
	clangVersion = regexFind.group(1)

	# Make profile

	outputPath = conanHome + "/profiles/android_" + arch + "_" + level + "_" + generator
	inputPath = os.path.dirname(os.path.realpath(__file__)) + "/src/platforms/android/android_profile"
	profile = Path(inputPath).read_text()

	profile = profile.format(level=level, arch=arch, clangVersion=clangVersion, androidNdk=os.environ["ANDROID_NDK"], generator=generator)

	with open(outputPath, "w") as output:
		output.write(profile)

def doBuild(mode, conanHome, llvmRootDir, arch, level, generator, simd, doInstall, enableShaderCompiler):

	profile = "android_" + arch + "_" + level + "_" + generator

	if arch == "x86_64":
		archName = "x64"
	else:
		archName = "arm64"

	if not os.path.isfile(conanHome + "/profiles/" + profile):
		makeProfile(conanHome, llvmRootDir, arch, level, generator)

	# Build dependencies

	subprocess.check_output("conan create packages/openal_soft -s build_type=" + mode + " --profile \"" + profile + "\" --build=missing")

	if enableShaderCompiler:
		subprocess.check_output("conan create packages/nvapi -s build_type=" + mode + " --profile \"" + profile + "\" --build=missing")
		subprocess.check_output("conan create packages/spirv_reflect -s build_type=" + mode + " --profile \"" + profile + "\" --build=missing")
		subprocess.check_output("conan create packages/dxc -s build_type=" + mode + " --profile \"" + profile + "\" --build=missing")

	outputFolder = "\"build/" + mode + "/android/" + archName + "\""
	subprocess.check_output("conan build . -of " + outputFolder + " -o cliGraphics=False -o enableOxC3CLI=False -o forceFloatFallback=False -o enableTests=False -o dynamicLinkingGraphics=False -o enableShaderCompiler=False -s build_type=" + mode + " -o enableSIMD=" + str(simd) + " --profile \"" + profile + "\" --build=missing")

	if doInstall:
		subprocess.check_output("conan export-pkg . -of " + outputFolder + " -o cliGraphics=False -o enableOxC3CLI=False -o forceFloatFallback=False -o enableTests=False -o dynamicLinkingGraphics=False -o enableShaderCompiler=False -s build_type=" + mode + " -o enableSIMD=" + str(simd) + " --profile \"" + profile + "\"")

def main():

	# Check environment

	if not "VULKAN_SDK" in os.environ:
		print("Vulkan SDK not found", file=sys.stderr)
		return

	if not "ANDROID_SDK" in os.environ:
		print("Android SDK not found", file=sys.stderr)
		return

	if not "ANDROID_NDK" in os.environ:
		print("Android NDK not found", file=sys.stderr)
		return

	# Build command line args

	parser = argparse.ArgumentParser(description="Build a lib or apk via OxC3")

	parser.add_argument("-mode", type=str, default="Release", choices=["Release", "Debug", "RelWithDebInfo", "MinSizeRel"], help="Build mode")
	parser.add_argument("-api", type=int, default=33, help="Android api level (e.g. 33 = Android 13)")
	parser.add_argument("-arch", type=str, default="all", choices=["arm64", "x64", "all"], help="Architecture")
	parser.add_argument("-simd", type=bool, default=False, help="EnableSIMD (False by default until properly supported)")
	parser.add_argument("-generator", type=str, help="CMake Generator")
	parser.add_argument("--skip_build", help="Run full build, if false, can be used to package an already built project", action="store_true")
	
	parser.add_argument("-package", type=str, help="APK package name (required if -apk)")
	parser.add_argument("-version", type=str, help="APK version (required if -apk)")
	parser.add_argument("-lib", type=str, help="Final name of lib built for the APK (required if -apk)")
	parser.add_argument("-name", type=str, help="Display name of the app itself (required if -apk)")
	parser.add_argument("-category", type=str, default="game", help="APK category", choices=[ "accessibility", "audio", "game", "image", "maps", "news", "productivity", "social", "video"])

	parser.add_argument("-keystore", type=str, help="Sign an apk with a certain keystore, if empty, will require JAVA_HOME to be set (will generate)", default=None)
	parser.add_argument("-keystore_password", type=str, help="Keystore password, if not entered, will ask it when signing", default=None)

	parser.add_argument("--shader_compiler", help="EnableShaderCompiler (False by default to reduce build times)", action="store_true")
	parser.add_argument("--sign", help="Sign apk if built", action="store_true")
	parser.add_argument("--run", help="Run apk if built", action="store_true")
	parser.add_argument("--apk", help="Build apk (requires -package, -version, -lib and -name)", action="store_true")
	
	parser.add_argument("--install", help="Install package rather than build", action="store_true")

	args = parser.parse_args()

	# Grab generator; on Windows, this is likely MinGW Makefiles while otherwise it's probably Unix Makefiles

	if args.generator == None:
		args.generator = "MinGW Makefiles" if os.name == "nt" else "Unix Makefiles"

	# Grab conan home to prepare the profiles for build

	conanHome = subprocess.check_output("conan config home", shell=True, stderr=subprocess.DEVNULL).decode("utf-8").strip()

	rootDir = os.environ["ANDROID_NDK"] + "/toolchains/llvm/prebuilt"

	for folder, subfolders, files in os.walk(rootDir):

		if folder == rootDir:
			continue

		rootDir = folder + "/bin"
		break

	# Build

	if not args.skip_build:
			
		if args.arch == "all" or args.arch == "x64":
			doBuild(args.mode, conanHome, rootDir, "x86_64", str(args.api), args.generator, args.simd, args.install, args.shader_compiler)

		if args.arch == "all" or args.arch == "arm64":
			doBuild(args.mode, conanHome, rootDir, "armv8", str(args.api), args.generator, args.simd, args.install, args.shader_compiler)

	# Make APK

	if args.apk:

		if args.package == None or args.version == None or args.lib == None or args.name == None:
			print("APK requires the following arguments (example): -package net.osomi.test -version 0.1.0 -lib test -name \"Test app\"", file=sys.stderr)
			return

			# Make manifest
					
		inputPath = os.path.dirname(os.path.realpath(__file__)) + "/src/platforms/android/AndroidManifest.xml"
		manifest = Path(inputPath).read_text()

		manifest = manifest.format(APP_PACKAGE=args.package, APP_VERSION=args.version, APP_API_LEVEL=str(args.api), APP_NAME=args.name, APP_DEBUGGABLE="false" if args.mode == "Release" else "true", APP_CATEGORY=args.category, APP_LIB_NAME=args.lib)

		outputPath = "build/" + args.mode + "/android/"
		Path(outputPath).mkdir(parents=True, exist_ok=True)
			
		outputPath += "apk/"
		shutil.rmtree(outputPath, ignore_errors=True)
		Path(outputPath).mkdir(parents=True, exist_ok=True)
		
		outputPath += "/AndroidManifest.xml"

		with open(outputPath, "w") as output:
			output.write(manifest)

		# Copy so file(s)

		outputPath = "build/" + args.mode + "/android/apk/lib"
		Path(outputPath).mkdir(parents=True, exist_ok=True)

		if args.arch == "arm64" or args.arch == "all":

			tmpPath = outputPath + "/arm64-v8a"
			Path(tmpPath).mkdir(parents=True, exist_ok=True)

			for f in glob.glob("build/" + args.mode + "/android/arm64/lib/*.so"):
				shutil.copy2(f, outputPath + "/arm64-v8a")

		if args.arch == "x64" or args.arch == "all":

			tmpPath = outputPath + "/x86_64"
			Path(tmpPath).mkdir(parents=True, exist_ok=True)

			for f in glob.glob("build/" + args.mode + "/android/x64/lib/*.so"):
				shutil.copy2(f, outputPath + "/x86_64")

		# Copy packages
		
		assetsFolder = "build/" + args.mode + "/android/apk/assets"
		outputPath = assetsFolder
		Path(outputPath).mkdir(parents=True, exist_ok=True)

		outputPath += "/packages"
		Path(outputPath).mkdir(parents=True, exist_ok=True)
		
		for f in glob.glob("build/android/packages/*"):

			# We need to make a file named section_ here, because AAssetManager can't iterate directories

			sectionName = Path(f).name
			open(outputPath + "/section_" + sectionName, 'a').close()

			# Copy folder

			Path(outputPath + "/" + sectionName).mkdir(parents=True, exist_ok=True)

			for fc in glob.glob("build/android/packages/" + sectionName + "/*"):
				shutil.copy2(fc, outputPath + "/" + sectionName + "/" + Path(fc).name)

		# Copy into /res (and provide -S res to aapt package)
		
		inputPath = os.path.dirname(os.path.realpath(__file__)) + "/src/platforms/android/res"
		resFolder = "build/" + args.mode + "/android/apk/res"
		Path(resFolder).mkdir(parents=True, exist_ok=True)
		shutil.copytree(inputPath, resFolder, dirs_exist_ok=True)

		# Find directory with aapt, zipalign, apksigner, etc.

		for f in glob.glob(os.environ["ANDROID_SDK"] + "/build-tools/*"):
			buildTools = f
			break

		buildTools += "/"

		# Make unsigned apk
		
		print("-- Creating apk")
		subprocess.check_output("\"" + buildTools + "aapt\" package -f -I \"" + os.environ["ANDROID_SDK"] + "/platforms/android-" + str(args.api) + "/android.jar\" -M \"build/" + args.mode + "/android/apk/AndroidManifest.xml\" -A \"" + assetsFolder + "\" -S \"" + resFolder + "\" -m -F \"build/" + args.mode + "/android/apk/app-unsigned.apk\"")
		
		print("-- Adding libs to apk")

		# Gotta temporary move to android/apk to avoid names from getting messed up by aapt
			
		cwd = os.getcwd()
		os.chdir("build/" + args.mode + "/android/apk")
		
		for f in glob.glob("lib/*"):
			for f0 in glob.glob(f + "/*.so"):
				subprocess.check_output("\"" + buildTools + "aapt\" add \"app-unsigned.apk\" \"" + f0.replace("\\", "/") + "\"")

		os.chdir(cwd)

		print("-- Zipalign apk")
		subprocess.check_output("\"" + buildTools + "zipalign\" -v -f 4 \"build/" + args.mode + "/android/apk/app-unsigned.apk\" \"build/" + args.mode + "/android/apk/app.apk\"")

		apkFile = "\"build/" + args.mode + "/android/apk/app.apk\""

		if args.sign:

			if args.keystore == None:

				args.keystore = "build/" + args.mode + "/android/apk/.keystore"

				if not os.path.isfile(args.keystore):

					print("-- Creating keystore")

					if not "JAVA_HOME" in os.environ:
						print("JAVA_HOME not found", file=sys.stderr)
						return

					subprocess.check_output("\"" + os.environ["JAVA_HOME"] + "/bin/keytool\" -genkeypair -v -keystore \"" + args.keystore + "\" -keyalg RSA -keysize 2048 -validity 10000")

			if args.keystore_password == None:
				print("-- Sign apk, please enter keystore password")
				pwd = str(input())

			else:
				pwd = args.keystore_password

			signCommand = "\"" + buildTools + "apksigner.bat\"" if os.name == "nt" else "bash \"" + buildTools + "apksigner.sh\""
			subprocess.check_output(signCommand + " sign --ks \"" + args.keystore + "\" --ks-pass \"pass:" + pwd + "\" --key-pass \"pass:" + pwd + "\" --min-sdk-version " + str(args.api) + " \"build/" + args.mode + "/android/apk/app.apk\"")
			
			print("-- Successfully signed apk")

	# Run

	if args.run:

		apkFile = "\"build/" + args.mode + "/android/apk/app.apk\""

		if args.package == None or args.lib == None:
			print("apk run requires the following arguments (example): -package net.osomi.test -lib test", file=sys.stderr)
			return

		if not args.sign and args.apk:
			print("apk run requires apk to be signed", file=sys.stderr)
			return

		adb = os.environ["ANDROID_SDK"] + "/platform-tools/adb"

		print("-- Installing apk file (" + apkFile + ") using adb (" + adb + ")")

		if args.mode == "Release":
			subprocess.check_output(adb + " install -r " + apkFile)
		else:
			subprocess.check_output(adb + " install -t -r " + apkFile)

		if args.mode == "Release":
			print("-- Installed apk file, but Release mode has android:exported turned off for security reasons, please manually launch the app")
			return
		
		print("-- Running apk file")
		subprocess.check_output(adb + " logcat -c")		# Clear log first
		subprocess.check_output(adb + " shell am start -n " + args.package + "/android.app.NativeActivity")
		
		print("-- Starting logcat")
		print("-- Couldn't start logcat, please manually run: " + adb + " logcat -s OxC3")

if __name__ == "__main__":
	main()