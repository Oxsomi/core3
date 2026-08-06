from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename
import os
import subprocess

required_conan_version = ">=2.0"

class dxc(ConanFile):

	name = "dxc"
	version = "2026.08.06"

	# Optional metadata
	license = "LLVM Release License"
	author = "Microsoft (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/DirectXShaderCompiler"
	description = "Fork of Microsoft's DXC that contains static linking, conan support and support for other platforms"
	topics = ("hlsl", "dxil", "shader-programs", "directx-shader-compiler")

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"

	exports_sources = [ "include/dxc/*", "external/SPIRV-Tools/include/*", "external/DirectX-Headers/include/*" ]

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	# (major, minor) of the C++ compiler cmake will actually invoke, or None if it can't be probed.
	# settings.compiler.version isn't usable for this; it comes from the conan profile and is
	# regularly stale (the profile that produced the miscompiled dxc packages said gcc 11 while
	# the cc it ran was 14.2), so a version check against it would test the wrong compiler.

	def _real_compiler_version(self):

		executables = self.conf.get("tools.build:compiler_executables", default={}, check_type=dict)
		cxx = executables.get("cpp") or executables.get("cxx") or os.environ.get("CXX") or "c++"

		try:
			probe = subprocess.run(
				[cxx, "-dumpfullversion", "-dumpversion"], capture_output=True, text=True, timeout=10
			)
		except Exception:
			return None

		if probe.returncode:
			return None

		version = probe.stdout.strip().split(".")

		try:
			return (int(version[0]), int(version[1]) if len(version) > 1 else 0)
		except ValueError:
			return None

	def generate(self):

		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)

		tc.cppstd = "20"
		tc.variables["CLANG_BUILD_EXAMPLES"] = False
		tc.variables["CLANG_CL"] = False
		tc.variables["CLANG_INCLUDE_TESTS"] = False
		tc.variables["LLVM_OPTIMIZED_TABLEGEN"] = False
		tc.variables["LLVM_INCLUDE_DOCS"] = False
		tc.variables["LLVM_INCLUDE_EXAMPLES"] = False
		tc.variables["HLSL_INCLUDE_TESTS"] = False
		tc.variables["CLANG_ENABLE_STATIC_ANALYZER"] = False
		tc.variables["CLANG_ENABLE_ARCMT"] = False
		tc.variables["SPIRV_BUILD_TESTS"] = False
		tc.variables["HLSL_BUILD_DXILCONV"] = False
		tc.variables["HLSL_ENABLE_FIXED_VER"] = False
		tc.variables["HLSL_OFFICIAL_BUILD"] = False
		tc.variables["HLSL_OPTIONAL_PROJS_IN_DEFAULT"] = False
		tc.variables["LLVM_ENABLE_TERMINFO"] = False
		tc.variables["BUILD_SHARED_LIBS"] = False
		tc.variables["LLVM_INCLUDE_TESTS"] = False

		tc.variables["LLVM_ENABLE_RTTI"] = True
		tc.variables["LLVM_ENABLE_EH"] = True
		tc.variables["LLVM_APPEND_VC_REV"] = True
		tc.variables["LIBCLANG_BUILD_STATIC"] = True
		tc.variables["ENABLE_SPIRV_CODEGEN"] = True
		tc.variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True
		tc.variables["DXC_USE_LIT"] = True
		tc.variables["LLVM_ENABLE_ASSERTIONS"] = True
		tc.variables["ENABLE_DXC_STATIC_LINKING"] = True

		tc.variables["LLVM_LIT_ARGS"] = "-v"
		tc.variables["LLVM_TARGETS_TO_BUILD"] = "None"
		tc.variables["LLVM_DEFAULT_TARGET_TRIPLE"] = "dxil-ms-dx"

		if self.settings.os == "Android":
			if self.settings.arch == "x86_64":
				tc.variables["LLVM_INFERRED_HOST_TRIPLE"] = "x86_64-linux-android"
			else:
				tc.variables["LLVM_INFERRED_HOST_TRIPLE"] = "aarch64-linux-android"

		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)

		tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded"

		# GCC 14.1 and 14.2 miscompile the component type remap in CGMSHLSLRuntime::SetUAVSRV at
		# -O2/-O3; they emit the bt/cmov select with inverted polarity, so every typed SRV/UAV ends
		# up with ComponentType U32. Texture2D<float4> becomes Texture2D<4xU32>, which makes every
		# sample_* fail DXIL validation below SM 6.7 ("sample_* instructions require resource to be
		# declared to return UNORM, SNORM or FLOAT") and makes reflection report uint for float
		# textures. -fno-if-conversion (or -fno-tree-vrp) stops GCC forming that select.
		# Fixed in GCC 14.3; 12.x, 13.x and 15+ are unaffected, as are -O0/-O1/-Os and clang
		# (which is why Microsoft's own clang-built DXC binaries are fine).
		# This is not the GCC 13 -funswitch-loops miscompile (PR109934 / DXC #7364); that one is
		# already handled inside DXC's own HandleLLVMOptions.cmake, and its guard stops at < 14.0.

		if self.settings.compiler == "gcc" and self._real_compiler_version() in [ (14, 1), (14, 2) ]:
			tc.extra_cflags.append("-fno-if-conversion")
			tc.extra_cxxflags.append("-fno-if-conversion")

		tc.generate()

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "DirectXShaderCompiler")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])
		git.run("submodule update --init --recursive")

	def build(self):

		cmake = CMake(self)

		if os.path.isdir("../DirectXShaderCompiler") or os.path.isdir("../../DirectXShaderCompiler"):
			cmake.configure(build_script_folder="DirectXShaderCompiler")
		else:
			cmake.configure()

		cmake.build()

	def package(self):

		cmake = CMake(self)
		cmake.build(target="dxcompiler")

		# First do a copy for all debug libs
		# So that we can rename them all at once to suffixed by D
		# Apparently it looks for dxcompilerD.lib/.a rather than dxcompiler.lib/.a

		cwd = os.getcwd()

		# Headers: We use include/dxcompiler to avoid vulkan sdk interfering

		dxc_src = os.path.join(self.source_folder, "DirectXShaderCompiler/include/dxc")
		dxc_dst = os.path.join(self.package_folder, "include/dxcompiler")
		copy(self, "*.h", dxc_src, dxc_dst)
		copy(self, "*.hpp", dxc_src, dxc_dst)

		spirv_tools_src = os.path.join(self.source_folder, "DirectXShaderCompiler/external/SPIRV-Tools/include/spirv-tools")
		spirv_tools_dst = os.path.join(self.package_folder, "include/spirv-tools")
		copy(self, "*.h", spirv_tools_src, spirv_tools_dst)
		copy(self, "*.hpp", spirv_tools_src, spirv_tools_dst)

		dx_headers_src = os.path.join(self.source_folder, "DirectXShaderCompiler/external/DirectX-Headers/include")
		include_dir = os.path.join(self.package_folder, "include")
		copy(self, "*.h", dx_headers_src, include_dir)
		copy(self, "*.hpp", dx_headers_src, include_dir)

		# WSL is only needed for non windows
		if not self.settings.os == "Windows":

			wsl_winadapter_src = os.path.join(self.source_folder, "DirectXShaderCompiler/external/DirectX-Headers/include/wsl")
			wsl_winadapter_dst = os.path.join(self.package_folder, "")	# So that ../winadapter.h works (include/../windapter.h)
			copy(self, "winadapter.h", wsl_winadapter_src, wsl_winadapter_dst)

			wsl_stubs_src = os.path.join(self.source_folder, "DirectXShaderCompiler/external/DirectX-Headers/include/wsl/stubs")
			copy(self, "*.h", wsl_stubs_src, include_dir)

		lib_src = os.path.join(self.build_folder, "lib")
		lib_dst = os.path.join(self.package_folder, "lib")

		# Linux, OSX, etc. all run from build/Debug or build/Release, so we need to change it a bit
		if not self.settings.os == "Windows":
			copy(self, "*.a", lib_src, lib_dst)

		# Windows uses more complicated setups
		else:

			# Copy libs

			for config in ["Debug", "Release", "MinSizeRel", "RelWithDebInfo"]:
				lib_cfg_src = os.path.join(self.build_folder, f"lib/{config}")
				alt_lib_cfg_src = os.path.join(self.build_folder, f"{config}/lib")
				copy(self, "*.lib", lib_cfg_src, lib_dst)
				copy(self, "*.lib", alt_lib_cfg_src, lib_dst)
				copy(self, "*.pdb", lib_cfg_src, lib_dst)
				copy(self, "*.pdb", alt_lib_cfg_src, lib_dst)
				copy(self, "*.exp", lib_cfg_src, lib_dst)
				copy(self, "*.exp", alt_lib_cfg_src, lib_dst)

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "dxc")
		self.cpp_info.set_property("cmake_target_name", "dxc::dxc")
		self.cpp_info.set_property("pkg_config_name", "dxc")

		self.cpp_info.libs = [ "dxcompiler", "dxcvalidator", "LLVMDxilHash", "LLVMDxilValidation" ]

		if self.settings.compiler == "msvc":
			self.cpp_info.libs += [ "libclang" ]
		else:
			self.cpp_info.libs += [ "clang" ]

		self.cpp_info.includedirs = [ "include", "include/spirv-tools" ]

		# Important note. This is built by using the cmake dependency graph
		# We have to order from most dependent to least dependent
		# Since apparently only MSVC is ok with linking in an "invalid" order

		self.cpp_info.libs += [

			"dxcreflection",
			"dxcreflectioncontainer",

			"LLVMDxrFallback",

			"clangFrontendTool",
			"clangCodeGen",
			"LLVMTarget",

			"LLVMScalarOpts",
			"LLVMPassPrinters",

			"LLVMProfileData",
			"LLVMDxilCompression",
			"LLVMOption",
			"LLVMDxilDia",
			"LLVMDxilPdbInfo",
			"LLVMPasses",
			"LLVMDxilRootSignature",
			"LLVMInstCombine",
			"LLVMDxilPIXPasses",
			"LLVMVectorize",

			"clangRewriteFrontend",
			"clangTooling",
			"clangSPIRV",

			"SPIRV-Tools-opt",
			"SPIRV-Tools",

			"clangFrontend",
			"clangDriver",
			"clangParse",
			"clangASTMatchers",
			"clangIndex",

			"clangFormat",
			"clangToolingCore",
			"clangRewrite",

			"clangSema",
			"clangAST",
			"clangEdit",
			"clangLex",
			"clangBasic",

			"LLVMLinker",
			"LLVMDxilContainer",
			"LLVMTransformUtils",
			"LLVMipa",
			"LLVMAnalysis",
			"LLVMDxcBindingTable",
			"LLVMDXIL",
			"LLVMIRReader",
			"LLVMBitWriter",
			"LLVMBitReader",
			"LLVMAsmParser",
			"LLVMTableGen",
			"LLVMDxcSupport",
			"LLVMCore",
			"LLVMSupport",
			"LLVMMSSupport"
		]
