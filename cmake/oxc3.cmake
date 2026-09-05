# oxc3's vector headers generate their swizzles with nested __VA_ARGS__ macros,
# F32x4_expand -> expand2 -> expand3 -> expand4 in vec4f_swizzle.h, which MSVC's LEGACY preprocessor
# mis-expands into a wall of syntax errors.
# C never trips it, since C17 mode already implies the conformant preprocessor, but C++ still defaults to
# the legacy one, so EVERY consumer TU that reaches an oxc:: header fails to compile without this.
# That makes it a property of the headers rather than a choice each consumer should have to rediscover,
# hence riding on the imported target. core3's own build sets the same flag for its C++ targets.
# clang-cl ignores /Zc:preprocessor entirely and is conformant regardless, hence the compiler id check.

if(TARGET oxc3::oxc3 AND MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set_property(
		TARGET oxc3::oxc3 APPEND PROPERTY
		INTERFACE_COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>"
	)
endif()

# Setting the icon of the app
# Call this immediately before apply_dependencies with the executable

function(configure_icon target icon)

	if(NOT TARGET ${target})
		message(FATAL_ERROR "configure_icon: target ${target} not present.")
	endif()

	if(WIN32)
		get_property(res TARGET ${target} PROPERTY RESOURCE_LIST_RC)
		set_property(TARGET ${target} PROPERTY RESOURCE_LIST_RC LOGO\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ICON\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \"${icon}\"\n${res})
		target_sources(${target} PRIVATE ${icon})
	endif()

endfunction()

# Link options every wasm64 executable needs.
# Both of the web targets (the CLI and the test bundle) are node programs, so they take the same set; a
# browser consumer would embed the libraries and choose its own -sENVIRONMENT instead.
#
# NODERAWFS mounts the real filesystem, so packages/ resolves through plain POSIX paths from the working
# directory rather than needing anything preloaded.
# The memory and stack settings are hard requirements: DXC recursion overflows the 64KB default stack
# silently, and 2GB is the default memory cap.
# INITIAL_MEMORY is what the module commits at instantiation, before it does any work, so it stays small
# and ALLOW_MEMORY_GROWTH sizes it to the workload; a phone should not hand over half a gigabyte just to
# load the module. DXC grows this a lot while compiling, but only for the run that needs it.
# ASan starts bigger: wasm-ld refuses to link unless the data segment plus stack fit inside INITIAL_MEMORY,
# and the global redzones grow DXC's statics to ~50MB, past what the unsanitized figure holds.
# The shadow region is no part of this figure; emcc adds an eighth of MAXIMUM_MEMORY on top of
# INITIAL_MEMORY by itself.

function(apply_web_link_options target)

	if(NOT TARGET ${target})
		message(FATAL_ERROR "apply_web_link_options: target ${target} not present.")
	endif()

	if(NOT EMSCRIPTEN)
		return()
	endif()

	if(EnableASAN)
		set(initialMemory 128MB)
	else()
		set(initialMemory 32MB)
	endif()

	target_link_options(${target} PRIVATE
		"-sENVIRONMENT=node"
		"-sNODERAWFS=1"
		"-sEXIT_RUNTIME=1"
		"-sALLOW_MEMORY_GROWTH=1"
		"-sINITIAL_MEMORY=${initialMemory}"
		"-sMAXIMUM_MEMORY=16GB"
		"-sSTACK_SIZE=8MB"
	)

	# ENVIRONMENT stays 'node' for the threaded flavor too: emscripten appends 'worker' itself once
	# shared memory is on. A browser flavor would need web,worker and no NODERAWFS instead.
	# A worker inherits STACK_SIZE otherwise, so every thread would reserve the main thread's 8 MB out
	# of a heap that starts at 32 MB. Sized down explicitly, and it is a starting point rather than a
	# measured one: DXC recurses deeply, so raise it if a threaded shader compile overflows.

	if(CMAKE_C_FLAGS MATCHES "-pthread")
		target_link_options(${target} PRIVATE "-sDEFAULT_PTHREAD_STACK_SIZE=4MB")
	endif()

endfunction()

# One module holding every unit test suite, for a platform that builds no per-suite executables
# (OxC3TestExecutables off, see the root CMakeLists for which platforms those are and why).
# Android has no exec at all, and web would otherwise link a separate wasm module per suite,
# so both compile the same test sources a second time with _OXC3_TEST_BUNDLED,
# which turns each suite's main() into a named entry (see OXC3_TEST_MAIN / OXC3_TEST_ENTRY in types/test/test.h)
# that the platform's own entry point calls in turn.
# Compiling them twice is deliberate: every module's own test target stays untouched,
# so a bundle can't affect the host build.
#
#   oxc3_add_bundled_test(
#       TARGET      OxC3_wtest          # also the virtual file system namespace the suites read their data from
#       ROOT        ${testRoot}         # src, the directory the shared suites are globbed out of
#       SELF        ${testRoot}/..      # repository root, the packages are written under its build/<config>/<platform>
#       ENTRY       wtest_main.c        # entry point calling the suites, relative to the caller
#       [SHARED]                        # a library the platform loads, instead of an executable it runs
#       [SOURCES    <files...>]         # suites this platform bundles on top of the shared set
#       [LIBS       <targets...>]       # the libraries those extra suites test
#   )
#
# The caller keeps what differs per platform: link options, how the module is launched, and any further data
# it packages.
# apply_dependencies stays with the caller too, since it reads the resource list and so has to run after the
# last add_virtual_files that caller adds.

function(oxc3_add_bundled_test)

	set(options SHARED)
	set(oneValue TARGET ROOT SELF ENTRY)
	set(multiValue SOURCES LIBS)
	cmake_parse_arguments(T "${options}" "${oneValue}" "${multiValue}" ${ARGN})

	# A misspelled keyword otherwise vanishes and the function silently does less than the caller asked.
	# A keyword given with no value is indistinguishable from not passing it at all.

	if(T_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "oxc3_add_bundled_test: unrecognized arguments: ${T_UNPARSED_ARGUMENTS}")
	endif()

	if(T_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "oxc3_add_bundled_test: arguments missing a value: ${T_KEYWORDS_MISSING_VALUES}")
	endif()

	if(NOT T_TARGET)
		message(FATAL_ERROR "oxc3_add_bundled_test: 'TARGET' argument required.")
	endif()

	if(TARGET ${T_TARGET})
		message(FATAL_ERROR "oxc3_add_bundled_test: target ${T_TARGET} already present.")
	endif()

	if(NOT T_ROOT)
		message(FATAL_ERROR "oxc3_add_bundled_test: 'ROOT' argument required.")
	endif()

	if(NOT IS_DIRECTORY ${T_ROOT})
		message(FATAL_ERROR "oxc3_add_bundled_test: 'ROOT' folder not present.")
	endif()

	if(NOT T_SELF)
		message(FATAL_ERROR "oxc3_add_bundled_test: 'SELF' argument required.")
	endif()

	if(NOT T_ENTRY)
		message(FATAL_ERROR "oxc3_add_bundled_test: 'ENTRY' argument required.")
	endif()

	# The entry point lives next to the CMakeLists that bundles it, so a relative ENTRY resolves against the
	# caller rather than against this file.

	get_filename_component(entry "${T_ENTRY}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

	if(NOT EXISTS "${entry}")
		message(FATAL_ERROR "oxc3_add_bundled_test: 'ENTRY' file not present.")
	endif()

	# The suites every bundle takes.
	# A suite that needs a GPU or a human at the device is a per platform decision,
	# so it comes in through SOURCES instead.

	file(GLOB_RECURSE bundledSources CONFIGURE_DEPENDS
		"${T_ROOT}/types/base/test/*.c"
		"${T_ROOT}/types/base/test/*.cpp"
		"${T_ROOT}/types/math/test/*.c"
		"${T_ROOT}/types/math/test/*.cpp"
		"${T_ROOT}/types/container/test/*.c"
		"${T_ROOT}/types/container/test/*.cpp"
		"${T_ROOT}/formats/*/test/*.c"
		"${T_ROOT}/formats/*/test/*.cpp"
		"${T_ROOT}/audio/test/interface/*.c"
		"${T_ROOT}/platforms/test/interface/*.c"
		"${T_ROOT}/platforms/test/interface/*.cpp"
	)

	# CMakeLists.txt is a source so the bundle stays editable from the IDE, like every other test target.

	if(T_SHARED)
		add_library(${T_TARGET} SHARED ${bundledSources} ${T_SOURCES} "${entry}" CMakeLists.txt)
	else()
		add_executable(${T_TARGET} ${bundledSources} ${T_SOURCES} "${entry}" CMakeLists.txt)
	endif()

	# _OXC3_TEST_VFS_TARGET: platforms_interface reaches its test data through the virtual file system, and a
	# section is namespaced by the target that packaged it.
	# add_virtual_files below registers it under the bundle, not under OxC3_plinttst as the desktop build
	# does, so the suite is told which name to expect.

	target_compile_definitions(${T_TARGET} PRIVATE _OXC3_TEST_BUNDLED _OXC3_TEST_VFS_TARGET="${T_TARGET}")

	# The formats list has to cover EVERY formats/*/test directory the glob above picks up, since bundling a
	# suite compiles its sources but nothing else pulls in the library it tests.
	# A format whose tests are globbed but whose library is missing here only shows up as undefined symbols
	# at link, and only on a bundling platform, since every other platform links each suite against its own
	# module.

	target_link_libraries(${T_TARGET} PUBLIC
		OxC3_platforms
		OxC3_audio
		OxC3_formats_bmp OxC3_formats_dds OxC3_formats_hdr
		OxC3_formats_oiBC OxC3_formats_oiCA OxC3_formats_oiDL OxC3_formats_oiSB
		OxC3_formats_oiSH OxC3_formats_oiPL OxC3_formats_oiSP OxC3_formats_oiSR OxC3_formats_wav
		OxC3_types_test
		OxC3_types_container_test_util
		${T_LIBS}
	)

	# Test_dynamicLibrary dlopens this by bare name, and nothing links against it,
	# so a dependency is all that ties it to the bundle.

	add_dependencies(${T_TARGET} OxC3_platforms_interface_dylib_test)

	# platforms_interface reads these through the virtual file system, same as OxC3_plinttst does

	add_virtual_files(
		TARGET  ${T_TARGET}
		NAME    testdata
		ROOT    ${T_ROOT}/platforms/test/interface/res
		SELF    ${T_SELF}
		FORCE_PACKAGER
	)

	# shader_compiler reads its HLSL off disk when ctest runs it from src/shader_compiler/test, a working
	# directory a bundle doesn't have, so the same tree is packaged and the suite reads it back through
	# //<target>/shaderdata (TEST_SHADER_ROOT in shader_compiler/test/test_shader_compiler_shared.h resolves
	# there whenever the suites are bundled).
	# A bundled build MUST package this or every compileFileShader() call fails to read its source.
	# -raw is what keeps it readable: the packager otherwise compiles .hlsl into oiSH, and these tests want
	# the source (they drive the compiler themselves).
	# The .c/.h next to it aren't staged, since add_virtual_files packages a whole directory.

	if(EnableShaderCompiler)

		set(shaderRes ${CMAKE_CURRENT_BINARY_DIR}/shaderdata)

		file(GLOB_RECURSE shaderTestData CONFIGURE_DEPENDS
			"${T_ROOT}/shader_compiler/test/*/*.hlsl"
			"${T_ROOT}/shader_compiler/test/*/*.hlsli"
			"${T_ROOT}/shader_compiler/test/*.oiSH"
			"${T_ROOT}/shader_compiler/test/*.oiSR"
		)

		foreach(f ${shaderTestData})
			file(RELATIVE_PATH rel "${T_ROOT}/shader_compiler/test" "${f}")
			get_filename_component(relDir "${rel}" DIRECTORY)
			file(COPY "${f}" DESTINATION "${shaderRes}/${relDir}")
		endforeach()

		add_virtual_files(
			TARGET  ${T_TARGET}
			NAME    shaderdata
			ROOT    ${shaderRes}
			SELF    ${T_SELF}
			FORCE_PACKAGER
			ARGS    -raw
		)

	endif()

	set_target_properties(${T_TARGET} PROPERTIES FOLDER Oxsomi/test)

endfunction()

function(apply_dependencies target)

	if(NOT TARGET ${target})
		message(FATAL_ERROR "apply_dependencies: target ${target} not present.")
	endif()

	install(RUNTIME_DEPENDENCY_SET ${target}
		PRE_EXCLUDE_REGEXES
			[[api-ms-win-.*]]
			[[ext-ms-.*]]
			[[kernel32\.dll]]
			[[libc\.so\..*]] [[libgcc_s\.so\..*]] [[libm\.so\..*]] [[libstdc\+\+\.so\..*]]
		POST_EXCLUDE_REGEXES
			[[.*/system32/.*\.dll]]
			[[^/lib.*]]
			[[^/usr/lib.*]]
		DIRECTORIES ${CONAN_RUNTIME_LIB_DIRS}
	)
	
	add_custom_command(
		TARGET ${target} POST_BUILD
		COMMAND ${CMAKE_COMMAND}
			-D "DESTDIR=$<TARGET_FILE_DIR:${target}>"
			-D "LIBDIRS=\"${CONAN_RUNTIME_LIB_DIRS}\""
			-P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/copy_dlls.cmake"
	)
	
	if(ANDROID)
		target_sources(${target} PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
		target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include/platforms/android/aoxc3_activity_glue.c")
		target_include_directories(${target} PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")
		set_source_files_properties("${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c" PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter)
	endif()

	# Ensure that working directory is set to the same place as the exe to ensure it can find .dll/.so
	set_target_properties(${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")

	get_property(res TARGET ${target} PROPERTY RESOURCE_LIST)

	set(_objcopySections)

	foreach(file ${res})

		string(REPLACE "\\" "/" file "${file}")

		string(FIND "${file}" "packages/" PACKAGE_POS)
		string(SUBSTRING "${file}" ${PACKAGE_POS} -1 RESULT)
		string(SUBSTRING "${RESULT}" 9 -1 FINAL_RESULT)

		string(REGEX REPLACE "\\.oiCA$" "" RELATIVE_PATH "${FINAL_RESULT}")

		string(REPLACE "/" ";" PARTS ${RELATIVE_PATH})
		list(LENGTH PARTS PART_COUNT)

		if(NOT PART_COUNT EQUAL 2)
			message(FATAL_ERROR "Package file is expected to be packages/target/file")
		endif()

		foreach(PART IN LISTS PARTS)

			string(LENGTH "${PART}" PART_LEN)

			if(PART_LEN GREATER 15)
				message(FATAL_ERROR "Package name and target name is limited to 15 characters")
			endif()

		endforeach()

		list(GET PARTS 0 TARGET_OF_PACKAGE)
		list(GET PARTS 1 PACKAGE_NAME)

		# Differences in packaging:
		# Windows you can embed using an .rc file; then this handle can be opened through FindResourceW
		# Linux you can embed into the elf manually by using objcopy and manually read the section data
		# to find where it's located
		# Android has APKs which are just like zip files, so can be easily read (though the NDK can't access subfolders easily)
		# iOS has IPA which is the same idea as APK.
		# OS X asks the linker for the section with -sectcreate, so it needs no external tool at all
		# web/emscripten has a virtual filesystem, so nothing is embedded into the module at all.
		# The android model applies instead: packages/<target>/<name>.oiCA is read at runtime.
		# node reaches it through NODERAWFS; a browser build would have to populate MEMFS itself first,
		# which no target does yet (see src/platforms/web).
		
		if(WIN32)
			get_property(res2 TARGET ${target} PROPERTY RESOURCE_LIST_RC)
			set_property(TARGET ${target} PROPERTY RESOURCE_LIST_RC ${RELATIVE_PATH}\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ RCDATA\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \"${file}\"\n${res2})
		elseif(APPLE)

			# Embedded by the linker rather than by patching the binary afterwards.
			# llvm-objcopy --add-section on Mach-O rewrites the load commands and leaves __LINKEDIT at a
			# file offset dyld refuses to load ("segment '__LINKEDIT' file offset out of order"), whether it
			# adds one section or several. -sectcreate is the supported way in: the linker lays the segment
			# out itself, so the result is well formed by construction.
			# oplatform.c reads whatever it finds, it only cares that the segment name starts with @, so the
			# naming (and the 15 character cap that keeps segname within Mach-O's 16) is unchanged.

			target_link_options(${target} PRIVATE
				"LINKER:-sectcreate,@${TARGET_OF_PACKAGE},${PACKAGE_NAME},${file}"
			)

			# The linker only reruns when an input changes, and a .oiCA named in a flag isn't one.

			set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${file}")

		elseif(UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)
			list(APPEND _objcopySections --add-section "packages/${RELATIVE_PATH}=${file}")
		endif()

	endforeach()

	# ELF only; macOS embeds at link time above.
	# Still one objcopy for all sections rather than one per package: each invocation rewrites the whole
	# binary, so per-file commands cost a full copy each and give the tool more chances to get it wrong.

	if(_objcopySections)
		add_custom_command(
			TARGET ${target} POST_BUILD
			COMMAND objcopy ${_objcopySections} "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:${target}>" "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:${target}>"
		)
	endif()

	if(WIN32)
		get_property(res2 TARGET ${target} PROPERTY RESOURCE_LIST_RC)
		if(NOT "${res2}" STREQUAL "")
			set(_rc_file "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${target}.rc")
			file(GENERATE
				OUTPUT "${_rc_file}"
				CONTENT "${res2}"
			)
			set_source_files_properties("${_rc_file}" PROPERTIES GENERATED TRUE)

			# The .rc only names the .oiCA files, so its own text is identical no matter what's inside them.
			# Without this the resource compiler isn't rerun when a package is rebuilt and the binary silently keeps
			#  embedding the previous archive, surfacing much later as a missing shader binary nowhere near the build.
			# The other platforms don't need it, they embed through a POST_BUILD objcopy that runs every build.

			set_source_files_properties("${_rc_file}" PROPERTIES OBJECT_DEPENDS "${res}")

			target_sources(${target} PRIVATE "${_rc_file}")
		endif()
	endif()

endfunction()

# Add virtual directory as a loadable section.
# Example:
# add_virtual_files(
#	TARGET
#		myTarget
#	NAME
#		shaders
#	ROOT
#		${CMAKE_CURRENT_SOURCE_DIR}/res/test_shaders
#	SELF
#		${CMAKE_CURRENT_SOURCE_DIR}
#	ARGS
#		-threads "100%%" -compile-type dxil	# Compile shaders using all available threads and only output DXIL, %% = escape % on windows
# )
# This would add myTarget/shaders as a virtual directory.
# myTarget/shaders has to be loaded manually by the app to process it.
# This is then passed onto to OxC3 package to ensure all files are converted to the right types.
# This effectively means it'd be packaged as a separate .oiCA file that has to be loaded via File_loadVirtual.

macro(add_virtual_files)

	set(_OPTIONS FORCE_PACKAGER)
	set(_ONE_VALUE TARGET ROOT NAME SELF)
	set(_MULTI_VALUE ARGS)

	cmake_parse_arguments(_ARGS "${_OPTIONS}" "${_ONE_VALUE}" "${_MULTI_VALUE}" ${ARGN})

	# A misspelled keyword otherwise vanishes and the function silently does less than the caller asked.
	# A keyword given with no value is indistinguishable from not passing it at all.

	if(_ARGS_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "add_virtual_files: unrecognized arguments: ${_ARGS_UNPARSED_ARGUMENTS}")
	endif()

	if(_ARGS_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "add_virtual_files: arguments missing a value: ${_ARGS_KEYWORDS_MISSING_VALUES}")
	endif()

	# Validate

	if(NOT TARGET ${_ARGS_TARGET})
		message(FATAL_ERROR "add_virtual_files: target ${_ARGS_TARGET} not present.")
	endif()

	if(NOT _ARGS_ROOT)
		message(FATAL_ERROR "add_virtual_files: 'ROOT' argument required.")
	endif()

	if(NOT IS_DIRECTORY ${_ARGS_ROOT})
		message(FATAL_ERROR "add_virtual_files: 'ROOT' folder not present.")
	endif()

	if(NOT _ARGS_NAME)
		message(FATAL_ERROR "add_virtual_files: 'NAME' argument required.")
	endif()

	if(NOT _ARGS_SELF)
		message(FATAL_ERROR "add_virtual_files: 'SELF' argument required.")
	endif()

	if(NOT _ARGS_NAME MATCHES "^[0-9A-Za-z_\$]+$")
		message(FATAL_ERROR "add_virtual_files: 'NAME' has to conform to alphanumeric (plus _ and $).")
	endif()

	if(NOT _ARGS_TARGET MATCHES "^[0-9A-Za-z_\$]+$")
		message(FATAL_ERROR "add_virtual_files: 'TARGET' has to conform to alphanumeric (plus _ and $).")
	endif()

	string(TOUPPER ${_ARGS_TARGET} ANGRY_TARGET)

	if(ANGRY_TARGET MATCHES "ACCESS" OR ANGRY_TARGET MATCHES "FUNCTION" OR ANGRY_TARGET MATCHES "NETWORK")
		message(FATAL_ERROR "add_virtual_files: 'TARGET' can't be 'access' or 'function' or 'network'.")
	endif()

	# Add processed file as a file

	# Packaging can be done through the following ways:
	# OxC3_package_simple; Only processes using dependencies below platforms (formats, types).
	#						This excludes shader compilation and graphics dependencies.
	# OxC3_package; Full process to package a file (including shader compilation, not graphics).
	# OxC3; Full executable with all functionality.
	
	# When cross compiling, an in-build packager target can't run on the build machine
	# (android and web don't build the executable at all), so prefer the host
	# tool from tool_requires (on PATH); see conanfile.build_requirements().

	if(_ARGS_FORCE_PACKAGER)
		if(TARGET OxC3_package AND NOT CMAKE_CROSSCOMPILING)
			set(OXC3_PACKAGE OxC3_package)
		elseif(TARGET OxC3_package_simple AND NOT CMAKE_CROSSCOMPILING)
			set(OXC3_PACKAGE OxC3_package_simple)
		else()
			find_program(OXC3_PACKAGE OxC3_package REQUIRED)
		endif()
	else()
		if(NOT TARGET OxC3 OR CMAKE_CROSSCOMPILING)
			find_program(OXC3 OxC3 REQUIRED)
		else()
			set(OXC3 OxC3)
		endif()
	endif()
	
	if(WIN32)
		set(platform windows)
	elseif(IOS)
		set(platform ios)
	elseif(APPLE)
		set(platform osx)
	elseif(ANDROID)
		set(platform android)
	elseif(EMSCRIPTEN)
		set(platform web)
	else()
		set(platform linux)
	endif()

		
	if(NOT CMAKE_CONFIGURATION_TYPES)
		set(RuntimeOutputDir "${_ARGS_SELF}/build/${platform}")
	else()
		set(RuntimeOutputDir "${_ARGS_SELF}/build/$<CONFIG>/${platform}")
	endif()

	if(_ARGS_FORCE_PACKAGER)

		set(OxC3_command "${OXC3_PACKAGE} \"${_ARGS_ROOT}\" \"${RuntimeOutputDir}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA\"")
	
		add_custom_target(
			${_ARGS_TARGET}_package_${_ARGS_NAME}
			COMMAND ${OXC3_PACKAGE} \"${_ARGS_ROOT}\" \"${RuntimeOutputDir}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA\" ${_ARGS_ARGS}
			WORKING_DIRECTORY ${_ARGS_SELF}
		)

	else()

		set(OxC3_command "${OXC3} file package -input \"${_ARGS_ROOT}\" -output \"${RuntimeOutputDir}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA\"")
	
		add_custom_target(
			${_ARGS_TARGET}_package_${_ARGS_NAME}
			COMMAND ${OXC3} file package -input \"${_ARGS_ROOT}\" -output \"${RuntimeOutputDir}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA\" ${_ARGS_ARGS}
			WORKING_DIRECTORY ${_ARGS_SELF}
		)

	endif()
		
	string (REPLACE ";" " " ARGS_STR "${_ARGS_ARGS}")
	message("-- Packaging: ${OxC3_command} ${ARGS_STR}")
	message("-- Packaging: ${_ARGS_TARGET}_package_${_ARGS_NAME} @ ${_ARGS_SELF}")

	set_target_properties(${_ARGS_TARGET}_package_${_ARGS_NAME} PROPERTIES FOLDER Oxsomi/package)

	# When adding from external package manager, it's already been installed

	if(_ARGS_FORCE_PACKAGER AND TARGET OxC3_package)
		add_dependencies(${_ARGS_TARGET} ${_ARGS_TARGET}_package_${_ARGS_NAME} OxC3_package)
	elseif(TARGET OxC3)
		add_dependencies(${_ARGS_TARGET} ${_ARGS_TARGET}_package_${_ARGS_NAME} OxC3)
	else()
		add_dependencies(${_ARGS_TARGET} ${_ARGS_TARGET}_package_${_ARGS_NAME})
	endif()

	get_property(res TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST)
	set_property(TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST ${RuntimeOutputDir}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA;${res})

endmacro()

# Add a dependency to ensure the dependency files for a project are present.
# This is useful if a dependency would need to include things like fonts, or other kinds of resources.
# This would for example allow you to load myTarget/shaders in myNewTarget (from the other example).
# These dependencies are always public, so if myTarget would have a dependency, this one does too.
# NOTE: This only works for projects visible to cmake,
#       but for projects managed with conan you should use add_virtual_dependencies_external
# Example:
# add_virtual_dependencies(TARGET myNewTarget DEPENDENCIES myTarget)

macro(add_virtual_dependencies)

	set(_OPTIONS)
	set(_ONE_VALUE TARGET)
	set(_MULTI_VALUE DEPENDENCIES)

	cmake_parse_arguments(_ARGS "${_OPTIONS}" "${_ONE_VALUE}" "${_MULTI_VALUE}" ${ARGN})

	# A misspelled keyword otherwise vanishes and the function silently does less than the caller asked.
	# A keyword given with no value is indistinguishable from not passing it at all.

	if(_ARGS_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "add_virtual_dependencies: unrecognized arguments: ${_ARGS_UNPARSED_ARGUMENTS}")
	endif()

	if(_ARGS_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "add_virtual_dependencies: arguments missing a value: ${_ARGS_KEYWORDS_MISSING_VALUES}")
	endif()

	if(NOT TARGET ${_ARGS_TARGET})
		message(FATAL_ERROR "add_virtual_dependencies: target \"${_ARGS_TARGET}\" not present.")
	endif()

	# Add dependencies

	if(_ARGS_DEPENDENCIES)
		foreach(file ${_ARGS_DEPENDENCIES})

			if(NOT TARGET ${file})
				message(FATAL_ERROR "add_virtual_dependencies: target \"${file}\" not present.")
			endif()

			get_property(res0 TARGET ${file} PROPERTY RESOURCE_LIST)
			get_property(res1 TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST)

			add_dependencies(${_ARGS_TARGET} ${file})
			set_property(TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST ${res0};${res1})

		endforeach()
	else()
		message(FATAL_ERROR "add_virtual_dependencies: DEPENDENCIES argument is required!")
	endif()

endmacro()

# See add_virtual_dependencies, this is the same, except "DEPENDENCIES" is now the programs fetched from conan.
# For example: add_virtual_dependencies_external(TARGET rt_core PROGRAMS OxC3)

macro(add_virtual_dependencies_external)
	
	set(_OPTIONS)
	set(_ONE_VALUE TARGET)
	set(_MULTI_VALUE DEPENDENCIES)

	cmake_parse_arguments(_ARGS "${_OPTIONS}" "${_ONE_VALUE}" "${_MULTI_VALUE}" ${ARGN})

	# A misspelled keyword otherwise vanishes and the function silently does less than the caller asked.
	# A keyword given with no value is indistinguishable from not passing it at all.

	if(_ARGS_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "add_virtual_dependencies_external: unrecognized arguments: ${_ARGS_UNPARSED_ARGUMENTS}")
	endif()

	if(_ARGS_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "add_virtual_dependencies_external: arguments missing a value: ${_ARGS_KEYWORDS_MISSING_VALUES}")
	endif()

	if(NOT TARGET ${_ARGS_TARGET})
		message(FATAL_ERROR "add_virtual_dependencies_external: target \"${_ARGS_TARGET}\" not present.")
	endif()
	
	if(_ARGS_DEPENDENCIES)
		foreach(file ${_ARGS_DEPENDENCIES})

			# Get bin folder from our conan package
		
			# find_package(${file} REQUIRED)

			# set(INCLUDE_DIRS_VAR "${file}_INCLUDE_DIRS")

			# if(DEFINED ${INCLUDE_DIRS_VAR})
			#	list(GET ${INCLUDE_DIRS_VAR} 0 FIRST_INCLUDE_DIR)
			# 	string(REPLACE "/include" "/bin" BIN_DIR "${FIRST_INCLUDE_DIR}")
			# else()
			# 	message(FATAL_ERROR "Can't find bin directory of package dependency")
			# endif()
			
			# find_program caches, and what it caches here is an absolute path INTO a conan package folder.
			# Those are content addressed: any rebuild that changes the package revision leaves the old folder
			# deleted and this cache entry pointing at nothing, which then surfaces as the packages directory
			# below "not existing" rather than as the missing program it actually is.
			# So the cached value is re-validated before it's trusted.
			# The variable is also per dependency: one shared name would make every entry after the first reuse
			# whatever the first one resolved to, since find_program does nothing when its variable is already set.

			set(PROGRAM_PATH_VAR OXC3_EXTERNAL_DEP_${file})

			if(${PROGRAM_PATH_VAR} AND NOT EXISTS "${${PROGRAM_PATH_VAR}}")
				unset(${PROGRAM_PATH_VAR} CACHE)
			endif()

			find_program(${PROGRAM_PATH_VAR} ${file} REQUIRED)
			get_filename_component(BIN_DIR "${${PROGRAM_PATH_VAR}}" DIRECTORY)
			
			message(STATUS "add_virtual_dependencies_external: Found ${file}'s bin Directory: ${BIN_DIR}")

			# Grab the packages conan has prepared

			set(PACKAGE_DIR "${BIN_DIR}/packages")

			if(EXISTS "${PACKAGE_DIR}")

				file(GLOB_RECURSE PACKAGE_FILES CONFIGURE_DEPENDS "${PACKAGE_DIR}/*/*.oiCA")

				foreach(PACKAGE_FILE ${PACKAGE_FILES})
					get_property(res TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST)
					set_property(TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST ${PACKAGE_FILE};${res})
				endforeach()

			else()
				message(FATAL_ERROR
					"${_ARGS_TARGET}: package directory not found: ${PACKAGE_DIR} "
					"(resolved from ${file} at ${${PROGRAM_PATH_VAR}})"
				)
			endif()

		endforeach()
	else()
		message(FATAL_ERROR "add_virtual_dependencies: DEPENDENCIES argument is required!")
	endif()

endmacro()

# oxc3_add_test: one test target, defined once.
#
# Replaces ~18 lines of per module boilerplate that spelled the target name five or six times.
# That repetition is what let the C++ glob go missing from every module except types/container, which silently excluded
# C++ test TUs from the build while it still exited 0.
#
#   oxc3_add_test(
#       NAME        OxC3_formats_bmp_test
#       FOLDER      Oxsomi/test/formats
#       LIBS        OxC3_types_container_test_util OxC3_formats_bmp
#       [DIR        test]                # source dir, relative to the caller
#       [INCLUDES   ../../../include]
#       [WORKING_DIR <dir>]              # for a test that reads data relative to itself
#       [SOURCES    <files...>]          # extra sources (e.g. an HLSL corpus shown in the IDE)
#       [DATA       <files...>]          # re-run when these change, not just on relink
#       [DEPS       <targets/files...>]   # e.g. a DLL the exe loads but does not relink against, or a
#                                         # package built by another target
#       [NO_AUTORUN]                     # register with ctest but don't run it after building
#   )
#
# Tests run as a POST_BUILD step, so a target that didn't relink doesn't re-run: the build graph does the
# dirty tracking rather than a second cache with its own staleness rules.
# POST_BUILD is portable across generators (PRE_BUILD is the Visual Studio only one), but it EXECUTES the
# binary, so it is skipped when cross compiling; Android runs its suite on device through OxC3_atest.

function(oxc3_add_test)

	set(options NO_AUTORUN)
	set(oneValue NAME FOLDER DIR WORKING_DIR)
	set(multiValue LIBS INCLUDES DATA DEPS SOURCES)
	cmake_parse_arguments(T "${options}" "${oneValue}" "${multiValue}" ${ARGN})

	# A misspelled keyword otherwise vanishes and the function silently does less than the caller asked.
	# A keyword given with no value is indistinguishable from not passing it at all.

	if(T_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "oxc3_add_test: unrecognized arguments: ${T_UNPARSED_ARGUMENTS}")
	endif()

	if(T_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "oxc3_add_test: arguments missing a value: ${T_KEYWORDS_MISSING_VALUES}")
	endif()

	if(NOT T_NAME)
		message(FATAL_ERROR "oxc3_add_test: NAME is required")
	endif()

	if(NOT T_DIR)
		set(T_DIR "test")
	endif()

	# Every test kind in one place, so a new module can't forget the C++ half.

	file(GLOB testSources CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/${T_DIR}/*.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/${T_DIR}/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/${T_DIR}/*.h"
	)

	add_executable(${T_NAME} ${testSources} ${T_SOURCES} CMakeLists.txt)

	if(T_LIBS)
		target_link_libraries(${T_NAME} PUBLIC ${T_LIBS})
	endif()

	if(T_INCLUDES)
		target_include_directories(${T_NAME} PUBLIC ${T_INCLUDES})
	endif()

	set_target_properties(${T_NAME} PROPERTIES FOLDER "${T_FOLDER}")

	if(T_WORKING_DIR)
		set_target_properties(${T_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${T_WORKING_DIR}")
	endif()

	set(runArgs NAME ${T_NAME} FOLDER "${T_FOLDER}")

	if(T_WORKING_DIR)
		list(APPEND runArgs WORKING_DIR "${T_WORKING_DIR}")
	endif()

	if(T_NO_AUTORUN)
		list(APPEND runArgs NO_AUTORUN)
	endif()

	# Appended only when non empty, like the two above: an empty list would pass the keyword with no value,
	# which is what a caller writing DATA and then forgetting the files looks like.

	if(T_DATA)
		list(APPEND runArgs DATA ${T_DATA})
	endif()

	if(T_DEPS)
		list(APPEND runArgs DEPS ${T_DEPS})
	endif()

	oxc3_add_test_run(${runArgs})

endfunction()

# The run half on its own, for a target built by hand because it needs more than the wrapper does (a package attached
# to it, a dylib it dlopens).
# Registers with ctest, and runs it after building unless OxC3TestAutoRun is off.

function(oxc3_add_test_run)

	set(options NO_AUTORUN)
	set(oneValue NAME FOLDER WORKING_DIR)
	set(multiValue DATA DEPS)
	cmake_parse_arguments(T "${options}" "${oneValue}" "${multiValue}" ${ARGN})

	# A misspelled keyword otherwise vanishes and the function silently does less than the caller asked.
	# A keyword given with no value is indistinguishable from not passing it at all.

	if(T_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "oxc3_add_test_run: unrecognized arguments: ${T_UNPARSED_ARGUMENTS}")
	endif()

	if(T_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "oxc3_add_test_run: arguments missing a value: ${T_KEYWORDS_MISSING_VALUES}")
	endif()

	if(NOT TARGET ${T_NAME})
		message(FATAL_ERROR "oxc3_add_test_run: ${T_NAME} is not a target")
	endif()

	if(T_WORKING_DIR)
		add_test(NAME ${T_NAME} COMMAND ${T_NAME} WORKING_DIRECTORY "${T_WORKING_DIR}")
	else()
		add_test(NAME ${T_NAME} COMMAND ${T_NAME})
	endif()

	if(NOT OxC3TestAutoRun OR T_NO_AUTORUN OR CMAKE_CROSSCOMPILING)
		return()
	endif()

	# A STAMP, not a POST_BUILD event.
	# Under MSBuild a post build event runs every time the project is built, and it builds every project each pass even
	# when the link is skipped, so POST_BUILD would re-run every suite on every build.
	# A custom command with an OUTPUT re-runs only when something it DEPENDS on is newer than that output, which is real
	# dirty tracking and behaves the same on Ninja.
	#
	# Through the wrapper, never directly: MSBuild fails a step whose output merely LOOKS like an error, and these tests
	# print error shaped text on purpose.
	# See cmake/run_test.cmake.

	set(runScript "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_test.cmake")
	set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${T_NAME}.passed")

	set(envFile "${CMAKE_CURRENT_BINARY_DIR}/${T_NAME}.env")

	set(runArgs -DTEST_EXE=$<TARGET_FILE:${T_NAME}> -DTEST_NAME=${T_NAME} -DTEST_ENV_FILE=${envFile})

	if(T_WORKING_DIR)
		list(APPEND runArgs -DTEST_WORKING_DIR=${T_WORKING_DIR})
	endif()

	# DATA is what makes a data driven suite honest: a test built from a golden, an HLSL corpus or a
	# packaged shader relinks nothing when one of those changes, so without naming them the stamp stays
	# newer than its real inputs and the one test whose whole job is comparing against them never re-runs.
	#
	# These are FILES on purpose.
	# Depending on a custom target here would re-run every build, since a custom target is out of date by definition.

	add_custom_command(
		OUTPUT "${stamp}"
		COMMAND ${CMAKE_COMMAND} ${runArgs} -P "${runScript}"
		COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
		DEPENDS ${T_NAME} ${T_DATA} ${T_DEPS}
		COMMENT "Running ${T_NAME}"
		VERBATIM
	)

	add_custom_target(${T_NAME}_run ALL DEPENDS "${stamp}")
	set_target_properties(${T_NAME}_run PROPERTIES FOLDER "${T_FOLDER}")

	# Generated rather than written now, so oxc3_test_env can still add to it afterwards.

	file(GENERATE OUTPUT "${envFile}" CONTENT "$<JOIN:$<TARGET_PROPERTY:${T_NAME}_run,OXC3_TEST_ENV>,\n>\n$<$<BOOL:$<TARGET_PROPERTY:${T_NAME}_run,OXC3_TEST_PRELOAD>>:LD_PRELOAD=path_list_prepend:$<TARGET_FILE:$<TARGET_PROPERTY:${T_NAME}_run,OXC3_TEST_PRELOAD>>\n>")

endfunction()

# Environment for one test, in ctest's own ENVIRONMENT_MODIFICATION syntax (NAME=op:VALUE).
#
# Applied to BOTH ways a suite runs: set_property(TEST) covers ctest, and the same entries are written beside
# the stamp for run_test.cmake, which is a plain execute_process and cannot read test properties.
# Without the second half an ASan build that runs its suites at build time (the default) would run them
# without the options CI applies, and fail on things CI never sees.
#
# Callable after the test is registered, which is how both existing callers are written: the entries land on
# a target property and file(GENERATE) resolves it once every CMakeLists has run.

function(oxc3_test_env testName)

	if(NOT ARGN)
		return()
	endif()

	if(TEST ${testName})
		set_property(TEST ${testName} APPEND PROPERTY ENVIRONMENT_MODIFICATION ${ARGN})
	endif()

	if(TARGET ${testName}_run)
		set_property(TARGET ${testName}_run APPEND PROPERTY OXC3_TEST_ENV ${ARGN})
	endif()

endfunction()

# LD_PRELOAD of a target built here, which is the one env entry oxc3_test_env cannot carry.
#
# Its value is only knowable through $<TARGET_FILE:>, and a generator expression STORED IN a property comes
# back out verbatim when file(GENERATE) reads that property: evaluation is a single pass, not recursive.
# The result was an env file containing the literal text of the expression, which the loader then split on
# the colon inside it and refused, silently leaving the build-time run without the preload it asked for.
# So what gets stored is the target NAME, and the env file builds the entry around it in one expression,
# where $<TARGET_FILE:> is still evaluated.

function(oxc3_test_preload testName preloadTarget)

	if(TEST ${testName})
		set_property(TEST ${testName} APPEND PROPERTY ENVIRONMENT_MODIFICATION
			"LD_PRELOAD=path_list_prepend:$<TARGET_FILE:${preloadTarget}>"
		)
	endif()

	if(TARGET ${testName}_run)
		set_property(TARGET ${testName}_run PROPERTY OXC3_TEST_PRELOAD ${preloadTarget})
	endif()

endfunction()

# A test that is not an executable of its own (the CLI suite drives a python script against a built binary).
# It hangs off the target it exercises, so it re-runs when that binary changes.

function(oxc3_add_test_command)

	set(options NO_AUTORUN)
	set(oneValue NAME TARGET WORKING_DIR FOLDER)
	set(multiValue COMMAND DATA DEPS)
	cmake_parse_arguments(T "${options}" "${oneValue}" "${multiValue}" ${ARGN})

	# A misspelled keyword otherwise vanishes and the function silently does less than the caller asked.
	# A keyword given with no value is indistinguishable from not passing it at all.

	if(T_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "oxc3_add_test_command: unrecognized arguments: ${T_UNPARSED_ARGUMENTS}")
	endif()

	if(T_KEYWORDS_MISSING_VALUES)
		message(FATAL_ERROR "oxc3_add_test_command: arguments missing a value: ${T_KEYWORDS_MISSING_VALUES}")
	endif()

	if(T_WORKING_DIR)
		add_test(NAME ${T_NAME} COMMAND ${T_COMMAND} WORKING_DIRECTORY ${T_WORKING_DIR})
	else()
		add_test(NAME ${T_NAME} COMMAND ${T_COMMAND})
	endif()

	if(OxC3TestAutoRun AND NOT T_NO_AUTORUN AND NOT CMAKE_CROSSCOMPILING AND TARGET ${T_TARGET})

		set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${T_NAME}.passed")
		set(envFile "${CMAKE_CURRENT_BINARY_DIR}/${T_NAME}.env")
		set(runScript "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/run_test.cmake")

		# Through the same wrapper as an executable suite, for the same two reasons: MSBuild fails a step whose
		# output merely LOOKS like an error, and the environment has to match what ctest applies.
		# | separated because -D would split a ; separated list itself.

		string(JOIN "|" commandLine ${T_COMMAND})

		add_custom_command(
			OUTPUT "${stamp}"
			COMMAND ${CMAKE_COMMAND}
				"-DTEST_COMMAND=${commandLine}" -DTEST_NAME=${T_NAME} -DTEST_ENV_FILE=${envFile}
				-DTEST_WORKING_DIR=${T_WORKING_DIR} -P "${runScript}"
			COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
			DEPENDS ${T_TARGET} ${T_DATA} ${T_DEPS}
			WORKING_DIRECTORY ${T_WORKING_DIR}
			COMMENT "Running ${T_NAME}"
			VERBATIM
		)

		add_custom_target(${T_NAME}_run ALL DEPENDS "${stamp}")
		set_target_properties(${T_NAME}_run PROPERTIES FOLDER "${T_FOLDER}")

		file(GENERATE OUTPUT "${envFile}" CONTENT "$<JOIN:$<TARGET_PROPERTY:${T_NAME}_run,OXC3_TEST_ENV>,\n>\n$<$<BOOL:$<TARGET_PROPERTY:${T_NAME}_run,OXC3_TEST_PRELOAD>>:LD_PRELOAD=path_list_prepend:$<TARGET_FILE:$<TARGET_PROPERTY:${T_NAME}_run,OXC3_TEST_PRELOAD>>\n>")
	endif()

endfunction()
