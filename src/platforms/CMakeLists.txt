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

message("-- Targeting platform: ${platform}")

file(GLOB platformsHeaders CONFIGURE_DEPENDS "../../include/platforms/*.h")

file(GLOB_RECURSE platformsPlatformSources CONFIGURE_DEPENDS "${platform}/*.c")
file(GLOB_RECURSE platformsPlatformIncludes CONFIGURE_DEPENDS "../../include/platforms/${platform}/*.h")
file(GLOB_RECURSE platformsSources CONFIGURE_DEPENDS "generic/*.c")
file(GLOB_RECURSE platformsSIMDSources CONFIGURE_DEPENDS "simd/${simd}/*.c")

if(UNIX)
	file(GLOB_RECURSE platformsUnixSources CONFIGURE_DEPENDS "unix/*.c")
	list(APPEND platformsSources ${platformsUnixSources})
endif()

add_library(
	OxC3_platforms
	STATIC
	${platformsHeaders}
	${platformsPlatformSources}
	${platformsPlatformIncludes}
	${platformsSources}
	${platformsSIMDSources}
	CMakeLists.txt
)

source_group("Source Files (${platform})" FILES ${platformsPlatformSources})
source_group("Header Files (${platform})" FILES ${platformsPlatformIncludes})
source_group("Source Files (simd: ${simd})" FILES ${platformsSIMDSources})

set_target_properties(OxC3_platforms PROPERTIES FOLDER Oxsomi)

target_link_libraries(
	OxC3_platforms PUBLIC
	OxC3_types_base OxC3_types_math OxC3_types_container
	OxC3_formats_oiXX OxC3_formats_oiCA OxC3_formats_oiDL OxC3_formats_dds
)

if(WIN32)
	target_link_libraries(OxC3_platforms PUBLIC dwmapi dwrite Msimg32)
elseif(APPLE)
	target_link_libraries(OxC3_platforms PUBLIC "-framework CoreFoundation" "-framework ApplicationServices" "-framework AppKit")
elseif(ANDROID)
	target_include_directories(OxC3_platforms PUBLIC "${ANDROID_NDK}/sources/android/native_app_glue")
elseif(EMSCRIPTEN)
	# No system deps to find: emscripten provides the libc, and the canvas windowing in web/webwindow.c
	# needs only html5.h from the sysroot.
	# Files come from whatever filesystem is mounted, which is NODERAWFS under node.
	# A browser build would have an empty MEMFS until something populates it, and nothing does yet
	# (see the packaging note in cmake/oxc3.cmake).
	# The loop is owned by node or by the browser either way, so there is no platform loop to link against.
elseif(UNIX AND NOT APPLE)
	find_package(xdg_shell REQUIRED)
	find_package(xdg_decoration REQUIRED)
	target_link_libraries(OxC3_platforms PUBLIC xdg_shell::xdg_shell xdg_decoration::xdg_decoration xkbcommon wayland-cursor)
endif()

if(EnableTests)

	# The dummy library Test_dynamicLibrary loads.
	# Built on every platform, including the ones that bundle their suites instead of building executables:
	# the bundle needs it too, and on android it just rides along in the apk's lib/<abi>/
	# where the loader can find it by bare name.

	file(GLOB testsDylibC CONFIGURE_DEPENDS "test/dylib/*.c")
	add_library(OxC3_platforms_interface_dylib_test SHARED
		${testsDylibC}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_platforms_interface_dylib_test PUBLIC OxC3_platforms)
	set_target_properties(OxC3_platforms_interface_dylib_test PROPERTIES FOLDER Oxsomi/test/platforms)

endif()

if(EnableTests AND OxC3TestExecutables)

	# Functional test is standalone, but can't be run by CI, needs someone to manually re-run and approve.

	file(GLOB testsFunctionalH CONFIGURE_DEPENDS "test/functional/*.h")
	file(GLOB testsFunctionalC CONFIGURE_DEPENDS "test/functional/*.c")

	add_executable(
		OxC3_platforms_functional_test
		${testsFunctionalH}
		${testsFunctionalC}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_platforms_functional_test PUBLIC OxC3_platforms OxC3_types_test)
	set_target_properties(OxC3_platforms_functional_test PROPERTIES FOLDER Oxsomi/test/platforms)

	include(../../cmake/oxc3.cmake)
	file(GLOB testsInterfaceC CONFIGURE_DEPENDS "test/interface/*.c")
	file(GLOB testsInterfaceCpp CONFIGURE_DEPENDS "test/interface/*.cpp")    #C++ TUs (e.g. hpp wrapper tests)

	add_executable(
		OxC3_plinttst
		${testsInterfaceC}
		${testsInterfaceCpp}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_plinttst PUBLIC OxC3_platforms OxC3_types_test OxC3_platforms_interface_dylib_test)
	set_target_properties(OxC3_plinttst PROPERTIES FOLDER Oxsomi/test/platforms)

	add_virtual_files(
		TARGET  OxC3_plinttst
		NAME    testdata
		ROOT    ${CMAKE_CURRENT_SOURCE_DIR}/test/interface/res       # contains hello.txt + sub/world.txt
		SELF    ${CMAKE_CURRENT_SOURCE_DIR}/../..
		FORCE_PACKAGER
	)
	
	apply_dependencies(OxC3_plinttst)

	# Built by hand above rather than through oxc3_add_test, because the package and the dylib it dlopens have to be
	# attached to the target before it is usable.
	# The run half is the shared one.

	include(../../cmake/oxc3.cmake)

	file(GLOB_RECURSE plinttstData CONFIGURE_DEPENDS "test/interface/res/*")

	oxc3_add_test_run(
		NAME   OxC3_plinttst
		FOLDER Oxsomi/test/platforms
		DATA   ${plinttstData}
	)

endif()
