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

#include "types/container/list_impl.h"
#include "audio/audio_stream.h"
#include "audio/audio_device.h"
#include "audio/openal_soft/openal_soft.h"
#include "formats/wav/wav_file.h"
#include "types/container/log.h"
#include "types/container/ref_ptr.h"
#include "types/base/mathf.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

U32 AudioStream_sizeExt = sizeof(ALAudioStream);

Bool AudioStream_createExt(AudioStream *stream, const Allocator *alloc, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;
	AudioDevice *device = AudioDeviceRef_ptr(stream->device);
	ALAudioDevice *deviceExt = AudioDevice_ext(device, AL);
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);

	//Create buffer handles

	ALC_PROCESS_ERROR(deviceExt->device, alcMakeContextCurrent(deviceExt->context));
	AL_PROCESS_ERROR(alGenBuffers(ALAudioStream_bufferCount, streamExt->buffer));
	streamExt->initializedBuffers = true;

	//This is the format that the implementation understands, that we have to translate to first.

	EAudioStreamFormat ogFormat = AudioStreamInfo_format(&stream->info);
	U8 stride = EAudioStreamFormat_getStrideBytes(ogFormat);
	Bool fallback = false;

	switch (stride) {
		case 3:		fallback = !(device->info.flags & EAudioDeviceFlags_HasU24Ext);		break;
		case 4:		fallback = !(device->info.flags & EAudioDeviceFlags_HasF32Ext);		break;
		case 8:		fallback = !(device->info.flags & EAudioDeviceFlags_HasF64Ext);		break;
		default:																		break;
	}

	EAudioStreamFormat format = ogFormat;

	if(fallback)
		format = EAudioStreamFormat_Mono16 + (format & 1);

	stream->format = format;

	Bool flatten = AudioStreamInfo_flattenSound(&stream->info);

	if(flatten)			//Get rid of stereo
		format &=~ 1;

	#ifndef NDEBUG
		if(stream->format != ogFormat)
			Log_performanceLn(alloc, "AudioStream: Converting non native format to supported format at runtime");
	#endif
	
	ALenum fmt;

	switch (format) {
	
		case EAudioStreamFormat_Mono8:			fmt = AL_FORMAT_MONO8;				break;
		case EAudioStreamFormat_Stereo8:		fmt = AL_FORMAT_STEREO8;			break;

		case EAudioStreamFormat_Mono16:			fmt = AL_FORMAT_MONO16;				break;
		case EAudioStreamFormat_Stereo16:		fmt = AL_FORMAT_STEREO16;			break;

		case EAudioStreamFormat_Mono32fExt:		fmt = AL_FORMAT_MONO_FLOAT32;		break;
		case EAudioStreamFormat_Stereo32fExt:	fmt = AL_FORMAT_STEREO_FLOAT32;		break;

		case EAudioStreamFormat_Mono64fExt:		fmt = AL_FORMAT_MONO_DOUBLE_EXT;	break;
		case EAudioStreamFormat_Stereo64fExt:	fmt = AL_FORMAT_STEREO_DOUBLE_EXT;	break;

		default:
			retError(clean, Error_unsupportedOperation(0, "AudioStream_createExt() unsupported format"));
	}
	
	//Due to OpenAL not liking instancing streams much, we have to create a fake source here.
	//This source will be muted, but the pitch is the one that needs to be shared with other sources.
	//This is because the pitch affects the speed at which the playback occurs, affecting the stream itself.
	//This means that instancing is possible, as long as both sources are OK with using the same pitch and offset.

	AL_PROCESS_ERROR(alGenSources(1, &streamExt->source));
	streamExt->initializedSource = true;

	//Must be done by the stream itself
	AL_PROCESS_ERROR(alSourcei(streamExt->source, AL_LOOPING, AL_FALSE));

	AL_PROCESS_ERROR(alSourcef(streamExt->source, AL_GAIN, 0));		//Muted, AudioSource has to take control
	AL_PROCESS_ERROR(alSourcef(streamExt->source, AL_PITCH, stream->info.pitch));

	//Buffer to handle things like mono -> stereo, reverse, looping, etc.
	//Things that can't be handled by directly reading the stream
	gotoIfError3(clean, Buffer_createUninitializedBytes(ALAudioStream_bufferSize, alloc, &streamExt->tmp, e_rr));

	U8 newStride = EAudioStreamFormat_getStrideBytes(format);
	gotoIfError3(clean, Buffer_createUninitializedBytes(
		ALAudioStream_bufferSize * stride / newStride, alloc, &streamExt->tmpCvt, e_rr
	));

	streamExt->format = fmt;

clean:
	return s_uccess;
}

Bool AudioStream_playExt(AudioStream *stream, Error *e_rr) {

	Bool s_uccess = true;
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);
	AL_PROCESS_ERROR(alSourcePlay(streamExt->source));

clean:
	return s_uccess;
}

Bool AudioStream_stopExt(AudioStream *stream, Error *e_rr) {

	Bool s_uccess = true;
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);
	AL_PROCESS_ERROR(alSourceStop(streamExt->source));

	ALint queued = 0;
	AL_PROCESS_ERROR(alGetSourcei(streamExt->source, AL_BUFFERS_QUEUED, &queued));

	if (queued) {
		ALuint tmp[ALAudioStream_bufferCount];
		AL_PROCESS_ERROR(alSourceUnqueueBuffers(streamExt->source, queued, tmp));
	}

	streamExt->filledStream = false;

clean:
	return s_uccess;
}

void AudioStream_freeExt(AudioStream *stream, const Allocator *alloc) {

	(void) alloc;

	ALAudioDevice *deviceExt = AudioDevice_ext(AudioDeviceRef_ptr(stream->device), AL);
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);

	if (stream->isPlaying) {

		alSourceStop(streamExt->source);

		ALint queued = 0;
		alGetSourcei(streamExt->source, AL_BUFFERS_QUEUED, &queued);

		if (queued) {
			ALuint tmp[ALAudioStream_bufferCount];
			alSourceUnqueueBuffers(streamExt->source, queued, tmp);
		}
	}

	Buffer_free(&streamExt->tmp, alloc);
	Buffer_free(&streamExt->tmpCvt, alloc);

	if(streamExt->initializedBuffers | streamExt->initializedSource)
		alcMakeContextCurrent(deviceExt->context);

	if(streamExt->initializedSource)
		alDeleteSources(1, &streamExt->source);

	if(streamExt->initializedBuffers)
		alDeleteBuffers(ALAudioStream_bufferCount, streamExt->buffer);
}

Bool AudioStream_update(AudioStream *stream, U64 index, const Allocator *alloc, Error *e_rr) {

	(void)alloc;

	Bool s_uccess = true;
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);

	ALuint freeBuffers[ALAudioStream_bufferCount];
	U8 freeBufferCount = !streamExt->filledStream ? ALAudioStream_bufferCount : 0;

	AudioDevice *device = AudioDeviceRef_ptr(stream->device);

	StreamCursor cursor = (StreamCursor){ 0 };

	if (!freeBufferCount) {

		ALint state = 0;
		AL_PROCESS_ERROR(alGetSourcei(streamExt->source, AL_SOURCE_STATE, &state));

		if (state == AL_STOPPED || state == AL_PAUSED) {
			stream->isPlaying = false;
			ListWeakRefPtr_erase(&device->streams, index, NULL);
			goto clean;
		}

		ALint buffersProcessed = 0;
		AL_PROCESS_ERROR(alGetSourcei(streamExt->source, AL_BUFFERS_PROCESSED, &buffersProcessed));

		freeBufferCount = (U8)buffersProcessed;
		AL_PROCESS_ERROR(alSourceUnqueueBuffers(streamExt->source, freeBufferCount, freeBuffers));
	}

	else Buffer_memcpy(
		Buffer_createRef(freeBuffers, sizeof(freeBuffers)),
		Buffer_createRef(streamExt->buffer, sizeof(freeBuffers))
	);

	if (!freeBufferCount)
		goto clean;

	Bool finished = stream->loops >= stream->info.loops;
	Bool isInfiniteLoop = AudioStreamInfo_isInfiniteLoop(&stream->info);

	if (finished && !isInfiniteLoop)
		goto clean;

	EAudioStreamFormat ogFormat = AudioStreamInfo_format(&stream->info);
	Bool isFallback = ogFormat != stream->format;

	U64 maxLen = Buffer_length(streamExt->tmpCvt);

	//Always use tmpCvt as the cursor cache; tmp is the final output for fallback/loop paths
	gotoIfError3(clean, StreamCursor_createWithCache(stream->info.stream, &streamExt->tmpCvt, true, &cursor, e_rr));

	U32 queued = 0;

	U8 ogStride = EAudioStreamFormat_getStrideBytes(ogFormat);
	U8 newStride = EAudioStreamFormat_getStrideBytes(stream->format);
	U8 ogChannels = EAudioStreamFormat_getChannels(ogFormat);
	U8 newChannels = EAudioStreamFormat_getChannels(stream->format);
	Bool changeChannels = ogChannels != newChannels;
	U8 inputStep = changeChannels ? 2 : 1;

	Bool isLoop = AudioStreamInfo_isLoop(&stream->info);

	for (U32 i = 0; i < freeBufferCount; ++i) {

		ALuint bufferi = freeBuffers[i];

		U64 len = U64_min(AudioStreamInfo_dataLength(&stream->info) - stream->streamOffset, maxLen);

		if (!len && !isLoop)
			break;

		if (isLoop || isFallback) {

			U64 filled = 0;
			U64 consumed = 0;

			void *tmp = (void*)streamExt->tmp.ptr;
			U8 *tmp8 = (U8*)tmp;
			U16 *tmp16 = (U16*)tmp;

			Bool finishedLoop = false;

			while (isLoop || !stream->loops) {

				U64 seekPos = stream->streamOffset + stream->info.dataStart;
				gotoIfError3(clean, StreamCursor_read(&cursor, Buffer_createNull(), seekPos, 0, len, false, alloc, e_rr));

				U64 cvtOff = seekPos - cursor.lastLocation;
				const void *src = cursor.cacheData.ptr + cvtOff;

				if (isFallback) {

					U64 blocks = len / ogStride;

					for (U64 j = 0, k = filled / newStride; j < blocks; j += inputStep, ++k) {

						U64 result = WAVFile_cvt(src, ogStride, newStride, j);

						if (changeChannels)
							result = WAVFile_avg(result, WAVFile_cvt(src, ogStride, newStride, j + 1), newStride);

						switch (newStride) {
							default:	tmp16[k] = (U16)result;	break;
							case 1:		tmp8[k] = (U8)result;	break;
						}
					}

					consumed += len;
					filled += (blocks / inputStep) * newStride;
				}

				//Loop, no conversion, memcpy chunk into tmp at current offset
				else {

					Buffer_memcpy(
						Buffer_createRef(tmp8 + filled, len),
						Buffer_createRefConst(src, len)
					);

					filled += len;
					consumed += len;
				}

				U64 dataLength = AudioStreamInfo_dataLength(&stream->info);
				stream->streamOffset = (stream->streamOffset + len) % dataLength;
				len = U64_min(dataLength - stream->streamOffset, maxLen - consumed);

				if (consumed < maxLen) {

					if (!isLoop || (stream->info.loops && stream->loops + 1 >= stream->info.loops)) {
						finishedLoop = true;
						++stream->loops;
						break;
					}

					++stream->loops;
					continue;
				}

				break;
			}

			if (!filled)
				break;

			AL_PROCESS_ERROR(alBufferData(
				bufferi, (ALenum)streamExt->format, streamExt->tmp.ptr, (ALsizei)filled, stream->info.sampleRate
			));

			++queued;

			if (finishedLoop && !isInfiniteLoop)
				break;

			continue;
		}

		//Direct path: read into tmpCvt via cursor, pass directly to alBufferData

		U64 seekPos = stream->streamOffset + stream->info.dataStart;
		gotoIfError3(clean, StreamCursor_read(&cursor, Buffer_createNull(), seekPos, 0, len, false, alloc, e_rr));

		U64 off = seekPos - cursor.lastLocation;
		const void *data = cursor.cacheData.ptr + off;

		AL_PROCESS_ERROR(alBufferData(
			bufferi, (ALenum)streamExt->format, data, (ALsizei)len, stream->info.sampleRate
		));

		stream->streamOffset += len;
		++queued;
	}

	if (queued)
		AL_PROCESS_ERROR(alSourceQueueBuffers(streamExt->source, queued, freeBuffers));

	if (!streamExt->filledStream) {
		AL_PROCESS_ERROR(alSourcePlay(streamExt->source));
		streamExt->filledStream = true;
	}

clean:
	if (cursor.cacheData.ptr)
		StreamCursor_closeAndKeepCache(&cursor, alloc, &streamExt->tmpCvt, NULL);

	return s_uccess;
}

