#  MIT License
#   
#  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
#  
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#  
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#  
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE. 

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
set(WorkingDir OFF CACHE BOOL "Use working directory as local root (OFF = Use file installation directory, ON = working dir)")

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

if(WorkingDir) 
	message("-- Enabling reads from the working dir instead of installation dir (-DWorkingDir=ON)")
	set(_WORKING_DIR "1")
else()
	message("-- Disabling reads from the working dir instead of installation dir (-DWorkingDir=OFF)")
	set(_WORKING_DIR "0")
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

target_compile_definitions(OxC3_types PUBLIC -D_SIMD=${_SIMD} -D_WORKING_DIR=${_WORKING_DIR} -D_RELAX_FLOAT=${_RELAX_FLOAT} -D_ARCH=${_ARCH} -D_PLATFORM_TYPE=${_PLATFORM_TYPE})

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

target_link_libraries(OxC3_formats PUBLIC OxC3_types)

# Platforms

message("-- Targetting platform: ${platform}")

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

target_link_libraries(OxC3_platforms PUBLIC OxC3_formats)

# Setting the icon of the app
# Call this immediately before configure_virtual_files with the executable

function(configure_icon target icon)

	if(WIN32)
		get_property(res TARGET ${target} PROPERTY RESOURCE_LIST)
		set_property(TARGET ${target} PROPERTY RESOURCE_LIST LOGO\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ICON\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \"${icon}\"\n${res})
		target_sources(${target} PRIVATE ${icon})
	endif()

endfunction()

# Configure virtual files (and icon) for a target

function(configure_virtual_files target)

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
