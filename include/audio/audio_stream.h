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

//audio/audio_stream.h

#pragma once
#include "types/container/ref_ptr.h"
#include "types/container/stream.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Error Error;
typedef struct Allocator Allocator;
typedef struct CharString CharString;

typedef struct AudioStreamInfo AudioStreamInfo;

//Stride bytes has one exception; stride of 5 corresponds with 24-bit, as this is common but will have to be remapped.
//Otherwise it's 2^n, 0 = 1B (8b), 1 = 2B (16b), 2 = 4 (32b), 3 = 8 (64b). 4 = 3 (24b), 5 = 4 (32b, PCM).

#define EAudioStreamFormat_remapStride(x) \
	((x) == 1 ? 0 : ((x) == 2 ? 1 : ((x) == 4 ? 2 : ((x) == 3 || (x) == 5 ? 4 : 3))))

#define EAudioStreamFormat_create(channels, strideBytes) \
	((EAudioStreamFormat_remapStride(strideBytes) << 1) | ((channels - 1) & 1))

typedef enum EAudioStreamFormat {		//1b channels, 2b (1 << x = strideBytes), 4 bit max.

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

	EAudioStreamFormat_Mono24Ext,				//PCM24 mono
	EAudioStreamFormat_Stereo24Ext,				//PCM24 stereo

	EAudioStreamFormat_Mono32Ext,				//PCM32 mono
	EAudioStreamFormat_Stereo32Ext,				//PCM32 stereo

	EAudioStreamFormat_Count,

	EAudioStreamFormat_FloatStart	= EAudioStreamFormat_Mono32fExt,
	EAudioStreamFormat_FloatEnd		= EAudioStreamFormat_Stereo64fExt

} EAudioStreamFormat;

typedef U8 AudioStreamFormat;

static inline U8 EAudioStreamFormat_getChannels(EAudioStreamFormat format) {
	return (U8)(format & 1) + 1;
}

static inline U8 EAudioStreamFormat_getStrideBytes(EAudioStreamFormat format) {

	if ((format >> 1) > 4)	//U32
		return 4;

	if ((format >> 1) == 4)	//U24
		return 3;

	return 1 << (U8)(format >> 1);
}

static inline U8 EAudioStreamFormat_getSize(EAudioStreamFormat format) {
	return EAudioStreamFormat_getChannels(format) * EAudioStreamFormat_getStrideBytes(format);
}

typedef enum EAudioStreamInfoFlags {			//4-bit flags
	EAudioStreamInfoFlags_None			= 0,
	EAudioStreamInfoFlags_IsLoop		= 1 << 0,
	EAudioStreamInfoFlags_FlattenSound	= 1 << 1	//Force stereo sound into mono, required for 3D spatial audio if stereo
} EAudioStreamInfoFlags;

typedef struct AudioStreamInfo {

	F32 pitch;
	U8 dataLengthHi8;
	U8 flags4_format4;			//EAudioStreamInfoFlags, AudioStreamFormat
	U16 loops;					//0: infinite, otherwise how many times a loop is permitted for a stream

	Ns duration;

	U64 dataStart;

	U32 sampleRate;
	U32 dataLengthLo32;

	U32 streamLength;			//0 = bytesPerSecond, else must be >=64KiB
	U32 bytesPerSecond;

	StreamRef *stream;

} AudioStreamInfo;

static inline EAudioStreamInfoFlags AudioStreamInfo_flags(const AudioStreamInfo *info) {
	return info ? (info->flags4_format4 & 0xF) : EAudioStreamInfoFlags_None;
}

static inline EAudioStreamFormat AudioStreamInfo_format(const AudioStreamInfo *info) {
	return info ? (info->flags4_format4 >> 4) : EAudioStreamFormat_Count;
}

static inline Bool AudioStreamInfo_isLoop(const AudioStreamInfo *info) {
	return info && (info->flags4_format4 & EAudioStreamInfoFlags_IsLoop);
}

static inline Bool AudioStreamInfo_isInfiniteLoop(const AudioStreamInfo *info) {
	return info && (info->flags4_format4 & EAudioStreamInfoFlags_IsLoop) && !info->loops;
}

static inline Bool AudioStreamInfo_flattenSound(const AudioStreamInfo *info) {
	return info && (info->flags4_format4 & EAudioStreamInfoFlags_FlattenSound);
}

static inline U64 AudioStreamInfo_dataLength(const AudioStreamInfo *info) {
	return info ? (info->dataLengthLo32 | ((U64)info->dataLengthHi8 << 32)) : 0;
}

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

static inline AudioStream *AudioStreamRef_ptr(AudioStreamRef *ptr) { return RefPtr_data(ptr, AudioStream); }
static inline void *AudioStream_extVoid(AudioStream *src) { return !src ? NULL : (src + 1); }

#define AudioStream_ext(ptr, T) (T##AudioStream*)(AudioStream_extVoid(ptr))

RefPtrType AudioStream_makeType(const Allocator *alloc);

Bool AudioDeviceRef_createStream(
	AudioDeviceRef *device,
	AudioStreamInfo *info,				//Takes ownership of info
	Ns startOffset,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioStreamRef **stream,
	Error *e_rr
);

Bool AudioDeviceRef_createFromFile(		//Detect stream by stream file header (currently only wav supported)
	AudioDeviceRef *device,
	StreamRef *inputStream,
	U64 inputStreamOffset,
	U16 loops,							//0 = infinite loops, 1 = normal, else how many times it will loop
	Ns startOffset,
	F32 pitch,
	Bool flattenSound,					//Required for spatial audio if the source is stereo
	const Allocator *alloc,
	const RefPtrType *type,
	AudioStreamRef **stream,
	Error *e_rr
);

Bool AudioStreamRef_seekTime(AudioStreamRef *stream, Ns offset, Error *e_rr);

Bool AudioStreamRef_play(AudioStreamRef *stream, const Allocator *alloc, Error *e_rr);
Bool AudioStreamRef_stop(AudioStreamRef *stream, Error *e_rr);

#ifdef __cplusplus
	}
#endif
