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

#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/math/math.h"
#include "types/math/vec.h"

#include <string.h>

Bool BitRef_get(BitRef b) { return b.ptr && (*b.ptr >> b.off) & 1; }
void BitRef_set(BitRef b) { if(b.ptr && !b.isConst) *b.ptr |= 1 << b.off; }
void BitRef_reset(BitRef b) { if(b.ptr && !b.isConst) *b.ptr &= ~(1 << b.off); }

void BitRef_setTo(BitRef b, Bool v) {
	if(v) BitRef_set(b);
	else BitRef_reset(b);
}

Error Buffer_createBits(U64 length, Bool value, Allocator alloc, Buffer *result) {
	return !value ? Buffer_createZeroBits(length, alloc, result) : Buffer_createOneBits(length, alloc, result);
}

Error Buffer_allocBitsInternal(U64 length, Allocator alloc, Buffer *result) {

	if(!length)
		return Error_invalidParameter(0, 0, "Buffer_allocBitsInternal()::length should be non zero");

	if(length >> 51)
		return Error_invalidParameter(0, 0, "Buffer_allocBitsInternal()::length buffer length can't exceed 1 << 48 bytes");

	if(!alloc.alloc)
		return Error_nullPointer(1, "Buffer_allocBitsInternal()::alloc should have allocator");

	if(!result)
		return Error_nullPointer(0, "Buffer_allocBitsInternal()::result is required");

	if(result->ptr)
		return Error_invalidParameter(2, 0, "Buffer_allocBitsInternal()::result->ptr is non zero, could indicate memleak");

	length = (length + 7) >> 3;	//Align to bytes
	return alloc.alloc(alloc.ptr, length, result);
}

Error Buffer_createZeroBits(U64 length, Allocator alloc, Buffer *result) {

	Error e = Buffer_allocBitsInternal(length, alloc, result);

	if(e.genericError)
		return e;

	e = Buffer_unsetAllBits(*result);

	if(e.genericError)
		Buffer_free(result, alloc);

	return e;
}

Error Buffer_createOneBits(U64 length, Allocator alloc, Buffer *result) {

	Error e = Buffer_allocBitsInternal(length, alloc, result);

	if(e.genericError)
		return e;

	e = Buffer_setAllBits(*result);

	if(e.genericError)
		Buffer_free(result, alloc);

	return e;
}

Error Buffer_createCopy(Buffer buf, Allocator alloc, Buffer *result) {

	if (result && result->ptr)
		return Error_invalidOperation(0, "Buffer_createCopy()::result is non zero, could indicate memleak");

	if(!Buffer_length(buf)) {

		if(result)
			*result = Buffer_createNull();

		return Error_none();
	}

	const U64 l = Buffer_length(buf);
	const Error e = Buffer_allocBitsInternal(l << 3, alloc, result);

	if(e.genericError)
		return e;

	Buffer_copy(*result, buf);
	return Error_none();
}

Bool Buffer_free(Buffer *buf, Allocator alloc) {

	if(!buf || !Buffer_length(*buf))
		return true;

	//References SHOULD NEVER be freed through the allocator.
	//We aren't the ones managing them

	if (Buffer_isRef(*buf)) {
		*buf = Buffer_createNull();
		return true;
	}

	//Otherwise we should free

	if(!alloc.free)
		return false;

	const Bool success = alloc.free(alloc.ptr, *buf);
	*buf = Buffer_createNull();
	return success;
}

Error Buffer_createEmptyBytes(U64 length, Allocator alloc, Buffer *output) {
	return Buffer_createZeroBits(length >> 61 ? U64_MAX : length << 3, alloc, output);
}

Error Buffer_createUninitializedBytes(U64 length, Allocator alloc, Buffer *result) {
	return Buffer_allocBitsInternal(length >> 61 ? U64_MAX : length << 3, alloc, result);
}

Error Buffer_createSubset(
	Buffer buf,
	U64 offset,
	U64 length,
	Bool isConst,
	Buffer *output
) {

	if (!output)
		return Error_nullPointer(4, "Buffer_createSubset()::output is required");

	if (output && output->ptr)
		return Error_invalidOperation(0, "Buffer_createSubset()::output->ptr is non zero, could indicate memleak");

	if(Buffer_isConstRef(buf) && !isConst)
		return Error_constData(0, 0, "Buffer_createSubset() tried creating non const subset from const data");

	//Since our buffer was passed here, it's safe to make a ref (to ensure Buffer_offset doesn't fail)

	buf = Buffer_createRefFromBuffer(buf, isConst);
	const Error e = Buffer_offset(&buf, offset);

	if(e.genericError)
		return e;

	if(length > Buffer_length(buf))
		return Error_outOfBounds(2, length, Buffer_length(buf), "Buffer_createSubset()::length out of bounds");

	buf.lengthAndRefBits = length | (buf.lengthAndRefBits >> 62 << 62);
	*output = buf;
	return Error_none();
}

Error Buffer_combine(Buffer a, Buffer b, Allocator alloc, Buffer *output) {

	const U64 alen = Buffer_length(a), blen = Buffer_length(b);

	if(alen + blen < alen)
		return Error_overflow(0, alen + blen, alen, "Buffer_combine() overflow");

	const Error err = Buffer_createUninitializedBytes(alen + blen, alloc, output);

	if(err.genericError)
		return err;

	Buffer_copy(*output, a);
	Buffer_copy(Buffer_createRef(output->ptrNonConst + alen, blen), b);
	return Error_none();
}

//UTF-8
//https://en.wikipedia.org/wiki/UTF-8

Error Buffer_readAsUTF8(Buffer buf, U64 i, UnicodeCodePointInfo *codepoint) {

	if(!codepoint)
		return Error_nullPointer(3, "Buffer_readAsUTF8()::codepoint is required");

	if(i >= U64_MAX - 4 || i + 1 > Buffer_length(buf))
		return Error_outOfBounds(0, i, Buffer_length(buf), "Buffer_readAsUTF8() out of bounds");

	//Ascii

	const U8 v0 = buf.ptr[i++];

	if (!(v0 >> 7)) {

		if(!C8_isValidAscii((C8)v0))
			return Error_invalidParameter(0, 0, "Buffer_readAsUTF8()::buf[i] didn't contain valid ascii");

		*codepoint = (UnicodeCodePointInfo) { .chars = 1, .bytes = 1, .index = v0 };
		return Error_none();
	}

	if(v0 < 0xC0)
		return Error_invalidParameter(0, 1, "Buffer_readAsUTF8()::buf[i] didn't contain valid UTF8");

	//2-4 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(1, i, Buffer_length(buf), "Buffer_readAsUTF8()::buf UTF8 ended unexpectedly");

	const U8 v1 = buf.ptr[i++];

	if(v1 < 0x80 || v1 >= 0xC0)
		return Error_invalidParameter(0, 2, "Buffer_readAsUTF8()::buf[i + 1] had invalid encoding");

	//2 bytes

	if (v0 < 0xE0) {
		*codepoint = (UnicodeCodePointInfo) { .chars = 2, .bytes = 2, .index = (((U16)v0 & 0x1F) << 6) | (v1 & 0x3F) };
		return Error_none();
	}

	//3 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(2, i, Buffer_length(buf), "Buffer_readAsUTF8()::buf UTF8 ended unexpectedly");

	const U8 v2 = buf.ptr[i++];

	if(v2 < 0x80 || v2 >= 0xC0)
		return Error_invalidParameter(0, 3, "Buffer_readAsUTF8()::buf[i + 2] had invalid encoding");

	if (v0 < 0xF0) {

		*codepoint = (UnicodeCodePointInfo) {
			.chars = 3, .bytes = 3, .index = (((U32)v0 & 0xF) << 12) | (((U32)v1 & 0x3F) << 6) | (v2 & 0x3F)
		};

		return Error_none();
	}

	//4 bytes

	if(i + 1 > Buffer_length(buf))
		return Error_outOfBounds(3, i, Buffer_length(buf), "Buffer_readAsUTF8()::buf UTF8 ended unexpectedly");

	const U8 v3 = buf.ptr[i++];

	if(v3 < 0x80 || v3 >= 0xC0)
		return Error_invalidParameter(0, 4, "Buffer_readAsUTF8()::buf[i + 3] had invalid encoding");

	*codepoint = (UnicodeCodePointInfo) {
		.chars = 4,
		.bytes = 4,
		.index = (((U32)v0 & 0x7) << 18) | (((U32)v1 & 0x3F) << 12) | (((U32)v2 & 0x3F) << 6) | (v3 & 0x3F)
	};

	return Error_none();
}

Error Buffer_writeAsUTF8(Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes) {

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_writeAsUTF8()::buf should be writable");

	if ((codepoint & 0x7F) == codepoint) {

		if(i + 1 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 1, Buffer_length(buf), "Buffer_writeAsUTF8()::i out of bounds");

		if(bytes)
			*bytes = 1;

		buf.ptrNonConst[i] = (U8) codepoint;
		return Error_none();
	}

	if ((codepoint & 0x7FF) == codepoint) {

		if(i + 2 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 2, Buffer_length(buf), "Buffer_writeAsUTF8()::i + 1 out of bounds");

		if(bytes)
			*bytes = 2;

		buf.ptrNonConst[i]		= 0xC0 | (U8)(codepoint >> 6);
		buf.ptrNonConst[i + 1]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	if ((codepoint & 0xFFFF) == codepoint) {

		if(i + 3 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 3, Buffer_length(buf), "Buffer_writeAsUTF8()::i + 2 out of bounds");

		if(bytes)
			*bytes = 3;

		buf.ptrNonConst[i]		= 0xE0 | (U8)(codepoint >> 12);
		buf.ptrNonConst[i + 1]	= 0x80 | (U8)((codepoint >> 6) & 0x3F);
		buf.ptrNonConst[i + 2]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	if (codepoint <= 0x10FFFF) {

		if(i + 4 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 4, Buffer_length(buf), "Buffer_writeAsUTF8()::i + 3 out of bounds");

		if(bytes)
			*bytes = 4;

		buf.ptrNonConst[i]		= 0xF0 | (U8)(codepoint >> 18);
		buf.ptrNonConst[i + 1]	= 0x80 | (U8)((codepoint >> 12) & 0x3F);
		buf.ptrNonConst[i + 2]	= 0x80 | (U8)((codepoint >> 6) & 0x3F);
		buf.ptrNonConst[i + 3]	= 0x80 | (U8)(codepoint & 0x3F);
		return Error_none();
	}

	return Error_invalidParameter(2, 0, "Buffer_writeAsUTF8()::codepoint out of bounds (>0x10FFFF)");
}

//https://en.wikipedia.org/wiki/UTF-16

Error Buffer_readAsUTF16(Buffer buf, U64 i, UnicodeCodePointInfo *codepoint) {

	if(!codepoint)
		return Error_nullPointer(3, "Buffer_readAsUTF16()::codepoint is required");

	if(i >= U64_MAX - 2 || i + 2 > Buffer_length(buf))
		return Error_outOfBounds(0, i, Buffer_length(buf), "Buffer_readAsUTF16() out of bounds");

	//1 U16

	const U16 v0 = Buffer_readU16(buf, i, NULL);	//Uhoh: FORBIDDEN
	i += 2;

	if (v0 <= 0xD7FF) {

		if(v0 <= 0x7F && !C8_isValidAscii((C8)v0))
			return Error_invalidParameter(0, 0, "Buffer_readAsUTF16()::buf[i] didn't contain valid ascii");

		*codepoint = (UnicodeCodePointInfo) { .chars = 1, .bytes = 2, .index = v0 };
		return Error_none();
	}

	if((v0 - 0xD800) >= (1 << 10))
		return Error_invalidParameter(0, 1, "Buffer_readAsUTF16()::buf[i] didn't contain valid UTF16");

	//2 U16s

	if(i + 2 > Buffer_length(buf))
		return Error_outOfBounds(1, i, Buffer_length(buf), "Buffer_readAsUTF16()::buf UTF16 ended unexpectedly");

	const U16 v1 = Buffer_readU16(buf, i, NULL);

	if(v1 < 0xDC00 || (v1 - 0xDC00) >= (1 << 10))
		return Error_invalidParameter(0, 2, "Buffer_readAsUTF16()::buf[i + 1] had invalid encoding");

	*codepoint = (UnicodeCodePointInfo) { .chars = 2, .bytes = 4, .index = ((v0 - 0xD800) << 10) | (v1 - 0xDC00) | 0x10000 };
	return Error_none();
}

Error Buffer_writeAsUTF16(Buffer buf, U64 i, UnicodeCodePoint codepoint, U8 *bytes) {

	if(Buffer_isConstRef(buf))
		return Error_constData(0, 0, "Buffer_writeAsUTF16()::buf should be writable");

	if (codepoint <= 0xD7FF) {

		if(i + 2 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 2, Buffer_length(buf), "Buffer_writeAsUTF16()::i out of bounds");

		if(bytes)
			*bytes = 2;

		Buffer_writeU16(buf, i, (U16) codepoint);
		return Error_none();
	}

	if (codepoint <= 0x10FFFF) {

		if(i + 4 > Buffer_length(buf))
			return Error_outOfBounds(0, i + 4, Buffer_length(buf), "Buffer_writeAsUTF16()::i + 4 out of bounds");

		if(bytes)
			*bytes = 4;

		Buffer_writeU16(buf, i, 	0xD800 | (U16)(codepoint >> 10));
		Buffer_writeU16(buf, i + 2, 0xDC00 | (U16)(codepoint & 1023));
		return Error_none();
	}

	return Error_invalidParameter(2, 0, "Buffer_writeAsUTF16()::codepoint out of bounds (>0x10FFFF)");
}

//Detect unicode

Bool Buffer_isUnicode(Buffer buf, F32 threshold, Bool isUTF16) {

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
			(!isUTF16 && (Buffer_readAsUTF8(buf, i, &info)).genericError) ||
			(isUTF16 && (Buffer_readAsUTF16(buf, i, &info)).genericError)
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

Bool Buffer_isUTF8(Buffer buf, F32 threshold) { return Buffer_isUnicode(buf, threshold, false); }
Bool Buffer_isUTF16(Buffer buf, F32 threshold) { return Buffer_isUnicode(buf, threshold, true); }

Bool Buffer_isAscii(Buffer buf) {

	for (U64 i = 0; i < Buffer_length(buf); ++i)
		if(!C8_isValidAscii((C8) buf.ptr[i]))
			return false;

	return !!Buffer_length(buf);
}
