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

//types/container/test/test_types_container_encryption_stream.c

#include "test_types_container_shared.h"
#include "types/container/test/stream_harness.h"
#include "types/container/encryption_stream.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/container/buffer_encrypt.h"
#include "types/base/mathi.h"
#include "types/math/vec4i.h"
#include <stddef.h>

static const U32 encTestKey[8] = {
	0xCD00324F, 0x4CBAAE34, 0x67924E05, 0x78012F15,
	0x5A8F573A, 0xA066652D, 0xDDB8C2E1, 0xF76AF7FE
};

RefPtrType memType;

static Bool EncStream_harnessCreate(
	const StreamHarness *h, U64 size, Bool isResizable, RefPtr **out, Test *t
) {
	(void) h;

	const I32x4      rootIV  = I32x4_create4(0x11223344, 0x55667788, 0x99AABBCC, 0);
	static const U8 empty[65536] = { 0 };
	static const U64 chunkSize = sizeof(empty);

	RefPtr *backing = NULL;
	EMemoryStreamFlags flags = EMemoryStreamFlags_IsWritable;
	
	U64 realSize = EncryptionStream_underlyingSize(chunkSize, size);

	if (isResizable)
		flags |= EMemoryStreamFlags_IsResizable;

	if (!MemoryStream_create(realSize, flags, &memType, &backing, &t->err)) {
		Test_assert(t, "EncStream backing MemStream create", false);
		return false;
	}

	Bool ok = EncryptionStream_create(
		backing, 0, encTestKey, rootIV, chunkSize, 0, h->type, out, &t->err
	);

	RefPtr_dec(&backing);

	if (!ok) {
		Test_assert(t, "EncryptionStream_create", false);
		return false;
	}

	if (size) {

		OxStream *s = RefPtr_data(*out, OxStream);

		//Clear which also sets s->size without having garbage.
		//Garbage is fine with MemoryStream but not with others like EncryptionStream

		for(U64 i = 0; i < size; i += chunkSize)
			if (!s->write(s, i, U64_min(size - i, chunkSize), Buffer_createRefConst(empty, chunkSize), t->alloc, &t->err)) {
				Test_assert(t, "EncStream write empty bytes", false);
				RefPtr_dec(out);
				return false;
			}
	}

	return true;
}

static void Test_encryptionKeyDerivation(Test *t) {

	Test_setModule(t, "Encryption HKDF-SHA256");

	//Known answers from an independent HMAC-SHA256 (Python's hmac): HKDF with the iv's 12 little endian bytes as salt and the
	// domain label as info, so the derivation can't drift from RFC 5869 or from the key schedule files store, unnoticed.

	typedef struct KeyDerivationCase {
		const C8 *name;
		U32 key[8], iv[3];
		U8 expected[EEncryptionKeyDomain_Count][32];
	} KeyDerivationCase;

	static const KeyDerivationCase CASES[] = {
		{
			"counting bytes",
			{ 0x03020100, 0x07060504, 0x0B0A0908, 0x0F0E0D0C, 0x13121110, 0x17161514, 0x1B1A1918, 0x1F1E1D1C },
			{ 0xA3A2A1A0, 0xA7A6A5A4, 0xABAAA9A8 },
			{
				{
					0x4C, 0x8D, 0x7D, 0xDD, 0xA3, 0x64, 0xA9, 0xC5, 0xC9, 0xD8, 0xAD, 0xFF, 0x3E, 0x86, 0x45, 0x9B,
					0x52, 0x30, 0x2C, 0x63, 0x53, 0xD7, 0x75, 0x57, 0x78, 0xC5, 0x2E, 0xCD, 0x80, 0x85, 0x0A, 0xCF
				},
				{
					0x10, 0x3F, 0x11, 0x30, 0xE0, 0x33, 0x65, 0x7F, 0x71, 0x51, 0xC3, 0xF3, 0x62, 0x01, 0x6F, 0x69,
					0x4C, 0x78, 0x2C, 0x7B, 0x2B, 0x90, 0x7A, 0x44, 0x2D, 0xA1, 0x3D, 0xAD, 0xBB, 0xA2, 0x6E, 0x37
				},
				{
					0x4F, 0x98, 0xAC, 0x07, 0x10, 0x06, 0x24, 0x03, 0xA3, 0xEA, 0x40, 0x4C, 0x2C, 0x40, 0x4C, 0xF1,
					0xC1, 0x62, 0x41, 0xFE, 0x0F, 0xA4, 0xA0, 0xBE, 0xD6, 0x08, 0xE0, 0x9E, 0x50, 0xDB, 0x6E, 0x82
				}
			}
		},
		{
			"all zero",
			{ 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
			{ 0x00000000, 0x00000000, 0x00000000 },
			{
				{
					0x51, 0x62, 0x0C, 0xA8, 0xBC, 0x48, 0xCF, 0xEF, 0x46, 0x0F, 0x35, 0x13, 0xF2, 0x67, 0xF4, 0x66,
					0x21, 0xF9, 0x56, 0x28, 0x31, 0x2D, 0x5D, 0x20, 0x3E, 0xE4, 0xA4, 0x43, 0x16, 0xEF, 0x8F, 0x2D
				},
				{
					0x13, 0x69, 0xAB, 0x74, 0x66, 0xA1, 0x96, 0xAE, 0x70, 0xA2, 0x77, 0x4F, 0x16, 0x06, 0xB9, 0xA9,
					0xBC, 0x83, 0x4B, 0xF7, 0x83, 0xB2, 0xFE, 0xB0, 0xE1, 0x01, 0xE2, 0x66, 0x4C, 0xFD, 0xBC, 0x8A
				},
				{
					0x73, 0x79, 0xDA, 0x2C, 0x4A, 0x8E, 0x00, 0x81, 0x03, 0x86, 0xC4, 0x93, 0x15, 0x9E, 0x88, 0xF9,
					0x50, 0x57, 0xC0, 0x9B, 0x9D, 0x82, 0x70, 0x0F, 0x29, 0xBC, 0x13, 0xF6, 0x4C, 0xD2, 0xB9, 0x87
				}
			}
		},
		{
			"all ones",
			{ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
			{ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
			{
				{
					0xBD, 0x20, 0x1E, 0x96, 0xAC, 0xC7, 0xC9, 0xA9, 0xF1, 0x0A, 0xBC, 0xB6, 0xB9, 0xBF, 0x54, 0xA0,
					0xBD, 0x16, 0x63, 0x6A, 0xC6, 0xAC, 0x8C, 0x41, 0xF1, 0x63, 0xB5, 0x1B, 0x30, 0x6E, 0xC4, 0x68
				},
				{
					0xCD, 0xB8, 0x9F, 0x62, 0xD8, 0xA2, 0x3C, 0xE3, 0x92, 0x54, 0x65, 0x11, 0x0E, 0x0D, 0x59, 0xBB,
					0x2C, 0xD8, 0x83, 0x02, 0xD0, 0xAA, 0xAE, 0xFE, 0x6B, 0x54, 0x16, 0xF5, 0xB9, 0xBF, 0x28, 0x59
				},
				{
					0xDF, 0x52, 0x40, 0x43, 0xB0, 0x54, 0xB8, 0xB7, 0xB3, 0xAC, 0x07, 0xB9, 0x70, 0xE3, 0x71, 0x49,
					0xE7, 0x63, 0x59, 0x02, 0x7E, 0xE1, 0x20, 0x78, 0xB3, 0x0F, 0x93, 0x4C, 0x91, 0x5E, 0x2C, 0x5B
				}
			}
		},
		{
			"mixed",
			{ 0x9E3779B9, 0x3C6EF372, 0xDAA66D2B, 0x78DDE6E4, 0x1715609D, 0xB54CDA56, 0x5384540F, 0xF1BBCDC8 },
			{ 0x01234567, 0x89ABCDEF, 0xFEDCBA98 },
			{
				{
					0x8D, 0x6E, 0xAF, 0x88, 0x32, 0xCB, 0x63, 0x71, 0x33, 0x20, 0x82, 0x7F, 0xE3, 0x8B, 0xA1, 0x2D,
					0x28, 0x40, 0xBA, 0x0D, 0x6B, 0x31, 0x48, 0xB2, 0x9C, 0x5B, 0x06, 0x26, 0x24, 0x8B, 0x1A, 0xCC
				},
				{
					0x0A, 0x9E, 0x6A, 0x53, 0x63, 0xB5, 0x3C, 0x3C, 0xAF, 0xF2, 0x5D, 0xCB, 0x43, 0xE3, 0xC6, 0x23,
					0x65, 0xDF, 0x7A, 0xB3, 0xA8, 0x7D, 0x80, 0xFF, 0x06, 0x00, 0x26, 0xF2, 0x9A, 0xFB, 0x61, 0x87
				},
				{
					0xC1, 0xE6, 0xE6, 0x75, 0x71, 0x6F, 0x3C, 0x8F, 0x89, 0x5A, 0x4F, 0xD2, 0xAD, 0x51, 0xF1, 0xD9,
					0x9F, 0xC5, 0x0E, 0x8C, 0xB1, 0x65, 0x2C, 0x27, 0x3F, 0x48, 0xCA, 0xE0, 0xDF, 0xE1, 0xF0, 0x80
				}
			}
		}
	};

	for (U64 c = 0; c < sizeof(CASES) / sizeof(CASES[0]); ++c)
		for (U32 i = 0; i < EEncryptionKeyDomain_Count; ++i) {

			U32 derived[8] = { 0 };

			Test_assert(t, CASES[c].name, Buffer_deriveEncryptionKey(
				CASES[c].key, I32x4_load3(CASES[c].iv), (EEncryptionKeyDomain) i, derived, &t->err
			) && Buffer_eq(
				Buffer_createRefConst(derived, sizeof(derived)), Buffer_createRefConst(CASES[c].expected[i], 32)
			));
		}

	//Every byte of the 96-bit iv and of the key goes into the key: flipping any one of them gives another key.

	const KeyDerivationCase *base = &CASES[0];
	Bool ivBytesMatter = true, keyBytesMatter = true;

	for (U32 b = 0; b < 12; ++b) {

		U32 iv[3], derived[8] = { 0 };
		Buffer_memcpy(Buffer_createRef(iv, sizeof(iv)), Buffer_createRefConst(base->iv, sizeof(iv)));
		((U8*) iv)[b] ^= 1;

		ivBytesMatter &= Buffer_deriveEncryptionKey(
			base->key, I32x4_load3(iv), EEncryptionKeyDomain_StreamData, derived, &t->err
		) && Buffer_neq(Buffer_createRefConst(derived, 32), Buffer_createRefConst(base->expected[0], 32));
	}

	for (U32 b = 0; b < 32; ++b) {

		U32 key[8], derived[8] = { 0 };
		Buffer_memcpy(Buffer_createRef(key, sizeof(key)), Buffer_createRefConst(base->key, sizeof(key)));
		((U8*) key)[b] ^= 1;

		keyBytesMatter &= Buffer_deriveEncryptionKey(
			key, I32x4_load3(base->iv), EEncryptionKeyDomain_StreamData, derived, &t->err
		) && Buffer_neq(Buffer_createRefConst(derived, 32), Buffer_createRefConst(base->expected[0], 32));
	}

	Test_assert(t, "every iv byte changes the key", ivBytesMatter);
	Test_assert(t, "every key byte changes the key", keyBytesMatter);

	//The iv is 96 bits: the vector's fourth lane isn't part of it.

	{
		const U32 iv4[4] = { base->iv[0], base->iv[1], base->iv[2], 0xDEADBEEF };
		U32 derived[8] = { 0 };

		Test_assert(t, "the fourth iv lane is ignored", Buffer_deriveEncryptionKey(
			base->key, I32x4_load4(iv4), EEncryptionKeyDomain_StreamData, derived, &t->err
		) && Buffer_eq(Buffer_createRefConst(derived, 32), Buffer_createRefConst(base->expected[0], 32)));
	}

	//Refused inputs (the errors are expected, so they aren't reported)

	{
		U32 derived[8] = { 0 };
		const I32x4 iv = I32x4_load3(base->iv);

		Test_assert(t, "unknown domain refused", !Buffer_deriveEncryptionKey(
			base->key, iv, EEncryptionKeyDomain_Count, derived, NULL
		));

		Test_assert(t, "missing key refused", !Buffer_deriveEncryptionKey(
			NULL, iv, EEncryptionKeyDomain_StreamData, derived, NULL
		));

		Test_assert(t, "missing output refused", !Buffer_deriveEncryptionKey(
			base->key, iv, EEncryptionKeyDomain_StreamData, NULL, NULL
		));
	}

	U32 keys[EEncryptionKeyDomain_Count][8] = { 0 };

	for (U32 i = 0; i < EEncryptionKeyDomain_Count; ++i)
		Test_assert(t, "derive domain", Buffer_deriveEncryptionKey(
			encTestKey, I32x4_zero(), (EEncryptionKeyDomain) i, keys[i], &t->err
		));

	for (U32 i = 0; i < EEncryptionKeyDomain_Count; ++i)
		for (U32 j = i + 1; j < EEncryptionKeyDomain_Count; ++j)
			Test_assert(t, "header and data domains differ", Buffer_neq(
				Buffer_createRefConst(keys[i], 32), Buffer_createRefConst(keys[j], 32)
			));
}

static Bool EncStream_failWrite(OxStream *s, U64 off, U64 len, Buffer b, const Allocator *a, Error *e_rr) {

	(void) s; (void) off; (void) len; (void) b; (void) a;
	Bool s_uccess = true;

	retError(clean, Error_invalidState(0, "EncStream_failWrite()::injected failure"));

clean:
	return s_uccess;
}

static void Test_encryptionSecurity(Test *t) {

	Test_setModule(t, "EncryptionStream nonce and authentication");

	const U64 chunkSize = 65536;
	const I32x4 root = I32x4_create4(1, 2, 3, 0);
	const RefPtrType type = EncryptionStream_makeType(t->alloc);
	StreamRef *backing = NULL, *encrypted = NULL, *other = NULL;
	Buffer input = Buffer_createNull(), saved = Buffer_createNull();
	U8 readback[32] = { 0 };

	if (!Test_assert(t, "backing", MemoryStream_create(
		EncryptionStream_underlyingSize(chunkSize, chunkSize * 2), EMemoryStreamFlags_IsWritable,
		&memType, &backing, &t->err
	)))
		goto clean;

	if (!Test_assert(t, "encrypted stream", EncryptionStream_create(
		backing, 0, encTestKey, root, chunkSize, 0, &type, &encrypted, &t->err
	)))
		goto clean;

	EncryptionStream *es = RefPtr_data(encrypted, EncryptionStream);
	MemoryStream *ms = RefPtr_data(backing, MemoryStream);
	const U8 chunkSizeShift = 16;

	Test_assert(
		t, "packed field initializes only the chunk-size shift",
		es->encryptionCount56ChunkSizeShift8 == chunkSizeShift
	);

	Test_assert(t, "mutable scratch excludes concurrent reads", !(es->parent.streamType & EStreamType_ConcurrentRead));

	if (!Test_assert(t, "input", Buffer_createEmptyBytes(chunkSize * 2, t->alloc, &input, &t->err)))
		goto clean;

	Buffer_setAllToU8(input, 0x57, NULL);

	if (!Test_assert(t, "initial chunks", Stream_write(&es->parent, 0, 0, input, t->alloc, &t->err)))
		goto clean;

	Test_assert(
		t, "multi-chunk writes preserve the shift",
		es->encryptionCount56ChunkSizeShift8 == (((U64) 2 << 8) | chunkSizeShift)
	);

	CryptoChunk first = *(const CryptoChunk*) ms->data.ptr;

	if (!Test_assert(t, "rewrite same byte", Stream_write(
		&es->parent, 0, 1, input, t->alloc, &t->err
	)))
		goto clean;

	Test_assert(t, "rewrite generates fresh nonce", Buffer_neq(
		Buffer_createRefConst(first.nonce, 12), Buffer_createRefConst(ms->data.ptr, 12)
	));

	Test_assert(t, "rewrite reads", Stream_read(
		&es->parent, 0, sizeof(readback), Buffer_createRef(readback, sizeof(readback)), t->alloc, &t->err
	));

	Test_assert(t, "rewrite preserves neighbors", Buffer_eq(
		Buffer_createRefConst(readback, sizeof(readback)), Buffer_createRefConst(input.ptr, sizeof(readback))
	));

	if (!Test_assert(t, "save ciphertext", Buffer_createCopy(ms->data, t->alloc, &saved, &t->err)))
		goto clean;

	const U64 tamperOffsets[] = { 0, 12, 16, sizeof(CryptoChunk) };

	for (U8 i = 0; i < sizeof(tamperOffsets) / sizeof(tamperOffsets[0]); ++i) {

		ms->data.ptrNonConst[tamperOffsets[i]] ^= 1;

		Test_assert(t, "nonce reserved tag and ciphertext tampering rejected", !Stream_read(
			&es->parent, 0, 1, Buffer_createRef(readback, sizeof(readback)), t->alloc, NULL
		));

		Buffer_memcpy(ms->data, saved);
	}

	Buffer_memcpy(
		Buffer_createRef(ms->data.ptrNonConst, chunkSize + sizeof(CryptoChunk)),
		Buffer_createRefConst(saved.ptr + chunkSize + sizeof(CryptoChunk), chunkSize + sizeof(CryptoChunk))
	);

	Test_assert(t, "record relocation rejected", !Stream_read(
		&es->parent, 0, 1, Buffer_createRef(readback, sizeof(readback)), t->alloc, NULL
	));

	Buffer_memcpy(ms->data, saved);

	if (Test_assert(t, "open with other identity", EncryptionStream_create(
		backing, 0, encTestKey, I32x4_create4(1, 2, 4, 0), chunkSize, chunkSize * 2, &type, &other, &t->err
	))) {

		Test_assert(t, "other stream cannot accept record", !Stream_read(
			RefPtr_data(other, OxStream), 0, 1, Buffer_createRef(readback, sizeof(readback)), t->alloc, NULL
		));

		RefPtr_dec(&other);
	}

	if (Test_assert(t, "open with wrong logical length", EncryptionStream_create(
		backing, 0, encTestKey, root, chunkSize, chunkSize - 1, &type, &other, &t->err
	))) {

		Test_assert(t, "chunk length is authenticated", !Stream_read(
			RefPtr_data(other, OxStream), 0, 1, Buffer_createRef(readback, sizeof(readback)), t->alloc, NULL
		));

		RefPtr_dec(&other);
	}

	StreamFunc write = ms->parent.write;

	ms->parent.write = EncStream_failWrite;

	const U64 attempts = EncryptionStream_encryptionCount(es);

	Test_assert(t, "write failure propagates", !Stream_write(&es->parent, 0, 1, input, t->alloc, NULL));
	Test_assert(t, "failed write consumes key budget", EncryptionStream_encryptionCount(es) == attempts + 1);

	ms->parent.write = write;

	Test_assert(t, "retry succeeds", Stream_write(&es->parent, 0, 1, input, t->alloc, &t->err));
	Test_assert(t, "retry consumes fresh encryption", EncryptionStream_encryptionCount(es) == attempts + 2);
	Test_assert(t, "failure and retry preserve the shift", EncryptionStream_chunkSizeShift(es) == chunkSizeShift);

	const U64 maxAttempts = (U64) 1 << 32;
	const U64 exhausted = (maxAttempts << 8) | chunkSizeShift;

	es->encryptionCount56ChunkSizeShift8 = ((maxAttempts - 2) << 8) | chunkSizeShift;

	Test_assert(t, "final two permitted encryptions", Stream_write(&es->parent, 0, 0, input, t->alloc, &t->err));
	Test_assert(t, "multi-chunk write reaches the inclusive limit", es->encryptionCount56ChunkSizeShift8 == exhausted);

	const U64 oneRemaining = ((maxAttempts - 1) << 8) | chunkSizeShift;

	es->encryptionCount56ChunkSizeShift8 = oneRemaining;

	Test_assert(t, "multi-chunk write requires the whole remaining budget", !Stream_write(
		&es->parent, 0, 0, input, t->alloc, NULL
	));

	Test_assert(
		t, "refused multi-chunk write preserves the packed field",
		es->encryptionCount56ChunkSizeShift8 == oneRemaining
	);

	Test_assert(t, "final permitted encryption", Stream_write(&es->parent, 0, 1, input, t->alloc, &t->err));
	Test_assert(t, "final encryption preserves the shift", es->encryptionCount56ChunkSizeShift8 == exhausted);
	Test_assert(t, "exhausted key rejected", !Stream_write(&es->parent, 0, 1, input, t->alloc, NULL));
	Test_assert(t, "exhausted write preserves the packed field", es->encryptionCount56ChunkSizeShift8 == exhausted);
	Test_assert(t, "cross-chunk read at the counter limit", Stream_read(
		&es->parent, chunkSize - 8, sizeof(readback), Buffer_createRef(readback, sizeof(readback)), t->alloc, &t->err
	));

	Test_assert(t, "cross-chunk read preserves content", Buffer_eq(
		Buffer_createRefConst(readback, sizeof(readback)), Buffer_createRefConst(input.ptr, sizeof(readback))
	));

	Test_assert(t, "reading preserves the exhausted counter and shift", es->encryptionCount56ChunkSizeShift8 == exhausted);
	Test_assert(t, "oversized source rejected", !Stream_write(&es->parent, 0, Buffer_length(input) + 1, input, t->alloc, NULL));
	Test_assert(t, "overflowing offset rejected", !Stream_write(&es->parent, U64_MAX, 1, input, t->alloc, NULL));
	Test_assert(t, "const destination rejected", !Stream_read(
		&es->parent, 0, 1, Buffer_createRefConst(readback, sizeof(readback)), t->alloc, NULL
	));

clean:
	RefPtr_dec(&other);
	RefPtr_dec(&encrypted);
	RefPtr_dec(&backing);
	Buffer_free(&input, t->alloc);
	Buffer_free(&saved, t->alloc);
}

//A write past the end can extend across chunks: the old partial final chunk is sealed again at its new length,
// every chunk in the gap reads back as zeros and authenticates, and each chunk touched is one encryption.

static void Test_encryptionSparseExtension(Test *t) {

	Test_setModule(t, "EncryptionStream sparse extension across chunks");

	const U64 chunkSize = 65536;
	const I32x4 root = I32x4_create4(5, 6, 7, 0);
	const RefPtrType type = EncryptionStream_makeType(t->alloc);
	StreamRef *backing = NULL, *encrypted = NULL, *reopened = NULL;
	Buffer readback = Buffer_createNull();

	U8 head[100], tail[10];

	for(U8 i = 0; i < sizeof(head); ++i)
		head[i] = (U8) (i + 1);

	for(U8 i = 0; i < sizeof(tail); ++i)
		tail[i] = (U8) (0xA0 + i);

	const U64 tailOffset = chunkSize * 3 + 50;
	const U64 endSize = tailOffset + sizeof(tail);

	if (
		!Test_assert(t, "backing", MemoryStream_create(
			EncryptionStream_underlyingSize(chunkSize, endSize), EMemoryStreamFlags_IsWritable, &memType, &backing, &t->err
		)) ||
		!Test_assert(t, "encrypted stream", EncryptionStream_create(
			backing, 0, encTestKey, root, chunkSize, 0, &type, &encrypted, &t->err
		))
	)
		goto clean;

	EncryptionStream *es = RefPtr_data(encrypted, EncryptionStream);

	if(!Test_assert(t, "write head", Stream_write(
		&es->parent, 0, 0, Buffer_createRefConst(head, sizeof(head)), t->alloc, &t->err
	)))
		goto clean;

	const U64 attempts = EncryptionStream_encryptionCount(es);

	if (!Test_assert(t, "write past the end", Stream_write(
		&es->parent, tailOffset, 0, Buffer_createRefConst(tail, sizeof(tail)), t->alloc, &t->err
	)))
		goto clean;

	Test_assert(t, "extension grows to the write's end", es->parent.size == endSize);
	Test_assert(
		t, "old final chunk, two gap chunks and the new one", EncryptionStream_encryptionCount(es) == attempts + 4
	);

	if (
		!Test_assert(t, "reopen at the new size", EncryptionStream_create(
			backing, 0, encTestKey, root, chunkSize, endSize, &type, &reopened, &t->err
		)) ||
		!Test_assert(t, "readback", Buffer_createEmptyBytes(endSize, t->alloc, &readback, &t->err)) ||
		!Test_assert(t, "every chunk authenticates", Stream_read(
			RefPtr_data(reopened, OxStream), 0, endSize, readback, t->alloc, &t->err
		))
	)
		goto clean;

	Test_assert(t, "head kept", Buffer_eq(
		Buffer_createRefConst(readback.ptr, sizeof(head)), Buffer_createRefConst(head, sizeof(head))
	));

	Test_assert(t, "gap reads as zeros", Buffer_isZero(
		Buffer_createRefConst(readback.ptr + sizeof(head), tailOffset - sizeof(head))
	));

	Test_assert(t, "tail written", Buffer_eq(
		Buffer_createRefConst(readback.ptr + tailOffset, sizeof(tail)), Buffer_createRefConst(tail, sizeof(tail))
	));

	RefPtr_dec(&reopened);

	U8 byte = 0;

	if (Test_assert(t, "reopen at the old size", EncryptionStream_create(
		backing, 0, encTestKey, root, chunkSize, sizeof(head), &type, &reopened, &t->err
	)))
		Test_assert(t, "old final chunk was sealed at its new length", !Stream_read(
			RefPtr_data(reopened, OxStream), 0, 1, Buffer_createRef(&byte, 1), t->alloc, NULL
		));

clean:
	RefPtr_dec(&reopened);
	RefPtr_dec(&encrypted);
	RefPtr_dec(&backing);
	Buffer_free(&readback, t->alloc);
}

//A cursor over an encryption stream flushes whole chunks, so sequential appends encrypt every chunk once whatever sizes
// they come in: appends within the window, appends larger than the window and a stream copy.
//The cache is a chunk plus a CryptoChunk, as oiDL sizes it, so it isn't a whole number of chunks itself.

static void Test_encryptionCursorAlignment(Test *t) {

	Test_setModule(t, "EncryptionStream cursor writes whole chunks");

	const U64 chunkSize = 65536;
	const I32x4 root = I32x4_create4(9, 10, 11, 0);
	const RefPtrType type = EncryptionStream_makeType(t->alloc);
	static const U64 appends[] = { 1000, 70000, 200000, 30000, 65536 * 3 + 7, 12345 };
	const U64 copied = 300000;

	U64 total = copied;

	for (U64 i = 0; i < sizeof(appends) / sizeof(appends[0]); ++i)
		total += appends[i];

	StreamRef *backing = NULL, *encrypted = NULL, *source = NULL;
	StreamCursor output = (StreamCursor) { 0 }, input = (StreamCursor) { 0 };
	Buffer data = Buffer_createNull(), readback = Buffer_createNull();

	if (
		!Test_assert(t, "data", Buffer_createUninitializedBytes(total, t->alloc, &data, &t->err)) ||
		!Test_assert(t, "readback", Buffer_createEmptyBytes(total, t->alloc, &readback, &t->err))
	)
		goto clean;

	for (U64 i = 0; i < total; ++i)
		data.ptrNonConst[i] = (U8) (i * 31 + i / 997);

	if (
		!Test_assert(t, "backing", MemoryStream_create(
			0, EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable, &memType, &backing, &t->err
		)) ||
		!Test_assert(t, "encrypted stream", EncryptionStream_create(
			backing, 0, encTestKey, root, chunkSize, 0, &type, &encrypted, &t->err
		)) ||
		!Test_assert(t, "copy source", MemoryStream_createFromBufferRegion(
			Buffer_createRefConst(data.ptr, total), total - copied, copied, EMemoryStreamFlags_None, &memType, &source, &t->err
		)) ||
		!Test_assert(t, "output cursor", StreamCursor_create(
			encrypted, chunkSize + sizeof(CryptoChunk), true, t->alloc, &output, &t->err
		)) ||
		!Test_assert(t, "input cursor", StreamCursor_create(source, 32 * KIBI, false, t->alloc, &input, &t->err))
	)
		goto clean;

	U64 offset = 0;

	for (U64 i = 0; i < sizeof(appends) / sizeof(appends[0]); ++i)
		if (!Test_assert(t, "append", StreamCursor_append(
			&output, &offset, data.ptr + offset, appends[i], t->alloc, &t->err
		)))
			goto clean;

	if (
		!Test_assert(t, "copy stream", StreamCursor_copyStream(
			&output, &input, 0, offset, copied, t->alloc, &t->err
		)) ||
		!Test_assert(t, "flush", StreamCursor_flush(&output, t->alloc, &t->err))
	)
		goto clean;

	EncryptionStream *es = RefPtr_data(encrypted, EncryptionStream);
	const U64 chunks = (total + chunkSize - 1) / chunkSize;

	Test_assert(t, "every chunk encrypted once", EncryptionStream_encryptionCount(es) == chunks);

	Test_assert(t, "read back", Stream_read(&es->parent, 0, total, readback, t->alloc, &t->err) && Buffer_eq(
		Buffer_createRefConst(readback.ptr, total), Buffer_createRefConst(data.ptr, total)
	));

clean:
	StreamCursor_close(&input, t->alloc);
	StreamCursor_close(&output, t->alloc);
	RefPtr_dec(&source);
	RefPtr_dec(&encrypted);
	RefPtr_dec(&backing);
	Buffer_free(&readback, t->alloc);
	Buffer_free(&data, t->alloc);
}

void Test_encryptionStream(Test *t) {

	Test_setModule(t, "EncryptionStream packed layout");

	Test_assert(
		t, "counter and shift occupy one eight-byte slot",
		offsetof(EncryptionStream, internalCache) == offsetof(EncryptionStream, chunkSize) + sizeof(U32) + sizeof(U64)
	);

	Test_assert(
		t, "compact structure has no additional counter storage",
		sizeof(EncryptionStream) == offsetof(EncryptionStream, internalCache) + sizeof(Buffer)
	);

	const RefPtrType type = EncryptionStream_makeType(t->alloc);
	memType = MemoryStream_makeType(t->alloc);

	//Chunk sizes: a power of two above 32 KiB and below 4 GiB

	{
		const U64 refused[] = { 0, 32 * KIBI, 96 * KIBI, 65537, (U64) 1 << 32 };
		const I32x4 root = I32x4_create4(1, 2, 3, 0);
		StreamRef *backing = NULL, *encrypted = NULL;

		if (Test_assert(t, "chunk size backing", MemoryStream_create(
			0, EMemoryStreamFlags_IsWritable | EMemoryStreamFlags_IsResizable, &memType, &backing, &t->err
		))) {

			for (U64 i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
				Test_assert(t, "invalid chunk size refused", !EncryptionStream_create(
					backing, 0, encTestKey, root, refused[i], 0, &type, &encrypted, NULL
				));

			Test_assert(t, "64 KiB chunks accepted", EncryptionStream_create(
				backing, 0, encTestKey, root, 64 * KIBI, 0, &type, &encrypted, &t->err
			));
		}

		RefPtr_dec(&encrypted);
		RefPtr_dec(&backing);
	}

	StreamHarness h = {
		.create = EncStream_harnessCreate,
		.type = &type,
		.name = "EncryptionStream"
	};

	Test_encryptionKeyDerivation(t);
	Test_encryptionSecurity(t);
	Test_encryptionSparseExtension(t);
	Test_encryptionCursorAlignment(t);
	StreamHarness_testStream(&h, t);
	StreamHarness_testCursor(&h, t);
	Test_setModule(t, NULL);
}
