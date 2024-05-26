# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

cmake_minimum_required(VERSION 3.21.0)
project(OxC3)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 20)				# We need some C++ to interface with DXC, but we wrap it using C
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "32-bit builds are disabled by core3 because it's legacy and could cause unintended behavior if built anyways.")
endif()

set(EnableSIMD ON CACHE BOOL "Enables SIMD")
set(RelaxFloat OFF CACHE BOOL "Relax float (perf > accuracy)")
set(CLIGraphics ON CACHE BOOL "Turn off/on OxC3 graphics CLI (might be unsupported on device)")
set(EnableShaderCompiler OFF CACHE BOOL "Turn off/on shader compiler + OxC3 compile shaders CLI (increases compile time)")

set(EnableTests ON CACHE BOOL "Turn off/on OxC3 tests")
set(EnableOxC3CLI OFF CACHE BOOL "Turn on/off OxC3 CLI")

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
		add_compile_options(-mfpmath=sse -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mrdrnd -msha -maes -mbmi2 -mf16c)
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
else()
	message("-- Disabling OxC3 unit tests (-DEnableTests=Off)")
endif()

if(EnableShaderCompiler)
	message("-- Enabling OxC3 compile shaders (-DEnableShaderCompiler=On)")
else()
	message("-- Disabling OxC3 compile shaders (-DEnableShaderCompiler=Off)")
endif()

if(EnableSIMD)
	message("-- Enabling SIMD (-DEnableSIMD=ON)")
	set(_SIMD "SIMD_SSE")
	set(simd sse)
else()
	message("-- Disabling SIMD (-DEnableSIMD=OFF)")
	set(_SIMD "SIMD_NONE")
	set(simd none)
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
	set(_PLATFORM_TYPE PLATFORM_WINDOWS)
elseif(IOS)
	message("-- Enabling iOS support...")
	set(platform ios)
	set(_PLATFORM_TYPE PLATFORM_IOS)
elseif(APPLE)
	message("-- Enabling OS X support...")
	set(platform osx)
	set(_PLATFORM_TYPE PLATFORM_OSX)
elseif(ANDROID)
	message("-- Enabling Android support...")
	set(platform android)
	set(_PLATFORM_TYPE PLATFORM_ANDROID)
else()
	message("-- Enabling Linux support...")
	set(platform linux)
	set(_PLATFORM_TYPE PLATFORM_LINUX)
endif()

set(ForceUpdateDeps OFF CACHE BOOL "Force update dependencies")
set(ARM OFF CACHE BOOL "Force ARM executable")											# //TODO:
set(ForceVulkan OFF CACHE BOOL "Force Vulkan support (disable native API if available)")

# Types

file(GLOB_RECURSE typesHeaders "inc/types/*.h")
file(GLOB typesSources "src/types/*.c")
file(GLOB_RECURSE typesPlatformSources "src/types/platforms/${platform}/*.c")
file(GLOB_RECURSE typesPlatformHeaders "inc/types/platforms/${platform}/*.h")
file(GLOB_RECURSE typesSIMDSources "src/types/simd/${simd}/*.c")

if(UNIX)
	file(GLOB_RECURSE typesUnixSources "src/types/platforms/unix/*.c")
	list(APPEND typesPlatformSources ${typesUnixSources})
endif()

add_library(
	OxC3_types
	STATIC
	${typesHeaders}
	${typesSources}
	${typesPlatformSources}
	${typesSIMDSources}
	${typesPlatformHeaders}
	CMakeLists.txt
)

source_group("Source Files (${platform})" FILES ${typesPlatformSources})
source_group("Header Files (${platform})" FILES ${typesPlatformHeaders})
source_group("Source Files (simd: ${simd})" FILES ${typesSIMDSources})

set_target_properties(OxC3_types PROPERTIES FOLDER Oxsomi)

target_compile_definitions(OxC3_types PUBLIC -D_SIMD=${_SIMD} -D_RELAX_FLOAT=${_RELAX_FLOAT} -D_ARCH=${_ARCH} -D_PLATFORM_TYPE=${_PLATFORM_TYPE} -D_FORCE_FLOAT_FALLBACK=${_FORCE_FLOAT_FALLBACK})

target_include_directories(OxC3_types PUBLIC inc)

if(WIN32)
	target_link_libraries(OxC3_types PUBLIC Bcrypt.lib)
elseif(APPLE)
	target_link_libraries(OxC3_types PUBLIC "-framework Security")
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

file(GLOB platformsHeaders "inc/platforms/*.h" "inc/platforms/ext/*.h")

file(GLOB_RECURSE platformsPlatformSources "src/platforms/${platform}/*.c")
file(GLOB_RECURSE platformsPlatformIncludes "inc/platforms/${platform}/*.h")
file(GLOB_RECURSE platformsSources "src/platforms/generic/*.c")
file(GLOB_RECURSE platformsSIMDSources "src/platforms/simd/${simd}/*.c")

if(UNIX)
	file(GLOB_RECURSE platformsUnixSources "src/platforms/unix/*.c")
	list(APPEND platformsSources ${platformsUnixSources})
endif()

add_library(
	OxC3_platforms
	STATIC
	${platformsHeaders}
	${platformsPlatformSources}
	${platformsPlatformIncludes}
	${platformsSources}
	${platformsSIMDSources}
	CMakeLists.txt
)

source_group("Source Files (${platform})" FILES ${platformsPlatformSources})
source_group("Header Files (${platform})" FILES ${platformsPlatformIncludes})
source_group("Source Files (simd: ${simd})" FILES ${platformsSIMDSources})

set_target_properties(OxC3_platforms PROPERTIES FOLDER Oxsomi)

target_link_libraries(OxC3_platforms PUBLIC OxC3_formats)

if(APPLE)
	target_link_libraries(OxC3_platforms PUBLIC "-framework CoreFoundation" "-framework ApplicationServices" "-framework AppKit")
endif()

# Graphics layer

if(WIN32 AND NOT ${ForceVulkan})
	message("-- Enabling DirectX12 support...")
	set(graphicsApi directx12)
	set(_GRAPHICS_API GRAPHICS_API_D3D12)
else()
	message("-- Enabling Vulkan support...")
	set(graphicsApi vulkan)
	set(_GRAPHICS_API GRAPHICS_API_VULKAN)
endif()

set_property(GLOBAL PROPERTY graphicsApi "${graphicsApi}")

message("-- Targeting graphics api: ${graphicsApi} on platform ${platform}")

file(GLOB_RECURSE graphicsHeaders "inc/graphics/generic/*.h")
file(GLOB_RECURSE graphicsApiHeaders "inc/graphics/${graphicsApi}/*.h")

file(GLOB_RECURSE graphicsApiSources "src/graphics/${graphicsApi}/generic/*.c")
file(GLOB_RECURSE graphicsApiPlatformSources "src/graphics/${graphicsApi}/${platform}/*.c")

file(GLOB_RECURSE graphicsSources "src/graphics/generic/*.c")
# file(GLOB_RECURSE shaders "src/*.hlsl")

add_library(
	OxC3_graphics
	STATIC
	${graphicsHeaders}
	${graphicsApiHeaders}
	${graphicsApiSources}
	${graphicsApiPlatformSources}
	${graphicsSources}
	# ${shaders}
	CMakeLists.txt
)

source_group("Header Files (${graphicsApi})" FILES ${graphicsApiHeaders})
source_group("Source Files (${graphicsApi})" FILES ${graphicsApiSources})
source_group("Source Files (${graphicsApi})/${platform}" FILES ${graphicsApiPlatformSources})
# source_group("Shaders" FILES ${shaders})

target_compile_definitions(OxC3_graphics PUBLIC -D_GRAPHICS_API=${_GRAPHICS_API})

set_target_properties(OxC3_graphics PROPERTIES FOLDER Oxsomi)

target_link_libraries(OxC3_graphics PUBLIC OxC3_platforms)

# Vulkan setup

if("${graphicsApi}" STREQUAL "vulkan")

	if(DEFINED ENV{VULKAN_SDK})
		include_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/include)
		target_link_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/lib)
	else()
		message(FATAL_ERROR "-- Vulkan incompatible: Please download the latest Vulkan SDK so you can compile")
	endif()

	if(WIN32)
			set(VULKAN_LIB vulkan-1)
	else()
		set(VULKAN_LIB vulkan)
	endif()

	if("${platform}" STREQUAL "windows")
		add_definitions(-D_VK_SURFACE_EXT="VK_KHR_win32_surface")
	elseif("${platform}" STREQUAL "android")
		add_definitions(-D_VK_SURFACE_EXT="VK_KHR_android_surface")
	elseif("${platform}" STREQUAL "osx")
		add_definitions(-D_VK_SURFACE_EXT="VK_MVK_macos_surface")
	elseif("${platform}" STREQUAL "ios")
		add_definitions(-D_VK_SURFACE_EXT="VK_MVK_ios_surface")
	else()
		message(FATAL_ERROR "Platform not supported for Vulkan yet: ${platform}")
	endif()

	target_link_libraries(OxC3_graphics PUBLIC ${VULKAN_LIB})

# DirectX setup

elseif("${graphicsApi}" STREQUAL "directx12")

	# Download warp, so more extensive GPUBV is available

	if(${ForceUpdateDeps} OR (NOT EXISTS "${CMAKE_BINARY_DIR}/external/warp/"))

		message("-- Downloading and unzipping Microsoft.Direct3D.WARP")

		file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/external/warp/")
		file(DOWNLOAD "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.WARP/" "${CMAKE_BINARY_DIR}/external/warp.zip")
		file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/external/warp/)

		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_BINARY_DIR}/external/warp.zip
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/warp/
		)

		file(REMOVE "${CMAKE_BINARY_DIR}/external/warp.zip")

	endif()

	# Download the agility SDK so newer features are always available if the driver supports them

	if(${ForceUpdateDeps} OR (NOT EXISTS "${CMAKE_BINARY_DIR}/external/agility/"))

		message("-- Downloading and unzipping Microsoft.Direct3D.D3D12")

		file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/external/agility/")
		file(DOWNLOAD "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/" "${CMAKE_BINARY_DIR}/external/agility.zip")
		file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/external/agility/)

		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_BINARY_DIR}/external/agility.zip
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external/agility/
		)

		file(REMOVE "${CMAKE_BINARY_DIR}/external/agility.zip")

		write_file(
			"${CMAKE_BINARY_DIR}/external/agility/version.c"
			"#include <stdint.h>\n"
			"#include \"d3d12.h\"\n"
			"__declspec(dllexport) extern const uint32_t D3D12SDKVersion = D3D12_SDK_VERSION;\n"
			"__declspec(dllexport) extern const char *D3D12SDKPath = \"./D3D12/\";"
		)

	endif()

	# Add agility source files and includes as static lib

	file(GLOB_RECURSE agilityInclude "${CMAKE_BINARY_DIR}/external/agility/build/native/include")
	file(GLOB_RECURSE agilitySrc "${CMAKE_BINARY_DIR}/external/agility/build/native/src")

	add_library(
		DirectX12
		STATIC
		${agilityInclude}
		"${CMAKE_BINARY_DIR}/external/agility/version.c"
		CMakeLists.txt
	)

	target_link_libraries(DirectX12 PUBLIC dxgi d3d12)
	target_link_libraries(OxC3_graphics PUBLIC DirectX12)
	target_include_directories(DirectX12 PUBLIC "${CMAKE_BINARY_DIR}/external/agility/build/native/include")

	set_target_properties(DirectX12 PROPERTIES FOLDER External/D3D12)

	# Add an interface to link to that copies to target directory

	if(NOT ${ARM})

		# AMD AGS and NVAPI are required because DirectX12 was apparently not good enough :/
		# These are not supported for ARM, so don't include them there

		# NVAPI, used for NV extensions and getting driver version

		target_link_libraries(DirectX12 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/nvapi/amd64/nvapi64.lib")
		target_include_directories(DirectX12 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/nvapi")

		# AMD AGS, used to get driver version

		target_link_libraries(DirectX12 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/AGS_SDK/ags_lib/lib/amd_ags_x64.lib")
		target_include_directories(DirectX12 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/AGS_SDK/ags_lib/inc")

		add_custom_command(TARGET DirectX12 POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_BINARY_DIR}/external/warp/build/native/bin/x64/d3d10warp.dll"
			"${CMAKE_CURRENT_SOURCE_DIR}/external/AGS_SDK/ags_lib/lib/amd_ags_x64.dll"
			"${CMAKE_BINARY_DIR}/bin/$<CONFIG>/"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/D3D12/"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_BINARY_DIR}/external/agility/build/native/bin/x64/D3D12Core.dll"
			"${CMAKE_BINARY_DIR}/external/agility/build/native/bin/x64/d3d12SDKLayers.dll"
			"${CMAKE_BINARY_DIR}/bin/$<CONFIG>/D3D12/"
		)

	else()

		message(WARNING "-- ARM doesn't support d3d10warp or NVAPI, AMDAGS until the Direct3D.Warp package does and NVAPI/AMD AGS GitHub repo does")

		target_link_libraries(DirectX12 PUBLIC "${CMAKE_BINARY_DIR}/external/win_pix_runtime/bin/ARM64/WinPixEventRuntime.lib")

		add_custom_command(TARGET DirectX12 POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/D3D12/"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_BINARY_DIR}/external/agility/build/native/bin/arm64/D3D12Core.dll"
			"${CMAKE_BINARY_DIR}/external/agility/build/native/bin/arm64/d3d12SDKLayers.dll"
			"${CMAKE_BINARY_DIR}/bin/$<CONFIG>/D3D12/"
		)

	endif()

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
	endif()

endfunction()

# Tests

if(EnableTests)

	file(GLOB_RECURSE tests "tst/*.c")

	add_executable(
		OxC3_test
		${tests}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_test PUBLIC OxC3_platforms)

	set_target_properties(OxC3_test PROPERTIES FOLDER Oxsomi/test)

endif()

# Shader compiler, useful for baker and runtime if required by the app (though should only be used for editor applications)

if(EnableShaderCompiler)

	# Configure DXC

	set(CLANG_BUILD_EXAMPLES OFF CACHE BOOL "")
	set(CLANG_CL OFF CACHE BOOL "")
	set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
	set(CLANG_INCLUDE_TESTS OFF CACHE BOOL "")
	set(LLVM_DEFAULT_TARGET_TRIPLE "dxil-ms-dx" CACHE STRING "")
	set(LLVM_ENABLE_RTTI ON CACHE BOOL "")
	set(LLVM_ENABLE_EH ON CACHE BOOL "")
	set(LLVM_APPEND_VC_REV ON CACHE BOOL "")
	set(LLVM_OPTIMIZED_TABLEGEN OFF CACHE BOOL "")
	set(LIBCLANG_BUILD_STATIC ON CACHE BOOL "")
	set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "")
	set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "")
	set(LLVM_TARGETS_TO_BUILD "None" CACHE STRING "")
	set(HLSL_INCLUDE_TESTS OFF CACHE BOOL "")
	set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "")
	set(CLANG_ENABLE_ARCMT OFF CACHE BOOL "")
	set(SPIRV_BUILD_TESTS OFF CACHE BOOL "")
	set(ENABLE_SPIRV_CODEGEN ON CACHE BOOL "")
	set(HLSL_BUILD_DXILCONV OFF CACHE BOOL "")
	set(HLSL_ENABLE_FIXED_VER OFF CACHE BOOL "")
	set(HLSL_OFFICIAL_BUILD OFF CACHE BOOL "")
	set(HLSL_OPTIONAL_PROJS_IN_DEFAULT OFF CACHE BOOL "")
	
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "")
	set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "")
	set(DXC_USE_LIT ON CACHE BOOL "")
	set(LLVM_ENABLE_ASSERTIONS ON CACHE BOOL "")
	set(LLVM_LIT_ARGS "-v" CACHE STRING "")
	set(ENABLE_DXC_STATIC_LINKING ON CACHE BOOL "")

	add_subdirectory(external/DirectXShaderCompiler)

	# Our parser

	file(GLOB_RECURSE compilerHeaders "inc/shader_compiler/*.h")
	file(GLOB_RECURSE compilerSources "src/shader_compiler/*.c" "src/shader_compiler/*.cpp")	# Some CPP to interface with DXC

	add_library(
		OxC3_shader_compiler
		STATIC
		${compilerHeaders}
		${compilerSources}
		CMakeLists.txt
	)

	set_target_properties(OxC3_shader_compiler PROPERTIES FOLDER Oxsomi)
	target_link_libraries(OxC3_shader_compiler PUBLIC OxC3_platforms dxcompiler)

	target_include_directories(OxC3_shader_compiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/nvapi")
	target_include_directories(OxC3_shader_compiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXShaderCompiler/include")

endif()

# OxC3 tool

if(EnableOxC3CLI)

	file(GLOB_RECURSE headers "tools/inc/*.h")
	file(GLOB_RECURSE sources "tools/src/*.c")

	add_executable(
		OxC3
		${headers}
		${sources}
		CMakeLists.txt
	)

	set_target_properties(OxC3 PROPERTIES FOLDER Oxsomi)

	if(CLIGraphics)
		target_link_libraries(OxC3 PUBLIC OxC3_graphics)
		target_compile_definitions(OxC3 PUBLIC -DCLI_GRAPHICS)
	endif()

	if(EnableShaderCompiler)
		target_link_libraries(OxC3 PUBLIC OxC3_shader_compiler)
		target_compile_definitions(OxC3 PUBLIC -DCLI_SHADER_COMPILER)
	endif()

	target_include_directories(OxC3 PUBLIC tools/inc)

	configure_icon(OxC3 "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
	configure_virtual_files(OxC3)

endif()
