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

	Test_assert(t, "u8[0]", WAVFile_cvt(buf, 1, 1, 0) == 0x00);
	Test_assert(t, "u8[1]", WAVFile_cvt(buf, 1, 1, 1) == 0x7F);
	Test_assert(t, "u8[2]", WAVFile_cvt(buf, 1, 1, 2) == 0x80);
	Test_assert(t, "u8[3]", WAVFile_cvt(buf, 1, 1, 3) == 0xFF);
}

void Test_WAVCvtI16Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I16 -> I16 identity");

	U16 buf[3] = { 0x0000, 0x1234, 0xFFFF };

	Test_assert(t, "i16[0]", WAVFile_cvt(buf, 2, 2, 0) == 0x0000);
	Test_assert(t, "i16[1]", WAVFile_cvt(buf, 2, 2, 1) == 0x1234);
	Test_assert(t, "i16[2]", WAVFile_cvt(buf, 2, 2, 2) == 0xFFFF);
}

//I16 -> U8: high byte + 0x7F (documents current behaviour)
void Test_WAVCvtI16ToU8(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I16 -> U8");

	U16 zero   = 0;
	U16 posMax = (U16)I16_MAX;
	U16 negMax = (U16)I16_MIN;

	//(U8)(high_byte + 0x7F)

	Test_assert(t, "I16 zero -> U8",   WAVFile_cvt(&zero,   2, 1, 0) == (U8)(0x00 + 0x7F));
	Test_assert(t, "I16 max  -> U8",   WAVFile_cvt(&posMax, 2, 1, 0) == (U8)(0x7F + 0x7F));
	Test_assert(t, "I16 min  -> U8",   WAVFile_cvt(&negMax, 2, 1, 0) == (U8)(0x80 + 0x7F));
}

void Test_WAVCvtI24Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I24 -> I24 identity");

	U8 buf[3] = { 0x56, 0x34, 0x12 };   //LE 0x123456

	Test_assert(t, "i24 identity", WAVFile_cvt(buf, 3, 3, 0) == 0x123456);
}

//I24 -> I16: drops the LSB
void Test_WAVCvtI24ToI16(Test *t) {

	Test_setModule(t, "WAVFile_cvt: I24 -> I16 truncation");

	U8 buf[3] = { 0x56, 0x34, 0x12 };   //bytes [1],[2] -> 0x1234
	Test_assert(t, "i24 -> i16", WAVFile_cvt(buf, 3, 2, 0) == 0x1234);
}

//F32 -> F32: passthrough with clamping
void Test_WAVCvtF32Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F32 -> F32 passthrough + clamp");

	F32 samples[3] = { 0.5f, 2, -2 };

	Test_assert(t, "f32  0.5 passthrough", F32_fromU32Bits((U32)WAVFile_cvt(samples, 4, 4, 0)) ==  0.5f);
	Test_assert(t, "f32  2.0 -> +1.0",     F32_fromU32Bits((U32)WAVFile_cvt(samples, 4, 4, 1)) ==  1.0f);
	Test_assert(t, "f32 -2.0 -> -1.0",     F32_fromU32Bits((U32)WAVFile_cvt(samples, 4, 4, 2)) == -1.0f);
}

//F32 -> U8: full-scale mapping
void Test_WAVCvtF32ToU8(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F32 -> U8 full-scale");

	F32 samples[3] = { -1, 0, 1 };

	Test_assert(t, "f32 -1 -> u8 0",   WAVFile_cvt(samples, 4, 1, 0) == 0);
	Test_assert(t, "f32  0 -> u8 127", WAVFile_cvt(samples, 4, 1, 1) == 127);
	Test_assert(t, "f32 +1 -> u8 255", WAVFile_cvt(samples, 4, 1, 2) == 255);
}

//F32 -> I16: 0.0 maps to 0
void Test_WAVCvtF32ToI16(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F32 -> I16 midpoint");

	F32 mid[1] = { 0.0f };
	Test_assert(t, "f32 0 -> i16 0", (I16)WAVFile_cvt(mid, 4, 2, 0) == 0);
}

//F64 -> F32 downcast
void Test_WAVCvtF64ToF32(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F64 -> F32 downcast");

	F64 samples[2] = { 0.5, 2 };

	F32 out0 = F32_fromU32Bits((U32)WAVFile_cvt(samples, 8, 4, 0));
	Test_assert(t, "f64 0.5 -> f32 ~= 0.5", out0 > 0.4999f && out0 < 0.5001f);

	F32 out1 = F32_fromU32Bits((U32)WAVFile_cvt(samples, 8, 4, 1));
	Test_assert(t, "f64 2.0 clamped -> f32 1.0", out1 == 1.0f);
}

//F64 -> F64 identity
void Test_WAVCvtF64Identity(Test *t) {

	Test_setModule(t, "WAVFile_cvt: F64 -> F64 identity");

	F64 samples[1] = { -0.75 };
	F64 out = F64_fromU64Bits(WAVFile_cvt(samples, 8, 8, 0));
	Test_assert(t, "f64 -0.75 identity", out > -0.750001 && out < -0.749999);
}

//Index offset: reads the correct sample when i > 0
void Test_WAVCvtIndexOffset(Test *t) {

	Test_setModule(t, "WAVFile_cvt: index offset");

	U16 buf[2] = { 0x0010, 0x0020 };
	Test_assert(t, "i16 index 0", WAVFile_cvt(buf, 2, 2, 0) == 0x0010);
	Test_assert(t, "i16 index 1", WAVFile_cvt(buf, 2, 2, 1) == 0x0020);
}

//WAVFile_avg, pure math, no I/O

void Test_WAVAvgU8(Test *t) {

	Test_setModule(t, "WAVFile_avg: U8");

	Test_assert(t, "u8 (10+20)/2 = 15",  WAVFile_avg(10,  20,  1) ==  15);
	Test_assert(t, "u8 (0+0)/2   = 0",   WAVFile_avg(0,   0,   1) ==   0);
	Test_assert(t, "u8 (0+255)/2 = 127", WAVFile_avg(0,   255, 1) == 127);
	Test_assert(t, "u8 (255+255) = 255", WAVFile_avg(255, 255, 1) == 255);
}

void Test_WAVAvgI16(Test *t) {

	Test_setModule(t, "WAVFile_avg: I16");

	Test_assert(t, "i16 (0 + 0) / 2 = 0",        (I16)WAVFile_avg(0, 0, 2) == 0);
	Test_assert(t, "i16 equal values",           (I16)WAVFile_avg((U16)500, (U16)500, 2) == 500);
	Test_assert(t, "i16 symmetric around zero",  (I16)WAVFile_avg((U16)1000, (U16)(U16)(-1000), 2) == 0);
}

void Test_WAVAvgF32(Test *t) {

	Test_setModule(t, "WAVFile_avg: F32");

	U64 a = U32_fromF32Bits(-0.5f);
	U64 b = U32_fromF32Bits( 0.5f);
	F32 r = F32_fromU32Bits((U32)WAVFile_avg(a, b, 4));
	Test_assert(t, "f32 (-0.5 + 0.5) / 2 = 0", r > -1e-6f && r < 1e-6f);

	a = U32_fromF32Bits(0.25f);
	b = U32_fromF32Bits(0.75f);
	r = F32_fromU32Bits((U32)WAVFile_avg(a, b, 4));
	Test_assert(t, "f32 (0.25 + 0.75) / 2 = 0.5", r > 0.4999f && r < 0.5001f);
}

void Test_WAVAvgF64(Test *t) {

	Test_setModule(t, "WAVFile_avg: F64");

	U64 a = U64_fromF64Bits(1.0);
	U64 b = U64_fromF64Bits(3.0);
	F64 r = F64_fromU64Bits(WAVFile_avg(a, b, 8));
	Test_assert(t, "f64 (1 + 3) / 2 = 2", r > 1.9999999 && r < 2.0000001);
}

void Test_WAVAvgI24(Test *t) {

	Test_setModule(t, "WAVFile_avg: I24");

	Test_assert(t, "i24 (100 + 200) / 2 = 150", (WAVFile_avg(100, 200, 3) & 0xFFFFFF) == 150u);

	U64 neg1000  = (U64)((U32)(-(I32)1000) & 0xFFFFFF);
	U64 r        = WAVFile_avg(1000, neg1000, 3);
	I32 signed_r = (I32)((r & 0x800000) ? (r | 0xFF000000) : r);
	Test_assert(t, "i24 (+1000 + -1000)/2 = 0", signed_r == 0);
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
	const RefPtrType *type,
	StreamRef **archiveSr,
	WAVFile *result
) {
	if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, archiveSr, &t->err))
		return false;

	if (!WAV_write(*archiveSr, inputSr, 0, inputOff, dataLen, isStereo, freq, stride, NULL, t->alloc, &t->err)) {
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

	const U32  freq       = 44100;
	const U16  stride     = 16;
	const U64  numSamples = 512;
	const U64  dataLen    = numSamples * 2 * (stride >> 3);

	{
		if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
			Test_assert(t, "build input stream", false);
			goto doneStereo16;
		}

		WAVFile result = { 0 };
		Test_assert(t, "stereo16 round-trip",    wavRoundTrip(t, inputSr, 0, dataLen, true, freq, stride, &type, &archiveSr, &result));
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

	const U32 freq    = 48000;
	const U16 stride  = 32;
	const U64 nSamples = 256;
	const U64 dataLen  = nSamples * (stride >> 3);

	{
		if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
			Test_assert(t, "build f32 stream", false);
			goto doneMonoF32;
		}

		WAVFile result = { 0 };
		Test_assert(t, "monoF32 round-trip", wavRoundTrip(t, inputSr, 0, dataLen, false, freq, stride, &type, &archiveSr, &result));
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

	{
		if (!makeSampleStream(t, NULL, 256, &type, &inputSr)) {
			Test_assert(t, "build 8bit stream", false);
			goto doneMono8;
		}

		WAVFile result = { 0 };
		Test_assert(t, "mono8 round-trip", wavRoundTrip(t, inputSr, 0, 256, false, 44100, 8, &type, &archiveSr, &result));
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

	{
		if (!makeSampleStream(t, NULL, dataLen, &type, &inputSr)) {
			Test_assert(t, "build f64 stream", false);
			goto doneMono64;
		}

		WAVFile result = { 0 };
		Test_assert(t, "mono64 round-trip", wavRoundTrip(t, inputSr, 0, dataLen, false, 192000, 64, &type, &archiveSr, &result));
		Test_assert(t, "mono64 freq",       result.fmt.frequency == 192000);
		Test_assert(t, "mono64 stride",     result.fmt.stride    == 64);
		Test_assert(t, "mono64 format",     result.fmt.format    == ERIFFAudioFormat_IEEE754);
	}

doneMono64:
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

	{
		if (!makeSampleStream(t, dummy, 4, &type, &inputSr)) {
			Test_assert(t, "build dummy stream", false);
			goto doneInvalidFreq;
		}
		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create archive stream", false);
			goto doneInvalidFreq;
		}
		Test_assert(t, "bad freq fails",
			!WAV_write(archiveSr, inputSr, 0, 0, 4, false, 12345, 16, NULL, t->alloc, NULL)
		);
	}

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

	{
		if (!makeSampleStream(t, dummy, 4, &type, &inputSr)) {
			Test_assert(t, "build dummy stream", false);
			goto doneInvalidStride;
		}
		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create archive stream", false);
			goto doneInvalidStride;
		}
		Test_assert(t, "bad stride fails",
			!WAV_write(archiveSr, inputSr, 0, 0, 4, false, 44100, 12, NULL, t->alloc, NULL)
		);
	}

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

	{
		if (!makeSampleStream(t, NULL, 5, &type, &inputSr)) {
			Test_assert(t, "build dummy stream", false);
			goto doneUnaligned;
		}
		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &archiveSr, &t->err)) {
			Test_assert(t, "create archive stream", false);
			goto doneUnaligned;
		}
		Test_assert(t, "unaligned length fails",
			!WAV_write(archiveSr, inputSr, 0, 0, 5, true, 44100, 16, NULL, t->alloc, NULL)
		);
	}

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

	{
		if (!MemoryStream_create(128, EMemoryStreamFlags_IsWritable, &type, &sr, &t->err)) {
			Test_assert(t, "create zeroed stream", false);
			goto doneInvalidMagic;
		}

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

	{
		if (!makeSampleStream(t, tiny, 4, &type, &sr)) {
			Test_assert(t, "build tiny stream", false);
			goto doneTruncated;
		}

		WAVFile result = { 0 };
		Test_assert(t, "truncated stream fails", !WAV_read(sr, 0, 0, t->alloc, &result, NULL));
	}

doneTruncated:
	RefPtr_dec(&sr);
}

//WAVFile_convert

//Stereo I16 averaged to mono I16. All pairs have equal L and R so the averaged output equals either channel.
void Test_WAVConvertStereoToMono(Test *t) {

	Test_setModule(t, "WAVFile_convert: stereo I16 -> mono I16 average");
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	StreamRef *inputSr  = NULL;
	StreamRef *outputSr = NULL;

	//L and R are distinct so only the true average produces the expected result.
	//avg(200, 100) = 150, avg(-100, -300) = -200, avg(0, 400) = 200, avg(1000, -1000) = 0
	//Left-only would give  200, -100,     0,   1000, all wrong except sample[3].
	//Right-only would give 100, -300,   400,  -1000, all wrong.

	I16 stereo[8] = { 200, 100,  -100, -300,  0, 400,  1000, -1000 };
	const U64 srcLen = sizeof(stereo);

	{
		if (!makeSampleStream(t, stereo, srcLen, &type, &inputSr)) {
			Test_assert(t, "build stereo stream", false); goto doneS2M;
		}
		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
			Test_assert(t, "create output stream", false); goto doneS2M;
		}

		WAVConversionInfo info = {
			.format       = EAudioFormat_WAV,
			.splitType    = ESplitType_Average,
			.oldByteCount = 0x82,   //bit7 = stereo, low7 = 2 bytes per channel
			.newByteCount = 2
		};

		Bool ok = WAVFile_convert(inputSr, 0, srcLen, outputSr, 0, info, 44100, true, t->alloc, &t->err);
		Test_assert(t, "convert ok", ok);

		if (ok) {
			WAVFile wf = { 0 };
			if (Test_assert(t, "read back converted", WAV_read(outputSr, 0, 0, t->alloc, &wf, &t->err))) {
				Test_assert(t, "mono channels", wf.fmt.channels == 1);
				Test_assert(t, "mono dataLen",  wf.dataLength   == 4 * 2);   // 4 samples x 2 bytes

				MemoryStream *s = RefPtr_data(outputSr, MemoryStream);
				const I16 *out = (const I16*)(s->data.ptr + wf.dataStart);

				Test_assert(t, "avg sample[0] ==  150", out[0] == 150);
				Test_assert(t, "avg sample[1] == -200", out[1] == -200);
				Test_assert(t, "avg sample[2] ==  200", out[2] == 200);
				Test_assert(t, "avg sample[3] ==    0", out[3] == 0);
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

	//L = 1000, R = -1000, left split should give +1000
	I16 stereo[2] = { 1000, -1000 };

	{
		if (!makeSampleStream(t, stereo, sizeof(stereo), &type, &inputSr)) {
			Test_assert(t, "build stereo stream", false);
			goto doneLeft;
		}

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
			Test_assert(t, "create output stream", false);
			goto doneLeft;
		}

		WAVConversionInfo info = {
			.format       = EAudioFormat_WAV,
			.splitType    = ESplitType_Left,
			.oldByteCount = 0x82,
			.newByteCount = 2
		};

		Bool ok = WAVFile_convert(inputSr, 0, sizeof(stereo), outputSr, 0, info, 44100, true, t->alloc, &t->err);
		Test_assert(t, "left split ok", ok);

		if (ok) {
			WAVFile wf = { 0 };

			if (Test_assert(t, "read back left", WAV_read(outputSr, 0, 0, t->alloc, &wf, &t->err))) {

				Test_assert(t, "left mono channels", wf.fmt.channels == 1);

				MemoryStream *s = RefPtr_data(outputSr, MemoryStream);
				const I16 *out = (const I16*)(s->data.ptr + wf.dataStart);
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

	{
		if (!makeSampleStream(t, NULL, 3, &type, &inputSr)) {
			Test_assert(t, "build misaligned stream", false);
			goto doneMisaligned;
		}

		if (!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &outputSr, &t->err)) {
			Test_assert(t, "create output stream", false);
			goto doneMisaligned;
		}

		WAVConversionInfo info = {
			.format       = EAudioFormat_WAV,
			.splitType    = ESplitType_Untouched,
			.oldByteCount = 0x02,   //mono, 2 bytes
			.newByteCount = 2
		};

		Test_assert(t, "misaligned srcLen fails",
			!WAVFile_convert(inputSr, 0, 3, outputSr, 0, info, 44100, false, t->alloc, NULL)
		);
	}

doneMisaligned:
	RefPtr_dec(&inputSr);
	RefPtr_dec(&outputSr);
}
