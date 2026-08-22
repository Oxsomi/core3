# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/rt_core/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

cmake_minimum_required(VERSION 3.26.0)

# The dependencies come from conan, so the toolchain it writes has to exist before project() runs.
# Checked here because project() is where CMake consumes it, and the error it raises there names a missing
# file without saying which tool was supposed to produce it.
# An IDE configuring from a preset is the usual way to arrive here, since it will happily configure a
# configuration that was never built.

if(CMAKE_TOOLCHAIN_FILE AND NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")

	# Recovered from the TOOLCHAIN path, not the build folder: a preset builds into a tree of its own
	# (build/ide/<preset>) and only reads the toolchain out of the one the script made, so the build folder no
	# longer names the configuration but the toolchain still does.
	# Everything but windows puts the mode in that path (build/<mode>/<platform>/...), because those trees are
	# single config; windows keeps every config in one tree, so -mode is optional there.

	set(setupScript "build.py")
	set(setupArgs "-tests True")

	if("${CMAKE_TOOLCHAIN_FILE}" MATCHES "/build/([^/]+)/(android|linux|osx)/")
		string(PREPEND setupArgs "-mode ${CMAKE_MATCH_1} ")
	endif()

	if("${CMAKE_TOOLCHAIN_FILE}" MATCHES "/android/([^/]+)/")

		# A cross build, driven by its own script, and the arch is explicit rather than the host's.

		set(setupScript "build_android.py")
		string(APPEND setupArgs " -arch ${CMAKE_MATCH_1} -generator Ninja")

	else()

		if("${CMAKE_TOOLCHAIN_FILE}" MATCHES "_clang")
			string(APPEND setupArgs " -compiler clang")
		endif()

		if("${CMAKE_TOOLCHAIN_FILE}" MATCHES "_ninja")
			string(APPEND setupArgs " -generator ninja")
		endif()

	endif()

	# An IDE preset bootstraps itself rather than sending the reader to a terminal: opening a configuration
	# nobody has built is the normal way to arrive here, and "run this and come back" is a poor answer to it.
	# -setup_only stops after conan writes the toolchain and dependency files, so this does not build OxC3
	# inside a configure step; the IDE builds it afterwards like any other tree.
	# A plain cmake invocation still only reports the command, since there the caller drives the build anyway.

	if(OxC3IdeTree)

		find_program(oxc3Python NAMES python3 python py REQUIRED)

		message("-- Setting this configuration up first: ${setupScript} ${setupArgs} (one time, and slow)")

		separate_arguments(setupArgList NATIVE_COMMAND "${setupArgs}")

		execute_process(
			COMMAND "${oxc3Python}" "${CMAKE_CURRENT_SOURCE_DIR}/${setupScript}" ${setupArgList} -setup_only True
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			RESULT_VARIABLE setupResult
		)

		if(setupResult EQUAL 0 AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
			message("-- Set up, continuing")
		else()
			message(FATAL_ERROR
				"setting this configuration up failed (${setupResult}), so there is still no toolchain at "
				"${CMAKE_TOOLCHAIN_FILE}. Run it by hand to see what went wrong:"
				"\n\n    python ${setupScript} ${setupArgs}\n"
			)
		endif()

	else()

		message(FATAL_ERROR
			"conan has not set this configuration up yet, so there is no toolchain at "
			"${CMAKE_TOOLCHAIN_FILE} -- the build scripts own that step. Run this once, then configure again:"
			"\n\n    python ${setupScript} ${setupArgs}\n"
		)

	endif()
endif()

# conan generates its dependency files per configuration, so a tree only has the ones the script was asked to
# build. An IDE offers every configuration in the preset regardless, and picking one conan never generated
# fails late and unhelpfully: find_package quietly finds nothing and the first compile reports a missing
# AL/al.h or nvapi.h, which looks like a broken checkout rather than a configuration that was never set up.
#
# The generators folder names them (agility_sdk-Target-release.cmake and friends), so the available set is
# read straight out of it and the IDE is offered exactly that.

if(OxC3IdeTree AND CMAKE_TOOLCHAIN_FILE)

	get_filename_component(oxc3GenDir "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
	file(GLOB oxc3ConanConfigs "${oxc3GenDir}/*-Target-*.cmake")

	set(oxc3HaveConfigs "")

	foreach(cfgFile ${oxc3ConanConfigs})
		if("${cfgFile}" MATCHES "-Target-([a-z]+)\.cmake$")
			set(cfgName "${CMAKE_MATCH_1}")
			foreach(known Debug Release MinSizeRel RelWithDebInfo)
				string(TOLOWER "${known}" knownLower)
				if("${cfgName}" STREQUAL "${knownLower}")
					list(APPEND oxc3HaveConfigs "${known}")
				endif()
			endforeach()
		endif()
	endforeach()

	list(REMOVE_DUPLICATES oxc3HaveConfigs)

	if(oxc3HaveConfigs)

		set(CMAKE_CONFIGURATION_TYPES "${oxc3HaveConfigs}" CACHE STRING "" FORCE)
		message("-- conan set this tree up for: ${oxc3HaveConfigs}")

		# A single config generator picks one, so it has to be one of those rather than whatever it defaulted to.

		if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE IN_LIST oxc3HaveConfigs)
			list(GET oxc3HaveConfigs 0 oxc3FirstConfig)
			message(FATAL_ERROR
				"conan set this tree up for ${oxc3HaveConfigs}, not ${CMAKE_BUILD_TYPE}. Pick one of those, or "
				"run the build script again for ${CMAKE_BUILD_TYPE} first. On windows build.py builds every "
				"configuration when -mode is left off."
			)
		endif()

		if(NOT CMAKE_BUILD_TYPE)
			list(GET oxc3HaveConfigs 0 CMAKE_BUILD_TYPE)
		endif()

	endif()

endif()

project(OxC3)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SUPPRESS_REGENERATION TRUE)

# What clangd and VS Code's C/C++ extension read to know how each file is compiled.
# Only the Ninja and Makefile generators emit it; the Visual Studio generator ignores it and gets its
# IntelliSense from the .vcxproj instead, so this is harmless there rather than conditional.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SET(GRAPHVIZ_CUSTOM_TARGETS TRUE)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)				# We need some C++ to interface with DXC, but we wrap it using C
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "32-bit builds are disabled by core3 because it's legacy and could cause unintended behavior if built anyways.")
endif()

set(EnableSIMD ON CACHE BOOL "Enables SIMD")
set(CLIGraphics ON CACHE BOOL "Turn off/on OxC3 graphics CLI (might be unsupported on device)")
set(EnableShaderCompiler OFF CACHE BOOL "Turn off/on shader compiler + OxC3 compile shaders CLI (increases compile time)")

# DXC and SPIRV-Reflect are the bulk of a from-scratch build and are almost never what's being debugged, so build.py keeps them
# in Release even for a Debug build.
# Turn this on (-debug_shader_compiler True) to build and consume them in the current mode, at the cost of a much slower first
# build.
# Off keeps CI fast; it's the reason a Debug build doesn't pay for a Debug DXC.

set(DebugShaderCompiler OFF CACHE BOOL "Build/consume the shader compiler's dependencies in the current mode rather than Release")

set(EnableTests ON CACHE BOOL "Turn off/on OxC3 tests")

# Diagnostic builds, both off by default and never shipped.
# Deliberately not offered on MSVC: its AddressSanitizer isn't dependable enough to be worth having.
# An unreliable sanitizer is worse than none, because a clean run reads as proof that nothing is wrong.
# Asking for one there is a hard error rather than a silent no-op for that reason;
# a flag that quietly does nothing buys false confidence.
# Use RelWithDebInfo: ASan wants optimized code with frame pointers and symbols, not a Debug build.

set(EnableASAN OFF CACHE BOOL "Build with AddressSanitizer (clang/gcc only)")
set(EnableUBSAN OFF CACHE BOOL "Build with UndefinedBehaviorSanitizer (clang/gcc only)")
set(EnableOxC3CLI OFF CACHE BOOL "Turn on/off OxC3 CLI")
set(DynamicLinkingGraphics OFF CACHE BOOL "Turn on/off OxC3 graphics dynamic linking")

# Independent of DynamicLinkingGraphics on purpose: they solve different problems.
# Graphics dynamic linking exists so Vulkan and D3D12 can sit side by side and be picked at runtime:
# Each backend is its own module exporting one factory (GraphicsInterface_getTable).
# This one is about size and deployment: OxC3_shader_compiler statically links DXC and SPIRV-Tools,
# so every executable that touches it grows by ~28 MB.
# As a shared library that bulk exists once and can be shipped, or left out, separately.
# There's no runtime selection, so it's an ordinary link-time shared library rather than a loaded interface table.
# (Though could also support different shader compilers in the future)
# Wanting dynamic graphics with a static shader compiler (or no shader compiler at all) is normal.
#
# On by default on desktop: it's smaller, it builds faster (DXC gets linked once instead of once per executable)
# and it keeps the compiler behind a module boundary, so a different backend (Slang, clang)
# could be dropped in without relinking consumers, as long as it keeps the ABI.

set(DynamicLinkingShaderCompiler ON CACHE BOOL "Turn on/off OxC3 shader compiler dynamic linking")

# Set general compile options

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "ARM64|arm64|aarch64|armv8" OR "${ANDROID_ABI}" MATCHES "arm64-v8a")
	message("-- Enabling ARM specific optimizations")
	set(ARM ON)
	set(target "arm64")
else()
	message("-- Enabling x64 specific optimizations")
	set(ARM OFF)
	set(target "x64")
endif()

if(ANDROID AND DynamicLinkingGraphics)
	message(FATAL_ERROR "-- FATAL configuration (dynamic linking graphics) not compatible with Android")
endif()

# Coerced rather than fatal, because this defaults to ON: a platform that can't do it, or a build with
# no shader compiler to share, shouldn't fail on a value nobody asked for.

if(DynamicLinkingShaderCompiler AND ANDROID)
	message("-- Shader compiler dynamic linking off: not supported on Android")
	set(DynamicLinkingShaderCompiler OFF)
endif()

if(DynamicLinkingShaderCompiler AND NOT EnableShaderCompiler)
	set(DynamicLinkingShaderCompiler OFF)
endif()

# Dynamic linking folds the static OxC3 libs into shared objects (libOxC3_graphics_vk.so).
# On ELF (Linux) every object linked into a .so must be PIC, or ld fails with
# "relocation R_X86_64_PC32 / R_AARCH64_ADR_PREL_PG_HI21 ... recompile with -fPIC".
# No-op on MSVC, effectively no-op on Apple.
# Off for static builds to keep non-PIC codegen.
if(DynamicLinkingGraphics OR DynamicLinkingShaderCompiler)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# clang-cl sets MSVC too, since it takes MSVC's flags and targets its ABI.
# It is not MSVC though: it has clang's warnings and lacks the MSVC-only flags.
# So everything below that is genuinely MSVC-specific has to be gated on this rather than on MSVC.
set(isClangCl FALSE)
if(MSVC AND CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(isClangCl TRUE)
endif()

# NOT MSVC would be the wrong guard here: clang-cl sets MSVC too,
# and it's the one Windows toolchain that does have usable sanitizers.
# Key off the compiler id instead.

if((EnableASAN OR EnableUBSAN) AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
	message(FATAL_ERROR
		"EnableASAN/EnableUBSAN are only supported on clang and gcc. On Windows rebuild with "
		"-compiler clang (clang-cl), which keeps MSVC's ABI and CRT."
	)
endif()

# Instruction sets the x64 SIMD paths assume.
# MSVC declares every intrinsic regardless and only wants a codegen baseline (/d2archSSE42),
# but clang and gcc gate each one behind its own -m flag.
# Without these, flp.c's _mm_cvtph_ps/_mm_cvtps_ph (F16C) and vec4f_sse.inc.h's __builtin_ia32_vfmaddps
# (FMA) don't exist as far as the compiler is concerned.
# Shared so the clang-cl and gcc/clang branches can't drift apart.
#
# Only spellings every driver accepts belong here.
# clang-cl rejects -mfpmath=sse and -msse4, and rejection is fatal rather than cosmetic:
# that's -Wunknown-argument, which /WX turns into an error.
# It is a different warning from the -Wunused-command-line-argument covering flags it merely ignores.
# -msse4 is dropped rather than moved, being exactly -msse4.1 plus -msse4.2, both already here.

set(X64_SIMD_FLAGS
	-msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2
	-mrdrnd -msha -maes -mpclmul -mbmi2 -mf16c -mavx -mfma
)

if(MSVC)

	# /wd4201: anonymous struct/union members are standard in C11 but an extension in C++; OxC3 uses them
	# throughout (e.g. sh_entries.h), and gcc/clang get the same via -fms-extensions below.
	set(BUILD_FLAGS "/MP" "/GR" "/D_CRT_SECURE_NO_WARNINGS" "/Gy" "/W4" "/WX" "/wd4201" "/fp:strict")

	if(EnableSIMD)
		if(NOT ARM)
			if(isClangCl)
				list(APPEND BUILD_FLAGS ${X64_SIMD_FLAGS})	# /d2archSSE42 is MSVC-only and undocumented
			else()
				list(APPEND BUILD_FLAGS "/d2archSSE42")		#  Require SSE4.2
			endif()
		elseif(isClangCl)

			# Same features the gcc/clang branch below asks for on arm64: clang gates each NEON crypto
			# intrinsic (vaeseq_u8, vmull_p64, the sha2 set) behind its target feature and errors at the
			# always_inline call site without it, while MSVC declares every intrinsic unconditionally.
			# clang-cl has no -march spelling of its own, so /clang: hands the flag to the GCC-style
			# driver underneath.

			list(APPEND BUILD_FLAGS "/clang:-march=armv8-a+simd+crypto+crc")
		endif()
	endif()

	if(isClangCl)

		# clang-cl defaults to -fms-compatibility, which emulates MSVC's legacy preprocessor.
		# That breaks the variadic generators (sb_variable.c's EXPAND4x4, the vec swizzle expanders),
		# which are conformant and rely on __VA_ARGS__ being prescanned into separate arguments.
		# /Zc:preprocessor is silently ignored by clang-cl, so this is the only lever.
		# The code is standard C either way.
		list(APPEND BUILD_FLAGS "-fno-ms-compatibility")

		# The same two suppressions the gcc/clang branch below applies, for the same reasons.
		# `{ 0 }` is the idiomatic zero-initializer, and TList(T) expands to a set of static inline
		# accessors a TU only partly uses.
		# MSVC has no equivalent warnings, so they only need saying for clang.
		list(APPEND BUILD_FLAGS "-Wno-missing-field-initializers" "-Wno-unused-function")

		# MSVC-only flags that survive elsewhere in this file (and in conan's toolchain) are accepted
		# but ignored by clang-cl, which reports them as unused arguments -- fatal under /WX.
		list(APPEND BUILD_FLAGS "-Wno-unused-command-line-argument")

		# The /fsanitize spelling, not -fsanitize.
		# It makes clang-cl embed /defaultlib directives for the clang_rt runtime into the objects,
		# which is how the runtime gets linked when CMake drives lld-link directly
		# instead of going through the compiler driver.

		# CMake drives lld-link directly rather than going through the compiler driver, so the /defaultlib
		# directives clang-cl embeds for the sanitizer runtimes never reach the linker.
		# Point it at clang's own runtime directory ourselves.

		if(EnableASAN OR EnableUBSAN)

			get_filename_component(clangBinDir "${CMAKE_C_COMPILER}" DIRECTORY)
			file(GLOB clangRtDirs "${clangBinDir}/../lib/clang/*/lib/windows")

			if(NOT clangRtDirs)
				message(FATAL_ERROR "Sanitizer runtimes not found next to ${CMAKE_C_COMPILER}")
			endif()

			list(GET clangRtDirs 0 clangRtDir)
			link_directories("${clangRtDir}")
		endif()

		# /Oy- is the frame pointer flag clang-cl takes.
		# -fno-omit-frame-pointer is a driver spelling it rejects outright, which /WX turns into an error.
		# The dynamic runtime rather than the static one, because dynamicLinkingShaderCompiler puts a DLL in
		# the picture and a statically linked ASan in more than one module doesn't work.

		# The annotation defines are not optional here.
		# MSVC's STL annotates its string and vector storage for ASan, and records which way it was built in
		# an object directive, so lld-link rejects the mix with /failifmismatch on 'annotate_string'.
		# Our code is instrumented while the prebuilt dependencies (DXC, SPIRV-Reflect) are not, and
		# rebuilding those under ASan would cost far more than the annotations are worth.

		# compiler-rt names its Windows runtimes after the target arch, so this can't be hardcoded to x86_64:
		# that is what kept an arm64 sanitizer build off the table entirely.

		if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
			set(sanitizerArch "aarch64")
		else()
			set(sanitizerArch "x86_64")
		endif()

		# Whether a given toolchain actually ships the runtime for this arch is a build-time fact we can't
		# assume: LLVM has supported ASan on Windows aarch64 for a while, but whether a particular installer
		# includes the prebuilt library is up to whoever produced it.
		# Checking here turns "unresolved external symbol __asan_report_load8" thousands of lines into the
		# link log into a configure-time error that names the arch and lists what the toolchain does have,
		# which is what the first arm64 sanitizer run needs in order to be actionable.

		set(requiredSanitizerLibs "")

		if(EnableASAN)
			list(APPEND requiredSanitizerLibs
				"clang_rt.asan_dynamic-${sanitizerArch}.lib"
				"clang_rt.asan_dynamic_runtime_thunk-${sanitizerArch}.lib"

				# The DLL and not just its import library, since it's needed to RUN anything we build; it is
				# copied next to the executables below.

				"clang_rt.asan_dynamic-${sanitizerArch}.dll"
			)
		endif()

		if(EnableUBSAN)
			list(APPEND requiredSanitizerLibs "clang_rt.ubsan_standalone-${sanitizerArch}.lib")
		endif()

		foreach(sanitizerLib ${requiredSanitizerLibs})
			if(NOT EXISTS "${clangRtDir}/${sanitizerLib}")

				file(GLOB availableSanitizerLibs RELATIVE "${clangRtDir}" "${clangRtDir}/clang_rt.*")
				string(REPLACE ";" "\n  " availableSanitizerLibs "${availableSanitizerLibs}")

				message(FATAL_ERROR
					"${sanitizerLib} is missing from ${clangRtDir}, so this toolchain can't produce a "
					"sanitized ${sanitizerArch} build. Runtimes it does ship:\n  ${availableSanitizerLibs}"
				)
			endif()
		endforeach()

		if(EnableASAN)
			list(APPEND BUILD_FLAGS
				"/fsanitize=address" "/Oy-"
				"-D_DISABLE_STRING_ANNOTATION=1" "-D_DISABLE_VECTOR_ANNOTATION=1"
			)
			link_libraries(
				"clang_rt.asan_dynamic-${sanitizerArch}.lib"
				"clang_rt.asan_dynamic_runtime_thunk-${sanitizerArch}.lib"
			)

			# Linking the import library is only half of it: the dynamic ASan runtime has to be findable when
			# anything we built actually RUNS.
			# The build runs tools of its own (the packaging steps in add_virtual_files) and ctest runs the test
			# executables, and the loader looks next to the exe while knowing nothing about the toolchain's
			# lib/windows, so every one of them dies at load with
			# "clang_rt.asan_dynamic-<arch>.dll was not found".
			# A custom build step reports that as nothing but an exit code (0xC0000135), which says nothing
			# about a DLL, so this is worth being explicit about rather than rediscovering.
			# Adding the directory here reuses the copy every executable already does for its dependencies,
			# see apply_dependencies in cmake/oxc3.cmake.

			list(APPEND CONAN_RUNTIME_LIB_DIRS "${clangRtDir}")
		endif()

		# Identical COMDAT folding has to be off for a sanitized link, or ASan reports false positives on
		# perfectly legal reads.
		# ASan gives every global a trailing redzone, and Release folds identical globals across translation
		# units, so one global's redzone ends up sitting on another global's data.
		# The report that found this had THREE globals at one address, DXC's L"\0" and L"" plus our own "" from
		# compiler_helper_jobs.c, all of them nothing but NUL bytes and therefore foldable.
		# Reading the empty string then looks like reading a redzone, and ASan names the nearest preceding
		# global instead, which is why it read as a 6491 byte overrun of an unrelated 26 KB literal.
		# Debug never reproduced it because folding is a Release optimization, and DXC was identical in both.
		# ODR detection is likely the same story, since it works through a per-global indicator symbol that
		# folding can alias, so detect_odr_violation may be worth re-enabling once this is in.

		if(EnableASAN OR EnableUBSAN)
			add_link_options("-opt:noicf")
		endif()

		# Trap rather than diagnose: UBSan's reporting runtime is the flakiest part of its Windows support,
		# and a trap is enough here since OxC3 installs a crash handler that prints a stacktrace anyway.
		# vptr is off because it needs RTTI across the whole program, which the prebuilt dependencies
		# don't give us.

		# Linking the standalone runtime so UBSan names the check and the line rather than just trapping.
		# vptr is off because it needs RTTI across the whole program, which the prebuilt dependencies don't
		# give us. enum is off because OxC3 uses flag enums and sentinels (e.g. ESHExtension holds OR'd bits
		# and a 1<<Count "unset" marker) whose values are legal by design but not individual enumerators, which
		# -fsanitize=enum flags on every load; that idiom is pervasive here, so the check is all noise.

		if(EnableUBSAN)
			list(APPEND BUILD_FLAGS "-fsanitize=undefined" "-fno-sanitize=vptr,enum" "/Oy-")
			link_libraries("clang_rt.ubsan_standalone-${sanitizerArch}.lib")
		endif()
	endif()

	# add_compile_options(/fsanitize=address)

	add_compile_options(
		"$<$<COMPILE_LANGUAGE:C>:${BUILD_FLAGS}>"
		"$<$<COMPILE_LANGUAGE:CXX>:${BUILD_FLAGS}>"
	)

	# MSVC's legacy preprocessor mis-expands nested __VA_ARGS__,
	# which vec4f_swizzle.h's generators rely on (F32x4_expand -> expand2 -> expand3 -> expand4).
	# C doesn't hit it because C17 mode already implies the conformant preprocessor.
	# C++ still defaults to the legacy one, so the oxc:: wrappers, which pull the vector headers into oxc::c,
	# fail to compile without this.
	# gcc/clang are conformant either way.
	# clang-cl ignores the flag entirely, and gets the same result from -fno-ms-compatibility above.
	if(NOT isClangCl)
		add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>")
	endif()

	# Whole-program optimization (MSVC LTO): /GL to compile, /LTCG to link. lib.exe needs /LTCG told separately
	# via the static-linker flags or every archive step warns (LNK4075/D9035). /LTCG makes the link the
	# bottleneck, so this is gated to Release only - a ship/CI config, keeping local Debug/RelWithDebInfo fast.
	# A target that sets OXC3_NO_WHOLE_PROGRAM opts out.
	# WINDOWS_EXPORT_ALL_SYMBOLS reads the .obj files to write the .def,
	# and neither /GL nor -flto leaves anything there it can parse.
	# Excluding those targets up front beats overriding with /GL- afterwards,
	# which works but makes MSVC warn D9025 on every TU, since it sees both on the command line.

	set(wholeProgram "$<AND:$<CONFIG:Release>,$<NOT:$<BOOL:$<TARGET_PROPERTY:OXC3_NO_WHOLE_PROGRAM>>>>")

	if(isClangCl)

		# ThinLTO would go here, and it does work when one LLVM does the whole build:
		# add_compile_options("$<$<AND:${wholeProgram},$<COMPILE_LANGUAGE:C,CXX>>:-flto=thin>")
		#
		# It's off because the ClangCL toolset doesn't guarantee that.
		# The compile runs the clang-cl CMake resolved, usually from PATH,
		# while the link runs lld-link out of $(LLVMInstallDir), which defaults to the LLVM shipped with Visual Studio.
		# Bitcode isn't readable across LLVM major versions,
		# so the moment those two differ every link fails with "is not a native COFF file",
		# and the build works or doesn't purely by which clang happens to be installed.
		# Pinning both to one LLVM is what this needs before it can come back.

	else()
		add_compile_options("$<$<AND:${wholeProgram},$<COMPILE_LANGUAGE:C,CXX>>:/GL>")
		add_link_options("$<${wholeProgram}:/LTCG>")
		string(APPEND CMAKE_STATIC_LINKER_FLAGS_RELEASE " /LTCG")
	endif()

else()

	if(ARM)
		if(EnableSIMD)
			add_compile_options(-march=armv8-a+simd+crypto+crc)
		else()
			add_compile_options(-march=armv8-a)
		endif()
	else()
		if(EnableSIMD)
			add_compile_options(${X64_SIMD_FLAGS} -mfpmath=sse)		# gcc spelling, kept off the shared list
		else()
			add_compile_options(-mfpmath=sse -msse)		# Consistent float behavior
		endif()
	endif()

	add_compile_options(-fno-unsafe-math-optimizations)
	add_compile_options(-fms-extensions)
	add_compile_options(-Wall -Wextra -Werror)

	# LTO only for optimized configs, on both compile and link.
	# Debug stays fast (LTO makes the link the bottleneck).
	# -flto=auto parallelizes the link-time work across cores.
	#
	# clang emits LLVM bitcode, and GNU ld only reads that through LLVMgold.so,
	# which distros build against whatever LLVM the *system* has rather than the clang you're compiling with.
	# One major version apart is enough to fail the link with
	# "Unknown attribute kind ... (Producer: LLVM 18, Reader: LLVM 17)",
	# after which every archive looks empty and every symbol comes back undefined.
	# lld reads its own bitcode, so point clang at it, and drop LTO when it isn't installed:
	# a slightly slower binary beats a build that can't link.
	#
	# gcc is unaffected (its plugin ships with the compiler), and so are apple and android, which already
	# default to a linker that matches their toolchain.

	set(useLTO TRUE)

	if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT APPLE AND NOT ANDROID)

		find_program(OXC3_LLD NAMES ld.lld lld)

		if(OXC3_LLD)
			add_link_options("$<$<NOT:$<CONFIG:Debug>>:-fuse-ld=lld>")
		else()
			message("-- LTO disabled: clang needs lld to link its own bitcode and ld.lld wasn't found")
			set(useLTO FALSE)
		endif()

	endif()

	if(useLTO)
		add_compile_options("$<$<NOT:$<CONFIG:Debug>>:-flto=auto>")
		add_link_options("$<$<NOT:$<CONFIG:Debug>>:-flto=auto>")
	endif()

	# `{ 0 }` is OxC3's idiomatic zero-initializer (every field ends up zeroed).
	# -Wextra's -Wmissing-field-initializers still flags it when the C headers are compiled as C++ (the oxc::c compat layer),
	# where `{}` would be needed but isn't valid C.
	# The idiom is intentional, so silence this one.
	add_compile_options(-Wno-missing-field-initializers)

	# TList(T) expands to a full set of `static inline` accessors and a TU only ever uses a handful of them.
	# gcc and MSVC don't mind, but clang reports every leftover under -Wunused-function whenever the macro is
	# expanded in a .c instead of a header, which is the case for platform.c, the vulkan/d3d12 backends and the shader compiler.
	# Keep the warning on for gcc, which still catches genuinely dead static functions.
	if(CMAKE_C_COMPILER_ID MATCHES "Clang")
		add_compile_options(-Wno-unused-function)
	endif()

	# Fix for certain GCC versions misbehaving with struct copies.
	# (Yes, I did run into this, but it seems fixed in 14.3, 13.4)
	if(
		CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
		CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1" AND
		CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL "14.2"
	)
		add_compile_options(-mprefer-vector-width=128)
	endif()

	# Here the compiler driver does the link, so the same flag goes to both halves and pulls in its runtime.

	if(EnableASAN)
		add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
		add_link_options(-fsanitize=address)
	endif()

	# vptr needs RTTI across the prebuilt dependencies, which they aren't built with. enum is off because OxC3
	# uses flag enums and sentinels (ESHExtension holds OR'd bits and a 1<<Count "unset" marker) whose values
	# are legal by design but not individual enumerators, which -fsanitize=enum flags on every load.
	# function stays ON: every callback implementation now genuinely takes void* and casts internally, so an
	# indirect call through the callback pointer is a true type match.

	if(EnableUBSAN)
		add_compile_options(-fsanitize=undefined -fno-sanitize=vptr,enum -fno-omit-frame-pointer)
		add_link_options(-fsanitize=undefined)
	endif()

	# add_compile_options(-fsanitize=thread)
	# add_link_options(-fsanitize=thread)

endif()

if(EnableOxC3CLI)

	message("-- Enabling OxC3 CLI (-DEnableOxC3CLI=On)")

	if(CLIGraphics)
		message("-- Enabling OxC3 graphics CLI (-DCLIGraphics=On)")
	else()
		message("-- Disabling OxC3 graphics CLI (-DCLIGraphics=Off)")
	endif()

else()
	message("-- Disabling OxC3 CLI (-DEnableOxC3CLI=Off)")
endif()

if(EnableTests)
	message("-- Enabling OxC3 unit tests (-DEnableTests=On)")
	enable_testing()
else()
	message("-- Disabling OxC3 unit tests (-DEnableTests=Off)")
endif()

if(EnableShaderCompiler)
	if(DebugShaderCompiler)
		message("-- Shader compiler dependencies follow the build mode (-DDebugShaderCompiler=On)")
	else()
		message("-- Shader compiler dependencies pinned to Release (-DDebugShaderCompiler=Off)")
	endif()
endif()

# Can a test suite be its own executable that ctest runs?
#
# Not everywhere:
# android loads a .so through a NativeActivity and has no exec at all,
#  so those targets
#  can't even link (Platform_defineEntrypoint gives android_main, not main).
# wasm and iOS will be the same shape.
# Those platforms bundle every suite into a single module instead,
# one per platform under src/test/<platform>; see src/test/android for the reference implementation.
#
# Test *libraries* (OxC3_types_test and friends) are still built everywhere, since the bundle links them.

if(ANDROID OR EMSCRIPTEN OR IOS)
	set(OxC3TestExecutables OFF)
	message("-- Test suites bundle into one module (no exec on this platform)")
else()
	set(OxC3TestExecutables ON)
endif()

# Each suite runs behind a stamp file that depends on everything it was built from, so one whose inputs did not change
# does not re-run and the build graph does the dirty tracking.
# CI turns this OFF and drives ctest explicitly, which always runs everything and reports per test rather than as one
# build failure.

option(OxC3TestAutoRun "Run each test right after building it" ON)

if(OxC3TestAutoRun AND OxC3TestExecutables AND NOT CMAKE_CROSSCOMPILING)
	message("-- Tests run automatically after building (-DOxC3TestAutoRun=Off to disable)")
endif()

if(EnableShaderCompiler)

	message("-- Enabling OxC3 compile shaders (-DEnableShaderCompiler=On)")

	if(DynamicLinkingShaderCompiler)
		message("-- Enabling OxC3 shader compiler dynamic linking (-DDynamicLinkingShaderCompiler=On)")
	else()
		message("-- Disabling OxC3 shader compiler dynamic linking (-DDynamicLinkingShaderCompiler=Off)")
	endif()

else()
	message("-- Disabling OxC3 compile shaders (-DEnableShaderCompiler=Off)")
endif()

if(DynamicLinkingGraphics)
	message("-- Enabling OxC3 graphics dynamic linking (-DDynamicLinkingGraphics=On)")
else()
	message("-- Disabling OxC3 graphics dynamic linking (-DDynamicLinkingGraphics=Off)")
endif()

if(EnableSIMD)
	message("-- Enabling SIMD (-DEnableSIMD=ON)")
	if(ARM)
		set(simd neon)
	else()
		set(simd sse)
	endif()
	set(SIMD 1)
else()
	message("-- Disabling SIMD (-DEnableSIMD=OFF)")
	set(simd none)
	set(SIMD 0)
endif()

if(ForceFloatFallback)
	message("-- Enabling float fallback (supported float ops are now handled in software) (-DForceFloatFallback=ON)")
	set(_FORCE_FLOAT_FALLBACK "1")
else()
	message("-- Disabling float fallback (supported float ops are now handled in hardware) (-DForceFloatFallback=OFF)")
	set(_FORCE_FLOAT_FALLBACK "0")
endif()

if(WIN32)
	message("-- Enabling Windows support...")
	set(platform windows)
elseif(IOS)
	message("-- Enabling iOS support...")
	set(platform ios)
elseif(APPLE)
	message("-- Enabling OS X support...")
	set(platform osx)
elseif(ANDROID)
	message("-- Enabling Android support...")
	set(platform android)
else()
	message("-- Enabling Linux support...")
	set(platform linux)
endif()

# These directories are shared by every build folder, so a second toolchain writing into them is what
# makes builds pick up each other's artifacts: MSBuild's up to date check compares against the output
# file, sees the other compiler's newer one, skips the target and links the foreign archive.
# That's fatal rather than merely wrong, since MSVC Release compiles with /GL and only its own linker
# reads those objects; lld-link rejects them with "is not a native COFF file".
# So each non default toolchain gets its own directory, matching the separate build trees build.py
# already hands them.
# Default toolchains keep the bare path so existing trees stay valid, and android is left alone entirely
# because build_android.py packages the apk straight out of it.
#
# conanfile.py's package() and build_common.py's buildHostToolPackage rebuild this same path; keep the
# three in step.

set(archDir "${target}")

if(NOT ANDROID)
	if(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "Clang")
		set(archDir "${target}_clang")
	elseif(UNIX AND NOT APPLE AND NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
		set(archDir "${target}_clang")
	endif()
endif()

# Where the built artifacts land.
#
# An IDE that owns a build folder writes into it and clears what it does not recognise, so a preset points one
# at its own tree (build/ide/<preset>) rather than at the tree conan set up. That keeps conan's generators out
# of its reach, but both trees still describe the same configuration, so their artifacts would otherwise land
# on the same paths and overwrite each other. OxC3IdeTree keeps the IDE's inside its own tree.
#
# Packages are not affected: add_virtual_files derives those from SELF (the source root) rather than from the
# build folder, and writing one is idempotent, so both trees share them.

set(OxC3IdeTree OFF CACHE BOOL "Build tree belongs to an IDE, so keep artifacts inside it (see CMakePresets.json)")

if(OxC3IdeTree)
    set(oxc3OutRoot "${CMAKE_BINARY_DIR}/out")
    set(oxc3OutTail "")
else()
    set(oxc3OutRoot "${CMAKE_CURRENT_SOURCE_DIR}/build")
    set(oxc3OutTail "/${platform}/${archDir}")
endif()

# Single-config generators (Makefile, Ninja)

if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${oxc3OutRoot}/${CMAKE_BUILD_TYPE}${oxc3OutTail}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${oxc3OutRoot}/${CMAKE_BUILD_TYPE}${oxc3OutTail}/bin")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${oxc3OutRoot}/${CMAKE_BUILD_TYPE}${oxc3OutTail}/bin")
else()                                            # Multi-config generators (Visual Studio, Xcode)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${config} CONFIG)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG} "${oxc3OutRoot}/${config}${oxc3OutTail}/lib")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} "${oxc3OutRoot}/${config}${oxc3OutTail}/bin")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${oxc3OutRoot}/${config}${oxc3OutTail}/bin")
    endforeach()
endif()

# Enable symbols

if("${platform}" STREQUAL "linux")
	if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		set(CMAKE_ENABLE_EXPORTS ON)
	endif()
endif()

set(ForceUpdateDeps OFF CACHE BOOL "Force update dependencies")
set(ForceVulkan OFF CACHE BOOL "Force Vulkan support (disable native API if static linking)")

add_subdirectory(src)

# compile_commands.json lands in the build tree, but every editor looks for it beside the sources.
# Copied rather than symlinked, since a symlink on Windows needs Developer Mode or elevation.
# Plain copy rather than copy_if_different, so the output is always newer than its input and this does not
# re-run on every build.
# The generator writes the file at generate time, so it is only ever copied after a reconfigure.

if(CMAKE_EXPORT_COMPILE_COMMANDS AND EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")

	add_custom_command(
		OUTPUT "${CMAKE_SOURCE_DIR}/compile_commands.json"
		COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json"
		DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
		COMMENT "Exporting compile_commands.json"
		VERBATIM
	)

	add_custom_target(OxC3_compile_commands ALL DEPENDS "${CMAKE_SOURCE_DIR}/compile_commands.json")
	set_target_properties(OxC3_compile_commands PROPERTIES FOLDER Oxsomi/tools)

endif()
