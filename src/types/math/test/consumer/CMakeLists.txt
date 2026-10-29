cmake_minimum_required(VERSION 3.20)
project(OxC3TypesConsumer C CXX)

# Configure this as its own project so no producer directory flags reach the consumer.
# OXC3_TYPES_TARGETS is the build tree's exported OxC3TypesTargets.cmake.
# OXC3_LIBRARY_CONFIG optionally maps this consumer's configurations to one library configuration.

if(NOT OXC3_TYPES_TARGETS)
	message(FATAL_ERROR "OXC3_TYPES_TARGETS must name OxC3TypesTargets.cmake from a types test build")
endif()

include("${OXC3_TYPES_TARGETS}")

if(OXC3_LIBRARY_CONFIG)
	foreach(target OxC3::OxC3_types_base OxC3::OxC3_types_math)
		foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
			set_property(TARGET ${target} PROPERTY MAP_IMPORTED_CONFIG_${config} "${OXC3_LIBRARY_CONFIG}")
		endforeach()
	endforeach()
endif()

add_executable(types_consumer consumer.c)
add_executable(types_consumer_cpp consumer.cpp)
add_library(types_consumer_cpp_headers OBJECT consumer_headers.cpp)

# The imported targets must raise these baselines to the public headers' language requirements.
set_target_properties(types_consumer PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
set_target_properties(types_consumer_cpp types_consumer_cpp_headers PROPERTIES
	CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
)

foreach(target types_consumer types_consumer_cpp types_consumer_cpp_headers)
	target_compile_definitions(${target} PRIVATE
		OXC3_EXPECTED_SIMD=${OXC3_EXPECTED_SIMD}
		OXC3_EXPECTED_ERROR_FRAMES=${OXC3_EXPECTED_ERROR_FRAMES}
	)
	target_link_libraries(${target} PRIVATE OxC3::OxC3_types_math)
endforeach()

if(OXC3_TEST_LINK_SETTINGS)
	include("${OXC3_TEST_LINK_SETTINGS}")
	foreach(target types_consumer types_consumer_cpp)
		target_link_options(${target} PRIVATE ${OXC3_TEST_LINK_OPTIONS})
		target_link_directories(${target} PRIVATE ${OXC3_TEST_LINK_DIRECTORIES})
		foreach(runtime IN LISTS OXC3_TEST_RUNTIME_FILES)
			add_custom_command(TARGET ${target} POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${runtime}" "$<TARGET_FILE_DIR:${target}>"
				VERBATIM
			)
		endforeach()
	endforeach()
endif()

enable_testing()
add_test(NAME types_consumer COMMAND types_consumer)
add_test(NAME types_consumer_cpp COMMAND types_consumer_cpp)
