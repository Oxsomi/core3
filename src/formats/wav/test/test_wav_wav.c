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

#include "test_wav_shared.h"
#include "formats/wav/wav_file.h"
#include "formats/wav/wav_headers.h"
#include "types/container/memory_stream.h"
#include "types/container/buffer.h"
#include "types/math/type_cast.h"

//WAVFile_cvt, pure conversion, no I/O

void Test_WAVCvtU8Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: U8 -> U8 identity");

	U8 buf[4] = { 0x00, 0x7F, 0x80, 0xFF };

	Test_assert(t, "u8[0]", WAVFile_cvt(buf, 1, 1, 0, false, false) == 0x00);
	Test_assert(t, "u8[1]", WAVFile_cvt(buf, 1, 1, 1, false, false) == 0x7F);
	Test_assert(t, "u8[2]", WAVFile_cvt(buf, 1, 1, 2, false, false) == 0x80);
	Test_assert(t, "u8[3]", WAVFile_cvt(buf, 1, 1, 3, false, false) == 0xFF);
}

void Test_WAVCvtI16Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I16 -> I16 identity");

	U16 buf[3] = { 0x0000, 0x1234, 0xFFFF };

	Test_assert(t, "i16[0]", WAVFile_cvt(buf, 2, 2, 0, false, false) == 0x0000);
	Test_assert(t, "i16[1]", WAVFile_cvt(buf, 2, 2, 1, false, false) == 0x1234);
	Test_assert(t, "i16[2]", WAVFile_cvt(buf, 2, 2, 2, false, false) == 0xFFFF);
}

//I16 -> U8: high byte + 0x7F (documents current behaviour)
void Test_WAVCvtI16ToU8(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I16 -> U8");

	U16 zero   = 0;
	U16 posMax = (U16)I16_MAX;
	U16 negMax = (U16)I16_MIN;

	Test_assert(t, "I16 zero -> U8", WAVFile_cvt(&zero,   2, 1, 0, false, false) == (U8)(0x00 + 0x7F));
	Test_assert(t, "I16 max  -> U8", WAVFile_cvt(&posMax, 2, 1, 0, false, false) == (U8)(0x7F + 0x7F));
	Test_assert(t, "I16 min  -> U8", WAVFile_cvt(&negMax, 2, 1, 0, false, false) == (U8)(0x80 + 0x7F));
}

void Test_WAVCvtI24Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I24 -> I24 identity");

	U8 buf[3] = { 0x56, 0x34, 0x12 };   //LE 0x123456

	Test_assert(t, "i24 identity", WAVFile_cvt(buf, 3, 3, 0, false, false) == 0x123456);
}

//I24 -> I16: drops the LSB
void Test_WAVCvtI24ToI16(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I24 -> I16 truncation");

	U8 buf[3] = { 0x56, 0x34, 0x12 };   //bytes [1],[2] -> 0x1234
	Test_assert(t, "i24 -> i16", WAVFile_cvt(buf, 3, 2, 0, false, false) == 0x1234);
}

//F32 -> F32: passthrough with clamping
void Test_WAVCvtF32Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F32 -> F32 passthrough + clamp");

	F32 samples[3] = { 0.5f, 2, -2 };

	Test_assert(t, "f32  0.5 passthrough", F32_fromU32Bits((U32)WAVFile_cvt(samples, 4, 4, 0, false, false)) ==  0.5f);
	Test_assert(t, "f32  2.0 -> +1.0",     F32_fromU32Bits((U32)WAVFile_cvt(samples, 4, 4, 1, false, false)) ==  1.0f);
	Test_assert(t, "f32 -2.0 -> -1.0",     F32_fromU32Bits((U32)WAVFile_cvt(samples, 4, 4, 2, false, false)) == -1.0f);
}

//F32 -> U8: full-scale mapping
void Test_WAVCvtF32ToU8(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F32 -> U8 full-scale");

	F32 samples[3] = { -1, 0, 1 };

	Test_assert(t, "f32 -1 -> u8 0",   WAVFile_cvt(samples, 4, 1, 0, false, false) == 0);
	Test_assert(t, "f32  0 -> u8 127", WAVFile_cvt(samples, 4, 1, 1, false, false) == 127);
	Test_assert(t, "f32 +1 -> u8 255", WAVFile_cvt(samples, 4, 1, 2, false, false) == 255);
}

//F32 -> I16: 0.0 maps to 0
void Test_WAVCvtF32ToI16(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F32 -> I16 midpoint");

	F32 mid[1] = { 0.0f };
	Test_assert(t, "f32 0 -> i16 0", (I16)WAVFile_cvt(mid, 4, 2, 0, false, false) == 0);
}

//F64 -> F32 downcast
void Test_WAVCvtF64ToF32(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F64 -> F32 downcast");

	F64 samples[2] = { 0.5, 2 };

	F32 out0 = F32_fromU32Bits((U32)WAVFile_cvt(samples, 8, 4, 0, false, false));
	Test_assert(t, "f64 0.5 -> f32 ~= 0.5", out0 > 0.4999f && out0 < 0.5001f);

	F32 out1 = F32_fromU32Bits((U32)WAVFile_cvt(samples, 8, 4, 1, false, false));
	Test_assert(t, "f64 2.0 clamped -> f32 1.0", out1 == 1.0f);
}

//F64 -> F64 identity
void Test_WAVCvtF64Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F64 -> F64 identity");

	F64 samples[1] = { -0.75 };
	F64 out = F64_fromU64Bits(WAVFile_cvt(samples, 8, 8, 0, false, false));
	Test_assert(t, "f64 -0.75 identity", out > -0.750001 && out < -0.749999);
}

//Index offset: reads the correct sample when i > 0
void Test_WAVCvtIndexOffset(Test *t) {

	Test_setModule(t, "WAVFile_cvt: index offset");

	U16 buf[2] = { 0x0010, 0x0020 };
	Test_assert(t, "i16 index 0", WAVFile_cvt(buf, 2, 2, 0, false, false) == 0x0010);
	Test_assert(t, "i16 index 1", WAVFile_cvt(buf, 2, 2, 1, false, false) == 0x0020);
}

//PCM32 identity
void Test_WAVCvtI32Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I32 -> I32 identity");

	I32 buf[2] = { 0x12345678, (I32)0x80000000 };

	Test_assert(t, "i32[0] identity", (I32)WAVFile_cvt(buf, 4, 4, 0, true, true) == 0x12345678);
	Test_assert(t, "i32[1] identity", (I32)WAVFile_cvt(buf, 4, 4, 1, true, true) == (I32)0x80000000);
}

//PCM32 -> I16: high 2 bytes
void Test_WAVCvtI32ToI16(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I32 -> I16 truncation");

	//0x12345678 -> high U16 = 0x1234
	I32 buf[1] = { 0x12345678 };
	Test_assert(t, "i32 -> i16 high bytes", WAVFile_cvt(buf, 4, 2, 0, true, false) == 0x1234);
}

//PCM32 -> I24: high 3 bytes
void Test_WAVCvtI32ToI24(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I32 -> I24 truncation");

	//0x12345678 -> drop lowest byte -> 0x123456
	I32 buf[1] = { 0x12345678 };
	Test_assert(t, "i32 -> i24 high bytes", WAVFile_cvt(buf, 4, 3, 0, true, false) == 0x123456);
}

//PCM32 -> F32: I32_MAX maps to +1, 0 maps to 0, -I32_MAX maps to ~-1
void Test_WAVCvtI32ToF32(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I32 -> F32 conversion");

	I32 buf[3] = { I32_MAX, 0, -I32_MAX };

	F32 out0 = F32_fromU32Bits((U32)WAVFile_cvt(buf, 4, 4, 0, true, false));
	Test_assert(t, "i32 max -> f32 ~+1",  out0 > 0.9999f && out0 <= 1.0f);

	F32 out1 = F32_fromU32Bits((U32)WAVFile_cvt(buf, 4, 4, 1, true, false));
	Test_assert(t, "i32 zero -> f32 0",   out1 > -1e-6f && out1 < 1e-6f);

	F32 out2 = F32_fromU32Bits((U32)WAVFile_cvt(buf, 4, 4, 2, true, false));
	Test_assert(t, "i32 min -> f32 ~-1",  out2 >= -1.0f && out2 < -0.9999f);
}

//WAVFile_avg, pure math, no I/O

void Test_WAVAvgU8(Test *t) {

	Test_setModule(t, "WAVFile_avg: U8");

	Test_assert(t, "u8 (10+20)/2 = 15",  WAVFile_avg(10,  20,  1, false) ==  15);
	Test_assert(t, "u8 (0+0)/2   = 0",   WAVFile_avg(0,   0,   1, false) ==   0);
	Test_assert(t, "u8 (0+255)/2 = 127", WAVFile_avg(0,   255, 1, false) == 127);
	Test_assert(t, "u8 (255+255) = 255", WAVFile_avg(255, 255, 1, false) == 255);
}

void Test_WAVAvgI16(Test *t) {

	Test_setModule(t, "WAVFile_avg: I16");

	Test_assert(t, "i16 (0 + 0) / 2 = 0",         (I16)WAVFile_avg(0, 0, 2, false) == 0);
	Test_assert(t, "i16 equal values",            (I16)WAVFile_avg((U16)500, (U16)500, 2, false) == 500);
	Test_assert(t, "i16 symmetric around zero",   (I16)WAVFile_avg((U16)1000, (U16)(U16)(-1000), 2, false) == 0);
}

void Test_WAVAvgF32(Test *t) {

	Test_setModule(t, "WAVFile_avg: F32");

	U64 a = U32_fromF32Bits(-0.5f);
	U64 b = U32_fromF32Bits( 0.5f);
	F32 r = F32_fromU32Bits((U32)WAVFile_avg(a, b, 4, false));
	Test_assert(t, "f32 (-0.5 + 0.5) / 2 = 0", r > -1e-6f && r < 1e-6f);

	a = U32_fromF32Bits(0.25f);
	b = U32_fromF32Bits(0.75f);
	r = F32_fromU32Bits((U32)WAVFile_avg(a, b, 4, false));
	Test_assert(t, "f32 (0.25 + 0.75) / 2 = 0.5", r > 0.4999f && r < 0.5001f);
}

void Test_WAVAvgF64(Test *t) {

	Test_setModule(t, "WAVFile_avg: F64");

	U64 a = U64_fromF64Bits(1.0);
	U64 b = U64_fromF64Bits(3.0);
	F64 r = F64_fromU64Bits(WAVFile_avg(a, b, 8, false));
	Test_assert(t, "f64 (1 + 3) / 2 = 2", r > 1.9999999 && r < 2.0000001);
}

void Test_WAVAvgI24(Test *t) {

	Test_setModule(t, "WAVFile_avg: I24");

	Test_assert(t, "i24 (100 + 200) / 2 = 150", (WAVFile_avg(100, 200, 3, false) & 0xFFFFFF) == 150u);

	U64 neg1000  = (U64)((U32)(-(I32)1000) & 0xFFFFFF);
	U64 r        = WAVFile_avg(1000, neg1000, 3, false);
	I32 signed_r = (I32)((r & 0x800000) ? (r | 0xFF000000) : r);
	Test_assert(t, "i24 (+1000 + -1000)/2 = 0", signed_r == 0);
}

//PCM32 avg
void Test_WAVAvgI32(Test *t) {

	Test_setModule(t, "WAVFile_avg: I32 PCM");

	Test_assert(t, "i32 (0 + 0) / 2 = 0",      (I32)WAVFile_avg(0, 0, 4, true) == 0);
	Test_assert(t, "i32 equal values",           (I32)WAVFile_avg((U64)(U32)1000, (U64)(U32)1000, 4, true) == 1000);
	Test_assert(t, "i32 symmetric around zero",
		(I32)WAVFile_avg((U64)(U32)I32_MAX, (U64)(U32)(-I32_MAX), 4, true) == 0
	);
}

//Build a MemoryStream backed by a caller-supplied byte buffer. Caller owns *sr.
static Bool makeSampleStream(
	Test *t,
	const void *samples,
	U64 totalBytes,
	const RefPtrType *type,
	StreamRef **sr
) {
	if (!MemoryStream_create(totalBytes, EMemoryStreamFlags_IsWritable, type, sr, &t->err))
		return false;

	if (samples) {
		OxStream *s = RefPtr_data(*sr, OxStream);
		s->write(s, 0, totalBytes, Buffer_createRefConst(samples, totalBytes), t->alloc, &t->err);
	}

	return true;
}

//Write a WAV to a fresh resizable MemoryStream then read it back.
static Bool wavRoundTrip(
	Test *t,
	StreamRef *inputSr,
	U64 inputOff,
	U64 dataLen,
	Bool isStereo,
	U32 freq,
	U16 stride,
	Bool isPcm,
	const RefPtrType *type,
	StreamRef **archiveSr,
	WAVFile *result
) {
	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, archiveSr, &t->err))
		return false;

	if (!WAV_write(*archiveSr, inputSr, 0, inputOff, dataLen, isStereo, freq, stride, isPcm, NULL, t->alloc, &t->err)) {
		RefPtr_dec(archiveSr);
		return false;
	}

	if (!WAV_read(*archiveSr, 0, 0, t->alloc, result, &t->err)) {
		RefPtr_dec(archiveSr);
		return false;
	}

	return true;
}

//WAV_write / WAV_read round-trips

//44.1 KHz stereo 16-bit, most common configuration
void Test_WAVRoundTripStereo16(Test *t) {

	Test_setModule(t, "WAV round-trip: stereo 16-bit 44100 Hz");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;

	const U32 freq       = 44100;
	const U16 stride     = 16;
	const U64 numSamples = 512;
	const U64 dataLen    = numSamples * 2 * (stride >> 3);

	if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
		Test_assert(t, "build input stream", false);
		goto doneStereo16;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "stereo16 round-trip",    wavRoundTrip(
			t, inputSr, 0, dataLen, true, freq, stride, true, &type, &archiveSr, &result
		));

		Test_assert(t, "stereo16 freq",          result.fmt.frequency  == freq);
		Test_assert(t, "stereo16 channels",      result.fmt.channels   == 2);
		Test_assert(t, "stereo16 stride",        result.fmt.stride     == stride);
		Test_assert(t, "stereo16 format PCM",    result.fmt.format     == ERIFFAudioFormat_PCM);
		Test_assert(t, "stereo16 dataLength",    result.dataLength     == (U32)dataLen);
		Test_assert(t, "stereo16 dataStart > 0", result.dataStart      >  0);
	}

doneStereo16:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//48 KHz mono F32
void Test_WAVRoundTripMonoF32(Test *t) {

	Test_setModule(t, "WAV round-trip: mono F32 48000 Hz");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;

	const U32 freq     = 48000;
	const U16 stride   = 32;
	const U64 nSamples = 256;
	const U64 dataLen  = nSamples * (stride >> 3);

	if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
		Test_assert(t, "build f32 stream", false);
		goto doneMonoF32;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "monoF32 round-trip", wavRoundTrip(
			t, inputSr, 0, dataLen, false, freq, stride, false, &type, &archiveSr, &result
		));

		Test_assert(t, "monoF32 freq",       result.fmt.frequency == freq);
		Test_assert(t, "monoF32 channels",   result.fmt.channels  == 1);
		Test_assert(t, "monoF32 stride",     result.fmt.stride    == 32);
		Test_assert(t, "monoF32 format",     result.fmt.format    == ERIFFAudioFormat_IEEE754);
		Test_assert(t, "monoF32 dataLen",    result.dataLength    == (U32)dataLen);
	}

doneMonoF32:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//44.1 KHz mono 8-bit
void Test_WAVRoundTripMono8(Test *t) {

	Test_setModule(t, "WAV round-trip: mono 8-bit 44100 Hz");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;

	if (!makeSampleStream(t, NULL, 256, &type, &inputSr)) {
		Test_assert(t, "build 8bit stream", false);
		goto doneMono8;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "mono8 round-trip", wavRoundTrip(
			t, inputSr, 0, 256, false, 44100, 8, true, &type, &archiveSr, &result
		));

		Test_assert(t, "mono8 stride",     result.fmt.stride   == 8);
		Test_assert(t, "mono8 channels",   result.fmt.channels == 1);
		Test_assert(t, "mono8 dataLen",    result.dataLength   == 256);
	}

doneMono8:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//192 KHz mono 64-bit float, upper boundary of supported rates
void Test_WAVRoundTripMono64(Test *t) {

	Test_setModule(t, "WAV round-trip: mono F64 192000 Hz");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;

	const U64 nSamples = 64;
	const U64 dataLen  = nSamples * 8;

	if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
		Test_assert(t, "build f64 stream", false);
		goto doneMono64;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "mono64 round-trip", wavRoundTrip(
			t, inputSr, 0, dataLen, false, 192000, 64, false, &type, &archiveSr, &result
		));

		Test_assert(t, "mono64 freq",       result.fmt.frequency == 192000);
		Test_assert(t, "mono64 stride",     result.fmt.stride    == 64);
		Test_assert(t, "mono64 format",     result.fmt.format    == ERIFFAudioFormat_IEEE754);
	}

doneMono64:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//44.1 KHz mono PCM32
void Test_WAVRoundTripMonoPCM32(Test *t) {

	Test_setModule(t, "WAV round-trip: mono PCM32 44100 Hz");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;

	const U64 nSamples = 256;
	const U64 dataLen  = nSamples * 4;

	if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
		Test_assert(t, "build pcm32 stream", false);
		goto doneMonoPCM32;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "monoPCM32 round-trip", wavRoundTrip(
			t, inputSr, 0, dataLen, false, 44100, 32, true, &type, &archiveSr, &result
		));

		Test_assert(t, "monoPCM32 freq",       result.fmt.frequency == 44100);
		Test_assert(t, "monoPCM32 channels",   result.fmt.channels  == 1);
		Test_assert(t, "monoPCM32 stride",     result.fmt.stride    == 32);
		Test_assert(t, "monoPCM32 format PCM", result.fmt.format    == ERIFFAudioFormat_PCM);
		Test_assert(t, "monoPCM32 dataLen",    result.dataLength    == (U32)dataLen);
	}

doneMonoPCM32:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//44.1 KHz stereo PCM32
void Test_WAVRoundTripStereoPCM32(Test *t) {

	Test_setModule(t, "WAV round-trip: stereo PCM32 44100 Hz");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;

	const U64 nSamples = 256;
	const U64 dataLen  = nSamples * 2 * 4;

	if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
		Test_assert(t, "build stereo pcm32 stream", false);
		goto doneStereoPCM32;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "stereoPCM32 round-trip", wavRoundTrip(
			t, inputSr, 0, dataLen, true, 44100, 32, true, &type, &archiveSr, &result
		));

		Test_assert(t, "stereoPCM32 channels",   result.fmt.channels == 2);
		Test_assert(t, "stereoPCM32 stride",     result.fmt.stride   == 32);
		Test_assert(t, "stereoPCM32 format PCM", result.fmt.format   == ERIFFAudioFormat_PCM);
		Test_assert(t, "stereoPCM32 dataLen",    result.dataLength   == (U32)dataLen);
	}

doneStereoPCM32:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//WAV_write error cases

void Test_WAVWriteInvalidFreq(Test *t) {

	Test_setModule(t, "WAV_write: invalid frequency rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;
	U8 dummy[4] = { 0 };

	if (!makeSampleStream(t, dummy, 4, &type, &inputSr)) {
		Test_assert(t, "build dummy stream", false);
		goto doneInvalidFreq;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
		Test_assert(t, "create archive stream", false);
		goto doneInvalidFreq;
	}

	Test_assert(t, "bad freq fails",
		!WAV_write(archiveSr, inputSr, 0, 0, 4, false, 12345, 16, true, NULL, t->alloc, NULL)
	);

doneInvalidFreq:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

void Test_WAVWriteInvalidStride(Test *t) {

	Test_setModule(t, "WAV_write: invalid stride rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;
	U8 dummy[4] = { 0 };

	if (!makeSampleStream(t, dummy, 4, &type, &inputSr)) {
		Test_assert(t, "build dummy stream", false);
		goto doneInvalidStride;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
		Test_assert(t, "create archive stream", false);
		goto doneInvalidStride;
	}

	Test_assert(t, "bad stride fails",
		!WAV_write(archiveSr, inputSr, 0, 0, 4, false, 44100, 12, true, NULL, t->alloc, NULL)
	);

doneInvalidStride:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//Data length not a multiple of bytesPerBlock
void Test_WAVWriteUnalignedLength(Test *t) {

	Test_setModule(t, "WAV_write: unaligned data length rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;

	if (!makeSampleStream(t, NULL, 5, &type, &inputSr)) {
		Test_assert(t, "build dummy stream", false);
		goto doneUnaligned;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
		Test_assert(t, "create archive stream", false);
		goto doneUnaligned;
	}

	Test_assert(t, "unaligned length fails",
		!WAV_write(archiveSr, inputSr, 0, 0, 5, true, 44100, 16, true, NULL, t->alloc, NULL)
	);

doneUnaligned:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}

//WAV_read error cases

//All-zero stream has wrong magic
void Test_WAVReadInvalidMagic(Test *t) {

	Test_setModule(t, "WAV_read: invalid magic rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *sr = NULL;

	if (!MemoryStream_create(128, EMemoryStreamFlags_IsWritable, &type, &sr, &t->err)) {
		Test_assert(t, "create zeroed stream", false);
		goto doneInvalidMagic;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "zero magic fails", !WAV_read(sr, 0, 0, t->alloc, &result, NULL));
	}

doneInvalidMagic:
	RefPtr_dec(&sr);
}

//OxStream too small to hold even a RIFFSection
void Test_WAVReadTruncated(Test *t) {

	Test_setModule(t, "WAV_read: truncated stream rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *sr = NULL;
	U8 tiny[4] = { 'R', 'I', 'F', 'F' };

	if (!makeSampleStream(t, tiny, 4, &type, &sr)) {
		Test_assert(t, "build tiny stream", false);
		goto doneTruncated;
	}

	{
		WAVFile result = { 0 };
		Test_assert(t, "truncated stream fails", !WAV_read(sr, 0, 0, t->alloc, &result, NULL));
	}

doneTruncated:
	RefPtr_dec(&sr);
}

//WAVFile_convert
//bit layout: bit7 = isStereo, bit6 = isPcm, low6 = byteCount

void Test_WAVConvertStereoToMono(Test *t) {

	Test_setModule(t, "WAVFile_convert: stereo I16 -> mono I16 average");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr  = NULL;
	StreamRef *outputSr = NULL;

	//avg(200,100)=150, avg(-100,-300)=-200, avg(0,400)=200, avg(1000,-1000)=0
	I16 stereo[8] = { 200, 100,  -100, -300,  0, 400,  1000, -1000 };
	const U64 srcLen = sizeof(stereo);

	if (!makeSampleStream(t, stereo, srcLen, &type, &inputSr)) {
		Test_assert(t, "build stereo stream", false);
		goto doneS2M;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
		Test_assert(t, "create output stream", false);
		goto doneS2M;
	}

	{
		//isStereo=1, isPcm=1, byteCount=2
		WAVConversionInfo info = {
			.format                = EAudioFormat_WAV,
			.splitType             = ESplitType_Average,
			.oldByteCountStereoPcm = (1 << 7) | (1 << 6) | 2,
			.newByteCountStereoPcm = (1 << 6) | 2			//mono, isPcm=1, byteCount=2
		};

		Bool ok = WAVFile_convert(inputSr, 0, srcLen, outputSr, 0, info, 44100, true, t->alloc, &t->err);
		Test_assert(t, "convert ok", ok);

		if (ok) {
			WAVFile wf = { 0 };
			if (Test_assert(t, "read back converted", WAV_read(outputSr, 0, 0, t->alloc, &wf, &t->err))) {
				Test_assert(t, "mono channels", wf.fmt.channels == 1);
				Test_assert(t, "mono dataLen",  wf.dataLength   == 4 * 2);

				MemoryStream *s = RefPtr_data(outputSr, MemoryStream);
				const I16 *out  = (const I16*)(s->data.ptr + wf.dataStart);

				Test_assert(t, "avg sample[0] ==  150", out[0] ==  150);
				Test_assert(t, "avg sample[1] == -200", out[1] == -200);
				Test_assert(t, "avg sample[2] ==  200", out[2] ==  200);
				Test_assert(t, "avg sample[3] ==    0", out[3] ==    0);
			}
		}
	}

doneS2M:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&outputSr);
}

//Left-channel-only split
void Test_WAVConvertStereoLeftOnly(Test *t) {

	Test_setModule(t, "WAVFile_convert: stereo I16 -> mono left channel only");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr  = NULL;
	StreamRef *outputSr = NULL;

	I16 stereo[2] = { 1000, -1000 };

	if (!makeSampleStream(t, stereo, sizeof(stereo), &type, &inputSr)) {
		Test_assert(t, "build stereo stream", false);
		goto doneLeft;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
		Test_assert(t, "create output stream", false);
		goto doneLeft;
	}

	{
		//isStereo=1, isPcm=1, byteCount=2
		WAVConversionInfo info = {
			.format                = EAudioFormat_WAV,
			.splitType             = ESplitType_Left,
			.oldByteCountStereoPcm = (1 << 7) | (1 << 6) | 2,
			.newByteCountStereoPcm = (1 << 6) | 2
		};

		Bool ok = WAVFile_convert(inputSr, 0, sizeof(stereo), outputSr, 0, info, 44100, true, t->alloc, &t->err);
		Test_assert(t, "left split ok", ok);

		if (ok) {
			WAVFile wf = { 0 };
			if (Test_assert(t, "read back left", WAV_read(outputSr, 0, 0, t->alloc, &wf, &t->err))) {
				Test_assert(t, "left mono channels", wf.fmt.channels == 1);
				MemoryStream *s  = RefPtr_data(outputSr, MemoryStream);
				const I16 *out   = (const I16*)(s->data.ptr + wf.dataStart);
				Test_assert(t, "left sample == 1000", out[0] == 1000);
			}
		}
	}

doneLeft:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&outputSr);
}

//srcLen not a multiple of bytes-per-block must be rejected
void Test_WAVConvertMisalignedSrc(Test *t) {

	Test_setModule(t, "WAVFile_convert: misaligned srcLen rejected");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr  = NULL;
	StreamRef *outputSr = NULL;

	if (!makeSampleStream(t, NULL, 3, &type, &inputSr)) {
		Test_assert(t, "build misaligned stream", false);
		goto doneMisaligned;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
		Test_assert(t, "create output stream", false);
		goto doneMisaligned;
	}

	{
		//mono, isPcm=1, byteCount=2
		WAVConversionInfo info = {
			.format                = EAudioFormat_WAV,
			.splitType             = ESplitType_Untouched,
			.oldByteCountStereoPcm = (1 << 6) | 2,
			.newByteCountStereoPcm = (1 << 6) | 2
		};

		Test_assert(t, "misaligned srcLen fails",
			!WAVFile_convert(inputSr, 0, 3, outputSr, 0, info, 44100, false, t->alloc, NULL)
		);
	}

doneMisaligned:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&outputSr);
}

//PCM32 -> F32 conversion via WAVFile_convert
//Verifies that the special I32->F32 path produces correct normalized output.

void Test_WAVConvertPCM32ToF32(Test *t) {

	Test_setModule(t, "WAVFile_convert: mono PCM32 -> mono F32");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr  = NULL;
	StreamRef *outputSr = NULL;

	//I32_MAX -> +1.0f, 0 -> 0.0f, -I32_MAX -> ~-1.0f
	I32 samples[3] = { I32_MAX, 0, -I32_MAX };

	if (!makeSampleStream(t, samples, sizeof(samples), &type, &inputSr)) {
		Test_assert(t, "build pcm32 stream", false);
		goto donePCM32ToF32;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
		Test_assert(t, "create output stream", false);
		goto donePCM32ToF32;
	}

	{
		//mono, isPcm=1, byteCount=4 -> mono, isPcm=0 (float), byteCount=4
		WAVConversionInfo info = {
			.format                = EAudioFormat_WAV,
			.splitType             = ESplitType_Untouched,
			.oldByteCountStereoPcm = (1 << 6) | 4,
			.newByteCountStereoPcm = 4
		};

		Bool ok = WAVFile_convert(inputSr, 0, sizeof(samples), outputSr, 0, info, 44100, true, t->alloc, &t->err);
		Test_assert(t, "pcm32->f32 convert ok", ok);

		if (ok) {
			WAVFile wf = { 0 };
			if (Test_assert(t, "read back f32", WAV_read(outputSr, 0, 0, t->alloc, &wf, &t->err))) {
				Test_assert(t, "f32 format",   wf.fmt.format   == ERIFFAudioFormat_IEEE754);
				Test_assert(t, "f32 stride",   wf.fmt.stride   == 32);
				Test_assert(t, "f32 channels", wf.fmt.channels == 1);
				Test_assert(t, "f32 dataLen",  wf.dataLength   == 3 * 4);

				MemoryStream *s  = RefPtr_data(outputSr, MemoryStream);
				const F32 *out   = (const F32*)(s->data.ptr + wf.dataStart);

				Test_assert(t, "pcm32 max -> f32 ~+1", out[0] > 0.9999f && out[0] <= 1.0f);
				Test_assert(t, "pcm32 zero -> f32 0",  out[1] > -1e-6f  && out[1] < 1e-6f);
				Test_assert(t, "pcm32 min -> f32 ~-1", out[2] >= -1.0f  && out[2] < -0.9999f);
			}
		}
	}

donePCM32ToF32:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&outputSr);
}

//Stereo PCM32 averaged to mono PCM32
void Test_WAVConvertStereoPCM32ToMono(Test *t) {

	Test_setModule(t, "WAVFile_convert: stereo PCM32 -> mono PCM32 average");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr  = NULL;
	StreamRef *outputSr = NULL;

	//avg(I32_MAX, -I32_MAX) = 0, avg(1000, 3000) = 2000
	I32 stereo[4] = { I32_MAX, -I32_MAX, 1000, 3000 };

	if (!makeSampleStream(t, stereo, sizeof(stereo), &type, &inputSr)) {
		Test_assert(t, "build stereo pcm32 stream", false);
		goto doneSterPCM32;
	}

	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
		Test_assert(t, "create output stream", false);
		goto doneSterPCM32;
	}

	{
		//isStereo=1, isPcm=1, byteCount=4 -> mono, isPcm=1, byteCount=4
		WAVConversionInfo info = {
			.format                = EAudioFormat_WAV,
			.splitType             = ESplitType_Average,
			.oldByteCountStereoPcm = (1 << 7) | (1 << 6) | 4,
			.newByteCountStereoPcm = (1 << 6) | 4
		};

		Bool ok = WAVFile_convert(inputSr, 0, sizeof(stereo), outputSr, 0, info, 44100, true, t->alloc, &t->err);
		Test_assert(t, "stereo pcm32 -> mono ok", ok);

		if (ok) {
			WAVFile wf = { 0 };
			if (Test_assert(t, "read back mono pcm32", WAV_read(outputSr, 0, 0, t->alloc, &wf, &t->err))) {
				Test_assert(t, "mono pcm32 channels", wf.fmt.channels == 1);
				Test_assert(t, "mono pcm32 stride",   wf.fmt.stride   == 32);
				Test_assert(t, "mono pcm32 format",   wf.fmt.format   == ERIFFAudioFormat_PCM);
				Test_assert(t, "mono pcm32 dataLen",  wf.dataLength   == 2 * 4);

				MemoryStream *s  = RefPtr_data(outputSr, MemoryStream);
				const I32 *out   = (const I32*)(s->data.ptr + wf.dataStart);

				Test_assert(t, "pcm32 avg[0] == 0",    out[0] == 0);
				Test_assert(t, "pcm32 avg[1] == 2000", out[1] == 2000);
			}
		}
	}

doneSterPCM32:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&outputSr);
}

//WAV_read: WAVE_FORMAT_EXTENSIBLE
//Hand-crafted extensible WAV: 44100Hz, stereo, 24-bit valid in 32-bit container, 8 bytes of data (2 stereo frames).
//Verifies that WAV_read correctly parses the extensible fmt chunk and extracts bitsPerSample as the real stride.

void Test_WAVReadExtended(Test *t) {
 
	Test_setModule(t, "WAV_read: WAVE_FORMAT_EXTENSIBLE stereo 24-in-32");
	const RefPtrType type = MemoryStream_makeType(t->alloc);
 
	StreamRef *sr = NULL;
 
	//Layout:
	//  RIFFHeader       12 bytes  (RIFF + size + WAVE)
	//  fmt  chunk       48 bytes  (RIFFSection=8, WAVEFORMATEX body=16, cbSize=2, ext body=22)
	//  data chunk        8 bytes  (RIFFSection=8) + 8 bytes data = 16 bytes
	//  Total            76 bytes, RIFF size = 76 - 8 = 68
 
	//44100Hz stereo 32-bit container: bytesPerBlock = 8, bytesPerSec = 44100*8 = 352800
	//RIFF size = 4(WAVE) + 48(fmt) + 8(data hdr) + 8(data) = 68
 
	static const U8 wav[] = {
 
		//RIFFHeader (12 bytes)
		'R','I','F','F',
		68, 0, 0, 0,			//RIFF size = 68
		'W','A','V','E',
 
		//fmt chunk: RIFFSection (8 bytes)
		'f','m','t',' ',
		40, 0, 0, 0,			//fmt chunk body size = 40
 
		//WAVEFORMATEX body (16 bytes)
		0xFE, 0xFF,				//wFormatTag = WAVE_FORMAT_EXTENSIBLE (0xFFFE little-endian)
		2, 0,					//nChannels = 2
		0x44, 0xAC, 0, 0,		//nSamplesPerSec = 44100 (0xAC44)
		0x20, 0x62, 0x05, 0x00,	//nAvgBytesPerSec = 352800
		8, 0,					//nBlockAlign = 8
		32, 0,					//wBitsPerSample = 32 (container)
 
		//cbSize + extension body (24 bytes)
		22, 0,					//cbSize = 22
		24, 0,					//wValidBitsPerSample = 24
		4, 0, 0, 0,				//dwChannelMask = SPEAKER_FRONT_CENTER (0x4) -- stereo would be 0x3
		1, 0, 0, 0,				//SubFormat Data1 = 0x00000001 (PCM)
		0x00, 0x00,				//Data2 = 0x0000
		0x10, 0x00,				//Data3 = 0x0010 (little-endian)
		0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,	//Data4
 
		//data chunk (8 + 8 bytes)
		'd','a','t','a',
		8, 0, 0, 0,				//data size = 8
 
		//2 stereo frames of 32-bit PCM (8 bytes)
		0x00, 0x00, 0x00, 0x12,	//frame 0 left
		0x00, 0x00, 0x00, 0x34	//frame 0 right
	};
 
	if (!makeSampleStream(t, wav, sizeof(wav), &type, &sr)) {
		Test_assert(t, "build extensible stream", false);
		goto doneExtended;
	}
 
	{
		WAVFile result = { 0 };
		Test_assert(t, "WAV_read extensible succeeds", WAV_read(sr, 0, 0, t->alloc, &result, &t->err));
		Test_assert(t, "ext channels",   result.fmt.channels  == 2);
		Test_assert(t, "ext stride",     result.fmt.stride    == 32);		//bitsPerSample, not container
		Test_assert(t, "ext freq",       result.fmt.frequency == 44100);
		Test_assert(t, "ext format PCM", result.fmt.format    == ERIFFAudioFormat_PCM);
		Test_assert(t, "ext dataLen",    result.dataLength    == 8);
		Test_assert(t, "ext dataStart",  result.dataStart     >  0);
	}
 
doneExtended:
	RefPtr_dec(&sr);
}

//WAV_write / WAV_read: odd-length data chunk gets a padding byte.
//Uses 1 sample of mono 8-bit (1 byte of data), odd length, so a 0x00 pad byte must follow.
//WAV_read must correctly parse this without the padding byte being counted as data.
 
void Test_WAVOddLengthPadding(Test *t) {
 
	Test_setModule(t, "WAV odd-length: padding byte written and read correctly");
	const RefPtrType type = MemoryStream_makeType(t->alloc);
 
	StreamRef *inputSr   = NULL;
	StreamRef *archiveSr = NULL;
 
	//1 byte of mono 8-bit data -- odd, so WAV_write must append a padding byte
	U8 sample[1] = { 0xAB };
 
	if (!makeSampleStream(t, sample, 1, &type, &inputSr)) {
		Test_assert(t, "build 1-byte stream", false);
		goto doneOdd;
	}
 
	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
		Test_assert(t, "create archive stream", false);
		goto doneOdd;
	}

	Bool wavWriteSuccess = WAV_write(archiveSr, inputSr, 0, 0, 1, false, 44100, 8, true, NULL, t->alloc, &t->err);
 
	Test_assert(t, "WAV_write 1-byte succeeds", wavWriteSuccess);
 
	if(wavWriteSuccess) {

		//Stream size should be: 12 (RIFFHeader) + 24 (fmt) + 8 (data hdr) + 1 (data) + 1 (pad) = 46
		OxStream *s = RefPtr_data(archiveSr, OxStream);
		Test_assert(t, "stream size includes pad byte", s->size == 46);
 
		//Padding byte must be 0x00
		MemoryStream *ms  = RefPtr_data(archiveSr, MemoryStream);
		Test_assert(t, "pad byte is 0x00", ms->data.ptr[45] == 0x00);
 
		//WAV_read must parse cleanly and report dataLength=1, not 2
		WAVFile result = { 0 };
		Test_assert(t, "WAV_read odd-length succeeds", WAV_read(archiveSr, 0, 0, t->alloc, &result, &t->err));
		Test_assert(t, "odd dataLength == 1", result.dataLength == 1);
		Test_assert(t, "odd channels == 1",   result.fmt.channels == 1);
		Test_assert(t, "odd stride == 8",     result.fmt.stride   == 8);
 
		//Verify the sample byte is at the right offset and correct value
		Test_assert(t, "sample byte correct", ms->data.ptr[result.dataStart] == 0xAB);
	}
 
doneOdd:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&archiveSr);
}
