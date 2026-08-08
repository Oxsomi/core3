from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy
from conan.tools.scm import Git
import os

required_conan_version = ">=2.0"

class vulkan_validation_layers(ConanFile):

	name = "vulkan_validation_layers"
	version = "2025.01.25"

	license = "Apache"
	author = "KhronosGroup"
	url = "https://github.com/Oxsomi/Vulkan-ValidationLayers"
	description = "Vulkan Validation Layers (VVL)"

	settings = "os", "compiler", "build_type", "arch"

	exports_sources = [ "Vulkan-ValidationLayers/LICENSE.txt" ]

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "Vulkan-ValidationLayers")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

	def build(self):
		cmake = CMake(self)
		cmake.configure(build_script_folder="Vulkan-ValidationLayers")
		cmake.build()
		cmake.install()
		
	def generate(self):

		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)
		tc.cppstd = "20"
		tc.cache_variables["ANDROID_USE_LEGACY_TOOLCHAIN_FILE"] = False
		tc.cache_variables["UPDATE_DEPS"] = True
		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)
		tc.generate()

	def package(self):
		copy(self, "LICENSE.txt", os.path.join(self.source_folder, "Vulkan-ValidationLayers"), self.package_folder)
		copy(self, "libVkLayer_khronos_validation.so", os.path.join(self.source_folder, "lib"), os.path.join(self.package_folder, "lib"))

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "vvl")
		self.cpp_info.set_property("cmake_target_name", "vvl::vvl")
		self.cpp_info.set_property("pkg_config_name", "vvl")

		# self.cpp_info.libs = [ "VkLayer_khronos_validation" ]		Can't do this, will attempt to link at runtime
