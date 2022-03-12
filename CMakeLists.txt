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

if(MSVC)
    add_compile_options(/W4 /WX /MD /MP /GR /arch:AVX2 /D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror -mavx2)
endif()

# Types 

file(GLOB_RECURSE typesHeaders "inc/*.h")
file(GLOB_RECURSE typesSources "src/*.c")

add_library(
	typesOC3
	STATIC
    ${typesHeaders}
    ${typesSources}
	CMakeLists.txt
)

target_include_directories(typesOC3 PRIVATE inc/types)

file(GLOB_RECURSE tests "tst/*.c")

add_executable(
	testsOC3
    ${tests}
)

target_include_directories(testsOC3 PRIVATE inc/types)
target_link_libraries(testsOC3 PRIVATE typesOC3)