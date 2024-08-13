from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy
import os

required_conan_version = ">=2.0"

class oxc3(ConanFile):

	name = "oxc3"
	version = "0.2.0"

	# Optional metadata
	license = "GPLv3 and dual licensable"
	author = "Oxsomi / Nielsbishere"
	url = "https://github.com/Oxsomi/core3"
	description = "Oxsomi Core3 is a combination of standalone C libraries useful for building applications, such as types, platform, graphics abstraction and file formats"
	topics = ("c", "windows", "linux", "hashing", "encryption", "osx", "vulkan", "simd", "d3d12", "directx12", "shader-compiler")

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"

	options = {
		"forceVulkan": [ True, False ],
		"enableSIMD": [ True, False ],
		"enableTests": [ True, False ],
		"enableOxC3CLI": [ True, False ],
		"forceFloatFallback": [ True, False ],
		"enableShaderCompiler": [ True, False ],
		"cliGraphics": [ True, False ]
	}

	default_options = {
		"forceVulkan": False,
		"enableSIMD": True,
		"enableTests": False,
		"enableOxC3CLI": True,
		"forceFloatFallback": False,
		"enableShaderCompiler": True,
		"cliGraphics": True
	}

	exports_sources = "inc/*"

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def generate(self):

		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)
		tc.cache_variables["ForceFloatFallback"] = self.options.forceFloatFallback
		tc.cache_variables["EnableTests"] = self.options.enableTests
		tc.cache_variables["EnableOxC3CLI"] = self.options.enableOxC3CLI
		tc.cache_variables["EnableSIMD"] = self.options.enableSIMD
		tc.cache_variables["ForceVulkan"] = self.options.forceVulkan
		tc.cache_variables["EnableShaderCompiler"] = self.options.enableShaderCompiler
		tc.cache_variables["CLIGraphics"] = self.options.cliGraphics
		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)
		tc.generate()

	def source(self):

		# If it's already cloned then we're in the root of the folder
		if os.path.isdir("docs"):
			git = Git(self)
			git.run("submodule update --init --recursive")

		# Otherwise, we need to do a fresh checkout
		else:
			git = Git(self)
			git.clone(url=self.conan_data["sources"][self.version]["url"])
			git.folder = os.path.join(self.source_folder, "core3")
			git.checkout(self.conan_data["sources"][self.version]["checkout"])
			git.run("submodule update --init --recursive")

	def build(self):

		cmake = CMake(self)

		if os.path.isdir("../core3"):
			cmake.configure(build_script_folder="core3")
		else:
			cmake.configure()

		cmake.build()

	def requirements(self):
		self.requires("dxc/2024.08.05")

	def package(self):
		cmake = CMake(self)
		cmake.build(target="OxC3")
		copy(self, "*.cmake", "cmake", "../../p/cmake")
		copy(self, "*.lib", "lib", "../../p/lib")
		copy(self, "*.a", "lib", "../../p/lib")
		copy(self, "*.lib", "lib/Release", "../../p/lib")
		copy(self, "*.lib", "lib/Debug", "../../p/lib")
		copy(self, "*.a", "lib/Release", "../../p/lib")
		copy(self, "*.a", "lib/Debug", "../../p/lib")
		copy(self, "", "bin", "../../p/bin")
		copy(self, "", "bin/Release", "../../p/bin")
		copy(self, "", "bin/Debug", "../../p/bin")

	def package_info(self):
		self.cpp_info.components["oxc3"].libs = ["OxC3"]
		self.cpp_info.set_property("cmake_file_name", "OxC3")
		self.cpp_info.set_property("cmake_target_name", "OxC3")
		self.cpp_info.set_property("pkg_config_name", "OxC3")
		self.cpp_info.libs = collect_libs(self)