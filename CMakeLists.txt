cmake_minimum_required(VERSION 3.13.0)
project(ocore3)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

IF (MSVC)
  SET(CMAKE_CXX_FLAGS_DEBUG "/EHsc /RTC1 /MTd /ZI /Od /D_DEBUG")
  SET(CMAKE_CXX_FLAGS_RELEASE "/EHsc /MT /O2 /DNDEBUG")
  
  STRING(REGEX REPLACE "/MDd" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
  STRING(REGEX REPLACE "/MD" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
  
  SET (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
  SET (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")

  SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG /NODEFAULTLIB:\"MSVCRTD\"")
  SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG /NODEFAULTLIB:\"MSVCRTD\"")
  SET(CMAKE_STATIC_LINKER_FLAGS_DEBUG "/NODEFAULTLIB:\"MSVCRTD\"")

  SET(CMAKE_EXE_LINKER_FLAGS "/INCREMENTAL")
  SET(CMAKE_SHARED_LINKER_FLAGS "")
  SET(CMAKE_STATIC_LINKER_FLAGS "")
  
  SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "/NODEFAULTLIB:\"MSVCRT\"")
  SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/NODEFAULTLIB:\"MSVCRT\"")
  SET(CMAKE_STATIC_LINKER_FLAGS_RELEASE "/NODEFAULTLIB:\"MSVCRT\"")

  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
  SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

  STRING(REGEX REPLACE "/W[01234]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /W4 /wd4201 /wd4464 /wd4505 /MP")
  
  STRING(REGEX REPLACE "/W[01234]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX /W4 /wd4201 /wd4464 /wd4505 /MP")
ENDIF ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(EnableSIMD ON CACHE BOOL "Enables SIMD")

if(EnableSIMD) 

	if(MSVC) 
		add_compile_options(/arch:AVX2 )
	else()
		add_compile_options(-mavx2)
	endif()

	set(_SIMD "SIMD_SSE")
else()
	set(_SIMD "SIMD_NONE")
endif()

configure_file(config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/inc/cfg/config.h)

# Types 

file(GLOB_RECURSE typesHeaders "inc/file/*.h" "inc/math/*.h" "inc/types/*.h" "inc/cfg/*.h" "*.h.in")
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
