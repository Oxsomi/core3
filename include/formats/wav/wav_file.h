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

//formats/wav/wav_file.h

#pragma once
#include "types/base/types.h"
#include "formats/wav/wav_headers.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;
typedef struct Allocator Allocator;
typedef struct Error Error;

Bool WAV_write(
	StreamRef *stream,
	StreamRef *inputStream,
	U64 outputStreamOffset,
	U64 inputStreamOffset,
	U64 streamLength,        //0 = remainder of input stream; length of output data
	Bool isStereo,
	U32 freq,                //8KHz, 11.025KHz, 22.05KHz, 32KHz, 44.1KHz, 48KHz, 96KHz, 192KHz
	U16 stride,              //8, 16, 24 (PCM), 32 (PCM or Float), 64 (Float)
	Bool isPcm,              //To distinguish 32-bit
	U64 *dataOutput,         //If NULL, will directly output the data to stream, otherwise points to the offset of the data
	const Allocator *alloc,
	Error *e_rr
);

//Only used through read, don't cast directly.
//To avoid loading the whole WAV, a stream is used to find all these sections.
typedef struct WAVFile {
	RIFFFmtHeader fmt;
	U32 dataLength;
	U64 dataStart;
} WAVFile;

Bool WAV_read(StreamRef *stream, U64 off, U64 len, const Allocator *allocator, WAVFile *result, Error *e_rr);

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
	SplitType splitType;         //Only possible if isStereo, bit below: pcm
	U8 oldByteCountStereoPcm;    //Upper bit indicates 'isStereo', bit below: pcm
	U8 newByteCountStereoPcm;
} WAVConversionInfo;

Bool WAVFile_convert(
	StreamRef *inputStream,
	U64 srcOff,
	U64 srcLen,
	StreamRef *outputStream,
	U64 dstOff,
	WAVConversionInfo info,
	U32 freq,                    //Must match input
	Bool writeHeader,
	const Allocator *alloc,
	Error *e_rr
);

//Assumes correct alignment, mostly called internally, 'pcm' is only used to distinguish ambiguous formats
U64 WAVFile_cvt(const void *cvt, U8 ogStride, U8 newStride, U64 i, Bool pcm, Bool newPcm);

//Average two values (preserving encoding), 'pcm' is only used to distinguish ambiguous formats
U64 WAVFile_avg(U64 a, U64 b, U64 stride, Bool pcm);

#ifdef __cplusplus
	}
#endif
