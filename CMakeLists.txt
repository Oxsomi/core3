cmake_minimum_required(VERSION 3.13.0)
project(ocore3)

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
	message(FATAL_ERROR "32-bit builds are disabled by core3 because it's legacy and could cause unintended behavior if builded anyways.")
endif()

if(MSVC)
    add_compile_options(/W4 /WX /MD /MP /GR /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror -mavx2)
endif()

# Types 

file(GLOB_RECURSE typesHeaders "inc/file/*.h" "inc/math/*.h" "inc/types/*.h")
file(GLOB_RECURSE typesSources "src/file/*.c" "src/math/*.c" "src/types/*.c")

add_library(
	core3_types
	STATIC
	${typesHeaders}
	${typesSources}
	CMakeLists.txt
)

target_include_directories(core3_types PRIVATE inc)

# Formats

file(GLOB_RECURSE formatsHeaders "inc/formats/*.h")
file(GLOB_RECURSE formatsSources "src/formats/*.c")

add_library(
	core3_formats
	STATIC
	${formatsHeaders}
	${formatsSources}
	CMakeLists.txt
)

target_include_directories(core3_formats PRIVATE inc)
target_link_libraries(core3_formats PRIVATE core3_types)

# Platforms

if(WIN32)
	set(platform windows)
	add_definitions(-D_PLATFORM_TYPE=Platform_Windows)
else()
	set(platform linux)
	add_definitions(-D_PLATFORM_TYPE=Platform_Linux)
endif()

message("-- Targetting platform: ${platform}")

file(GLOB_RECURSE platformsHeaders "inc/platforms/*.h")
file(GLOB_RECURSE platformsSources "src/platforms/${platform}/*.c" "src/platforms/generic/*.c")

add_library(
	core3_platforms
	STATIC
	${platformsHeaders}
	${platformsSources}
	CMakeLists.txt
)

target_include_directories(core3_platforms PRIVATE inc)
target_link_libraries(core3_platforms PRIVATE core3_types)

# Tests

file(GLOB_RECURSE tests "tst/*.c")

add_executable(
	core3_test
	${tests}
	CMakeLists.txt
)

target_include_directories(core3_test PRIVATE inc)
target_link_libraries(core3_test PRIVATE core3_types)