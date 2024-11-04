from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename, rm
import os
import glob

required_conan_version = ">=2.0"

class openal_soft(ConanFile):

	name = "openal_soft"
	version = "2024.11.04.01"

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

		lib_src = os.path.join(self.build_folder, "lib")
		lib_dst = os.path.join(self.package_folder, "lib")
		bin_dst = os.path.join(self.package_folder, "bin")

		# Linux, OSX, etc. all run from build/Debug or build/Release, so we need to change it a bit
		if not self.settings.os == "Windows":
		
			if self.settings.os == "Linux":
			
				rm(self, "*.so", self.build_folder)		# Imposter!
			
				soVersion = glob.glob(os.path.join(self.build_folder, "*.so.*.*.*"))
				
				if not len(soVersion) == 1:
					error("Can't find real SO!")
					
				rename(self, soVersion[0], soVersion[0].rsplit('.', 3)[0])
				copy(self, "*.so", self.build_folder, lib_dst)
				copy(self, "*.so", self.build_folder, bin_dst)
				
			else:
				copy(self, "*.dylib", self.build_folder, lib_dst)
				copy(self, "*.dylib", self.build_folder, bin_dst)
			
			copy(self, "*.a", self.build_folder, lib_dst)
			
		# Windows uses more complicated setups
		else:

			lib_dbg_src = os.path.join(self.build_folder, "Debug")

			copy(self, "*.lib", lib_dbg_src, lib_dst)
			copy(self, "*.pdb", lib_dbg_src, lib_dst)
			copy(self, "*.exp", lib_dbg_src, lib_dst)
			copy(self, "*.dll", lib_dbg_src, bin_dst)

			# Copy release libs
			
			lib_rel_src = os.path.join(self.build_folder, "Release")

			copy(self, "*.lib", lib_rel_src, lib_dst)
			copy(self, "*.pdb", lib_rel_src, lib_dst)
			copy(self, "*.exp", lib_rel_src, lib_dst)
			copy(self, "*.dll", lib_rel_src, bin_dst)

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "openal_soft")
		self.cpp_info.set_property("cmake_target_name", "openal_soft::openal_soft")
		self.cpp_info.set_property("pkg_config_name", "openal_soft")
		
		if not self.settings.os == "Windows":
			self.cpp_info.libs = [ "alsoft.common", "alsoft.excommon", "openal" ]

		else:
			self.cpp_info.libs = [ "alsoft.common", "alsoft.excommon", "OpenAL32" ]
