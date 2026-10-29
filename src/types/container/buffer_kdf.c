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

//types/container/buffer_kdf.c

#include "types/container/buffer_encrypt.h"
#include "types/container/buffer.h"
#include "types/math/vec4i_swizzle.h"

//Buffer_sha256 returns numeric words, whereas HMAC concatenates the big endian digest bytes.
//I32x4_swapEndianness reverses all 16 bytes, so the lanes are reversed first to keep the words in order.
static void Buffer_sha256DigestBytes(const U32 hash[8], U8 output[32]) {
	I32x4_store4(output, I32x4_swapEndianness(I32x4_wzyx(I32x4_load4(hash))));
	I32x4_store4(output + 16, I32x4_swapEndianness(I32x4_wzyx(I32x4_load4(hash + 4))));
}

//HMAC key lengths here never exceed one SHA256 block.
static void Buffer_hmacSha256Small(Buffer key, Buffer input, U8 output[32]) {

	alignas(16) U8 inner[129] = { 0 };
	alignas(16) U8 outer[96] = { 0 };
	alignas(16) U32 hash[8] = { 0 };

	const U64 keyLength = Buffer_length(key);

	for (U64 i = 0; i < 64; ++i) {

		const U8 byte = i < keyLength ? key.ptr[i] : 0;

		inner[i] = byte ^ 0x36;
		outer[i] = byte ^ 0x5C;
	}

	Buffer_memcpy(Buffer_createRef(inner + 64, Buffer_length(input)), input);
	Buffer_sha256(Buffer_createRefConst(inner, 64 + Buffer_length(input)), hash);
	Buffer_sha256DigestBytes(hash, outer + 64);

	Buffer_sha256(Buffer_createRefConst(outer, sizeof(outer)), hash);
	Buffer_sha256DigestBytes(hash, output);

	Buffer_clearAllSecure(Buffer_createRef(inner, sizeof(inner)));
	Buffer_clearAllSecure(Buffer_createRef(outer, sizeof(outer)));
	Buffer_clearAllSecure(Buffer_createRef(hash, sizeof(hash)));
}

//RFC 5869 HKDF-SHA256 producing one 32-byte block, as the digest's bytes in order (unlike Buffer_sha256's numeric words).
//Internal, so the callers' fixed inputs are trusted: key, salt and info are at most 64 bytes each (both HMAC passes fit
// stack buffers) and output holds 32 bytes. An empty salt is the RFC's zero salt.
static void Buffer_hkdfSha256(Buffer key, Buffer salt, Buffer info, U32 output[8]) {

	U8 prk[32] = { 0 };
	U8 input[65] = { 0 };

	Buffer_hmacSha256Small(salt, key, prk);
	Buffer_memcpy(Buffer_createRef(input, Buffer_length(info)), info);
	input[Buffer_length(info)] = 1;
	Buffer_hmacSha256Small(
		Buffer_createRefConst(prk, sizeof(prk)), Buffer_createRefConst(input, Buffer_length(info) + 1), (U8*) output
	);

	Buffer_clearAllSecure(Buffer_createRef(prk, sizeof(prk)));
	Buffer_clearAllSecure(Buffer_createRef(input, sizeof(input)));
}

Bool Buffer_deriveEncryptionKey(
	const U32 key[8], I32x4 rootIv, EEncryptionKeyDomain domain, U32 output[8], Error *e_rr
) {

	Bool s_uccess = true;

	//Fixed width, so the length is the type's: a longer label doesn't compile, and HKDF's info stays within 64 bytes.
	//The labels are 16 characters, and the stored key schedule hashes those without the terminator.

	static const C8 DOMAINS[][17] = {
		"OxC3 stream data", "OxC3 oiDL header", "OxC3 oiCA header"
	};

	static_assert(
		sizeof(DOMAINS) / sizeof(DOMAINS[0]) == EEncryptionKeyDomain_Count,
		"Every EEncryptionKeyDomain needs a label in DOMAINS"
	);

	static_assert(sizeof(DOMAINS[0]) - 1 <= 64, "Buffer_hkdfSha256 takes at most 64 bytes of info");

	if (!key || !output)
		retError(clean, Error_nullPointer(0, "Buffer_deriveEncryptionKey()::key and output are required"));

	if ((U32) domain >= EEncryptionKeyDomain_Count)
		retError(clean, Error_invalidEnum(2, domain, EEncryptionKeyDomain_Count, "Buffer_deriveEncryptionKey()::domain"));

	Buffer_hkdfSha256(
		Buffer_createRefConst(key, 32), Buffer_createRefConst(&rootIv, sizeof(U32) * 3),
		Buffer_createRefConst(DOMAINS[domain], sizeof(DOMAINS[domain]) - 1), output
	);

clean:
	return s_uccess;
}
