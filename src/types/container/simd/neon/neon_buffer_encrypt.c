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

#include "types/container/buffer.h"
#include "types/math/vec4i_swizzle.h"

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

static inline I32x4 AES_keyGenAssistInternal(I32x4 a, U8 rcon) {

	U32 x1 = (U32)I32x4_y(a);
	U32 x3 = (U32)I32x4_w(a);

	U32 sx1 = AES_subWord(x1);
	U32 sx3 = AES_subWord(x3);

	return I32x4_create4(
		(I32)sx1,
		(I32)(U32_ror(sx1, 8) ^ rcon),
		(I32)sx3,
		(I32)(U32_ror(sx3, 8) ^ rcon)
	);
}

I32x4 AES_keyGenAssist(I32x4 a, U8 i) {

	if(i >= 11)
		return I32x4_zero();

	switch (i) {
		case 0:		return AES_keyGenAssistInternal(a, 0x00);
		case 1:		return AES_keyGenAssistInternal(a, 0x01);
		case 2:		return AES_keyGenAssistInternal(a, 0x02);
		case 3:		return AES_keyGenAssistInternal(a, 0x04);
		case 4:		return AES_keyGenAssistInternal(a, 0x08);
		case 5:		return AES_keyGenAssistInternal(a, 0x10);
		case 6:		return AES_keyGenAssistInternal(a, 0x20);
		case 7:		return AES_keyGenAssistInternal(a, 0x40);
		case 8:		return AES_keyGenAssistInternal(a, 0x80);
		case 9:		return AES_keyGenAssistInternal(a, 0x1B);
		default:	return AES_keyGenAssistInternal(a, 0x36);
	}
}

//Neon aes is a bit special.
//It first does the xor before doing the shiftRows/subBytes.
//This produces a different result, so we pass 0 as the key and do the xor afterwards.
static inline uint8x16_t AES_block(I32x4 state) {
	return vaeseq_u8(vreinterpretq_u8_s32(state), vdupq_n_u8(0));
}

I32x4 AES_encodeBlock(I32x4 state, I32x4 rk) {
	uint8x16_t block = AES_block(state);
	block = vaesmcq_u8(block);		//mixColumns
	return I32x4_xor(block, rk);
}

I32x4 AES_encodeBlockLast(I32x4 state, I32x4 rk) {
	uint8x16_t block = AES_block(state);
	return I32x4_xor(block, rk);
}

void AES_checkSupport(I8 *hasAES256) {
	if(*hasAES256 < 0) {
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			*hasAES256 = !!IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
		#elif _PLATFORM_TYPE == PLATFORM_IOS ||  _PLATFORM_TYPE == PLATFORM_OSX
			*hasAES256 = 1;		//Apple says they support this always
		#elif defined(HWCAP_AES)
			*hasSHA256 = getauxval(AT_HWCAP) & HWCAP_AES;
		#else
			*hasSHA256 = 0;
		#endif
	}
}
