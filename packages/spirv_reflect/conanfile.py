from conan import ConanFile
from conan.tools.files import copy
from conan.tools.scm import Git
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
import os

required_conan_version = ">=2.0"

class spirv_reflect(ConanFile):

	name = "spirv_reflect"
	version = "2024.09.22"

	license = "Apache License 2.0"
	author = "KhronosGroup"
	url = "https://github.com/Oxsomi/SPIRV-Reflect"
	description = "SPIRV-Reflect is a lightweight library that provides a C/C++ reflection API for SPIR-V shader bytecode in Vulkan applications."
	topics = ("khronos", "gpu", "spirv")

	exports_sources = [ "*.h", "include/*", "LICENSE" ]

	settings = "os", "compiler", "build_type", "arch"

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

		lib_src = os.path.join(self.build_folder, "lib")
		lib_dst = os.path.join(self.package_folder, "lib")
		lib_deb_src = os.path.join(self.build_folder, "Debug")
		lib_rel_src = os.path.join(self.build_folder, "Release")

		copy(self, "*.a", self.build_folder, lib_dst)

		copy(self, "*.lib", lib_deb_src, lib_dst)
		copy(self, "*.pdb", lib_deb_src, lib_dst)

		copy(self, "*.lib", lib_rel_src, lib_dst)
		copy(self, "*.pdb", lib_rel_src, lib_dst)

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "spirv_reflect")
		self.cpp_info.set_property("cmake_target_name", "spirv_reflect::spirv_reflect")
		self.cpp_info.set_property("pkg_config_name", "spirv_reflect")
		self.cpp_info.libs = [ "spirv-reflect-static" ]
