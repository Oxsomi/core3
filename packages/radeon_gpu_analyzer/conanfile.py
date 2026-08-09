from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.files import copy, rmdir
from conan.errors import ConanInvalidConfiguration, ConanException
import os
import sys

required_conan_version = ">=2.0"

# Radeon GPU Analyzer, packaged as a command line tool (no libraries are exposed).
#
# What this provides: the rga CLI plus the offline helper binaries it drives, so OxC3 can
#  - enumerate supported devices and the architecture they belong to (rga -s <mode> --list-asics),
#  - compile/disassemble shaders to AMD ISA offline (e.g. SPIR-V through vk-offline/amdllpc,
#    OpenCL through the LC compiler, plus amdgpu-dis for ISA disassembly).
#
# Build shape: CLI only (--no-qt drops the Qt GUI entirely, --no-vulkan drops the live-driver Vulkan
# mode and with it the Vulkan SDK requirement). The offline modes don't need a driver or a GPU.
#
# Platform reality check: the offline compilers (amdllpc, LC, amdgpu-dis) are prebuilt AMD binaries
# vendored in the repo (git-lfs) for Windows and Linux x64 only, so that's where this package exists;
# everywhere else (macOS/Android/wasm) the root conanfile simply doesn't require it, like nvapi/ags.
# If wasm output is ever wanted, the realistic route is running this on the server/tool side, not
# compiling RGA itself to wasm (it depends on those prebuilt compilers).

class radeon_gpu_analyzer(ConanFile):

	name = "radeon_gpu_analyzer"
	version = "2026.08.02"

	license = "MIT (RGA); bundled AMD offline compilers are proprietary redistributables, see EULA.txt/RGA_NOTICES.txt"
	author = "AMD (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/radeon_gpu_analyzer"
	description = "Fork of AMD's Radeon GPU Analyzer; CLI-only build for offline shader-to-ISA analysis and device enumeration"
	topics = ("amd", "isa", "shader-analysis", "spirv", "dxil", "gpu")

	package_type = "application"

	# A tool package: the binary only varies by os/arch, not by the consumer's compiler or build_type.
	# This also means one cache entry serves every config (no Debug/Release rebuild churn like a library would have).
	settings = "os", "arch"

	def validate(self):

		if self.settings.os not in ("Windows", "Linux") or self.settings.arch != "x86_64":
			raise ConanInvalidConfiguration(
				"radeon_gpu_analyzer only supports Windows/Linux x86_64 (the vendored AMD offline compilers "
				"are prebuilt for those platforms only)"
			)

	def _repo(self):
		return os.path.join(self.source_folder, "radeon_gpu_analyzer")

	def source(self):

		# Clone into an explicitly named subfolder so git.folder can't end up pointing at a non-repo (conan's implicit
		# clone-target name is derived from the URL, which is brittle if the URL ever changes or gets a trailing slash).

		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"], target="radeon_gpu_analyzer")
		git.folder = "radeon_gpu_analyzer"
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

		# The offline compilers (amdllpc, LC, amdgpu-dis, atidxx64) are git-lfs blobs; without git-lfs the
		# checkout silently contains pointer files and the tool is useless, so pull them explicitly and fail loud.

		try:
			git.run("lfs install --local")
			git.run("lfs pull")
		except Exception as e:
			raise ConanException(
				"radeon_gpu_analyzer requires git-lfs to fetch the vendored AMD offline compilers: " + str(e)
			)

		# Populate the remaining external/ dependencies (device_info, update_check_api, third_party, ...).
		# The dependency map pins every repo to a fixed revision, so this stays reproducible.

		self.run('"{}" build/fetch_dependencies.py'.format(sys.executable), cwd=self._repo())

	def build(self):

		repo = os.path.join(self.build_folder, "..", "..", "radeon_gpu_analyzer")

		if not os.path.isdir(repo):
			repo = self._repo()

		out = os.path.join(self.build_folder, "rga_out")

		# pre_build.py is RGA's canonical cross-platform configure step (it assembles the cmake invocation,
		# version info and per-platform generators); riding it beats replicating its logic here.
		# --no-qt = headless CLI only, --no-vulkan = no live-driver Vulkan mode (and no Vulkan SDK needed);
		# the offline modes (vk-offline/amdllpc, LC, amdgpu-dis) are unaffected by both.

		preBuild = '"{}" pre_build.py --no-qt --no-vulkan --output "{}"'.format(sys.executable, out)

		if self.settings.os == "Windows":
			preBuild += " --vs 2022"

		self.run(preBuild, cwd=os.path.join(repo, "build"))

		if self.settings.os == "Windows":
			cmakeDir = os.path.join(out, "vs2022")
		else:
			cmakeDir = os.path.join(out, "make", "release")

		self.run('cmake --build "{}" --config Release --target radeon_gpu_analyzer_cli --parallel'.format(cmakeDir))

	def package(self):

		repo = self._repo()
		out = os.path.join(self.build_folder, "rga_out")

		# The CLI's post build steps copy the offline helpers (utils/amdllpc, utils/lc/**, amdgpu-dis, ...)
		# next to the executable, so packaging the executable's whole directory gives a self contained tool.
		# Where that directory lands depends on the generator (vs2022/release on Windows, release on makefiles,
		# and RGA's own CMake overrides pre_build's runtime dir in places), so locate rga instead of assuming.

		exeName = "rga.exe" if self.settings.os == "Windows" else "rga"
		releaseDir = None

		for root, _, files in os.walk(out):
			if exeName in files and "release" in root.lower():
				releaseDir = root
				break

		if not releaseDir:
			raise ConanException("radeon_gpu_analyzer: couldn't locate the built " + exeName + " under " + out)

		copy(self, "*", releaseDir, os.path.join(self.package_folder, "bin"))

		# OxC3 only uses the shader-to-ISA paths, not the OpenCL/Lightning compiler; drop it to shave package size.
		# The ISA disassembler (utils/lc/disassembler/amdgpu-dis) lives elsewhere and is kept.
		rmdir(self, os.path.join(self.package_folder, "bin", "utils", "lc", "opencl"))

		copy(self, "LICENSE.txt", repo, self.package_folder)
		copy(self, "EULA.txt", repo, self.package_folder)
		copy(self, "RGA_NOTICES.txt", repo, self.package_folder)

	def package_info(self):

		self.cpp_info.includedirs = []
		self.cpp_info.libdirs = []
		self.cpp_info.bindirs = ["bin"]

		binDir = os.path.join(self.package_folder, "bin")

		# So consumers (and OxC3 at build/test time) can locate and spawn the tool without hardcoding cache paths

		self.buildenv_info.prepend_path("PATH", binDir)
		self.runenv_info.prepend_path("PATH", binDir)
		self.runenv_info.define_path("RGA_PATH", binDir)
