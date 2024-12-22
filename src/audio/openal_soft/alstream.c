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

#include "platforms/ext/listx_impl.h"
#include "audio/stream.h"
#include "audio/device.h"
#include "audio/openal_soft/openal_soft.h"
#include "platforms/log.h"
#include "types/container/ref_ptr.h"
#include "types/math/math.h"

U32 AudioStream_sizeExt = sizeof(ALAudioStream);

Bool AudioStream_createExt(AudioStream *stream, Allocator alloc, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;
	AudioDevice *device = AudioDeviceRef_ptr(stream->device);
	ALAudioDevice *deviceExt = AudioDevice_ext(device, AL);
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);

	//Create buffer handles

	AL_PROCESS_ERROR(deviceExt->device, alcMakeContextCurrent(deviceExt->context))
	AL_PROCESS_ERROR(deviceExt->device, alGenBuffers(ALAudioStream_bufferCount, streamExt->buffer))
	streamExt->initializedBuffers = true;

	//This is the format that the implementation understands, that we have to translate to first.

	EAudioStreamFormat format = stream->info.format;
	U8 stride = EAudioStreamFormat_getStrideBytes(stream->info.format);
	Bool fallback = false;

	switch (stride) {
		case 3:		fallback = !(device->info.flags & EAudioDeviceFlags_HasU24Ext);		break;
		case 4:		fallback = !(device->info.flags & EAudioDeviceFlags_HasF32Ext);		break;
		case 8:		fallback = !(device->info.flags & EAudioDeviceFlags_HasF64Ext);		break;
		default:																		break;
	}

	if(fallback)
		format = EAudioStreamFormat_Mono16 + (stream->info.format & 1);

	stream->format = format;

	if(stream->info.flattenSound)	//Get rid of stereo
		format &=~ 1;

	if(stream->format != stream->info.format)
		Log_performanceLn(alloc, "AudioStream: Converting non native format to supported format at runtime");
	
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
			retError(clean, Error_unsupportedOperation(0, "AudioStream_createExt() unsupported format"))
	}
	
	//Due to OpenAL not liking instancing streams much, we have to create a fake source here.
	//This source will be muted, but the pitch is the one that needs to be shared with other sources.
	//This is because the pitch affects the speed at which the playback occurs, affecting the stream itself.
	//This means that instancing is possible, as long as both sources are OK with using the same pitch and offset.

	AL_PROCESS_ERROR(deviceExt->device, alGenSources(1, &streamExt->source))
	streamExt->initializedSource = true;

	AL_PROCESS_ERROR(deviceExt->device, alSourcei(streamExt->source, AL_LOOPING, AL_FALSE))		//Must be done by the stream itself
	AL_PROCESS_ERROR(deviceExt->device, alSourcef(streamExt->source, AL_GAIN, 1 /* TODO: Set this to 0 again */))
	AL_PROCESS_ERROR(deviceExt->device, alSourcef(streamExt->source, AL_PITCH, stream->info.pitch))

	//Buffer to handle things like mono -> stereo, reverse, looping, etc.
	//Things that can't be handled by directly reading the stream
	gotoIfError2(clean, Buffer_createUninitializedBytes(ALAudioStream_bufferSize, alloc, &streamExt->tmp))

	if(format != stream->info.format) {
		U8 newStride = EAudioStreamFormat_getStrideBytes(format);
		gotoIfError2(clean, Buffer_createUninitializedBytes(ALAudioStream_bufferSize * stride / newStride, alloc, &streamExt->tmpCvt))
	}

	streamExt->format = fmt;

clean:
	return s_uccess;
}

Bool AudioStream_stopExt(AudioStream *stream, Error *e_rr) {

	Bool s_uccess = true;
	ALAudioDevice *deviceExt = AudioDevice_ext(AudioDeviceRef_ptr(stream->device), AL);
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);
	AL_PROCESS_ERROR(deviceExt->device, alSourcePause(streamExt->source));

clean:
	return s_uccess;
}

Bool AudioStream_freeExt(AudioStream *stream, Allocator alloc) {

	(void) alloc;

	ALAudioDevice *deviceExt = AudioDevice_ext(AudioDeviceRef_ptr(stream->device), AL);
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);
	Error *e_rr = NULL;
	Bool s_uccess = true;

	if (stream->isPlaying) {
		AL_PROCESS_ERROR(deviceExt->device, alSourceStop(streamExt->source));
		AL_PROCESS_ERROR(deviceExt->device, alSourceUnqueueBuffers(streamExt->source, ALAudioStream_bufferCount, streamExt->buffer))
	}

	Buffer_free(&streamExt->tmp, alloc);
	Buffer_free(&streamExt->tmpCvt, alloc);

	if(streamExt->initializedBuffers || streamExt->initializedSource)
		AL_PROCESS_ERROR(deviceExt->device, alcMakeContextCurrent(deviceExt->context))

	if(streamExt->initializedSource)
		AL_PROCESS_ERROR(deviceExt->device, alDeleteSources(1, &streamExt->source))

	if(streamExt->initializedBuffers)
		AL_PROCESS_ERROR(deviceExt->device, alDeleteBuffers(ALAudioStream_bufferCount, streamExt->buffer))

clean:
	return s_uccess;
}

U64 ALAudioStream_fetchInput(const U8 *cvt8, const U16 *cvt16, const F32 *cvt32, const F64 *cvt64, U8 ogStride, U64 i) {

	switch (ogStride) {

		//Truncation to U16

		default:	//Skip 0 to avoid the least significant bit
			return cvt8[i * 3 + 1] | ((U16)cvt8[i * 3 + 2] << 8);								//TODO: Dither

		case 4:		return (U16) (I16) F64_clamp(-cvt32[i] * I16_MIN, I16_MIN, I16_MAX);		//TODO: Dither
		case 8:		return (U16) (I16) F64_clamp(-cvt64[i] * I16_MIN, I16_MIN, I16_MAX);		//TODO: Dither

		//Same type to same type; only changes from stereo to mono

		case 1:		return cvt8[i];
		case 2:		return cvt16[i];
	}
}

Bool AudioStream_update(AudioStream *stream, U64 index, Allocator alloc, Error *e_rr) {

	(void) alloc;

	Bool s_uccess = true;
	ALAudioStream *streamExt = AudioStream_ext(stream, AL);

	ALuint freeBuffers[ALAudioStream_bufferCount];
	U8 freeBufferCount = !streamExt->filledStream ? ALAudioStream_bufferCount : 0;

	AudioDevice *device = AudioDeviceRef_ptr(stream->device);
	ALAudioDevice *deviceExt = AudioDevice_ext(device, AL);

	if(!freeBufferCount) {
	
		ALint state = 0;
		AL_PROCESS_ERROR(deviceExt->device, alGetSourcei(streamExt->source, AL_SOURCE_STATE, &state))

		if (state == AL_STOPPED || state == AL_PAUSED) {		//Finished, stop updating for now
			stream->isPlaying = false;
			ListWeakRefPtr_erase(&device->streams, index);
			goto clean;
		}

		ALint buffersProcessed = 0;
		AL_PROCESS_ERROR(deviceExt->device, alGetSourcei(streamExt->source, AL_BUFFERS_PROCESSED, &buffersProcessed))

		freeBufferCount = (U8) buffersProcessed;
		AL_PROCESS_ERROR(deviceExt->device, alSourceUnqueueBuffers(streamExt->source, freeBufferCount, freeBuffers))
	}

	else Buffer_copy(
		Buffer_createRef(freeBuffers, sizeof(freeBuffers)),
		Buffer_createRef(streamExt->buffer, sizeof(freeBuffers))
	);

	//Don't need to loop if we don't have anything to do

	if(!freeBufferCount)
		goto clean;

	//Fill all buffers we need to

	U32 queued = 0;
	Bool isFallback = stream->info.format != stream->format;
	Buffer target = isFallback ? streamExt->tmpCvt : streamExt->tmp;
	U64 maxLen = Buffer_length(target);

	for (U32 i = 0; i < freeBufferCount; ++i) {

		ALuint bufferi = freeBuffers[i];

		U64 len = U64_min(stream->info.dataLength - stream->streamOffset, maxLen);

		if(!len && !stream->info.isLoop)
			break;

		//Loops or fallbacks need special care;
		//For fallbacks, we need to do a conversion process to ensure the buffer can read the right format.
		//For loops, we need to load the start of the stream again.

		if(stream->info.isLoop || isFallback) {

			U64 filled = 0;

			const void *cvt  = streamExt->tmpCvt.ptr;
			const U8 *cvt8   = (const U8*) cvt;
			const U16 *cvt16 = (const U16*) cvt;
			const F32 *cvt32 = (const F32*) cvt;
			const F64 *cvt64 = (const F64*) cvt;

			void *tmp		 = (void*) streamExt->tmp.ptr;
			U8 *tmp8		 = (U8*) tmp;
			U16 *tmp16		 = (U16*) tmp;

			Bool finishedLoop = false;

			while(stream->info.isLoop || !stream->loops) {

				U64 seekPos = stream->streamOffset + stream->info.dataStart;
				gotoIfError3(clean, Stream_read(&stream->info.stream, target, seekPos, filled, len, false, e_rr))
				filled += len;

				stream->streamOffset = (stream->streamOffset + len) % stream->info.dataLength;
				len = U64_min(stream->info.dataLength - stream->streamOffset, maxLen - filled);

				if(filled < maxLen) {

					if (!stream->info.isLoop) {
						finishedLoop = true;
						++stream->loops;
						goto processChunk;
					}

					++stream->loops;
					continue;
				}

			processChunk:

				//Load from cvt to regular

				if (isFallback) {

					U8 ogStride = EAudioStreamFormat_getStrideBytes(stream->info.format);
					U8 newStride = EAudioStreamFormat_getStrideBytes(stream->format);
					U8 ogChannels = EAudioStreamFormat_getChannels(stream->info.format);
					U8 newChannels = EAudioStreamFormat_getChannels(stream->format);

					Bool changeChannels = ogChannels != newChannels;

					U64 blocks = filled / ogStride;
					U8 inputStep = changeChannels ? 2 : 1;

					for (U64 j = 0, k = 0; j < blocks; j += inputStep, ++k) {

						U64 result1 = ALAudioStream_fetchInput(cvt8, cvt16, cvt32, cvt64, ogStride, j);

						//Avg stereo to mono

						if (changeChannels) {
							result1 += ALAudioStream_fetchInput(cvt8, cvt16, cvt32, cvt64, ogStride, j + 1);
							result1 >>= 1;
						}

						//Write to output format

						switch (newStride) {
							default:	tmp16[k] = (U16) result1;	break;
							case 1:		tmp8[k] = (U8) result1;		break;
						}
					}

					filled = blocks * newStride;
				}

				AL_PROCESS_ERROR(deviceExt->device, alBufferData(
					bufferi, (ALenum) streamExt->format, streamExt->tmp.ptr, (ALsizei) filled, stream->info.sampleRate
				))

				++queued;
				break;
			}

			if(finishedLoop && !stream->info.isLoop)
				break;

			continue;
		}

		//Otherwise we can load directly from memory

		U64 seekPos = stream->streamOffset + stream->info.dataStart;
		gotoIfError3(clean, Stream_read(&stream->info.stream, Buffer_createNull(), seekPos, 0, len, false, e_rr))

		U64 off = seekPos - stream->info.stream.lastLocation;
		const void *data = stream->info.stream.cacheData.ptr + off;

		AL_PROCESS_ERROR(deviceExt->device, alBufferData(
			bufferi, (ALenum) streamExt->format, data, (ALsizei) len, stream->info.sampleRate
		))

		stream->streamOffset += len;
		++queued;
	}

	if(queued)
		AL_PROCESS_ERROR(deviceExt->device, alSourceQueueBuffers(streamExt->source, queued, freeBuffers))

	if(!streamExt->filledStream) {
		AL_PROCESS_ERROR(deviceExt->device, alSourcePlay(streamExt->source));
		streamExt->filledStream = true;
	}

clean:
	return s_uccess;
}

