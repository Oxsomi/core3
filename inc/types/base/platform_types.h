/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#pragma once
#include <stdint.h>
#include <assert.h>

typedef uint32_t U32;

//Defines instead of enums to allow #if

#define PLATFORM_UNINITIALIZED 0
#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX 2
#define PLATFORM_ANDROID 3
#define PLATFORM_WEB 4
#define PLATFORM_IOS 5
#define PLATFORM_OSX 6

//Platform and arch stuff

#define SIMD_NONE 0
#define SIMD_SSE 1
#define SIMD_NEON 2

#define ARCH_NONE 0
#define ARCH_X86_64 1
#define ARCH_ARM64 2

static_assert(sizeof(void*) == 8, "OxC3 is only supported on 64-bit");

//Moved from CMake to header to allow better compilation as a lib
//https://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor

#ifdef _WIN32
	#define _PLATFORM_TYPE PLATFORM_WINDOWS
#elif __wasm__
	#define _PLATFORM_TYPE PLATFORM_WEB
#elif __APPLE__
	#include <TargetConditionals.h>
	#if																	\
	(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) ||					\
	(defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR) ||	\
	(defined(TARGET_OS_EMBEDDED) && TARGET_OS_EMBEDDED)
		#define _PLATFORM_TYPE PLATFORM_IOS
	#elif defined(TARGET_OS_MAC) && TARGET_OS_MAC
		#define _PLATFORM_TYPE PLATFORM_OSX
	#else
		#error "Undetected apple platform type"
	#endif
#elif __ANDROID__
	#define _PLATFORM_TYPE PLATFORM_ANDROID
#elif __linux__
	#define _PLATFORM_TYPE PLATFORM_LINUX
#else
	#error "Undetected platform type"
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
	#define _ARCH ARCH_ARM64
#elif defined(_WIN64) || defined(__x86_64__)
	#define _ARCH ARCH_X86_64
#else
	#error "Undetected architecture type"
#endif

#if (defined(_ENABLE_SIMD) && !_ENABLE_SIMD) || (_PLATFORM_TYPE != PLATFORM_WINDOWS)		//TODO: Fix SIMD on non windows
	#define _SIMD SIMD_NONE
#elif _ARCH == ARCH_ARM64
	#define _SIMD SIMD_NONE																	//TODO: SIMD_NEON
	#warning "-- ARM NEON isn't natively supported. Falling back to non SIMD for testing purposes, PLEASE don't ship like this"
#else
	#define _SIMD SIMD_SSE
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef U32 EPlatform;

void Platform_getCPUId(int leaf, U32 result[4]);

#ifdef __cplusplus
	}
#endif
