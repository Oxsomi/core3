# Build policy for OxC3 targets, applied directly instead of through a linked policy target:
# a static library's private target dependency can still appear in its exported link interface.

set(OXC3_BUILD_COMPILE_OPTIONS "")
set(OXC3_BUILD_LINK_OPTIONS "")
set(OXC3_BUILD_STATIC_LIBRARY_OPTIONS "")

# emsdk 6.0.6 misfolds scalar I32x4_clmul64Fallback at -O3 (docs/bug_wasm64_o3_constfold.md); SIMD wasm is unaffected.

if(EMSCRIPTEN AND NOT EnableSIMD)
	list(APPEND OXC3_BUILD_COMPILE_OPTIONS "$<$<CONFIG:Release>:-O2>")
endif()

# clang-cl sets MSVC too, but has clang's warnings and lacks MSVC-only flags, so MSVC-specific options gate on this.

set(isClangCl FALSE)

if(MSVC AND CMAKE_C_COMPILER_ID MATCHES "Clang")
	set(isClangCl TRUE)
endif()

# Key off the compiler id: clang-cl sets MSVC and is the one Windows toolchain with usable sanitizers.

if((EnableASAN OR EnableUBSAN) AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
	message(FATAL_ERROR
		"EnableASAN/EnableUBSAN are only supported on clang and gcc. On Windows rebuild with "
		"-compiler clang (clang-cl), which keeps MSVC's ABI and CRT."
	)
endif()

# Instruction sets the x64 SIMD paths assume (F16C for flp.c, FMA for vec4f_sse.inc.h).
# clang and gcc gate each intrinsic behind a -m flag, so only spellings every driver accepts belong here:
# clang-cl treats -mfpmath=sse and -msse4 as unknown arguments, which /WX makes fatal.

set(X64_SIMD_FLAGS
	-msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2
	-mrdrnd -msha -maes -mpclmul -mbmi2 -mf16c -mavx -mfma
)

# Shared by the inline vector functions and the producer build; the math target publishes it to consumers.

set(OXC3_SIMD_FLAGS "")

if(EnableSIMD)

	if(EMSCRIPTEN)
		set(OXC3_SIMD_FLAGS -msimd128)
	elseif(ARM)
		if(isClangCl)
			set(OXC3_SIMD_FLAGS "/clang:-march=armv8-a+simd+crypto+crc")
		elseif(NOT MSVC)
			set(OXC3_SIMD_FLAGS -march=armv8-a+simd+crypto+crc)
		endif()
	elseif(MSVC AND NOT isClangCl)
		set(OXC3_SIMD_FLAGS /d2archSSE42)
	else()
		set(OXC3_SIMD_FLAGS ${X64_SIMD_FLAGS})
	endif()
endif()

if(MSVC)

	# /wd4201: anonymous structs/unions are standard C11 but a C++ extension, and OxC3 uses them throughout.
	set(BUILD_FLAGS "/MP" "/GR" "/D_CRT_SECURE_NO_WARNINGS" "/Gy" "/W4" "/WX" "/wd4201" "/fp:strict")

	list(APPEND BUILD_FLAGS ${OXC3_SIMD_FLAGS})

	if(isClangCl)

		# clang-cl defaults to -fms-compatibility, whose legacy preprocessor breaks the variadic generators
		# (sb_variable.c's EXPAND4x4, the vec swizzle expanders). It ignores /Zc:preprocessor.
		list(APPEND BUILD_FLAGS "-fno-ms-compatibility")

		# Same suppressions as the gcc/clang branch below; MSVC has no equivalent warnings.
		list(APPEND BUILD_FLAGS "-Wno-missing-field-initializers" "-Wno-unused-function")

		# clang-cl accepts but reports MSVC-only flags from elsewhere (including conan's toolchain) as unused, which /WX makes fatal.
		list(APPEND BUILD_FLAGS "-Wno-unused-command-line-argument")

		# CMake drives lld-link directly, so the /defaultlib directives clang-cl embeds for the sanitizer runtimes never reach it.
		# Point it at clang's own runtime directory.

		if(EnableASAN OR EnableUBSAN)

			get_filename_component(clangBinDir "${CMAKE_C_COMPILER}" DIRECTORY)
			file(GLOB clangRtDirs "${clangBinDir}/../lib/clang/*/lib/windows")

			if(NOT clangRtDirs)
				message(FATAL_ERROR "Sanitizer runtimes not found next to ${CMAKE_C_COMPILER}")
			endif()

			list(GET clangRtDirs 0 clangRtDir)
			link_directories("${clangRtDir}")
		endif()

		# /Oy- is clang-cl's frame pointer flag; -fno-omit-frame-pointer is rejected, fatally under /WX.
		# The dynamic runtime is used because dynamicLinkingShaderCompiler adds a DLL, and a static ASan in several modules doesn't work.
		# The annotation defines are required: MSVC's STL records its ASan annotation mode in an object directive, and lld-link rejects
		# the mix with the uninstrumented prebuilt dependencies (DXC, SPIRV-Reflect).

		# compiler-rt names its Windows runtimes after the target arch.

		if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
			set(sanitizerArch "aarch64")
		else()
			set(sanitizerArch "x86_64")
		endif()

		# Check the runtime exists for this arch now: a toolchain may not ship it, and the alternative is thousands of
		# unresolved __asan_* symbols at link.

		set(requiredSanitizerLibs "")

		if(EnableASAN)
			list(APPEND requiredSanitizerLibs
				"clang_rt.asan_dynamic-${sanitizerArch}.lib"
				"clang_rt.asan_dynamic_runtime_thunk-${sanitizerArch}.lib"

				# The DLL too, as running anything built needs it; it is copied next to the executables.

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

			# The dynamic ASan runtime must be findable when anything built runs (custom build steps, ctest). The loader only looks next
			# to the exe, and a custom step reports the failure as a bare 0xC0000135 that doesn't name the DLL.
			# Adding the directory reuses the copy apply_dependencies already does (cmake/oxc3.cmake).
			list(APPEND CONAN_RUNTIME_LIB_DIRS "${clangRtDir}")
		endif()

		# Identical COMDAT folding must be off for a sanitized link: Release folds identical globals (e.g. empty literals) across
		# TUs, so one global's redzone lands on another's data and legal reads are reported as overruns.

		if(EnableASAN OR EnableUBSAN)
			list(APPEND OXC3_BUILD_LINK_OPTIONS "-opt:noicf")
		endif()

		# Linking the standalone runtime so UBSan names the check and line instead of trapping.
		# vptr is off because it needs RTTI across the prebuilt dependencies. enum is off because flag enums and sentinels
		# (e.g. ESHExtension's OR'd bits and 1<<Count marker) are legal by design but not individual enumerators.

		if(EnableUBSAN)
			list(APPEND BUILD_FLAGS "-fsanitize=undefined" "-fno-sanitize=vptr,enum" "/Oy-")
			link_libraries("clang_rt.ubsan_standalone-${sanitizerArch}.lib")
		endif()
	endif()

	list(APPEND OXC3_BUILD_COMPILE_OPTIONS ${BUILD_FLAGS})

	# MSVC's legacy preprocessor mis-expands nested __VA_ARGS__ in vec4f_swizzle.h's generators. C17 mode implies the conformant one,
	# but C++ (the oxc:: wrappers) doesn't. gcc/clang are conformant; clang-cl gets this from -fno-ms-compatibility.
	if(NOT isClangCl)
		list(APPEND OXC3_BUILD_COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>")
	endif()

	# Whole-program optimization (/GL, /LTCG), Release only since /LTCG makes the link the bottleneck. lib.exe needs /LTCG told
	# separately or every archive warns (LNK4075/D9035). Targets setting OXC3_NO_WHOLE_PROGRAM opt out, as
	# WINDOWS_EXPORT_ALL_SYMBOLS can't parse /GL objects (overriding with /GL- warns D9025 on every TU).

	set(wholeProgram "$<AND:$<CONFIG:Release>,$<NOT:$<BOOL:$<TARGET_PROPERTY:OXC3_NO_WHOLE_PROGRAM>>>>")

	if(isClangCl)

		# ThinLTO stays off: the compile uses whichever clang-cl CMake found while the link uses Visual Studio's lld-link,
		# and bitcode isn't readable across LLVM major versions ("is not a native COFF file"). Pin both to one LLVM first.

	else()
		list(APPEND OXC3_BUILD_COMPILE_OPTIONS "$<$<AND:${wholeProgram},$<COMPILE_LANGUAGE:C,CXX>>:/GL>")
		list(APPEND OXC3_BUILD_LINK_OPTIONS "$<${wholeProgram}:/LTCG>")
		list(APPEND OXC3_BUILD_STATIC_LIBRARY_OPTIONS "$<${wholeProgram}:/LTCG>")
	endif()

else()

	list(APPEND OXC3_BUILD_COMPILE_OPTIONS ${OXC3_SIMD_FLAGS})

	if(NOT EMSCRIPTEN)

		if(ARM)
			if(NOT EnableSIMD)
				list(APPEND OXC3_BUILD_COMPILE_OPTIONS -march=armv8-a)
			endif()
		else()

			list(APPEND OXC3_BUILD_COMPILE_OPTIONS -mfpmath=sse)

			if(NOT EnableSIMD)
				list(APPEND OXC3_BUILD_COMPILE_OPTIONS -msse)
			endif()
		endif()
	endif()

	list(APPEND OXC3_BUILD_COMPILE_OPTIONS -fno-unsafe-math-optimizations)
	list(APPEND OXC3_BUILD_COMPILE_OPTIONS -fms-extensions)
	list(APPEND OXC3_BUILD_COMPILE_OPTIONS -Wall -Wextra -Werror)

	# LTO for optimized configs only, since it makes the link the bottleneck in Debug.
	# clang's bitcode needs lld: GNU ld reads it through a system LLVMgold.so that can be another major version
	# ("Unknown attribute kind ... Producer: LLVM 18, Reader: LLVM 17"), leaving every archive looking empty.
	# So use lld, and drop LTO when it isn't installed. gcc, apple and android already link with a matching linker.

	set(useLTO TRUE)

	if(EMSCRIPTEN)

		# Not emscripten: bitcode archives would force every consumer link through single threaded wasm-ld LTO
		# (DXC alone is ~500 MB) for marginal gain.

		set(useLTO FALSE)

	elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT APPLE AND NOT ANDROID)

		find_program(OXC3_LLD NAMES ld.lld lld)

		if(OXC3_LLD)
			list(APPEND OXC3_BUILD_LINK_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:-fuse-ld=lld>")
		else()
			message("-- LTO disabled: clang needs lld to link its own bitcode and ld.lld wasn't found")
			set(useLTO FALSE)
		endif()

	endif()

	if(useLTO)
		list(APPEND OXC3_BUILD_COMPILE_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:-flto=auto>")
		list(APPEND OXC3_BUILD_LINK_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:-flto=auto>")
	endif()

	# `{ 0 }` is the idiomatic zero-initializer, but -Wextra flags it when the C headers compile as C++ (oxc::c) where `{}` isn't valid C.
	list(APPEND OXC3_BUILD_COMPILE_OPTIONS -Wno-missing-field-initializers)

	# TList(T) expands to many static inline accessors; clang reports the unused ones under -Wunused-function when it is
	# expanded in a .c. Kept on for gcc, which still catches dead static functions.
	if(CMAKE_C_COMPILER_ID MATCHES "Clang")
		list(APPEND OXC3_BUILD_COMPILE_OPTIONS -Wno-unused-function)
	endif()

	# GCC 14.1/14.2 x64 struct-copy workaround, applied only to each affected language compiler.

	if(NOT ARM AND NOT EMSCRIPTEN)

		foreach(language C CXX)

			if (
				CMAKE_${language}_COMPILER_ID STREQUAL "GNU" AND
				CMAKE_${language}_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1" AND
				CMAKE_${language}_COMPILER_VERSION VERSION_LESS "14.3"
			)
				list(APPEND OXC3_BUILD_COMPILE_OPTIONS
					"$<$<COMPILE_LANGUAGE:${language}>:-mprefer-vector-width=128>"
				)
			endif()
		endforeach()
	endif()

	# Here the compiler driver does the link, so the same flag goes to both halves and pulls in its runtime.

	if(EnableASAN)

		list(APPEND OXC3_BUILD_COMPILE_OPTIONS -fsanitize=address -fno-omit-frame-pointer)
		list(APPEND OXC3_BUILD_LINK_OPTIONS -fsanitize=address)

		# The runtime must be the SHARED one: linked statically it is searched before LD_PRELOAD, so the DEEPBIND shim below
		# can't win the dlopen interception. gcc already does this; clang adds no rpath and -print-runtime-dir isn't reliable,
		# so take the path from the runtime itself. Linux only, where the shim is built.

		if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")

			execute_process(
				COMMAND ${CMAKE_C_COMPILER} -print-file-name=libclang_rt.asan-${CMAKE_SYSTEM_PROCESSOR}.so
				OUTPUT_VARIABLE asanRuntime
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)

			if(NOT IS_ABSOLUTE "${asanRuntime}" OR NOT EXISTS "${asanRuntime}")
				message(FATAL_ERROR
					"EnableASAN needs the shared runtime libclang_rt.asan-${CMAKE_SYSTEM_PROCESSOR}.so, which "
					"this toolchain doesn't ship. Without it the RTLD_DEEPBIND shim is inert and a sanitized "
					"run silently covers only the software adapters."
				)
			endif()

			get_filename_component(asanRuntimeDir "${asanRuntime}" DIRECTORY)
			list(APPEND OXC3_BUILD_LINK_OPTIONS -shared-libasan "-Wl,-rpath,${asanRuntimeDir}")
		endif()
	endif()

	# Proprietary drivers fail vkCreateDevice under a sanitizer for two reasons: they map memory inside the shadow gap and
	# load their compiler with RTLD_DEEPBIND. Both fixes are needed (one alone is intermittent), and they live here so
	# running ctest directly behaves the same.

	if(EnableASAN AND CMAKE_SYSTEM_NAME STREQUAL "Linux")

		add_library(OxC3_asan_no_deepbind MODULE cmake/linux/asan_no_deepbind.c)
		set_target_properties(OxC3_asan_no_deepbind PROPERTIES FOLDER Oxsomi/test)

		# Uninstrumented: preloaded ahead of the sanitizer runtime so it wins the dlopen interception.

		target_compile_options(OxC3_asan_no_deepbind PRIVATE -fno-sanitize=all)
		target_link_options(OxC3_asan_no_deepbind PRIVATE -fno-sanitize=all)
	endif()

	# vptr needs RTTI in the prebuilt dependencies. enum is off for the flag enum idiom (see the clang-cl branch).
	# function stays on: callbacks take void* and cast internally, so indirect calls match.

	if(EnableUBSAN)
		list(APPEND OXC3_BUILD_COMPILE_OPTIONS -fsanitize=undefined -fno-sanitize=vptr,enum -fno-omit-frame-pointer)
		list(APPEND OXC3_BUILD_LINK_OPTIONS -fsanitize=undefined)
	endif()

endif()

# Called after src declares its targets. BEFORE keeps directory defaults ahead of local overrides (e.g. the preload helper).

function(oxc3_apply_build_options directory)

	get_property(buildTargets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)

	foreach(buildTarget IN LISTS buildTargets)

		if(NOT buildTarget MATCHES "^OxC3")
			continue()
		endif()

		get_target_property(buildType ${buildTarget} TYPE)

		if(NOT buildType MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
			continue()
		endif()

		get_target_property(applied ${buildTarget} OXC3_BUILD_OPTIONS_APPLIED)

		if(applied)
			continue()
		endif()

		target_compile_options(${buildTarget} BEFORE PRIVATE
			"$<$<COMPILE_LANGUAGE:C,CXX>:${OXC3_BUILD_COMPILE_OPTIONS}>"
		)

		target_link_options(${buildTarget} BEFORE PRIVATE ${OXC3_BUILD_LINK_OPTIONS})

		if(buildType STREQUAL "STATIC_LIBRARY" AND OXC3_BUILD_STATIC_LIBRARY_OPTIONS)

			get_target_property(archiveOptions ${buildTarget} STATIC_LIBRARY_OPTIONS)

			if(NOT archiveOptions)
				set(archiveOptions "")
			endif()

			set_property(TARGET ${buildTarget} PROPERTY STATIC_LIBRARY_OPTIONS
				${OXC3_BUILD_STATIC_LIBRARY_OPTIONS} ${archiveOptions}
			)
		endif()

		set_property(TARGET ${buildTarget} PROPERTY OXC3_BUILD_OPTIONS_APPLIED TRUE)
	endforeach()

	get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)

	foreach(subdirectory IN LISTS subdirectories)
		oxc3_apply_build_options("${subdirectory}")
	endforeach()
endfunction()
