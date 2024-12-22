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

#include "audio/stream.h"
#include "audio/interface.h"
#include "audio/device.h"
#include "platforms/platform.h"
#include "platforms/file.h"
#include "formats/wav/wav.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"

U8 EAudioStreamFormat_getChannels(EAudioStreamFormat format) {
	return (U8)(format & 1) + 1;
}

U8 EAudioStreamFormat_getStrideBytes(EAudioStreamFormat format) {

	if((format >> 1) > 3)	//U24
		return 3;

	return 1 << (U8)(format >> 1);
}

U8 EAudioStreamFormat_getSize(EAudioStreamFormat format) {
	return EAudioStreamFormat_getChannels(format) * EAudioStreamFormat_getStrideBytes(format);
}

Error AudioStreamRef_dec(AudioStreamRef **dev) {
	return !RefPtr_dec(dev) ?
		Error_invalidOperation(0, "AudioStreamRef_dec()::dev is invalid") : Error_none();
}

Error AudioStreamRef_inc(AudioStreamRef *dev) {
	return !RefPtr_inc(dev) ?
		Error_invalidOperation(0, "AudioStreamRef_inc()::dev is invalid") : Error_none();
}

Bool AudioDeviceRef_createStreamx(
	AudioDeviceRef *device,
	AudioStreamInfo *info,
	Ns startOffset,
	AudioStreamRef **stream,
	Error *e_rr
) {
	return AudioDeviceRef_createStream(device, info, startOffset, Platform_instance->alloc, stream, e_rr);
}

Bool AudioStreamRef_seekTime(AudioStreamRef *streamRef, Ns offset, Error *e_rr) {

	Bool s_uccess = true;

	if(!streamRef || streamRef->typeId != (ETypeId) EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(0, "AudioStreamRef_seekTime()::stream is required"))

	AudioStream *stream = AudioStreamRef_ptr(streamRef);

	if(offset >= stream->info.duration)
		retError(clean, Error_invalidParameter(1, 0, "AudioDeviceRef_createFileStream() duration offset out of bounds"))

	stream->timeOffset = offset;

	U64 stride = EAudioStreamFormat_getSize(stream->info.format);

	U64 streamOffset = offset / SECOND * stream->info.bytesPerSecond;
	streamOffset += (Ns)((offset % SECOND) * (stream->info.bytesPerSecond / (F64)SECOND));

	stream->streamOffset = (streamOffset + stride - 1) &~ (stride - 1);

clean:
	return s_uccess;
}

Bool AudioStreamRef_play(AudioStreamRef *streamRef, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	AudioDevice *dev = NULL;

	if(!streamRef || streamRef->typeId != (ETypeId) EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(0, "AudioStreamRef_play()::stream is required"))
		
	AudioStream *stream = AudioStreamRef_ptr(streamRef);

	dev = AudioDeviceRef_ptr(stream->device);
	acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioStreamRef_play() couldn't acquire device lock in time"))

	if(stream->isPlaying)
		goto clean;

	gotoIfError2(clean, ListWeakRefPtr_pushBack(&dev->streams, streamRef, alloc))
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

	if(!streamRef || streamRef->typeId != (ETypeId) EAudioTypeId_AudioStream)
		retError(clean, Error_nullPointer(0, "AudioStreamRef_stop()::stream is required"))
		
	AudioStream *stream = AudioStreamRef_ptr(streamRef);

	dev = AudioDeviceRef_ptr(stream->device);
	acq = SpinLock_lock(&dev->pendingUpdateLock, SECOND);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "AudioStreamRef_stop() couldn't acquire device lock in time"))

	if(!stream->isPlaying)
		goto clean;

	U64 id = ListWeakRefPtr_findFirst(dev->streams, streamRef, 0, NULL);

	if(id != U64_MAX)
		ListWeakRefPtr_erase(&dev->streams, id);

	gotoIfError3(clean, AudioStream_stopExt(stream, e_rr))
	stream->isPlaying = false;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	return s_uccess;
}

Bool AudioStreamRef_playx(AudioStreamRef *stream, Error *e_rr) {
	return AudioStreamRef_play(stream, Platform_instance->alloc, e_rr);
}

impl extern U32 AudioStream_sizeExt;
impl Bool AudioStream_createExt(AudioStream *stream, Allocator alloc, Error *e_rr);
impl Bool AudioStream_freeExt(AudioStream *stream, Allocator alloc);

Bool AudioStream_free(AudioStream *stream, Allocator alloc) {

	if(!stream)
		return true;

	AudioStream_freeExt(stream, alloc);
	Stream_close(&stream->info.stream, alloc);

	//Ensure stream isn't updated next time

	AudioStreamRef *streamRef = (AudioStreamRef*)stream - 1;

	AudioDevice *dev = AudioDeviceRef_ptr(stream->device);
	ELockAcquire acq = SpinLock_lock(&dev->pendingUpdateLock, U64_MAX);

	U64 id = ListWeakRefPtr_findFirst(dev->streams, streamRef, 0, NULL);

	if(id != U64_MAX)
		ListWeakRefPtr_erase(&dev->streams, id);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&dev->pendingUpdateLock);

	AudioDeviceRef_dec(&stream->device);

	return true;
}

Bool AudioDeviceRef_createStream(
	AudioDeviceRef *device,
	AudioStreamInfo *info,
	Ns startOffset,
	Allocator alloc,
	AudioStreamRef **stream,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool madeRef = false;

	if(!device || device->typeId != (ETypeId) EAudioTypeId_AudioDevice)
		retError(clean, Error_nullPointer(0, "AudioDeviceRef_createStream()::device is required"))

	if(!info)
		retError(clean, Error_nullPointer(1, "AudioDeviceRef_createStream()::info is required"))

	if(!info->sampleRate || !info->bytesPerSecond)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream()::info.sampleRate and bytesPerSecond are required"
		))

	if(info->format >= EAudioStreamFormat_Count)
		retError(clean, Error_outOfBounds(
			0, info->format, EAudioStreamFormat_Count, "AudioDeviceRef_createStream()::info->formatId is invalid"
		))
	
	U8 bytesPerBlock = EAudioStreamFormat_getSize(info->format);

	if((U64)info->sampleRate * bytesPerBlock != info->bytesPerSecond)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream()::info.sampleRate is invalid"
		))

	if(!info->stream.cacheData.ptr)
		retError(clean, Error_invalidParameter(1, 0, "AudioDeviceRef_createStream()::info.stream is required"))

	if(info->dataStart + info->dataLength > info->stream.handle.fileSize)
		retError(clean, Error_outOfBounds(
			0, info->dataStart + info->dataLength, info->stream.handle.fileSize,
			"AudioDeviceRef_createStream()::info dataStart + dataLength out of bounds"
		))

	if(info->streamLength && info->streamLength < 64 * KIBI)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream()::info.streamLength is too small"
		))

	if(!info->streamLength)
		info->streamLength = info->bytesPerSecond;

	if(!info->pitch)
		info->pitch = 1;

	if(info->pitch < 0)
		retError(clean, Error_invalidParameter(
			1, 0, "AudioDeviceRef_createStream() reversing playing audio not supported yet"
		))

	gotoIfError2(clean, RefPtr_create(
		(U32)(sizeof(AudioStream) + AudioStream_sizeExt),
		alloc,
		(ObjectFreeFunc) AudioStream_free,
		(ETypeId) EAudioTypeId_AudioStream,
		stream
	))

	gotoIfError2(clean, AudioDeviceRef_inc(device))

	*AudioStreamRef_ptr(*stream) = (AudioStream) {
		.device = device,
		.info = *info
	};

	*info = (AudioStreamInfo) { 0 };

	gotoIfError3(clean, AudioStreamRef_seekTime(*stream, startOffset, e_rr))
	gotoIfError3(clean, AudioStream_createExt(AudioStreamRef_ptr(*stream), alloc, e_rr))

clean:

	if(madeRef && !s_uccess)
		AudioStreamRef_dec(stream);

	return s_uccess;
}

Bool AudioDeviceRef_createFileStream(
	AudioDeviceRef *device,
	CharString path,
	Bool isLoop,
	Ns startOffset,
	F32 pitch,
	Allocator alloc,
	AudioStreamRef **stream,
	Error *e_rr
) {

	Bool s_uccess = true;

	AudioStreamInfo streamInfo = (AudioStreamInfo) { 0 };
	U32 magic = 0;

	if(pitch < 0)
		retError(clean, Error_invalidState(0, "AudioDeviceRef_createFileStream() negative pitch not supported yet"))	//TODO:

	gotoIfError3(clean, File_openStream(path, 1 * MS, EFileOpenType_Read, false, 256 * KIBI, alloc, &streamInfo.stream, e_rr))
	gotoIfError3(clean, Stream_read(&streamInfo.stream, Buffer_createRef(&magic, sizeof(magic)), 0, 0, 0, false, e_rr))

	streamInfo.pitch = !pitch ? 1 : pitch;
	streamInfo.isLoop = isLoop;

	switch (magic) {

		case RIFFHeader_magic: {

			WAVFile file = (WAVFile) { 0 };
			gotoIfError3(clean, WAV_read(&streamInfo.stream, 0, 0, alloc, &file, e_rr))

			U64 seconds = file.dataLength / file.fmt.bytesPerSecond;
			F64 nanos = (F64)(file.dataLength % file.fmt.bytesPerSecond) / ((F64)file.fmt.bytesPerSecond / SECOND);

			if(file.fmt.stride == 24)
				streamInfo.format = file.fmt.channels == 2 ? EAudioStreamFormat_Stereo24Ext : EAudioStreamFormat_Mono24Ext;

			else streamInfo.format = (AudioStreamFormat) EAudioStreamFormat_create(file.fmt.channels, file.fmt.stride >> 3);

			streamInfo.duration = seconds * SECOND + (U64) nanos;
			streamInfo.sampleRate = file.fmt.frequency;
			streamInfo.dataStart = file.dataStart;
			streamInfo.dataLength = file.dataLength;
			streamInfo.bytesPerSecond = file.fmt.bytesPerSecond;
			break;
		}

		//case OGGHeader_MAGIC:
		//case MP3Header_MAGIC:
		//case WMAHeader_MAGIC:
		//FLAC/M4A/AAC/AIFF?

		default:
			retError(clean, Error_invalidParameter(1, 0, "AudioDeviceRef_createFileStream() file format unsupported"))
	}

	gotoIfError3(clean, AudioDeviceRef_createStream(device, &streamInfo, startOffset, alloc, stream, e_rr))

clean:

	if(!s_uccess)		//Only keep file open if the audio stream is still alive
		Stream_close(&streamInfo.stream, alloc);

	return s_uccess;
}

Bool AudioDeviceRef_createFileStreamx(
	AudioDeviceRef *device,
	CharString path,
	Bool isLoop,
	Ns startOffset,
	F32 pitch,
	AudioStreamRef **stream,
	Error *e_rr
) {
	return AudioDeviceRef_createFileStream(device, path, isLoop, startOffset, pitch, Platform_instance->alloc, stream, e_rr);
}
