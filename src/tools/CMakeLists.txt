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

find_package(Python3)

add_custom_target(check_style ALL
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running OxC3 style check..."
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_style.py ${CMAKE_SOURCE_DIR} --no-todo-report
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "OxC3 style check"
    VERBATIM
)

set_target_properties(check_style PROPERTIES FOLDER Oxsomi/tools)

if(EnableOxC3CLI)
    add_subdirectory(oxc3_cli)
endif()
