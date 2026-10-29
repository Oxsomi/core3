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

//With dataOutput the caller writes the data itself at *dataOutput; the pad byte of an odd length is already written behind it.
//Lengths that don't fit the U32 RIFF sizes (with the headers and the pad) are refused.
Bool WAV_write(
	StreamRef *stream,
	StreamRef *inputStream,
	U64 outputStreamOffset,
	U64 inputStreamOffset,
	U64 streamLength,        //0 = remainder of input stream; length of output data
	Bool isStereo,
	U32 freq,                //Positive sample rate; freq * bytesPerBlock must fit U32
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

//off and len select the file in the stream (len 0 is the rest of the stream); bytes after the RIFF chunk are ignored.
//dataStart is an absolute stream offset.
//Unknown RIFF chunks are skipped. Any positive sample rate with a consistent byte rate is accepted.
//fmt is PCM or IEEE float, or WAVE_FORMAT_EXTENSIBLE with the matching subformat. For extensible the container (stride)
// decides how a sample is read; fewer valid bits than the stride (24 in 32) means the sample is MSB justified in it,
// which reads correctly as a stride sized sample, so it is accepted and only reported as the stride.
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

//Converts srcLen bytes. Byte counts have to be 1, 2, 3, 4 or 8 with the pcm flag matching (1 to 3: PCM, 8: float, 4: either),
// splitType needs a stereo source and srcLen a multiple of the source's block size.
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

//Converts one sample (index i, in samples of ogStride bytes) to newStride bytes, returned as a little endian integer.
//Strides 1 to 3 are PCM (1 is unsigned with a 0x80 bias, 2 and 3 are signed), 4 is PCM or F32 (pcm and newPcm say which)
// and 8 is F64. Any other stride returns 0. The source can be at any alignment.
//Float -> PCM clamps (NaN is 0) and scales by 2^(N - 1) - 1 with rounding to nearest, so -1, 0 and 1 are -max, 0 and max.
//PCM -> float divides by the same scale (max is exactly 1, the most negative value clamps to -1).
//PCM -> PCM drops the low bits (floor) or fills them with zeros.
U64 WAVFile_cvt(const void *cvt, U8 ogStride, U8 newStride, U64 i, Bool pcm, Bool newPcm);

//Average two values (preserving encoding, signed values floor), 'pcm' is only used to distinguish ambiguous formats
U64 WAVFile_avg(U64 a, U64 b, U64 stride, Bool pcm);

#ifdef __cplusplus
	}
#endif
