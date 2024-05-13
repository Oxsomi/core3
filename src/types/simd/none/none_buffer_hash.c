/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/math.h"
#include "types/vec.h"
#include "types/platform_types.h"

void Buffer_sha256Internal(Buffer buf, U32 *output);

//Fallback CRC32 implementation

//CRC32C ported from:
//https://github.com/rurban/smhasher/blob/master/crc32c.cpp

extern const U32 CRC32C_TABLE[16][256];

U32 Buffer_crc32c(Buffer buf) {

	U64 crc = U32_MAX;

	const U64 bufLen = Buffer_length(buf);

	if(!bufLen)
		return (U32) crc ^ U32_MAX;

	U64 len = bufLen;
	U64 it = (U64)(void*)buf.ptr;
	const U64 align8 = U64_min(it + len, (it + 7) & ~7);

	while (it < align8) {
		crc = CRC32C_TABLE[0][(U8)(crc ^ *(const U8*)it)] ^ (crc >> 8);
		++it;
		--len;
	}

	while (len >= sizeof(U64) * 2) {

		crc ^= *(const U64*) it;
		const U64 next = *((const U64*) it + 1);

		U64 res = 0;

		for(U64 i = 0; i < sizeof(U64); ++i)		//Compiler will unroll for us
			res ^= CRC32C_TABLE[15 - i][(U8)(crc >> (i * 8))];

		for(U64 i = 0; i < sizeof(U64); ++i)		//Compiler will unroll for us
			res ^= CRC32C_TABLE[7 - i][(U8)(next >> (i * 8))];

		crc = res;

		it += sizeof(U64) * 2;
		len -= sizeof(U64) * 2;
	}

	while (len > 0) {
		crc = CRC32C_TABLE[0][(U8)(crc ^ *(const U8*)it)] ^ (crc >> 8);
		++it;
		--len;
	}

	return (U32)crc ^ U32_MAX;
}

void Buffer_sha256(Buffer buf, U32 output[8]) {

	if(!output)
		return;

	Buffer_sha256Internal(buf, output);
}
