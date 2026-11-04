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
//Otherwise it's 2^n, 0 = 1B (8b), 1 = 2B (16b), 2 = 4 (32b), 3 = 8 (64b).
//4 = 3 (24b), 5 = 4 (32b, PCM).

#define EAudioStreamFormat_remapStride(x) \
	((x) == 1 ? 0 : ((x) == 2 ? 1 : ((x) == 4 ? 2 : ((x) == 3 || (x) == 5 ? 4 : 3))))

#define EAudioStreamFormat_create(channels, strideBytes) \
	((EAudioStreamFormat_remapStride(strideBytes) << 1) | ((channels - 1) & 1))

typedef enum EAudioStreamFormat {        //1b channels, 2b (1 << x = strideBytes), 4 bit max.

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

	EAudioStreamFormat_Mono24Ext,                  //PCM24 mono
	EAudioStreamFormat_Stereo24Ext,                //PCM24 stereo

	EAudioStreamFormat_Mono32Ext,                  //PCM32 mono
	EAudioStreamFormat_Stereo32Ext,                //PCM32 stereo

	EAudioStreamFormat_Count,

	EAudioStreamFormat_FloatStart      = EAudioStreamFormat_Mono32fExt,
	EAudioStreamFormat_FloatEnd        = EAudioStreamFormat_Stereo64fExt

} EAudioStreamFormat;

typedef U8 AudioStreamFormat;

static inline U8 EAudioStreamFormat_getChannels(EAudioStreamFormat format) {
	return (U8) (format & 1) + 1;
}

static inline U8 EAudioStreamFormat_getStrideBytes(EAudioStreamFormat format) {

	if ((format >> 1) > 4)     //U32
		return 4;

	if ((format >> 1) == 4)    //U24
		return 3;

	return 1 << (U8) (format >> 1);
}

static inline U8 EAudioStreamFormat_getSize(EAudioStreamFormat format) {
	return EAudioStreamFormat_getChannels(format) * EAudioStreamFormat_getStrideBytes(format);
}

typedef enum EAudioStreamInfoFlags {                  //4-bit flags
	EAudioStreamInfoFlags_None            = 0,
	EAudioStreamInfoFlags_IsLoop        = 1 << 0,
	EAudioStreamInfoFlags_FlattenSound    = 1 << 1    //Force stereo sound into mono, required for 3D spatial audio if stereo
} EAudioStreamInfoFlags;

typedef struct AudioStreamInfo {

	F32 pitch;                    //Playback speed factor (speeds up and raises the pitch), 0 = 1, negative is refused
	U8 dataLengthHi8;             //Bits 32 and up of the data length, read it with AudioStreamInfo_dataLength
	U8 flags4_format4;            //EAudioStreamInfoFlags, AudioStreamFormat
	U16 loops;                    //Total plays. 0 is infinite. Without IsLoop it must be 1

	Ns duration;                  //Length of the data at pitch 1, seekTime offsets are bounded by it

	U64 dataStart;                //Byte offset in stream of the first sample

	U32 sampleRate;               //Frames per second (Hz)
	U32 dataLengthLo32;           //Bytes of sample data from dataStart

	U32 streamLength;            //Bytes of each of the 4 backend buffers, 0 = bytesPerSecond, else 64 KiB to 2 GiB - 1
	U32 bytesPerSecond;          //Must equal sampleRate times the bytes per frame of the format

	StreamRef *stream;           //Read during AudioDeviceRef_update, so it must stay readable and unchanged while playing

} AudioStreamInfo;

static inline EAudioStreamInfoFlags AudioStreamInfo_flags(const AudioStreamInfo *info) {
	return info ? (EAudioStreamInfoFlags)(info->flags4_format4 & 0xF) : EAudioStreamInfoFlags_None;
}

static inline EAudioStreamFormat AudioStreamInfo_format(const AudioStreamInfo *info) {
	return info ? (EAudioStreamFormat)(info->flags4_format4 >> 4) : EAudioStreamFormat_Count;
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
	return info ? (info->dataLengthLo32 | ((U64) info->dataLengthHi8 << 32)) : 0;
}

typedef RefPtr AudioDeviceRef;

typedef struct AudioStream {

	AudioDeviceRef *device;       //Reference held by the stream

	AudioStreamInfo info;

	U64 streamOffset;             //Bytes into the data that are read next, ahead of what is audible by the queued buffers
	Ns timeOffset;                //[0, duration], only knows the one on start, since the streamOffset is always ahead

	//Passes over the data, counted only by the looping and converting read paths: without IsLoop it stays 0 on a direct
	// read and reaches 1 on a converting one. Counted when read, so it is ahead of what is audible. Reset to 0 by stop and
	// by a seek while not playing. A seek while playing steps it back to the pass being heard if the last was already read.
	U32 loops;
	Bool isPlaying;               //Cleared by AudioDeviceRef_update when playback ends, and by stop
	AudioStreamFormat format;     //The real format that the device is reading. Stereo to mono and/or F32/F64/U24 -> U16
	Bool seekPending;             //A seek is recorded and the next AudioDeviceRef_update drops the queued audio for it
	U8 padding;

} AudioStream;

typedef RefPtr AudioStreamRef;

static inline AudioStream *AudioStreamRef_ptr(AudioStreamRef *ptr) { return RefPtr_data(ptr, AudioStream); }
static inline void *AudioStream_extVoid(AudioStream *src) { return !src ? NULL : (src + 1); }

#define AudioStream_ext(ptr, T) (T##AudioStream*)(AudioStream_extVoid(ptr))

RefPtrType AudioStream_makeType(const Allocator *alloc);

//*stream must be NULL on entry. The stream adds a reference to device and is silent until an AudioSource is made for
// it. The device holds a playing stream weakly, so releasing the last reference stops it.
//startOffset is in nanoseconds from the start of the data, at most info->duration.
//A call refused on its arguments, or whose allocation fails, leaves the caller owning info->stream. Past that the
// created stream owns it and info is zeroed, even if the call then fails.
//Validation writes the defaults (streamLength = bytesPerSecond, pitch = 1) into info, even if the call then fails.
//streamLength and the data length are rounded down to whole frames of the format (written back into info), and a stream
// queues 4 buffers of streamLength ahead of playback.
//info->loops must be 1 unless info has IsLoop, and the data length (after rounding) must not be 0.
Bool AudioDeviceRef_createStream(
	AudioDeviceRef *device,
	AudioStreamInfo *info,                //Takes ownership of info (its stream reference), see above
	Ns startOffset,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioStreamRef **stream,
	Error *e_rr
);

//Borrows inputStream: the audio stream takes a reference of its own, so the caller releases its reference either way.
//inputStreamOffset is the byte offset of the file inside inputStream. inputStream must be readable and is read again
// during playback, by whoever calls AudioDeviceRef_update.
//*stream must be NULL on entry and is left NULL on failure. startOffset is in nanoseconds into the audio data.
//loops is the total number of plays. pitch 0 means 1, negative is refused.
//Other contracts match AudioDeviceRef_createStream.
Bool AudioDeviceRef_createFromFile(       //Detect stream by stream file header (currently only wav supported)
	AudioDeviceRef *device,
	StreamRef *inputStream,
	U64 inputStreamOffset,
	U16 loops,                            //0 = infinite loops, 1 = normal, else how many times it will loop
	Ns startOffset,
	F32 pitch,
	Bool flattenSound,                    //Required for spatial audio if the source is stereo
	const Allocator *alloc,
	const RefPtrType *type,
	AudioStreamRef **stream,
	Error *e_rr
);

//offset is in nanoseconds from the start of the data, at most the duration. Rounded up to a whole frame, but never
// past the end of the data.
//Only records the seek, without a backend call: the next AudioDeviceRef_update drops the audio already queued (up to 4
// buffers of streamLength) and refills from offset. A playing stream keeps playing and a stopped one stays stopped.
//A stream that isn't playing starts over from the first pass, so a play after it makes every pass again. This is also how
// a stream that played to its end is played again: until a seek it stays at its end and a play ends at the next update.
//Does not lock: don't call it while another thread runs AudioDeviceRef_update on the device.
Bool AudioStreamRef_seekTime(AudioStreamRef *stream, Ns offset, Error *e_rr);

//Playing and stopping take the device lock (waits up to a second, then fail). Both are no-ops if already in that state.
//A play the backend can't start fails and leaves the stream not playing. The sound only advances while
// AudioDeviceRef_update runs. A stream that played to its end stays at its end until a seek: playing it then ends at the
// next update without sound.
Bool AudioStreamRef_play(AudioStreamRef *stream, const Allocator *alloc, Error *e_rr);

//Discards the queued audio and resets the loop count without rewinding, so a later play continues from the read
// position, past what was last audible. seekTime restarts it. If the backend can't stop the stream it keeps playing.
Bool AudioStreamRef_stop(AudioStreamRef *stream, Error *e_rr);

#ifdef __cplusplus
	}
#endif
