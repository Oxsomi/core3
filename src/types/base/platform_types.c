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

//types/base/platform_types.c

#include "types/base/platform_types.h"

#if _ARCH == ARCH_X86_64

	#ifdef _MSC_VER
		#include <intrin.h>        //__cpuid; MSVC resolves it without this, clang-cl needs the declaration
	#else
		#include <cpuid.h>
	#endif

	void Platform_getCPUId(int leaf, U32 result[4]) {

		if(!result)
			return;

		#ifdef _MSC_VER
			__cpuid((int*) result, leaf);        //Takes int[4]; the leaves we read are bit fields either way
		#else
			if(leaf == 7)
				__get_cpuid_count(7, 0, &result[0], &result[1], &result[2], &result[3]);
			else
				__get_cpuid(leaf, &result[0], &result[1], &result[2], &result[3]);
		#endif
	}

	//Reads the extended control register (XCR0) to see which SIMD state the OS actually enabled.
	//Only valid after checking OSXSAVE (leaf1 ECX bit 27), else xgetbv raises #UD.

	#ifdef _MSC_VER
		#include <intrin.h>
		static U64 Platform_getXCR0() { return _xgetbv(0); }
	#else
		static U64 Platform_getXCR0() {
			U32 eax, edx;
			__asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
			return ((U64) edx << 32) | eax;
		}
	#endif

	ECPUFeatures Platform_detectCPUFeatures() {

		U32 leaf1[4] = { 0 }, leaf7[4] = { 0 };
		Platform_getCPUId(1, leaf1);
		Platform_getCPUId(7, leaf7);

		//Leaf 1 ECX

		const Bool aes      = (leaf1[2] & (1 << 25)) != 0;
		const Bool pclmul   = (leaf1[2] & (1 << 1))  != 0;
		const Bool sse42    = (leaf1[2] & (1 << 20)) != 0;
		const Bool fma      = (leaf1[2] & (1 << 12)) != 0;
		const Bool osxsave  = (leaf1[2] & (1 << 27)) != 0;
		const Bool avx      = (leaf1[2] & (1 << 28)) != 0;
		const Bool f16c     = (leaf1[2] & (1 << 29)) != 0;

		//Leaf 7 EBX / ECX

		const Bool avx2     = (leaf7[1] & (1 << 5))   != 0;
		const Bool avx512f  = (leaf7[1] & (1 << 16))  != 0;
		const Bool avx512dq = (leaf7[1] & (1 << 17))  != 0;
		const Bool sha      = (leaf7[1] & (1 << 29))  != 0;
		const Bool avx512bw = (leaf7[1] & (1 << 30))  != 0;
		const Bool avx512vl = (leaf7[1] & (1u << 31)) != 0;
		const Bool vaes     = (leaf7[2] & (1 << 9))   != 0;
		const Bool vpclmul  = (leaf7[2] & (1 << 10))  != 0;
		const Bool avx512vnni = (leaf7[2] & (1 << 11)) != 0;        //AVX512-VNNI (leaf 7, sub-leaf 0, ECX)

		//Leaf 7 sub-leaf 1 for AVX-VNNI (the AVX2-width int8 dot-product, no AVX512 needed)

		U32 leaf7_1[4] = { 0 };
		#ifdef _MSC_VER
			__cpuidex((int*) leaf7_1, 7, 1);
		#else
			__get_cpuid_count(7, 1, &leaf7_1[0], &leaf7_1[1], &leaf7_1[2], &leaf7_1[3]);
		#endif
		const Bool avxvnni = (leaf7_1[0] & (1 << 4)) != 0;         //AVX-VNNI (leaf 7, sub-leaf 1, EAX)

		//OS enablement: bit 1|2 = XMM|YMM (AVX), bits 5-7 = opmask|ZMM_Hi256|Hi16_ZMM (AVX512)

		const U64 xcr0  = osxsave ? Platform_getXCR0() : 0;
		const Bool osAVX = (xcr0 & 0x6)  == 0x6;
		const Bool osZMM = (xcr0 & 0xE0) == 0xE0;

		//Compose the "full speed" flags the runtime dispatches on

		ECPUFeatures f = ECPUFeatures_None;

		if (sse42)                       f |= ECPUFeatures_HwCRC32C;
		if (aes && pclmul)               f |= ECPUFeatures_HwAES;
		if (sha)                         f |= ECPUFeatures_HwSHA256;
		if (pclmul)                      f |= ECPUFeatures_PCLMULQDQ;
		if (fma && osAVX)                f |= ECPUFeatures_FMA;
		if (avx && osAVX)                f |= ECPUFeatures_AVX;
		if (f16c && osAVX)               f |= ECPUFeatures_F16C;
		if (avx2 && osAVX)               f |= ECPUFeatures_Vec8i;

		if (avx512f && avx512bw && avx512dq && avx512vl && osZMM)
			f |= ECPUFeatures_Vec16i;

		//int8 dot-product: AVX-VNNI (256-bit, OS-AVX) or AVX512-VNNI (512-bit, OS-ZMM)

		if ((avxvnni && osAVX) || (avx512vnni && osZMM))
			f |= ECPUFeatures_Int8Dot;

		//Full-speed AES256 mirrors buffer_encrypt's wide VAES path (256/512-bit)

		if ((f & ECPUFeatures_HwAES) && vaes && vpclmul && avx2 && avx512vl && avx512bw && osZMM)
			f |= ECPUFeatures_HwAES256;

		return f;
	}

#elif _ARCH == ARCH_ARM64

	#if _PLATFORM_TYPE == PLATFORM_WINDOWS
		#define UNICODE
		#define WIN32_LEAN_AND_MEAN
		#define NOMINMAX
		#include <Windows.h>
	#elif _PLATFORM_TYPE == PLATFORM_IOS || _PLATFORM_TYPE == PLATFORM_OSX
		#include <sys/sysctl.h>
	#elif _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID
		#include <asm/hwcap.h>
		#include <sys/auxv.h>
	#endif

	void Platform_getCPUId(int leaf, U32 result[4]) { (void) leaf; (void) result; }

	ECPUFeatures Platform_detectCPUFeatures() {

		//NEON baseline (ARMv8-A): FMLA (FMA) and FP16<->FP32 conversion (FCVT) are always present.
		ECPUFeatures f = ECPUFeatures_FMA | ECPUFeatures_F16C;

		Bool hasCrypto = false, hasCRC = false, hasSHA = false;

		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			hasCrypto = !!IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
			hasSHA    = hasCrypto;
			hasCRC    = !!IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
		#elif _PLATFORM_TYPE == PLATFORM_IOS || _PLATFORM_TYPE == PLATFORM_OSX
			hasCrypto = true;        //Apple ARM always ships the crypto extension (see _CRYPTO_ALWAYS)
			hasSHA    = true;
			int v = 0; size_t siz = sizeof(v);
			sysctlbyname("hw.optional.armv8_crc32", &v, &siz, NULL, 0);
			hasCRC = v != 0;
		#elif defined(HWCAP_AES)
			const unsigned long hwcap = getauxval(AT_HWCAP);
			hasCrypto = (hwcap & HWCAP_AES)   != 0;
			hasSHA    = (hwcap & HWCAP_SHA2)  != 0;
			hasCRC    = (hwcap & HWCAP_CRC32) != 0;
		#endif

		if (hasCRC)
			f |= ECPUFeatures_HwCRC32C;

		//ARMv8 crypto covers AES (128- and 256-bit keys) and PMULL, all at full speed
		if (hasCrypto)
			f |= ECPUFeatures_HwAES | ECPUFeatures_HwAES256 | ECPUFeatures_PCLMULQDQ;

		if (hasSHA)
			f |= ECPUFeatures_HwSHA256;

		//int8 dot-product (ARMv8.4 DotProd). Windows-on-ARM has no dedicated PF_ flag, so it's best-effort there.

		#if _PLATFORM_TYPE == PLATFORM_IOS || _PLATFORM_TYPE == PLATFORM_OSX
			f |= ECPUFeatures_Int8Dot;        //Apple silicon supports DotProd
		#elif defined(HWCAP_ASIMDDP)
			if (getauxval(AT_HWCAP) & HWCAP_ASIMDDP)
				f |= ECPUFeatures_Int8Dot;
		#endif

		return f;
	}

#else
	void Platform_getCPUId(int leaf, U32 result[4]) { (void) leaf; (void) result; }
	ECPUFeatures Platform_detectCPUFeatures() { return ECPUFeatures_None; }
#endif
