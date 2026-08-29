from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.files import copy
from conan.errors import ConanInvalidConfiguration, ConanException
import os
import stat

required_conan_version = ">=2.0"

# The AMD offline shader compilers Radeon GPU Analyzer vendors, packaged as a command line tool package.
#
# What this provides: amdllpc (SPIR-V -> AMD ISA ELF, per gfxip) and amdgpu-dis (ELF -> ISA text), which is all
# OxC3 drives for its `isa` commands; the rga CLI itself isn't built or shipped, since OxC3 spawns the two
# compilers directly and rga would only add a Boost dependency and a sizeable C++ build on every CI runner.
#
# Platform reality check: the compilers are prebuilt AMD binaries vendored in the repo (git-lfs) for Windows and
# Linux x64 only, so that's where this package exists; everywhere else (macOS/Android/wasm) the root conanfile
# simply doesn't require it, like nvapi/ags.

class radeon_gpu_analyzer(ConanFile):

	name = "radeon_gpu_analyzer"
	version = "2026.08.02"

	license = "MIT (RGA); bundled AMD offline compilers are proprietary redistributables, see EULA.txt/RGA_NOTICES.txt"
	author = "AMD (original) & Oxsomi (modifications only)"
	url = "https://github.com/Oxsomi/radeon_gpu_analyzer"
	description = "AMD's offline shader compilers (amdllpc, amdgpu-dis) as vendored by Radeon GPU Analyzer"
	topics = ("amd", "isa", "shader-analysis", "spirv", "gpu")

	package_type = "application"

	# A tool package: the binaries only vary by os/arch, not by the consumer's compiler or build_type.
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

	# The two tools per platform, as (path inside the repo, path inside the package's bin/).
	# The bin/ layout mirrors where rga's own build puts them, which is the layout OxC3 looks them up in
	# (rga/utils/amdllpc, rga/utils/lc/disassembler/amdgpu-dis next to the executable).

	@staticmethod
	def _toolsFor(windows):

		platform = "windows" if windows else "linux"
		ext = ".exe" if windows else ""

		return [
			(
				"external/vulkan_offline/" + platform + "/amdllpc" + ext,
				os.path.join("utils", "amdllpc" + ext)
			),
			(
				"external/lc/disassembler/" + platform + "/amdgpu-dis" + ext,
				os.path.join("utils", "lc", "disassembler", "amdgpu-dis" + ext)
			),
		]

	def _tools(self):
		return self._toolsFor(self.settings.os == "Windows")

	def source(self):

		# Clone into an explicitly named subfolder so git.folder can't end up pointing at a non-repo (conan's implicit
		# clone-target name is derived from the URL, which is brittle if the URL ever changes or gets a trailing slash).

		git = Git(self)
		git.clone(url=self.conan_data["sources"][self.version]["url"], target="radeon_gpu_analyzer")
		git.folder = "radeon_gpu_analyzer"
		git.checkout(self.conan_data["sources"][self.version]["checkout"])

		# The compilers are git-lfs blobs; without git-lfs the checkout silently contains pointer files and the
		# tools are useless, so pull them explicitly and fail loud.
		# Only the files OxC3 uses are pulled, which keeps the download at their size rather than every vendored
		# compiler's. Both platforms' are pulled, since source() is shared by every configuration and may not
		# consult the settings; package() then takes the pair for the one being built.

		include = ",".join(src for windows in (True, False) for src, _ in self._toolsFor(windows))

		try:
			git.run("lfs install --local")
			git.run('lfs pull --include="{}"'.format(include))
		except Exception as e:
			raise ConanException(
				"radeon_gpu_analyzer requires git-lfs to fetch the vendored AMD offline compilers: " + str(e)
			)

	def build(self):

		# Nothing is compiled: the tools are prebuilt.
		# A pointer file left behind by a checkout without lfs is a few hundred bytes, so anything that small is
		# the silent failure mode source() guards against, caught here a second time in case lfs answered but
		# skipped a file.

		for src, _ in self._tools():

			path = os.path.join(self._repo(), src)

			if not os.path.isfile(path) or os.path.getsize(path) < 1024 * 1024:
				raise ConanException(
					"radeon_gpu_analyzer: " + src + " is missing or is still a git-lfs pointer; the lfs pull "
					"in source() didn't fetch it"
				)

	def package(self):

		repo = self._repo()
		binDir = os.path.join(self.package_folder, "bin")

		for src, dst in self._tools():

			srcDir = os.path.dirname(os.path.join(repo, src))
			dstDir = os.path.dirname(os.path.join(binDir, dst))
			copy(self, os.path.basename(src), srcDir, dstDir)

			# The blobs come out of git without their execute bit.

			if self.settings.os != "Windows":
				out = os.path.join(binDir, dst)
				os.chmod(out, os.stat(out).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

		copy(self, "LICENSE.txt", repo, self.package_folder)
		copy(self, "EULA.txt", repo, self.package_folder)
		copy(self, "RGA_NOTICES.txt", repo, self.package_folder)

	def package_info(self):

		self.cpp_info.includedirs = []
		self.cpp_info.libdirs = []
		self.cpp_info.bindirs = ["bin"]

		binDir = os.path.join(self.package_folder, "bin")

		# So consumers (and OxC3 at build/test time) can locate and spawn the tools without hardcoding cache paths

		self.buildenv_info.prepend_path("PATH", binDir)
		self.runenv_info.prepend_path("PATH", binDir)
		self.runenv_info.define_path("RGA_PATH", binDir)
