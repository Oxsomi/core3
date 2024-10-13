# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

message("-- Enabling Vulkan support...")

file(GLOB_RECURSE graphicsApiHeaders "../../../inc/graphics/vulkan/*.h")
file(GLOB_RECURSE graphicsApiSources "generic/*.c")
file(GLOB_RECURSE graphicsApiPlatformSources "${platform}/*.c")

if(NOT DynamicLinkingGraphics)

    message("-- Targeting graphics api: ${graphicsApi} on platform ${platform}")

    target_compile_definitions(OxC3_graphics PUBLIC -D_GRAPHICS_API=GRAPHICS_API_VULKAN)

    add_library(
        OxC3_graphics_vk
        STATIC
        ${graphicsApiHeaders}
        ${graphicsApiSources}
        ${graphicsApiPlatformSources}
        CMakeLists.txt
    )

else()
    add_library(
        OxC3_graphics_vk
        SHARED
        ${graphicsApiHeaders}
        ${graphicsApiSources}
        ${graphicsApiPlatformSources}
        CMakeLists.txt
    )
endif()

target_link_libraries(OxC3_graphics_vk PUBLIC OxC3_types_base OxC3_types_math OxC3_types_container OxC3_platforms OxC3_formats_oiSH OxC3_graphics)

if(DynamicLinkingGraphics)
    target_compile_definitions(OxC3_graphics_vk PUBLIC -DGRAPHICS_API_DYNAMIC)
endif()

if("${platform}" STREQUAL "windows")
    target_compile_definitions(OxC3_graphics_vk PRIVATE -D_VK_SURFACE_EXT="VK_KHR_win32_surface")
elseif("${platform}" STREQUAL "android")
    target_compile_definitions(OxC3_graphics_vk PRIVATE -D_VK_SURFACE_EXT="VK_KHR_android_surface")
elseif("${platform}" STREQUAL "osx")
    target_compile_definitions(OxC3_graphics_vk PRIVATE -D_VK_SURFACE_EXT="VK_MVK_macos_surface")
elseif("${platform}" STREQUAL "ios")
    target_compile_definitions(OxC3_graphics_vk PRIVATE -D_VK_SURFACE_EXT="VK_MVK_ios_surface")
elseif("${platform}" STREQUAL "linux")
    target_compile_definitions(OxC3_graphics_vk PRIVATE -D_VK_SURFACE_EXT="VK_KHR_wayland_surface")
else()
    message(FATAL_ERROR "Platform not supported for Vulkan yet: ${platform}")
endif()

set_target_properties(OxC3_graphics_vk PROPERTIES FOLDER Oxsomi/graphics)

source_group("Source Files (${platform})" FILES ${graphicsApiPlatformSources})

if(NOT "${platform}" STREQUAL "osx")
    include_directories(OxC3_graphics_vk PUBLIC $ENV{VULKAN_SDK}/include)
    target_link_directories(OxC3_graphics_vk PUBLIC $ENV{VULKAN_SDK}/lib)
endif()

if(WIN32)
    target_link_libraries(OxC3_graphics_vk PUBLIC vulkan-1)
elseif("${platform}" STREQUAL "osx")

    if(EXISTS "$ENV{VULKAN_SDK}/macOS/include")
        include_directories(OxC3_graphics_vk PUBLIC $ENV{VULKAN_SDK}/macOS/include)
        target_link_directories(OxC3_graphics_vk PUBLIC $ENV{VULKAN_SDK}/macOS/lib)
    else()
        include_directories(OxC3_graphics_vk PUBLIC $ENV{VULKAN_SDK}/include)
        target_link_directories(OxC3_graphics_vk PUBLIC $ENV{VULKAN_SDK}/lib)
    endif()

    target_link_libraries(OxC3_graphics_vk PUBLIC vulkan)

else()
    target_link_libraries(OxC3_graphics_vk PUBLIC vulkan)
endif()

target_link_libraries(OxC3_graphics_target INTERFACE OxC3_graphics_vk)
