from conan import ConanFile
from conan.tools.build import cross_building
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy
import os
import shutil

required_conan_version = ">=2.0"

# Options oxc3 is built with when it's consumed as a host *tool* rather than as a library; this is the
# OxC3_package packager that add_virtual_files() shells out to (see cmake/oxc3.cmake).
#
# Deliberately constant instead of forwarded from self.options: it's a host binary that only turns files
# into oiCA archives, so nothing about the target (SIMD level, graphics backend, ...) should change it.
# Keeping it fixed also means a single prebuilt package serves every android configuration.
#
# build_common.py reads this dict straight out of this file so build_android.py can prebuild exactly the
# binary tool_requires below is going to ask for.

HOST_TOOL_OPTIONS = {
	"forceVulkan": False,
	"enableSIMD": True,
	"enableTests": False,
	"enableOxC3CLI": True,
	"forceFloatFallback": False,
	"enableShaderCompiler": True,
	"cliGraphics": False,
	"dynamicLinkingGraphics": True,

	# Static on purpose, unlike the default.
	# This one is invoked by CMake through find_program during other people's builds,
	# so a single self-contained exe beats an exe plus a DLL to locate.

	"dynamicLinkingShaderCompiler": False
}

class oxc3(ConanFile):

	name = "oxc3"
	version = "3.2.105"

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
		"testAutoRun": [ True, False ],
		"enableOxC3CLI": [ True, False ],
		"forceFloatFallback": [ True, False ],
		"enableShaderCompiler": [ True, False ],
		"cliGraphics": [ True, False ],
		"dynamicLinkingGraphics": [ True, False ],
		"dynamicLinkingShaderCompiler": [ True, False ],
		"debugShaderCompiler": [ True, False ],
		"enableHostCrypto": [ True, False ],
		"enableASAN": [ True, False ],
		"enableUBSAN": [ True, False ]
	}

	default_options = {
		"forceVulkan": False,
		"enableSIMD": True,
		"enableTests": False,
		"testAutoRun": True,
		"enableOxC3CLI": True,
		"forceFloatFallback": False,
		"enableShaderCompiler": True,
		"cliGraphics": True,

		# DXC and SPIRV-Reflect are big, slow to build and almost never what you're actually debugging,
		# so they stay Release even in a Debug build.
		# Turn this on to build and consume them in the current mode instead,
		# which is what you want when stepping into the shader compiler itself.

		"dynamicLinkingGraphics": False,

		# Separate from dynamicLinkingGraphics: that one exists so Vulkan/D3D12 can be picked at runtime,
		# this one so DXC's ~28 MB lives in one shared module rather than in every consumer.
		# On by default on desktop; CMakeLists coerces it off where it can't apply (android, or no shader compiler at all).

		"dynamicLinkingShaderCompiler": True,
		"debugShaderCompiler": False,

		# Web only: routes SHA-256/AES-GCM to the host's crypto (needs COOP/COEP + a Worker).
		# Off here, and build_web.py turns it on for the threaded flavor, which needs isolation anyway.

		"enableHostCrypto": False,

		# Diagnostic only, and clang/gcc only.
		# CMakeLists turns a request under MSVC into a hard error rather than ignoring it.

		"enableASAN": False,
		"enableUBSAN": False
	}

	exports_sources = [ "include/*", "cmake/*" ]

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def generate(self):

		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)

		# The repo ships its own CMakePresets.json, so conan's root CMakeUserPresets.json is not generated.
		# It is an index of every build folder ever configured, and it broke `cmake --list-presets` outright in
		# two ways: every folder called its preset conan-default, which collides, and a folder that is later
		# removed leaves an include pointing at nothing.
		# conan build is unaffected, since that uses the presets inside the build folder rather than this index.

		tc.user_presets_path = False

		# Those per folder presets still get a name of their own, so including one by hand stays unambiguous.

		tc.presets_prefix = "conan-" + "-".join(str(x) for x in [
			self.settings.os, self.settings.arch, self.settings.compiler, self.settings.build_type
		]).lower().replace(" ", "-").replace("_", "-")

		tc.cache_variables["ForceFloatFallback"] = self.options.forceFloatFallback
		tc.cache_variables["EnableTests"] = self.options.enableTests

		# Off when the caller drives ctest itself, otherwise every suite would run twice: once at link time and
		# again in the ctest pass.

		tc.cache_variables["OxC3TestAutoRun"] = self.options.testAutoRun
		tc.cache_variables["EnableOxC3CLI"] = self.options.enableOxC3CLI
		tc.cache_variables["EnableSIMD"] = self.options.enableSIMD
		tc.cache_variables["ForceVulkan"] = self.options.forceVulkan
		tc.cache_variables["DynamicLinkingShaderCompiler"] = self.options.dynamicLinkingShaderCompiler
		tc.cache_variables["DebugShaderCompiler"] = self.options.debugShaderCompiler
		tc.cache_variables["EnableShaderCompiler"] = self.options.enableShaderCompiler
		tc.cache_variables["CLIGraphics"] = self.options.cliGraphics
		tc.cache_variables["DynamicLinkingGraphics"] = self.options.dynamicLinkingGraphics
		tc.cache_variables["EnableHostCrypto"] = self.options.enableHostCrypto
		tc.cache_variables["EnableASAN"] = self.options.enableASAN
		tc.cache_variables["EnableUBSAN"] = self.options.enableUBSAN

		if not self.settings.os == "Windows":
			tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)

		tc.generate()

	def source(self):
		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"])
		git.folder = os.path.join(self.source_folder, "core3")
		git.checkout(self.conan_data["sources"][self.version]["checkout"])
		git.run("submodule update --init --recursive")

	# Where CMakeLists.txt lives. `conan build`/`export-pkg` run against a working tree so that's the recipe
	# folder itself, while `conan create` calls source() first which clones the repo into ./core3.

	def _cmakeRoot(self):

		cloned = os.path.join(self.source_folder, "core3")

		if os.path.isfile(os.path.join(cloned, "CMakeLists.txt")):
			return cloned

		return self.source_folder

	def build(self):

		cmake = CMake(self)

		if self._cmakeRoot() != self.source_folder:
			cmake.configure(build_script_folder="core3")
		else:
			cmake.configure()

		cmake.build()

	# In case we don't have the shader compiler (and OxC3 package) enabled, we will have to depend on a previously built
	# OxC3 with shader compiler enabled.
	# This happens for example with Android, where shader compilation is disabled by default.
	#
	# It's a tool_requires, so conan resolves it against the *build* profile and puts its bin/ on PATH;
	# that's how add_virtual_files()'s find_program(OxC3_package) resolves while cross compiling.
	# build_android.py prebuilds this from the working tree, otherwise conan falls back to building oxc3
	# from github (see source()), which is almost never what you want locally.

	def build_requirements(self):

		# Cross building needs it regardless of enableShaderCompiler: what matters is being able to *run* the packager,
		# and a cross build's binaries target the device.
		# Android can't even produce the executable (Platform_defineEntrypoint gives android_main, not main),
		# and the wasm build's own OxC3_package is a .js the build machine can't exec either,
		# so without this add_virtual_files' find_program picks up whatever OxC3_package happens to be on PATH,
		# which is how a months-old one out of the conan cache ended up packaging the shader tests.

		if not self.options.enableShaderCompiler or cross_building(self):
			self.tool_requires(f"{self.name}/{self.version}", options = HOST_TOOL_OPTIONS)

	def requirements(self):

		hasD3D12 = not self.options.forceVulkan and self.settings.os == "Windows"

		if self.options.dynamicLinkingGraphics and self.settings.os == "Windows":
			hasD3D12 = True

		# The three dependencies that compile C/C++ have to be built the same way we are: a sanitized
		# consumer can't link unsanitized dependencies (ASan has to own operator new, and MSVC's STL records
		# its container annotation state per object).
		# Propagating here rather than only on the command line that creates them is what makes the graph
		# ask for the packages that were actually built. Without it the root passes -o enableASAN=True while
		# dxc/spirv_reflect/openal_soft fall back to their own default of False, so the resolved package ids
		# are for unsanitized binaries that nothing ever built -- which is a "Missing binary" error at
		# install time, not a link failure later.

		sanitized = { "enableASAN": self.options.enableASAN, "enableUBSAN": self.options.enableUBSAN }

		# NVAPI is only needed for the D3D12 backend (RT validation, and future cluster / mega geometry).
		if hasD3D12:
			self.requires("nvapi/2026.08.02")

		# agility_sdk ships d3d12shader.h, which DXC's dxcreflect.h includes.
		if hasD3D12 or self.options.enableShaderCompiler:
			self.requires("agility_sdk/2026.07.29")

		if hasD3D12 and self.settings.arch == "x86_64":
			self.requires("ags/2024.09.21")

		if self.options.enableShaderCompiler:

			# Both shader-compiler deps are sanitized. DXC's build compiles host tablegen tools and RUNS them
			# mid-build; instrumented on Windows they abort for want of the sanitizer runtime, so build.py
			# feeds a Windows sanitized DXC unsanitized tablegen binaries via user.dxc:tablegen_dir (the same
			# split the android/web cross builds use). That conf is not part of the package id, so the graph
			# still just asks for a sanitized DXC here regardless of where its tablegen came from.
			self.requires("dxc/2026.08.23", options=sanitized)
			self.requires("spirv_reflect/2026.08.17", options=sanitized)

			# The AMD offline compilers RGA vendors (amdllpc, amdgpu-dis): offline SPIR-V to AMD ISA for the isa
			# commands. Only exists where AMD prebuilds them (Windows/Linux x64); run=True so the tools' bin dir
			# reaches the run environment (RGA_PATH/PATH) for tests and the OxC3 CLI.
			if self.settings.os in ("Windows", "Linux") and self.settings.arch == "x86_64":
				self.requires("radeon_gpu_analyzer/2026.09.02", run=True)

		if self.settings.os == "Linux":
			self.requires("xdg_shell/2024.10.21")
			self.requires("xdg_decoration/2024.12.22")

		# Vulkan headers come from the Oxsomi fork (headers-only), so building no longer needs a system VULKAN_SDK.
		# The loader is still loaded dynamically at runtime.
		# Required wherever the Vulkan backend is compiled.
		# No graphics module on the web target yet (no WebGPU backend), so no Vulkan headers either.
		usesVulkan = self.settings.os != "Windows" or self.options.forceVulkan or self.options.dynamicLinkingGraphics
		usesVulkan = usesVulkan and self.settings.os != "Emscripten"

		if usesVulkan:
			self.requires("vulkan_headers/2026.07.28")

		# Validation layers must be built manually for Android; not needed elsewhere.
		# Pinned near the vulkan_headers version, since an old layer silently weakens validation.
		if self.settings.os == "Android" and str(self.settings.build_type) == "Debug":
			self.requires("vulkan_validation_layers/1.4.357.0-oxc1")

		# emscripten ships its own OpenAL (WebAudio-backed); openal_soft has no web backend.
		if self.settings.os != "Emscripten":
			self.requires("openal_soft/2026.08.06", options=sanitized)

	def package(self):

		cmake = CMake(self)
		cmake.build()

		# Headers ship from the SAME tree the libraries were built from. source_folder holds the exported
		# working tree, which `conan create` leaves sitting beside the clone, so packaging from it shipped
		# whatever branch the local checkout happened to be on while the binaries came from the pinned
		# commit. A consumer then compiled against one revision's headers and linked another's objects.

		root = self._cmakeRoot()

		copy(self, "*.cmake", os.path.join(root, "cmake"), os.path.join(self.package_folder, "cmake"))

		inc_src = os.path.join(root, "include")
		inc_dst = os.path.join(self.package_folder, "include")
		copy(self, "*.h", inc_src, inc_dst)
		copy(self, "*.c", inc_src, inc_dst)
		copy(self, "*.hpp", inc_src, inc_dst)

		lib_dst = os.path.join(self.package_folder, "lib")
		bin_dst = os.path.join(self.package_folder, "bin")

		# The web target has one output directory per flavor, and CMakeLists.txt's EMSCRIPTEN block is
		# authoritative for its name: wasm64, then _mt when the compile flags carry -pthread, then _asan,
		# then _ubsan, in that order.
		# Rebuild the same name here or package() collects another flavor's binaries, or none at all.
		# CMakeToolchain feeds tools.build:cflags into CMAKE_C_FLAGS_INIT, which is the CMAKE_C_FLAGS that
		# block matches -pthread against, so reading that conf is reading the flags CMake saw.
		# build_web.py's webFlavorSuffix() is the third copy of the rule.

		if self.settings.os == "Emscripten":

			archName = "wasm64"

			if "-pthread" in self.conf.get("tools.build:cflags", default=[], check_type=list):
				archName += "_mt"

			if self.options.enableASAN:
				archName += "_asan"

			if self.options.enableUBSAN:
				archName += "_ubsan"

		elif self.settings.arch == "x86_64":
			archName = "x64"
		else:
			archName = "arm64"

		if self.settings.os == "Windows":
			platform = "windows"
		elif self.settings.os == "Macos":
			platform = "osx"
		elif self.settings.os == "Android":
			platform = "android"
		elif self.settings.os == "Emscripten":
			platform = "web"
		else:
			platform = "linux"

		# Artifacts don't land in the conan build folder: CMakeLists.txt redirects ARCHIVE/LIBRARY/RUNTIME
		# output to <cmake root>/build/<config>/<platform>/<arch>/{lib,bin}, and add_virtual_files() writes
		# its oiCA archives to <cmake root>/build/<config>/<platform>/packages (see cmake/oxc3.cmake).
		# So derive both from the source tree rather than guessing from build_folder, which moves around
		# depending on -of, the generator and whether the recipe was created or built in place.

		cmake_root = self._cmakeRoot()
		out_root   = os.path.join(cmake_root, "build", str(self.settings.build_type), platform)

		# CMakeLists gives a non default toolchain its own output directory so two compilers can't
		# inherit each other's archives; see the note there.
		# Same rule, or package() collects nothing.

		if platform == "windows" and str(self.settings.compiler) != "msvc":
			archName += "_clang"

		elif platform == "linux" and str(self.settings.compiler) != "gcc":
			archName += "_clang"

		input_dir        = os.path.join(out_root, archName)
		OxC3_package_dir = os.path.join(out_root, "packages")

		# Single-config generators (Ninja/Makefiles, what conan uses off Windows) omit the config subdir for the
		# packages output oxc3.cmake writes (build/<platform>/packages), while bin and lib keep it. Fall back to
		# the no-config path so the shader library still lands in bin/packages for consumers.
		if not os.path.isdir(OxC3_package_dir):
			OxC3_package_dir = os.path.join(cmake_root, "build", platform, "packages")

		input_lib_dir = os.path.join(input_dir, "lib")
		input_bin_dir = os.path.join(input_dir, "bin")

		# The test executables live in the same bin/ as the CLI and the packagers, but nothing consuming
		# this package runs them, and OxC3_shader_compiler_test alone is tens of MB.
		# Only the executables are dropped: OxC3_types_test and OxC3_types_container_test_util are the
		# libraries a consumer writes its *own* tests against, so those stay in lib/.
		# _dylib_test is a shared library rather than an executable, but it exists purely for
		# Test_dynamicLibrary to dlopen, so it goes too.
		# OxC3_plinttst is the platforms interface test; it doesn't end in _test because a packaged
		# target name is capped at 15 characters (see apply_dependencies in cmake/oxc3.cmake).
		# The _perf binaries are benchmarks, equally uninteresting to a consumer.

		testBinaries = [
			"*_test", "*_test.exe", "*_test.pdb", "*_dylib_test*",
			"*plinttst*", "*_perf", "*_perf.exe", "*_perf.pdb"
		]

		# OxC3_types_test and OxC3_types_container_test_util stay: those are the libraries a consumer links
		# to write its own tests. What goes is the import library of the dylib fixture excluded above
		# (whose DLL is no longer here to import) and the benchmark support library.

		testLibs = [ "*dylib_test*", "*_perf_lib*" ]

		copy(self, "*.a", input_lib_dir, lib_dst, excludes=testLibs)
		copy(self, "*.lib", input_lib_dir, lib_dst, excludes=testLibs)
		copy(self, "*.pdb", input_lib_dir, lib_dst, excludes=testLibs)
		copy(self, "*.exp", input_lib_dir, lib_dst, excludes=testLibs)
		copy(self, "*", input_bin_dir, bin_dst, excludes=testBinaries)
		copy(self, "*", OxC3_package_dir, bin_dst + "/packages")

	def package_info(self):

		if self.settings.os == "Windows":
			self.cpp_info.system_libs = [ "Bcrypt", "dxgi", "dwmapi", "dwrite", "Msimg32" ]

		elif self.settings.os == "Macos" or self.settings.os == "iOS" or self.settings.os == "watchOS":
			self.cpp_info.frameworks = [ "Security", "CoreFoundation", "ApplicationServices", "AppKit" ]

		elif self.settings.os == "Emscripten":
			# -lopenal resolves to emscripten's built-in OpenAL (WebAudio-backed)
			self.cpp_info.system_libs = [ "openal" ]

		else:
			self.cpp_info.system_libs = [ "m", "xkbcommon", "wayland-cursor" ]

		self.cpp_info.libs = [ "OxC3_formats_bmp", "OxC3_formats_oiBC", "OxC3_formats_hdr" ]

		# Headless: no graphics module on the web target (no WebGPU backend yet)
		if self.settings.os != "Emscripten":
			self.cpp_info.libs += [ "OxC3_graphics" ]

		self.cpp_info.libs += [ "OxC3_formats_oiSH", "OxC3_formats_oiSB", "OxC3_platforms", "OxC3_formats_dds", "OxC3_formats_oiCA", "OxC3_formats_oiDL", "OxC3_formats_oiXX", "OxC3_types_container", "OxC3_types_math", "OxC3_types_base" ]

		# The Vulkan loader is loaded dynamically at runtime (see vk_instance.c) and its headers come from the
		# vulkan_headers package, so there's no Vulkan import lib, system lib or SDK dir to link/include here.
		if self.settings.os == "Android":
			self.cpp_info.system_libs += [ "android", "log" ]

		self.cpp_info.set_property("cmake_file_name", "oxc3")
		self.cpp_info.set_property("cmake_target_name", "oxc3::oxc3")
		self.cpp_info.set_property("pkg_config_name", "oxc3")
		self.cpp_info.set_property("cmake_build_modules", [os.path.join("cmake", "oxc3.cmake")])
