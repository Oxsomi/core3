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

set(WorkingDir FORCE ON CACHE BOOL "Use working directory as local root (OFF = Use file installation directory, ON = working dir)")

add_subdirectory(.. ${CMAKE_BINARY_DIR}/bin/OxC3)

file(GLOB_RECURSE headers "inc/*.h")
file(GLOB_RECURSE sources "src/*.c")

add_executable(
	OxC3
	${headers}
	${sources}
	CMakeLists.txt
)

target_link_libraries(OxC3 PUBLIC OxC3_platforms)
target_include_directories(OxC3 PUBLIC inc)