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

#include "types/base/platform_types.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/math/vec4i_swizzle.h"
#include "types/math/vec_cvt.h"
#include "types/base/constants.h"

#include "types/base/mathi.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
	#define NOMINMAX
	#include <Windows.h>
#elif _PLATFORM_TYPE == PLATFORM_IOS ||  _PLATFORM_TYPE == PLATFORM_OSX
	#include <sys/sysctl.h>
#elif _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID
	#include <asm/hwcap.h>
	#include <sys/auxv.h>
#else
	#error Unsupported platform!
#endif

#include <arm_neon.h>

#define SIMD_CRC32C_U64 __crc32cd
#define SIMD_CRC32C_U32 __crc32cw
#define SIMD_CRC32C_U16 __crc32ch
#define SIMD_CRC32C_U8 __crc32cb
#define DISABLE_CRC32C_TRIPLET //TODO:

#include "types/container/simd/buffer_simd_crc32c.inc.h"

static I8 hasCRC32 = -1;

U32 Buffer_crc32c(const Buffer buf) {

	//Check if CRC32C is present
	
	if(hasCRC32 < 0) {
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			hasCRC32 = !!IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
		#elif _PLATFORM_TYPE == PLATFORM_IOS ||  _PLATFORM_TYPE == PLATFORM_OSX
			int v = 0;
			size_t siz = sizeof(v);
			sysctlbyname("hw.optional.armv8_crc32", &v, &siz, NULL, 0);
			hasCRC32 = v;
		#elif defined(HWCAP_CRC32)
			hasCRC32 = getauxval(AT_HWCAP) & HWCAP_CRC32;
		#else
			hasCRC32 = 0;
		#endif
	}

	if (!hasCRC32)
		return Buffer_crc32cFallback(buf);

	return Buffer_crc32cSimd(buf);
}

static inline void I32x4_sha256rnds4(I32x4 msgOriginal, I32x4 round, I32x4 *state0, I32x4 *state1) {

	I32x4 msg = I32x4_add(msgOriginal, round);

	I32x4 a = *state1;
	I32x4 b = *state0;

	uint32x4_t abcd = vreinterpretq_u32_s32(I32x4_create2_2(I32x4_wz(b), I32x4_wz(a)));
	uint32x4_t efgh = vreinterpretq_u32_s32(I32x4_create2_2(I32x4_yx(b), I32x4_yx(a)));
    
	uint32x4_t wk = vreinterpretq_u32_s32(msg);
    
	uint32x4_t abcdNew = vsha256hq_u32(abcd, efgh, wk);
	uint32x4_t efghNew = vsha256h2q_u32(efgh, abcd, wk);

	I32x4 abcdNewi = vreinterpretq_s32_u32(abcdNew);
	I32x4 efghNewi = vreinterpretq_s32_u32(efghNew);

    I32x4 resultB = I32x4_create2_2(I32x4_yx(efghNewi), I32x4_yx(abcdNewi));
    I32x4 resultA = I32x4_create2_2(I32x4_wz(efghNewi), I32x4_wz(abcdNewi));

	*state0 = resultB;
	*state1 = resultA;
}

static inline I32x4 I32x4_msg1(I32x4 a, I32x4 b) {
	return vreinterpretq_s32_u32(vsha256su0q_u32(vreinterpretq_u32_s32(a), vreinterpretq_u32_s32(b)));
}

static inline I32x4 I32x4_msg2(I32x4 a, I32x4 b, I32x4 c) {
	(void)a;
	return vreinterpretq_s32_u32(
		vsha256su1q_u32(vreinterpretq_u32_s32(a), vreinterpretq_u32_s32(b), vreinterpretq_u32_s32(c))
	);
}

#define SIMD_SHA256_RNDS4 I32x4_sha256rnds4
#define SIMD_SHA256_MSG1 I32x4_msg1
#define SIMD_SHA256_MSG2 I32x4_msg2
#define SIMD_SHA256_SUFFIX(x) x##Simd
#include "types/container/simd/buffer_simd_sha.inc.h"

static I8 hasSHA256 = -1;

void Buffer_sha256(const Buffer buf, U32 output[8]) {

	if(!output)
		return;
	
	//Check if SHA256 is present
	
	#ifndef _CRYPTO_ALWAYS
		if(hasSHA256 < 0) {
			#if _PLATFORM_TYPE == PLATFORM_WINDOWS
				hasSHA256 = !!IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
			#elif defined(HWCAP_SHA2)
				hasSHA256 = getauxval(AT_HWCAP) & HWCAP_SHA2;
			#else
				hasSHA256 = 0;
			#endif
		}

		if(!hasSHA256) {
			Buffer_sha256Fallback(buf, output);
			return;
		}
	#endif

	Buffer_sha256Simd(buf, output);
}
