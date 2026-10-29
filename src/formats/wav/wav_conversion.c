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

//formats/wav/wav_conversion.c

#include "formats/wav/wav_file.h"
#include "formats/wav/wav_headers.h"
#include "types/container/buffer.h"
#include "types/container/stream.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/base/constants.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/mathf.h"
#include "types/base/buffer_base.h"
#include "types/math/type_cast.h"

//Sample model: strides 1, 2 and 3 are PCM (1 is unsigned with a 0x80 bias, 2 and 3 are signed), 4 is PCM or F32 depending on
// the pcm flag and 8 is F64. A PCM sample of N bits spans [-2^(N-1), 2^(N-1) - 1] and a float sample [-1, 1].
//Float -> PCM clamps (NaN becomes 0), scales by 2^(N-1) - 1 and rounds to nearest (ties away from zero), so -1, 0 and 1
// become -max, 0 and max; PCM -> float divides by that same scale, so max is exactly 1 and the most negative value
// clamps to -1. PCM -> PCM shifts: a narrower target drops the low bits (floor), a wider one fills them with zero.

static Bool WAVFile_validStride(U8 stride) {
	return stride == 1 || stride == 2 || stride == 3 || stride == 4 || stride == 8;
}

U64 WAVFile_cvt(const void *cvt, U8 ogStride, U8 newStride, U64 i, Bool pcm, Bool newPcm) {

	if(!cvt || !WAVFile_validStride(ogStride) || !WAVFile_validStride(newStride))
		return 0;

	const U8 *p = (const U8*) cvt + i * ogStride;

	U64 raw = 0;

	for (U8 j = 0; j < ogStride; ++j)
		raw |= (U64) p[j] << (j << 3);

	Bool srcFloat = ogStride == 8 || (ogStride == 4 && !pcm);
	Bool dstFloat = newStride == 8 || (newStride == 4 && !newPcm);

	U8 srcBits = ogStride << 3;
	U8 dstBits = newStride << 3;

	//Decode

	F64 f = 0;
	I64 n = 0;

	if (srcFloat) {
		f = ogStride == 4 ? (F64) F32_fromU32Bits((U32) raw) : F64_fromU64Bits(raw);
		f = F64_isNaN(f) ? 0 : F64_clamp(f, -1, 1);
	}

	//Signed PCM: subtract 2^N when bit N - 1 is set to recover the signed N-bit value.
	//8-bit PCM is unsigned with a 0x80 bias instead.

	else switch (ogStride) {

		case 1:
			n = (I64) (raw & 0xFF) - 0x80;
			break;

		case 2:
			n = (I64) (raw & 0xFFFF) - (I64) ((raw & 0x8000) << 1);
			break;

		case 3:
			n = (I64) (raw & 0xFFFFFF) - (I64) ((raw & 0x800000) << 1);
			break;

		default:
			n = (I64) (raw & U32_MAX) - (I64) ((raw & 0x80000000) << 1);
			break;
	}

	//Float or PCM -> float

	if (dstFloat) {

		if(!srcFloat)
			f = F64_max((F64) n / (F64) ((1ULL << (srcBits - 1)) - 1), -1);

		if (newStride == 4)
			return U32_fromF32Bits((F32) f);

		return U64_fromF64Bits(f);
	}

	//Float -> PCM

	if (srcFloat) {
		F64 t = f * (F64) ((1ULL << (dstBits - 1)) - 1);
		n = (I64) (t < 0 ? t - 0.5 : t + 0.5);
	}

	//PCM -> PCM

	else if(dstBits < srcBits)
		n >>= srcBits - dstBits;

	//Multiplication supports negative samples; left-shifting a negative signed value is undefined.
	else n *= (I64) 1 << (dstBits - srcBits);

	if(newStride == 1)
		return (U64) (n + 0x80) & 0xFF;

	return (U64) n & (((U64) 1 << dstBits) - 1);
}

U64 WAVFile_avg(U64 a, U64 b, U64 newStride, Bool pcm) {

	switch (newStride) {

		case 4: {

			if (pcm) {

				U64 x = (a & U32_MAX) ^ 0x80000000;        //Sign bias, so the sum is unsigned and the floor is exact
				U64 y = (b & U32_MAX) ^ 0x80000000;

				return (((x + y) >> 1) ^ 0x80000000) & U32_MAX;
			}

			F32 avg = (F32_fromU32Bits((U32) a) + F32_fromU32Bits((U32) b)) / 2;
			return U32_fromF32Bits(avg);
		}

		case 8: {
			F64 avg = (F64_fromU64Bits(a) + F64_fromU64Bits(b)) / 2;
			return U64_fromF64Bits(avg);
		}

		case 3: {

			U64 x = (a & U24_MAX) ^ 0x800000;
			U64 y = (b & U24_MAX) ^ 0x800000;

			return (((x + y) >> 1) ^ 0x800000) & U24_MAX;
		}

		case 2: {

			U64 x = (a & U16_MAX) ^ 0x8000;
			U64 y = (b & U16_MAX) ^ 0x8000;

			return (((x + y) >> 1) ^ 0x8000) & U16_MAX;
		}

		case 1:
		default:
			return ((a & U8_MAX) + (b & U8_MAX)) / 2;        //Unsigned
	}
}

Bool WAVFile_convert(
	StreamRef *inputStreamRef,
	U64 srcOff,
	U64 srcLen,
	StreamRef *outputStreamRef,
	U64 dstOff,
	WAVConversionInfo info,
	U32 freq,
	Bool writeHeader,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	StreamCursor cursorWrite = (StreamCursor) { 0 };
	StreamCursor cursorRead = (StreamCursor) { 0 };

	Bool stereo = info.oldByteCountStereoPcm >> 7;
	Bool pcm = info.oldByteCountStereoPcm & (1 << 6);
	U8 oldByteCount = info.oldByteCountStereoPcm & 0x3F;

	Bool newPcm = info.newByteCountStereoPcm & (1 << 6);
	U8 newByteCount = info.newByteCountStereoPcm & 0x3F;

	//Strides 1 to 3 are PCM, 8 is float and 4 is either; the flag has to say which

	if(info.format != EAudioFormat_WAV)
		retError(clean, Error_invalidParameter(5, 0, "WAVFile_convert()::info.format is unsupported"));

	if(info.splitType >= ESplitType_Count)
		retError(clean, Error_invalidParameter(5, 1, "WAVFile_convert()::info.splitType is invalid"));

	if(info.splitType && !stereo)
		retError(clean, Error_invalidParameter(5, 2, "WAVFile_convert()::info.splitType requires a stereo source"));

	if(!WAVFile_validStride(oldByteCount) || !WAVFile_validStride(newByteCount))
		retError(clean, Error_invalidParameter(5, 3, "WAVFile_convert()::info byte count must be 1, 2, 3, 4 or 8"));

	if (
		(oldByteCount < 4 && !pcm) || (oldByteCount == 8 && pcm) ||
		(newByteCount < 4 && !newPcm) || (newByteCount == 8 && newPcm)
	)
		retError(clean, Error_invalidParameter(5, 4, "WAVFile_convert()::info pcm flag doesn't match the byte count"));

	if(srcLen > U64_MAX - srcOff)
		retError(clean, Error_outOfBounds(2, srcOff, srcLen, "WAVFile_convert()::srcOff + srcLen overflows"));

	gotoIfError3(clean, StreamCursor_create(inputStreamRef, 0, false, alloc, &cursorRead, e_rr));
	gotoIfError3(clean, StreamCursor_create(outputStreamRef, 0, true, alloc, &cursorWrite, e_rr));

	U8 oldBytesPerStep = oldByteCount;

	if(info.splitType)
		oldBytesPerStep <<= 1;

	U64 srcEnd = srcOff + srcLen;

	U64 left = 0, right = left;

	switch (info.splitType) {
		default:                                           break;
		case ESplitType_Right:        right = left = 1;    break;
		case ESplitType_Average:      right = 1;           break;
	}

	if(srcLen % (stereo ? oldByteCount << 1 : oldByteCount))
		retError(clean, Error_invalidState(0, "WAVFile_convert() mismatches srcLen with bytes per block"));

	if(writeHeader) {

		U64 newStreamLen = srcLen / oldBytesPerStep * newByteCount;

		gotoIfError3(clean, WAV_write(
			outputStreamRef,
			NULL,
			dstOff,
			0,
			newStreamLen,
			stereo && !info.splitType,
			freq,
			newByteCount << 3,
			newPcm,
			&dstOff,
			alloc,
			e_rr
		));
	}

	for(; srcOff < srcEnd; srcOff += oldBytesPerStep, dstOff += newByteCount) {

		U8 tmp[16];
		gotoIfError3(clean, StreamCursor_read(
			&cursorRead, Buffer_createRef(tmp, sizeof(tmp)), srcOff, 0, oldBytesPerStep, false, alloc, e_rr
		));

		U64 converted = WAVFile_cvt(tmp, oldByteCount, newByteCount, left, pcm, newPcm);

		if (right != left) {
			U64 convertedRight = WAVFile_cvt(tmp, oldByteCount, newByteCount, right, pcm, newPcm);
			converted = WAVFile_avg(converted, convertedRight, newByteCount, newPcm);
		}

		U8 cvtBytes[8];        //Little endian, whatever the host is

		for (U8 i = 0; i < newByteCount; ++i)
			cvtBytes[i] = (U8) (converted >> (i << 3));

		Buffer cvt = Buffer_createRefConst(cvtBytes, newByteCount);

		gotoIfError3(clean, StreamCursor_write(&cursorWrite, cvt, 0, dstOff, newByteCount, false, alloc, e_rr));
	}

	gotoIfError3(clean, StreamCursor_flush(&cursorWrite, alloc, e_rr));

clean:
	StreamCursor_close(&cursorWrite, alloc);
	StreamCursor_close(&cursorRead, alloc);
	return s_uccess;
}
