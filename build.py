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
# along with this program. If not, see https://github.com/Oxsomi/rt_core/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

import argparse
import os
import sys
import platform
import subprocess

def run(cmd, **kwargs):
	result = subprocess.run(cmd, shell=True, **kwargs)
	if result.returncode != 0:
		print(f"-- Command failed: {cmd}", file=sys.stderr)
		sys.exit(result.returncode)

def main():

	parser = argparse.ArgumentParser(description="Build OxC3 for the host platform")

	parser.add_argument("-mode", type=str, default="Release", choices=["Release", "Debug", "RelWithDebInfo", "MinSizeRel"], help="Build type")
	parser.add_argument("-simd",            type=str, default="True",  choices=["True", "False"], help="Enable SIMD")
	parser.add_argument("-tests",           type=str, default="False", choices=["True", "False"], help="Enable tests")
	parser.add_argument("-dynamic_linking", type=str, default="False", choices=["True", "False"], help="Dynamic linking graphics")

	args, remainder = parser.parse_known_args()

	system  = platform.system()   # Windows / Linux / Darwin
	machine = platform.machine()  # x86_64 / AMD64 / aarch64 / arm64

	# Normalise architecture

	if machine in ("AMD64", "x86_64"):
		arch       = "x64"
		conan_arch = "x86_64"
	elif machine in ("aarch64", "arm64", "ARM64"):
		arch       = "aarch64"
		conan_arch = "armv8"
	else:
		print(f"Unsupported architecture: {machine}", file=sys.stderr)
		sys.exit(1)

	# Select conan profile and platform name

	if system == "Windows":
		profile  = f"packages/conan/profiles/windows_msvc_{arch}_{args.mode}"
		platform_name = "windows"
	elif system == "Darwin":
		profile  = f"packages/conan/profiles/osx_clang_{arch}"
		platform_name = "osx"
	else:
		profile  = f"packages/conan/profiles/linux_gcc_{arch}"
		platform_name = "linux"

	profile_args = f"--profile:build={profile} --profile:host={profile}"
	mode_arg     = f"-s build_type={args.mode}"
	build_dir    = f"build/{args.mode}/{platform_name}/{arch}"

	# Build dependencies

	if system == "Windows":
		run(f"conan create packages/agility_sdk  {profile_args} {mode_arg} --build=missing")
		run(f"conan create packages/amd_ags      {profile_args} {mode_arg} --build=missing")

	run(f"conan create packages/nvapi        {profile_args} {mode_arg} --build=missing")
	run(f"conan create packages/spirv_reflect {profile_args} {mode_arg} --build=missing")
	run(f"conan create packages/dxc          {profile_args} {mode_arg} --build=missing")
	run(f"conan create packages/openal_soft  {profile_args} {mode_arg} --build=missing")

	if system == "Linux":
		run(f"conan create packages/xdg_shell      {profile_args} {mode_arg} --build=missing")
		run(f"conan create packages/xdg_decoration {profile_args} {mode_arg} --build=missing")

	# Build project

	extra = " ".join(remainder)

	run(
		f"conan build . "
		f"-of {build_dir} "
		f"{profile_args} "
		f"{mode_arg} "
		f"-o enableSIMD={args.simd} "
		f"-o enableTests={args.tests} "
		f"-o dynamicLinkingGraphics={args.dynamic_linking} "
		f"{extra}"
	)

	# Run tests

	if args.tests == "True":

		bin_dir = os.path.join(build_dir, "bin")
		ext     = ".exe" if system == "Windows" else ""

		tests = [
			"OxC3_types_base_test",
			"OxC3_types_math_test",
			"OxC3_types_container_test",
			"OxC3_formats_oiBC_test",
			"OxC3_formats_oiDL_test",
			"OxC3_formats_oiCA_test",
			"OxC3_formats_oiSB_test",
			"OxC3_formats_oiSH_test",
			"OxC3_formats_dds_test",
			"OxC3_formats_wav_test",
			"OxC3_formats_bmp_test",
			"OxC3_audio_interface_test",
			"OxC3_platforms_interface_test"
		]

		for test in tests:

			path = os.path.join(bin_dir, test + ext)

			if system == "Windows":
				path = path.replace("/", "\\");

			run(path)
		
		path = "tools/test.py"

		if system == "Windows":
			path = path.replace("/", "\\");

		run(rf"python {path}", cwd=os.getcwd())

if __name__ == "__main__":
	main()