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

message("-- Targeting audio API: OpenAL")
find_package(openal_soft REQUIRED)

file(GLOB_RECURSE graphicsHeaders CONFIGURE_DEPENDS "../../include/audio/*.h")
file(GLOB graphicsSources CONFIGURE_DEPENDS "*.c")

file(GLOB_RECURSE apiPlatformSources CONFIGURE_DEPENDS "openal_soft/*.c")
file(GLOB_RECURSE apiPlatformIncludes CONFIGURE_DEPENDS "../../include/audio/openal_soft/*.h")

add_library(
	OxC3_audio
	STATIC
	${graphicsHeaders}
	${graphicsSources}
	${apiPlatformSources}
	${apiPlatformIncludes}
	CMakeLists.txt
)

source_group("Source Files (openal_soft)" FILES ${apiPlatformSources})
source_group("Header Files (openal_soft)" FILES ${apiPlatformIncludes})

set_target_properties(OxC3_audio PROPERTIES FOLDER Oxsomi)
target_link_libraries(OxC3_audio PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container openal_soft::openal_soft OxC3_formats_wav)

set(AUDIO_TEST_DEBUG OFF)

if(AUDIO_TEST_DEBUG)
	target_link_libraries(OxC3_audio PUBLIC OxC3_platforms)
	target_compile_definitions(OxC3_audio PUBLIC AUDIO_TEST_DEBUG)
endif()

if(EnableTests AND OxC3TestExecutables)

	file(GLOB testsInterfaceC CONFIGURE_DEPENDS "test/interface/*.c")
	file(GLOB testsInterfaceH CONFIGURE_DEPENDS "test/interface/*.h")

	add_executable(
		OxC3_audio_interface_test
		${testsInterfaceC}
		${testsInterfaceH}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_audio_interface_test PUBLIC OxC3_audio OxC3_types_container_test_util)
	set_target_properties(OxC3_audio_interface_test PROPERTIES FOLDER Oxsomi/test/audio)
	add_test(NAME OxC3_audio_interface_test COMMAND OxC3_audio_interface_test)

	file(GLOB testsFunctionalC CONFIGURE_DEPENDS "test/functional/*.c")
	file(GLOB testsFunctionalH CONFIGURE_DEPENDS "test/functional/*.h")

	add_executable(
		OxC3_audio_functional_test
		${testsFunctionalC}
		${testsFunctionalH}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_audio_functional_test PUBLIC OxC3_audio OxC3_types_container_test_util)
	set_target_properties(OxC3_audio_functional_test PROPERTIES FOLDER Oxsomi/test/audio)

endif()
