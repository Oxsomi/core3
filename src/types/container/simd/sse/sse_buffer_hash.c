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

#include "types/base/platform_types.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/math/vec4i_swizzle.h"
#include "types/base/constants.h"

#include "types/base/mathi.h"

#include <nmmintrin.h>

#define SIMD_CRC32C_U64 _mm_crc32_u64
#define SIMD_CRC32C_U32 _mm_crc32_u32
#define SIMD_CRC32C_U16 _mm_crc32_u16
#define SIMD_CRC32C_U8 _mm_crc32_u8

#include "types/container/simd/buffer_simd_crc32c.inc.h"

U32 Buffer_crc32c(const Buffer buf) {
	return Buffer_crc32cSimd(buf);
}

static inline I32x4 I32x4_sha256msg2(I32x4 i0, I32x4 i2, I32x4 i3) {
	I32x4 tmp = I32x4_combineRightShift(i3, i2, 1);		//_mm_alignr_epi8(a, b, 4)
	I32x4 msgTmp = I32x4_add(i0, tmp);
	I32x4 a = msgTmp;
	I32x4 b = i3;
	return _mm_sha256msg2_epu32(a, b);
}

static inline void I32x4_sha256rnds4(I32x4 msgOriginal, I32x4 round, I32x4 *state0, I32x4 *state1) {
	I32x4 msg = I32x4_add(msgOriginal, round);
	*state1 = _mm_sha256rnds2_epu32(*state1, *state0, msg);
	msg = I32x4_zwxx(msg);														//_mm_shuffle_epi32(msg, 0xE);
	*state0 = _mm_sha256rnds2_epu32(*state0, *state1, msg);
}

#define SIMD_SHA256_RNDS4 I32x4_sha256rnds4
#define SIMD_SHA256_MSG1 _mm_sha256msg1_epu32
#define SIMD_SHA256_MSG2 I32x4_sha256msg2
#define SIMD_SHA256_SUFFIX(x) x##Simd
#include "types/container/simd/buffer_simd_sha.inc.h"

static I8 hasSHA256 = -1;

void Buffer_sha256(const Buffer buf, U32 output[8]) {

	if(!output)
		return;

	//Fallback if the hardware doesn't support SHA extension

	if(hasSHA256 < 0) {
		U32 cpuInfo1[4];
		Platform_getCPUId(7, cpuInfo1);		//TODO: Find a cleaner way
		hasSHA256 = (cpuInfo1[1] >> 29) & 1;
	}

	if(!hasSHA256) {
		Buffer_sha256Fallback(buf, output);
		return;
	}

	Buffer_sha256Simd(buf, output);
}
