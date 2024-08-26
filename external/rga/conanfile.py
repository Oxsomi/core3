from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename
import os

required_conan_version = ">=2.0"

class rga(ConanFile):

	name = "rga"
	version = "2024.08.26"

	# Optional metadata
	license = "MIT License"
	author = "GPUOpen-Tools (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/radeon_gpu_analyzer"
	description = "Fork of AMD's RGA: The Radeon GPU Analyzer (RGA) is an offline compiler and code analysis tool for Vulkan, DirectX, OpenGL, and OpenCL."
	topics = ("gpu", "radeon", "analyzer", "dxil", "shader-programs", "spirv")

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"

	exports_sources = [ "source/radeon_gpu_analyzer_backend/*.h" ]

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def generate(self):
		cwd = os.getcwd()
		print(cwd)

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "radeon_gpu_analyzer")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])
		git.run("submodule update --init --recursive")

	def build(self):
		cwd = os.getcwd()
		build_folder = os.path.join(self.source_folder, "radeon_gpu_analyzer/build")
		os.chdir(build_folder)
		os.system("python pre_build.py --no-qt --no-dx10 --build --no-vulkan --use-mt")
		os.chdir(cwd)

	def package(self):

		self.build()

		lib_dst = os.path.join(self.package_folder, "lib")
		bin_dst = os.path.join(self.package_folder, "bin")
		include_dst = os.path.join(self.package_folder, "include")

		build_folder = os.path.join(self.source_folder, "radeon_gpu_analyzer/build")
		root_folder = os.path.join(self.source_folder, "radeon_gpu_analyzer")
		source_folder = os.path.join(self.source_folder, "radeon_gpu_analyzer/source")
		device_info_folder = os.path.join(self.source_folder, "radeon_gpu_analyzer/external/device_info")

		copy(self, "radeon_gpu_analyzer_backend/*.h", source_folder, include_dst)
		copy(self, "external/amdt_base_tools/Include/*.h", root_folder, include_dst)
		copy(self, "external/dx11/AsicReg/*.h", root_folder, include_dst)
		copy(self, "*.h", device_info_folder, include_dst)

		if self.settings.build_type == "Debug":
			copy(self, "*/debug/arch/*.lib", build_folder, lib_dst, keep_path=False)
			copy(self, "*/debug/arch/*.a", build_folder, lib_dst, keep_path=False)
			copy(self, "*/debug/arch/*.exp", build_folder, lib_dst, keep_path=False)
			copy(self, "*/debug/arch/*.pdb", build_folder, lib_dst, keep_path=False)
			copy(self, "*/*/debug/*.txt", build_folder, bin_dst, keep_path=False)

		else:
			copy(self, "*/release/arch/*.lib", build_folder, lib_dst, keep_path=False)
			copy(self, "*/release/arch/*.a", build_folder, lib_dst, keep_path=False)
			copy(self, "*/release/arch/*.exp", build_folder, lib_dst, keep_path=False)
			copy(self, "*/release/arch/*.pdb", build_folder, lib_dst, keep_path=False)
			copy(self, "*/*/release/*.txt", build_folder, bin_dst, keep_path=False)

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "rga")
		self.cpp_info.set_property("cmake_target_name", "rga::rga")
		self.cpp_info.set_property("pkg_config_name", "rga")
		
		self.cpp_info.includedirs = [ "include", "include/external" ]
		self.cpp_info.defines = [ "AMDT_PUBLIC" ]

		self.cpp_info.libs = [
			"radeon_gpu_analyzer_backend",
			"dx12_backend",
			"AMDTOSWrappers",
			# "tinyxml2",
			"AMDTBaseTools"
		]
