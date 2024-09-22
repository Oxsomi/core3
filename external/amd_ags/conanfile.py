from conan import ConanFile
from conan.tools.files import copy
from conan.tools.scm import Git
import os

required_conan_version = ">=2.0"

class ags(ConanFile):

	name = "ags"
	version = "2024.09.21"

	license = "MIT"
	author = "AMD (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/AGS_SDK"
	description = "Fork of AMD's AGS SDK that contains fixes for C support"
	topics = ("amd", "gpu", "pre-built")

	exports_sources = [ "ags_lib/inc/*", "ags_lib/LICENSE.txt" ]

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "AGS_SDK")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

	def package(self):

		copy(self, "*.h", os.path.join(self.source_folder, "AGS_SDK/ags_lib/inc"), os.path.join(self.package_folder, "include"))
		copy(self, "LICENSE.txt", os.path.join(self.source_folder, "AGS_SDK/ags_lib"), self.package_folder)
		
		copy(self, "amd_ags_x64_2022_MT.lib", os.path.join(self.source_folder, "AGS_SDK/ags_lib/lib"), os.path.join(self.package_folder, "lib"))

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "ags")
		self.cpp_info.set_property("cmake_target_name", "ags::ags")
		self.cpp_info.set_property("pkg_config_name", "ags")
		
		self.cpp_info.libs = [ "amd_ags_x64_2022_MT" ]
