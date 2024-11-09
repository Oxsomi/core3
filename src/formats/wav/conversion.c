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

#include "types/container/list_impl.h"
#include "formats/wav/wav.h"
#include "formats/wav/headers.h"
#include "types/math/math.h"
#include "platforms/file.h"
#include "platforms/platform.h"

//Note: This only handles truncation, it can't handle expansion

U64 WAVFile_cvt(const void *cvt, U8 ogStride, U8 newStride, U64 i) {

	const U8  *cvt8  = (const U8*)  cvt;
	const U16 *cvt16 = (const U16*) cvt;
	const F32 *cvt32 = (const F32*) cvt;
	const F64 *cvt64 = (const F64*) cvt;

	switch (ogStride) {

		case 3:			//Skip first byte(s) to avoid the least significant byte(s)

			i *= 3;

			if(newStride == 3)
				return cvt8[i] | ((U32)cvt8[i + 1] << 8) | ((U32)cvt8[i + 2] << 16);

			else if(newStride == 2)
				return cvt8[i + 1] | ((U16)cvt8[i + 2] << 8);

			return (U8)(cvt8[i + 2] + 0x7F);		//Unsigned normalization

		case 4:	{

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
				default:
					return *(const U32*)&clamped;
			}
		}

		case 8:	{

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
					return *(const U32*)&clampedf;
				}

				case 8:
				default:
					return *(const U64*)&clamped;
			}
		}

		default:
		case 2:

			if(newStride == 2)
				return cvt16[i];

			return (U8) (cvt8[(i << 1) | 1] + 0x7F);		//Unsigned normalization

		case 1:
			return cvt8[i];
	}
}

U64 WAVFile_avg(U64 a, U64 b, U64 newStride) {

	switch (newStride) {

		case 4: {
			F32 avg = (*(const F32*)&a + *(const F32*)&b) / 2;
			return *(const U32*)&avg;
		}

		case 8: {
			F64 avg = (*(const F64*)&a + *(const F64*)&b) / 2;
			return *(const U64*)&avg;
		}

		case 3: {
			a = (a + I24_MAX) & U24_MAX;		//Same as sign cast
			b = (b + I24_MAX) & U24_MAX;
			U64 v = (a + b) / 2;
			return (v - I24_MAX) & U24_MAX;
		}

		case 2: {
			a = (U16)(a + (U16)I16_MAX);		//Same as sign cast
			b = (U16)(b + (U16)I16_MAX);
			U64 v = (a + b) / 2;
			return (U16)(v - (U16)I16_MAX);
		}

		case 1:
		default:
			return (a + b) / 2;
	}
}

Bool WAVFile_convert(
	Stream *inputStream,
	U64 srcOff,
	U64 srcLen,
	Stream *outputStream,
	U64 dstOff,
	WAVConversionInfo info,
	U32 freq,
	Bool writeHeader,
	Allocator alloc,
	Error *e_rr
) {

	(void) alloc;

	Bool s_uccess = true;

	if(!inputStream || !outputStream)
		retError(clean, Error_nullPointer(!inputStream ? 0 : 3, "WAVFile_convert() requires inputStream and outputStream"))

	Bool stereo = info.oldByteCount >> 7;
	U8 oldByteCount = info.oldByteCount & 0x7F;

	U8 oldBytesPerStep = oldByteCount;

	if(info.splitType)
		oldBytesPerStep <<= 1;

	U64 srcEnd = srcOff + srcLen;

	U64 left = 0, right = left;

	if(info.splitType) {
		switch (info.splitType) {
			default:										break;
			case ESplitType_Right:		right = left = 1;	break;
			case ESplitType_Average:	right = 1;			break;
		}
	}

	if(srcLen % (stereo ? oldByteCount << 1 : oldByteCount))
		retError(clean, Error_invalidState(0, "WAVFile_convert() mismatches srcLen with bytes per block"))

	if(writeHeader) {

		U64 newStreamLen = srcLen / oldBytesPerStep * info.newByteCount;

		gotoIfError3(clean, WAV_write(
			outputStream, NULL, dstOff, 0, newStreamLen, stereo && !info.splitType, freq, info.newByteCount << 3, &dstOff, e_rr
		))
	}

	for(; srcOff < srcEnd; srcOff += oldBytesPerStep, dstOff += info.newByteCount) {

		U8 tmp[16];
		gotoIfError3(clean, Stream_read(inputStream, Buffer_createRef(tmp, sizeof(tmp)), srcOff, 0, oldBytesPerStep, false, e_rr))

		U64 converted = WAVFile_cvt(tmp, oldByteCount, info.newByteCount, left);

		if (right != left) {
			U64 convertedRight = WAVFile_cvt(tmp, oldByteCount, info.newByteCount, right);
			converted = WAVFile_avg(converted, convertedRight, info.newByteCount);
		}

		Buffer cvt = Buffer_createRefConst(&converted, info.newByteCount);
		gotoIfError3(clean, Stream_write(outputStream, cvt, 0, dstOff, info.newByteCount, false, e_rr))
	}

clean:
	return s_uccess;
}

Bool WAVFile_convertx(
	Stream *inputStream,
	U64 srcOff,
	U64 srcLen,
	Stream *outputStream,
	U64 dstOff,
	WAVConversionInfo info,
	U32 freq,				//Must match input
	Bool writeHeader,
	Error *e_rr
) {
	return WAVFile_convert(
		inputStream,
		srcOff,
		srcLen,
		outputStream,
		dstOff,
		info,
		freq,				//Must match input
		writeHeader,
		Platform_instance->alloc,
		e_rr
	);
}
