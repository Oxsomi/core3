# OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
# Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

cmake_minimum_required(VERSION 3.13.0)
project(OxC3)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "32-bit builds are disabled by core3 because it's legacy and could cause unintended behavior if built anyways.")
endif()

set(EnableSIMD ON CACHE BOOL "Enables SIMD")
set(RelaxFloat OFF CACHE BOOL "Relax float (perf > accuracy)")

# Use ${HideConsole} to add_executable to hide the console window

if(MSVC)
	set(HideConsole "WIN32")
else()
	set(HideConsole "")
endif()

# Even though EnableSIMD can be turned off, it won't be truly off
# This is because we want to maintain proper IEEE754 compliance, which we can't do without SSE

if(MSVC)

	add_compile_options(/W4 /WX /MD /MP /GR /D_CRT_SECURE_NO_WARNINGS /Gy)

	if(EnableSIMD) 
		add_compile_options(/d2archSSE42)		# Require SSE4.2
	endif()

	if(RelaxFloat)
		add_compile_options(/fp:fast)
	else()
		add_compile_options(/fp:strict)
	endif()

else()

	add_compile_options(-Wall -Wextra -Werror)

	if(EnableSIMD) 
		add_compile_options(-mfpmath=sse -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mrdrnd -msha -maes)
	elseif(NOT RelaxFloat)
		add_compile_options(-mfpmath=sse -msse)
	endif()

	if(RelaxFloat)
		add_compile_options(-ffast-math)
	else()
		add_compile_options(-fno-unsafe-math-optimizations)
	endif()

endif()

message("-- Enabling x64 specific optimizations such as CRC32C, AES256 and SHA256 (optional) instructions")
set(_ARCH "ARCH_X64")

if(EnableSIMD) 
	message("-- Enabling SIMD (-DEnableSIMD=ON)")
	set(_SIMD "SIMD_SSE")
else()
	message("-- Disabling SIMD (-DEnableSIMD=OFF)")
	set(_SIMD "SIMD_NONE")
endif()

if(RelaxFloat) 
	message("-- Enabling relaxed float (better perf, but less consistency) (-DRelaxFloat=ON)")
	set(_RELAX_FLOAT "1")
else()
	message("-- Disabling relaxed floats (worse perf, but more consistency) (-DRelaxFloat=OFF)")
	set(_RELAX_FLOAT "0")
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
	set(_PLATFORM_TYPE EPlatform_Windows)
else()
	message("-- Enabling Linux support...")
	set(platform linux)
	set(_PLATFORM_TYPE EPlatform_Linux)
endif()

if(TRUE)
	message("-- Enabling Vulkan support...")
	set(graphicsApi vulkan)
	set(_GRAPHICS_API EGraphicsApi_Vulkan)
endif()

# Types 

file(GLOB_RECURSE typesHeaders "inc/file/*.h" "inc/math/*.h" "inc/types/*.h" "inc/cfg/*.h" "*.h.in")
file(GLOB_RECURSE typesSources "src/file/*.c" "src/math/*.c" "src/types/*.c")

add_library(
	OxC3_types
	STATIC
	${typesHeaders}
	${typesSources}
	CMakeLists.txt
)

set_target_properties(OxC3_types PROPERTIES FOLDER Oxsomi)

target_compile_definitions(OxC3_types PUBLIC -D_SIMD=${_SIMD} -D_RELAX_FLOAT=${_RELAX_FLOAT} -D_ARCH=${_ARCH} -D_PLATFORM_TYPE=${_PLATFORM_TYPE} -D_FORCE_FLOAT_FALLBACK=${_FORCE_FLOAT_FALLBACK})

target_include_directories(OxC3_types PUBLIC inc)

if(WIN32)
	target_link_libraries(OxC3_types PUBLIC Bcrypt.lib)
endif()

# Formats

file(GLOB_RECURSE formatsHeaders "inc/formats/*.h")
file(GLOB_RECURSE formatsSources "src/formats/*.c")

add_library(
	OxC3_formats
	STATIC
	${formatsHeaders}
	${formatsSources}
	CMakeLists.txt
)

set_target_properties(OxC3_formats PROPERTIES FOLDER Oxsomi)

target_link_libraries(OxC3_formats PUBLIC OxC3_types)

# Platforms

message("-- Targeting platform: ${platform}")

file(GLOB platformsHeaders "inc/platforms/*.h")
file(GLOB_RECURSE platformsHeadersExt "inc/platforms/ext/*.h")
file(GLOB_RECURSE platformsHeadersDependent "inc/platforms/ext/${platform}/*.h")

file(GLOB_RECURSE platformsSources "src/platforms/${platform}/*.c" "src/platforms/generic/*.c")

add_library(
	OxC3_platforms
	STATIC
	${platformsHeaders}
	${platformsHeadersExt}
	${platformsHeadersDependent}
	${platformsSources}
	CMakeLists.txt
)

set_target_properties(OxC3_platforms PROPERTIES FOLDER Oxsomi)

target_link_libraries(OxC3_platforms PUBLIC OxC3_formats)

# Graphics layer

message("-- Targeting graphics api: ${graphicsApi} on platform ${platform}")

file(GLOB_RECURSE graphicsHeaders "inc/graphics/generic/*.h")
file(GLOB_RECURSE graphicsApiHeaders "inc/graphics/${graphicsApi}/*.h")

file(GLOB_RECURSE graphicsApiSources "src/graphics/${graphicsApi}/generic/*.c")
file(GLOB_RECURSE graphicsApiPlatformSources "src/graphics/${graphicsApi}/${platform}/*.c")

file(GLOB_RECURSE graphicsSources "src/graphics/generic/*.c")
file(GLOB_RECURSE shaders "src/shaders/*.hlsl")

add_library(
	OxC3_graphics
	STATIC
	${graphicsHeaders}
	${graphicsApiHeaders}
	${graphicsApiSources}
	${graphicsApiPlatformSources}
	${graphicsSources}
	${shaders}
	CMakeLists.txt
)

source_group("Header Files (${graphicsApi})" FILES ${graphicsApiHeaders})
source_group("Source Files (${graphicsApi})" FILES ${graphicsApiSources})
source_group("Source Files (${graphicsApi})/${platform}" FILES ${graphicsApiPlatformSources})
source_group("Shaders" FILES ${shaders})

target_compile_definitions(OxC3_graphics PUBLIC -D_GRAPHICS_API=${_GRAPHICS_API})

set_target_properties(OxC3_graphics PROPERTIES FOLDER Oxsomi)

target_link_libraries(OxC3_graphics PUBLIC OxC3_platforms)

if("${graphicsApi}" STREQUAL "vulkan")
	
	if(WIN32)
		if(DEFINED ENV{VULKAN_SDK})

			set(VULKAN_LIB vulkan-1)
			
			include_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/Include)
			target_link_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/Lib)

		else()
			message(FATAL_ERROR "-- Vulkan incompatible: Please download the latest Vulkan SDK so you can compile")
		endif()
	else()
		set(VULKAN_LIB vulkan)
	endif()
	
	if("${platform}" STREQUAL "windows")
		add_definitions(-D_VK_SURFACE_EXT="VK_KHR_win32_surface")
		add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
	elseif("${platform}" STREQUAL "android")
		add_definitions(-D_VK_SURFACE_EXT="VK_KHR_android_surface")
		add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)
	else()
		message(FATAL_ERROR "Platform not supported for Vulkan yet: ${platform}")
	endif()

	target_link_libraries(OxC3_graphics PUBLIC ${VULKAN_LIB})

elseif("${graphicsApi}" STREQUAL "directx12")
	message(FATAL_ERROR "Graphics api not supported yet: DirectX12")
elseif("${graphicsApi}" STREQUAL "metal")
	message(FATAL_ERROR "Graphics api not supported yet: Metal")
elseif("${graphicsApi}" STREQUAL "webgpu")
	message(FATAL_ERROR "Graphics api not supported yet: WebGPU")
else()
	message(FATAL_ERROR "Graphics api not supported: ${graphicsApi}")
endif()

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
# add_virtual_directory(
#	TARGET
#		myTarget
#	NAME
#		shaders
#	ROOT
#		${CMAKE_CURRENT_SOURCE_DIR}/res/test_shaders
# )
# This would add myTarget/shaders as a virtual directory.
# myTarget/shaders has to be loaded manually by the app to process it.
# This is then passed onto to OxC3 package to ensure all files are converted to the right types.
# This effectively means it'd be packaged as a separate .oiCA file that has to be loaded via File_loadVirtual.

macro(add_virtual_files)

	set(_OPTIONS)
	set(_ONE_VALUE TARGET ROOT NAME)
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
	
	if(NOT _ARGS_NAME MATCHES "^[0-9A-Za-z_\$]+$")
		message(FATAL_ERROR "add_virtual_files: 'NAME' has to conform to alphanumeric (plus _ and $).")
	endif()
	
	if(NOT _ARGS_TARGET MATCHES "^[0-9A-Za-z_\$]+$")
		message(FATAL_ERROR "add_virtual_files: 'TARGET' has to conform to alphanumeric (plus _ and $).")
	endif()

	string(TOUPPER ${_ARGS_TARGET} ANGRY_TARGET)
	
	if(ANGRY_TARGET MATCHES "access" OR ANGRY_TARGET MATCHES "function")
		message(FATAL_ERROR "add_virtual_files: 'TARGET' can't be 'access' or 'function'.")
	endif()

	# Add processed file as a file

	if(WIN32)

		add_custom_target(
			${_ARGS_TARGET}_package
			COMMAND OxC3 file package -i "${_ARGS_ROOT}" -o "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/packages/${_ARGS_TARGET}/${_ARGS_NAME}.oiCA"
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		)

		set_target_properties(${_ARGS_TARGET}_package PROPERTIES FOLDER Oxsomi/package)

		add_dependencies(${_ARGS_TARGET} ${_ARGS_TARGET}_package OxC3)

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
	endif()
	
endfunction()

# Tests

file(GLOB_RECURSE tests "tst/*.c")

add_executable(
	OxC3_test
	${tests}
	CMakeLists.txt
)

target_link_libraries(OxC3_test PUBLIC OxC3_types)

set_target_properties(OxC3_test PROPERTIES FOLDER Oxsomi/test)

# OxC3 tool

file(GLOB_RECURSE headers "tools/inc/*.h")
file(GLOB_RECURSE sources "tools/src/*.c")

add_executable(
	OxC3
	${headers}
	${sources}
	CMakeLists.txt
)

set_target_properties(OxC3 PROPERTIES FOLDER Oxsomi)

target_link_libraries(OxC3 PUBLIC OxC3_platforms)
target_include_directories(OxC3 PUBLIC tools/inc)

configure_icon(OxC3 "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
configure_virtual_files(OxC3)
