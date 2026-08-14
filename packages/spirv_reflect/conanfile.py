from conan import ConanFile
from conan.tools.files import copy
from conan.tools.scm import Git
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
import os

required_conan_version = ">=2.0"

class spirv_reflect(ConanFile):

	name = "spirv_reflect"
	version = "2026.08.06"

	license = "Apache License 2.0"
	author = "KhronosGroup"
	url = "https://github.com/Oxsomi/SPIRV-Reflect"
	description = "SPIRV-Reflect is a lightweight library that provides a C/C++ reflection API for SPIR-V shader bytecode in Vulkan applications."
	topics = ("khronos", "gpu", "spirv")

	exports_sources = [ "*.h", "include/*", "LICENSE" ]

	settings = "os", "compiler", "build_type", "arch"
	options = { "enableASAN": [ True, False ], "enableUBSAN": [ True, False ] }
	default_options = { "enableASAN": False, "enableUBSAN": False }

	python_requires = "oxc3_sanitizers/1.0"

	# Sanitizer wiring lives in the shared oxc3_sanitizers python_requires so dxc, spirv_reflect and
	# openal_soft can't drift apart. Only the disabled UBSan checks differ per dependency:
	#  vptr      needs RTTI across the whole program, which the prebuilt setup lacks.
	#  function  would only catch this dependency's own callback-type mismatches, which are not ours to fix.
	#  enum      flag enums and sentinels are legal by design but are not individual enumerators.

	def _sanitizerFlags(self):
		return self.python_requires["oxc3_sanitizers"].module.sanitizerFlags(self, "vptr,function,enum")

	def _sanitizerLinkFlags(self):
		return self.python_requires["oxc3_sanitizers"].module.sanitizerLinkFlags(self)


	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def generate(self):

		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)
		tc.cppstd = "20"
		tc.variables["SPIRV_REFLECT_EXECUTABLE"] = False
		tc.variables["SPIRV_REFLECT_STATIC_LIB"] = True
		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)
		tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded"

		# This static library gets linked into OxC3's shared shader compiler (dynamicLinkingShaderCompiler),
		# and on ELF every object folded into a .so must be position independent. Without this an arm64
		# dynamic build fails at link with "relocation R_AARCH64_ADR_PREL_PG_HI21 ... recompile with -fPIC"
		# (x86_64 happens to tolerate the non-PIC relocations, arm64 does not). OxC3 sets the same flag on its
		# own targets; the dependency has to opt in separately because it is built as its own CMake project.
		tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = True
		for flag in self._sanitizerLinkFlags():
			tc.extra_exelinkflags.append(flag)
			tc.extra_sharedlinkflags.append(flag)

		for flag in self._sanitizerFlags():
			tc.extra_cflags.append(flag)
			tc.extra_cxxflags.append(flag)

		tc.generate()

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "SPIRV-Reflect")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

	def build(self):

		cmake = CMake(self)

		if os.path.isdir("../SPIRV-Reflect") or os.path.isdir("../../SPIRV-Reflect"):
			cmake.configure(build_script_folder="SPIRV-Reflect")
		else:
			cmake.configure()

		cmake.build()

	def package(self):

		copy(self, "*.h", os.path.join(self.source_folder, "SPIRV-Reflect"), os.path.join(self.package_folder, "include/SPIRV-Reflect"))
		copy(self, "LICENSE", os.path.join(self.source_folder, "SPIRV-Reflect"), self.package_folder)

		copy(self, "*.h", os.path.join(self.source_folder, "SPIRV-Reflect/include"), os.path.join(self.package_folder, "include/include"))

		lib_dst = os.path.join(self.package_folder, "lib")

		# Linux (single-config) writes the archive directly under the build folder;
		# MSVC (multi-config) writes the lib into a <BuildType> subfolder.
		# Copy from the config that was actually built (build_type), so RelWithDebInfo / MinSizeRel work too
		# rather than only Debug / Release.
		copy(self, "*.a", self.build_folder, lib_dst)

		lib_cfg_src = os.path.join(self.build_folder, str(self.settings.build_type))
		copy(self, "*.lib", lib_cfg_src, lib_dst)
		copy(self, "*.pdb", lib_cfg_src, lib_dst)

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "spirv_reflect")
		self.cpp_info.set_property("cmake_target_name", "spirv_reflect::spirv_reflect")
		self.cpp_info.set_property("pkg_config_name", "spirv_reflect")
		self.cpp_info.libs = [ "spirv-reflect-static" ]
