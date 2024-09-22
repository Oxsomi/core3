from conan import ConanFile
from conan.tools.files import copy
from conan.tools.scm import Git
import os

required_conan_version = ">=2.0"

class nvapi(ConanFile):

	name = "nvapi"
	version = "2024.09.21"

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

	def package(self):

		copy(self, "*.h", os.path.join(self.source_folder, "nvapi"), os.path.join(self.package_folder, "include"))
		copy(self, "License.txt", os.path.join(self.source_folder, "nvapi"), self.package_folder)
		
		# We'll only have a link target for windows x64 (D3D12 release),
		# otherwise it's just shader includes for the shader compiler

		if self.settings.os == "Windows" and self.settings.arch == "x86_64":
			copy(self, "nvapi64.lib", os.path.join(self.source_folder, "nvapi/amd64"), os.path.join(self.package_folder, "lib"))

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "nvapi")
		self.cpp_info.set_property("cmake_target_name", "nvapi::nvapi")
		self.cpp_info.set_property("pkg_config_name", "nvapi")

		if self.settings.os == "Windows" and self.settings.arch == "x86_64":
			self.cpp_info.libs = [ "nvapi64" ]
