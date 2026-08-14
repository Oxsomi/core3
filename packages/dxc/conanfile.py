from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.build import cross_building
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy, rename, replace_in_file
import os
import platform
import subprocess

required_conan_version = ">=2.0"

class dxc(ConanFile):

	name = "dxc"
	version = "2026.08.07.03"

	# Optional metadata
	license = "LLVM Release License"
	author = "Microsoft (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/DirectXShaderCompiler"
	description = "Fork of Microsoft's DXC that contains static linking, conan support and support for other platforms"
	topics = ("hlsl", "dxil", "shader-programs", "directx-shader-compiler")

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"
	options = { "enableASAN": [ True, False ], "enableUBSAN": [ True, False ] }
	default_options = { "enableASAN": False, "enableUBSAN": False }

	# A sanitized consumer can't link unsanitized dependencies, so the flags have to reach these too.
	# MSVC's STL records its ASan container annotation state per object and lld-link rejects the mix,
	# hence disabling the annotations rather than instrumenting the STL.

	def _sanitizerFlags(self):

		flags = []

		# Windows means an MSVC ABI driver (msvc or clang-cl), which spells these with a slash and rejects
		# the driver forms; a plain clang/gcc rejects the slash forms just as hard ("no such file or
		# directory: '/fsanitize=address'"), so the spelling has to follow the platform.
		# The _DISABLE_*_ANNOTATION defines are MSVC STL specific: it records its ASan container annotation
		# state per object and lld-link refuses to mix instrumented and uninstrumented ones.

		msvcStyle = self.settings.os == "Windows"

		if self.options.enableASAN:
			if msvcStyle:
				flags += [ "/fsanitize=address", "/Oy-", "-D_DISABLE_STRING_ANNOTATION=1", "-D_DISABLE_VECTOR_ANNOTATION=1" ]
			else:
				flags += [ "-fsanitize=address", "-fno-omit-frame-pointer" ]

		# vptr needs RTTI across the whole program, which the prebuilt setup lacks. enum is off because DXC's
		# own reflection uses 0xFFFFFFFF as an "unknown" _D3D_SHADER_VARIABLE_TYPE sentinel, which is a valid
		# use of an out-of-range enum value that -fsanitize=enum flags; that is DXC's design, not our bug, and
		# with halt_on_error it would abort the build. The rest of UBSan stays on so it can still catch real
		# undefined behaviour DXC hits while processing our shaders.
		if self.options.enableUBSAN:
			flags += [ "-fsanitize=undefined", "-fno-sanitize=vptr,enum" ]
			flags += [ "/Oy-" ] if msvcStyle else [ "-fno-omit-frame-pointer" ]

		return flags

	# The compile side flags alone aren't enough: CMake drives lld-link directly, so the /defaultlib
	# directives clang-cl embeds for the sanitizer runtimes never reach it.
	# Point the linker at clang's own runtime directory and name the libraries here too.

	def _sanitizerLinkFlags(self):

		if not (self.options.enableASAN or self.options.enableUBSAN):
			return []

		if self.settings.os != "Windows":
			return []

		import glob as _glob
		import shutil as _shutil

		executables = self.conf.get("tools.build:compiler_executables", default={}, check_type=dict)
		cc = executables.get("c") or "clang-cl"
		resolved = cc if os.path.isabs(cc) else (_shutil.which(cc) or "")

		if not resolved:
			return []

		binDir = os.path.dirname(resolved)
		found = _glob.glob(os.path.join(binDir, "..", "lib", "clang", "*", "lib", "windows"))

		if not found:
			return []

		# Forward slashes: this ends up inside a quoted string in conan_toolchain.cmake, where CMake reads
		# a backslash as an escape and dies on paths like C:\Program Files ("Invalid character escape '\P'").

		flags = [ "-libpath:\\\"%s\\\"" % os.path.normpath(found[0]).replace("\\", "/") ]

		# compiler-rt names these after the target arch, so it can't be pinned to x86_64 or an arm64 build
		# silently links the wrong runtime (or none).

		arch = "aarch64" if str(self.settings.arch) in ("armv8", "arm64", "aarch64") else "x86_64"

		if self.options.enableASAN:
			flags += [ "clang_rt.asan_dynamic-%s.lib" % arch, "clang_rt.asan_dynamic_runtime_thunk-%s.lib" % arch ]

		if self.options.enableUBSAN:
			flags += [ "clang_rt.ubsan_standalone-%s.lib" % arch ]

		return flags


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
		# On for Debug and for any sanitized build; off for the Release that ships.
		# In a shipping Release an assert firing just aborts the app rather than helping anyone, and it also
		# drops the android test .so from 547 to 490 MB (~11%, not the bulk: statically linked
		# LLVM/clang/SPIRV-Tools plus an unstripped symbol table is what actually makes it that size).
		# A sanitizer run is the opposite situation: we WANT DXC's own asserts firing next to ASan/UBSan so a
		# bug in how it processes our shaders is caught at its source. Tying it to the sanitizer flags rather
		# than to build_type=Debug gets those asserts on an optimized DXC, avoiding the multi-GB Debug
		# static-LLVM build that would risk the runner's disk.

		tc.variables["LLVM_ENABLE_ASSERTIONS"] = (
			self.settings.build_type == "Debug" or self.options.enableASAN or self.options.enableUBSAN
		)
		tc.variables["ENABLE_DXC_STATIC_LINKING"] = True

		tc.variables["LLVM_LIT_ARGS"] = "-v"
		tc.variables["LLVM_TARGETS_TO_BUILD"] = "None"
		tc.variables["LLVM_DEFAULT_TARGET_TRIPLE"] = "dxil-ms-dx"

		if self.settings.os == "Android":
			if self.settings.arch == "x86_64":
				tc.variables["LLVM_INFERRED_HOST_TRIPLE"] = "x86_64-linux-android"
			else:
				tc.variables["LLVM_INFERRED_HOST_TRIPLE"] = "aarch64-linux-android"

		# TableGen runs during the build, so a cross build needs one built for the *build* machine.
		# LLVM's answer is the NATIVE sub build, but it configures itself with default options
		# (LLVM_ENABLE_EH=OFF) and then can't compile this fork's LLVMSupport, and it inherits
		# CMAKE_GENERATOR from us, which for a cross build is a generator pointed at a compiler that
		# isn't the build machine's.
		# Upstream 37ccb7a9 made -DLLVM_USE_HOST_TOOLS=OFF stick for exactly this case, so hand it the
		# ones a host build of this same recipe already produced and packaged.
		#
		# Keyed on the conf rather than on a target, because nothing here is specific to one: android
		# and wasm need it for the same reason, and so would any future cross target.
		# Say nothing without it and the old NATIVE path runs, which is the previous behaviour rather
		# than a new failure.

		tablegenDir = self.conf.get("user.dxc:tablegen_dir", check_type=str)

		if tablegenDir:

			# The build machine runs these, so the suffix follows it rather than settings.os.

			exe = ".exe" if platform.system() == "Windows" else ""

			tc.variables["LLVM_USE_HOST_TOOLS"] = False
			tc.variables["LLVM_TABLEGEN"] = os.path.join(tablegenDir, "llvm-tblgen" + exe).replace("\\", "/")
			tc.variables["CLANG_TABLEGEN"] = os.path.join(tablegenDir, "clang-tblgen" + exe).replace("\\", "/")

		elif cross_building(self):

			# Any target that isn't the build machine lands here: android, ios, wasm, or a plain x64 -> arm64 build.
			# The NATIVE fallback is going to fail, and it fails deep inside a sub configure where the reason
			# isn't visible, so name it here instead.

			self.output.warning(
				"Cross building without user.dxc:tablegen_dir, so LLVM's NATIVE sub build has to "
				"produce TableGen. That configures itself with LLVM_ENABLE_EH=OFF and can't compile "
				"this fork. Pass the bin/ of a host build of this recipe instead "
				"(build_common.hostTablegenDir gives you the path)."
			)

		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)

		tc.variables["CMAKE_MSVC_RUNTIME_LIBRARY"] = "MultiThreaded"

		# GCC 14.1 and 14.2 miscompile the component type remap in CGMSHLSLRuntime::SetUAVSRV at -O2/-O3;
		# they emit the bt/cmov select with inverted polarity, so every typed SRV/UAV ends up with ComponentType U32.
		# Texture2D<float4> becomes Texture2D<4xU32>, which makes every sample_* fail DXIL validation below SM 6.7
		# ("sample_* instructions require resource to be declared to return UNORM, SNORM or FLOAT")
		# and makes reflection report uint for float textures.
		# -fno-if-conversion (or -fno-tree-vrp) stops GCC forming that select.
		# Fixed in GCC 14.3; 12.x, 13.x and 15+ are unaffected, as are -O0/-O1/-Os and clang
		# (which is why Microsoft's own clang-built DXC binaries are fine).
		# This is not the GCC 13 -funswitch-loops miscompile (PR109934 / DXC #7364);
		# that one is already handled inside DXC's own HandleLLVMOptions.cmake, and its guard stops at < 14.0.

		if self.settings.compiler == "gcc" and self._real_compiler_version() in [ (14, 1), (14, 2) ]:
			tc.extra_cflags.append("-fno-if-conversion")
			tc.extra_cxxflags.append("-fno-if-conversion")

		# DXC builds its own sources and its vendored SPIRV-Tools with -Werror,
		# and those have only ever been compiled with MSVC on Windows.
		# clang-cl warns where MSVC doesn't, so the build dies on third party code we don't own:
		# -Wmissing-field-initializers in SPIRV-Tools' binary.cpp/text.cpp and -Wunused-private-field on a padding member.
		# Demote just those two rather than dropping -Werror,
		# so a real diagnostic in the rest of the tree still stops the build.

		if self.settings.compiler == "clang" and self.settings.os == "Windows":
			for flag in ("-Wno-missing-field-initializers", "-Wno-unused-private-field"):
				tc.extra_cflags.append(flag)
				tc.extra_cxxflags.append(flag)

		for flag in self._sanitizerLinkFlags():
			tc.extra_exelinkflags.append(flag)
			tc.extra_sharedlinkflags.append(flag)

		for flag in self._sanitizerFlags():
			tc.extra_cflags.append(flag)
			tc.extra_cxxflags.append(flag)

		# ASan intercepts operator new, and DXC replaces it (dxcmem.cpp), which lld-link rejects outright.
		# Upstream added this option for exactly that case.

		if self.options.enableASAN:
			tc.variables["DXC_DISABLE_ALLOCATOR_OVERRIDES"] = True

		tc.generate()

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "DirectXShaderCompiler")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])
		git.run("submodule update --init --recursive")

	def _relaxAssertOnlyWarnings(self):
		"""Let dxcreflection compile LLVM headers with assertions off.

		A lot of LLVM only reads a variable inside an assert, so NDEBUG turns those into
		-Wunused-parameter / -Wunused-but-set-variable (WinAdapter.h's ScopedLocale CodePage, DenseMap.h's
		NumEntries, and more behind them). dxcreflection and dxcreflectioncontainer compile those headers
		with -Wall -Wextra -Werror, which only ever held up because assertions were on.

		Suppressed on the targets rather than through the toolchain: their options are PRIVATE, so they
		land after anything CMakeToolchain sets and the later -Wextra would just re-enable it. Upstream
		already keeps a -Wno-unused-template here for the same reason, so this rides along with it.
		"""

		extra = "-Wno-unused-template -Wno-unused-parameter -Wno-unused-but-set-variable"

		for tool in ("dxcreflection", "dxcreflectioncontainer"):

			path = os.path.join(
				self.source_folder, "DirectXShaderCompiler/tools/clang/tools", tool, "CMakeLists.txt"
			)

			if not os.path.isfile(path):
				continue

			replace_in_file(
				self, path,
				f"target_compile_options({tool} PRIVATE -Wno-unused-template)",
				f"target_compile_options({tool} PRIVATE {extra})",
				strict=False
			)

	def build(self):

		self._relaxAssertOnlyWarnings()

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

		# Host builds carry the tablegens so a cross build of this same recipe can borrow them rather
		# than running LLVM's NATIVE sub build; see the LLVM_USE_HOST_TOOLS note in generate().
		# They're build machine executables, so they're only useful out of a package built for the
		# machine doing the building.

		bin_dst = os.path.join(self.package_folder, "bin")

		for config in ("", "Debug", "Release", "MinSizeRel", "RelWithDebInfo"):

			tablegen_src = os.path.join(self.build_folder, config, "bin") if config else os.path.join(self.build_folder, "bin")

			for name in ("llvm-tblgen", "clang-tblgen"):
				copy(self, name, tablegen_src, bin_dst)
				copy(self, name + ".exe", tablegen_src, bin_dst)

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

				# Only the configs that exist to be debugged keep their pdbs.
				# They are 310 of this package's 1085 MB, and with 16 cache groups on a 10 GB repo cap that
				# difference is the margin between caches surviving and being evicted between pushes.
				# Release and MinSizeRel are dropped because nothing steps through an optimized DXC with no
				# debug info asked for; RelWithDebInfo keeps them, since carrying debug info is its entire point.
				# A missing pdb costs LNK4099, a warning, since /WX is compile-time only (see CMakeLists.txt).

				if config in ("Debug", "RelWithDebInfo"):
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

		# Important note.
		# This is built by using the cmake dependency graph
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
