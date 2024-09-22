from conan import ConanFile
from conan.tools.files import copy, download, unzip, rename, rm
import os

required_conan_version = ">=2.0"

class agility_sdk(ConanFile):

	name = "agility_sdk"
	version = "2024.09.22"

	license = "Microsoft DirectX, Direct3D WARP and MIT licenses"
	author = "Microsoft"
	url = "https://www.nuget.org/profiles/Direct3D"
	description = "Library that contains both d3d10warp.dll and the agility sdk"
	topics = ("microsoft", "d3d12", "agility", "directx", "warp", "pre-built")

	exports_sources = [ "agility/build/native/include/*.h", "agility/LICENSE.txt", "agility/LICENSE-CODE.txt", "warp/LICENSE.txt" ]

	settings = "arch"

	def source(self):

		download(self, "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.WARP", "warp.zip")
		unzip(self, "warp.zip", "warp")

		download(self, "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12", "d3d12.zip")
		unzip(self, "d3d12.zip", "agility")

	def package(self):

		copy(self, "*.h", os.path.join(self.source_folder, "agility/build/native/include"), os.path.join(self.package_folder, "include"))

		copy(self, "LICENSE.txt", os.path.join(self.source_folder, "agility"), self.package_folder)
		rename(self, os.path.join(self.package_folder, "LICENSE.txt"), os.path.join(self.package_folder, "LICENSE-AGILITY.txt"))

		copy(self, "LICENSE-CODE.txt", os.path.join(self.source_folder, "agility"), self.package_folder)

		copy(self, "LICENSE.txt", os.path.join(self.source_folder, "warp"), self.package_folder)
		rename(self, os.path.join(self.package_folder, "LICENSE.txt"), os.path.join(self.package_folder, "LICENSE-WARP.txt"))
		
		if self.settings.arch == "x86_64":
			copy(self, "*.dll", os.path.join(self.source_folder, "warp/build/native/bin/x64"), os.path.join(self.package_folder, "bin"))
			copy(self, "*.pdb", os.path.join(self.source_folder, "warp/build/native/bin/x64"), os.path.join(self.package_folder, "bin"))
			copy(self, "*.dll", os.path.join(self.source_folder, "agility/build/native/bin/x64"), os.path.join(self.package_folder, "bin/D3D12"))
			copy(self, "*.pdb", os.path.join(self.source_folder, "agility/build/native/bin/x64"), os.path.join(self.package_folder, "bin/D3D12"))
			rm(self, "d3dconfig.pdb", os.path.join(self.package_folder, "bin/D3D12"))

		else:
			copy(self, "*.dll", os.path.join(self.source_folder, "warp/build/native/bin/arm64"), os.path.join(self.package_folder, "bin"))
			copy(self, "*.pdb", os.path.join(self.source_folder, "warp/build/native/bin/arm64"), os.path.join(self.package_folder, "bin"))
			copy(self, "*.dll", os.path.join(self.source_folder, "agility/build/native/bin/arm64"), os.path.join(self.package_folder, "bin/D3D12"))
			copy(self, "*.pdb", os.path.join(self.source_folder, "agility/build/native/bin/arm64"), os.path.join(self.package_folder, "bin/D3D12"))
			rm(self, "d3dconfig.pdb", os.path.join(self.package_folder, "bin/D3D12"))

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "agility_sdk")
		self.cpp_info.set_property("cmake_target_name", "agility_sdk::agility_sdk")
		self.cpp_info.set_property("pkg_config_name", "agility_sdk")
		self.cpp_info.system_libs = [ "dxgi", "d3d12" ]
