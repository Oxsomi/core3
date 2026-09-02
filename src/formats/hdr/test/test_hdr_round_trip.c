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

//formats/hdr/test/test_hdr_round_trip.c

#include "test_hdr_shared.h"
#include "formats/hdr/hdr_file.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/container/texture_format.h"
#include "types/base/mathf.h"

//A writable, resizable sink for the encoder. HDR_write reserves against it once the size is known,
// so it never grows per scanline.

static Bool makeSink(Test *t, StreamRef **out, const RefPtrType *type) {
	return MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, (MemoryStreamRef**) out, &t->err);
}

//Source radiance the tests encode. Deliberately spans several exponents rather than one,
// since RGBE's precision is relative and a flat image would exercise a single scale.

static Bool makeFloatStream(Test *t, U32 w, U32 h, StreamRef **out, const RefPtrType *type) {

	const U64 total = (U64) w * h * 4 * sizeof(F32);

	if(!MemoryStream_create(total, EMemoryStreamFlags_IsWritable, type, (MemoryStreamRef**) out, &t->err))
		return false;

	OxStream *s = RefPtr_data(*out, OxStream);

	for(U32 y = 0; y < h; ++y)
		for(U32 x = 0; x < w; ++x) {

			const F32 px[4] = {
				(F32)(x + 1) * 0.125f,
				(F32)(y + 1) * 4.0f,
				(F32)(x + y + 1) * 64.0f,
				1
			};

			if(!s->write(
				s, ((U64) y * w + x) * 4 * sizeof(F32), 0,
				Buffer_createRefConst(px, sizeof(px)), t->alloc, &t->err
			))
				return false;
		}

	return true;
}

//The decoder streams rows out, so every read here goes through a resizable sink and moves the buffer out of it afterwards.
//That is the same shape oxc::hdr::read has, so the tests exercise the real path.

static Bool readAll(
	Test *t, StreamRef *src, U64 *off, EHDRReadFlags flags, HDRInfo *info, Buffer *result,
	const RefPtrType *type
) {
	MemoryStreamRef *sink = NULL;
	Bool ok = false;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &sink, &t->err))
		goto clean;

	if(!HDR_read(src, off, flags, info, (StreamRef*) sink, 0, t->alloc, &t->err))
		goto clean;

	ok = MemoryStream_move(&sink, result, &t->err);

clean:
	RefPtr_dec((RefPtr**) &sink);
	return ok;
}

//Encode then decode, handing back whatever the reader produced.

static Bool roundTrip(
	Test *t, StreamRef *src, U32 w, U32 h, EHDRWriteFlags wf, EHDRReadFlags rf,
	HDRInfo *info, Buffer *result, U64 *encodedLen, const RefPtrType *type
) {
	StreamRef *sink = NULL;
	Bool ok = false;
	U64 off = 0;

	if(!makeSink(t, &sink, type))
		goto clean;

	if(!HDR_write(sink, &off, wf, w, h, t->alloc, src, 0, &t->err))
		goto clean;

	*encodedLen = off;
	off = 0;
	ok = readAll(t, sink, &off, rf, info, result, type);

clean:
	RefPtr_dec(&sink);
	return ok;
}

//RGBE carries an 8-bit mantissa over a shared exponent, so the brightest channel is good to about 0.4% and a channel far
// below it proportionally worse. This is the format's floor, not the encoder's error.

static Bool closeEnough(F32 got, F32 want, F32 brightest) {
	return F32_abs(got - want) <= brightest * (1.0f / 128);
}

void Test_HDRRoundTripBasic(Test *t) {

	Test_setModule(t, "HDR round-trip: 16x8 RGBA32f");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL;
	Buffer result = Buffer_createNull();
	HDRInfo info = { 0 };
	U64 encoded = 0;

	if(!makeFloatStream(t, 16, 8, &src, &type)) {
		Test_assert(t, "make source", false);
		goto clean;
	}

	Test_assert(t, "round-trip", roundTrip(
		t, src, 16, 8, EHDRWriteFlags_None, EHDRReadFlags_None, &info, &result, &encoded, &type
	));

	Test_assert(t, "w", info.w == 16);
	Test_assert(t, "h", info.h == 8);
	Test_assert(t, "format", info.textureFormatId == ETextureFormatId_RGBA32f);
	Test_assert(t, "size", Buffer_length(result) == (U64) 16 * 8 * 4 * sizeof(F32));

	{
		const F32 *px = (const F32*) result.ptr;
		Bool within = true;

		for(U32 y = 0; y < 8 && within; ++y)
			for(U32 x = 0; x < 16 && within; ++x) {

				const F32 want[3] = { (F32)(x + 1) * 0.125f, (F32)(y + 1) * 4.0f, (F32)(x + y + 1) * 64.0f };
				const F32 brightest = F32_max(want[0], F32_max(want[1], want[2]));
				const U64 at = ((U64) y * 16 + x) * 4;

				for(int c = 0; c < 3; ++c)
					if(!closeEnough(px[at + c], want[c], brightest))
						within = false;
			}

		Test_assert(t, "values within RGBE quantization", within);
	}

clean:
	Buffer_free(&result, t->alloc);
	RefPtr_dec(&src);
}

//The plane read undecoded must decode BY HAND to exactly what the reader's own float path produced.
//Anything else means the two paths disagree, which is the bug KeepRGBE could plausibly introduce.

void Test_HDRRoundTripKeepRGBEMatchesDecoded(Test *t) {

	Test_setModule(t, "HDR KeepRGBE decodes to the float path exactly");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL, *sink = NULL;
	Buffer asFloat = Buffer_createNull(), asPlane = Buffer_createNull();
	HDRInfo fi = { 0 }, pi = { 0 };
	U64 off = 0;

	if(!makeFloatStream(t, 16, 8, &src, &type) || !makeSink(t, &sink, &type)) {
		Test_assert(t, "make streams", false);
		goto clean;
	}

	if(!HDR_write(sink, &off, EHDRWriteFlags_None, 16, 8, t->alloc, src, 0, &t->err)) {
		Test_assert(t, "write", false);
		goto clean;
	}

	off = 0;
	Test_assert(t, "read decoded", readAll(t, sink, &off, EHDRReadFlags_None, &fi, &asFloat, &type));

	off = 0;
	Test_assert(t, "read plane", readAll(t, sink, &off, EHDRReadFlags_KeepRGBE, &pi, &asPlane, &type));

	Test_assert(t, "plane format", pi.textureFormatId == ETextureFormatId_RGBA8u);
	Test_assert(t, "plane is a quarter the size", Buffer_length(asPlane) * sizeof(F32) == Buffer_length(asFloat));

	{
		const F32 *dec = (const F32*) asFloat.ptr;
		const U8 *raw = (const U8*) asPlane.ptr;
		Bool same = true;

		for(U64 i = 0; i < (U64) 16 * 8 && same; ++i) {

			const U8 e = raw[i * 4 + 3];
			const F32 scale = e ? F32_exp2((F32)((I32) e - (128 + 8))) : 0;

			for(int c = 0; c < 3; ++c)
				if(((F32) raw[i * 4 + c] + 0.5f) * scale != dec[i * 4 + c])
					same = false;
		}

		Test_assert(t, "hand-decoded plane equals the float path", same);
	}

clean:
	Buffer_free(&asPlane, t->alloc);
	Buffer_free(&asFloat, t->alloc);
	RefPtr_dec(&sink);
	RefPtr_dec(&src);
}

//Encoding from a plane and reading one back must be a pure passthrough: nothing re-encodes what was already encoded,
// so the bytes have to survive untouched.

void Test_HDRRoundTripSourceIsRGBE(Test *t) {

	Test_setModule(t, "HDR SourceIsRGBE passes bytes through");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL, *sink = NULL, *planeSrc = NULL;
	Buffer plane = Buffer_createNull(), again = Buffer_createNull();
	HDRInfo i0 = { 0 }, i1 = { 0 };
	U64 off = 0;

	if(!makeFloatStream(t, 16, 8, &src, &type) || !makeSink(t, &sink, &type)) {
		Test_assert(t, "make streams", false);
		goto clean;
	}

	if(!HDR_write(sink, &off, EHDRWriteFlags_None, 16, 8, t->alloc, src, 0, &t->err)) {
		Test_assert(t, "write floats", false);
		goto clean;
	}

	off = 0;

	if(!readAll(t, sink, &off, EHDRReadFlags_KeepRGBE, &i0, &plane, &type)) {
		Test_assert(t, "read plane", false);
		goto clean;
	}

	//Re-encode straight from that plane, then read it back the same way.

	if(!MemoryStream_createFromBufferRegion(
		Buffer_createRefFromBuffer(plane, true), 0, Buffer_length(plane),
		EMemoryStreamFlags_None, &type, (MemoryStreamRef**) &planeSrc, &t->err
	)) {
		Test_assert(t, "plane stream", false);
		goto clean;
	}

	{
		StreamRef *sink2 = NULL;
		U64 off2 = 0;

		if(!makeSink(t, &sink2, &type)) {
			Test_assert(t, "second sink", false);
			goto clean;
		}

		Test_assert(t, "write plane", HDR_write(
			sink2, &off2, EHDRWriteFlags_SourceIsRGBE, 16, 8, t->alloc, planeSrc, 0, &t->err
		));

		off2 = 0;

		Test_assert(t, "read back", readAll(t, sink2, &off2, EHDRReadFlags_KeepRGBE, &i1, &again, &type));

		RefPtr_dec(&sink2);
	}

	Test_assert(t, "planes identical", Buffer_eq(plane, again));

clean:
	Buffer_free(&again, t->alloc);
	Buffer_free(&plane, t->alloc);
	RefPtr_dec(&planeSrc);
	RefPtr_dec(&sink);
	RefPtr_dec(&src);
}

//Below 8 wide the adaptive marker cannot be told apart from pixel data, so both sides fall back to flat scanlines.
//The boundary is the interesting part, not the size.

void Test_HDRRoundTripFlatScanline(Test *t) {

	Test_setModule(t, "HDR round-trip: 4x4, below the adaptive width");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL;
	Buffer result = Buffer_createNull();
	HDRInfo info = { 0 };
	U64 encoded = 0;

	if(!makeFloatStream(t, 4, 4, &src, &type)) {
		Test_assert(t, "make source", false);
		goto clean;
	}

	Test_assert(t, "round-trip", roundTrip(
		t, src, 4, 4, EHDRWriteFlags_None, EHDRReadFlags_None, &info, &result, &encoded, &type
	));

	Test_assert(t, "w", info.w == 4);
	Test_assert(t, "h", info.h == 4);

	//Flat means exactly four bytes a texel plus the header, with no per-plane counts.

	Test_assert(t, "no RLE overhead", encoded > (U64) 4 * 4 * 4);

clean:
	Buffer_free(&result, t->alloc);
	RefPtr_dec(&src);
}

//And just above it, where adaptive RLE becomes legal.

void Test_HDRRoundTripWideScanline(Test *t) {

	Test_setModule(t, "HDR round-trip: 8x2, the adaptive minimum");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL;
	Buffer result = Buffer_createNull();
	HDRInfo info = { 0 };
	U64 encoded = 0;

	if(!makeFloatStream(t, 8, 2, &src, &type)) {
		Test_assert(t, "make source", false);
		goto clean;
	}

	Test_assert(t, "round-trip", roundTrip(
		t, src, 8, 2, EHDRWriteFlags_None, EHDRReadFlags_None, &info, &result, &encoded, &type
	));

	Test_assert(t, "w", info.w == 8);
	Test_assert(t, "h", info.h == 2);

clean:
	Buffer_free(&result, t->alloc);
	RefPtr_dec(&src);
}

//A constant image is all runs. It must compress hard AND still come back,
// which is what separates a working run encoder from one that merely produces something a decoder accepts.

void Test_HDRRoundTripRunLength(Test *t) {

	Test_setModule(t, "HDR round-trip: constant image compresses");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL;
	Buffer result = Buffer_createNull();
	HDRInfo info = { 0 };
	U64 encoded = 0;

	const U32 w = 256, h = 4;
	const U64 total = (U64) w * h * 4 * sizeof(F32);

	if(!MemoryStream_create(total, EMemoryStreamFlags_IsWritable, &type, (MemoryStreamRef**) &src, &t->err)) {
		Test_assert(t, "make source", false);
		goto clean;
	}

	{
		OxStream *s = RefPtr_data(src, OxStream);
		const F32 px[4] = { 0.25f, 0.5f, 1.0f, 1 };
		Bool ok = true;

		for(U64 i = 0; i < (U64) w * h && ok; ++i)
			ok = s->write(
				s, i * 4 * sizeof(F32), 0, Buffer_createRefConst(px, sizeof(px)), t->alloc, &t->err
			);

		if(!ok) {
			Test_assert(t, "fill source", false);
			goto clean;
		}
	}

	Test_assert(t, "round-trip", roundTrip(
		t, src, w, h, EHDRWriteFlags_None, EHDRReadFlags_None, &info, &result, &encoded, &type
	));

	//Flat would be four bytes a texel; runs should land far under a quarter of that.

	Test_assert(t, "runs compressed it", encoded < (U64) w * h);

	{
		const F32 *px = (const F32*) result.ptr;
		Bool same = true;

		for(U64 i = 1; i < (U64) w * h && same; ++i)
			for(int c = 0; c < 3; ++c)
				if(px[i * 4 + c] != px[c])
					same = false;

		Test_assert(t, "every texel decoded the same", same);
	}

clean:
	Buffer_free(&result, t->alloc);
	RefPtr_dec(&src);
}

//Exponent 0 is an exact zero rather than a denormal, at both ends.

void Test_HDRRoundTripExactZero(Test *t) {

	Test_setModule(t, "HDR round-trip: black is exactly zero");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL;
	Buffer result = Buffer_createNull();
	HDRInfo info = { 0 };
	U64 encoded = 0;

	const U32 w = 8, h = 2;

	if(!MemoryStream_create(
		(U64) w * h * 4 * sizeof(F32), EMemoryStreamFlags_IsWritable, &type, (MemoryStreamRef**) &src, &t->err
	)) {
		Test_assert(t, "make source", false);
		goto clean;
	}

	Test_assert(t, "round-trip", roundTrip(
		t, src, w, h, EHDRWriteFlags_None, EHDRReadFlags_None, &info, &result, &encoded, &type
	));

	{
		const F32 *px = (const F32*) result.ptr;
		Bool zero = true;

		for(U64 i = 0; i < (U64) w * h && zero; ++i)
			for(int c = 0; c < 3; ++c)
				if(px[i * 4 + c] != 0)
					zero = false;

		Test_assert(t, "decoded to exact zero", zero);
	}

clean:
	Buffer_free(&result, t->alloc);
	RefPtr_dec(&src);
}

//A properly exposed sun sits orders of magnitude above the sky beside it, which is the case RGBE exists for and the one a
// half-float cannot hold at all.

void Test_HDRRoundTripDynamicRange(Test *t) {

	Test_setModule(t, "HDR round-trip: sun beside sky");

	const RefPtrType type = MemoryStream_makeType(t->alloc);
	StreamRef *src = NULL;
	Buffer result = Buffer_createNull();
	HDRInfo info = { 0 };
	U64 encoded = 0;

	const U32 w = 8, h = 1;

	if(!MemoryStream_create(
		(U64) w * 4 * sizeof(F32), EMemoryStreamFlags_IsWritable, &type, (MemoryStreamRef**) &src, &t->err
	)) {
		Test_assert(t, "make source", false);
		goto clean;
	}

	{
		OxStream *s = RefPtr_data(src, OxStream);
		Bool ok = true;

		for(U32 x = 0; x < w && ok; ++x) {
			const F32 v = x == 4 ? 1.0e9f : 1.0e3f;
			const F32 px[4] = { v, v, v, 1 };
			ok = s->write(s, (U64) x * 4 * sizeof(F32), 0, Buffer_createRefConst(px, sizeof(px)), t->alloc, &t->err);
		}

		if(!ok) {
			Test_assert(t, "fill source", false);
			goto clean;
		}
	}

	Test_assert(t, "round-trip", roundTrip(
		t, src, w, h, EHDRWriteFlags_None, EHDRReadFlags_None, &info, &result, &encoded, &type
	));

	{
		const F32 *px = (const F32*) result.ptr;

		Test_assert(t, "sun survived",  closeEnough(px[4 * 4], 1.0e9f, 1.0e9f));
		Test_assert(t, "sky survived",  closeEnough(px[0],     1.0e3f, 1.0e3f));
		Test_assert(t, "sun is not sky", px[4 * 4] > px[0] * 1000);
	}

clean:
	Buffer_free(&result, t->alloc);
	RefPtr_dec(&src);
}
