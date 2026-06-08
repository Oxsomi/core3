from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename, rm
import os
import glob

required_conan_version = ">=2.0"

class openal_soft(ConanFile):

	name = "openal_soft"
	version = "2026.06.04"

	# Optional metadata
	license = "BSD-3 License"
	author = "kcat, creative labs, etc."
	url = "https://github.com/Oxsomi/openal-soft"
	description = "OpenAL Soft is a software implementation of the OpenAL 3D audio API."

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"

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

		tc.generate()

	def system_requirements(self):
		if self.settings.os == "Linux":
			self.run("sudo apt-get install -y libasound2-dev libpipewire-0.3-dev", ignore_errors=True)

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