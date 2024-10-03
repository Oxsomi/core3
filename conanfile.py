from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy
import os
import shutil

required_conan_version = ">=2.0"

class oxc3(ConanFile):

	name = "oxc3"
	version = "0.2.037"  

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
		
		isDX12 = not self.options.forceVulkan and self.settings.os == "Windows"

		if self.options.enableShaderCompiler or isDX12:
			self.requires("nvapi/2024.09.21")

		if isDX12:
			self.requires("agility_sdk/2024.09.22")
		
		if self.options.enableShaderCompiler:
			self.requires("dxc/2024.10.03")
			self.requires("spirv_reflect/2024.09.22")

		if not self.options.forceVulkan and self.settings.os == "Windows":
			self.requires("ags/2024.09.21")

	def package(self):

		cmake = CMake(self)
		cmake.build()

		copy(self, "*.cmake", os.path.join(self.source_folder, "cmake"), os.path.join(self.package_folder, "cmake"))

		inc_src = os.path.join(self.source_folder, "inc")
		inc_dst = os.path.join(self.package_folder, "include")
		copy(self, "*.h", inc_src, inc_dst)
		copy(self, "*.hpp", inc_src, inc_dst)

		cwd = os.getcwd()

		lib_dst = os.path.join(self.package_folder, "lib")
		bin_dst = os.path.join(self.package_folder, "bin")

		# Linux, OSX, etc. all run from build/Debug or build/Release, so we need to change it a bit
		if cwd.endswith("Debug") or cwd.endswith("Release"):
			copy(self, "*.a", os.path.join(self.build_folder, "lib"), os.path.join(self.package_folder, "lib"))
			copy(self, "^([^.]+)$", os.path.join(self.build_folder, "bin"), os.path.join(self.package_folder, "bin"))

		# Windows uses more complicated setups
		else:
			dbg_lib_src = os.path.join(self.build_folder, "lib/Debug")
			dbg_bin_src = os.path.join(self.build_folder, "bin/Debug")

			copy(self, "*.lib", dbg_lib_src, lib_dst)
			copy(self, "*.pdb", dbg_lib_src, lib_dst)
			copy(self, "*.exp", dbg_lib_src, lib_dst)

			copy(self, "*.exp", dbg_bin_src, bin_dst)
			copy(self, "*.exe", dbg_bin_src, bin_dst)
			copy(self, "*.dll", dbg_bin_src, bin_dst)
			copy(self, "*.pdb", dbg_bin_src, bin_dst)

			if os.path.isfile(lib_dst):
				for filename in os.listdir(lib_dst):
					f = os.path.join(lib_dst, filename)
					if os.path.isfile(f):
						offset = f.rfind(".")
						rename(self, f, f[:offset] + "d." + f[offset+1:])

			if os.path.isfile(bin_dst):
				for filename in os.listdir(bin_dst):
					f = os.path.join(bin_dst, filename)
					if os.path.isfile(f):
						offset = f.rfind(".")
						rename(self, f, f[:offset] + "d." + f[offset+1:])

			# Copy release libs

			rel_lib_src = os.path.join(self.build_folder, "lib/Release")
			rel_bin_src = os.path.join(self.build_folder, "bin/Release")

			copy(self, "*.lib", rel_lib_src, lib_dst)
			copy(self, "*.pdb", rel_lib_src, lib_dst)
			copy(self, "*.exp", rel_lib_src, lib_dst)

			copy(self, "*.exp", rel_bin_src, bin_dst)
			copy(self, "*.exe", rel_bin_src, bin_dst)
			copy(self, "*.dll", rel_bin_src, bin_dst)
			copy(self, "*.pdb", rel_bin_src, bin_dst)

	def package_info(self):

		if self.settings.os == "Windows":
			self.cpp_info.system_libs = [ "Bcrypt" ]

		elif self.settings.os == "Macos" or self.settings.os == "iOS" or self.settings.os == "watchOS":
			self.cpp_info.frameworks = [ "Security", "CoreFoundation", "ApplicationServices", "AppKit" ]

		else:
			self.cpp_info.system_libs = [ "m" ]

		vulkan = False
		
		self.cpp_info.libs = collect_libs(self)

		if self.settings.os != "Windows":
			self.cpp_info.libs += [ "vulkan" ]
			vulkan = True

		elif self.options.forceVulkan:
			self.cpp_info.libs += [ "vulkan-1" ]
			vulkan = True

		vulkanMacos = os.path.join(os.environ['VULKAN_SDK'], "macOS")

		if os.path.isdir(vulkanMacos):
			self.cpp_info.libdirs += [ os.path.join(vulkanMacos, "lib") ]
			self.cpp_info.includedirs += [ os.path.join(vulkanMacos, "include") ]

		else:
			self.cpp_info.libdirs += [ os.path.join(os.environ['VULKAN_SDK'], "lib") ]
			self.cpp_info.includedirs += [ os.path.join(os.environ['VULKAN_SDK'], "include") ]

		self.cpp_info.set_property("cmake_file_name", "oxc3")
		self.cpp_info.set_property("cmake_target_name", "oxc3::oxc3")
		self.cpp_info.set_property("pkg_config_name", "oxc3")
		self.cpp_info.set_property("cmake_build_modules", [os.path.join("cmake", "oxc3.cmake")])