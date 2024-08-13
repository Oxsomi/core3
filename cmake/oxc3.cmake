# Setting the icon of the app
# Call this immediately before configure_virtual_files with the executable

function(configure_icon target icon)

	if(NOT TARGET ${target})
		message(FATAL_ERROR "configure_icon: target ${target} not present.")
	endif()

	if(WIN32)
		get_property(res TARGET ${target} PROPERTY RESOURCE_LIST)
		set_property(TARGET ${target} PROPERTY RESOURCE_LIST LOGO\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ICON\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \"${icon}\"\n${res})
		target_sources(${target} PRIVATE ${icon})
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
# )
# This would add myTarget/shaders as a virtual directory.
# myTarget/shaders has to be loaded manually by the app to process it.
# This is then passed onto to OxC3 package to ensure all files are converted to the right types.
# This effectively means it'd be packaged as a separate .oiCA file that has to be loaded via File_loadVirtual.

macro(add_virtual_files)

	set(_OPTIONS)
	set(_ONE_VALUE TARGET ROOT NAME SELF)
	set(_MULTI_VALUE)

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

	if(WIN32)

		add_custom_target(
			${_ARGS_TARGET}_package_${_ARGS_NAME}
			COMMAND OxC3 file package -input "${_ARGS_ROOT}" -output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA"
			WORKING_DIRECTORY ${_ARGS_SELF}
		)

		# message("-- ${_ARGS_TARGET}_package_${_ARGS_NAME}: OxC3 file package -i \"${_ARGS_ROOT}\" -o \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA\" @ work dir ${_ARGS_SELF}")

		set_target_properties(${_ARGS_TARGET}_package_${_ARGS_NAME} PROPERTIES FOLDER Oxsomi/package)

		add_dependencies(${_ARGS_TARGET} ${_ARGS_TARGET}_package_${_ARGS_NAME} OxC3)

		get_property(res TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST)
		set_property(TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST ${_ARGS_TARGET}/${_ARGS_NAME}\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ RCDATA\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA\"\n${res})

	endif()

endmacro()

# Add a dependency to ensure the dependency files for a project are present.
# This is useful if a dependency would need to include things like fonts, or other kinds of resources.
# This would for example allow you to load myTarget/shaders in myNewTarget (from the other example).
# These dependencies are always public, so if myTarget would have a dependency, this one does too.
# Example:
# add_virtual_dependencies(TARGET myNewTarget DEPENDENCIES myTarget)

macro(add_virtual_dependencies)

	set(_OPTIONS)
	set(_ONE_VALUE TARGET)
	set(_MULTI_VALUE DEPENDENCIES)

	cmake_parse_arguments(_ARGS "${_OPTIONS}" "${_ONE_VALUE}" "${_MULTI_VALUE}" ${ARGN})

	# Validate

	if(NOT TARGET ${_ARGS_TARGET})
		message(FATAL_ERROR "add_virtual_dependencies: target \"${_ARGS_TARGET}\" not present.")
	endif()

	if(NOT _ARGS_TARGET MATCHES "^[0-9A-Za-z_\$]+$")
		message(FATAL_ERROR "add_virtual_dependencies: 'TARGET' has to conform to alphanumeric (plus _ and $).")
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
			set_property(TARGET ${_ARGS_TARGET} PROPERTY RESOURCE_LIST ${res0}\n${res1})

		endforeach()
	else()
		message(FATAL_ERROR "add_virtual_dependencies: DEPENDENCIES argument is required!")
	endif()

endmacro()

# Configure virtual files (and icon) for a target

function(configure_virtual_files target)

	if(NOT TARGET ${target})
		message(FATAL_ERROR "configure_virtual_files: target ${target} not present.")
	endif()

	if(WIN32)

		get_property(res TARGET ${target} PROPERTY RESOURCE_LIST)
		if(NOT "${res}" STREQUAL "")
			file(WRITE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.rc" ${res})
			target_sources(${target} PRIVATE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.rc")
		endif()

		configure_file(${CMAKE_OXC3_ROOT}/src/platforms/windows/manifest.xml "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}_manifest.xml")
		add_custom_command(
			TARGET ${target}
			POST_BUILD DEPENDS
			COMMAND mt.exe -manifest "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}_manifest.xml" -inputresource:"$<TARGET_FILE:${target}>"\;\#1 -updateresource:"$<TARGET_FILE:${target}>"\;\#1
		)

	endif()

endfunction()