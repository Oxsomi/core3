cmake_minimum_required(VERSION 3.26)

# Configure-only checks: the fixture uses a declared compiler identity and stub rules.
# No compiler, linker, or shared project build is run.
# Run with cmake -DTEST_BINARY_DIR=<temporary-directory> -P cmake/tests/test_build_options.cmake.

get_filename_component(oxc3Source "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT TEST_BINARY_DIR)
	set(TEST_BINARY_DIR "${oxc3Source}/build/cmake-build-options-test")
endif()

find_program(testNinja NAMES ninja ninja-build REQUIRED)
file(MAKE_DIRECTORY "${TEST_BINARY_DIR}/source")

file(WRITE "${TEST_BINARY_DIR}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.26)

foreach(language C CXX)

	set(CMAKE_${language}_COMPILER "${CMAKE_COMMAND}")
	set(CMAKE_${language}_COMPILER_ID_RUN TRUE)
	set(CMAKE_${language}_COMPILER_ID GNU)
	set(CMAKE_${language}_COMPILER_VERSION 14.2.1)
	set(CMAKE_${language}_COMPILER_FORCED TRUE)
	set(CMAKE_${language}_COMPILER_WORKS TRUE)
	set(CMAKE_${language}_ABI_COMPILED TRUE)
endforeach()

project(OxC3BuildOptionsFixture LANGUAGES C CXX)

set(CMAKE_C_COMPILE_OBJECT "stub-compile <SOURCE> <OBJECT>")
set(CMAKE_C_ARCHIVE_CREATE "stub-archive <TARGET> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH "")
set(CMAKE_C_LINK_EXECUTABLE "stub-link <TARGET> <OBJECTS>")

set(MSVC FALSE)
set(EnableSIMD TRUE)
set(EnableASAN FALSE)
set(EnableUBSAN TRUE)
set(ARM FALSE)
set(EMSCRIPTEN FALSE)

if(TEST_CASE STREQUAL "arm")
	set(ARM TRUE)

elseif(TEST_CASE STREQUAL "wasm_scalar")
	set(EMSCRIPTEN TRUE)
	set(EnableSIMD FALSE)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -DKEEP_C_RELEASE_FLAG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DKEEP_CXX_RELEASE_FLAG")
include("${OXC3_SOURCE_DIR}/cmake/build_options.cmake")

if (
	NOT CMAKE_C_FLAGS_RELEASE STREQUAL "-O3 -DNDEBUG -DKEEP_C_RELEASE_FLAG" OR
	NOT CMAKE_CXX_FLAGS_RELEASE STREQUAL "-O3 -DNDEBUG -DKEEP_CXX_RELEASE_FLAG"
)
	message(FATAL_ERROR "Build policy replaced caller Release flags")
endif()

get_directory_property(directoryCompileOptions COMPILE_OPTIONS)
get_directory_property(directoryLinkOptions LINK_OPTIONS)

if(directoryCompileOptions OR directoryLinkOptions)
	message(FATAL_ERROR "Build policy leaked into directory options")
endif()

if(TEST_CASE STREQUAL "x64")

	if(NOT "-mavx" IN_LIST OXC3_SIMD_FLAGS OR NOT OXC3_BUILD_COMPILE_OPTIONS MATCHES "mprefer-vector-width=128")
		message(FATAL_ERROR "x64 SIMD or GCC workaround missing")
	endif()

elseif(TEST_CASE STREQUAL "arm")

	if (
		NOT OXC3_SIMD_FLAGS STREQUAL "-march=armv8-a+simd+crypto+crc" OR
		OXC3_BUILD_COMPILE_OPTIONS MATCHES "mprefer-vector-width|mfpmath|msse"
	)
		message(FATAL_ERROR "ARM received an incorrect ISA baseline or x86 options")
	endif()

elseif(TEST_CASE STREQUAL "wasm_scalar")

	if(OXC3_SIMD_FLAGS OR OXC3_BUILD_COMPILE_OPTIONS MATCHES "mprefer-vector-width|mfpmath|msse")
		message(FATAL_ERROR "Scalar wasm received native SIMD options")
	endif()
endif()

file(WRITE "${CMAKE_BINARY_DIR}/dummy.c" "int main(void) { return 0; }\n")
add_library(OxC3_owned STATIC "${CMAKE_BINARY_DIR}/dummy.c")
target_compile_options(OxC3_owned PRIVATE -fno-sanitize=all)
target_link_options(OxC3_owned PRIVATE -fno-sanitize=all)
target_compile_options(OxC3_owned INTERFACE "$<$<COMPILE_LANGUAGE:C,CXX>:${OXC3_SIMD_FLAGS}>")
add_executable(OxC3 "${CMAKE_BINARY_DIR}/dummy.c")
add_library(OxC3CLI STATIC "${CMAKE_BINARY_DIR}/dummy.c")
add_executable(plain_consumer "${CMAKE_BINARY_DIR}/dummy.c")
target_link_libraries(plain_consumer PRIVATE OxC3_owned)
add_library(third_party STATIC "${CMAKE_BINARY_DIR}/dummy.c")
add_library(OxC3_imported STATIC IMPORTED)
add_library(OxC3_headers INTERFACE)
add_custom_target(OxC3_utility)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/child")
file(WRITE "${CMAKE_BINARY_DIR}/child/CMakeLists.txt"
	"add_library(OxC3_child STATIC \"${CMAKE_BINARY_DIR}/dummy.c\")\n"
)

add_subdirectory("${CMAKE_BINARY_DIR}/child" child-build)
oxc3_apply_build_options("${CMAKE_CURRENT_SOURCE_DIR}")

foreach(target OxC3_owned OxC3 OxC3CLI OxC3_child)

	get_target_property(options ${target} COMPILE_OPTIONS)

	if(NOT options MATCHES "-Werror")
		message(FATAL_ERROR "Private build policy missing from ${target}")
	endif()
endforeach()

foreach(target third_party plain_consumer OxC3_imported OxC3_headers OxC3_utility)

	get_target_property(applied ${target} OXC3_BUILD_OPTIONS_APPLIED)

	if(applied)
		message(FATAL_ERROR "Build policy applied to excluded target ${target}")
	endif()
endforeach()

get_target_property(before OxC3_owned COMPILE_OPTIONS)
oxc3_apply_build_options("${CMAKE_CURRENT_SOURCE_DIR}")
get_target_property(after OxC3_owned COMPILE_OPTIONS)

if(NOT before STREQUAL after)
	message(FATAL_ERROR "Applying build policy twice changed the target")
endif()

get_target_property(linkOptions OxC3_owned LINK_OPTIONS)

if(NOT linkOptions MATCHES "-fsanitize=undefined.*-fno-sanitize=all")
	message(FATAL_ERROR "Private link policy overrides a target's local setting")
endif()

export(TARGETS OxC3_owned FILE "${CMAKE_BINARY_DIR}/exports.cmake")
]=])

function(read_target_flags buildFile target result)

	file(READ "${buildFile}" ninjaFile)
	string(FIND "${ninjaFile}" "build CMakeFiles/${target}.dir/" start)

	if(start LESS 0)
		message(FATAL_ERROR "No compile rule for ${target}")
	endif()

	string(SUBSTRING "${ninjaFile}" ${start} -1 rule)
	string(FIND "${rule}" "\n\n" end)
	string(SUBSTRING "${rule}" 0 ${end} rule)
	string(REGEX MATCH "FLAGS = [^\n]*" flags "${rule}")
	set(${result} "${flags}" PARENT_SCOPE)
endfunction()

foreach(testCase x64 arm wasm_scalar)

	set(caseBinary "${TEST_BINARY_DIR}/${testCase}")

	execute_process(
		COMMAND "${CMAKE_COMMAND}" -S "${TEST_BINARY_DIR}/source" -B "${caseBinary}" -G Ninja
			"-DCMAKE_MAKE_PROGRAM=${testNinja}" -DCMAKE_BUILD_TYPE=Release
			"-DOXC3_SOURCE_DIR=${oxc3Source}" "-DTEST_CASE=${testCase}"
		RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors
	)

	if(NOT status EQUAL 0)
		message(FATAL_ERROR "${testCase} configure failed:\n${output}\n${errors}")
	endif()

	read_target_flags("${caseBinary}/build.ninja" OxC3_owned ownedFlags)

	if(NOT ownedFlags MATCHES "-fsanitize=undefined.*-fno-sanitize=all")
		message(FATAL_ERROR "${testCase}: private compile policy overrides a local setting: ${ownedFlags}")
	endif()

	if(testCase STREQUAL "wasm_scalar" AND NOT ownedFlags MATCHES "-O3.*-DKEEP_C_RELEASE_FLAG.*-O2")
		message(FATAL_ERROR "wasm optimization override lost or replaced caller flags: ${ownedFlags}")
	endif()

	read_target_flags("${caseBinary}/build.ninja" plain_consumer consumerFlags)

	if(consumerFlags MATCHES "-Werror|-fsanitize=undefined|-fno-sanitize=all")
		message(FATAL_ERROR "${testCase}: private flags reached a consumer: ${consumerFlags}")
	endif()

	file(READ "${caseBinary}/exports.cmake" exports)

	if(exports MATCHES "-Werror|-fsanitize|LINK_ONLY")
		message(FATAL_ERROR "${testCase}: private policy leaked into the exported interface")
	endif()

	message(STATUS "${testCase}: flags, overrides, recursion and export privacy passed")
endforeach()
