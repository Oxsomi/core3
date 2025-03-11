# Setting the icon of the app
# Call this immediately before configure_virtual_files with the executable

function(configure_icon target icon)

	if(NOT TARGET ${target})
		message(FATAL_ERROR "configure_icon: target ${target} not present.")
	endif()

	if(WIN32)
		get_property(res TARGET ${target} PROPERTY RESOURCE_LIST_RC)
		set_property(TARGET ${target} PROPERTY RESOURCE_LIST LOGO\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ICON\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \"${icon}\"\n${res})
		target_sources(${target} PRIVATE ${icon})
	endif()

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

	foreach(file ${res})

		message("Package file: ${file}")

		string(REPLACE "\\" "/" file "${file}")

		string(FIND "${file}" "packages/" PACKAGE_POS)
		string(SUBSTRING "${file}" ${PACKAGE_POS} -1 RESULT)
		string(SUBSTRING "${RESULT}" 9 -1 FINAL_RESULT)

		string(REGEX REPLACE "\\.oiCA$" "" RELATIVE_PATH "${FINAL_RESULT}")

		# Differences in packaging:
		# Windows you can embed using an .rc file; then this handle can be opened through FindResourceW
		# Linux you can embed into the elf manually by using objcopy and manually read the section data to find where it's located
		# Android has APKs which are just like zip files, so can be easily read (though the NDK can't access subfolders easily)
		# iOS has IPA which is the same idea as APK.
		# OS X has llvm-objcopy.
		# web/emscripten has a virtual filesystem.
		
		if(WIN32)
			get_property(res2 TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST_RC)
			set_property(TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST_RC ${RELATIVE_PATH}\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ RCDATA\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \"${file}\"\n${res2})
		elseif(UNIX AND NOT APPLE AND NOT ANDROID)
			add_custom_command(
				TARGET ${_ARGS_TARGET} POST_BUILD
				COMMAND objcopy --add-section "packages/${RELATIVE_PATH}=${file}" "$<TARGET_FILE_DIR:${_ARGS_TARGET}>/$<TARGET_FILE_NAME:${_ARGS_TARGET}>" "$<TARGET_FILE_DIR:${_ARGS_TARGET}>/$<TARGET_FILE_NAME:${_ARGS_TARGET}>"
			)
		endif()

	endforeach()

	if(WIN32)
		get_property(res2 TARGET ${target} PROPERTY RESOURCE_LIST_RC)
		if(NOT "${res2}" STREQUAL "")
			file(WRITE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.rc" ${res2})
			target_sources(${target} PRIVATE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.rc")
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
	
	if(_ARGS_FORCE_PACKAGER)
		if(NOT TARGET OxC3_package)
			find_program(OXC3_PACKAGE OxC3_package REQUIRED)
		else()
			set(OXC3_PACKAGE OxC3_package)
		endif()
	else()
		if(NOT TARGET OxC3)
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
	else()
		set(platform linux)
	endif()

	set(RuntimeOutputDir "${_ARGS_SELF}/build/${platform}")

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
			
			find_program(PROGRAM_PATH ${file} REQUIRED)
			get_filename_component(BIN_DIR "${PROGRAM_PATH}" DIRECTORY)
			
			message(STATUS "add_virtual_dependencies_external: Found ${file}'s bin Directory: ${BIN_DIR}")

			# Grab the packages conan has prepared

			set(PACKAGE_DIR "${BIN_DIR}/packages")

			if(EXISTS "${PACKAGE_DIR}")

				file(GLOB_RECURSE PACKAGE_FILES "${PACKAGE_DIR}/*/*.oiCA")

				foreach(PACKAGE_FILE ${PACKAGE_FILES})
					get_property(res TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST)
					set_property(TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST ${PACKAGE_FILE};${res})
				endforeach()

			else()
				message(FATAL_ERROR "${target} package directory not found: ${PACKAGE_DIR}")
			endif()

		endforeach()
	else()
		message(FATAL_ERROR "add_virtual_dependencies: DEPENDENCIES argument is required!")
	endif()

endmacro()
