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

//formats/hdr/hdr_write.c

#include "formats/hdr/hdr_file.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"
#include "types/container/stream.h"
#include "types/container/ref_ptr.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/mathf.h"
#include "types/math/type_cast.h"

//Adaptive RLE is only legal in this width range; anything else writes flat scanlines,
// which every reader including HDR_read accepts as the fallback path.

#define HDR_RLE_MIN 8
#define HDR_RLE_MAX 32768

//One texel, float triple to RGBE. The BRIGHTEST channel sets the shared exponent, so it lands in [128,
// 256) after scaling and the other two keep whatever precision that leaves them.
//
//Mirrors the decoder's scale of 2^(e - 136) exactly: a byte written as c * 2^(8 - e) reads back as c * 2^(8 - e) * 2^(e -
// 136 + 128), which is c.

static void HDR_encodeTexel(F32 r, F32 g, F32 b, U8 *out) {

	//NaN has no encoding here, and is undefined both to clamp and to cast, so it goes before either sees it.
	//An infinity needs no such handling: it clamps to the format's maximum like any other overflow.

	if(F32_isNaN(r)) r = 0;
	if(F32_isNaN(g)) g = 0;
	if(F32_isNaN(b)) b = 0;

	const F32 v = F32_max(r, F32_max(g, b));

	if(v <= 1e-32f) {
		out[0] = out[1] = out[2] = out[3] = 0;
		return;
	}

	//frexp's exponent, straight off the bits: v is mantissa * 2^e with the mantissa in [0.5, 1),
	// so a bias of 126 rather than 127.

	I32 e = (I32)((U32_fromF32Bits(v) >> 23) & 0xFF) - 126;

	//e + 128 has to fit a byte. Real radiance never reaches either end, but a stray infinity would.

	if(e > 127)
		e = 127;

	//2^(8 - e) built straight out of the exponent field rather than called for: exp2 is powf underneath,
	// so a table existed only to keep a library call off the per-texel path, and a power of two needs neither.
	//The field cannot leave its range here: e is clamped to 127 above, and the 1e-32 floor keeps it over -106,
	// so 135 - e stays inside 8 to 241.

	const F32 scale = F32_fromU32Bits((U32)(135 - e) << 23);

	//Clamped in the FLOAT domain, before the cast: converting an out-of-range float to an integer is undefined,
	// and the brightest channel is only guaranteed to land inside the range by construction.

	out[0] = (U8) F32_clamp(r * scale, 0, 255);
	out[1] = (U8) F32_clamp(g * scale, 0, 255);
	out[2] = (U8) F32_clamp(b * scale, 0, 255);
	out[3] = (U8)(e + 128);
}

//One channel plane of one scanline. A run repeats one byte and is worth it from four up;
// anything shorter goes out as literals, since a two byte run costs the same as writing it twice.

static U64 HDR_writePlane(const U8 *src, U32 w, U64 stride, U8 *dst) {

	U64 at = 0;
	U32 x = 0;

	while(x < w) {

		U32 run = 1;

		while(x + run < w && run < 127 && src[(U64)(x + run) * stride] == src[(U64) x * stride])
			++run;

		if(run >= 4) {
			dst[at++] = (U8)(128 + run);
			dst[at++] = src[(U64) x * stride];
			x += run;
			continue;
		}

		//Literals up to 128, stopping early where a run worth encoding begins.

		U32 lit = 0;

		while(x + lit < w && lit < 128) {

			U32 ahead = 1;

			while(x + lit + ahead < w && ahead < 4 && src[(U64)(x + lit + ahead) * stride] == src[(U64)(x + lit) * stride])
				++ahead;

			if(ahead >= 4)
				break;

			++lit;
		}

		dst[at++] = (U8) lit;

		for(U32 i = 0; i < lit; ++i)
			dst[at++] = src[(U64)(x + i) * stride];

		x += lit;
	}

	return at;
}

//Streams both ways, like BMP_write: one scanline of source in, one encoded scanline out,
// so the cost is O(width) rather than O(width * height). A 4k render is 64 KiB of scratch here instead of 134 MiB.

Bool HDR_write(
	StreamRef *streamRef,
	U64 *off,
	EHDRWriteFlags flags,
	U32 w,
	U32 h,
	const Allocator *alloc,
	StreamRef *inputStreamRef,
	U64 inputOffset,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListF32 rowF32 = (ListF32) { 0 };
	ListU8 rgbe = (ListU8) { 0 };
	ListU8 enc = (ListU8) { 0 };
	CharString header = CharString_createNull();

	if(!streamRef || !inputStreamRef || !off)
		retError(clean, Error_nullPointer(0, "HDR_write() requires both streams and off"));

	if(!w || !h || w > HDR_MAX_DIM || h > HDR_MAX_DIM)
		retError(clean, Error_invalidParameter(2, 0, "HDR_write() resolution out of range"));

	OxStream *stream = RefPtr_data(streamRef, OxStream);
	OxStream *inputStream = RefPtr_data(inputStreamRef, OxStream);

	//Already encoded, a source row is the RGBE plane itself and goes straight to the RLE.

	const Bool srcIsRGBE = (flags & EHDRWriteFlags_SourceIsRGBE) != 0;
	const U64 rowBytes = (U64) w * 4 * (srcIsRGBE ? 1 : sizeof(F32));

	if(inputOffset + rowBytes * h > inputStream->size)
		retError(clean, Error_outOfBounds(
			6, inputOffset + rowBytes * h, inputStream->size, "HDR_write()::inputOffset out of bounds"
		));

	//"#?RADIANCE" is what every reader sniffs for, and -Y +X fixes row 0 at the TOP,
	// matching both the input laid out here and what HDR_read hands back.

	gotoIfError3(clean, CharString_format(
		alloc, &header, e_rr, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %u +X %u\n", h, w
	));

	const U64 headerLen = CharString_length(header);

	const Bool adaptive = w >= HDR_RLE_MIN && w < HDR_RLE_MAX;

	//Worst case per scanline is every plane all literals, one count byte per 128 plus the four byte marker.
	//Reserved rather than trimmed afterwards: the overshoot is under 1% and a resizable stream would otherwise grow once per
	// scanline.

	const U64 perLine = adaptive ? 4 + 4 * ((U64) w + (w + 127) / 128) : (U64) w * 4;

	if(stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, *off + headerLen + perLine * h, alloc, e_rr));

	gotoIfError3(clean, stream->write(
		stream, *off, headerLen, Buffer_createRefConst(header.ptr, headerLen), alloc, e_rr
	));

	*off += headerLen;

	if(!srcIsRGBE)
		gotoIfError3(clean, ListF32_create((U64) w * 4, alloc, &rowF32, e_rr));

	gotoIfError3(clean, ListU8_create((U64) w * 4, alloc, &rgbe, e_rr));
	gotoIfError3(clean, ListU8_create(perLine, alloc, &enc, e_rr));

	F32 *row = rowF32.ptrNonConst;
	U8 *line = rgbe.ptrNonConst;
	U8 *dst = enc.ptrNonConst;

	for(U32 y = 0; y < h; ++y) {

		gotoIfError3(clean, inputStream->read(
			inputStream, inputOffset + (U64) y * rowBytes, rowBytes,
			Buffer_createRef(srcIsRGBE ? (void*) line : (void*) row, rowBytes), alloc, e_rr
		));

		if(!srcIsRGBE)
			for(U32 x = 0; x < w; ++x)
				HDR_encodeTexel(row[x * 4], row[x * 4 + 1], row[x * 4 + 2], line + (U64) x * 4);

		U64 at = 0;

		if(adaptive) {

			//The adaptive marker: 2, 2, then the width big endian. A reader tells the two layouts apart by exactly this,
			// which is why a width that could collide takes the flat path instead.

			dst[at++] = 2;
			dst[at++] = 2;
			dst[at++] = (U8)(w >> 8);
			dst[at++] = (U8)(w & 0xFF);

			for(U32 ch = 0; ch < 4; ++ch)
				at += HDR_writePlane(line + ch, w, 4, dst + at);
		}

		else for(U64 i = 0; i < (U64) w * 4; ++i)
			dst[at++] = line[i];

		gotoIfError3(clean, stream->write(
			stream, *off, at, Buffer_createRefConst(dst, at), alloc, e_rr
		));

		*off += at;
	}

clean:

	CharString_free(&header, alloc);
	ListU8_free(&enc, alloc);
	ListU8_free(&rgbe, alloc);
	ListF32_free(&rowF32, alloc);
	return s_uccess;
}
