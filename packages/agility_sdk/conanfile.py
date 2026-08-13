from conan import ConanFile
from conan.tools.files import copy, download, unzip, rename, rm
import os

required_conan_version = ">=2.0"

class agility_sdk(ConanFile):

	name = "agility_sdk"
	version = "2026.07.29"

	license = "Microsoft DirectX, Direct3D WARP and MIT licenses"
	author = "Microsoft"
	url = "https://www.nuget.org/profiles/Direct3D"
	description = "Library that contains both d3d10warp.dll (1.0.20) and the agility sdk (1.721.2-preview)"
	topics = ("microsoft", "d3d12", "agility", "directx", "warp", "pre-built")

	exports_sources = [ "agility/build/native/include/*.h", "agility/LICENSE.txt", "agility/LICENSE-CODE.txt", "warp/LICENSE.txt" ]

	settings = "os", "arch"

	def source(self):

		download(self, "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.WARP/1.0.20", "warp.zip")
		unzip(self, "warp.zip", "warp")

		download(self, "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.721.2-preview", "d3d12.zip")
		unzip(self, "d3d12.zip", "agility")

	#Find a LICENSE.txt case-insensitively under `folder` and stage it into the package as `dstName`.
	#nuget packages vary in casing and copy()'s fnmatch is case-sensitive on Linux, so a plain copy can miss it.

	def _stage_license(self, folder, dstName):

		for root, _, files in os.walk(folder):
			for name in files:
				if name.lower() == "license.txt":
					copy(self, name, root, self.package_folder)
					rename(self, os.path.join(self.package_folder, name), os.path.join(self.package_folder, dstName))
					return

		self.output.warning("agility_sdk: no LICENSE.txt found in " + folder + "; skipping " + dstName)

	def package(self):

		copy(self, "*.h", os.path.join(self.source_folder, "agility/build/native/include"), os.path.join(self.package_folder, "include"))

		copy(self, "LICENSE.txt", os.path.join(self.source_folder, "agility"), self.package_folder)
		rename(self, os.path.join(self.package_folder, "LICENSE.txt"), os.path.join(self.package_folder, "LICENSE-AGILITY.txt"))

		copy(self, "LICENSE-CODE.txt", os.path.join(self.source_folder, "agility"), self.package_folder)

		# The WARP nuget's license file casing varies (LICENSE.txt vs license.txt) and copy()'s fnmatch is
		# case-sensitive on Linux, so find it explicitly and stage it as LICENSE-WARP.txt.
		self._stage_license(os.path.join(self.source_folder, "warp"), "LICENSE-WARP.txt")

		# The WARP + agility runtimes are Windows DLLs.
		# Everywhere else (e.g. Linux/macOS building the shader compiler) only the headers are needed
		# (DXC's dxcreflect.h includes d3d12shader.h),
		# so this is an include-only package there, just like nvapi on non-(Windows x64).

		if self.settings.os == "Windows":

			archName = "x64" if self.settings.arch == "x86_64" else "arm64"

			copy(self, "*.dll", os.path.join(self.source_folder, "warp/build/native/bin/" + archName), os.path.join(self.package_folder, "bin"))
			copy(self, "*.pdb", os.path.join(self.source_folder, "warp/build/native/bin/" + archName), os.path.join(self.package_folder, "bin"))
			copy(self, "*.dll", os.path.join(self.source_folder, "agility/build/native/bin/" + archName), os.path.join(self.package_folder, "bin/D3D12"))
			copy(self, "*.pdb", os.path.join(self.source_folder, "agility/build/native/bin/" + archName), os.path.join(self.package_folder, "bin/D3D12"))
			rm(self, "d3dconfig.pdb", os.path.join(self.package_folder, "bin/D3D12"))

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "agility_sdk")
		self.cpp_info.set_property("cmake_target_name", "agility_sdk::agility_sdk")
		self.cpp_info.set_property("pkg_config_name", "agility_sdk")

		# dxgi/d3d12 only exist on Windows; elsewhere this is headers-only (for DXC's d3d12shader.h).
		if self.settings.os == "Windows":
			self.cpp_info.system_libs = [ "dxgi", "d3d12" ]
