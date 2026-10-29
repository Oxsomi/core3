cmake_minimum_required(VERSION 3.24)
project(OxC3PackageConsumer LANGUAGES C CXX)

find_package(oxc3 CONFIG REQUIRED)
enable_testing()

if(ANDROID)
	# Android's platform entrypoint is an activity exported by a shared library.
	add_library(oxc3_c SHARED test_oxc3_package.c)
	add_library(oxc3_cpp SHARED test_oxc3_package.cpp)
else()
	add_executable(oxc3_c test_oxc3_package.c)
	add_executable(oxc3_cpp test_oxc3_package.cpp)
endif()

if(OXC3_TEST_GRAPHICS)

	set(shaderArchive "${OXC3_TEST_PACKAGE_BIN}/packages/OxC3_graphics/shaders.oiCA")

	if(NOT EXISTS "${shaderArchive}")
		message(FATAL_ERROR "Installed graphics shader archive is missing: ${shaderArchive}")
	endif()

	# apply_dependencies embeds the installed archive under its original virtual file namespace.
	set_property(TARGET oxc3_c PROPERTY RESOURCE_LIST "${shaderArchive}")

endif()

# The public C APIs can pull in OpenAL and the optional static shader compiler, which are implemented in C++.
set_target_properties(oxc3_c PROPERTIES LINKER_LANGUAGE CXX)

foreach(target oxc3_c oxc3_cpp)

	target_link_libraries(${target} PRIVATE oxc3::oxc3)
	target_compile_definitions(${target} PRIVATE
		OXC3_TEST_SIMD=${OXC3_TEST_SIMD}
		OXC3_TEST_ERROR_FRAMES=${OXC3_TEST_ERROR_FRAMES}
		OXC3_TEST_COMPILER=$<BOOL:${OXC3_TEST_COMPILER}>
		OXC3_TEST_DYNAMIC_COMPILER=$<BOOL:${OXC3_TEST_DYNAMIC_COMPILER}>
		OXC3_TEST_GRAPHICS=$<BOOL:${OXC3_TEST_GRAPHICS}>
	)

	if(NOT ANDROID OR target STREQUAL "oxc3_c")
		apply_dependencies(${target})
	endif()

	apply_web_link_options(${target})

	if(NOT ANDROID)
		add_test(NAME ${target} COMMAND ${target})
		set_tests_properties(${target} PROPERTIES WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")
	endif()

endforeach()

if(OXC3_TEST_DYNAMIC_COMPILER)

	set(compilerModule "${CMAKE_SHARED_LIBRARY_PREFIX}OxC3_shader_compiler${CMAKE_SHARED_LIBRARY_SUFFIX}")

	add_test(NAME oxc3_runtime
		COMMAND "${CMAKE_COMMAND}"
			"-DPACKAGE_MODULE=${OXC3_TEST_PACKAGE_BIN}/${compilerModule}"
			"-DCONSUMER_MODULE=$<TARGET_FILE_DIR:oxc3_c>/${compilerModule}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/check_runtime.cmake"
	)

endif()
