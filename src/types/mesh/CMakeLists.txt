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

# The shared half alone: the window, the sink and what a triangle becomes. Every reader is its own library
# beside this one (../obj, ../ply) so a consumer takes only the containers it reads, the same way it takes
# bmp without dds. The internal header stays here rather than in include/, since a reader needs it and a
# consumer must not have it.

file(GLOB formatsMeshHeaders CONFIGURE_DEPENDS "../../../include/types/mesh/*.h" "../../../include/types/mesh.hpp")
file(GLOB formatsMeshSources CONFIGURE_DEPENDS "*.c" "*.h")

add_library(
	OxC3_types_mesh
	STATIC
	${formatsMeshHeaders}
	${formatsMeshSources}
	CMakeLists.txt
)

set_target_properties(OxC3_types_mesh PROPERTIES FOLDER Oxsomi/types)
target_link_libraries(OxC3_types_mesh PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container)

if(EnableTests)

	# The harness every reader suite reads through. A reader's suite lives beside that reader, so the sink
	# plumbing and the accessors that decode a written record are a library rather than a copy per reader.

	file(GLOB typesMeshTestfH CONFIGURE_DEPENDS "../../../include/types/mesh/test/*.h")
	file(GLOB typesMeshTestfC CONFIGURE_DEPENDS "test/shared/*.c")

	add_library(
		OxC3_types_mesh_test_util STATIC
		${typesMeshTestfH}
		${typesMeshTestfC}
		CMakeLists.txt
	)

	target_link_libraries(OxC3_types_mesh_test_util PUBLIC OxC3_types_mesh OxC3_types_test)
	set_target_properties(OxC3_types_mesh_test_util PROPERTIES FOLDER Oxsomi/test/types)

	if(OxC3TestExecutables)

		include(../../../cmake/oxc3.cmake)

		# What mesh owns and no reader does, which is reached through the internal header beside the core.

		oxc3_add_test(
			NAME     OxC3_types_mesh_test
			FOLDER   Oxsomi/test/types
			LIBS     OxC3_types_container_test_util OxC3_types_mesh_test_util
			INCLUDES "../../../include"
		)

	endif()

endif()
