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

//types/container/buffer.c

#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/c8.h"
#include "types/container/buffer.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "types/base/buffer_base.h"

#include <string.h>

static inline Bool Buffer_allocBitsInternal(U64 length, const Allocator *alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!length)
		retError(clean, Error_invalidParameter(0, 0, "Buffer_allocBitsInternal()::length should be non zero"));

	if(length >> 51)
		retError(clean, Error_invalidParameter(
			0, 0, "Buffer_allocBitsInternal()::length buffer length can't exceed 1 << 48 bytes"
		));

	if(!alloc || !alloc->alloc)
		retError(clean, Error_nullPointer(1, "Buffer_allocBitsInternal()::alloc and alloc->alloc should be defined"));

	if(!result)
		retError(clean, Error_nullPointer(0, "Buffer_allocBitsInternal()::result is required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "Buffer_allocBitsInternal()::result->ptr is non zero, could indicate memleak"
		));

	length = (length + 7) >> 3;    //Align to bytes
	gotoIfError3(clean, alloc->alloc(alloc->ptr, length, result, e_rr));

clean:
	return s_uccess;
}

Bool Buffer_createZeroBits(U64 length, const Allocator *alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	gotoIfError3(clean, Buffer_allocBitsInternal(length, alloc, result, e_rr));
	allocated = true;
	gotoIfError3(clean, Buffer_unsetAllBits(*result, e_rr));

clean:
	if(!s_uccess && allocated)
		Buffer_free(result, alloc);

	return s_uccess;
}

Bool Buffer_createOneBits(U64 length, const Allocator *alloc, Buffer *result, Error *e_rr ) {

	Bool s_uccess = true;
	Bool allocated = false;

	gotoIfError3(clean, Buffer_allocBitsInternal(length, alloc, result, e_rr));
	allocated = true;
	gotoIfError3(clean, Buffer_setAllBits(*result, e_rr));

clean:
	if(!s_uccess && allocated)
		Buffer_free(result, alloc);

	return s_uccess;
}

Bool Buffer_createCopy(const Buffer buf, const Allocator *alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;

	if (result && result->ptr)
		retError(clean, Error_invalidOperation(0, "Buffer_createCopy()::result is non zero, could indicate memleak"));

	if(!Buffer_length(buf)) {

		if(result)
			*result = Buffer_createNull();

		goto clean;
	}

	const U64 l = Buffer_length(buf);
	gotoIfError3(clean, Buffer_allocBitsInternal(l << 3, alloc, result, e_rr));

	Buffer_memcpy(*result, buf);

clean:
	return s_uccess;
}

Bool Buffer_createUninitializedBytes(U64 length, const Allocator *alloc, Buffer *result, Error *e_rr) {

	if (!length && result) {
		*result = Buffer_createNull();
		return true;
	}

	return Buffer_allocBitsInternal(length >> 61 ? U64_MAX : length << 3, alloc, result, e_rr);
}

//A single U8 immediately before the aligned pointer, holding how far we stepped forward from the real allocation.
//That's all Buffer_free needs to find the base again, and a byte is aligned anywhere,
// so it needs no padding of its own.
//
//The over-allocation is a fixed BUFFER_ALIGN_MAX rather than the requested alignment,
// which is what keeps the header down to that one byte: the size is then always length + BUFFER_ALIGN_MAX,
// so it follows from the length the Buffer already carries and never has to be stored.
//The offset lands in [1, alignment], so BUFFER_ALIGN_MAX is what makes it fit in a U8.

static Bool Buffer_createBytesAlignedInternal(
	U64 length, U64 alignment, U64 headerOffset, Bool zeroed, const Allocator *alloc, Buffer *result, Error *e_rr
) {

	Bool s_uccess = true;
	Buffer base = Buffer_createNull();

	if(!result)
		retError(clean, Error_nullPointer(4, "Buffer_createBytesAlignedInternal()::result is required"));

	if(result->ptr)
		retError(clean, Error_invalidParameter(
			4, 0, "Buffer_createBytesAlignedInternal()::result->ptr is non zero, could indicate memleak"
		));

	if(!alignment || (alignment & (alignment - 1)))
		retError(clean, Error_invalidParameter(
			1, 0, "Buffer_createBytesAlignedInternal()::alignment must be a non zero power of two"
		));

	if(alignment > BUFFER_ALIGN_MAX)
		retError(clean, Error_invalidParameter(
			1, 0, "Buffer_createBytesAlignedInternal()::alignment exceeds BUFFER_ALIGN_MAX"
		));

	if(!length)
		goto clean;

	const U64 total = length + BUFFER_ALIGN_MAX;

	if(zeroed) {
		gotoIfError3(clean, Buffer_createEmptyBytes(total, alloc, &base, e_rr));
	}

	else {
		gotoIfError3(clean, Buffer_createUninitializedBytes(total, alloc, &base, e_rr));
	}

	//From start + 1 so there's always a byte in front of the result to put the offset in.
	//headerOffset shifts what gets aligned: RefPtr wants its payload aligned, not the header before it.

	U8 *const start = base.ptrNonConst;

	const U64 misaligned = (U64)(start + 1 + headerOffset) & (alignment - 1);
	U8 *const aligned = start + 1 + (misaligned ? alignment - misaligned : 0);

	aligned[-1] = (U8) (aligned - start);

	//Marked aligned so Buffer_free knows to walk back to start rather than freeing the interior pointer.
	//Without the bit the caller has to remember which creator produced the buffer, which anything holding it
	// generically (a C++ destructor, a container) has no way of knowing.

	*result = Buffer_createManagedPtr(aligned, length);
	Buffer_markAligned(result);

	base = Buffer_createNull();        //Ownership moved into result

clean:
	Buffer_free(&base, alloc);
	return s_uccess;
}

Bool Buffer_createUninitializedBytesAligned(
	U64 length, U64 alignment, U64 headerOffset, const Allocator *alloc, Buffer *result, Error *e_rr
) {
	return Buffer_createBytesAlignedInternal(length, alignment, headerOffset, false, alloc, result, e_rr);
}

Bool Buffer_createEmptyBytesAligned(
	U64 length, U64 alignment, U64 headerOffset, const Allocator *alloc, Buffer *result, Error *e_rr
) {
	return Buffer_createBytesAlignedInternal(length, alignment, headerOffset, true, alloc, result, e_rr);
}

void Buffer_free(Buffer *buf, const Allocator *alloc) {

	if(!buf || !Buffer_length(*buf))
		return;

	//References SHOULD NEVER be freed through the allocator.
	//We aren't the ones managing them

	if (Buffer_isRef(*buf)) {
		*buf = Buffer_createNull();
		return;
	}

	//Otherwise we should free

	if(!alloc || !alloc->free)
		return;

	//An aligned allocation points into the middle of what was allocated, with the distance back to the base in
	// the byte before it, so hand the allocator the whole block instead of the interior pointer.
	//The reconstructed buffer is deliberately plain: it carries no aligned bit, so this doesn't recurse.

	if (Buffer_isAligned(*buf)) {

		const U64 offset = ((const U8*) buf->ptr)[-1];
		const Buffer whole = Buffer_createManagedPtr(buf->ptrNonConst - offset, Buffer_length(*buf) + BUFFER_ALIGN_MAX);

		alloc->free(alloc->ptr, whole);
		*buf = Buffer_createNull();
		return;
	}

	alloc->free(alloc->ptr, *buf);
	*buf = Buffer_createNull();
}

Bool Buffer_createSubset(
	Buffer buf,
	U64 offset,
	U64 length,
	Bool isConst,
	Buffer *output,
	Error *e_rr
) {

	Bool s_uccess = true;

	if (!output)
		retError(clean, Error_nullPointer(4, "Buffer_createSubset()::output is required"));

	if (output && output->ptr)
		retError(clean, Error_invalidOperation(0, "Buffer_createSubset()::output->ptr is non zero, could indicate memleak"));

	if(Buffer_isConstRef(buf) && !isConst)
		retError(clean, Error_constData(0, 0, "Buffer_createSubset() tried creating non const subset from const data"));

	//Since our buffer was passed here, it's safe to make a ref (to ensure Buffer_offset doesn't fail)

	buf = Buffer_createRefFromBuffer(buf, isConst);
	gotoIfError3(clean, Buffer_offset(&buf, offset, e_rr));

	if(length > Buffer_length(buf))
		retError(clean, Error_outOfBounds(2, length, Buffer_length(buf), "Buffer_createSubset()::length out of bounds"));

	Buffer_setLength(&buf, length);
	*output = buf;

clean:
	return s_uccess;
}

Bool Buffer_resize(
	Buffer *buf, U64 newLen, Bool preserveContents, Bool clearUnsetContents, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;
	Buffer tmp = Buffer_createNull();

	if (!buf)
		retError(clean, Error_nullPointer(0, "Buffer_resize()::buf is required"));

	if(Buffer_isRef(*buf) && Buffer_length(*buf))
		retError(clean, Error_invalidState(0, "Buffer_resize()::buf must not be a ref, to avoid memleaks"));

	U64 prevLen = Buffer_length(*buf);

	if(prevLen == newLen)
		goto clean;

	gotoIfError3(clean, Buffer_createUninitializedBytes(newLen, alloc, &tmp, e_rr));

	if(preserveContents) {

		Buffer_memcpy(tmp, *buf);

		if(clearUnsetContents && newLen > prevLen)
			Buffer_unsetAllBits(Buffer_createRef(tmp.ptrNonConst + prevLen, newLen - prevLen), NULL);
	}

	else if(clearUnsetContents)
		Buffer_unsetAllBits(tmp, NULL);

	if(buf->ptr)
		Buffer_free(buf, alloc);

	*buf = tmp;

clean:
	return s_uccess;
}

Bool Buffer_combine(const Buffer *a, const Buffer *b, const Allocator *alloc, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;

	if (!a || !b)
		retError(clean, Error_nullPointer(0, "Buffer_combine()::a and b are required"));

	const U64 alen = Buffer_length(*a), blen = Buffer_length(*b);

	if(alen + blen < alen)
		retError(clean, Error_overflow(0, alen + blen, alen, "Buffer_combine() overflow"));

	gotoIfError3(clean, Buffer_createUninitializedBytes(alen + blen, alloc, output, e_rr));

	Buffer_memcpy(*output, *a);

	//Combining two empty buffers allocates nothing, so the output base is null and offsetting it is undefined
	// even by the zero alen would be here.
	//There is nothing to copy in that case either, so the second copy is simply skipped.

	if(blen)
		Buffer_memcpy(Buffer_createRef(output->ptrNonConst + alen, blen), *b);

clean:
	return s_uccess;
}

//UTF-8
//https://en.wikipedia.org/wiki/UTF-8

Bool Buffer_readAsUTF8(Buffer buf, U64 i, UnicodeCodePointInfo *codepoint, Error *e_rr) {

	Bool s_uccess = true;

	if(!codepoint)
		retError(clean, Error_nullPointer(3, "Buffer_readAsUTF8()::codepoint is required"));

	if(i >= U64_MAX - 4 || i + 1 > Buffer_length(buf))
		retError(clean, Error_outOfBounds(0, i, Buffer_length(buf), "Buffer_readAsUTF8() out of bounds"));

	//Ascii

	const U8 v0 = buf.ptr[i++];

	if (!(v0 >> 7)) {

		if(!C8_isValidAscii((C8)v0))
			retError(clean, Error_invalidParameter(0, 0, "Buffer_readAsUTF8()::buf[i] didn't contain valid ascii"));

		*codepoint = (UnicodeCodePointInfo) { .chars = 1, .bytes = 1, .index = v0 };
		goto clean;
	}

	if(v0 < 0xC0)
		retError(clean, Error_invalidParameter(0, 1, "Buffer_readAsUTF8()::buf[i] didn't contain valid UTF8"));

	//2-4 bytes

	if(i + 1 > Buffer_length(buf))
		retError(clean, Error_outOfBounds(1, i, Buffer_length(buf), "Buffer_readAsUTF8()::buf UTF8 ended unexpectedly"));

	const U8 v1 = buf.ptr[i++];

	if(v1 < 0x80 || v1 >= 0xC0)
		retError(clean, Error_invalidParameter(0, 2, "Buffer_readAsUTF8()::buf[i + 1] had invalid encoding"));

	//2 bytes

	if (v0 < 0xE0) {
		*codepoint = (UnicodeCodePointInfo) { .chars = 2, .bytes = 2, .index = (((U16)v0 & 0x1F) << 6) | (v1 & 0x3F) };
		goto clean;
	}

	//3 bytes

	if(i + 1 > Buffer_length(buf))
		retError(clean, Error_outOfBounds(2, i, Buffer_length(buf), "Buffer_readAsUTF8()::buf UTF8 ended unexpectedly"));

	const U8 v2 = buf.ptr[i++];

	if(v2 < 0x80 || v2 >= 0xC0)
		retError(clean, Error_invalidParameter(0, 3, "Buffer_readAsUTF8()::buf[i + 2] had invalid encoding"));

	if (v0 < 0xF0) {

		*codepoint = (UnicodeCodePointInfo) {
			.chars = 3, .bytes = 3, .index = (((U32)v0 & 0xF) << 12) | (((U32)v1 & 0x3F) << 6) | (v2 & 0x3F)
		};

		goto clean;
	}

	//4 bytes

	if(i + 1 > Buffer_length(buf))
		retError(clean, Error_outOfBounds(3, i, Buffer_length(buf), "Buffer_readAsUTF8()::buf UTF8 ended unexpectedly"));

	const U8 v3 = buf.ptr[i++];

	if(v3 < 0x80 || v3 >= 0xC0)
		retError(clean, Error_invalidParameter(0, 4, "Buffer_readAsUTF8()::buf[i + 3] had invalid encoding"));

	*codepoint = (UnicodeCodePointInfo) {
		.chars = 4,
		.bytes = 4,
		.index = (((U32)v0 & 0x7) << 18) | (((U32)v1 & 0x3F) << 12) | (((U32)v2 & 0x3F) << 6) | (v3 & 0x3F)
	};

clean:
	return s_uccess;
}

Bool Buffer_writeAsUTF8(const Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes, Error *e_rr) {

	Bool s_uccess = true;

	if(Buffer_isConstRef(buf))
		retError(clean, Error_constData(0, 0, "Buffer_writeAsUTF8()::buf should be writable"));

	if ((codepoint & 0x7F) == codepoint) {

		if(i + 1 > Buffer_length(buf))
			retError(clean, Error_outOfBounds(0, i + 1, Buffer_length(buf), "Buffer_writeAsUTF8()::i out of bounds"));

		if(bytes)
			*bytes = 1;

		buf.ptrNonConst[i] = (U8) codepoint;
		goto clean;
	}

	if ((codepoint & 0x7FF) == codepoint) {

		if(i + 2 > Buffer_length(buf))
			retError(clean, Error_outOfBounds(0, i + 2, Buffer_length(buf), "Buffer_writeAsUTF8()::i + 1 out of bounds"));

		if(bytes)
			*bytes = 2;

		buf.ptrNonConst[i]        = 0xC0 | (U8)(codepoint >> 6);
		buf.ptrNonConst[i + 1]    = 0x80 | (U8)(codepoint & 0x3F);
		goto clean;
	}

	if ((codepoint & 0xFFFF) == codepoint) {

		if(i + 3 > Buffer_length(buf))
			retError(clean, Error_outOfBounds(0, i + 3, Buffer_length(buf), "Buffer_writeAsUTF8()::i + 2 out of bounds"));

		if(bytes)
			*bytes = 3;

		buf.ptrNonConst[i]        = 0xE0 | (U8)(codepoint >> 12);
		buf.ptrNonConst[i + 1]    = 0x80 | (U8)((codepoint >> 6) & 0x3F);
		buf.ptrNonConst[i + 2]    = 0x80 | (U8)(codepoint & 0x3F);
		goto clean;
	}

	if (codepoint <= 0x10FFFF) {

		if(i + 4 > Buffer_length(buf))
			retError(clean, Error_outOfBounds(0, i + 4, Buffer_length(buf), "Buffer_writeAsUTF8()::i + 3 out of bounds"));

		if(bytes)
			*bytes = 4;

		buf.ptrNonConst[i]        = 0xF0 | (U8)(codepoint >> 18);
		buf.ptrNonConst[i + 1]    = 0x80 | (U8)((codepoint >> 12) & 0x3F);
		buf.ptrNonConst[i + 2]    = 0x80 | (U8)((codepoint >> 6) & 0x3F);
		buf.ptrNonConst[i + 3]    = 0x80 | (U8)(codepoint & 0x3F);
		goto clean;
	}

	retError(clean, Error_invalidParameter(2, 0, "Buffer_writeAsUTF8()::codepoint out of bounds (>0x10FFFF)"));

clean:
	return s_uccess;
}

//https://en.wikipedia.org/wiki/UTF-16

Bool Buffer_readAsUTF16(const Buffer buf, U64 i, UnicodeCodePointInfo *codepoint, Error *e_rr) {

	Bool s_uccess = true;

	if(!codepoint)
		retError(clean, Error_nullPointer(3, "Buffer_readAsUTF16()::codepoint is required"));

	if(i >= U64_MAX - 2 || i + 2 > Buffer_length(buf))
		retError(clean, Error_outOfBounds(0, i, Buffer_length(buf), "Buffer_readAsUTF16() out of bounds"));

	//1 U16

	const U16 v0 = Buffer_readU16(buf, i, NULL, NULL);
	i += 2;

	if (v0 <= 0xD7FF) {

		if(v0 <= 0x7F && !C8_isValidAscii((C8)v0))
			retError(clean, Error_invalidParameter(0, 0, "Buffer_readAsUTF16()::buf[i] didn't contain valid ascii"));

		*codepoint = (UnicodeCodePointInfo) { .chars = 1, .bytes = 2, .index = v0 };
		goto clean;
	}

	if((v0 - 0xD800) >= (1 << 10))
		retError(clean, Error_invalidParameter(0, 1, "Buffer_readAsUTF16()::buf[i] didn't contain valid UTF16"));

	//2 U16s

	if(i + 2 > Buffer_length(buf))
		retError(clean, Error_outOfBounds(1, i, Buffer_length(buf), "Buffer_readAsUTF16()::buf UTF16 ended unexpectedly"));

	const U16 v1 = Buffer_readU16(buf, i, NULL, NULL);

	if(v1 < 0xDC00 || (v1 - 0xDC00) >= (1 << 10))
		retError(clean, Error_invalidParameter(0, 2, "Buffer_readAsUTF16()::buf[i + 1] had invalid encoding"));

	*codepoint = (UnicodeCodePointInfo) { .chars = 2, .bytes = 4, .index = ((v0 - 0xD800) << 10) | (v1 - 0xDC00) | 0x10000 };
	
clean:
	return s_uccess;
}

Bool Buffer_writeAsUTF16(const Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes, Error *e_rr) {

	Bool s_uccess = true;

	if(Buffer_isConstRef(buf))
		retError(clean, Error_constData(0, 0, "Buffer_writeAsUTF16()::buf should be writable"));

	if (codepoint <= 0xD7FF) {

		if(i + 2 > Buffer_length(buf))
			retError(clean, Error_outOfBounds(0, i + 2, Buffer_length(buf), "Buffer_writeAsUTF16()::i out of bounds"));

		if(bytes)
			*bytes = 2;

		Buffer_writeU16(buf, i, (U16) codepoint, NULL);
		goto clean;
	}

	if (codepoint <= 0x10FFFF) {

		if(i + 4 > Buffer_length(buf))
			retError(clean, Error_outOfBounds(0, i + 4, Buffer_length(buf), "Buffer_writeAsUTF16()::i + 4 out of bounds"));

		if(bytes)
			*bytes = 4;

		Buffer_writeU16(buf, i,     0xD800 | (U16)(codepoint >> 10), NULL);
		Buffer_writeU16(buf, i + 2, 0xDC00 | (U16)(codepoint & 1023), NULL);
		goto clean;
	}

	retError(clean, Error_invalidParameter(2, 0, "Buffer_writeAsUTF16()::codepoint out of bounds (>0x10FFFF)"));

clean:
	return s_uccess;
}

//Detect unicode

Bool Buffer_isUnicode(const Buffer buf, F32 threshold, Bool isUTF16) {

	if (!Buffer_length(buf))
		return true;

	threshold = 1 - threshold;

	U64 i = 0;
	F64 counter = 0;
	const F64 invLen = 1.0 / (F64)Buffer_length(buf);
	const U64 minWidth = isUTF16 ? 2 : 1;

	if(Buffer_length(buf) & (minWidth - 1))
		return false;

	while (i + minWidth <= Buffer_length(buf)) {

		UnicodeCodePointInfo info = (UnicodeCodePointInfo) { 0 };

		if(
			(!isUTF16 && !Buffer_readAsUTF8(buf, i, &info, NULL)) ||
			(isUTF16 && !Buffer_readAsUTF16(buf, i, &info, NULL))
		) {
			counter += invLen;

			if(counter >= threshold)
				return false;

			i += minWidth;
			continue;
		}

		i += info.bytes;
	}

	return !!i;
}
