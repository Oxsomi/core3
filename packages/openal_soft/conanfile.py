from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename, rm
import os
import glob

required_conan_version = ">=2.0"

class openal_soft(ConanFile):

	name = "openal_soft"
	version = "2026.08.06"

	# Optional metadata
	license = "BSD-3 License"
	author = "kcat, creative labs, etc."
	url = "https://github.com/Oxsomi/openal-soft"
	description = "OpenAL Soft is a software implementation of the OpenAL 3D audio API."

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"
	options = { "enableASAN": [ True, False ], "enableUBSAN": [ True, False ] }
	default_options = { "enableASAN": False, "enableUBSAN": False }

	# A sanitized consumer can't link unsanitized dependencies, so the flags have to reach these too.
	# MSVC's STL records its ASan container annotation state per object and lld-link rejects the mix,
	# hence disabling the annotations rather than instrumenting the STL.

	def _sanitizerFlags(self):

		flags = []

		# Windows means an MSVC ABI driver (msvc or clang-cl), which spells these with a slash and rejects
		# the driver forms; a plain clang/gcc rejects the slash forms just as hard ("no such file or
		# directory: '/fsanitize=address'"), so the spelling has to follow the platform.
		# The _DISABLE_*_ANNOTATION defines are MSVC STL specific: it records its ASan container annotation
		# state per object and lld-link refuses to mix instrumented and uninstrumented ones.

		msvcStyle = self.settings.os == "Windows"

		if self.options.enableASAN:
			if msvcStyle:
				flags += [ "/fsanitize=address", "/Oy-", "-D_DISABLE_STRING_ANNOTATION=1", "-D_DISABLE_VECTOR_ANNOTATION=1" ]
			else:
				flags += [ "-fsanitize=address", "-fno-omit-frame-pointer" ]

		if self.options.enableUBSAN:
			flags += [ "-fsanitize=undefined", "-fno-sanitize=vptr" ]
			flags += [ "/Oy-" ] if msvcStyle else [ "-fno-omit-frame-pointer" ]

		return flags

	# The compile side flags alone aren't enough: CMake drives lld-link directly, so the /defaultlib
	# directives clang-cl embeds for the sanitizer runtimes never reach it.
	# Point the linker at clang's own runtime directory and name the libraries here too.

	def _sanitizerLinkFlags(self):

		if not (self.options.enableASAN or self.options.enableUBSAN):
			return []

		if self.settings.os != "Windows":
			return []

		import glob as _glob
		import shutil as _shutil

		executables = self.conf.get("tools.build:compiler_executables", default={}, check_type=dict)
		cc = executables.get("c") or "clang-cl"
		resolved = cc if os.path.isabs(cc) else (_shutil.which(cc) or "")

		if not resolved:
			return []

		binDir = os.path.dirname(resolved)
		found = _glob.glob(os.path.join(binDir, "..", "lib", "clang", "*", "lib", "windows"))

		if not found:
			return []

		# Forward slashes: this ends up inside a quoted string in conan_toolchain.cmake, where CMake reads
		# a backslash as an escape and dies on paths like C:\Program Files ("Invalid character escape '\P'").

		flags = [ "-libpath:%s" % os.path.normpath(found[0]).replace("\\", "/") ]

		# compiler-rt names these after the target arch, so it can't be pinned to x86_64 or an arm64 build
		# silently links the wrong runtime (or none).

		arch = "aarch64" if str(self.settings.arch) in ("armv8", "arm64", "aarch64") else "x86_64"

		if self.options.enableASAN:
			flags += [ "clang_rt.asan_dynamic-%s.lib" % arch, "clang_rt.asan_dynamic_runtime_thunk-%s.lib" % arch ]

		if self.options.enableUBSAN:
			flags += [ "clang_rt.ubsan_standalone-%s.lib" % arch ]

		return flags


	exports_sources = [ "include/*" ]

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def generate(self):
		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)
		tc.cppstd = "20"

		if self.settings.os == "Linux":
			tc.cache_variables["ALSOFT_BACKEND_ALSA"]      = True
			tc.cache_variables["ALSOFT_BACKEND_PIPEWIRE"]  = True
			tc.cache_variables["ALSOFT_BACKEND_OSS"]       = False
			tc.cache_variables["ALSOFT_BACKEND_WAVE"]      = False
		elif self.settings.os == "Windows":
			tc.cache_variables["ALSOFT_BACKEND_WASAPI"]    = True
			tc.cache_variables["ALSOFT_BACKEND_DSOUND"]    = True
			tc.cache_variables["ALSOFT_BACKEND_WINMM"]     = False
			tc.cache_variables["ALSOFT_BACKEND_WAVE"]      = False
		elif self.settings.os == "Macos":
			tc.cache_variables["ALSOFT_BACKEND_COREAUDIO"] = True
			tc.cache_variables["ALSOFT_BACKEND_WAVE"]      = False

		# On Windows the VS generator is already multi-config by default;
		# overriding CMAKE_CONFIGURATION_TYPES would collapse it to a single config.
		# On single-config generators (Ninja, Makefiles) we must pin the build type.
		if self.settings.os != "Windows":
			tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)

		for flag in self._sanitizerLinkFlags():
			tc.extra_exelinkflags.append(flag)
			tc.extra_sharedlinkflags.append(flag)

		for flag in self._sanitizerFlags():
			tc.extra_cflags.append(flag)
			tc.extra_cxxflags.append(flag)

		tc.generate()

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "openal-soft")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])
		git.run("submodule update --init --recursive")

	def build(self):
		cmake = CMake(self)
		cmake.configure(build_script_folder="openal-soft")
		cmake.build()

	def package(self):
		cmake = CMake(self)
		cmake.build(target="OpenAL")

		headers_src = os.path.join(self.source_folder, "openal-soft/include/AL")
		headers_dst = os.path.join(self.package_folder, "include/AL")
		copy(self, "*.h", headers_src, headers_dst)

		lib_dst = os.path.join(self.package_folder, "lib")
		bin_dst = os.path.join(self.package_folder, "bin")

		if self.settings.os != "Windows":

			if self.settings.os in ("Linux", "Android"):
				copy(self, "*.so",       self.build_folder, lib_dst)
				copy(self, "*.so",       self.build_folder, bin_dst)
				copy(self, "*.so.*",     self.build_folder, lib_dst)
				copy(self, "*.so.*",     self.build_folder, bin_dst)
				copy(self, "*.so.*.*.*", self.build_folder, lib_dst)
				copy(self, "*.so.*.*.*", self.build_folder, bin_dst)
			else:
				copy(self, "*.dylib", self.build_folder, lib_dst)
				copy(self, "*.dylib", self.build_folder, bin_dst)

			copy(self, "*.a", self.build_folder, lib_dst)

		else:
			# Copy per-config subdirs; only the config we built will be populated
			for config in ("Debug", "Release", "RelWithDebInfo", "MinSizeRel"):
				src = os.path.join(self.build_folder, config)
				if not os.path.isdir(src):
					continue
				copy(self, "*.lib", src, lib_dst)
				copy(self, "*.pdb", src, lib_dst)
				copy(self, "*.exp", src, lib_dst)
				copy(self, "*.dll", src, bin_dst)

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "openal_soft")
		self.cpp_info.set_property("cmake_target_name", "openal_soft::openal_soft")
		self.cpp_info.set_property("pkg_config_name", "openal_soft")

		# Only expose the final linked library; alsoft.common and alsoft.excommon
		# are internal static libs merged into it and must not be listed here.
		if self.settings.os == "Windows":
			self.cpp_info.libs = ["OpenAL32"]
		else:
			self.cpp_info.libs = ["openal"]