from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
import os

required_conan_version = ">=2.0"

# Shared sanitizer wiring for the dependencies OxC3 compiles from source (dxc, spirv_reflect, openal_soft).
#
# A sanitized consumer can't link unsanitized dependencies, so these flags have to reach them too, and the
# details are fiddly enough (platform spelling, which clang's runtime, MSVC STL annotations) that three
# copies drifted apart in practice. Consumed as a python_requires so there is exactly one copy:
#
#   python_requires = "oxc3_sanitizers/1.0"
#   def _sanitizerFlags(self):
#       return self.python_requires["oxc3_sanitizers"].module.sanitizerFlags(self, "vptr,enum")
#
# build_common.py exports this before creating any package that uses it, and folds this folder into the
# dependency hash so editing it rebuilds the packages that consume it.

class oxc3Sanitizers(ConanFile):
	name = "oxc3_sanitizers"
	version = "1.0"
	package_type = "python-require"


def sanitizerFlags(conanfile, disabledUBSanChecks, ubsanIgnorelist = None):
	"""Compile flags for a dependency built alongside a sanitized OxC3.

	disabledUBSanChecks is a comma separated -fno-sanitize list, since which checks are noise depends on the
	dependency's own idioms rather than on anything shared.

	ubsanIgnorelist is an optional absolute path to a -fsanitize-ignorelist file, for when the noise is one
	source file's rather than the whole dependency's; the check then keeps working everywhere else in it.
	"""

	flags = []

	# Second line of defence behind build.py's own check, for anyone invoking conan directly.
	# MSVC specifically, not everything that isn't clang: gcc sanitizes fine and is a real host for this on the
	# linux legs. MSVC is the one that ignores the UBSan half with a D9002 warning instead of failing, so a
	# "sanitized" MSVC build would quietly be no such thing, and its ASan wants a different runtime than these
	# flags assume.

	if conanfile.settings.compiler == "msvc" and (conanfile.options.enableASAN or conanfile.options.enableUBSAN):
		raise ConanInvalidConfiguration(
			"enableASAN/enableUBSAN require clang; MSVC's sanitizers are deliberately unused in OxC3"
		)

	# Windows means an MSVC ABI driver (msvc or clang-cl), which spells these with a slash and rejects the
	# driver forms; a plain clang/gcc rejects the slash forms just as hard ("no such file or directory:
	# '/fsanitize=address'"), so the spelling has to follow the platform.
	# The _DISABLE_*_ANNOTATION defines are MSVC STL specific: it records its ASan container annotation state
	# per object and lld-link refuses to mix instrumented and uninstrumented ones.

	msvcStyle = conanfile.settings.os == "Windows"

	if conanfile.options.enableASAN:
		if msvcStyle:
			flags += [ "/fsanitize=address", "/Oy-", "-D_DISABLE_STRING_ANNOTATION=1", "-D_DISABLE_VECTOR_ANNOTATION=1" ]
		else:
			flags += [ "-fsanitize=address", "-fno-omit-frame-pointer" ]

	# Windows caveat: MSVC's clang_rt.ubsan_standalone is not usable on its own there. It references
	# __coe_win::ContinueOnError/RawWrite/PrintStack, which live in MSVC's ASan runtime, so a UBSan-only build
	# fails to link with unresolved externals no matter which CRT is selected (verified: /MT and /MD fail
	# identically, /MT with ASan and UBSan together links fine). UBSan on Windows therefore requires ASan to be
	# on as well, which is how the sanitizer jobs run it.

	if conanfile.options.enableUBSAN:

		flags += [ "-fsanitize=undefined", "-fno-sanitize=%s" % disabledUBSanChecks ]
		flags += [ "/Oy-" ] if msvcStyle else [ "-fno-omit-frame-pointer" ]

		# clang-cl doesn't take the driver spelling of this one, so it goes through its /clang: escape hatch;
		# UBSan here is always clang, since MSVC's own sanitizers aren't used (see the note above).

		# Forward slashes even on Windows: the flag ends up inside a string in conan_toolchain.cmake, where a
		# path like C:\Users\... is a CMake escape error ("Invalid character escape '\U'").
		# clang takes either spelling.
		# clang only, since gcc has no equivalent flag.
		# No dependency passes an ignorelist today: openal_soft was the only one, and it is no longer sanitized
		# at all. Kept because it is the right shape for per-file noise in dxc or spirv_reflect later, but it is
		# untested from here on, so re-verify the flag actually reaches the compiler before relying on it.

		if ubsanIgnorelist and conanfile.settings.compiler == "clang":
			ignorelistFlag = "-fsanitize-ignorelist=%s" % ubsanIgnorelist.replace("\\", "/")
			flags += [ "/clang:%s" % ignorelistFlag ] if msvcStyle else [ ignorelistFlag ]

	return flags


def sanitizerLinkFlags(conanfile):
	"""Linker flags naming the sanitizer runtimes, Windows only.

	The compile side flags alone aren't enough there: CMake drives lld-link directly, so the /defaultlib
	directives clang-cl embeds for the sanitizer runtimes never reach it.
	"""

	if not (conanfile.options.enableASAN or conanfile.options.enableUBSAN):
		return []

	if conanfile.settings.os != "Windows":
		return []

	import glob as _glob
	import shutil as _shutil
	import subprocess as _subprocess

	# The sanitizer runtime has to come from the SAME clang that compiles these objects. The conan profiles
	# pin the Visual Studio generator, so MSBuild drives the LLVM bundled inside the VS install (the ClangCL
	# toolset) rather than whatever clang-cl sits on PATH. That distinction matters because the CI runners
	# also carry a standalone LLVM at a different major version: linking one version's clang_rt against
	# another's instrumentation yields a DLL whose exports do not match, which surfaces at process start as
	# 0xC0000139 STATUS_ENTRYPOINT_NOT_FOUND rather than as a link error.
	# PATH is only a fallback, for a toolchain that has no VS bundled LLVM at all.

	binDir = ""

	vswhere = os.path.join(
		os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
		"Microsoft Visual Studio", "Installer", "vswhere.exe"
	)

	if os.path.isfile(vswhere):

		try:
			vsRoot = _subprocess.run(
				[ vswhere, "-latest", "-products", "*", "-property", "installationPath" ],
				capture_output=True, text=True, timeout=30
			).stdout.strip()
		except Exception:
			vsRoot = ""

		# x64 is the only host that ships a usable sanitizer runtime today; the plain bin/ is the 32 bit host
		# and is checked purely as a fallback.

		for hostDir in ("x64", ""):

			candidate = os.path.join(vsRoot, "VC", "Tools", "Llvm", hostDir, "bin") if vsRoot else ""

			if candidate and os.path.isfile(os.path.join(candidate, "clang-cl.exe")):
				binDir = candidate
				break

	if not binDir:

		executables = conanfile.conf.get("tools.build:compiler_executables", default={}, check_type=dict)
		cc = executables.get("c") or "clang-cl"
		resolved = cc if os.path.isabs(cc) else (_shutil.which(cc) or "")

		if not resolved:
			return []

		binDir = os.path.dirname(resolved)

	found = _glob.glob(os.path.join(binDir, "..", "lib", "clang", "*", "lib", "windows"))

	if not found:
		return []

	# Forward slashes: this ends up inside a quoted string in conan_toolchain.cmake, where CMake reads a
	# backslash as an escape and dies on paths like C:\Program Files ("Invalid character escape '\P'").
	# The escaped quotes keep the space in "Program Files" from splitting the flag into two linker arguments;
	# an unescaped quote would instead terminate the CMake string and silently corrupt the path.

	flags = [ "-libpath:\\\"%s\\\"" % os.path.normpath(found[0]).replace("\\", "/") ]

	# compiler-rt names these after the target arch, so it can't be pinned to x86_64 or an arm64 build
	# silently links the wrong runtime (or none).

	arch = "aarch64" if str(conanfile.settings.arch) in ("armv8", "arm64", "aarch64") else "x86_64"

	if conanfile.options.enableASAN:
		flags += [ "clang_rt.asan_dynamic-%s.lib" % arch, "clang_rt.asan_dynamic_runtime_thunk-%s.lib" % arch ]

	if conanfile.options.enableUBSAN:
		flags += [ "clang_rt.ubsan_standalone-%s.lib" % arch ]

	return flags
