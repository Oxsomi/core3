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

//types/container/test/test_types_container_buffer.c

//Buffer itself.
//The hashing and encryption suites lean on Buffer heavily but only ever exercise create/free/eq, so the
//bit manipulation, ref/const-ref safety and unicode paths were untested.

#include "test_types_container_shared.h"
#include "types/container/buffer.h"
#include "types/base/buffer_base.h"
#include "types/base/algorithm.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

//Wraps another allocator and remembers the last pointer it handed out and the last one it was asked to
// release, so a test can assert those are the same rather than trusting that nothing crashed.

typedef struct RecordingAllocator {
	const Allocator *inner;
	const void *allocPtr, *freedPtr;
	U64 allocLength, freedLength;
	I64 live;
} RecordingAllocator;

static Bool RecordingAllocator_alloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	RecordingAllocator *rec = (RecordingAllocator*) allocator;

	if(!rec->inner->alloc(rec->inner->ptr, length, output, e_rr))
		return false;

	rec->allocPtr = output->ptr;
	rec->allocLength = Buffer_length(*output);
	++rec->live;
	return true;
}

static void RecordingAllocator_free(void *allocator, Buffer buf) {

	RecordingAllocator *rec = (RecordingAllocator*) allocator;

	rec->freedPtr = buf.ptr;
	rec->freedLength = Buffer_length(buf);
	--rec->live;

	rec->inner->free(rec->inner->ptr, buf);
}

void Test_containerBuffer(Test *t) {

	const Allocator *alloc = t->alloc;
	Error err = Error_none(), *e_rr = &err;

	// -- Bits ---------------------------------------------------------------------------------------

	Test_setModule(t, "BufferBits");

	{
		Buffer buf = Buffer_createNull();
		Test_assert(t, "createZeroBits", Buffer_createZeroBits(64, alloc, &buf, e_rr));
		Test_assert(t, "createZeroBits is 8 bytes", Buffer_length(buf) == 8);

		Bool bit = true;
		Test_assert(t, "zero bits are clear", Buffer_getBit(buf, 0, &bit, e_rr) && !bit);

		Test_assert(t, "setBit", Buffer_setBit(buf, 3, e_rr));
		Test_assert(t, "setBit landed", Buffer_getBit(buf, 3, &bit, e_rr) && bit);
		Test_assert(t, "setBit left neighbours alone", Buffer_getBit(buf, 2, &bit, e_rr) && !bit);

		Test_assert(t, "resetBit", Buffer_resetBit(buf, 3, e_rr));
		Test_assert(t, "resetBit landed", Buffer_getBit(buf, 3, &bit, e_rr) && !bit);

		Test_assert(t, "setBitTo true", Buffer_setBitTo(buf, 9, true, e_rr));
		Test_assert(t, "setBitTo true landed", Buffer_getBit(buf, 9, &bit, e_rr) && bit);
		Test_assert(t, "setBitTo false", Buffer_setBitTo(buf, 9, false, e_rr));
		Test_assert(t, "setBitTo false landed", Buffer_getBit(buf, 9, &bit, e_rr) && !bit);

		//Bit indices are in bits, so 64 is one past the end of an 8 byte buffer

		Test_assert(t, "getBit out of bounds", !Buffer_getBit(buf, 64, &bit, NULL));
		Test_assert(t, "setBit out of bounds", !Buffer_setBit(buf, 64, NULL));

		//Ranges, including one that doesn't start or end on a byte boundary

		Test_assert(t, "setBitRange", Buffer_setBitRange(buf, 5, 10, e_rr));

		Bool allSet = true;

		for(U64 i = 5; i < 15; ++i) {
			Buffer_getBit(buf, i, &bit, e_rr);
			allSet &= bit;
		}

		Test_assert(t, "setBitRange set every bit", allSet);
		Test_assert(t, "setBitRange stopped at the start", Buffer_getBit(buf, 4, &bit, e_rr) && !bit);
		Test_assert(t, "setBitRange stopped at the end", Buffer_getBit(buf, 15, &bit, e_rr) && !bit);

		Test_assert(t, "unsetBitRange", Buffer_unsetBitRange(buf, 5, 10, e_rr));
		Test_assert(t, "unsetBitRange cleared", Buffer_getBit(buf, 9, &bit, e_rr) && !bit);

		//A range wholly inside one byte has no full bytes in the middle, so head and tail would overlap

		Buffer_unsetAllBits(buf, e_rr);
		Test_assert(t, "setBitRange within one byte", Buffer_setBitRange(buf, 5, 2, e_rr));
		Test_assert(t, "sub-byte range set 5", Buffer_getBit(buf, 5, &bit, e_rr) && bit);
		Test_assert(t, "sub-byte range set 6", Buffer_getBit(buf, 6, &bit, e_rr) && bit);
		Test_assert(t, "sub-byte range left 4 alone", Buffer_getBit(buf, 4, &bit, e_rr) && !bit);
		Test_assert(t, "sub-byte range left 7 alone", Buffer_getBit(buf, 7, &bit, e_rr) && !bit);

		//Byte aligned, so only the memset path runs

		Buffer_unsetAllBits(buf, e_rr);
		Test_assert(t, "setBitRange byte aligned", Buffer_setBitRange(buf, 8, 16, e_rr));
		Test_assert(t, "aligned range filled", buf.ptr[1] == 0xFF && buf.ptr[2] == 0xFF);
		Test_assert(t, "aligned range stopped", !buf.ptr[0] && !buf.ptr[3]);

		//And the unset twin over the same shapes

		Buffer_setAllBits(buf, e_rr);
		Test_assert(t, "unsetBitRange within one byte", Buffer_unsetBitRange(buf, 5, 2, e_rr));
		Test_assert(t, "sub-byte unset 5", Buffer_getBit(buf, 5, &bit, e_rr) && !bit);
		Test_assert(t, "sub-byte unset kept 7", Buffer_getBit(buf, 7, &bit, e_rr) && bit);

		Buffer_setAllBits(buf, e_rr);
		Test_assert(t, "unsetBitRange unaligned span", Buffer_unsetBitRange(buf, 5, 10, e_rr));
		Test_assert(t, "unset span cleared 5", Buffer_getBit(buf, 5, &bit, e_rr) && !bit);
		Test_assert(t, "unset span cleared 14", Buffer_getBit(buf, 14, &bit, e_rr) && !bit);
		Test_assert(t, "unset span kept 4", Buffer_getBit(buf, 4, &bit, e_rr) && bit);
		Test_assert(t, "unset span kept 15", Buffer_getBit(buf, 15, &bit, e_rr) && bit);

		Test_assert(t, "setAllBits", Buffer_setAllBits(buf, e_rr));
		Test_assert(t, "setAllBits set the last one", Buffer_getBit(buf, 63, &bit, e_rr) && bit);

		Test_assert(t, "unsetAllBits", Buffer_unsetAllBits(buf, e_rr));
		Test_assert(t, "unsetAllBits cleared the last one", Buffer_getBit(buf, 63, &bit, e_rr) && !bit);

		Test_assert(t, "setAllBitsTo true", Buffer_setAllBitsTo(buf, true, e_rr));
		Test_assert(t, "setAllBitsTo true landed", Buffer_getBit(buf, 7, &bit, e_rr) && bit);

		Buffer_free(&buf, alloc);

		Buffer ones = Buffer_createNull();
		Test_assert(t, "createOneBits", Buffer_createOneBits(16, alloc, &ones, e_rr));
		Test_assert(t, "createOneBits is all ones", ones.ptr[0] == 0xFF && ones.ptr[1] == 0xFF);
		Buffer_free(&ones, alloc);

		Buffer bits = Buffer_createNull();
		Test_assert(t, "createBits(false)", Buffer_createBits(16, false, alloc, &bits, e_rr));
		Test_assert(t, "createBits(false) is zeroed", !bits.ptr[0] && !bits.ptr[1]);
		Buffer_free(&bits, alloc);
	}

	// -- Bitwise ops --------------------------------------------------------------------------------

	Test_setModule(t, "BufferBitwise");

	{
		U8 aBytes[4] = { 0xF0, 0x0F, 0xAA, 0x55 };
		U8 bBytes[4] = { 0xFF, 0x00, 0x0F, 0xF0 };

		Buffer a = Buffer_createRef(aBytes, sizeof(aBytes));
		const Buffer b = Buffer_createRefConst(bBytes, sizeof(bBytes));

		Test_assert(t, "bitwiseAnd", Buffer_bitwiseAnd(a, b, e_rr));
		Test_assert(t, "bitwiseAnd result", aBytes[0] == 0xF0 && aBytes[1] == 0x00 && aBytes[2] == 0x0A);

		aBytes[0] = 0xF0; aBytes[1] = 0x0F; aBytes[2] = 0xAA; aBytes[3] = 0x55;
		Test_assert(t, "bitwiseOr", Buffer_bitwiseOr(a, b, e_rr));
		Test_assert(t, "bitwiseOr result", aBytes[0] == 0xFF && aBytes[1] == 0x0F && aBytes[3] == 0xF5);

		aBytes[0] = 0xF0; aBytes[1] = 0x0F; aBytes[2] = 0xAA; aBytes[3] = 0x55;
		Test_assert(t, "bitwiseXor", Buffer_bitwiseXor(a, b, e_rr));
		Test_assert(t, "bitwiseXor result", aBytes[0] == 0x0F && aBytes[2] == 0xA5);

		aBytes[0] = 0xF0;
		Test_assert(t, "bitwiseNot", Buffer_bitwiseNot(a, e_rr));
		Test_assert(t, "bitwiseNot result", aBytes[0] == 0x0F);

		//A const ref is not writable, which is the whole point of the distinction

		Buffer constRef = Buffer_createRefConst(bBytes, sizeof(bBytes));
		Test_assert(t, "isConstRef", Buffer_isConstRef(constRef) && Buffer_isRef(constRef));
		Test_assert(t, "bitwiseNot on const ref rejected", !Buffer_bitwiseNot(constRef, NULL));
		Test_assert(t, "setBit on const ref rejected", !Buffer_setBit(constRef, 0, NULL));
	}

	// -- cmp / eq -----------------------------------------------------------------------------------

	Test_setModule(t, "BufferCompare");

	{
		const U8 x[3] = { 1, 2, 3 };
		const U8 y[3] = { 1, 2, 4 };
		const U8 z[4] = { 1, 2, 3, 4 };

		const Buffer bx = Buffer_createRefConst(x, sizeof(x));
		const Buffer by = Buffer_createRefConst(y, sizeof(y));
		const Buffer bz = Buffer_createRefConst(z, sizeof(z));

		Test_assert(t, "eq self", Buffer_eq(bx, bx));
		Test_assert(t, "neq differing content", Buffer_neq(bx, by));
		Test_assert(t, "neq differing length", Buffer_neq(bx, bz));
		Test_assert(t, "cmp equal", Buffer_cmp(bx, bx) == ECompareResult_Eq);
		Test_assert(t, "cmp less", Buffer_cmp(bx, by) == ECompareResult_Lt);
		Test_assert(t, "cmp greater", Buffer_cmp(by, bx) == ECompareResult_Gt);
	}

	// -- Copy / move / append / consume -------------------------------------------------------------

	Test_setModule(t, "BufferCopy");

	{
		U8 dst[8] = { 0 };
		const U8 src[4] = { 9, 8, 7, 6 };

		Buffer bdst = Buffer_createRef(dst, sizeof(dst));
		const Buffer bsrc = Buffer_createRefConst(src, sizeof(src));

		//memcpy copies min(dst, src) bytes, so a larger destination keeps its tail

		Test_assert(t, "memcpy", Buffer_memcpy(bdst, bsrc));
		Test_assert(t, "memcpy content", dst[0] == 9 && dst[3] == 6 && dst[4] == 0);

		//memmove has to survive overlapping ranges, which is why it exists separately

		U8 overlap[6] = { 1, 2, 3, 4, 5, 6 };
		const Buffer to = Buffer_createRef(overlap + 2, 4);
		const Buffer from = Buffer_createRef(overlap, 4);

		Test_assert(t, "memmove overlapping", Buffer_memmove(to, from));
		Test_assert(t, "memmove content", overlap[2] == 1 && overlap[3] == 2 && overlap[5] == 4);

		//append/consume walk a buffer as a cursor

		U8 storage[16] = { 0 };
		Buffer cursor = Buffer_createRef(storage, sizeof(storage));

		const U32 magic = 0xDEADBEEF;
		Test_assert(t, "append", Buffer_append(&cursor, &magic, sizeof(magic), e_rr));
		Test_assert(t, "append advanced the cursor", Buffer_length(cursor) == 12);

		Buffer readBack = Buffer_createRef(storage, sizeof(storage));
		U32 got = 0;
		Test_assert(t, "consume", Buffer_consume(&readBack, &got, sizeof(got), e_rr));
		Test_assert(t, "consume round trips", got == magic);

		//Running past the end is an error rather than a silent overrun

		Buffer tiny = Buffer_createRef(storage, 2);
		U32 tooBig = 0;
		Test_assert(t, "consume past end", !Buffer_consume(&tiny, &tooBig, sizeof(tooBig), NULL));
		Test_assert(t, "offset past end", !Buffer_offset(&tiny, 99, NULL));
	}

	// -- Subset / resize / combine ------------------------------------------------------------------

	Test_setModule(t, "BufferShape");

	{
		Buffer buf = Buffer_createNull();
		Test_assert(t, "createUninitializedBytes", Buffer_createUninitializedBytes(16, alloc, &buf, e_rr));
		Test_assert(t, "length", Buffer_length(buf) == 16);

		Buffer sub = Buffer_createNull();
		Test_assert(t, "createSubset", Buffer_createSubset(buf, 4, 8, false, &sub, e_rr));
		Test_assert(t, "subset aliases", sub.ptr == buf.ptr + 4 && Buffer_length(sub) == 8);
		Test_assert(t, "subset is a ref", Buffer_isRef(sub));
		Test_assert(t, "subset past end rejected", !Buffer_createSubset(buf, 12, 8, false, &sub, NULL));

		//resize keeps existing content and zeroes anything new

		Buffer_setAllBits(buf, e_rr);
		Test_assert(t, "resize larger", Buffer_resize(&buf, 32, true, true, alloc, e_rr));
		Test_assert(t, "resize kept content", buf.ptr[0] == 0xFF && buf.ptr[15] == 0xFF);
		Test_assert(t, "resize zeroed the growth", !buf.ptr[16] && !buf.ptr[31]);

		Test_assert(t, "resize smaller", Buffer_resize(&buf, 8, true, true, alloc, e_rr));
		Test_assert(t, "resize smaller length", Buffer_length(buf) == 8);

		Buffer_free(&buf, alloc);

		//combine concatenates into a fresh allocation

		const U8 p[2] = { 1, 2 };
		const U8 q[3] = { 3, 4, 5 };
		const Buffer bp = Buffer_createRefConst(p, sizeof(p));
		const Buffer bq = Buffer_createRefConst(q, sizeof(q));

		Buffer joined = Buffer_createNull();
		Test_assert(t, "combine", Buffer_combine(&bp, &bq, alloc, &joined, e_rr));
		Test_assert(t, "combine length", Buffer_length(joined) == 5);
		Test_assert(t, "combine order", joined.ptr[0] == 1 && joined.ptr[2] == 3 && joined.ptr[4] == 5);
		Buffer_free(&joined, alloc);
	}

	// -- Unicode ------------------------------------------------------------------------------------

	Test_setModule(t, "BufferUnicode");

	{
		//Codepoints U+0061, U+00E9 and U+20AC: 1, 2 and 3 byte UTF-8 sequences, so every width is exercised

		const U8 utf8[] = { 'a', 0xC3, 0xA9, 0xE2, 0x82, 0xAC };
		const Buffer b8 = Buffer_createRefConst(utf8, sizeof(utf8));

		UnicodeCodePointInfo cp = (UnicodeCodePointInfo) { 0 };

		Test_assert(t, "readAsUTF8 ascii", Buffer_readAsUTF8(b8, 0, &cp, e_rr));
		Test_assert(t, "readAsUTF8 ascii value", cp.index == 'a' && cp.bytes == 1);

		Test_assert(t, "readAsUTF8 two byte", Buffer_readAsUTF8(b8, 1, &cp, e_rr));
		Test_assert(t, "readAsUTF8 two byte value", cp.index == 0xE9 && cp.bytes == 2);

		Test_assert(t, "readAsUTF8 three byte", Buffer_readAsUTF8(b8, 3, &cp, e_rr));
		Test_assert(t, "readAsUTF8 three byte value", cp.index == 0x20AC && cp.bytes == 3);

		//Round trip through the writer

		U8 scratch[8] = { 0 };
		const Buffer bs = Buffer_createRef(scratch, sizeof(scratch));
		U8 written = 0;

		Test_assert(t, "writeAsUTF8", Buffer_writeAsUTF8(bs, 0, 0x20AC, &written, e_rr));
		Test_assert(t, "writeAsUTF8 width", written == 3);
		Test_assert(t, "writeAsUTF8 bytes", scratch[0] == 0xE2 && scratch[1] == 0x82 && scratch[2] == 0xAC);

		Test_assert(t, "writeAsUTF16", Buffer_writeAsUTF16(bs, 0, 'a', &written, e_rr));
		Test_assert(t, "writeAsUTF16 width", written == 2);

		Test_assert(t, "readAsUTF16", Buffer_readAsUTF16(bs, 0, &cp, e_rr));
		Test_assert(t, "readAsUTF16 value", cp.index == 'a');

		//Truncated sequences must be refused rather than read past the end

		const Buffer truncated = Buffer_createRefConst(utf8 + 3, 1);        //Lead byte of a 3 byte sequence
		Test_assert(t, "readAsUTF8 truncated", !Buffer_readAsUTF8(truncated, 0, &cp, NULL));

		//isAscii / isUnicode heuristics

		const U8 ascii[] = { 'h', 'e', 'l', 'l', 'o' };
		Test_assert(t, "isAscii", Buffer_isAscii(Buffer_createRefConst(ascii, sizeof(ascii))));
		Test_assert(t, "isAscii false for utf8", !Buffer_isAscii(b8));
		Test_assert(t, "isUTF8", Buffer_isUTF8(b8, 0.75f));
	}

	// -- Hashing / random ---------------------------------------------------------------------------

	Test_setModule(t, "BufferMisc");

	{
		const U8 data[4] = { 1, 2, 3, 4 };
		const Buffer b = Buffer_createRefConst(data, sizeof(data));

		const U64 h0 = Buffer_fnv1a64(b, Buffer_fnv1a64Offset);
		const U64 h1 = Buffer_fnv1a64(b, Buffer_fnv1a64Offset);
		Test_assert(t, "fnv1a64 is deterministic", h0 == h1);
		Test_assert(t, "fnv1a64 isn't the seed", h0 != Buffer_fnv1a64Offset);

		const U8 other[4] = { 1, 2, 3, 5 };
		const Buffer bo = Buffer_createRefConst(other, sizeof(other));
		Test_assert(t, "fnv1a64 differs on content", Buffer_fnv1a64(bo, Buffer_fnv1a64Offset) != h0);

		Test_assert(t, "fnv1a64Single chains", Buffer_fnv1a64Single(1, Buffer_fnv1a64Offset) != Buffer_fnv1a64Offset);

		//csprng has to actually fill the buffer; two draws matching would be a red flag

		Buffer r0 = Buffer_createNull(), r1 = Buffer_createNull();
		Test_assert(t, "csprng buffer 0", Buffer_createUninitializedBytes(32, alloc, &r0, e_rr));
		Test_assert(t, "csprng buffer 1", Buffer_createUninitializedBytes(32, alloc, &r1, e_rr));
		Test_assert(t, "csprng 0", Buffer_csprng(r0));
		Test_assert(t, "csprng 1", Buffer_csprng(r1));
		Test_assert(t, "csprng draws differ", Buffer_neq(r0, r1));

		Buffer_free(&r0, alloc);
		Buffer_free(&r1, alloc);

		//clearAllSecure must not be optimised away; all it has to guarantee here is that it clears

		Buffer secret = Buffer_createNull();
		Test_assert(t, "secret buffer", Buffer_createUninitializedBytes(16, alloc, &secret, e_rr));
		Buffer_setAllBits(secret, e_rr);
		Buffer_clearAllSecure(secret);
		Test_assert(t, "clearAllSecure zeroed", !secret.ptr[0] && !secret.ptr[15]);
		Buffer_free(&secret, alloc);
	}

	// -- Aligned allocation -------------------------------------------------------------------------

	Test_setModule(t, "BufferAligned");

	{
		//A run of them, because a single one lands on an aligned address often enough by luck to pass.

		for (U64 alignment = 1; alignment <= BUFFER_ALIGN_MAX; alignment <<= 1) {

			Buffer allocs[8];
			Bool aligned = true, zeroed = true;

			for (U64 i = 0; i < 8; ++i) {

				allocs[i] = Buffer_createNull();

				Test_assert(t, "aligned alloc", Buffer_createEmptyBytesAligned(
					1 + i * 13, alignment, 0, alloc, &allocs[i], e_rr
				));

				aligned &= !((U64)allocs[i].ptr & (alignment - 1));
				zeroed &= !allocs[i].ptr[0];

				allocs[i].ptrNonConst[0] = 0xAB;        //Catch a free that hands back the wrong base
			}

			Test_assert(t, "aligned to request", aligned);
			Test_assert(t, "aligned alloc is zeroed", zeroed);

			for (U64 i = 0; i < 8; ++i)
				Buffer_free(&allocs[i], alloc);

			Test_assert(t, "aligned free clears", !allocs[7].ptr && !Buffer_length(allocs[7]));
		}

		//headerOffset aligns what comes after the reserved bytes, not the pointer itself.

		Buffer offset = Buffer_createNull();

		Test_assert(t, "aligned alloc with header", Buffer_createUninitializedBytesAligned(
			256, 64, 24, alloc, &offset, e_rr
		));

		Test_assert(t, "header offset lands aligned", !((U64)(offset.ptr + 24) & 63));
		Buffer_free(&offset, alloc);

		//Rejected up front rather than silently rounded

		Buffer bad = Buffer_createNull();
		Test_assert(t, "rejects non power of two", !Buffer_createEmptyBytesAligned(16, 24, 0, alloc, &bad, NULL));
		Test_assert(t, "rejects zero alignment", !Buffer_createEmptyBytesAligned(16, 0, 0, alloc, &bad, NULL));

		Test_assert(t, "rejects alignment over the max", !Buffer_createEmptyBytesAligned(
			16, BUFFER_ALIGN_MAX << 1, 0, alloc, &bad, NULL
		));

		Test_assert(t, "nothing leaked on rejection", !bad.ptr);
	}

	// -- Aligned allocation gives the allocator back exactly what it handed out ----------------------

	//Everything above checks the pointer that comes out.
	//This checks the one that goes back in, which is the half that actually corrupts the heap when it is wrong:
	// an aligned buffer points into the middle of a bigger block, so freeing it at face value hands the
	// allocator an interior pointer with the wrong length.
	//The tracking allocator in Debug notices, a release build does not, and the damage is silent either way.

	Test_setModule(t, "BufferAlignedFree");

	{
		RecordingAllocator rec = (RecordingAllocator) { .inner = alloc };

		const Allocator recording = (Allocator) {
			.ptr = &rec,
			.alloc = RecordingAllocator_alloc,
			.free = RecordingAllocator_free
		};

		//headerOffset varied too, since that shifts where the aligned pointer lands inside the block and so
		// changes the offset byte that free has to read back.

		const U64 headerOffsets[] = { 0, 16, 24 };

		Bool balanced = true;

		for (U64 alignment = 1; alignment <= BUFFER_ALIGN_MAX; alignment <<= 1)
			for (U64 h = 0; h < sizeof(headerOffsets) / sizeof(headerOffsets[0]); ++h) {

				Buffer buf = Buffer_createNull();

				if(!Buffer_createEmptyBytesAligned(1 + alignment * 3, alignment, headerOffsets[h], &recording, &buf, NULL))
					continue;

				rec.freedPtr = NULL;
				rec.freedLength = 0;

				Buffer_free(&buf, &recording);

				balanced &= rec.freedPtr == rec.allocPtr && rec.freedLength == rec.allocLength;
			}

		Test_assert(t, "aligned free returns the allocated pointer", balanced);
		Test_assert(t, "no allocations outstanding", rec.live == 0);
	}

	Test_setModule(t, NULL);
}
