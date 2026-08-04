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

file(GLOB_RECURSE typesBaseHeaders CONFIGURE_DEPENDS "../../../include/types/base/*.h")
file(GLOB typesBaseSources CONFIGURE_DEPENDS "*.c")

if(WIN32)
	file(GLOB_RECURSE typesBasePlatformSources CONFIGURE_DEPENDS "platforms/windows/*.c")
elseif(UNIX)
	file(GLOB_RECURSE typesBasePlatformSources CONFIGURE_DEPENDS "platforms/unix/*.c")
endif()

add_library(
	OxC3_types_base
	STATIC
	${typesBaseHeaders}
	${typesBaseSources}
	${typesBasePlatformSources}
	CMakeLists.txt
)

target_compile_definitions(OxC3_types_base PUBLIC -D_ENABLE_SIMD=${SIMD})

if(ANDROID)
	file(GLOB_RECURSE typesBaseAndroidSources CONFIGURE_DEPENDS "platforms/android/*.c")
	target_sources(OxC3_types_base PRIVATE ${typesBaseAndroidSources})
endif()

if(UNIX AND NOT APPLE)
	target_link_libraries(OxC3_types_base PUBLIC m)
endif()

if(MSVC AND ARM)
	enable_language(ASM_MARMASM)
	file(GLOB_RECURSE armAsmWin CONFIGURE_DEPENDS "platforms/windows/arm/*.asm")
	target_sources(OxC3_types_base PRIVATE ${armAsmWin})
	set_source_files_properties(
		${armAsmWin}
		PROPERTY LANGUAGE ASM_MARMASM
	)
endif()

source_group("Source Files (${platform})" FILES ${typesBasePlatformSources})

set_target_properties(OxC3_types_base PROPERTIES FOLDER Oxsomi/types)
target_include_directories(OxC3_types_base PUBLIC "../../../include")

if(EnableTests)

	file(GLOB typesTestHeaders CONFIGURE_DEPENDS "../../../include/types/test/*.h")
	file(GLOB testsSharedC CONFIGURE_DEPENDS "test/shared/*.c")

	add_library(
		OxC3_types_test
		STATIC
		${typesTestHeaders}
		${testsSharedC}
		CMakeLists.txt
	)

	set_target_properties(OxC3_types_test PROPERTIES FOLDER Oxsomi/test/types)
	target_include_directories(OxC3_types_test PUBLIC "../../../include")

	# The per-suite executables can't link on android: Platform_defineEntrypoint gives android_main
	# rather than main, and an APK can't run an exec anyway. src/CMakeLists.txt builds the suites into
	# OxC3_atest instead, which still links the test library above.

	if(OxC3TestExecutables)

		file(GLOB testsC CONFIGURE_DEPENDS "test/*.c")
		file(GLOB testsH CONFIGURE_DEPENDS "test/*.h")

		add_executable(
			OxC3_types_base_test
			${testsC}
			${testsH}
			CMakeLists.txt
		)

		target_link_libraries(OxC3_types_base_test PUBLIC OxC3_types_base OxC3_types_test)
		set_target_properties(OxC3_types_base_test PROPERTIES FOLDER Oxsomi/test/types)
		add_test(NAME OxC3_types_base_test COMMAND OxC3_types_base_test)

	endif()

endif()
