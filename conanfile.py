from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy
import os
import shutil

required_conan_version = ">=2.0"

class oxc3(ConanFile):

	name = "oxc3"
	version = "0.2.095"

	# Optional metadata
	license = "GPLv3 and dual licensable"
	author = "Oxsomi / Nielsbishere"
	url = "https://github.com/Oxsomi/core3"
	description = "Oxsomi Core3 is a combination of standalone C libraries useful for building applications, such as types, platform, graphics abstraction and file formats"
	topics = ("c", "windows", "linux", "android", "hashing", "encryption", "osx", "vulkan", "simd", "d3d12", "directx12", "shader-compiler")

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"

	options = {
		"forceVulkan": [ True, False ],
		"enableSIMD": [ True, False ],
		"enableTests": [ True, False ],
		"enableOxC3CLI": [ True, False ],
		"forceFloatFallback": [ True, False ],
		"enableShaderCompiler": [ True, False ],
		"cliGraphics": [ True, False ],
		"dynamicLinkingGraphics": [ True, False ]
	}

	default_options = {
		"forceVulkan": False,
		"enableSIMD": True,
		"enableTests": False,
		"enableOxC3CLI": True,
		"forceFloatFallback": False,
		"enableShaderCompiler": True,
		"cliGraphics": True,
		"dynamicLinkingGraphics": False
	}

	exports_sources = [ "inc/*", "cmake/*" ]

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
		tc.cache_variables["DynamicLinkingGraphics"] = self.options.dynamicLinkingGraphics
		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)
		tc.generate()

	def source(self):
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

		hasD3D12 = not self.options.forceVulkan and self.settings.os == "Windows"

		if self.options.dynamicLinkingGraphics and self.settings.os == "Windows":
			hasD3D12 = True

		if self.options.enableShaderCompiler or hasD3D12:
			self.requires("nvapi/2024.09.21")

		if hasD3D12:
			self.requires("agility_sdk/2024.09.22")

		if hasD3D12 and self.settings.arch == "x86_64":
			self.requires("ags/2024.09.21")

		if self.options.enableShaderCompiler:
			self.requires("dxc/2025.01.11")
			self.requires("spirv_reflect/2024.09.22")

		if self.settings.os == "Linux":
			self.requires("xdg_shell/2024.10.21")
			self.requires("xdg_decoration/2024.12.22")

		self.requires("openal_soft/2024.11.04.01")

	def package(self):

		cmake = CMake(self)
		cmake.build()

		copy(self, "*.cmake", os.path.join(self.source_folder, "cmake"), os.path.join(self.package_folder, "cmake"))

		inc_src = os.path.join(self.source_folder, "inc")
		inc_dst = os.path.join(self.package_folder, "include")
		copy(self, "*.h", inc_src, inc_dst)
		copy(self, "*.hpp", inc_src, inc_dst)

		lib_dst = os.path.join(self.package_folder, "lib")
		bin_dst = os.path.join(self.package_folder, "bin")

		if self.settings.arch == "x86_64":
			archName = "x64"
		else:
			archName = "arm64"

		if self.settings.os == "Windows":
			platform = "windows"
		elif self.settings.os == "Macos":
			platform = "osx"
		elif self.settings.os == "Android":
			platform = "android"
		else:
			platform = "linux"

		# Android always appends <configPath>/build/Debug (etc.) for our package dir since we park our config there.
		# Windows it stays build/
		# Linux will keep it relative to core3/build/Debug and so we just append platform/archName

		if self.build_folder.replace("\\", "/").endswith("core3/build/" + str(self.settings.build_type)):
			input_dir = os.path.join(self.build_folder, platform + "/" + archName)

		elif self.build_folder.replace("\\", "/").endswith("/build/" + str(self.settings.build_type)):
			input_dir = self.build_folder + "/../../"

		else:
			input_dir = os.path.join(self.build_folder, str(self.settings.build_type) + "/" + platform + "/" + archName)

		input_lib_dir = os.path.join(input_dir, "lib")
		input_bin_dir = os.path.join(input_dir, "bin")

		copy(self, "*.a", input_lib_dir, lib_dst)
		copy(self, "*.lib", input_lib_dir, lib_dst)
		copy(self, "*.pdb", input_lib_dir, lib_dst)
		copy(self, "*.exp", input_lib_dir, lib_dst)
		copy(self, "*", input_bin_dir, bin_dst)

	def package_info(self):

		if self.settings.os == "Windows":
			self.cpp_info.system_libs = [ "Bcrypt" ]

		elif self.settings.os == "Macos" or self.settings.os == "iOS" or self.settings.os == "watchOS":
			self.cpp_info.frameworks = [ "Security", "CoreFoundation", "ApplicationServices", "AppKit" ]

		else:
			self.cpp_info.system_libs = [ "m" ]

		vulkan = False

		self.cpp_info.libs = [ "OxC3_formats_bmp", "OxC3_formats_oiBC" ]
		self.cpp_info.libs += [ "OxC3_graphics", "OxC3_formats_oiSH", "OxC3_formats_oiSB", "OxC3_platforms", "OxC3_formats_dds", "OxC3_formats_oiCA", "OxC3_formats_oiDL", "OxC3_formats_oiXX", "OxC3_types_container", "OxC3_types_math", "OxC3_types_base" ]

		if self.settings.os != "Windows":
			self.cpp_info.system_libs += [ "vulkan" ]
			vulkan = True

		elif self.options.forceVulkan or self.options.dynamicLinkingGraphics:
			self.cpp_info.libs += [ "vulkan-1" ]
			vulkan = True

		if self.settings.os == "Android":
			self.cpp_info.system_libs += [ "android", "log" ]

		vulkanMacos = os.path.join(os.environ['VULKAN_SDK'], "macOS")

		if os.path.isdir(vulkanMacos):
			self.cpp_info.libdirs += [ os.path.join(vulkanMacos, "lib") ]
			self.cpp_info.includedirs += [ os.path.join(vulkanMacos, "include") ]

		elif vulkan and os.path.isdir(os.path.join(os.environ['VULKAN_SDK'], "include")):
			self.cpp_info.libdirs += [ os.path.join(os.environ['VULKAN_SDK'], "lib") ]
			self.cpp_info.includedirs += [ os.path.join(os.environ['VULKAN_SDK'], "include") ]

		self.cpp_info.set_property("cmake_file_name", "oxc3")
		self.cpp_info.set_property("cmake_target_name", "oxc3::oxc3")
		self.cpp_info.set_property("pkg_config_name", "oxc3")
		self.cpp_info.set_property("cmake_build_modules", [os.path.join("cmake", "oxc3.cmake")])
