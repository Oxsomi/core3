from conan import ConanFile
from conan.tools.files import copy
from conan.tools.scm import Git
import os

required_conan_version = ">=2.0"

class nvapi(ConanFile):

	name = "nvapi"
	version = "2026.10.10"

	license = "NVIDIA custom license"
	author = "NVIDIA (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/nvapi"
	description = "Fork of NVAPI that contains fixes for C support"
	topics = ("nv", "gpu", "pre-built")

	exports_sources = [ "*.h", "License.txt" ]

	settings = "os", "arch"

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "nvapi")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

	# The prebuilt library NVIDIA ships for this Windows architecture, as its folder and name, or None.
	# Without one the package is just the headers; NVAPI is used by the D3D12 backend only.
	# ARM64EC has a library too (arm64ec/nvapia64ec), but isn't an OxC3 target.

	def _library(self):

		if self.settings.os != "Windows":
			return None

		return { "x86_64": ("amd64", "nvapi64"), "armv8": ("aarch64", "nvapia64") }.get(str(self.settings.arch))

	def package(self):

		copy(self, "*.h", os.path.join(self.source_folder, "nvapi"), os.path.join(self.package_folder, "include"))
		copy(self, "License.txt", os.path.join(self.source_folder, "nvapi"), self.package_folder)

		library = self._library()

		if library:
			folder, name = library
			lib = os.path.join(self.package_folder, "lib")
			copy(self, name + ".lib", os.path.join(self.source_folder, "nvapi", folder), lib)

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "nvapi")
		self.cpp_info.set_property("cmake_target_name", "nvapi::nvapi")
		self.cpp_info.set_property("pkg_config_name", "nvapi")

		library = self._library()

		if library:
			self.cpp_info.libs = [ library[1] ]
