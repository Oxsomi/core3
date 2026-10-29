import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualRunEnv

class OxC3PackageTest(ConanFile):

	settings = "os", "compiler", "build_type", "arch"
	test_type = "explicit"

	def requirements(self):
		self.requires(self.tested_reference_str)

	def layout(self):
		cmake_layout(self)

	def generate(self):

		dependency = self.dependencies["oxc3"]
		compiler = bool(dependency.options.enableShaderCompiler)
		dynamicCompiler = compiler and bool(dependency.options.dynamicLinkingShaderCompiler)
		dynamicCompiler = dynamicCompiler and str(self.settings.os) not in ("Android", "Emscripten")

		CMakeDeps(self).generate()
		VirtualRunEnv(self).generate()
		toolchain = CMakeToolchain(self)
		toolchain.user_presets_path = False
		toolchain.variables["OXC3_TEST_COMPILER"] = compiler
		toolchain.variables["OXC3_TEST_DYNAMIC_COMPILER"] = dynamicCompiler
		toolchain.variables["OXC3_TEST_GRAPHICS"] = str(self.settings.os) != "Emscripten"
		toolchain.variables["OXC3_TEST_SIMD"] = int(bool(dependency.options.enableSIMD))
		toolchain.variables["OXC3_TEST_ERROR_FRAMES"] = 32 if str(dependency.settings.build_type) == "Debug" else 1

		# These are installed dependency paths; the consumer never sees the producer's source or build tree.
		runtimeDirs = []

		for package in self.dependencies.host.values():
			runtimeDirs.extend(package.cpp_info.bindirs)
			runtimeDirs.extend(package.cpp_info.libdirs)

		toolchain.variables["CONAN_RUNTIME_LIB_DIRS"] = ";".join(dict.fromkeys(runtimeDirs)).replace("\\", "/")
		toolchain.variables["OXC3_TEST_PACKAGE_BIN"] = os.path.join(dependency.package_folder, "bin").replace("\\", "/")
		toolchain.generate()

	def build(self):

		cmake = CMake(self)
		cmake.configure()
		cmake.build()

	def test(self):

		if can_run(self):
			self.run(
				f'ctest --output-on-failure -C "{self.settings.build_type}"', cwd=self.build_folder, env="conanrun"
			)
