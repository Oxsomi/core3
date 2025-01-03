# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

# SET(GRAPHVIZ_CUSTOM_TARGETS TRUE)

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
set(DynamicLinkingGraphics OFF CACHE BOOL "Turn on/off OxC3 graphics dynamic linking")

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
		if(EnableSIMD)
			add_compile_options(-march=armv8-a+crc+simd+crypto)
		else()
			add_compile_options(-march=armv8-a+simd)
		endif()
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

	# add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
	# add_link_options(-fsanitize=address -fno-omit-frame-pointer)
	# add_compile_options(-fsanitize=undefined)
	# add_link_options(-fsanitize=undefined)
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
else()
	message("-- Disabling OxC3 unit tests (-DEnableTests=Off)")
endif()

if(EnableShaderCompiler)
	message("-- Enabling OxC3 compile shaders (-DEnableShaderCompiler=On)")
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
set(ForceVulkan OFF CACHE BOOL "Force Vulkan support (disable native API if static linking)")

add_subdirectory(src)
