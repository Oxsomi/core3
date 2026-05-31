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
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

file(GLOB typesContainerHeaders "../../../include/types/container/*.h")
file(GLOB typesContainerHeadersSimd "../../../include/types/container/simd/*.h")
file(GLOB typesContainerHeadersSimdTarg "../../../include/types/container/simd/${SIMD}/*.h")
file(GLOB typesContainerSources "*.c")
file(GLOB_RECURSE typesContainerPlatformSources "platforms/${platform}/*.c")
file(GLOB_RECURSE typesContainerSIMDSources "simd/${simd}/*.c")

if(UNIX)
	file(GLOB_RECURSE typesContainerUnixSources "platforms/unix/*.c")
endif()

add_library(
	OxC3_types_container
	STATIC
	${typesContainerHeaders}
	${typesContainerHeadersSimd}
	${typesContainerHeadersSimdTarg}
	${typesContainerSources}
	${typesContainerPlatformSources}
	${typesContainerSIMDSources}
	${typesContainerUnixSources}
	CMakeLists.txt
)

if("${simd}" STREQUAL "sse")
	if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		set_source_files_properties(
			simd/sse/buffer_encrypt_avx256.c
			PROPERTIES
				COMPILE_OPTIONS "-maes;-mpclmul;-mavx2;-mvaes;-mvpclmulqdq;-mavx512vl"
		)
		set_source_files_properties(
			simd/sse/buffer_encrypt_avx512.c
			PROPERTIES
				COMPILE_OPTIONS "-mavx512f;-mavx512dq;-mavx512bw;-mvaes;-mvpclmulqdq;-mavx512vl"
		)
	endif()
endif()

source_group("Source Files (${platform})" FILES ${typesContainerPlatformSources})
source_group("Source Files (simd: ${simd})" FILES ${typesContainerSIMDSources})
source_group("Source Files (unix)" FILES ${typesContainerUnixSources})
source_group("Header Files (simd: ${simd})" FILES ${typesContainerHeadersSimdTarg})

set_target_properties(OxC3_types_container PROPERTIES FOLDER Oxsomi/types)
target_link_libraries(OxC3_types_container PUBLIC OxC3_types_base OxC3_types_math)

if(WIN32)
	target_link_libraries(OxC3_types_container PUBLIC Bcrypt.lib)
elseif(APPLE)
	target_link_libraries(OxC3_types_container PUBLIC "-framework Security")
elseif(ANDROID)
	target_link_libraries(OxC3_types_container PUBLIC android log)
endif()

if(EnableTests)

	file(GLOB typesContainerTestfH "../../../include/types/container/test/*.h")
	file(GLOB typesContainerTestfC "test/shared/*.c")

	add_library(
		OxC3_types_container_test_util STATIC
		${typesContainerTestfH}
		${typesContainerTestfC}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_types_container_test_util PUBLIC OxC3_types_container OxC3_types_test)
	set_target_properties(OxC3_types_container_test_util PROPERTIES FOLDER Oxsomi/test/types)

	file(GLOB testsC "test/*.c")
	file(GLOB testsH "test/*.h")

	add_executable(
		OxC3_types_container_test
		${testsC}
		${testsH}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_types_container_test PUBLIC OxC3_types_container_test_util)
	set_target_properties(OxC3_types_container_test PROPERTIES FOLDER Oxsomi/test/types)

endif()

if(EnableOxC3CLI)

	file(GLOB typesContainerPerfH "../../../include/types/container/perf/*.h")
	file(GLOB typesContainerPerfC "perf/*.c")

	add_library(
		OxC3_types_container_perf_lib STATIC
		${typesContainerPerfH}
		${typesContainerPerfC}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_types_container_perf_lib PUBLIC OxC3_types_container)
	set_target_properties(OxC3_types_container_perf_lib PROPERTIES FOLDER Oxsomi/perf)

	file(GLOB testsExecC "perf/exec/*.c")

	add_executable(
		OxC3_types_container_perf
		${testsExecC}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_types_container_perf PUBLIC OxC3_types_container_perf_lib)
	set_target_properties(OxC3_types_container_perf PROPERTIES FOLDER Oxsomi/perf)

endif()
