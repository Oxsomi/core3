/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/base/types.h"
#include "formats/wav/headers.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Stream Stream;

Bool WAV_write(
	Stream *stream,
	Stream *inputStream,
	U64 outputStreamOffset,
	U64 inputStreamOffset,
	U64 streamLength,		//0 = remainder of input stream; length of output data
	Bool isStereo,
	U32 freq,				//8KHz, 11.025KHz, 22.05KHz, 32KHz, 44.1KHz, 48KHz, 96KHz, 192KHz
	U16 stride,				//8, 16, 24 (PCM), 32, 64 (Float)
	U64 *dataOutput,		//If NULL, will directly output the data to stream, otherwise points to the offset of the data
	Error *e_rr
);

//Only used through read, don't cast directly.
//To avoid loading the whole WAV, a stream is used to find all these sections.
typedef struct WAVFile {
	RIFFFmtHeader fmt;
	U32 dataLength;
	U64 dataStart;
} WAVFile;

Bool WAV_read(Stream *stream, U64 off, U64 len, Allocator allocator, WAVFile *result, Error *e_rr);
Bool WAV_readx(Stream *stream, U64 off, U64 len, WAVFile *result, Error *e_rr);

typedef enum ESplitType {
	ESplitType_Untouched,
	ESplitType_Left,
	ESplitType_Right,
	ESplitType_Average,
	ESplitType_Count
} ESplitType;

typedef U8 SplitType;

typedef enum EAudioFormat {
	EAudioFormat_WAV
} EAudioFormat;

typedef U8 AudioFormat;

typedef struct WAVConversionInfo {
	AudioFormat format;
	SplitType splitType;		//Only possible if isStereo
	U8 oldByteCount;			//Upper bit indicates 'isStereo'
	U8 newByteCount;
} WAVConversionInfo;

Bool WAVFile_convert(
	Stream *inputStream,
	U64 srcOff,
	U64 srcLen,
	Stream *outputStream,
	U64 dstOff,
	WAVConversionInfo info,
	U32 freq,				//Must match input
	Bool writeHeader,
	Allocator alloc,
	Error *e_rr
);

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
);

#ifdef __cplusplus
	}
#endif
