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

file(GLOB formatsWAVHeaders CONFIGURE_DEPENDS "../../../include/formats/wav/*.h")
file(GLOB formatsWAVSources CONFIGURE_DEPENDS "*.c")

add_library(
	OxC3_formats_wav
	STATIC
	${formatsWAVHeaders}
	${formatsWAVSources}
	CMakeLists.txt
)

set_target_properties(OxC3_formats_wav PROPERTIES FOLDER Oxsomi/formats)
target_link_libraries(OxC3_formats_wav PUBLIC OxC3_types_base OxC3_types_container)

if(EnableTests AND OxC3TestExecutables)

	file(GLOB testsC CONFIGURE_DEPENDS "test/*.c")
	file(GLOB testsH CONFIGURE_DEPENDS "test/*.h")

	add_executable(
		OxC3_formats_wav_test
		${testsC}
		${testsH}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_formats_wav_test PUBLIC OxC3_types_container_test_util OxC3_formats_wav)
	set_target_properties(OxC3_formats_wav_test PROPERTIES FOLDER Oxsomi/test/formats)
	target_include_directories(OxC3_formats_wav_test PUBLIC "../../../include")
	add_test(NAME OxC3_formats_wav_test COMMAND OxC3_formats_wav_test)

endif()

