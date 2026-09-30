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

# A pinned Mesa, built from source into a prefix of its own, so the Vulkan drivers a test runs against (RADV,
# lavapipe) are a version chosen here rather than whatever the distribution ships. Linux only: that is where
# Mesa's Vulkan drivers run.
#
#   python tools/mesa.py build 26.2.3                          upstream tag mesa-26.2.3
#   python tools/mesa.py build <ref> -repo <url>               any repository and tag, branch or commit (a fork)
#   python tools/mesa.py build 26.2.3 -D spirv2isa=enabled     extra meson options, repeatable
#   python tools/mesa.py env 26.2.3                            prints VK_ICD_FILENAMES for eval
#   python tools/mesa.py run 26.2.3 -- ./some_test             runs a command against it
#   python tools/mesa.py list                                  what is built
#
# build.py -mesa <ref> runs the suites against one; see the README.
#
# Each build lands in <root>/<id>, with a mesa.json saying the repository, the ref, the commit it resolved to and the
# meson options, so a result can always be traced to the exact driver that produced it. The root defaults to
# ~/.cache/oxc3/mesa, or OXC3_MESA_ROOT.
#
# The drivers are linked with a runpath to the prefix's own libraries: Mesa bundles a libdrm when the system's is
# older than it needs, and without the runpath the loader takes the system one and RADV fails its first query.

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys

DEFAULT_REPO = "https://gitlab.freedesktop.org/mesa/mesa.git"

# RADV and lavapipe, and nothing of OpenGL. lavapipe is built on llvmpipe, so that gallium driver comes with it.

DEFAULT_OPTIONS = [
	"buildtype=release", "vulkan-drivers=amd,swrast", "gallium-drivers=llvmpipe", "platforms=x11,wayland",
	"llvm=enabled", "opengl=false", "gles1=disabled", "gles2=disabled", "glx=disabled", "egl=disabled",
	"gbm=disabled", "video-codecs=", "build-tests=false", "libdir=lib"
]

def root():
	return os.environ.get("OXC3_MESA_ROOT", os.path.join(os.path.expanduser("~"), ".cache", "oxc3", "mesa"))

# An upstream version names its tag; anything else (a branch, a commit, a fork's tag) is taken as given.

def resolveRef(ref, repo):
	return f"mesa-{ref}" if repo == DEFAULT_REPO and re.fullmatch(r"\d+\.\d+\.\d+(-rc\d+)?", ref) else ref

# The folder a build lives in: the ref, prefixed by the repository when it isn't upstream, so a fork's 26.2 and
# upstream's never share a prefix.

def buildId(ref, repo = DEFAULT_REPO):

	name = re.sub(r"[^A-Za-z0-9._-]+", "_", ref)

	if repo == DEFAULT_REPO:
		return name

	origin = re.sub(r"[^A-Za-z0-9._-]+", "_", re.sub(r"^\w+://", "", repo).removesuffix(".git"))
	return f"{origin}__{name}"

def prefixOf(ref, repo = DEFAULT_REPO):
	return os.path.join(root(), buildId(ref, repo))

def icdFiles(ref, repo = DEFAULT_REPO):
	return sorted(glob.glob(os.path.join(prefixOf(ref, repo), "share", "vulkan", "icd.d", "*.json")))

def environment(ref, repo = DEFAULT_REPO):
	"""The variables that make the Vulkan loader use this build's drivers and no others. Raises when it isn't built."""

	icds = icdFiles(ref, repo)

	if not icds:
		raise RuntimeError(
			f"Mesa {ref} is not built under {prefixOf(ref, repo)}; run python tools/mesa.py build {ref}"
			+ ("" if repo == DEFAULT_REPO else f" -repo {repo}")
		)

	return { "VK_ICD_FILENAMES": ":".join(icds) }

def run(cmd, cwd = None):
	print("-- " + " ".join(cmd), flush=True)
	subprocess.run(cmd, cwd=cwd, check=True)

# LLVM for lavapipe: the unversioned llvm-config when there is one, else the newest versioned one, since
# distributions ship only llvm-config-N.

def llvmConfig():

	if shutil.which("llvm-config"):
		return shutil.which("llvm-config")

	found = []

	for path in os.environ.get("PATH", "").split(os.pathsep):
		for exe in glob.glob(os.path.join(path, "llvm-config-*")):
			match = re.search(r"llvm-config-(\d+)$", exe)
			if match:
				found.append((int(match.group(1)), exe))

	if not found:
		raise RuntimeError("No llvm-config on PATH; lavapipe needs LLVM (e.g. apt install llvm-19-dev)")

	return max(found)[1]

def checkout(repo, ref, src):
	"""A shallow checkout of ref: a tag or branch clones directly, a commit is fetched by its hash."""

	if os.path.isdir(os.path.join(src, ".git")):
		return

	if re.fullmatch(r"[0-9a-f]{7,40}", ref):
		os.makedirs(src, exist_ok=True)
		run(["git", "init", "-q"], cwd=src)
		run(["git", "remote", "add", "origin", repo], cwd=src)
		run(["git", "fetch", "--depth", "1", "origin", ref], cwd=src)
		run(["git", "checkout", "-q", "FETCH_HEAD"], cwd=src)
	else:
		run(["git", "clone", "--depth", "1", "--branch", ref, repo, src])

def build(ref, repo, extraOptions):

	tag    = resolveRef(ref, repo)
	prefix = prefixOf(ref, repo)
	work   = os.path.join(root(), "work", buildId(ref, repo))
	src    = os.path.join(work, "src")
	bld    = os.path.join(work, "build")

	checkout(repo, tag, src)

	commit = subprocess.run(
		["git", "rev-parse", "HEAD"], cwd=src, check=True, capture_output=True, text=True
	).stdout.strip()

	native = os.path.join(work, "native.ini")

	with open(native, "w") as f:
		f.write(f"[binaries]\nllvm-config = '{llvmConfig()}'\n")

	libdir  = os.path.join(prefix, "lib")
	rpath   = [ f"c_link_args=-Wl,-rpath,{libdir}", f"cpp_link_args=-Wl,-rpath,{libdir}" ]
	options = DEFAULT_OPTIONS + rpath + extraOptions

	# The bundled libdrm is used only where the option exists (Mesa 25 and on) and only when the system's is too old.

	optionsFile = os.path.join(src, "meson.options")

	if not os.path.isfile(optionsFile):
		optionsFile = os.path.join(src, "meson_options.txt")

	with open(optionsFile) as f:
		if "allow-fallback-for" in f.read():
			options.append("allow-fallback-for=libdrm")

	if not os.path.isdir(bld):
		run(["meson", "setup", bld, src, "--native-file", native, f"--prefix={prefix}"] + [ f"-D{o}" for o in options ])
	else:
		run(["meson", "configure", bld] + [ f"-D{o}" for o in options ])

	run(["ninja", "-C", bld, "install"])

	with open(os.path.join(prefix, "mesa.json"), "w") as f:
		json.dump({ "repo": repo, "ref": ref, "tag": tag, "commit": commit, "options": options }, f, indent=1)

	print(f"-- Mesa {ref} ({commit[:12]}) in {prefix}")
	for icd in icdFiles(ref, repo):
		print(f"   {os.path.basename(icd)}")

def main():

	parser = argparse.ArgumentParser(description="Build and use a pinned Mesa for OxC3's tests (Linux)")
	sub = parser.add_subparsers(dest="command", required=True)

	for name in ("build", "env", "run"):
		p = sub.add_parser(name)
		p.add_argument("ref", help="an upstream version (26.2.3) or any tag, branch or commit of -repo")
		p.add_argument("-repo", default=DEFAULT_REPO, help="the repository, for a fork")
		if name == "build":
			p.add_argument("-D", dest="options", action="append", default=[], help="an extra meson option, key=value")
		if name == "run":
			p.add_argument("cmd", nargs=argparse.REMAINDER, help="-- then the command to run")

	sub.add_parser("list")
	args = parser.parse_args()

	if not sys.platform.startswith("linux"):
		print("tools/mesa.py builds Mesa's Vulkan drivers, which run on Linux only")
		return 1

	if args.command == "list":
		for meta in sorted(glob.glob(os.path.join(root(), "*", "mesa.json"))):
			with open(meta) as f:
				m = json.load(f)
			print(f"{os.path.basename(os.path.dirname(meta))}: {m['repo']} {m['ref']} ({m['commit'][:12]})")
		return 0

	if args.command == "build":
		build(args.ref, args.repo, args.options)
		return 0

	try:
		env = environment(args.ref, args.repo)
	except RuntimeError as e:
		print(e, file=sys.stderr)
		return 1

	if args.command == "env":
		for k, v in env.items():
			print(f"export {k}='{v}'")
		return 0

	cmd = args.cmd[1:] if args.cmd[:1] == ["--"] else args.cmd
	return subprocess.run(cmd, env={ **os.environ, **env }).returncode

if __name__ == "__main__":
	sys.exit(main())
