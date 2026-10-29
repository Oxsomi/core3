# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

file(GLOB_RECURSE typesMathHeaders CONFIGURE_DEPENDS "../../../include/types/math/*.h")
file(GLOB typesMathSources CONFIGURE_DEPENDS "*.c")
file(GLOB_RECURSE typesMathSIMDSources CONFIGURE_DEPENDS "simd/${simdMath}/*.c")

add_library(
	OxC3_types_math
	STATIC
	${typesMathHeaders}
	${typesMathSources}
	${typesMathPlatformSources}
	${typesMathSIMDSources}
	CMakeLists.txt
)

source_group("Source Files (${platform})" FILES ${typesMathPlatformSources})
source_group("Source Files (simd: ${simdMath})" FILES ${typesMathSIMDSources})

set_target_properties(OxC3_types_math PROPERTIES FOLDER Oxsomi/types)
target_link_libraries(OxC3_types_math PUBLIC OxC3_types_base)

target_compile_definitions(OxC3_types_math PUBLIC -D_ENABLE_SIMD=${SIMD} -D_FORCE_FLOAT_FALLBACK=${_FORCE_FLOAT_FALLBACK})

# Public inline functions use the same instructions and vector ABI as the library.
# Private build options do not propagate, so consumers need the target's interface too.

if(OXC3_SIMD_FLAGS)
	target_compile_options(OxC3_types_math INTERFACE "$<$<COMPILE_LANGUAGE:C,CXX>:${OXC3_SIMD_FLAGS}>")
endif()

if(EnableTests AND OxC3TestExecutables)

	export(TARGETS OxC3_types_base OxC3_types_math
		NAMESPACE OxC3::
		FILE "${CMAKE_BINARY_DIR}/OxC3TypesTargets.cmake"
	)

	include(../../../cmake/oxc3.cmake)

	oxc3_add_test(
		NAME     OxC3_types_math_test
		FOLDER   Oxsomi/test/types
		LIBS     OxC3_types_math OxC3_types_test
	)

	if(NOT CMAKE_CROSSCOMPILING)

		# Link settings support the producer's instrumented or LTO archives; header requirements come from the targets.
		get_directory_property(consumerDirectoryLinkOptions LINK_OPTIONS)
		set(consumerLinkOptions ${OXC3_BUILD_LINK_OPTIONS} ${consumerDirectoryLinkOptions})
		get_directory_property(consumerLinkDirectories LINK_DIRECTORIES)
		set(consumerRuntimeFiles "")
		if(isClangCl AND EnableASAN)
			set(consumerRuntimeFiles "${clangRtDir}/clang_rt.asan_dynamic-${sanitizerArch}.dll")
		endif()

		configure_file(test/consumer/link_settings.cmake.in consumer-link-settings.in.cmake @ONLY)
		file(GENERATE
			OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/consumer-link-settings-$<CONFIG>.cmake"
			INPUT "${CMAKE_CURRENT_BINARY_DIR}/consumer-link-settings.in.cmake"
			TARGET OxC3_types_math
		)
		configure_file(test/consumer/run_consumer.cmake.in consumer-run.in.cmake @ONLY)
		file(GENERATE
			OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/consumer-run-$<CONFIG>.cmake"
			INPUT "${CMAKE_CURRENT_BINARY_DIR}/consumer-run.in.cmake"
		)

		oxc3_add_test_command(
			NAME OxC3_types_math_consumer_test
			TARGET OxC3_types_math
			FOLDER Oxsomi/test/types
			WORKING_DIR "${CMAKE_BINARY_DIR}"
			COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/consumer-run-$<CONFIG>.cmake"
			DATA test/consumer/consumer.c test/consumer/consumer.cpp test/consumer/consumer_headers.cpp
				test/consumer/CMakeLists.txt
				"${CMAKE_CURRENT_BINARY_DIR}/consumer-run-$<CONFIG>.cmake"
				"${CMAKE_CURRENT_BINARY_DIR}/consumer-link-settings-$<CONFIG>.cmake"
				"${CMAKE_BINARY_DIR}/OxC3TypesTargets.cmake"
			DEPS OxC3_types_base
		)
	endif()

endif()
