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

//formats/wav/test/test_wav_conversion.c

#include "test_wav_shared.h"
#include "formats/wav/wav_file.h"
#include "formats/wav/wav_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/math/type_cast.h"

//Every row converts one sample and compares the exact result; floats are the raw bits of the input.
//Strides: 1 = unsigned PCM, 2, 3 = signed PCM, 4 = PCM when the flag is set (else F32), 8 = F64.

typedef struct CvtRow {
	const C8 *name;
	U64 in;        //Little endian sample as an integer, or the float bits
	U8 inStride, outStride;
	Bool inPcm, outPcm;
	U64 expected;
} CvtRow;

static const CvtRow cvtRows[] = {

	//Signed -> unsigned 8-bit: the bias is 0x80, so silence is 0x80 and the extremes map to 0x00 and 0xFF

	{ "i16 min -> u8",      0x8000,   2, 1, true, true, 0x00 },
	{ "i16 -256 -> u8",     0xFF00,   2, 1, true, true, 0x7F },
	{ "i16 -1 -> u8",       0xFFFF,   2, 1, true, true, 0x7F },
	{ "i16 0 -> u8",        0x0000,   2, 1, true, true, 0x80 },
	{ "i16 +1 -> u8",       0x0001,   2, 1, true, true, 0x80 },
	{ "i16 +256 -> u8",     0x0100,   2, 1, true, true, 0x81 },
	{ "i16 max -> u8",      0x7FFF,   2, 1, true, true, 0xFF },
	{ "i24 min -> u8",      0x800000, 3, 1, true, true, 0x00 },
	{ "i24 -1 -> u8",       0xFFFFFF, 3, 1, true, true, 0x7F },
	{ "i24 0 -> u8",        0x000000, 3, 1, true, true, 0x80 },
	{ "i24 max -> u8",      0x7FFFFF, 3, 1, true, true, 0xFF },
	{ "i32 min -> u8",      0x80000000, 4, 1, true, true, 0x00 },
	{ "i32 -1 -> u8",       0xFFFFFFFF, 4, 1, true, true, 0x7F },
	{ "i32 0 -> u8",        0x00000000, 4, 1, true, true, 0x80 },
	{ "i32 max -> u8",      0x7FFFFFFF, 4, 1, true, true, 0xFF },

	//Unsigned 8-bit -> wider signed

	{ "u8 0 -> i16",        0x00, 1, 2, true, true, 0x8000 },
	{ "u8 0x7F -> i16",     0x7F, 1, 2, true, true, 0xFF00 },
	{ "u8 0x80 -> i16",     0x80, 1, 2, true, true, 0x0000 },
	{ "u8 0x81 -> i16",     0x81, 1, 2, true, true, 0x0100 },
	{ "u8 0xFF -> i16",     0xFF, 1, 2, true, true, 0x7F00 },
	{ "u8 0 -> i24",        0x00, 1, 3, true, true, 0x800000 },
	{ "u8 0xFF -> i32",     0xFF, 1, 4, true, true, 0x7F000000 },

	//Signed narrowing drops the low bits (floor) and widening fills them with zero

	{ "i16 min -> i24",     0x8000,   2, 3, true, true, 0x800000 },
	{ "i16 -1 -> i24",      0xFFFF,   2, 3, true, true, 0xFFFF00 },
	{ "i16 max -> i32",     0x7FFF,   2, 4, true, true, 0x7FFF0000 },
	{ "i16 -1 -> i32",      0xFFFF,   2, 4, true, true, 0xFFFF0000 },
	{ "i24 -1 -> i16",      0xFFFFFF, 3, 2, true, true, 0xFFFF },
	{ "i24 +255 -> i16",    0x0000FF, 3, 2, true, true, 0x0000 },
	{ "i32 -1 -> i16",      0xFFFFFFFF, 4, 2, true, true, 0xFFFF },
	{ "i32 min -> i24",     0x80000000, 4, 3, true, true, 0x800000 },
	{ "i32 max -> i24",     0x7FFFFFFF, 4, 3, true, true, 0x7FFFFF },
	{ "i32 max -> i32",     0x7FFFFFFF, 4, 4, true, true, 0x7FFFFFFF },

	//Same width: the sample comes back untouched

	{ "u8 0 -> u8",         0x00,     1, 1, true, true, 0x00 },
	{ "u8 0x7F -> u8",      0x7F,     1, 1, true, true, 0x7F },
	{ "u8 0x80 -> u8",      0x80,     1, 1, true, true, 0x80 },
	{ "u8 0xFF -> u8",      0xFF,     1, 1, true, true, 0xFF },
	{ "i16 0 -> i16",       0x0000,   2, 2, true, true, 0x0000 },
	{ "i16 0x1234 -> i16",  0x1234,   2, 2, true, true, 0x1234 },
	{ "i16 -1 -> i16",      0xFFFF,   2, 2, true, true, 0xFFFF },
	{ "i24 0x123456 -> i24", 0x123456, 3, 3, true, true, 0x123456 },
	{ "i32 0x12345678 -> i32", 0x12345678, 4, 4, true, true, 0x12345678 },
	{ "i32 min -> i32",     0x80000000, 4, 4, true, true, 0x80000000 },
	{ "f64 -0.75 -> f64",   0xBFE8000000000000, 8, 8, false, false, 0xBFE8000000000000 },

	//Narrowing keeps the high bytes

	{ "i24 0x123456 -> i16", 0x123456, 3, 2, true, true, 0x1234 },
	{ "i32 0x12345678 -> i16", 0x12345678, 4, 2, true, true, 0x1234 },
	{ "i32 0x12345678 -> i24", 0x12345678, 4, 3, true, true, 0x123456 },

	//F32 -> PCM: clamp, scale by 2^(N - 1) - 1, round to nearest, so +-1 are +-max and the negation is exact

	{ "f32 -1 -> i16",      0xBF800000,  4, 2, false, true, 0x8001 },
	{ "f32 -0.5 -> i16",    0xBF000000,  4, 2, false, true, 0xC000 },
	{ "f32 0 -> i16",       0x00000000,   4, 2, false, true, 0x0000 },
	{ "f32 -0 -> i16",      0x80000000,  4, 2, false, true, 0x0000 },
	{ "f32 0.5 -> i16",     0x3F000000,   4, 2, false, true, 0x4000 },
	{ "f32 1 -> i16",       0x3F800000,   4, 2, false, true, 0x7FFF },
	{ "f32 2 -> i16",       0x40000000,   4, 2, false, true, 0x7FFF },
	{ "f32 -2 -> i16",      0xC0000000,  4, 2, false, true, 0x8001 },
	{ "f32 inf -> i16",     0x7F800000,   4, 2, false, true, 0x7FFF },
	{ "f32 -inf -> i16",    0xFF800000,   4, 2, false, true, 0x8001 },
	{ "f32 nan -> i16",     0x7FC00000,   4, 2, false, true, 0x0000 },
	{ "f32 -1 -> i24",      0xBF800000,  4, 3, false, true, 0x800001 },
	{ "f32 0 -> i24",       0x00000000,   4, 3, false, true, 0x000000 },
	{ "f32 1 -> i24",       0x3F800000,   4, 3, false, true, 0x7FFFFF },
	{ "f32 2 -> i24",       0x40000000,   4, 3, false, true, 0x7FFFFF },
	{ "f32 -1 -> u8",       0xBF800000,  4, 1, false, true, 0x01 },
	{ "f32 0 -> u8",        0x00000000,   4, 1, false, true, 0x80 },
	{ "f32 1 -> u8",        0x3F800000,   4, 1, false, true, 0xFF },
	{ "f32 -9 -> u8",       0xC1100000,  4, 1, false, true, 0x01 },

	//Float -> PCM32 is an integer when the target is PCM (the flag was ignored before)

	{ "f32 -1 -> i32 pcm",  0xBF800000,  4, 4, false, true, 0x80000001 },
	{ "f32 0 -> i32 pcm",   0x00000000,   4, 4, false, true, 0x00000000 },
	{ "f32 1 -> i32 pcm",   0x3F800000,   4, 4, false, true, 0x7FFFFFFF },
	{ "f32 5 -> i32 pcm",   0x40A00000,   4, 4, false, true, 0x7FFFFFFF },
	{ "f32 0.5 -> i32 pcm", 0x3F000000,   4, 4, false, true, 0x40000000 },

	//F64 -> PCM

	{ "f64 -1 -> i16",      0xBFF0000000000000, 8, 2, false, true, 0x8001 },
	{ "f64 1 -> i16",       0x3FF0000000000000, 8, 2, false, true, 0x7FFF },
	{ "f64 1 -> i32 pcm",   0x3FF0000000000000, 8, 4, false, true, 0x7FFFFFFF },
	{ "f64 -1 -> i32 pcm",  0xBFF0000000000000, 8, 4, false, true, 0x80000001 },
	{ "f64 1 -> i24",       0x3FF0000000000000, 8, 3, false, true, 0x7FFFFF },
	{ "f64 100 -> i16",     0x4059000000000000, 8, 2, false, true, 0x7FFF },

	//PCM -> float: the max is exactly 1 and the most negative value clamps to -1

	{ "i16 max -> f32",     0x7FFF,   2, 4, true, false, 0x3F800000 },
	{ "i16 min -> f32",     0x8000,   2, 4, true, false, 0xBF800000 },
	{ "i16 0 -> f32",       0x0000,   2, 4, true, false, 0x00000000 },
	{ "i16 -32767 -> f32",  0x8001,   2, 4, true, false, 0xBF800000 },
	{ "u8 255 -> f32",      0xFF,     1, 4, true, false, 0x3F800000 },
	{ "u8 128 -> f32",      0x80,     1, 4, true, false, 0x00000000 },
	{ "u8 0 -> f32",        0x00,     1, 4, true, false, 0xBF800000 },
	{ "i24 max -> f32",     0x7FFFFF, 3, 4, true, false, 0x3F800000 },
	{ "i32 max -> f32",     0x7FFFFFFF, 4, 4, true, false, 0x3F800000 },
	{ "i32 min -> f32",     0x80000000, 4, 4, true, false, 0xBF800000 },
	{ "i32 0 -> f32",       0x00000000, 4, 4, true, false, 0x00000000 },
	{ "i32 -max -> f32",    0x80000001, 4, 4, true, false, 0xBF800000 },
	{ "i16 max -> f64",     0x7FFF,   2, 8, true, false, 0x3FF0000000000000 },
	{ "i16 min -> f64",     0x8000,   2, 8, true, false, 0xBFF0000000000000 },
	{ "i32 max -> f64",     0x7FFFFFFF, 4, 8, true, false, 0x3FF0000000000000 },

	//Float -> float: clamp (NaN is 0) and a width change

	{ "f32 2 -> f32",       0x40000000,   4, 4, false, false, 0x3F800000 },
	{ "f32 -2 -> f32",      0xC0000000,  4, 4, false, false, 0xBF800000 },
	{ "f32 0.25 -> f32",    0x3E800000,  4, 4, false, false, 0x3E800000 },
	{ "f32 nan -> f32",     0x7FC00000,   4, 4, false, false, 0x00000000 },
	{ "f32 0.25 -> f64",    0x3E800000,  4, 8, false, false, 0x3FD0000000000000 },
	{ "f64 0.25 -> f32",    0x3FD0000000000000, 8, 4, false, false, 0x3E800000 },
	{ "f64 -3 -> f64",      0xC008000000000000, 8, 8, false, false, 0xBFF0000000000000 },
	{ "f64 -3 -> f32",      0xC008000000000000, 8, 4, false, false, 0xBF800000 },
	{ "f64 0.5 -> f32",     0x3FE0000000000000, 8, 4, false, false, 0x3F000000 },
	{ "f64 2 -> f32",       0x4000000000000000, 8, 4, false, false, 0x3F800000 },
	{ "f32 0.5 -> f32",     0x3F000000, 4, 4, false, false, 0x3F000000 },
};

void Test_WAVCvtTable(Test *t) {

	Test_setModule(t, "WAVFile_cvt: exact samples at the extremes");

	for (U64 i = 0; i < sizeof(cvtRows) / sizeof(cvtRows[0]); ++i) {

		const CvtRow *row = &cvtRows[i];

		U8 bytes[8];

		for (U8 j = 0; j < 8; ++j)
			bytes[j] = (U8) (row->in >> (j << 3));

		U64 mask = row->outStride == 8 ? U64_MAX : (((U64) 1 << (row->outStride << 3)) - 1);
		U64 got = WAVFile_cvt(bytes, row->inStride, row->outStride, 0, row->inPcm, row->outPcm);

		Test_assert(t, row->name, got == (row->expected & mask));
	}
}

//The index is in samples of the source stride, and the source doesn't have to be aligned

void Test_WAVCvtUnalignedIndex(Test *t) {

	Test_setModule(t, "WAVFile_cvt: unaligned source and index");

	U8 buf[1 + 12] = { 0xEE, 0x00, 0x80, 0xFF, 0x7F, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x80 };
	const void *src = buf + 1;

	Test_assert(t, "i16[0] -> i24", WAVFile_cvt(src, 2, 3, 0, true, true) == 0x800000);
	Test_assert(t, "i16[1] -> i24", WAVFile_cvt(src, 2, 3, 1, true, true) == 0x7FFF00);
	Test_assert(t, "i16[2] -> i24", WAVFile_cvt(src, 2, 3, 2, true, true) == 0x000100);
	Test_assert(t, "i16[3] -> i24", WAVFile_cvt(src, 2, 3, 3, true, true) == 0xFFFF00);
	Test_assert(t, "i16[5] -> i16", WAVFile_cvt(src, 2, 2, 5, true, true) == 0x8000);
	Test_assert(t, "i24[1] -> i16", WAVFile_cvt(src, 3, 2, 1, true, true) == 0x0001);
	Test_assert(t, "bad stride 0 -> 0", WAVFile_cvt(src, 0, 2, 0, true, true) == 0);
	Test_assert(t, "bad stride 5 -> 0", WAVFile_cvt(src, 5, 2, 0, true, true) == 0);
	Test_assert(t, "bad out stride -> 0", WAVFile_cvt(src, 2, 6, 0, true, true) == 0);
}

typedef struct AvgRow {
	const C8 *name;
	U64 a, b;
	U8 stride;
	Bool pcm;
	U64 expected;
} AvgRow;

static const AvgRow avgRows[] = {

	//Unsigned 8-bit

	{ "u8 0 0",              0x00, 0x00, 1, true, 0x00 },
	{ "u8 255 255",          0xFF, 0xFF, 1, true, 0xFF },
	{ "u8 0 255",            0x00, 0xFF, 1, true, 0x7F },

	//Signed averages floor, with the sign handled by biasing by 2^(N - 1)

	{ "i16 min min",         0x8000, 0x8000, 2, true, 0x8000 },
	{ "i16 max max",         0x7FFF, 0x7FFF, 2, true, 0x7FFF },
	{ "i16 min max",         0x8000, 0x7FFF, 2, true, 0xFFFF },
	{ "i16 -1 0",            0xFFFF, 0x0000, 2, true, 0xFFFF },
	{ "i16 -1 +1",           0xFFFF, 0x0001, 2, true, 0x0000 },
	{ "i16 1 2",             0x0001, 0x0002, 2, true, 0x0001 },
	{ "i24 min min",         0x800000, 0x800000, 3, true, 0x800000 },
	{ "i24 max max",         0x7FFFFF, 0x7FFFFF, 3, true, 0x7FFFFF },
	{ "i24 min max",         0x800000, 0x7FFFFF, 3, true, 0xFFFFFF },
	{ "i24 -1 0",            0xFFFFFF, 0x000000, 3, true, 0xFFFFFF },
	{ "i24 -1 +1",           0xFFFFFF, 0x000001, 3, true, 0x000000 },
	{ "i24 0 0",             0, 0, 3, true, 0 },
	{ "i32 min min",         0x80000000, 0x80000000, 4, true, 0x80000000 },
	{ "i32 max max",         0x7FFFFFFF, 0x7FFFFFFF, 4, true, 0x7FFFFFFF },
	{ "i32 min max",         0x80000000, 0x7FFFFFFF, 4, true, 0xFFFFFFFF },
	{ "i32 -1 0",            0xFFFFFFFF, 0x00000000, 4, true, 0xFFFFFFFF },
	{ "i32 -1 +1",           0xFFFFFFFF, 0x00000001, 4, true, 0x00000000 },
	{ "i32 max -max",        0x7FFFFFFF, 0x80000001, 4, true, 0x00000000 },
	{ "i32 3 4",             0x00000003, 0x00000004, 4, true, 0x00000003 },
	{ "i32 -3 -4",           0xFFFFFFFD, 0xFFFFFFFC, 4, true, 0xFFFFFFFC },

	{ "u8 10 20",            0x0A, 0x14, 1, true, 0x0F },
	{ "i16 500 500",         0x01F4, 0x01F4, 2, true, 0x01F4 },
	{ "i16 1000 -1000",      0x03E8, 0xFC18, 2, true, 0x0000 },
	{ "i24 100 200",         0x64, 0xC8, 3, true, 0x96 },
	{ "i24 1000 -1000",      0x0003E8, 0xFFFC18, 3, true, 0x000000 },
	{ "i32 0 0",             0, 0, 4, true, 0 },
	{ "i32 1000 1000",       0x3E8, 0x3E8, 4, true, 0x3E8 },

	//Floats

	{ "f32 0.5 1.5",         0x3F000000, 0x3FC00000, 4, false, 0x3F800000 },
	{ "f32 -1 1",            0xBF800000, 0x3F800000, 4, false, 0x00000000 },
	{ "f32 -0.5 0.5",        0xBF000000, 0x3F000000, 4, false, 0x00000000 },
	{ "f32 0.25 0.75",       0x3E800000, 0x3F400000, 4, false, 0x3F000000 },
	{ "f64 1 3",             0x3FF0000000000000, 0x4008000000000000, 8, false, 0x4000000000000000 },
};

void Test_WAVAvgTable(Test *t) {

	Test_setModule(t, "WAVFile_avg: exact samples at the extremes");

	for (U64 i = 0; i < sizeof(avgRows) / sizeof(avgRows[0]); ++i) {
		const AvgRow *row = &avgRows[i];
		Test_assert(t, row->name, WAVFile_avg(row->a, row->b, row->stride, row->pcm) == row->expected);
	}
}

//The stream type has to outlive every stream made from it
static RefPtrType streamTypeStorage;

static const RefPtrType *streamType(Test *t) {
	streamTypeStorage = MemoryStream_makeType(t->alloc);
	return &streamTypeStorage;
}

//WAVFile_convert

static Bool convertBytes(
	Test *t,
	const void *src,
	U64 srcLen,
	WAVConversionInfo info,
	Bool writeHeader,
	StreamRef **output,
	Error *err
) {

	const RefPtrType *typePtr = streamType(t);
	StreamRef *input = NULL;
	Bool ok = false;

	if (!MemoryStream_create(srcLen, EMemoryStreamFlags_IsWritable, typePtr, &input, &t->err))
		return false;

	OxStream *s = RefPtr_data(input, OxStream);

	s->write(s, 0, srcLen, Buffer_createRefConst(src, srcLen), t->alloc, &t->err);

	if (MemoryStream_create(0, EMemoryStreamFlags_WriteResize, typePtr, output, &t->err))
		ok = WAVFile_convert(input, 0, srcLen, *output, 0, info, 44100, writeHeader, t->alloc, err);

	RefPtr_dec(&input);
	return ok;
}

#define PCM(n)   ((U8) ((1 << 6) | (n)))
#define FLT(n)   ((U8) (n))

//Widening never stored the wider sample: 2 to 4 bytes wrote one byte plus zeros

void Test_WAVConvertExpand(Test *t) {

	Test_setModule(t, "WAVFile_convert: expansion writes the whole sample");

	U16 src[4] = { 0x1234, 0xFFFF, 0x8000, 0x7FFF };

	WAVConversionInfo info = {
		.format = EAudioFormat_WAV, .splitType = ESplitType_Untouched,
		.oldByteCountStereoPcm = PCM(2), .newByteCountStereoPcm = PCM(4)
	};

	StreamRef *out = NULL;

	Test_assert(t, "2 -> 4 converts", convertBytes(t, src, sizeof(src), info, false, &out, &t->err));

	if (out) {

		const U8 *data = RefPtr_data(out, MemoryStream)->data.ptr;
		static const U8 expected[16] = {
			0x00, 0x00, 0x34, 0x12,
			0x00, 0x00, 0xFF, 0xFF,
			0x00, 0x00, 0x00, 0x80,
			0x00, 0x00, 0xFF, 0x7F
		};

		Bool eq = RefPtr_data(out, OxStream)->size == 16;

		for (U8 i = 0; eq && i < 16; ++i)
			eq = data[i] == expected[i];

		Test_assert(t, "2 -> 4 bytes", eq);
		RefPtr_dec(&out);
	}

	//u8 -> i24, as bytes (no alignment assumed)

	U8 u8[3] = { 0x00, 0x80, 0xFF };

	info.oldByteCountStereoPcm = PCM(1);
	info.newByteCountStereoPcm = PCM(3);

	Test_assert(t, "1 -> 3 converts", convertBytes(t, u8, sizeof(u8), info, false, &out, &t->err));

	if (out) {

		const U8 *data = RefPtr_data(out, MemoryStream)->data.ptr;
		static const U8 expected[9] = { 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F };

		Bool eq = RefPtr_data(out, OxStream)->size == 9;

		for (U8 i = 0; eq && i < 9; ++i)
			eq = data[i] == expected[i];

		Test_assert(t, "u8 -> i24 bytes", eq);
		RefPtr_dec(&out);
	}
}

//Float -> PCM through the header path: the format tag and samples both come out as PCM

void Test_WAVConvertFloatToPCM(Test *t) {

	Test_setModule(t, "WAVFile_convert: F32 -> PCM16, PCM24 and PCM32");

	F32 src[5] = { -1.0f, 0.0f, 1.0f, 2.0f, -2.0f };
	StreamRef *out = NULL;

	WAVConversionInfo info = {
		.format = EAudioFormat_WAV, .splitType = ESplitType_Untouched,
		.oldByteCountStereoPcm = FLT(4), .newByteCountStereoPcm = PCM(2)
	};

	Test_assert(t, "f32 -> i16", convertBytes(t, src, sizeof(src), info, true, &out, &t->err));

	if (out) {

		WAVFile wf = { 0 };

		if (Test_assert(t, "read", WAV_read(out, 0, 0, t->alloc, &wf, &t->err))) {

			const U8 *d = RefPtr_data(out, MemoryStream)->data.ptr + wf.dataStart;

			Test_assert(t, "pcm", wf.fmt.format == ERIFFAudioFormat_PCM && wf.fmt.stride == 16);
			Test_assert(t, "len", wf.dataLength == 10);
			Test_assert(t, "-1", d[0] == 0x01 && d[1] == 0x80);
			Test_assert(t, "0", d[2] == 0 && d[3] == 0);
			Test_assert(t, "1", d[4] == 0xFF && d[5] == 0x7F);
			Test_assert(t, "2", d[6] == 0xFF && d[7] == 0x7F);
			Test_assert(t, "-2", d[8] == 0x01 && d[9] == 0x80);
		}

		RefPtr_dec(&out);
	}

	info.newByteCountStereoPcm = PCM(4);
	Test_assert(t, "f32 -> i32", convertBytes(t, src, sizeof(src), info, true, &out, &t->err));

	if (out) {

		WAVFile wf = { 0 };

		if (Test_assert(t, "read", WAV_read(out, 0, 0, t->alloc, &wf, &t->err))) {

			const U8 *d = RefPtr_data(out, MemoryStream)->data.ptr + wf.dataStart;

			Test_assert(t, "pcm32", wf.fmt.format == ERIFFAudioFormat_PCM && wf.fmt.stride == 32);
			Test_assert(t, "-1", d[0] == 0x01 && d[1] == 0 && d[2] == 0 && d[3] == 0x80);
			Test_assert(t, "0", d[4] == 0 && d[5] == 0 && d[6] == 0 && d[7] == 0);
			Test_assert(t, "1", d[8] == 0xFF && d[9] == 0xFF && d[10] == 0xFF && d[11] == 0x7F);
			Test_assert(t, "2", d[12] == 0xFF && d[13] == 0xFF && d[14] == 0xFF && d[15] == 0x7F);
		}

		RefPtr_dec(&out);
	}

	//Odd byte total: 5 samples of 24 bit is 15 bytes; the header declares the pad and the converter leaves it zero

	info.newByteCountStereoPcm = PCM(3);
	Test_assert(t, "f32 -> i24", convertBytes(t, src, sizeof(src), info, true, &out, &t->err));

	if (out) {

		WAVFile wf = { 0 };

		if (Test_assert(t, "read", WAV_read(out, 0, 0, t->alloc, &wf, &t->err))) {

			const U8 *data = RefPtr_data(out, MemoryStream)->data.ptr + wf.dataStart;

			Test_assert(t, "len", wf.dataLength == 15);
			Test_assert(t, "-1", data[0] == 0x01 && data[1] == 0x00 && data[2] == 0x80);
			Test_assert(t, "1", data[6] == 0xFF && data[7] == 0xFF && data[8] == 0x7F);
			Test_assert(t, "size has pad", RefPtr_data(out, OxStream)->size == wf.dataStart + 16);
			Test_assert(t, "pad", data[15] == 0);
		}

		RefPtr_dec(&out);
	}
}

//Stereo average at the extremes: the averaged sample keeps the sign

void Test_WAVConvertAverageExtremes(Test *t) {

	Test_setModule(t, "WAVFile_convert: stereo average at the extremes");

	//Pairs: (min, max) -> -1, (-1, 0) -> -1, (max, max) -> max
	I32 src[6] = { I32_MIN, I32_MAX, -1, 0, I32_MAX, I32_MAX };

	WAVConversionInfo info = {
		.format = EAudioFormat_WAV, .splitType = ESplitType_Average,
		.oldByteCountStereoPcm = (U8) ((1 << 7) | PCM(4)), .newByteCountStereoPcm = PCM(4)
	};

	StreamRef *out = NULL;

	Test_assert(t, "converts", convertBytes(t, src, sizeof(src), info, false, &out, &t->err));

	if (out) {

		const U8 *d = RefPtr_data(out, MemoryStream)->data.ptr;

		Test_assert(t, "(min, max) = -1", d[0] == 0xFF && d[1] == 0xFF && d[2] == 0xFF && d[3] == 0xFF);
		Test_assert(t, "(-1, 0) = -1", d[4] == 0xFF && d[5] == 0xFF && d[6] == 0xFF && d[7] == 0xFF);
		Test_assert(t, "(max, max) = max", d[8] == 0xFF && d[9] == 0xFF && d[10] == 0xFF && d[11] == 0x7F);
		RefPtr_dec(&out);
	}
}

//Invalid info is rejected rather than turned into a modulo by zero, a stack overrun or a bogus write

typedef struct InvalidInfoCase {
	const C8 *name;
	WAVConversionInfo info;
} InvalidInfoCase;

void Test_WAVConvertInvalidInfo(Test *t) {

	Test_setModule(t, "WAVFile_convert: invalid info rejected");

	U8 src[64] = { 0 };

	const InvalidInfoCase cases[] = {
		{ "old byte count 0", { EAudioFormat_WAV, ESplitType_Untouched, PCM(0), PCM(2) } },
		{ "new byte count 0", { EAudioFormat_WAV, ESplitType_Untouched, PCM(2), PCM(0) } },
		{ "new byte count 9", { EAudioFormat_WAV, ESplitType_Untouched, PCM(2), FLT(9) } },
		{ "new byte count 5", { EAudioFormat_WAV, ESplitType_Untouched, PCM(2), PCM(5) } },
		{ "old byte count 9", { EAudioFormat_WAV, ESplitType_Untouched, FLT(9), PCM(2) } },
		{ "old byte count 17 stereo", { EAudioFormat_WAV, ESplitType_Average, (U8) (0x80 | PCM(17)), PCM(2) } },
		{ "old byte count 63", { EAudioFormat_WAV, ESplitType_Untouched, PCM(63), PCM(2) } },
		{ "split type 4", { EAudioFormat_WAV, 4, PCM(2), PCM(2) } },
		{ "split type 255", { EAudioFormat_WAV, 255, PCM(2), PCM(2) } },
		{ "split on mono", { EAudioFormat_WAV, ESplitType_Left, PCM(2), PCM(2) } },
		{ "format 1", { 1, ESplitType_Untouched, PCM(2), PCM(2) } },
		{ "pcm flag on 8 byte", { EAudioFormat_WAV, ESplitType_Untouched, PCM(8), PCM(2) } },
		{ "float flag on 2 byte", { EAudioFormat_WAV, ESplitType_Untouched, FLT(2), PCM(2) } },
		{ "new pcm 8 byte", { EAudioFormat_WAV, ESplitType_Untouched, PCM(2), PCM(8) } },
		{ "new float 3 byte", { EAudioFormat_WAV, ESplitType_Untouched, PCM(2), FLT(3) } },
	};

	for (U64 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {

		StreamRef *out = NULL;
		Error err = (Error) { 0 };
		Bool ok = convertBytes(t, src, 8, cases[i].info, true, &out, &err);

		Test_assert(t, cases[i].name, !ok && err.genericError);
		RefPtr_dec(&out);
	}

	//Range overflow

	const RefPtrType *typePtr = streamType(t);
	StreamRef *in = NULL, *out = NULL;

	if (
		Test_assert(t, "streams", MemoryStream_create(8, EMemoryStreamFlags_IsWritable, typePtr, &in, &t->err)) &&
		Test_assert(t, "streams", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, typePtr, &out, &t->err))
	) {

		WAVConversionInfo info = { EAudioFormat_WAV, ESplitType_Untouched, PCM(2), PCM(2) };

		Test_assert(
			t, "srcOff + srcLen wraps",
			!WAVFile_convert(in, 4, U64_MAX - 1, out, 0, info, 44100, false, t->alloc, NULL)
		);

		Test_assert(t, "past the end", !WAVFile_convert(in, 4, 8, out, 0, info, 44100, false, t->alloc, NULL));
	}

	RefPtr_dec(&in);
	RefPtr_dec(&out);
}
