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

	# Deliberately NOT sanitized, unlike dxc and spirv_reflect which share the oxc3_sanitizers python_requires.
	# The enableASAN/enableUBSAN options stay declared because build_common.py passes them and they keep the
	# package id distinct, but they no longer change how openal is compiled.
	#
	# Two reasons. A sanitized openal reported only its own defects, never ours: a null WAVEFORMATEXTENSIBLE in
	# the WASAPI backend (needed a UBSan ignorelist for that one file), then an access violation inside
	# al::base_exception::what() on the no-audio path that aborts any headless sanitizer run. Neither is
	# actionable here. And openal is a stepping stone rather than the destination, so hardening its internals
	# buys nothing that survives owning the audio stack outright.
	#
	# The cost, for whoever revisits this: openal's own reads and writes of buffers OxC3 hands it are no longer
	# checked, so ASan will not catch us misusing the audio API. Nothing else leaks across, since OxC3 is C and
	# no instrumented container crosses the boundary. Mixing an uninstrumented DLL into a sanitized process is
	# supported, and the MSVC STL annotation defines are moot here because those annotations only exist under
	# ASan in the first place.
	#
	# See build/reports/third_party_findings.md for both findings, still unfiled.

	exports_sources = [ "include/*" ]

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

		# Force the static RELEASE CRT even in a Debug build. OxC3 links /MT in every config (its CMakeLists
		# pins it), so a Debug openal on /MTd would be a CRT mismatch. This used to matter for a second reason
		# too, clang-cl's ASan refusing the debug CRT outright, which no longer applies now that openal is not
		# sanitized; the CRT match alone still requires it. Setting runtime_type here (rather than only
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