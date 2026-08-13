from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy, download, save, collect_libs
from io import StringIO
import os
import shutil

required_conan_version = ">=2.0"


def waylandProtocol(conanfile, relativePath, output):
	"""Put a wayland protocol xml at `output`, preferring the copy the system already has.

	freedesktop puts both cgit and gitlab behind Anubis, which answers anything that isn't a browser with a 418
	or an access denied page, so neither URL can be relied on from CI.
	Every distro we build on ships these same files in the wayland-protocols package, which is also where
	wayland-scanner users are meant to read them from, so ask pkg-config where they live and copy from there.
	The download stays as a fallback for a machine without the package, and will simply fail as it does today
	if freedesktop is still blocking.
	"""

	pkgDataDir = StringIO()

	try:
		conanfile.run("pkg-config --variable=pkgdatadir wayland-protocols", stdout=pkgDataDir)
		local = os.path.join(pkgDataDir.getvalue().strip(), relativePath)
	except Exception:
		local = ""

	if local and os.path.isfile(local):
		conanfile.output.info(f"Using system wayland-protocols copy of {relativePath}")
		shutil.copyfile(local, output)
		return

	conanfile.output.warning(f"No system wayland-protocols copy of {relativePath}, falling back to the network")

	download(
		conanfile,
		[
			f"https://gitlab.freedesktop.org/wayland/wayland-protocols/-/raw/1.38/{relativePath}",
			f"https://cgit.freedesktop.org/wayland/wayland-protocols/plain/{relativePath}",
		],
		output,
		retry=3,
		retry_wait=5,
	)

class xdg_decoration(ConanFile):

	name = "xdg_decoration"
	version = "2024.12.22"

	license = "MIT license"
	author = "Several (see xdg_decoration.xml)"
	url = "https://cgit.freedesktop.org/wayland/wayland-protocols/plain/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
	description = "Built headers and source file of xdg_decoration."

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

		waylandProtocol(self, "unstable/xdg-decoration/xdg-decoration-unstable-v1.xml", "xdg_decoration.xml")

		self.run("wayland-scanner private-code xdg_decoration.xml xdg_decoration_protocol.c")
		self.run("wayland-scanner client-header xdg_decoration.xml xdg_decoration_client_protocol.h")

		save(self, "CMakeLists.txt", "cmake_minimum_required(VERSION 3.13.0)\nproject(xdg_decoration)\nset(CMAKE_C_STANDARD 17)\nset(CMAKE_C_STANDARD_REQUIRED OFF)\nset(CMAKE_C_EXTENSIONS ON)\nset(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)\nadd_library(xdg_decoration STATIC xdg_decoration_protocol.c xdg_decoration_client_protocol.h)\n") 

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
		self.cpp_info.set_property("cmake_file_name", "xdg_decoration")
		self.cpp_info.set_property("cmake_target_name", "xdg_decoration::xdg_decoration")
		self.cpp_info.set_property("pkg_config_name", "xdg_decoration")
		self.cpp_info.system_libs = [ "wayland-client" ]
		self.cpp_info.libs = collect_libs(self)
