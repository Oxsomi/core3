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
#include "types/container/types.h"
#include "types/base/constants.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/mathf.h"
#include "types/base/buffer.h"

//Note: This only handles truncation, it can't handle expansion

U64 WAVFile_cvt(const void *cvt, U8 ogStride, U8 newStride, U64 i, Bool pcm, Bool newPcm) {

	const U8  *cvt8  = (const U8*)  cvt;
	const U16 *cvt16 = (const U16*) cvt;
	const U32 *cvt32pcm = (const U32*) cvt;
	const F32 *cvt32 = (const F32*) cvt;
	const F64 *cvt64 = (const F64*) cvt;

	switch (ogStride) {

		case 3:            //Skip first byte(s) to avoid the least significant byte(s)

			i *= 3;

			if(newStride == 3)
				return cvt8[i] | ((U32)cvt8[i + 1] << 8) | ((U32)cvt8[i + 2] << 16);

			else if(newStride == 2)
				return cvt8[i + 1] | ((U16)cvt8[i + 2] << 8);

			return (U8)(cvt8[i + 2] + 0x7F);        //Unsigned normalization

		case 4:    {

			if (pcm) {

				if (newStride == 4 && !newPcm) {
					F32 f = (F32)(I32)cvt32pcm[i] / (F32)I32_MAX;
					const void *fv = (const void*)&f;
					return *(const U32*)fv;
				}

				else if (newStride == 4)
					return cvt32pcm[i];

				else if (newStride == 3) {
					i *= 4;
					return cvt8[i + 1] | ((U32)cvt8[i + 2] << 8) | ((U32)cvt8[i + 3] << 16);
				}

				else if (newStride == 2)
					return cvt16[(i << 1) | 1];

				return (U8)(cvt8[(i << 2) | 3] + 0x7F);
			}

			F32 clamped = F32_clamp(cvt32[i], -1, 1);
			F32 normalized = clamped * 0.5f + 0.5f;

			switch(newStride) {
				
				case 1:
					return (U8)(normalized * U8_MAX);
				
				case 2:
					return (U16)(normalized * U16_MAX) - (U16) I16_MAX;
				
				case 3:
					return ((U32)((F64)normalized * U24_MAX) - I24_MAX) & U24_MAX;

				case 4:
				default: {
					const void *clampedv = (const void*)&clamped;
					return *(const U32*)clampedv;
				}
			}
		}

		case 8:    {

			F64 clamped = F64_clamp(cvt64[i], -1, 1);
			F64 normalized = clamped * 0.5 + 0.5;

			switch(newStride) {
				
				case 1:
					return (U8)(normalized * U8_MAX);
				
				case 2:
					return (U16)(normalized * U16_MAX) - (U16) I16_MAX;
				
				case 3:
					return ((U32)(normalized * U24_MAX) - I24_MAX) & U24_MAX;

				case 4: {
					F32 clampedf = (F32) clamped;
					const void *clampedfv = (const void*)&clampedf;
					return *(const U32*)clampedfv;
				}

				case 8:
				default: {
					const void *clampedv = (const void*)&clamped;
					return *(const U64*)clampedv;
				}
			}
		}

		default:
		case 2:

			if(newStride == 2)
				return cvt16[i];

			return (U8) (cvt8[(i << 1) | 1] + 0x7F);        //Unsigned normalization

		case 1:
			return cvt8[i];
	}
}

U64 WAVFile_avg(U64 a, U64 b, U64 newStride, Bool pcm) {

	const void *av = (const void*)&a;
	const void *bv = (const void*)&b;

	switch (newStride) {

		case 4: {

			if (pcm) {
				a = (a + I32_MAX) & U32_MAX;        //Same as sign cast
				b = (b + I32_MAX) & U32_MAX;
				U64 v = (a + b) / 2;
				return (v - I32_MAX) & U32_MAX;
			}

			F32 avg = (*(const F32*)av + *(const F32*)bv) / 2;
			const void *avgv = (const void*)&avg;
			return *(const U32*)avgv;
		}

		case 8: {
			F64 avg = (*(const F64*)av + *(const F64*)bv) / 2;
			const void *avgv = (const void*)&avg;
			return *(const U64*)avgv;
		}

		case 3: {
			a = (a + I24_MAX) & U24_MAX;        //Same as sign cast
			b = (b + I24_MAX) & U24_MAX;
			U64 v = (a + b) / 2;
			return (v - I24_MAX) & U24_MAX;
		}

		case 2:
			return (U64)(I16)(((I32)(I16)a + (I32)(I16)b) / 2);

		case 1:
		default:
			return (a + b) / 2;
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

	gotoIfError3(clean, StreamCursor_create(inputStreamRef, 0, false, alloc, &cursorRead, e_rr));
	gotoIfError3(clean, StreamCursor_create(outputStreamRef, 0, true, alloc, &cursorWrite, e_rr));

	Bool stereo = info.oldByteCountStereoPcm >> 7;
	Bool pcm = info.oldByteCountStereoPcm & (1 << 6);
	U8 oldByteCount = info.oldByteCountStereoPcm & 0x3F;

	Bool newPcm = info.newByteCountStereoPcm & (1 << 6);
	U8 newByteCount = info.newByteCountStereoPcm & 0x3F;

	U8 oldBytesPerStep = oldByteCount;

	if(info.splitType)
		oldBytesPerStep <<= 1;

	U64 srcEnd = srcOff + srcLen;

	U64 left = 0, right = left;

	if(info.splitType) {
		switch (info.splitType) {
			default:                                        break;
			case ESplitType_Right:        right = left = 1;    break;
			case ESplitType_Average:    right = 1;            break;
		}
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

		Buffer cvt = Buffer_createRefConst(&converted, newByteCount);
		gotoIfError3(clean, StreamCursor_write(&cursorWrite, cvt, 0, dstOff, newByteCount, false, alloc, e_rr));
	}

clean:
	StreamCursor_close(&cursorWrite, alloc);
	StreamCursor_close(&cursorRead, alloc);
	return s_uccess;
}
