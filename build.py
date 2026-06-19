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

import argparse
import hashlib
import json
import os
import sys
import platform
import shlex
import shutil
import subprocess

HASH_CACHE_FILE = ".dep_hashes.json"

def _is_steamos() -> bool:

    # Inside a flatpak sandbox /etc/os-release reflects the container, not the
    # host. Use flatpak-spawn --host to read the real host OS file instead.
    if shutil.which("flatpak-spawn"):
        try:
            result = subprocess.run(
                ["flatpak-spawn", "--host", "cat", "/etc/os-release"],
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

def _ensure_correct_environment():
	"""On SteamOS, re-exec this script inside the steamrt4-sdk distrobox,
	escaping the flatpak sandbox first if VSCode itself is sandboxed.
	No-op on Windows / macOS / plain Linux."""

	if "--already-escaped" in sys.argv:
		sys.argv.remove("--already-escaped")
		return

	if platform.system() != "Linux" or not _is_steamos():
		return

	script    = os.path.abspath(__file__)
	forwarded = " ".join(shlex.quote(a) for a in sys.argv[1:])
	inner_cmd = (
		f"distrobox enter steamrt4-sdk -- python3 {shlex.quote(script)} "
		f"{forwarded} --already-escaped"
	)

	if shutil.which("flatpak-spawn"):
		cmd = ["flatpak-spawn", "--host", "/bin/bash", "-l", "-c", inner_cmd]
	else:
		cmd = ["/bin/bash", "-l", "-c", inner_cmd]

	sys.exit(subprocess.run(cmd).returncode)

def run(cmd, **kwargs):
	result = subprocess.run(cmd, shell=True, **kwargs)
	if result.returncode != 0:
		print(f"-- Command failed: {cmd}", file=sys.stderr)
		sys.exit(result.returncode)

def hash_package(package_path, profile_path, mode):
	h = hashlib.sha256()
	h.update(mode.encode())

	if os.path.isfile(profile_path):
		with open(profile_path, "rb") as f:
			h.update(f.read())

	for root, _, files in os.walk(package_path):
		for fname in sorted(files):
			fpath = os.path.join(root, fname)
			h.update(fpath.encode())
			with open(fpath, "rb") as f:
				h.update(f.read())

	return h.hexdigest()

def load_hash_cache():
	if os.path.isfile(HASH_CACHE_FILE):
		with open(HASH_CACHE_FILE, "r") as f:
			return json.load(f)
	return {}

def save_hash_cache(cache):
	with open(HASH_CACHE_FILE, "w") as f:
		json.dump(cache, f, indent=2)

def conan_create_if_changed(package_path, profile, mode, profile_args, cache):
	key          = f"{package_path}::{mode}"
	current_hash = hash_package(package_path, profile, mode)

	if cache.get(key) == current_hash:
		print(f"-- Skipping {package_path} ({mode}), unchanged")
		return

	run(f"conan create {package_path} {profile_args} -s build_type={mode} --build=missing")

	cache[key] = current_hash

def main():

	_ensure_correct_environment()

	parser = argparse.ArgumentParser(description="Build OxC3 for the host platform")

	parser.add_argument("-mode", type=str, default=None, choices=["Release", "Debug", "RelWithDebInfo", "MinSizeRel"], help="Build type (optional on Windows; defaults to all configs)")
	parser.add_argument("-simd",            type=str, default="True",  choices=["True", "False"], help="Enable SIMD")
	parser.add_argument("-tests",           type=str, default="False", choices=["True", "False"], help="Enable tests")
	parser.add_argument("-dynamic_linking", type=str, default="False", choices=["True", "False"], help="Dynamic linking graphics")

	parser.add_argument("--force_deps", action="store_true", help="Ignore hash cache and rebuild all dependencies")

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

	# On non-Windows, mode is required

	if system != "Windows" and args.mode is None:
		print("-- Error: -mode is required on non-Windows platforms", file=sys.stderr)
		sys.exit(1)

	# Select conan profile and platform name

	if system == "Windows":
		profile_base  = f"packages/conan/profiles/windows_msvc_{arch}"
		platform_name = "windows"
	elif system == "Darwin":
		profile_base  = f"packages/conan/profiles/osx_clang_{arch}"
		platform_name = "osx"
	else:
		profile_base  = f"packages/conan/profiles/linux_gcc_{arch}"
		platform_name = "linux"

	# On Windows the profile is per-mode (MSVC debug/release CRT differs),
	# on other platforms the same profile covers all modes.

	def profile_for_mode(mode):
		if system == "Windows":
			return f"{profile_base}_{mode}"
		return profile_base

	def profile_args_for_mode(mode):
		p = profile_for_mode(mode)
		return f"--profile:build={p} --profile:host={p}"

	# Decide which modes to build deps for

	all_modes = ["Release", "Debug", "RelWithDebInfo", "MinSizeRel"]

	if system == "Windows":
		dep_modes   = all_modes if args.mode is None else [args.mode]
		build_dir   = f"build/{platform_name}/{arch}"
		build_mode_arg = ""                                        # VS picks config at build time
	else:
		dep_modes   = [args.mode]
		build_dir   = f"build/{args.mode}/{platform_name}/{arch}"
		build_mode_arg = f"-s build_type={args.mode}"

	# Build dependencies (skipped when hash unchanged)
	
	if args.force_deps and os.path.isfile(HASH_CACHE_FILE):
		os.remove(HASH_CACHE_FILE)

	cache = load_hash_cache()

	for mode in dep_modes:
		pa = profile_args_for_mode(mode)

		if system == "Windows":
			conan_create_if_changed("packages/agility_sdk",    profile_for_mode(mode), mode, pa, cache)
			conan_create_if_changed("packages/amd_ags",        profile_for_mode(mode), mode, pa, cache)

		conan_create_if_changed("packages/nvapi",              profile_for_mode(mode), mode, pa, cache)
		conan_create_if_changed("packages/spirv_reflect",      profile_for_mode(mode), mode, pa, cache)
		conan_create_if_changed("packages/dxc",                profile_for_mode(mode), mode, pa, cache)
		conan_create_if_changed("packages/openal_soft",        profile_for_mode(mode), mode, pa, cache)

		if system == "Linux":
			conan_create_if_changed("packages/xdg_shell",      profile_for_mode(mode), mode, pa, cache)
			conan_create_if_changed("packages/xdg_decoration", profile_for_mode(mode), mode, pa, cache)

	save_hash_cache(cache)

	# Build project, use the mode that was requested, or Release as the
	# Conan install step for a multi-config VS project (VS itself builds all configs)

	extra            = " ".join(remainder)

	build_modes = all_modes if (system == "Windows" and args.mode is None) else [args.mode or "Release"]

	for mode in build_modes:
		pa = profile_args_for_mode(mode)
		mode_arg = f"-s build_type={mode}"
		run(
			f"conan build . "
			f"-of {build_dir} "
			f"{pa} "
			f"{mode_arg} "
			f"-o enableSIMD={args.simd} "
			f"-o enableTests={args.tests} "
			f"-o dynamicLinkingGraphics={args.dynamic_linking} "
			f"{extra}"
		)

	# Run tests

	if args.tests == "True":

		test_mode = args.mode if args.mode is not None else "Release"

		if system != "Windows":
			run(f"ctest --test-dir {build_dir}/build/{test_mode} -C {test_mode} --output-on-failure", cwd=os.getcwd())
		else:
			run(f"ctest --test-dir {build_dir}/build -C {test_mode} --output-on-failure", cwd=os.getcwd())

		# path = "tools/test.py"		# TODO: Re-enable once OxC3 works

		# if system == "Windows":
		# 	path = path.replace("/", "\\")

		# run(rf"python3 {path}", cwd=os.getcwd())

if __name__ == "__main__":
	main()