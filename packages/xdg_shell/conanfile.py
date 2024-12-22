from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy, download, save, collect_libs
import os

required_conan_version = ">=2.0"

class xdg_shell(ConanFile):

	name = "xdg_shell"
	version = "2024.10.21"

	license = "MIT license"
	author = "Several (see xdg_shell.xml)"
	url = "https://cgit.freedesktop.org/wayland/wayland-protocols/plain/stable/xdg-shell/xdg-shell.xml"
	description = "Built headers and source file of xdg_shell."

	exports_sources = [ "*.h" ]

	settings = "os", "compiler", "build_type", "arch"

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def generate(self):

		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)
		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)
		tc.generate()

	def source(self):
		download(self, "https://cgit.freedesktop.org/wayland/wayland-protocols/plain/stable/xdg-shell/xdg-shell.xml", "xdg_shell.xml")

		self.run("wayland-scanner private-code xdg_shell.xml xdg_shell_protocol.c")
		self.run("wayland-scanner client-header xdg_shell.xml xdg_shell_client_protocol.h")

		save(self, "CMakeLists.txt", "cmake_minimum_required(VERSION 3.13.0)\nproject(xdg_shell)\nset(CMAKE_C_STANDARD 17)\nset(CMAKE_C_STANDARD_REQUIRED OFF)\nset(CMAKE_C_EXTENSIONS ON)\nset(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)\nadd_library(xdg_shell STATIC xdg_shell_protocol.c xdg_shell_client_protocol.h)\n") 

	def build(self):
		cmake = CMake(self)
		cmake.configure()
		cmake.build()

	def package(self):

		cmake = CMake(self)
		cmake.build()

		copy(self, "*.a", self.build_folder, os.path.join(self.package_folder, "lib"))
		copy(self, "*.h", self.source_folder, os.path.join(self.package_folder, "include"))

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "xdg_shell")
		self.cpp_info.set_property("cmake_target_name", "xdg_shell::xdg_shell")
		self.cpp_info.set_property("pkg_config_name", "xdg_shell")
		self.cpp_info.system_libs = [ "wayland-client" ]
		self.cpp_info.libs = collect_libs(self)
