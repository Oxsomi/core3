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

	python_requires = "oxc3_sanitizers/1.0"

	# Sanitizer wiring lives in the shared oxc3_sanitizers python_requires so dxc, spirv_reflect and
	# openal_soft can't drift apart. Only the disabled UBSan checks differ per dependency:
	#  vptr      needs RTTI across the whole program, which the prebuilt setup lacks.
	#  function  would only catch this dependency's own callback-type mismatches, which are not ours to fix.
	#  enum      flag enums and sentinels are legal by design but are not individual enumerators.

	#  null is NOT disabled here: wasapi.cpp is the only file that trips it, so it goes through the ignorelist
	#  next to this recipe instead, which leaves the check doing its job in the rest of openal.

	def _sanitizerFlags(self):
		return self.python_requires["oxc3_sanitizers"].module.sanitizerFlags(
			self, "vptr,function,enum", os.path.join(self.recipe_folder, "sanitizer_ignorelist.txt")
		)

	def _sanitizerLinkFlags(self):
		return self.python_requires["oxc3_sanitizers"].module.sanitizerLinkFlags(self)


	exports_sources = [ "include/*" ]

	#Exported rather than exports_sources: _sanitizerFlags reads it from recipe_folder at generate time, so it
	#has to sit next to the conanfile in the cache rather than in the source tree.

	exports = [ "sanitizer_ignorelist.txt" ]

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

		# Force the static RELEASE CRT even in a Debug build. OxC3 links /MT in every config (its CMakeLists
		# pins it), so a Debug openal on /MTd would be a CRT mismatch; and clang-cl's ASan refuses the debug
		# CRT outright ("/MTd not allowed with -fsanitize=address"). Setting runtime_type here (rather than only
		# CMAKE_MSVC_RUNTIME_LIBRARY in generate(), which conan's own msvc-runtime block overrode back to
		# MultiThreadedDebug) makes conan itself emit MultiThreaded, and since configure() runs during graph
		# expansion the package id stays consistent between the graph and the build.
		if self.settings.os == "Windows":
			self.settings.compiler.runtime_type = "Release"

	def generate(self):
		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)
		tc.cppstd = "20"

		# Belt-and-suspenders for the static release CRT; the load-bearing part is runtime_type=Release in
		# configure() (this alone was overridden by conan's own msvc-runtime block). See that comment.
		tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded"

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