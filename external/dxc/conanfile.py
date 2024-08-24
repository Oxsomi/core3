from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename
import os

required_conan_version = ">=2.0"

class dxc(ConanFile):

	name = "dxc"
	version = "2024.08.24"

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
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "DirectXShaderCompiler")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])
		git.run("submodule update --init --recursive")

	def build(self):

		cmake = CMake(self)

		print(os.getcwd())
		
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

		# WSL is only needed for linux
		if cwd.endswith("Debug") or cwd.endswith("Release"):

			wsl_winadapter_src = os.path.join(self.source_folder, "DirectXShaderCompiler/external/DirectX-Headers/include/wsl")
			wsl_winadapter_dst = os.path.join(self.package_folder, "")	# So that ../winadapter.h works (include/../windapter.h)
			copy(self, "winadapter.h", wsl_winadapter_src, wsl_winadapter_dst)

			wsl_stubs_src = os.path.join(self.source_folder, "DirectXShaderCompiler/external/DirectX-Headers/include/wsl/stubs")
			copy(self, "*.h", wsl_stubs_src, include_dir)

		lib_src = os.path.join(self.build_folder, "lib")
		lib_dst = os.path.join(self.package_folder, "lib")
		bin_src = os.path.join(self.build_folder, "bin")
		bin_dst = os.path.join(self.package_folder, "bin")
		
		# Linux, OSX, etc. all run from build/Debug or build/Release, so we need to change it a bit
		if cwd.endswith("Debug") or cwd.endswith("Release"):
			copy(self, "*.a", lib_src, lib_dst)
			copy(self, "^([^.]+)$", bin_src, bin_dst)
		
		# Windows uses more complicated setups
		else:
			
			lib_dbg_src = os.path.join(self.build_folder, "lib/Debug")
			dbg_lib_src = os.path.join(self.build_folder, "Debug/lib")
			bin_dbg_src = os.path.join(self.build_folder, "bin/Debug")
			dbg_bin_src = os.path.join(self.build_folder, "Debug/bin")

			copy(self, "*.lib", lib_dbg_src, lib_dst)
			copy(self, "*.lib", dbg_lib_src, lib_dst)
			copy(self, "*.pdb", lib_dbg_src, lib_dst)
			copy(self, "*.pdb", dbg_lib_src, lib_dst)
			copy(self, "*.exp", lib_dbg_src, lib_dst)
			copy(self, "*.exp", dbg_lib_src, lib_dst)

			copy(self, "*.exp", bin_dbg_src, bin_dst)
			copy(self, "*.exp", dbg_bin_src, bin_dst)
			copy(self, "*.exe", bin_dbg_src, bin_dst)
			copy(self, "*.exe", dbg_bin_src, bin_dst)
			copy(self, "*.dll", bin_dbg_src, bin_dst)
			copy(self, "*.dll", dbg_bin_src, bin_dst)
			copy(self, "*.pdb", bin_dbg_src, bin_dst)
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
			
			lib_rel_src = os.path.join(self.build_folder, "lib/Release")
			rel_lib_src = os.path.join(self.build_folder, "Release/lib")
			bin_rel_src = os.path.join(self.build_folder, "bin/Release")
			rel_bin_src = os.path.join(self.build_folder, "Release/bin")

			copy(self, "*.lib", lib_rel_src, lib_dst)
			copy(self, "*.lib", rel_lib_src, lib_dst)
			copy(self, "*.pdb", lib_rel_src, lib_dst)
			copy(self, "*.pdb", rel_lib_src, lib_dst)
			copy(self, "*.exp", lib_rel_src, lib_dst)
			copy(self, "*.exp", rel_lib_src, lib_dst)
			
			# Copy executables if applicable

			copy(self, "*.exp", bin_rel_src, bin_dst)
			copy(self, "*.exe", bin_rel_src, bin_dst)
			copy(self, "*.dll", bin_rel_src, bin_dst)
			copy(self, "*.pdb", bin_rel_src, bin_dst)

			copy(self, "*.exp", rel_bin_src, bin_dst)
			copy(self, "*.exe", rel_bin_src, bin_dst)
			copy(self, "*.dll", rel_bin_src, bin_dst)
			copy(self, "*.pdb", rel_bin_src, bin_dst)

	def package_info(self):

		self.cpp_info.set_property("cmake_file_name", "dxc")
		self.cpp_info.set_property("cmake_target_name", "dxc::dxc")
		self.cpp_info.set_property("pkg_config_name", "dxc")
		
		self.cpp_info.libs = [ "dxcompiler", "dxcvalidator", "LLVMDxilHash", "LLVMDxilValidation" ]
		
		if self.settings.compiler == "msvc":
			self.cpp_info.libs += [ "libclang" ]
		else:
			self.cpp_info.libs += [ "clang" ]
		
		# Important note. This is built by using the cmake dependency graph
		# We have to order from most dependent to least dependent
		# Since apparently only MSVC is ok with linking in an "invalid" order
		
		self.cpp_info.libs += [
			
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
