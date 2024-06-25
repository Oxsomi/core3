from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename
import os

required_conan_version = ">=2.0"

class dxc(ConanFile):

	name = "dxc"
	version = "2024.06.25"

	# Optional metadata
	license = "LLVM Release License"
	author = "Microsoft (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/DirectXShaderCompiler"
	description = "Fork of Microsoft's DXC that contains static linking, conan support and support for other platforms"
	topics = ("hlsl", "dxil", "shader-programs", "directx-shader-compiler")

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"

	exports_sources = "include/dxc/*"

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

		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)

		tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded"

		tc.generate()

	def source(self):
		git = Git(self)
		print(self.conan_data)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "DirectXShaderCompiler")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])
		git.run("submodule update --init --recursive")

	def build(self):
		cmake = CMake(self)
		cmake.configure(build_script_folder="DirectXShaderCompiler")
		cmake.build()

	def package(self):

		cmake = CMake(self)
		cmake.build(target="dxcompiler")

		# First do a copy for all debug libs
		# So that we can rename them all at once to suffixed by D
		# Apparently it looks for dxcompilerD.lib/.a rather than dxcompiler.lib/.a

		copy(self, "*.lib", "lib/Debug", "../../p/lib")
		copy(self, "*.a", "lib/Debug", "../../p/lib")
		copy(self, "*.lib", "Debug/lib", "../../p/lib")
		copy(self, "*.a", "Debug/lib", "../../p/lib")
		
		directory = "../../p/lib"

		if os.path.isfile(directory):
			for filename in os.listdir(directory):
				f = os.path.join(directory, filename)
				if os.path.isfile(f):
					offset = f.rfind(".")
					rename(self, f, f[:offset] + "d." + f[offset+1:])

		# Copy release libs

		copy(self, "*.lib", "lib/Release", "../../p/lib")
		copy(self, "*.a", "lib/Release", "../../p/lib")
		copy(self, "*.lib", "Release/lib", "../../p/lib")
		copy(self, "*.a", "Release/lib", "../../p/lib")

		# Headers

		copy(self, "*.h", "../DirectXShaderCompiler/include/dxc", "../../p/include/dxc")
		copy(self, "*.hpp", "../DirectXShaderCompiler/include/dxc", "../../p/include/dxc")

	def package_info(self):
		self.cpp_info.set_property("cmake_file_name", "dxc")
		self.cpp_info.set_property("cmake_target_name", "dxc::dxc")
		self.cpp_info.set_property("pkg_config_name", "dxc")
		self.cpp_info.libs = [
			"dxcompiler",
			"clangAnalysis",
			"clangAST",
			"clangASTMatchers",
			"clangBasic",
			"clangCodeGen",
			"clangDriver",
			"clangEdit",
			"clangFormat",
			"clangFrontend",
			"clangFrontendTool",
			"clangIndex",
			"clangLex",
			"clangParse",
			"clangRewrite",
			"clangRewriteFrontend",
			"clangSema",
			"clangSPIRV",
			"clangTooling",
			"clangToolingCore",
			"libclang",
			"LLVMAnalysis",
			"LLVMAsmParser",
			"LLVMBitReader",
			"LLVMBitWriter",
			"LLVMCore",
			"LLVMDxcBindingTable",
			"LLVMDxcSupport",
			"LLVMDXIL",
			"LLVMDxilCompression",
			"LLVMDxilContainer",
			"LLVMDxilDia",
			"LLVMDxilPdbInfo",
			"LLVMDxilPIXPasses",
			"LLVMDxilRootSignature",
			"LLVMDxrFallback",
			"LLVMInstCombine",
			"LLVMipa",
			"LLVMIRReader",
			"LLVMLinker",
			"LLVMMSSupport",
			"LLVMOption",
			"LLVMPasses",
			"LLVMPassPrinters",
			"LLVMProfileData",
			"LLVMScalarOpts",
			"LLVMSupport",
			"LLVMTableGen",
			"LLVMTarget",
			"LLVMTransformUtils",
			"LLVMVectorize",
			"SPIRV-Tools",
			"SPIRV-Tools-opt"
		]