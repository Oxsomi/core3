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

#pragma once
#include "types/container/string.h"
#include "platforms/file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef struct Error Error;
typedef struct Allocator Allocator;

typedef RefPtr AudioStreamRef;

typedef struct AudioStreamInfo AudioStreamInfo;

//Stride bytes has one exception; stride of 5 corresponds with 24-bit, as this is common but will have to be remapped.
//Otherwise it's 2^n, 0 = 1B (8b), 1 = 2B (16b), 2 = 4 (32b), 3 = 8 (64b). 4 = 3 (24b).

#define EAudioStreamFormat_remapStride(x) ((x) == 1 ? 0 : ((x) == 2 ? 1 : ((x) == 4 ? 2 : ((x) == 3 ? 4 : 3))))
#define EAudioStreamFormat_create(channels, strideBytes) \
	((EAudioStreamFormat_remapStride(strideBytes) << 1) | ((channels - 1) & 1))

typedef enum EAudioStreamFormat {		//1b channels, 2b (1 << x = strideBytes)

	EAudioStreamFormat_Mono8,
	EAudioStreamFormat_Stereo8,

	EAudioStreamFormat_Mono16,
	EAudioStreamFormat_Stereo16,

	//Extended formats; these may fallback to CPU processing if unsupported by the implementation.
	//E.g. The AudioStream or AudioBuffer will discard this information.

	EAudioStreamFormat_Mono32fExt,
	EAudioStreamFormat_Stereo32fExt,
	
	EAudioStreamFormat_Mono64fExt,
	EAudioStreamFormat_Stereo64fExt,

	EAudioStreamFormat_Mono24Ext,
	EAudioStreamFormat_Stereo24Ext,

	EAudioStreamFormat_Count

} EAudioStreamFormat;

typedef U8 AudioStreamFormat;

U8 EAudioStreamFormat_getChannels(EAudioStreamFormat format);
U8 EAudioStreamFormat_getStrideBytes(EAudioStreamFormat format);
U8 EAudioStreamFormat_getSize(EAudioStreamFormat format);

typedef struct AudioStreamInfo {

	F32 pitch;
	U8 padding;
	Bool flattenSound;			//Force stereo sound into mono, required for 3D spatial audio if stereo
	AudioStreamFormat format;	//The format in the stream
	Bool isLoop;

	Ns duration;

	U64 dataStart;

	U32 sampleRate;
	U32 dataLength;

	U32 streamLength;			//0 = bytesPerSecond, else must be >=64KiB
	U32 bytesPerSecond;

	Stream stream;

} AudioStreamInfo;

typedef RefPtr AudioDeviceRef;

typedef struct AudioStream {

	AudioDeviceRef *device;

	AudioStreamInfo info;

	U64 streamOffset;
	Ns timeOffset;				//[0, duration], only knows the one on start, since the streamOffset is always ahead

	U32 loops;					//How many times the stream has looped, stops at 1 if !isLoop
	Bool isPlaying;
	AudioStreamFormat format;	//The real format that the device is reading. Stereo to mono and/or F32/F64/U24 -> U16
	U8 padding[2];

} AudioStream;

typedef RefPtr AudioStreamRef;

#define AudioStream_ext(ptr, T) (!ptr ? NULL : (T##AudioStream*)(ptr + 1))		//impl
#define AudioStreamRef_ptr(ptr) RefPtr_data(ptr, AudioStream)

Error AudioStreamRef_dec(AudioStreamRef **stream);
Error AudioStreamRef_inc(AudioStreamRef *stream);

typedef RefPtr AudioDeviceRef;

Bool AudioDeviceRef_createStream(
	AudioDeviceRef *device,
	AudioStreamInfo *info,
	Ns startOffset,
	Allocator alloc,
	AudioStreamRef **stream,
	Error *e_rr
);

Bool AudioDeviceRef_createStreamx(
	AudioDeviceRef *device,
	AudioStreamInfo *info,
	Ns startOffset,
	AudioStreamRef **stream,
	Error *e_rr
);

Bool AudioDeviceRef_createFileStream(
	AudioDeviceRef *device,
	CharString path,
	Bool isLoop,
	Ns startOffset,
	F32 pitch,
	Allocator alloc,
	AudioStreamRef **stream,
	Error *e_rr
);

Bool AudioDeviceRef_createFileStreamx(
	AudioDeviceRef *device,
	CharString path,
	Bool isLoop,
	Ns startOffset,
	F32 pitch,
	AudioStreamRef **stream,
	Error *e_rr
);

Bool AudioStreamRef_seekTime(AudioStreamRef *stream, Ns offset, Error *e_rr);

Bool AudioStreamRef_play(AudioStreamRef *stream, Allocator alloc, Error *e_rr);
Bool AudioStreamRef_stop(AudioStreamRef *stream, Error *e_rr);
Bool AudioStreamRef_playx(AudioStreamRef *stream, Error *e_rr);

#ifdef __cplusplus
	}
#endif
