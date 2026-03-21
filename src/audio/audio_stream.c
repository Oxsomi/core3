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

#include "audio/audio_stream.h"
#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "formats/wav/wav_file.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/stream.h"
#include "types/container/types.h"
#include "types/base/error.h"
#include "types/base/constants.h"

Bool AudioStreamRef_seekTime(AudioStreamRef *streamRef, Ns offset, Error *e_rr) {

	Bool s_uccess = true;

	if (!streamRef || streamRef->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(0, "AudioStreamRef_seekTime()::stream is required"));

	AudioStream *stream = AudioStreamRef_ptr(streamRef);

	if(offset > stream->info.duration)
		retError(clean, Error_invalidParameter(1, 0, "AudioStreamRef_seekTime() duration offset out of bounds"));

	stream->timeOffset = offset;

	U64 stride = EAudioStreamFormat_getSize(AudioStreamInfo_format(&stream->info));

	U64 streamOffset = offset / SECOND * stream->info.bytesPerSecond;
	streamOffset += (Ns)((offset % SECOND) * (stream->info.bytesPerSecond / (F64)SECOND));

	stream->streamOffset = (streamOffset + stride - 1) &~ (stride - 1);

clean:
	return s_uccess;
}

impl Bool AudioStream_playExt(AudioStream *stream, Error *e_rr);

Bool AudioStreamRef_play(AudioStreamRef *streamRef, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if (!streamRef || streamRef->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(0, "AudioStreamRef_play()::stream is required"));
		
	AudioStream *stream = AudioStreamRef_ptr(streamRef);

	dev = AudioDeviceRef_ptr(stream->device);
	acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioStreamRef_play() couldn't acquire device lock in time"));

	if(stream->isPlaying)
		goto clean;

	gotoIfError3(clean, ListWeakRefPtr_pushBack(&dev->streams, streamRef, alloc, e_rr));
	gotoIfError3(clean, AudioStream_playExt(stream, e_rr));
	stream->isPlaying = true;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	return s_uccess;
}

impl Bool AudioStream_stopExt(AudioStream *stream, Error *e_rr);

Bool AudioStreamRef_stop(AudioStreamRef *streamRef, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if (!streamRef || streamRef->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(0, "AudioStreamRef_stop()::stream is required"));
		
	AudioStream *stream = AudioStreamRef_ptr(streamRef);

	dev = AudioDeviceRef_ptr(stream->device);
	acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioStreamRef_stop() couldn't acquire device lock in time"));

	if(!stream->isPlaying)
		goto clean;

	U64 id = ListWeakRefPtr_findFirst(dev->streams, streamRef, 0, NULL);

	if(id != U64_MAX)
		ListWeakRefPtr_erase(&dev->streams, id, NULL);

	gotoIfError3(clean, AudioStream_stopExt(stream, e_rr));
	stream->isPlaying = false;
	stream->loops = 0;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	return s_uccess;
}

impl extern U32 AudioStream_sizeExt;
impl Bool AudioStream_createExt(AudioStream *stream, const Allocator *alloc, Error *e_rr);
impl void AudioStream_freeExt(AudioStream *stream, const Allocator *alloc);

void AudioStream_free(AudioStream *stream, const Allocator *alloc) {

	if(!stream)
		return;

	AudioStream_freeExt(stream, alloc);

	OxStream *underlyingStream = RefPtr_data(stream->info.stream, OxStream);

	if(underlyingStream)
		underlyingStream->close(underlyingStream, alloc);

	//Ensure stream isn't updated next time

	AudioStreamRef *streamRef = (AudioStreamRef*)stream - 1;

	AudioDevice *dev = AudioDeviceRef_ptr(stream->device);
	ELockAcquire acq = SpinLock_lock(&dev->pendingUpdateLock, U64_MAX);

	if (acq >= ELockAcquire_Success) {

		U64 id = ListWeakRefPtr_findFirst(dev->streams, streamRef, 0, NULL);

		if (id != U64_MAX)
			ListWeakRefPtr_erase(&dev->streams, id, NULL);

		if (acq == ELockAcquire_Acquired)
			SpinLock_unlock(&dev->pendingUpdateLock);
	}

	RefPtr_dec(&stream->device);
}

RefPtrType AudioStream_makeType(const Allocator *alloc) {
	return (RefPtrType) {
		.typeId = (ETypeId) EAudioTypeId_AudioStream,
		.length = (U32)(sizeof(AudioStream) + AudioStream_sizeExt),
		.alloc = alloc,
		.free = (ObjectFreeFunc)AudioStream_free
	};
}

Bool AudioDeviceRef_createStream(
	AudioDeviceRef *device,
	AudioStreamInfo *info,
	Ns startOffset,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioStreamRef **stream,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool madeRef = false;

	if (!device || device->refPtrType->typeId != (ETypeId)EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_createStream()::device is required"));

	if(!info)
		retError(clean, Error_nullPointer(1, "AudioDeviceRef_createStream()::info is required"));

	if(!info->sampleRate || !info->bytesPerSecond)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream()::info.sampleRate and bytesPerSecond are required"
		));

	EAudioStreamFormat format = AudioStreamInfo_format(info);

	if(format >= EAudioStreamFormat_Count)
		retError(clean, Error_outOfBounds(
			0, format, EAudioStreamFormat_Count, "AudioDeviceRef_createStream()::info->formatId is invalid"
		));
	
	U8 bytesPerBlock = EAudioStreamFormat_getSize(format);

	if((U64)info->sampleRate * bytesPerBlock != info->bytesPerSecond)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream()::info.sampleRate is invalid"
		));

	if(!info->stream)
		retError(clean, Error_invalidParameter(1, 0, "AudioDeviceRef_createStream()::info.stream is required"));

	OxStream *underlyingStream = RefPtr_data(info->stream, OxStream);

	if(info->dataStart + AudioStreamInfo_dataLength(info) > underlyingStream->size)
		retError(clean, Error_outOfBounds(
			0, info->dataStart + AudioStreamInfo_dataLength(info), underlyingStream->size,
			"AudioDeviceRef_createStream()::info dataStart + dataLength out of bounds"
		));

	if(info->streamLength && info->streamLength < 64 * KIBI)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream()::info.streamLength is too small"
		));

	if(!info->streamLength)
		info->streamLength = info->bytesPerSecond;

	if(!info->pitch)
		info->pitch = 1;

	if(info->pitch < 0)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream() reversing playing audio not supported yet"
		));
		
	if(
		!type ||
		type->typeId != (ETypeId)EAudioTypeId_AudioStream ||
		type->length != sizeof(AudioStream) + AudioStream_sizeExt ||
		type->free != (ObjectFreeFunc)AudioStream_free ||
		type->alloc != alloc
	)
		retError(clean, Error_invalidParameter(4, 0, "AudioDeviceRef_create()::type is invalid"));

	gotoIfError3(clean, RefPtr_create(type, stream, e_rr));
	madeRef = true;
	RefPtr_inc(device);

	*AudioStreamRef_ptr(*stream) = (AudioStream) {
		.device = device,
		.info = *info
	};

	*info = (AudioStreamInfo) { 0 };

	gotoIfError3(clean, AudioStreamRef_seekTime(*stream, startOffset, e_rr));
	gotoIfError3(clean, AudioStream_createExt(AudioStreamRef_ptr(*stream), alloc, e_rr));

clean:

	if(madeRef && !s_uccess)
		RefPtr_dec(stream);

	return s_uccess;
}

Bool AudioDeviceRef_createFromFile(
	AudioDeviceRef *device,
	StreamRef *inputStream,
	U64 inputStreamOffset,
	U16 loops,
	Ns startOffset,
	F32 pitch,
	Bool flattenSound,
	const Allocator *alloc,
	const RefPtrType *type,
	AudioStreamRef **outStream,
	Error *e_rr
) {

	Bool s_uccess = true;

	AudioStreamInfo streamInfo = (AudioStreamInfo) { 0 };
	U32 magic = 0;

	if (pitch < 0)
		retError(clean, Error_invalidState(0, "AudioDeviceRef_createFileStream() negative pitch not supported yet"));	//TODO:

	if (!inputStream || inputStream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_createFileStream()::inputStream is required"));

	OxStream *stream = RefPtr_data(inputStream, OxStream);

	if(!stream->read)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_createFileStream()::stream must be readable"));

	gotoIfError3(clean, stream->read(
		stream, inputStreamOffset, sizeof(magic), Buffer_createRef(&magic, sizeof(magic)), alloc, e_rr
	));

	streamInfo.stream = inputStream;
	streamInfo.pitch = !pitch ? 1 : pitch;
	streamInfo.loops = loops;
	streamInfo.flags4_format4 =
		(loops != 1 ? EAudioStreamInfoFlags_IsLoop : EAudioStreamInfoFlags_None) |
		(flattenSound ? EAudioStreamInfoFlags_FlattenSound : EAudioStreamInfoFlags_None);

	switch (magic) {

		case RIFFHeader_magic: {

			WAVFile file = (WAVFile) { 0 };
			gotoIfError3(clean, WAV_read(streamInfo.stream, 0, 0, alloc, &file, e_rr));

			U64 seconds = file.dataLength / file.fmt.bytesPerSecond;
			F64 nanos = (F64)(file.dataLength % file.fmt.bytesPerSecond) / ((F64)file.fmt.bytesPerSecond / SECOND);

			EAudioStreamFormat format;

			if(file.fmt.stride == 24)
				format = file.fmt.channels == 2 ? EAudioStreamFormat_Stereo24Ext : EAudioStreamFormat_Mono24Ext;

			else {

				if(file.fmt.stride == 32 && file.fmt.format == ERIFFAudioFormat_PCM)
					format = file.fmt.channels == 2 ? EAudioStreamFormat_Stereo32Ext : EAudioStreamFormat_Mono32Ext;

				else format = (AudioStreamFormat) EAudioStreamFormat_create(file.fmt.channels, file.fmt.stride >> 3);
			}

			streamInfo.flags4_format4 |= format << 4;

			streamInfo.duration = seconds * SECOND + (U64) nanos;
			streamInfo.sampleRate = file.fmt.frequency;
			streamInfo.dataStart = file.dataStart + inputStreamOffset;
			streamInfo.dataLengthLo32 = file.dataLength;
			streamInfo.dataLengthHi8 = 0;					//Standard WAV is 32-bit (for now)
			streamInfo.bytesPerSecond = file.fmt.bytesPerSecond;
			break;
		}

		//TODO: case OGGHeader_MAGIC:
		//TODO: case MP3Header_MAGIC:
		//TODO: case WMAHeader_MAGIC:
		//TODO: FLAC/M4A/AAC/AIFF?

		default:
			retError(clean, Error_invalidParameter(1, 0, "AudioDeviceRef_createFileStream() file format unsupported"));
	}

	gotoIfError3(clean, AudioDeviceRef_createStream(device, &streamInfo, startOffset, alloc, type, outStream, e_rr));

clean:
	if(!s_uccess)
		RefPtr_dec(&streamInfo.stream);

	return s_uccess;
}
