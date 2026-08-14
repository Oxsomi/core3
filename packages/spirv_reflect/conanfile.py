from conan import ConanFile
from conan.tools.files import copy
from conan.tools.scm import Git
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
import os

required_conan_version = ">=2.0"

class spirv_reflect(ConanFile):

	name = "spirv_reflect"
	version = "2026.08.06"

	license = "Apache License 2.0"
	author = "KhronosGroup"
	url = "https://github.com/Oxsomi/SPIRV-Reflect"
	description = "SPIRV-Reflect is a lightweight library that provides a C/C++ reflection API for SPIR-V shader bytecode in Vulkan applications."
	topics = ("khronos", "gpu", "spirv")

	exports_sources = [ "*.h", "include/*", "LICENSE" ]

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
		tc.variables["SPIRV_REFLECT_EXECUTABLE"] = False
		tc.variables["SPIRV_REFLECT_STATIC_LIB"] = True
		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)
		tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded"
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
		git.folder = os.path.join(self.source_folder, "SPIRV-Reflect")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

	def build(self):

		cmake = CMake(self)

		if os.path.isdir("../SPIRV-Reflect") or os.path.isdir("../../SPIRV-Reflect"):
			cmake.configure(build_script_folder="SPIRV-Reflect")
		else:
			cmake.configure()

		cmake.build()

	def package(self):

		copy(self, "*.h", os.path.join(self.source_folder, "SPIRV-Reflect"), os.path.join(self.package_folder, "include/SPIRV-Reflect"))
		copy(self, "LICENSE", os.path.join(self.source_folder, "SPIRV-Reflect"), self.package_folder)

		copy(self, "*.h", os.path.join(self.source_folder, "SPIRV-Reflect/include"), os.path.join(self.package_folder, "include/include"))

		lib_dst = os.path.join(self.package_folder, "lib")

		# Linux (single-config) writes the archive directly under the build folder;
		# MSVC (multi-config) writes the lib into a <BuildType> subfolder.
		# Copy from the config that was actually built (build_type), so RelWithDebInfo / MinSizeRel work too
		# rather than only Debug / Release.
		copy(self, "*.a", self.build_folder, lib_dst)

		lib_cfg_src = os.path.join(self.build_folder, str(self.settings.build_type))
		copy(self, "*.lib", lib_cfg_src, lib_dst)
		copy(self, "*.pdb", lib_cfg_src, lib_dst)

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "spirv_reflect")
		self.cpp_info.set_property("cmake_target_name", "spirv_reflect::spirv_reflect")
		self.cpp_info.set_property("pkg_config_name", "spirv_reflect")
		self.cpp_info.libs = [ "spirv-reflect-static" ]
