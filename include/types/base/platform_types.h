/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

//types/base/platform_types.h

#pragma once
#include "types/base/types.h"

typedef U32 EPlatform;

//Defines instead of enums to allow #if

#define PLATFORM_UNINITIALIZED 0
#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX 2
#define PLATFORM_ANDROID 3
#define PLATFORM_WEB 4
#define PLATFORM_IOS 5
#define PLATFORM_OSX 6

#if _PLATFORM_TYPE != PLATFORM_WINDOWS
	#include <assert.h>
#endif

//Platform and arch stuff

#define SIMD_NONE 0
#define SIMD_SSE 1
#define SIMD_NEON 2

#define ARCH_NONE 0
#define ARCH_X86_64 1
#define ARCH_ARM64 2
#define ARCH_WASM64 3

//wasm64 (-m64 / MEMORY64) satisfies this; wasm32 is rejected on purpose, same as other 32-bit targets
static_assert(sizeof(void*) == 8, "OxC3 is only supported on 64-bit");

//Moved from CMake to header to allow better compilation as a lib
//https://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor

#ifdef _WIN32
	#define _PLATFORM_TYPE PLATFORM_WINDOWS
#elif __wasm__
	#define _PLATFORM_TYPE PLATFORM_WEB
#elif __APPLE__
	#include <TargetConditionals.h>
	#if                                                                    \
	(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) ||                    \
	(defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR) ||    \
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
#elif defined(__wasm64__) || defined(__wasm__)
	#define _ARCH ARCH_WASM64
#else
	#error "Undetected architecture type"
#endif

#if !_ENABLE_SIMD
	#define _SIMD SIMD_NONE
#elif _ARCH == ARCH_ARM64
	#define _SIMD SIMD_NEON
#elif _ARCH == ARCH_X86_64
	#define _SIMD SIMD_SSE
#else
	#define _SIMD SIMD_NONE //wasm: scalar fallback (no full SSE/NEON feature set)
#endif

//Whether the transcendental _mm_*_ps intrinsics (pow, log, exp, sin, ...) and _mm_div_epi32 resolve.
//They're Intel SVML, which MSVC and ICC ship but clang and gcc do not, on any platform.
//clang-cl targets Windows and still lacks them, so this has to key off the compiler rather than the OS.
//_SIMD_HAS_SVML == 0 means the naive per-component fallbacks in vec4f.h / vec4i.h are used instead.

#if _SIMD == SIMD_SSE && defined(_MSC_VER) && !defined(__clang__)
	#define _SIMD_HAS_SVML 1
#else
	#define _SIMD_HAS_SVML 0
#endif

#if _PLATFORM_TYPE == PLATFORM_IOS || _PLATFORM_TYPE == PLATFORM_OSX
	#define _CRYPTO_ALWAYS
#endif

//Runtime CPU capability flags, detected once and stored in Platform_instance->cpuFeatures.
//These say whether a given operation has a hardware / wide-SIMD "full speed" path on this machine,
// so the runtime (and CLI) can reason about how fast something should be without re-running cpuid.

typedef enum ECPUFeatures {

	ECPUFeatures_None       = 0,

	ECPUFeatures_HwCRC32C   = 1 << 0,        //Hardware CRC32C (SSE4.2 / ARMv8 CRC)
	ECPUFeatures_HwAES      = 1 << 1,        //Hardware AES (AES-NI / ARM AESE), AES128 fast path
	ECPUFeatures_HwAES256   = 1 << 2,        //Wide AES path (VAES + AVX512), full-speed AES256
	ECPUFeatures_HwSHA256   = 1 << 3,        //Hardware SHA-256 (SHA-NI / ARMv8 SHA2)

	ECPUFeatures_F16C       = 1 << 4,        //Fast F16 <-> F32 conversion
	ECPUFeatures_Vec8i      = 1 << 5,        //256-bit integer SIMD (AVX2)
	ECPUFeatures_Vec16i     = 1 << 6,        //512-bit integer SIMD (AVX512 F+BW+DQ+VL)

	ECPUFeatures_PCLMULQDQ  = 1 << 7,        //Carry-less multiply (GHASH / GCM)
	ECPUFeatures_FMA        = 1 << 8,        //Fused multiply-add
	ECPUFeatures_AVX        = 1 << 9,        //256-bit float SIMD (AVX, OS-enabled)

	ECPUFeatures_Int8Dot    = 1 << 10        //int8 dot-product accel (AVX-VNNI / AVX512-VNNI / ARM DotProd) - ML/AIU

} ECPUFeatures;

#ifdef __cplusplus
	extern "C" {
#endif

#if _ARCH == ARCH_X86_64

	void Platform_getCPUId(int leaf, U32 result[4]);

	//Extended control register 0, which says which SIMD state the OS actually enabled.
	//Only valid once leaf 1 ECX bit 27 (OSXSAVE) is known to be set, since xgetbv raises #UD otherwise.
	//Bits 2:1 are XMM and YMM, so an AVX capable CPU whose OS never enabled them still can't run AVX code.

	U64 Platform_getXCR0();

#endif

//Detects the ECPUFeatures above (cpuid + xgetbv on x86; ECPUFeatures_None on unsupported arch).
//Called once at Platform_create, prefer reading Platform_instance->cpuFeatures at runtime.
ECPUFeatures Platform_detectCPUFeatures();

#ifdef __cplusplus
	}
#endif
