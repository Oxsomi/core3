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

//types/container/simd/neon/neon_buffer_encrypt.inc.h

#include "types/container/buffer_encrypt.h"
#include "types/math/vec4i_swizzle.h"

//Neon aes is a bit special.
//It first does the xor before doing the shiftRows/subBytes.
//This produces a different result, so we pass 0 as the key and do the xor afterwards.
static inline uint8x16_t AES_block(I32x4 state) {
	return vaeseq_u8(vreinterpretq_u8_s32(state), vdupq_n_u8(0));
}

static inline I32x4 AES_subWord4(I32x4 input) {

	//AES_block applies: ShiftRows(SubBytes(input))
	uint8x16_t shifted = AES_block(input);
	
	//Undo ShiftRows using vqtbl1q_u8
	const uint8_t invShiftRows[16] = {
		15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12
	};
	
	uint8x16_t invTbl = vld1q_u8(invShiftRows);
	return vreinterpretq_s32_u8(vqtbl1q_u8(shifted, invTbl));
}

static inline I32x4 AES_keyGenAssistInternal(I32x4 a, U8 rcon) {

	const U64 rcon64 = (U64)rcon << 32;

	I32x4 sx1_3 = I32x4_yyww(AES_subWord4(a));

	//Apply U32_ror but only on .y and .w, leave .x and .z unchanged

	const uint8_t ror8Yw[16] = {
		3,   2,  1,  0,
		6,   5,  4,  7,
		11, 10,  9,  8,
		14, 13, 12, 15
	};
	uint8x16_t rorTable = vld1q_u8(ror8Yw);
	sx1_3 = vreinterpretq_s32_u8(vqtbl1q_u8(vreinterpretq_u8_s32(sx1_3), rorTable));

	I32x4 res = I32x4_xor(sx1_3, I32x4_createFromU64x2(rcon64, rcon64));
	return res;
}

static inline I32x4 AES_keyGenAssist(I32x4 a, U8 i) {

	if(i >= 11)
		return I32x4_zero();

	switch (i) {
		case 0:        return AES_keyGenAssistInternal(a, 0x00);
		case 1:        return AES_keyGenAssistInternal(a, 0x01);
		case 2:        return AES_keyGenAssistInternal(a, 0x02);
		case 3:        return AES_keyGenAssistInternal(a, 0x04);
		case 4:        return AES_keyGenAssistInternal(a, 0x08);
		case 5:        return AES_keyGenAssistInternal(a, 0x10);
		case 6:        return AES_keyGenAssistInternal(a, 0x20);
		case 7:        return AES_keyGenAssistInternal(a, 0x40);
		case 8:        return AES_keyGenAssistInternal(a, 0x80);
		case 9:        return AES_keyGenAssistInternal(a, 0x1B);
		default:    return AES_keyGenAssistInternal(a, 0x36);
	}
}

static inline I32x4 AES_encodeBlock(I32x4 state, I32x4 rk) {
	uint8x16_t block = AES_block(state);
	block = vaesmcq_u8(block);        //mixColumns
	return I32x4_xor(vreinterpretq_s32_u8(block), rk);
}

static inline I32x4 AES_encodeBlockLast(I32x4 state, I32x4 rk) {
	uint8x16_t block = AES_block(state);
	return I32x4_xor(vreinterpretq_s32_u8(block), rk);
}
