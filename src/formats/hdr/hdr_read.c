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

//formats/hdr/hdr_read.c

#include "formats/hdr/hdr_file.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/container/stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/container/list_basic_types.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/base/string_read.h"
#include "types/container/string.h"
#include "types/base/mathf.h"
#include "types/math/type_cast.h"

//A scanline is at most 4 channels of width bytes once expanded, and the encoded form of a channel can exceed that only if
// the file lies about its own runs, which the decoder refuses rather than grows for.

//The reader pulls the file through a small window rather than mapping it whole:
//a 4k lat-long is 25 MiB encoded and the decoded result is already 128 MiB,
// so holding a second copy of the source costs more than the streaming does.

#define HDR_CHUNK 65536

//RGBE's scale is 2^(ex - 136), built straight out of the bit pattern rather than called for: F32_exp2 is
// powf underneath, and a power of two needs neither a library call nor a table to look it up in.
//An exponent at or above -126 is normal and sets the exponent field. The nine below it are denormal and set
// a mantissa bit instead, since a denormal is mantissa * 2^-149. Exponent byte 0 is the format's exact zero
// rather than either of those.

static F32 HDR_scale(U8 ex) {

	if(!ex)
		return 0;

	const I32 n = (I32) ex - (128 + 8);

	return n >= -126 ? F32_fromU32Bits((U32)(n + 127) << 23) : F32_fromU32Bits(1u << (U32)(n + 149));
}

typedef struct HDRCursor {

	OxStream *stream;
	const Allocator *alloc;

	U64 base;                          //Stream offset the window starts at
	U64 fill;                          //Valid bytes in buf
	U64 pos;                           //Read cursor within buf

	U8 buf[HDR_CHUNK];

} HDRCursor;

//Refills from the stream when the window runs dry. Returns false at end of stream,
// which every caller treats as a truncated file rather than as a normal end: the header states how many samples exist.

static Bool HDRCursor_next(HDRCursor *c, U8 *result, Error *e_rr) {

	Bool s_uccess = true;

	if (c->pos >= c->fill) {

		if(c->base + c->fill >= c->stream->size)
			retError(clean, Error_outOfBounds(0, c->base + c->fill, c->stream->size, "HDR_read() ran past the end"));

		c->base += c->fill;
		c->pos = 0;

		U64 remaining = c->stream->size - c->base;
		c->fill = remaining < HDR_CHUNK ? remaining : HDR_CHUNK;

		gotoIfError3(clean, c->stream->read(
			c->stream, c->base, c->fill, Buffer_createRef(c->buf, c->fill), c->alloc, e_rr
		));
	}

	*result = c->buf[c->pos++];

clean:
	return s_uccess;
}

//Reads one header line, capped so a file with no newline can't spin. Radiance headers are short;
// the cap is generous rather than tuned.

static Bool HDR_readLine(HDRCursor *c, C8 *line, U64 lineCap, U64 *length, Error *e_rr) {

	Bool s_uccess = true;
	U64 n = 0;

	for (;;) {

		U8 ch = 0;
		gotoIfError3(clean, HDRCursor_next(c, &ch, e_rr));

		if(ch == '\n')
			break;

		if(n + 1 >= lineCap)
			retError(clean, Error_outOfBounds(0, n, lineCap, "HDR_read() header line too long"));

		line[n++] = (C8) ch;
	}

	line[n] = '\0';
	*length = n;

clean:
	return s_uccess;
}

static Bool HDR_startsWith(const C8 *line, const C8 *prefix) {

	while (*prefix) {

		if(*line != *prefix)
			return false;

		++line; ++prefix;
	}

	return true;
}

//Decimal parse of a header field, which is where a hostile file gets to pick an allocation size,
// so it refuses anything that isn't digits and clamps before the value can be multiplied out.

static Bool HDR_parseDim(const C8 **it, U32 *result, Error *e_rr) {

	Bool s_uccess = true;
	const C8 *p = *it;
	U64 v = 0;
	U64 digits = 0;

	while(*p == ' ')
		++p;

	while (*p >= '0' && *p <= '9') {

		v = v * 10 + (U64)(*p - '0');
		++digits; ++p;

		if(v > HDR_MAX_DIM)
			retError(clean, Error_outOfBounds(0, v, HDR_MAX_DIM, "HDR_read() dimension out of range"));
	}

	if(!digits || !v)
		retError(clean, Error_invalidParameter(0, 0, "HDR_read() resolution line malformed"));

	*result = (U32) v;
	*it = p;

clean:
	return s_uccess;
}

//The header, and nothing after it. Both entry points share this so a consumer that only wants w, h and
// where the pixels begin parses exactly what a decoding one does.

static Bool HDR_parseHeader(HDRCursor *cursor, U32 *wOut, U32 *hOut, F32 *exposureOut, Error *e_rr) {

	Bool s_uccess = true;
	U32 w = 0, h = 0;

		//---- header

		C8 line[256];
		U64 lineLen = 0;

		gotoIfError3(clean, HDR_readLine(cursor, line, sizeof(line), &lineLen, e_rr));

		if(!HDR_startsWith(line, "#?"))
			retError(clean, Error_invalidParameter(0, 0, "HDR_readHeader()::stream isn't a Radiance file"));

		Bool sawFormat = false;
		F32 exposure = 1;

		for (;;) {

			gotoIfError3(clean, HDR_readLine(cursor, line, sizeof(line), &lineLen, e_rr));

			if(!lineLen)                   //Blank line closes the header, the resolution line comes next
				break;

			if (HDR_startsWith(line, "FORMAT=")) {

				//RGBE only. XYZE decodes with the identical RLE and exponent maths but is a different colour space,
				// so accepting it here would hand back numbers that silently mean something else.

				if(!HDR_startsWith(line + 7, "32-bit_rle_rgbe"))
					retError(clean, Error_unsupportedOperation(
						0, "HDR_readHeader() only supports FORMAT=32-bit_rle_rgbe"
					));

				sawFormat = true;
			}

			else if (HDR_startsWith(line, "EXPOSURE=")) {

				//Radiance defines repeated EXPOSURE lines as cumulative, so they multiply.

				F64 parsed = 0;
				CharString value = CharString_createRefCStrConst(line + 9);

				if(CharString_parseDouble(value, &parsed) && parsed > 0)
					exposure *= (F32) parsed;
			}
		}

		if(!sawFormat)
			retError(clean, Error_invalidParameter(0, 0, "HDR_readHeader()::stream had no FORMAT line"));

		//---- resolution

		gotoIfError3(clean, HDR_readLine(cursor, line, sizeof(line), &lineLen, e_rr));

		//Only the standard orientation is accepted. The other seven exist in the spec and essentially never on disk,
		// and each one silently transposes or mirrors the image, so a wrong guess is invisible until something downstream is
		// upside down.

		if(!HDR_startsWith(line, "-Y "))
			retError(clean, Error_unsupportedOperation(0, "HDR_readHeader() only supports the -Y +X orientation"));

		const C8 *it = line + 3;

		gotoIfError3(clean, HDR_parseDim(&it, &h, e_rr));

		while(*it == ' ')
			++it;

		if(!HDR_startsWith(it, "+X "))
			retError(clean, Error_unsupportedOperation(0, "HDR_readHeader() only supports the -Y +X orientation"));

		it += 3;
		gotoIfError3(clean, HDR_parseDim(&it, &w, e_rr));

	*wOut = w;
	*hOut = h;
	*exposureOut = exposure;

clean:
	return s_uccess;
}

//Parses only the header, leaving *off at the first byte of the first scanline.
//
//That is what a consumer streaming the pixel data somewhere else needs: it learns the dimensions and where the data
// starts without decoding any of it, which is the shape a DirectStorage style path wants, where the file goes to the
// GPU and the unpacking happens there.
//
//Note that the scanlines are variable length, so knowing where the data starts is not the same as knowing where any
// PARTICULAR scanline starts. Decoding them in parallel needs an offset table built by one serial pass over the runs,
// which is far cheaper than a full decode since it reads the counts without writing any pixels.

Bool HDR_readHeader(
	StreamRef *streamRef,
	U64 *off,
	EHDRReadFlags flags,
	HDRInfo *info,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	HDRCursor *cursor = NULL;
	Buffer cursorBuf = Buffer_createNull();
	U32 w = 0, h = 0;
	F32 exposure = 1;

	if(!streamRef || !off || !info)
		retError(clean, Error_nullPointer(!streamRef ? 0 : (!off ? 1 : 3), "HDR_readHeader() requires stream, off and info"));

	if(streamRef->refPtrType->typeId != (TypeId) EContainerTypeId_Stream)
		retError(clean, Error_invalidParameter(0, 0, "HDR_readHeader()::stream needs to be the right type"));

	OxStream *stream = RefPtr_data(streamRef, OxStream);

	if(!stream->read)
		retError(clean, Error_invalidParameter(0, 0, "HDR_readHeader()::stream needs to be readable"));

	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(HDRCursor), alloc, &cursorBuf, e_rr));
	cursor = (HDRCursor*) cursorBuf.ptr;

	cursor->stream = stream;
	cursor->alloc = alloc;
	cursor->base = *off;
	cursor->fill = 0;
	cursor->pos = 0;

	gotoIfError3(clean, HDR_parseHeader(cursor, &w, &h, &exposure, e_rr));

	info->w = w;
	info->h = h;
	info->exposure = exposure;
	info->textureFormatId = (flags & EHDRReadFlags_KeepRGBE) ? ETextureFormatId_RGBA8u : ETextureFormatId_RGBA32f;

	*off = cursor->base + cursor->pos;

clean:
	Buffer_free(&cursorBuf, alloc);
	return s_uccess;
}

Bool HDR_read(
	StreamRef *streamRef,
	U64 *off,
	EHDRReadFlags flags,
	HDRInfo *info,
	StreamRef *outputStreamRef,
	U64 outputOffset,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	HDRCursor *cursor = NULL;
	Buffer cursorBuf = Buffer_createNull();
	ListU8 rgbe = (ListU8) { 0 };
	ListF32 rowF32 = (ListF32) { 0 };
	U32 w = 0, h = 0;
	F32 exposure = 1;

	if(!streamRef || !off || !info || !outputStreamRef)
		retError(clean, Error_nullPointer(
			(!streamRef ? 0 : (!off ? 1 : (!info ? 2 : 4))),
			"HDR_read()::stream, off, info and outputStream are required"
		));

	if(streamRef->refPtrType->typeId != (TypeId) EContainerTypeId_Stream)
		retError(clean, Error_invalidParameter(0, 0, "HDR_read()::stream needs to be the right type"));

	if(outputStreamRef->refPtrType->typeId != (TypeId) EContainerTypeId_Stream)
		retError(clean, Error_invalidParameter(4, 0, "HDR_read()::outputStream needs to be the right type"));

	OxStream *stream = RefPtr_data(streamRef, OxStream);
	OxStream *outputStream = RefPtr_data(outputStreamRef, OxStream);

	if(!outputStream->write)
		retError(clean, Error_invalidParameter(4, 0, "HDR_read()::outputStream needs to be writable"));

	if(!stream->read)
		retError(clean, Error_invalidParameter(0, 0, "HDR_read()::stream needs to be readable"));

	//The window is a KiB scale buffer, which is more than a stack frame should carry.

	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(HDRCursor), alloc, &cursorBuf, e_rr));
	cursor = (HDRCursor*) cursorBuf.ptr;

	cursor->stream = stream;
	cursor->alloc = alloc;
	cursor->base = *off;
	cursor->fill = 0;
	cursor->pos = 0;

	gotoIfError3(clean, HDR_parseHeader(cursor, &w, &h, &exposure, e_rr));

	//---- pixels

	const Bool keepRGBE = (flags & EHDRReadFlags_KeepRGBE) != 0;
	const U64 rowBytes = (U64) w * 4 * (keepRGBE ? 1 : sizeof(F32));

	//The output size is known exactly here, unlike the encoder's, so a resizable sink grows once rather than once a scanline
	// and ends up holding no slack.

	if(outputStream->reserve)
		gotoIfError3(clean, outputStream->reserve(outputStream, outputOffset + rowBytes * h, alloc, e_rr));

	//One scanline of RGBE, decoded channel-plane at a time by the adaptive path and interleaved by the flat one,
	// so both feed the same widening step below. Kept undecoded, this IS the output row.

	gotoIfError3(clean, ListU8_create((U64) w * 4, alloc, &rgbe, e_rr));

	if(!keepRGBE)
		gotoIfError3(clean, ListF32_create((U64) w * 4, alloc, &rowF32, e_rr));

	U8 *raw = rgbe.ptrNonConst;

	for (U32 y = 0; y < h; ++y) {

		U8 r = 0, g = 0, b = 0, e = 0;

		gotoIfError3(clean, HDRCursor_next(cursor, &r, e_rr));
		gotoIfError3(clean, HDRCursor_next(cursor, &g, e_rr));
		gotoIfError3(clean, HDRCursor_next(cursor, &b, e_rr));
		gotoIfError3(clean, HDRCursor_next(cursor, &e, e_rr));

		const Bool adaptive = r == 2 && g == 2 && !(b & 0x80) && (((U32) b << 8) | e) == w && w >= 8 && w < 32768;

		if (adaptive) {

			//New style: four separate RLE'd planes, each covering the full width. A run either repeats one byte (count > 128) or
			// copies count literals.

			for (U32 ch = 0; ch < 4; ++ch) {

				U32 x = 0;

				while (x < w) {

					U8 count = 0;
					gotoIfError3(clean, HDRCursor_next(cursor, &count, e_rr));

					if(!count)
						retError(clean, Error_invalidParameter(0, 0, "HDR_read() zero length run"));

					if (count > 128) {

						const U32 run = (U32) count - 128;

						if(x + run > w)
							retError(clean, Error_outOfBounds(0, x + run, w, "HDR_read() run past scanline"));

						U8 value = 0;
						gotoIfError3(clean, HDRCursor_next(cursor, &value, e_rr));

						for(U32 i = 0; i < run; ++i)
							raw[(U64)(x + i) * 4 + ch] = value;

						x += run;
					}

					else {

						if(x + count > w)
							retError(clean, Error_outOfBounds(0, x + count, w, "HDR_read() literal past scanline"));

						for (U32 i = 0; i < count; ++i) {

							U8 value = 0;
							gotoIfError3(clean, HDRCursor_next(cursor, &value, e_rr));
							raw[(U64)(x + i) * 4 + ch] = value;
						}

						x += count;
					}
				}
			}
		}

		else {

			//Old style: interleaved RGBE, where an (1,1,1,n) triple repeats the previous texel. The four bytes already consumed are
			// this scanline's first texel.

			raw[0] = r; raw[1] = g; raw[2] = b; raw[3] = e;

			U32 x = 1;
			U32 shift = 0;

			while (x < w) {

				U8 v[4];

				for(U32 i = 0; i < 4; ++i)
					gotoIfError3(clean, HDRCursor_next(cursor, &v[i], e_rr));

				if (v[0] == 1 && v[1] == 1 && v[2] == 1) {

					if(!x)
						retError(clean, Error_invalidParameter(0, 0, "HDR_read() repeat with no previous texel"));

					const U32 run = (U32) v[3] << shift;

					//A zero length repeat is refused for the same reason the adaptive path refuses a zero count, and
					// it matters more here: this one does not advance x, so a file of them would spin while shift
					// climbed 8 at a time until the shift itself went past 32 and became undefined.
					//With every repeat advancing x by at least one, shift cannot pass 16 before the bounds test
					// below fires, since a run at shift 16 already covers the widest scanline the format allows.

					if(!run)
						retError(clean, Error_invalidParameter(0, 0, "HDR_read() zero length repeat"));

					if(x + run > w)
						retError(clean, Error_outOfBounds(0, x + run, w, "HDR_read() repeat past scanline"));

					for (U32 i = 0; i < run; ++i) {
						raw[(U64)(x + i) * 4 + 0] = raw[(U64)(x - 1) * 4 + 0];
						raw[(U64)(x + i) * 4 + 1] = raw[(U64)(x - 1) * 4 + 1];
						raw[(U64)(x + i) * 4 + 2] = raw[(U64)(x - 1) * 4 + 2];
						raw[(U64)(x + i) * 4 + 3] = raw[(U64)(x - 1) * 4 + 3];
					}

					x += run;
					shift += 8;        //Consecutive repeats extend the count rather than restarting it
				}

				else {
					raw[(U64)x * 4 + 0] = v[0];
					raw[(U64)x * 4 + 1] = v[1];
					raw[(U64)x * 4 + 2] = v[2];
					raw[(U64)x * 4 + 3] = v[3];
					++x;
					shift = 0;
				}
			}
		}

		if(keepRGBE) {

			gotoIfError3(clean, outputStream->write(
				outputStream, outputOffset + (U64) y * rowBytes, rowBytes,
				Buffer_createRefConst(raw, rowBytes), alloc, e_rr
			));

			continue;
		}

		//RGBE to float, Radiance's own convention: the mantissa is biased by half a step because the encoder truncated rather
		// than rounded, and exponent 0 is the exact zero rather than a denormal.

		F32 *row = rowF32.ptrNonConst;

		//Exponent 0 needs no branch of its own: its scale is zero, so the multiply below produces the exact
		// zero the format means by it.

		for (U32 x = 0; x < w; ++x) {

			const F32 scale = HDR_scale(raw[(U64)x * 4 + 3]);

			row[(U64)x * 4 + 0] = ((F32) raw[(U64)x * 4 + 0] + 0.5f) * scale;
			row[(U64)x * 4 + 1] = ((F32) raw[(U64)x * 4 + 1] + 0.5f) * scale;
			row[(U64)x * 4 + 2] = ((F32) raw[(U64)x * 4 + 2] + 0.5f) * scale;
			row[(U64)x * 4 + 3] = 1;
		}

		gotoIfError3(clean, outputStream->write(
			outputStream, outputOffset + (U64) y * rowBytes, rowBytes,
			Buffer_createRefConst(row, rowBytes), alloc, e_rr
		));
	}

	info->w = w;
	info->h = h;
	info->textureFormatId = keepRGBE ? ETextureFormatId_RGBA8u : ETextureFormatId_RGBA32f;
	info->exposure = exposure;

	*off = cursor->base + cursor->pos;

clean:
	ListF32_free(&rowF32, alloc);
	ListU8_free(&rgbe, alloc);
	Buffer_free(&cursorBuf, alloc);
	return s_uccess;
}
