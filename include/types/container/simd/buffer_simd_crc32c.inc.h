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

//Implementation of hardware CRC32C but ported back to C and restructured a bit
//https://github.com/rurban/smhasher/blob/master/crc32c.cpp
//All _mm_crc32_x have been replaced to use SIMD_CRC32C(x) to keep it portable across different architectures
//e.g. #define SIMD_CRC32C_U64 _mm_crc32_u64 or __crc32cd

#ifndef SIMD_CRC32C_U64
	#error Define SIMD_CRC32C_U64 for this architecture; e.g. _mm_crc32_u64 or __crc32cd
#endif

#ifndef SIMD_CRC32C_U32
	#error Define SIMD_CRC32C_U64 for this architecture; e.g. _mm_crc32_u32 or __crc32cw
#endif

#ifndef SIMD_CRC32C_U16
	#error Define SIMD_CRC32C_U16 for this architecture; e.g. _mm_crc32_u16 or __crc32ch
#endif

#ifndef SIMD_CRC32C_U8
	#error Define SIMD_CRC32C_U8 for this architecture; e.g. _mm_crc32_u8 or __crc32cb
#endif

extern const U32 CRC32C_LONG_SHIFTS[4][256];
extern const U32 CRC32C_SHORT_SHIFTS[4][256];

static inline U32 CRC32C_shiftShort(U32 crc0) {
	return
		CRC32C_SHORT_SHIFTS[0][(U8) crc0] ^ CRC32C_SHORT_SHIFTS[1][(U8)(crc0 >> 8)] ^
		CRC32C_SHORT_SHIFTS[2][(U8)(crc0 >> 16)] ^ CRC32C_SHORT_SHIFTS[3][(U8)(crc0 >> 24)];
}

static inline U32 CRC32C_shiftLong(U32 crc0) {
	return
		CRC32C_LONG_SHIFTS[0][(U8) crc0] ^ CRC32C_LONG_SHIFTS[1][(U8)(crc0 >> 8)] ^
		CRC32C_LONG_SHIFTS[2][(U8)(crc0 >> 16)] ^ CRC32C_LONG_SHIFTS[3][(U8)(crc0 >> 24)];
}

static inline U32 Buffer_crc32cSimd(const Buffer buf) {

	U32 crc = U32_MAX;

	const U64 bufLen = Buffer_length(buf);

	if(!bufLen)
		return crc ^ U32_MAX;

	U64 off = (U64)(void*)buf.ptr;

	U64 len = bufLen;
	const U64 offNear8 = U64_min((U64)buf.ptr + bufLen, (off + 7) & ~7);

	//Go to nearest 8 byte boundary

	if (off & 7) {

		if (off + sizeof(U32) <= offNear8) {
			crc = SIMD_CRC32C_U32(crc, *(U32*)(void*)off);
			off += sizeof(U32);
			len -= sizeof(U32);
		}

		if (off + sizeof(U16) <= offNear8) {
			crc = SIMD_CRC32C_U16(crc, *(U16*)(void*)off);
			off += sizeof(U16);
			len -= sizeof(U16);
		}

		if (off + sizeof(U8) <= offNear8) {
			crc = SIMD_CRC32C_U8(crc, *(U8*)(void*)off);
			off += sizeof(U8);
			len -= sizeof(U8);
		}
	}

	//Can only be supported if CRC32C is 64-bit, otherwise needs a different version.
	#ifndef DISABLE_CRC32C_TRIPLET

		//Compute 3 CRCs at once, this is beneficial because the hardware is able to schedule these at once
		//Giving us better perf (up to 15x faster!)
		//We always run 64-bit, so we can assume U64 to be properly present

		U64 crc0 = crc;

		static const U64 LONG_SHIFT = 8192, LONG_SHIFT_U64 = 8192 / sizeof(U64);

		while(len >= LONG_SHIFT * 3) {

			U64 crc1 = 0, crc2 = 0;
			U64 *offu64 = (U64*)(void*)off;

			for (U64 i = 0; i < LONG_SHIFT_U64; ++i) {
				crc0 = SIMD_CRC32C_U64(crc0, offu64[i]);
				crc1 = SIMD_CRC32C_U64(crc1, offu64[LONG_SHIFT_U64 + i]);
				crc2 = SIMD_CRC32C_U64(crc2, offu64[LONG_SHIFT_U64 * 2 + i]);
			}

			crc0 = CRC32C_shiftLong((U32) crc0) ^ crc1;
			crc0 = CRC32C_shiftLong((U32) crc0) ^ crc2;

			off += LONG_SHIFT * 3;
			len -= LONG_SHIFT * 3;
		}

		//Do the same thing but for smaller blocks

		static const U64 SHORT_SHIFT = 256, SHORT_SHIFT_U64 = 256 / sizeof(U64);

		while(len >= SHORT_SHIFT * 3) {

			U64 crc1 = 0, crc2 = 0;
			U64 *offu64 = (U64*)(void*)off;

			for (U64 i = 0; i < SHORT_SHIFT_U64; ++i) {
				crc0 = SIMD_CRC32C_U64(crc0, offu64[i]);
				crc1 = SIMD_CRC32C_U64(crc1, offu64[SHORT_SHIFT_U64 + i]);
				crc2 = SIMD_CRC32C_U64(crc2, offu64[SHORT_SHIFT_U64 * 2 + i]);
			}

			crc0 = CRC32C_shiftShort((U32) crc0) ^ crc1;
			crc0 = CRC32C_shiftShort((U32) crc0) ^ crc2;

			off += SHORT_SHIFT * 3;
			len -= SHORT_SHIFT * 3;
		}

		crc = (U32) crc0;
	#endif

	//Process remaining U64s

	while (len >= sizeof(U64)) {
		crc = (U32) SIMD_CRC32C_U64(crc, *(U64*)(void*)off);
		off += sizeof(U64);
		len -= sizeof(U64);
	}

	if (len >= sizeof(U32)) {
		crc = SIMD_CRC32C_U32(crc, *(U32*)(void*)off);
		off += sizeof(U32);
		len -= sizeof(U32);
	}

	if (len >= sizeof(U16)) {
		crc = SIMD_CRC32C_U16(crc, *(U16*)(void*)off);
		off += sizeof(U16);
		len -= sizeof(U16);
	}

	if (len >= sizeof(U8))
		crc = SIMD_CRC32C_U8(crc, *(U8*)(void*)off);

	return (U32)crc ^ U32_MAX;
}
