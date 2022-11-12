cmake_minimum_required(VERSION 3.13.0)
project(OxC3_virtual_package)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(WorkingDir FORCE ON CACHE BOOL "Use working directory as local root (OFF = Use file installation directory, ON = working dir)")

add_subdirectory(.. ${CMAKE_BINARY_DIR}/bin/OxC3)

file(GLOB_RECURSE packageHeaders "inc/*.h")
file(GLOB_RECURSE packageSources "src/*.c")

add_executable(
	OxC3_virtual_package
	${packageHeaders}
	${packageSources}
	CMakeLists.txt
)

target_link_libraries(OxC3_virtual_package PUBLIC OxC3_platforms)