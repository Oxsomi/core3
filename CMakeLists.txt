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
set(CMAKE_SUPPRESS_REGENERATION TRUE)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)				# We need some C++ to interface with DXC, but we wrap it using C
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "32-bit builds are disabled by core3 because it's legacy and could cause unintended behavior if built anyways.")
endif()

set(EnableSIMD ON CACHE BOOL "Enables SIMD")
set(CLIGraphics ON CACHE BOOL "Turn off/on OxC3 graphics CLI (might be unsupported on device)")
set(EnableShaderCompiler OFF CACHE BOOL "Turn off/on shader compiler + OxC3 compile shaders CLI (increases compile time)")

set(EnableTests ON CACHE BOOL "Turn off/on OxC3 tests")
set(EnableOxC3CLI OFF CACHE BOOL "Turn on/off OxC3 CLI")

# Set general compile options

if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") 
	message("-- Enabling ARM specific optimizations")
	set(ARM ON)
else()
	message("-- Enabling x64 specific optimizations")
	set(ARM OFF)
endif()

if(MSVC)

	add_compile_options(/MP /GR /D_CRT_SECURE_NO_WARNINGS /Gy /W4 /WX)

	if(EnableSIMD)
		if(ARM)
			add_compile_options(/arch:ARMv7VE)		# Enable ARM arch
		else()
			add_compile_options(/d2archSSE42)		# Require SSE4.2
		endif()
	endif()

	add_compile_options(/fp:strict)

else()

	if(ARM)
		add_compile_options(-mfpu=neon -march=armv8-a+crc+simd+crypto)		# We enable neon, but we don't use it in vectors yet
	else()
		if(EnableSIMD)
			add_compile_options(-mfpmath=sse -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mrdrnd -msha -maes -mbmi2 -mf16c)
		else()
			add_compile_options(-mfpmath=sse -msse)		# Consistent float behavior
		endif()
	endif()

	add_compile_options(-fno-unsafe-math-optimizations)
	add_compile_options(-fms-extensions)
	add_compile_options(-flto -Wall -Wextra -Werror)
	
	if(NOT CMAKE_${lang}_COMPILER_VERSION VERSION_LESS 4.7)
		add_compile_options(-fno-fat-lto-objects)
	endif()

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
	set(simd sse)
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

# Enable symbols

if("${platform}" STREQUAL "linux")
	if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		set(CMAKE_ENABLE_EXPORTS ON)
	endif()
endif()

set(ForceUpdateDeps OFF CACHE BOOL "Force update dependencies")
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

target_compile_definitions(OxC3_types PUBLIC -D_ENABLE_SIMD=${SIMD} -D_FORCE_FLOAT_FALLBACK=${_FORCE_FLOAT_FALLBACK})

target_include_directories(OxC3_types PUBLIC inc)

if(WIN32)
	target_link_libraries(OxC3_types PUBLIC Bcrypt.lib)
elseif(APPLE)
	target_link_libraries(OxC3_types PUBLIC "-framework Security")
else()
	target_link_libraries(OxC3_types PUBLIC m)
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
# file(GLOB_RECURSE shaders "src/*.hlsli")

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

	if(NOT DEFINED ENV{VULKAN_SDK})
		message(FATAL_ERROR "VULKAN_SDK directory should be set!")
	endif()

	if(NOT "${platform}" STREQUAL "osx")
		include_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/include)
		target_link_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/lib)
	endif()

	if(WIN32)
		target_link_libraries(OxC3_graphics PUBLIC vulkan-1)
	elseif("${platform}" STREQUAL "osx")

		if(EXISTS "$ENV{VULKAN_SDK}/macOS/include")
			include_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/macOS/include)
			target_link_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/macOS/lib)
		else()
			include_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/include)
			target_link_directories(OxC3_graphics PUBLIC $ENV{VULKAN_SDK}/lib)
		endif()

		target_link_libraries(OxC3_graphics PUBLIC vulkan)

	else()
		target_link_libraries(OxC3_graphics PUBLIC vulkan)
	endif()

	if("${platform}" STREQUAL "windows")
		add_definitions(-D_VK_SURFACE_EXT="VK_KHR_win32_surface")
	elseif("${platform}" STREQUAL "android")
		add_definitions(-D_VK_SURFACE_EXT="VK_KHR_android_surface")
	elseif("${platform}" STREQUAL "osx")
		add_definitions(-D_VK_SURFACE_EXT="VK_MVK_macos_surface")
	elseif("${platform}" STREQUAL "ios")
		add_definitions(-D_VK_SURFACE_EXT="VK_MVK_ios_surface")
	elseif("${platform}" STREQUAL "linux")
		add_definitions(-D_VK_SURFACE_EXT="VK_KHR_wayland_surface")
	else()
		message(FATAL_ERROR "Platform not supported for Vulkan yet: ${platform}")
	endif()

# DirectX setup

elseif("${graphicsApi}" STREQUAL "directx12")

	add_library(
		DirectX12
		INTERFACE
		CMakeLists.txt
	)

	find_package(agility_sdk REQUIRED)

	target_link_libraries(DirectX12 INTERFACE agility_sdk::agility_sdk)

	target_include_directories(DirectX12 INTERFACE "${CMAKE_BINARY_DIR}/external/agility/build/native/include")

	if(NOT ${ARM})

		# AMD AGS and NVAPI are required because DirectX12 was apparently not good enough :/
		# These are not supported for ARM, so don't include them there

		# NVAPI, used for NV extensions and getting driver version

		find_package(ags REQUIRED)
		find_package(nvapi REQUIRED)

		target_link_libraries(DirectX12 INTERFACE ags::ags nvapi::nvapi)

	endif()

	target_link_libraries(OxC3_graphics PUBLIC DirectX12)

elseif("${graphicsApi}" STREQUAL "metal")
	message(FATAL_ERROR "Graphics api not supported yet: Metal")
elseif("${graphicsApi}" STREQUAL "webgpu")
	message(FATAL_ERROR "Graphics api not supported yet: WebGPU")
else()
	message(FATAL_ERROR "Graphics api not supported: ${graphicsApi}")
endif()

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

	find_package(dxc REQUIRED)
	find_package(nvapi REQUIRED)
	find_package(spirv_reflect REQUIRED)

	# Our parser & calling DXC and spirv-opt, spirv-reflect, DXIL reflect, etc.

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
	target_link_libraries(OxC3_shader_compiler PUBLIC OxC3_platforms dxc::dxc spirv_reflect::spirv_reflect nvapi::nvapi)

	target_include_directories(OxC3_shader_compiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/nvapi")
	target_include_directories(OxC3_shader_compiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external")

endif()

# OxC3 tool

if(EnableOxC3CLI)

	# CLI, useful for manually calling CLI utility functions

	file(GLOB headers "inc/tools/*.h")
	file(GLOB sources "src/tools/*.c")

	add_library(
		OxC3CLI STATIC
		${headers}
		${sources}
		CMakeLists.txt
	)

	set_target_properties(OxC3CLI PROPERTIES FOLDER Oxsomi)

	if(CLIGraphics)
		target_link_libraries(OxC3CLI PUBLIC OxC3_graphics)
		target_compile_definitions(OxC3CLI PUBLIC -DCLI_GRAPHICS)
	endif()

	if(EnableShaderCompiler)
		target_link_libraries(OxC3CLI PUBLIC OxC3_shader_compiler)
		target_compile_definitions(OxC3CLI PUBLIC -DCLI_SHADER_COMPILER)
	endif()

	target_include_directories(OxC3CLI PUBLIC inc/tools)

	# OxC3 executable, for easily executing OxC3CLI commands

	file(GLOB sources "src/tools/executable/*.c")

	add_executable(
		OxC3
		${sources}
		CMakeLists.txt
	)

	set_target_properties(OxC3 PROPERTIES FOLDER Oxsomi)
	target_link_libraries(OxC3 PUBLIC OxC3CLI)

	include(cmake/oxc3.cmake)
	configure_icon(OxC3 "${CMAKE_CURRENT_SOURCE_DIR}/res/logo.ico")
	configure_virtual_files(OxC3)

endif()
