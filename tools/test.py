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

# Functional test for the OxC3 CLI.
# It drives the built binary through (nearly) every command
# and asserts on the actual stdout/stderr, known hashes, round-trip contents, expected substrings.
# The harder surfaces are covered on purpose: virtual "//" files, -oiCA operations, encryption round-trips,
# and negative cases that must fail.
# Environment-dependent categories (graphics/audio/compile) are probed and skipped when the feature isn't in the build.
# A category can also be compiled in yet have no driver behind it (headless CI); those report SKIP at run time.
#
# Usage: python test.py <dir-containing-OxC3[.exe]>

from __future__ import annotations

import os
import sys
import platform
import subprocess
import shutil

# Known-answer values (stable substrings of the CLI output).
SHA256_HELLO = "64EC88CA00B268E5BA1A35678A1B5316D212F4F366B2477232534A8AECA37F3C"  # SHA256("Hello world")
CRC32C_HELLO = "72B51F78"                                                          # CRC32C("Hello world")
F32_ONE_HALF = "0x3fc00000"                                                        # IEEE-754 of 1.5
HEADLESS_GPU = "headless / no driver"                                              # the CLI's marker for a graphics build running without a driver/ICD
AES_KEY      = "CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE"

passed = 0
known_issues = 0
skipped = 0
failures: list[str] = []


def oxc3(bindir: str) -> str:
	return os.path.join(bindir, "OxC3" + (".exe" if platform.system() == "Windows" else ""))


def call(exe, args, stdin=None):
	r = subprocess.run([exe] + args, capture_output=True, text=True, input=stdin)
	return r.returncode, (r.stdout or "") + (r.stderr or "")


def gmac_tag(exe, args, stdin=None):
	"""Run 'file gmac ...' and return the hex tag (the token after 'Tag:'), or None if none was printed."""
	_, out = call(exe, args, stdin=stdin)
	for line in out.splitlines():
		if "Tag:" in line:
			return line.split("Tag:", 1)[1].strip()
	return None


def run(exe, args, contains=None, want_fail=False, xfail=None):
	"""Run 'exe args'; assert the exit code (want_fail flips it) and that every 'contains' substring is present.

	xfail: a reason string marks the command as KNOWN-broken. It never fails the suite; it reports XFAIL when it
	(still) fails and XPASS when it unexpectedly succeeds (a nudge to promote it back to a hard assertion)."""
	global passed, known_issues
	code, out = call(exe, args)

	ok = (code != 0) if want_fail else (code == 0)
	missing = [c for c in (contains or []) if c not in out] if ok else []
	ok = ok and not missing

	label = "OxC3 " + " ".join(args)

	if xfail is not None:
		known_issues += 1
		print(f"  {'XPASS' if ok else 'XFAIL'} {label}" +
			("  (known issue now passes - promote it!)" if ok else f"  (known issue: {xfail})"))
		return out

	if ok:
		passed += 1
		print(f"  PASS  {label}")
	else:
		failures.append(label)
		print(f"  FAIL  {label}")
		print(f"        exit={code} (want_fail={want_fail})" + (f", missing {missing}" if missing else ""))
		for line in out.splitlines()[:12]:
			print(f"        | {line}")
	return out


def check(cond, label):
	"""Assert a Python-side condition (e.g. a round-trip file comparison)."""
	global passed
	if cond:
		passed += 1
		print(f"  PASS  {label}")
	else:
		failures.append(label)
		print(f"  FAIL  {label}")


def skip(label, why):
	global skipped
	skipped += 1
	print(f"  SKIP  {label}  ({why})")


def write(path: str, text: str):
	with open(path, "w", newline="") as f:
		f.write(text)


def read_bytes(path: str) -> bytes:
	try:
		with open(path, "rb") as f:
			return f.read()
	except OSError:
		return b""


def section(name: str):
	print(f"\n== {name} ==")


def available(exe, category, needle) -> bool:
	"""True if 'category' lists a subcommand containing needle (i.e. the feature is compiled in)."""
	_, out = call(exe, [category])
	# An unknown/unavailable category falls back to the top-level "All categories:" help,
	# whose text happens to contain other categories' names (e.g. "devices").
	# Treat that fallback as not-available so e.g. graphics (whose DLLs may fail to load on a headless CI) is skipped
	# rather than false-matched.
	if "All categories" in out:
		return False
	return needle in out


def main():

	bindir = sys.argv[1] if len(sys.argv) > 1 else "."
	exe = oxc3(bindir)

	if not os.path.isfile(exe):
		print(f"OxC3 binary not found at {exe}", file=sys.stderr)
		sys.exit(1)

	# OxC3 refuses paths that escape its working directory, so all fixtures live UNDER the CWD as relative paths.
	work = "oxc3_test_tmp"
	shutil.rmtree(work, ignore_errors=True)
	os.makedirs(work)

	def p(name: str) -> str:
		return os.path.join(work, name)

	try:
		# ---- machine (log the CPU/RAM so CI runs record what hardware they ran on) -----------------
		section("machine")
		print(run(exe, ["devices", "cpu"], contains=["CPU:", "Cores"]))

		# ---- basics + rand ------------------------------------------------------------------------
		section("basics + rand")
		run(exe, [], want_fail=True)        # bare invocation prints usage, exits non-zero
		run(exe, ["rand", "key", "-count", "2"], contains=["Random operation returned"])
		run(exe, ["rand", "char", "-count", "3", "-length", "16", "--alphanumeric"], contains=["Random operation"])
		run(exe, ["rand", "num", "-length", "8", "-bits", "32"], contains=["Random operation"])
		run(exe, ["rand", "data", "-length", "64"], contains=["Random operation"])

		# ---- hash (known-answer, file==string) ----------------------------------------------------
		section("hash")
		run(exe, ["hash", "string", "-format", "SHA256", "-input", "Hello world"], contains=[SHA256_HELLO])
		run(exe, ["hash", "string", "-format", "CRC32C", "-input", "Hello world"], contains=[CRC32C_HELLO])
		run(exe, ["hash", "string", "-format", "MD5", "-input", "Hello world"], contains=["Hash:"])
		hello = p("hello.txt")
		write(hello, "Hello world")
		run(exe, ["hash", "file", "-format", "SHA256", "-input", hello], contains=[SHA256_HELLO])

		# ---- encryption round-trip (real assertions) ----------------------------------------------
		section("encryption round-trip")
		plain, enc, dec = p("plain.txt"), p("plain.enc"), p("plain.dec")
		write(plain, "Hello world\n")
		run(exe, ["file", "encr", "-input", plain, "-output", enc, "-aes", AES_KEY], contains=["Converted"])
		run(exe, ["file", "decr", "-input", enc, "-output", dec, "-aes", AES_KEY], contains=["Converted"])
		check(read_bytes(dec) == read_bytes(plain) and read_bytes(dec) != b"", "encr/decr round-trip content matches")
		run(exe, ["file", "header", "-input", enc], contains=["oiCA"])
		run(exe, ["file", "data", "-input", enc, "-aes", AES_KEY], contains=["plain.txt"])

		# ---- archive convert round-trips (oiCA / oiDL / combine) -----------------------------------
		section("archive convert")
		src = p("src"); os.makedirs(os.path.join(src, "sub"))
		write(os.path.join(src, "a.txt"), "alpha")
		write(os.path.join(src, "sub", "b.txt"), "beta-content")
		ca = p("src.oiCA")
		run(exe, ["file", "to", "-format", "oiCA", "-input", src, "-output", ca], contains=["Converted"])
		out_dir = p("srcout")
		run(exe, ["file", "from", "-format", "oiCA", "-input", ca, "-output", out_dir], contains=["Converted"])
		check(read_bytes(os.path.join(out_dir, "a.txt")) == b"alpha" and
			read_bytes(os.path.join(out_dir, "sub", "b.txt")) == b"beta-content", "oiCA pack/unpack round-trip")
		run(exe, ["file", "header", "-input", ca], contains=["oiCA"])
		run(exe, ["file", "data", "-input", ca], contains=["a.txt"])

		# oiDL from a folder: each entry holds a file's raw bytes.
		# Assert the exact per-entry byte lengths.
		dld = p("dld"); os.makedirs(dld)
		write(os.path.join(dld, "one.bin"), "AAA")            # 3 bytes
		write(os.path.join(dld, "two.bin"), "BBBBBB")         # 6 bytes
		oiDLfolder = p("dld.oiDL")
		run(exe, ["file", "to", "-format", "oiDL", "-input", dld, "-output", oiDLfolder], contains=["Converted"])
		out = run(exe, ["file", "data", "-input", oiDLfolder], contains=["length = 3", "length = 6"])
		check(out.count("length =") == 2, "oiDL folder -> exactly 2 data entries")

		# oiDL from text with --ascii: one entry per newline-separated line (no trailing newline -> no empty entry).
		lines = p("lines.txt")
		write(lines, "alpha\nbeta\ngamma")                    # alpha=5, beta=4, gamma=5
		oiDLtext = p("lines.oiDL")
		run(exe, ["file", "to", "-format", "oiDL", "-input", lines, "-output", oiDLtext, "--ascii"], contains=["Converted"])
		out = run(exe, ["file", "data", "-input", oiDLtext], contains=["length = 5", "length = 4"])
		check(out.count("length =") == 3, "oiDL --ascii -> exactly 3 line entries")

		# oiDL combine concatenates entries: 3 + 2 -> 5.
		lines2 = p("lines2.txt"); write(lines2, "delta\nepsilon")
		oiDL2 = p("lines2.oiDL")
		run(exe, ["file", "to", "-format", "oiDL", "-input", lines2, "-output", oiDL2, "--ascii"], contains=["Converted"])
		combined = p("combined.oiDL")
		run(exe, ["file", "combine", "-format", "oiDL", "-input", oiDLtext, "-input2", oiDL2, "-output", combined],
			contains=["Combined"])
		out = run(exe, ["file", "data", "-input", combined])
		check(out.count("length =") == 5, "oiDL combine (3 + 2) -> exactly 5 entries")

		# ---- aes key input: -aes / -aes-file / --aes-stdin -----------------------------------------
		# The GMAC tag depends only on (input, key), so the same key supplied three ways must produce the identical tag.
		# Exercises the shared CLI_getAesKey decoder (hex arg, hex file, raw file, stdin).
		section("aes key input (-aes / -aes-file / --aes-stdin)")

		keyhex = p("key.hex")
		write(keyhex, AES_KEY)                                          # 64-char hex, no trailing newline
		t_arg  = gmac_tag(exe, ["file", "gmac", "-input", hello, "-aes", AES_KEY])
		t_file = gmac_tag(exe, ["file", "gmac", "-input", hello, "-aes-file", keyhex])
		t_pipe = gmac_tag(exe, ["file", "gmac", "-input", hello, "--aes-stdin"], stdin=AES_KEY)
		check(t_arg is not None and t_arg == t_file, "-aes-file matches -aes (identical GMAC tag)")
		check(t_arg is not None and t_arg == t_pipe, "--aes-stdin matches -aes (identical GMAC tag)")

		# a raw 32-byte binary key file decodes to the same key as its hex form
		keyraw = p("key.raw")
		with open(keyraw, "wb") as f:
			f.write(bytes(range(32)))                                  # 00 01 .. 1f
		t_raw   = gmac_tag(exe, ["file", "gmac", "-input", hello, "-aes-file", keyraw])
		t_rawhx = gmac_tag(exe, ["file", "gmac", "-input", hello, "-aes", bytes(range(32)).hex()])
		check(t_raw is not None and t_raw == t_rawhx, "raw 32-byte key file matches its hex form")

		# only one key source may be given
		run(exe, ["file", "gmac", "-input", hello, "-aes", AES_KEY, "-aes-file", keyhex],
			want_fail=True, contains=["Only one of"])

		# -aes-file also enables encryption on a convert (convert tests AnyAES, not just the -aes bit)
		kfCA = p("kf.oiCA")
		run(exe, ["file", "to", "-format", "oiCA", "-input", src, "-output", kfCA, "-aes-file", keyhex],
			contains=["Converted"])
		run(exe, ["file", "list", "-oiCA", kfCA, "-aes-file", keyhex], contains=["a.txt"])
		check(call(exe, ["file", "list", "-oiCA", kfCA])[0] != 0, "archive made with -aes-file is encrypted (needs a key)")

		# encr/decr accept the key from a file too (round-trip); encr must refuse to run with no key at all
		encf, decf = p("plain.encf"), p("plain.decf")
		run(exe, ["file", "encr", "-input", plain, "-output", encf, "-aes-file", keyhex], contains=["Converted"])
		run(exe, ["file", "decr", "-input", encf, "-output", decf, "-aes-file", keyhex], contains=["Converted"])
		check(read_bytes(decf) == read_bytes(plain) and read_bytes(decf) != b"", "encr/decr round-trip via -aes-file")
		run(exe, ["file", "encr", "-input", plain, "-output", p("plain.nokey")], want_fail=True)

		# ---- file utilities -----------------------------------------------------------------------
		section("file utilities")
		run(exe, ["file", "stat", "-input", hello], contains=["Type:", "file"])
		run(exe, ["file", "count", "-input", "."], contains=["files"])
		run(exe, ["file", "hexdump", "-input", hello], contains=["48 65 6C 6C 6F"])        # "Hello" in hex
		run(exe, ["file", "gmac", "-input", hello, "-aes", AES_KEY], contains=["GMAC"])

		wiped = p("wipe.bin")
		write(wiped, "sensitive-data-here")
		run(exe, ["file", "wipe", "-input", wiped])
		wb = read_bytes(wiped)
		check(len(wb) > 0 and set(wb) == {0}, "file wipe zeroes contents")

		hello2 = p("hello2.txt")
		run(exe, ["file", "copy", "-input", hello, "-output", hello2])
		run(exe, ["file", "cmp", "-input", hello, "-input2", hello2], contains=["identical"])
		run(exe, ["file", "cmp", "-input", hello, "-input2", plain], contains=["differ"])

		newdir = p("newdir")
		run(exe, ["file", "mkdir", "-input", newdir])
		check(os.path.isdir(newdir), "file mkdir creates a directory")

		# file move takes a destination DIRECTORY (mv into dir), not a rename target
		run(exe, ["file", "move", "-input", hello2, "-output", newdir])
		check(os.path.isfile(os.path.join(newdir, "hello2.txt")) and not os.path.isfile(hello2),
			"file move relocates the file into the directory")

		touched = p("newdir/touched.txt")
		run(exe, ["file", "touch", "-input", touched])
		check(os.path.isfile(touched), "file touch creates a file")
		run(exe, ["file", "del", "-input", touched])
		check(not os.path.isfile(touched), "file del removes the file")

		# ---- file diff (oiCA + oiDL) --------------------------------------------------------------
		section("file diff")
		da, db = p("da"), p("db")
		os.makedirs(da); os.makedirs(db)
		write(os.path.join(da, "same.txt"), "identical")
		write(os.path.join(db, "same.txt"), "identical")
		write(os.path.join(da, "changed.txt"), "old")
		write(os.path.join(db, "changed.txt"), "new-and-longer")
		write(os.path.join(da, "only_a.txt"), "a")
		write(os.path.join(db, "only_b.txt"), "b")
		aCA, bCA = p("a.oiCA"), p("b.oiCA")
		run(exe, ["file", "to", "-format", "oiCA", "-input", da, "-output", aCA])
		run(exe, ["file", "to", "-format", "oiCA", "-input", db, "-output", bCA])
		run(exe, ["file", "diff", "-input", aCA, "-input2", bCA],
			contains=["+ only_b.txt", "- only_a.txt", "changed.txt", "1 added", "1 removed"])
		run(exe, ["file", "diff", "-input", aCA, "-input2", aCA], contains=["0 added", "0 removed", "0 modified"])
		aDL, bDL = p("a.oiDL"), p("b.oiDL")
		run(exe, ["file", "to", "-format", "oiDL", "-input", da, "-output", aDL])
		run(exe, ["file", "to", "-format", "oiDL", "-input", db, "-output", bDL])
		run(exe, ["file", "diff", "-input", aDL, "-input2", bDL], contains=["oiDL"])

		# ---- -oiCA operations (known archive -> EXACT assertions) ---------------------------------
		# We built this archive, so unlike an arbitrary folder we can assert exact counts, names and sizes.
		# Layout: f1.txt(3) f2.txt(6) sub/f3.txt(5)  ->  3 files, 1 folder.
		section("-oiCA operations")
		known = p("known")
		os.makedirs(os.path.join(known, "sub"))
		write(os.path.join(known, "f1.txt"), "one")           # 3 bytes
		write(os.path.join(known, "f2.txt"), "twotwo")        # 6 bytes
		write(os.path.join(known, "sub", "f3.txt"), "three")  # 5 bytes
		knownCA = p("known.oiCA")
		run(exe, ["file", "to", "-format", "oiCA", "-input", known, "-output", knownCA], contains=["Converted"])

		run(exe, ["file", "count", "-oiCA", knownCA], contains=["3 files, 1 folders"])
		run(exe, ["file", "list",  "-oiCA", knownCA], contains=["f1.txt", "f2.txt", "sub"])           # non-recursive
		run(exe, ["file", "tree",  "-oiCA", knownCA], contains=["f1.txt", "f2.txt", "sub/f3.txt"])    # recursive
		run(exe, ["file", "stat",  "-oiCA", knownCA, "-input", "f2.txt"], contains=["Size:      6 bytes", "file"])
		run(exe, ["file", "stat",  "-oiCA", knownCA, "-input", "sub/f3.txt"], contains=["Size:      5 bytes"])

		# encrypted archive: -aes decrypts on read; a mutate (del) re-encrypts, so the key is still required after.
		aesKey = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
		encCA = p("known.enc.oiCA")
		run(exe, ["file", "to", "-format", "oiCA", "-input", known, "-output", encCA, "-aes", aesKey], contains=["Converted"])
		run(exe, ["file", "list", "-oiCA", encCA, "-aes", aesKey], contains=["f1.txt", "f2.txt", "sub"])
		check(call(exe, ["file", "list", "-oiCA", encCA])[0] != 0, "encrypted -oiCA list fails without -aes")
		run(exe, ["file", "del", "-oiCA", encCA, "-input", "f1.txt", "-aes", aesKey], contains=["Updated archive"])
		run(exe, ["file", "count", "-oiCA", encCA, "-aes", aesKey], contains=["2 files, 1 folders"])
		check(call(exe, ["file", "count", "-oiCA", encCA])[0] != 0, "mutated archive is still encrypted (needs -aes)")

		# ---- file package -------------------------------------------------------------------------
		section("file package")
		run(exe, ["file", "package", "-input", src, "-output", p("src.pkg")], contains=["success"])

		# ---- float / time / info / devices --------------------------------------------------------
		section("float / time / info / devices")
		run(exe, ["float", "convert", "-input", "1.5"], contains=[F32_ONE_HALF])
		run(exe, ["float", "dissect", "-input", "1.5"], contains=[F32_ONE_HALF, "Normal"])
		run(exe, ["time", "now"], contains=["Unix epoch"])
		run(exe, ["time", "convert", "-input", "1785086967239392800"], contains=["ns", "->"])
		run(exe, ["info", "license"], contains=["Oxsomi"])
		run(exe, ["info", "about"], contains=["OxC3"])
		run(exe, ["devices", "cpu"], contains=["CPU:", "Cores"])
		# devices all dumps CPU + GPU + audio.
		# Live VRAM needs a created device (deferred prebuilt binary),
		# but the command degrades gracefully ("Memory in use: unavailable") and still succeeds.
		# Without a driver/ICD (headless CI) there is no GPU list either, just the CLI's headless marker,
		# so the GPU expectation only applies when that marker is absent.
		_, all_out = call(exe, ["devices", "all"])
		gpu_expect = [] if HEADLESS_GPU in all_out else ["graphics devices"]
		run(exe, ["devices", "all"], contains=["CPU:"] + gpu_expect)

		# ---- help (categories, operations-in-category, one operation, one format) -----------------
		section("help")
		run(exe, ["help", "categories"], contains=["file", "devices"])
		run(exe, ["help", "operations", "-input", "file"], contains=["file to", "file encr"])   # ops in a category
		run(exe, ["help", "operation", "-input", "file:encr"], contains=["file encr"])           # category:operation
		run(exe, ["help", "format", "-input", "file:to:oiCA"], contains=["oiCA"])                # category:operation:format

		# ---- profile (every op, with -length so it runs on a small buffer instead of the default 1 GiB) --
		section("profile")
		SMALL = "1048576"   # 1 MiB: fast, but still a real benchmark
		profs = [
			("cast", "casts"), ("rng", "Profile RNG"), ("crc32c", "Profile CRC32C"), ("fnv1a64", "Profile FNV1a64"),
			("sha256", "Profile SHA256"), ("md5", "Profile MD5"), ("aes256", "AES GCM"), ("aes128", "AES GCM"),
			("memcpy", "Profile memcpy"), ("memset", "Profile memset"), ("vec", "Profile vec4f"),
		]
		for name, kw in profs:
			run(exe, ["profile", name, "-length", SMALL], contains=[kw])
		run(exe, ["profile", "all", "-length", SMALL], contains=["Profile ", "casts", "AES GCM"])   # aggregates all

		# ---- compile shaders (probed) -------------------------------------------------------------
		section("compile shaders")
		if available(exe, "compile", "shaders"):
			shader = p("t.hlsl")
			write(shader,
				"[[oxc::uniforms(main)]]\ncbuffer G { float4 c; }\n"
				"[shader(\"pixel\")]\nfloat4 main() : SV_TARGET { return c; }\n")
			out = run(exe, ["compile", "shaders", "-format", "HLSL", "-input", shader, "-output", p("t.oiSH")],
				contains=["Compile success"])
		else:
			skip("compile shaders", "shader compiler not in this build")

		# ---- shader category (probed) -------------------------------------------------------------
		section("shader")
		if available(exe, "shader", "compile"):
			sh = p("s.hlsl")
			write(sh, "[shader(\"compute\")]\n[numthreads(1,1,1)]\nvoid main() {}\n")
			# compile (format detected from .hlsl); the compiler emits one oiSH per binary type
			run(exe, ["shader", "compile", "-input", sh, "-output", p("s.oiSH")], contains=["success"])
			soiSH = p("s.oiSH.spv.oiSH")
			run(exe, ["shader", "entrypoints", "-input", soiSH], contains=["entrypoint", "main"])
			run(exe, ["shader", "feature_set", "-input", soiSH], contains=["shader model"])
			run(exe, ["shader", "includes", "-input", soiSH], contains=["include"])
			# reflect: strips binaries -> reflection-only oiSH (round-trips back through entrypoints)
			run(exe, ["shader", "reflect", "-input", sh, "-output", p("sr.oiSH")], contains=["reflection-only"])
			run(exe, ["shader", "entrypoints", "-input", p("sr.oiSH.spv.oiSH")], contains=["main"])
			# disassemble + assemble round-trip on a standalone .spv extracted from the oiSH
			spv = p("s.spv")
			run(exe, ["file", "data", "-input", soiSH, "--bin", "-compile-output", "spv", "-entry", "0", "-output", spv])
			run(exe, ["shader", "disassemble", "-input", spv, "-output", p("s.spv.txt")])
			run(exe, ["shader", "assemble", "-input", p("s.spv.txt"), "-output", p("s2.spv")], contains=["Assembled"])
			run(exe, ["shader", "disassemble", "-input", p("s2.spv")], contains=["SPIR-V"])
			# same round-trip for DXIL (extracted from the dxil oiSH); assemble goes through DXC's IDxcAssembler
			doiSH = p("s.oiSH.dxil.oiSH")
			dxil = p("s.dxil")
			run(exe, ["file", "data", "-input", doiSH, "--bin", "-compile-output", "dxil", "-entry", "0", "-output", dxil])
			run(exe, ["shader", "disassemble", "-input", dxil, "-output", p("s.dxil.txt")])
			run(exe, ["shader", "assemble", "-input", p("s.dxil.txt"), "-output", p("s2.dxil")], contains=["Assembled"])
			run(exe, ["shader", "disassemble", "-input", p("s2.dxil")], contains=["target triple"])
		else:
			skip("shader", "shader compiler not in this build")

		# ---- graphics (probed) --------------------------------------------------------------------
		section("graphics")
		if available(exe, "graphics", "devices"):
			# The listing also succeeds on a machine whose compiled-in graphics stack has no driver/ICD behind
			# it (headless CI); it prints the headless marker per API then and names no device, which is a skip.
			_, gpu_out = call(exe, ["graphics", "devices"])
			if HEADLESS_GPU in gpu_out and "device" not in gpu_out:
				skip("graphics devices", "no driver (headless CI)")
			else:
				run(exe, ["graphics", "devices"], contains=["device"])
			# create passes on a real GPU (creates + destroys each device) and on headless CI (no interface/driver ->
			# nothing to create, still success); a genuine device-create failure on a machine that HAS a GPU still fails.
			run(exe, ["graphics", "create"])
		else:
			skip("graphics", "graphics not in this build")

		# ---- audio (probed) -----------------------------------------------------------------------
		section("audio")
		if available(exe, "audio", "devices"):
			run(exe, ["audio", "devices"], contains=["Device"])
			# audio convert needs a 32-bit-float WAV source (WAV_write rejects int PCM); synthesizing a valid
			# float WAV portably is out of scope for this smoke test, so it's intentionally not exercised here.
			skip("audio convert", "needs a float-format WAV source to exercise meaningfully")
		else:
			skip("audio", "audio not in this build")

		# ---- virtual "//" files (probed) ----------------------------------------------------------
		section("virtual files")
		vcode, vout = call(exe, ["file", "tree", "-input", "//OxC3_graphics"])
		if vcode == 0 and "oiSH" in vout:
			run(exe, ["file", "tree", "-input", "//OxC3_graphics"], contains=["image_copy.oiSH"])
			run(exe, ["file", "tree", "-input", "//"], contains=["oxc3_graphics"])
			run(exe, ["file", "tree", "-input", "//."], contains=["oxc3_graphics"])
			run(exe, ["file", "count", "-input", "//"], contains=["files"])
			run(exe, ["file", "stat", "-input", "//OxC3_graphics/shaders/image_copy.oiSH"], contains=["Size:", "file"])
			run(exe, ["file", "header", "-input", "//OxC3_graphics/shaders/mip_map.oiSH"], contains=["oiSH"])
		else:
			skip("virtual files", "no embedded //OxC3_graphics in this build")

		# ---- negative cases (must fail) -----------------------------------------------------------
		section("negative cases")
		run(exe, ["file", "stat", "-input", p("does_not_exist.xyz")], want_fail=True)
		run(exe, ["file", "diff", "-input", aCA, "-input2", aDL], want_fail=True)     # oiCA vs oiDL
		run(exe, ["file", "cmp", "-input", hello], want_fail=True)                    # missing -input2
		run(exe, ["hash", "string", "-format", "NOPE", "-input", "x"], want_fail=True)  # bad format

	finally:
		shutil.rmtree(work, ignore_errors=True)

	# ---- summary ----------------------------------------------------------------------------------
	print(f"\n== summary ==\n  {passed} passed, {len(failures)} failed, {known_issues} known-issue(s), {skipped} skipped")
	if failures:
		for f in failures:
			print(f"  FAILED: {f}")
		sys.exit(1)

	print("All OxC3 CLI tests passed.")


if __name__ == "__main__":
	main()
