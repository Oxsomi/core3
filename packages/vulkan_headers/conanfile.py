from conan import ConanFile
from conan.tools.files import copy
from conan.tools.scm import Git
import os

required_conan_version = ">=2.0"

class vulkan_headers(ConanFile):

	name = "vulkan_headers"
	version = "2026.07.28"

	license = "Apache-2.0"
	author = "The Khronos Group (original) & Oxsomi (fork)"
	url = "https://github.com/Oxsomi/Vulkan-Headers"
	description = "Vulkan API headers (Oxsomi fork). Headers only; OxC3 loads the Vulkan loader dynamically at runtime, so no import lib or system SDK is needed to build."
	topics = ("vulkan", "khronos", "headers", "graphics")

	settings = "os", "arch"

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "Vulkan-Headers")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

	def package(self):
		src = os.path.join(self.source_folder, "Vulkan-Headers")
		copy(self, "*.h", os.path.join(src, "include"), os.path.join(self.package_folder, "include"))
		copy(self, "LICENSE.md", src, self.package_folder)

	def package_info(self):

		# Masquerade as find_package(Vulkan) so the existing graphics CMake (find_package(Vulkan QUIET) +
		# ${Vulkan_INCLUDE_DIRS}) resolves to these headers with no CMake change. "both" generates a module
		# so the plain find_package(Vulkan) (module mode) resolves too.
		self.cpp_info.set_property("cmake_file_name", "Vulkan")
		self.cpp_info.set_property("cmake_target_name", "Vulkan::Vulkan")
		self.cpp_info.set_property("cmake_find_mode", "both")
		self.cpp_info.set_property("pkg_config_name", "vulkan")

		# Headers only. The loader is dlopen'd / LoadLibrary'd at runtime (see vk_instance.c), so there is no
		# import lib to link, which is why this doesn't need an arch-specific SDK.
		self.cpp_info.includedirs = [ "include" ]
		self.cpp_info.libdirs = []
		self.cpp_info.bindirs = []
