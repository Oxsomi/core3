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

//audio/test/interface/test_audio_interface_main.c

#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "audio/audio_source.h"
#include "audio/openal_soft/openal_soft.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/memory_stream.h"
#include "formats/wav/wav_file.h"
#include "types/container/log.h"
#include "types/math/vec4f.h"
#include "types/math/type_cast.h"
#include "types/test/test.h"
#include "types/base/time.h"
#include "types/base/mathi.h"
#include "types/base/thread.h"

#ifdef AUDIO_TEST_DEBUG
	#include "platforms/platform.h"
#endif

typedef struct AudioIfCtx {
	AudioInterfaceRef *interf;
	AudioDeviceRef *device;
} AudioIfCtx;

//Set by Test_audioInterfaceGetDeviceInfos: false in headless environments (e.g. CI with no audio hardware),
// where device enumeration returns zero devices. main() then skips the device-dependent modules.
static Bool hasAudioDevice = false;

static Bool AudioIfCtx_create(
	AudioIfCtx *ctx,
	const Allocator *alloc,
	const RefPtrType *ifType,
	const RefPtrType *devType,
	Error *e_rr
) {
 
	Bool s_uccess = true;
	AudioDeviceInfo info = (AudioDeviceInfo) { 0 };
 
	gotoIfError3(clean, AudioInterface_create(&ctx->interf, alloc, ifType, e_rr));
	gotoIfError3(clean, AudioInterface_getPreferredDevice(
		AudioInterfaceRef_ptr(ctx->interf), EAudioDeviceFlags_None, alloc, &info, e_rr
	));
 
	gotoIfError3(clean, AudioDeviceRef_create(ctx->interf, &info, false, alloc, devType, &ctx->device, e_rr));
 
clean:
	return s_uccess;
}

static void AudioIfCtx_free(AudioIfCtx *ctx) {
	RefPtr_dec(&ctx->device);
	RefPtr_dec(&ctx->interf);
}

//AudioInterface lifecycle

void Test_audioInterfaceCreateDestroy(Test *t) {
 
	Test_setModule(t, "AudioInterface_createDestroy");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
 
	RefPtrType type = AudioInterface_makeType(t->alloc);
	Test_assert(t, "AudioInterface_create succeeds", AudioInterface_create(&interf, t->alloc, &type, &err));

	Test_assert(t, "non-null after create", interf != NULL);
 
	RefPtr_dec(&interf);
	Test_assert(t, "null after dec", interf == NULL);
}

void Test_audioInterfaceDoubleDecSafe(Test *t) {
 
	Test_setModule(t, "AudioInterface_doubleDecSafe");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
 
	RefPtrType type = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create", AudioInterface_create(&interf, t->alloc, &type, &err));
 
	AudioInterfaceRef *copy = interf;

	RefPtr_inc(copy);
	RefPtr_dec(&copy);
	Test_assert(t, "alive after one dec", interf != NULL && AtomicI64_load(&interf->refCount) == 1);
 
	RefPtr_dec(&interf);
	Test_assert(t, "freed after second dec", interf == NULL);
}

void Test_audioInterfaceInvalidType(Test *t) {
 
	Test_setModule(t, "AudioInterface_invalidType");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
 
	RefPtrType type = AudioInterface_makeType(t->alloc);
	type.typeId = (TypeId)0xDEAD;
 
	Test_assert(t, "create fails on bad typeId", !AudioInterface_create(&interf, t->alloc, &type, &err));
	Test_assert(t, "no allocation on failure", interf == NULL);
}

//AudioInterface device querying

void Test_audioInterfaceGetDeviceInfos(Test *t) {
 
	Test_setModule(t, "AudioInterface_getDeviceInfos");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	ListAudioDeviceInfo devices = (ListAudioDeviceInfo){ 0 };
 
	RefPtrType type = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create", AudioInterface_create(&interf, t->alloc, &type, &err));
 
	Test_assert(t, "getDeviceInfos succeeds",
		AudioInterface_getDeviceInfos(AudioInterfaceRef_ptr(interf), t->alloc, &devices, &err)
	);

	//A headless environment (CI with no audio hardware) enumerates zero devices.
	//Record that and skip the device-presence checks instead of failing them.
	//main() skips the device-dependent modules accordingly.
	hasAudioDevice = devices.length >= 1;

	if (!hasAudioDevice)
		Log_debugLn(t->alloc, "No audio devices found (headless environment); skipping audio device tests");

	else {

		U64 mainCount = 0;

		for (U64 i = 0; i < devices.length; ++i)
			if (devices.ptr[i].flags & EAudioDeviceFlags_MainOutput)
				++mainCount;

		Test_assert(t, "exactly one MainOutput", mainCount == 1);

		for (U64 i = 0; i < devices.length; ++i)
			Test_assert(t, "device name non-empty", devices.ptr[i].name[0] != '\0');

		//OpenAL Soft names AL_EXT_FLOAT32 and AL_EXT_DOUBLE on every context, emscripten only the first.

		#if _PLATFORM_TYPE == PLATFORM_WEB
			const EAudioDeviceFlags floats = EAudioDeviceFlags_HasF32Ext;
		#else
			const EAudioDeviceFlags floats = (EAudioDeviceFlags) (EAudioDeviceFlags_HasF32Ext | EAudioDeviceFlags_HasF64Ext);
		#endif

		for (U64 i = 0; i < devices.length; ++i)
			Test_assert(t, "float formats reported", (devices.ptr[i].flags & floats) == floats);
	}

	ListAudioDeviceInfo_free(&devices, t->alloc);
	RefPtr_dec(&interf);
}

void Test_audioInterfaceGetPreferredDevice(Test *t) {
 
	Test_setModule(t, "AudioInterface_getPreferredDevice");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	AudioDeviceInfo info = (AudioDeviceInfo){ 0 };
 
	RefPtrType type = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create", AudioInterface_create(&interf, t->alloc, &type, &err));
 
	Test_assert(t, "getPreferredDevice succeeds",
		AudioInterface_getPreferredDevice(AudioInterfaceRef_ptr(interf), EAudioDeviceFlags_None, t->alloc, &info, &err)
	);

	Test_assert(t, "has MainOutput flag", info.flags & EAudioDeviceFlags_MainOutput);
	Test_assert(t, "name non-empty", info.name[0] != '\0');
 
	RefPtr_dec(&interf);
}

void Test_audioInterfaceGetPreferredDeviceNullOut(Test *t) {
 
	Test_setModule(t, "AudioInterface_getPreferredDevice_nullOut");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
 
	RefPtrType type = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create", AudioInterface_create(&interf, t->alloc, &type, &err));
 
	Test_assert(t, "fails on NULL out",
		!AudioInterface_getPreferredDevice(AudioInterfaceRef_ptr(interf), EAudioDeviceFlags_None,
			t->alloc, NULL, &err
		)
	);
 
	RefPtr_dec(&interf);
}

//AudioDevice lifecycle

void Test_audioDeviceCreateDestroy(Test *t) {
 
	Test_setModule(t, "AudioDevice_createDestroy");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	AudioDeviceRef *device = NULL;
	AudioDeviceInfo info = (AudioDeviceInfo) { 0 };

	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create interface", AudioInterface_create(&interf, t->alloc, &ifType, &err));

	Test_assert(t, "get preferred device",
		AudioInterface_getPreferredDevice(AudioInterfaceRef_ptr(interf), EAudioDeviceFlags_None, t->alloc, &info, &err)
	);
 
	RefPtrType devType = AudioDevice_makeType(t->alloc);
	Test_assert(t, "create device", AudioDeviceRef_create(interf, &info, false, t->alloc, &devType, &device, &err));
	Test_assert(t, "device non-null", device != NULL);

	RefPtr_dec(&device);
	Test_assert(t, "device freed", device == NULL);
	RefPtr_dec(&interf);
}

void Test_audioDeviceCreateDebug(Test *t) {
 
	Test_setModule(t, "AudioDevice_createDebug");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	AudioDeviceRef *device = NULL;
	AudioDeviceInfo info = (AudioDeviceInfo){ 0 };
 
	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create interface", AudioInterface_create(&interf, t->alloc, &ifType, &err));
	Test_assert(t, "get preferred device",
		AudioInterface_getPreferredDevice(AudioInterfaceRef_ptr(interf), EAudioDeviceFlags_None, t->alloc, &info, &err)
	);
 
	if (!(info.flags & EAudioDeviceFlags_Debug)) {
		RefPtr_dec(&interf);
		return;        //Skip: debug extension not available
	}
 
	RefPtrType devType = AudioDevice_makeType(t->alloc);
	Test_assert(t, "create debug device", AudioDeviceRef_create(interf, &info, true, t->alloc, &devType, &device, &err));

	Test_assert(t, "debug device non-null", device != NULL);
 
	RefPtr_dec(&device);
	RefPtr_dec(&interf);
}

void Test_audioDeviceInvalidType(Test *t) {
 
	Test_setModule(t, "AudioDevice_invalidType");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	AudioDeviceRef *device = NULL;
	AudioDeviceInfo info = (AudioDeviceInfo){ 0 };
 
	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create interface", AudioInterface_create(&interf, t->alloc, &ifType, &err));
	Test_assert(t, "get preferred device",
		AudioInterface_getPreferredDevice(AudioInterfaceRef_ptr(interf), EAudioDeviceFlags_None, t->alloc, &info, &err)
	);
 
	RefPtrType devType = AudioDevice_makeType(t->alloc);
	devType.typeId = (TypeId)0xDEAD;
 
	Test_assert(t, "create fails on bad typeId",
		!AudioDeviceRef_create(interf, &info, false, t->alloc, &devType, &device, &err)
	);

	Test_assert(t, "no allocation on failure", device == NULL);
 
	RefPtr_dec(&interf);
}

void Test_audioDeviceNullInfo(Test *t) {
 
	Test_setModule(t, "AudioDevice_nullInfo");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	AudioDeviceRef *device = NULL;
 
	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create interface", AudioInterface_create(&interf, t->alloc, &ifType, &err));
 
	RefPtrType devType = AudioDevice_makeType(t->alloc);
	Test_assert(t, "create fails on null info",
		!AudioDeviceRef_create(interf, NULL, false, t->alloc, &devType, &device, &err)
	);

	Test_assert(t, "no allocation on failure", device == NULL);
 
	RefPtr_dec(&interf);
}

//AudioDevice listener transform

void Test_audioDeviceListenerTransform(Test *t) {
 
	Test_setModule(t, "AudioDevice_listenerTransform");
 
	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	RefPtrType devType = AudioDevice_makeType(t->alloc);
 
	Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &ifType, &devType, &err));
 
	Test_assert(t, "updateListenerPosition", AudioDeviceRef_updateListenerPosition(ctx.device, F32x4_create3(1, 2, 3), &err));
	Test_assert(t, "updateListenerForward", AudioDeviceRef_updateListenerForward(ctx.device, F32x4_create3(0, 0, 1), &err));
	Test_assert(t, "updateListenerUp", AudioDeviceRef_updateListenerUp(ctx.device, F32x4_create3(0, 1, 0), &err));
	Test_assert(t, "updateListenerVelocity", AudioDeviceRef_updateListenerVelocity(ctx.device, F32x4_create3(1, 0, 0), &err));
	Test_assert(t, "updateListenerGain", AudioDeviceRef_updateListenerGain(ctx.device, 1, &err));
	Test_assert(t, "updateListenerOrientation",
		AudioDeviceRef_updateListenerOrientation(ctx.device, F32x4_create3(0, 0, 1), F32x4_create3(0, 1, 0), &err)
	);
 
	Test_assert(t, "first update flushes dirty mask", AudioDeviceRef_update(ctx.device, t->alloc, &err));
	Test_assert(t, "second update is a no-op", AudioDeviceRef_update(ctx.device, t->alloc, &err));
 
	AudioIfCtx_free(&ctx);
}

void Test_audioDeviceListenerTransformNull(Test *t) {
 
	Test_setModule(t, "AudioDevice_listenerTransform_null");
 
	Error err = Error_none();
	Test_assert(t, "fails on null device",
		!AudioDeviceRef_updateListenerTransform(NULL, F32x4_zero(), F32x4_zero(), F32x4_zero(), F32x4_zero(), 0, 31, &err)
	);
}

//AudioDeviceInfo print (smoke test, must not crash)

void Test_audioDeviceInfoPrint(Test *t) {
 
	Test_setModule(t, "AudioDeviceInfo_print");
 
	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	ListAudioDeviceInfo devices = (ListAudioDeviceInfo){ 0 };
 
	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	Test_assert(t, "create interface", AudioInterface_create(&interf, t->alloc, &ifType, &err));
	Test_assert(t, "getDeviceInfos", AudioInterface_getDeviceInfos(AudioInterfaceRef_ptr(interf), t->alloc, &devices, &err));
 
	for (U64 i = 0; i < devices.length; ++i)
		AudioDeviceInfo_print(devices.ptr[i], t->alloc);
 
	ListAudioDeviceInfo_free(&devices, t->alloc);
	RefPtr_dec(&interf);
}

//Source creation rejection (no audio playback, purely structural checks)

void Test_audioSourceStereoSpatialRejected(Test *t) {

	Test_setModule(t, "AudioSource_stereoSpatialRejected");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx){ 0 };
	MemoryStreamRef *memStream = NULL;
	AudioStreamRef *audioStream = NULL;
	AudioSourceRef *source = NULL;

	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	RefPtrType devType = AudioDevice_makeType(t->alloc);

	Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &ifType, &devType, &err));

	//Minimal stereo silence , just enough bytes for one stereo16 block (4 bytes)
	static const U8 silence[4] = { 0 };
	Buffer silenceBuf = Buffer_createRefConst(silence, sizeof(silence));

	RefPtrType memType = MemoryStream_makeType(t->alloc);
	Test_assert(t, "create silence stream",
		MemoryStream_createFromBufferRegion(silenceBuf, 0, 4, EMemoryStreamFlags_None, &memType, &memStream, &err)
	);

	RefPtr_inc(memStream);

	AudioStreamInfo info = (AudioStreamInfo) {
		.flags4_format4 = EAudioStreamFormat_Stereo16 << 4,
		.sampleRate = 44100,
		.bytesPerSecond = 44100 * 4,
		.dataLengthLo32 = sizeof(silence),
		.loops = 1,
		.pitch = 1,
		.stream = memStream
	};

	RefPtrType streamType = AudioStream_makeType(t->alloc);
	Test_assert(t, "createStream succeeds for stereo",
		AudioDeviceRef_createStream(ctx.device, &info, 0, t->alloc, &streamType, &audioStream, &err)
	);

	//Now attempt spatial source on stereo stream without flattenSound -- must be rejected
	AudioModifier modifier = (AudioModifier) { .gain = 1 };
	AudioPoint3D point = (AudioPoint3D) {
		.pos = F32x4_create3(5, 0, 0),
		.velocity = F32x4_zero()
	};

	RefPtrType sourceType = AudioSource_makeType(t->alloc);
	Bool result = AudioDeviceRef_createSource3D(
		ctx.device, audioStream, modifier, point, t->alloc, &sourceType, &source, &err
	);

	Test_assert(t, "createSource3D rejected for stereo spatial", !result);
	Test_assert(t, "no source allocated on rejection", source == NULL);

	RefPtr_dec(&audioStream);
	RefPtr_dec(&memStream);
	RefPtr_dec(&info.stream);
	AudioIfCtx_free(&ctx);
}

//A WAV inside a larger stream, as in an archive, is parsed where it starts and its samples are found there

void Test_audioDeviceFromFileAtOffset(Test *t) {

	Test_setModule(t, "AudioDevice_createFromFile at an offset");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	MemoryStreamRef *samples = NULL, *file = NULL;
	AudioStreamRef *audioStream = NULL;
	WAVFile wav = (WAVFile) { 0 };

	static const U8 silence[4096] = { 0 };
	const U64 padding = 100;

	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	RefPtrType devType = AudioDevice_makeType(t->alloc);
	RefPtrType memType = MemoryStream_makeType(t->alloc);
	RefPtrType streamType = AudioStream_makeType(t->alloc);

	if (
		Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &ifType, &devType, &err)) &&
		Test_assert(t, "samples", MemoryStream_createFromBufferRegion(
			Buffer_createRefConst(silence, sizeof(silence)), 0, 0, EMemoryStreamFlags_None, &memType, &samples, &err
		)) &&
		Test_assert(t, "padding", MemoryStream_create(padding, EMemoryStreamFlags_WriteResize, &memType, &file, &err)) &&
		Test_assert(t, "write wav after padding", WAV_write(
			file, samples, padding, 0, sizeof(silence), false, 44100, 16, true, NULL, t->alloc, &err
		)) &&
		Test_assert(t, "read wav after padding", WAV_read(file, padding, 0, t->alloc, &wav, &err))
	) {

		if(Test_assert(t, "createFromFile at offset", AudioDeviceRef_createFromFile(
			ctx.device, file, padding, 1, 0, 1, false, t->alloc, &streamType, &audioStream, &err
		))) {
			const AudioStreamInfo *info = &AudioStreamRef_ptr(audioStream)->info;
			Test_assert(t, "dataStart is absolute", info->dataStart == wav.dataStart && wav.dataStart > padding);
			Test_assert(t, "data length", info->dataLengthLo32 == sizeof(silence) && info->sampleRate == 44100);
			Test_assert(t, "the audio stream holds its own reference", AtomicI64_load(&file->refCount) == 2);
		}

		//A failure, early or late, leaves the caller's reference alone

		AudioStreamRef *refused = NULL;

		Test_assert(t, "negative pitch refused", !AudioDeviceRef_createFromFile(
			ctx.device, file, padding, 1, 0, -1, false, t->alloc, &streamType, &refused, NULL
		));

		Test_assert(t, "WAV at the wrong offset refused", !AudioDeviceRef_createFromFile(
			ctx.device, file, 0, 1, 0, 1, false, t->alloc, &streamType, &refused, NULL
		));

		Test_assert(
			t, "failures keep the caller's reference", !refused && AtomicI64_load(&file->refCount) == 1 + !!audioStream
		);
	}

	RefPtr_dec(&audioStream);
	RefPtr_dec(&file);
	RefPtr_dec(&samples);
	AudioIfCtx_free(&ctx);
}

//Listener position steps (verify dirty mask fires correctly each interval)
 
void Test_audioDeviceListenerPositionSteps(Test *t) {
 
	Test_setModule(t, "AudioDevice_listenerPositionSteps");
 
	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	RefPtrType ifType = AudioInterface_makeType(t->alloc);
	RefPtrType devType = AudioDevice_makeType(t->alloc);
 
	Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &ifType, &devType, &err));

	AudioDevice *device = RefPtr_data(ctx.device, AudioDevice);
 
	//Step listener position through several values, flushing each time
	F32 positions[][3] = {
		{ 0, 0, 0 },
		{ 1, 0, 0 },
		{ 2, 0, 0 },
		{ 1, 0, 0 },
		{ 0, 0, 0 }
	};
 
	for (U64 i = 0; i < 5; ++i) {

		Test_assert(t, "updateListenerPosition", AudioDeviceRef_updateListenerPosition(
			ctx.device, F32x4_create3(positions[i][0], positions[i][1], positions[i][2]), &err
		));

		Test_assert(t, "update", AudioDeviceRef_update(ctx.device, t->alloc, &err));
	}
 
	//Setting the same value twice should not mark dirty the second time

	Test_assert(t, "set position A", AudioDeviceRef_updateListenerPosition(ctx.device, F32x4_create3(5, 0, 0), &err));
	Test_assert(t, "expect dirty mask set", device && device->pendingDirtyMask);
	Test_assert(t, "update A", AudioDeviceRef_update(ctx.device, t->alloc, &err));
	Test_assert(t, "expect dirty mask cleared after update", device && !device->pendingDirtyMask);
 
	Test_assert(t, "set same position again", AudioDeviceRef_updateListenerPosition(ctx.device, F32x4_create3(5, 0, 0), &err));

	Test_assert(t, "expect dirty mask cleared", device && !device->pendingDirtyMask);
	Test_assert(t, "update B (should be no-op)", AudioDeviceRef_update(ctx.device, t->alloc, &err));
 
	AudioIfCtx_free(&ctx);
}

//Streams and sources made from a few bytes of silence, to check how the module treats what it is given

typedef struct AudioIfTypes {
	RefPtrType ifType, devType, memType, streamType, sourceType;
} AudioIfTypes;

static AudioIfTypes AudioIfTypes_create(const Allocator *alloc) {
	return (AudioIfTypes) {
		.ifType = AudioInterface_makeType(alloc),
		.devType = AudioDevice_makeType(alloc),
		.memType = MemoryStream_makeType(alloc),
		.streamType = AudioStream_makeType(alloc),
		.sourceType = AudioSource_makeType(alloc)
	};
}

//The stream is made over the given data, which has to outlive the stream.

static Bool AudioIfTest_makeStreamData(
	Test *t,
	AudioDeviceRef *device,
	AudioIfTypes *types,
	AudioStreamInfo info,
	Ns startOffset,
	const void *data,
	U64 dataSize,
	AudioStreamRef **stream
) {

	MemoryStreamRef *mem = NULL;
	Error err = Error_none();

	if (!MemoryStream_createFromBufferRegion(
		Buffer_createRefConst(data, dataSize), 0, 0, EMemoryStreamFlags_None, &types->memType, &mem, &err
	))
		return false;

	RefPtr_inc(mem);
	info.stream = mem;

	Bool ok = AudioDeviceRef_createStream(device, &info, startOffset, t->alloc, &types->streamType, stream, &err);

	//A refused call leaves the stream reference with the caller, a successful one zeroes info.

	if (!ok)
		RefPtr_dec(&info.stream);

	RefPtr_dec(&mem);
	return ok;
}

//The stream is made over 2 MiB of silence, of which info says how many are audio.

static Bool AudioIfTest_makeStream(
	Test *t,
	AudioDeviceRef *device,
	AudioIfTypes *types,
	AudioStreamInfo info,
	Ns startOffset,
	AudioStreamRef **stream
) {
	static const U8 silence[2 * 1024 * 1024] = { 0 };
	return AudioIfTest_makeStreamData(t, device, types, info, startOffset, silence, sizeof(silence), stream);
}

static AudioStreamInfo AudioIfTest_monoInfo(U32 dataLength) {
	return (AudioStreamInfo) {
		.flags4_format4 = EAudioStreamFormat_Mono16 << 4,
		.sampleRate = 44100,
		.bytesPerSecond = 44100 * 2,
		.dataLengthLo32 = dataLength,
		.loops = 1,
		.pitch = 1
	};
}

static Bool AudioIfTest_makeSource3D(
	Test *t, AudioIfCtx *ctx, AudioIfTypes *types, AudioStreamRef *stream, AudioModifier modifier, AudioPoint3D point,
	AudioSourceRef **source, Error *e_rr
) {
	return AudioDeviceRef_createSource3D(ctx->device, stream, modifier, point, t->alloc, &types->sourceType, source, e_rr);
}

//A source released before the next update must be gone from the device's pending list

void Test_audioSourceReleasedBeforeUpdate(Test *t) {

	Test_setModule(t, "AudioSource_releasedBeforeUpdate");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL;
	AudioSourceRef *source = NULL;

	if (
		Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err)) &&
		Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, AudioIfTest_monoInfo(32), 0, &stream))
	) {
		AudioDevice *dev = AudioDeviceRef_ptr(ctx.device);

		Test_assert(t, "create source", AudioDeviceRef_createSource(
			ctx.device, stream, (AudioModifier) { .gain = 1 }, t->alloc, &types.sourceType, &source, &err
		));

		Test_assert(t, "a new source is pending", dev->pendingSources.length == 1 && dev->pendingSources.ptr[0] == source);

		RefPtr_dec(&source);

		Test_assert(t, "a released source is not pending", dev->pendingSources.length == 0);
		Test_assert(t, "update after the release", AudioDeviceRef_update(ctx.device, t->alloc, &err));
	}

	RefPtr_dec(&stream);
	AudioIfCtx_free(&ctx);
}

//Changes that are small but not nothing are sent

void Test_audioSmallChanges(Test *t) {

	Test_setModule(t, "AudioSource_smallChanges");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL;
	AudioSourceRef *source = NULL;

	if (
		Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err)) &&
		Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, AudioIfTest_monoInfo(32), 0, &stream)) &&
		Test_assert(t, "create source", AudioIfTest_makeSource3D(
			t, &ctx, &types, stream, (AudioModifier) { .gain = 1 }, (AudioPoint3D) { 0 }, &source, &err
		))
	) {
		AudioDevice *dev = AudioDeviceRef_ptr(ctx.device);
		AudioSource *src = AudioSourceRef_ptr(source);
		AudioDeviceRef *devRef = ctx.device;

		Test_assert(t, "flush", AudioDeviceRef_update(devRef, t->alloc, &err));
		Test_assert(t, "flushed", !dev->pendingDirtyMask && !src->dirtyMask && !dev->pendingSources.length);

		Test_assert(t, "source gain 0.5", AudioSourceRef_updateGain(source, 0.5f, t->alloc, &err));
		Test_assert(t, "flush source gain", AudioDeviceRef_update(devRef, t->alloc, &err));
		Test_assert(t, "source gain 0.505", AudioSourceRef_updateGain(source, 0.505f, t->alloc, &err));
		Test_assert(t, "source gain 0.505 queued", (src->dirtyMask & 1) && dev->pendingSources.length == 1);
		Test_assert(t, "source gain 0.505 stored", src->modifier.gain == 0.505f);
		Test_assert(t, "flush source gain nudge", AudioDeviceRef_update(devRef, t->alloc, &err));

		Test_assert(t, "source gain again", AudioSourceRef_updateGain(source, 0.505f, t->alloc, &err));
		Test_assert(t, "same source gain is not queued", !src->dirtyMask && !dev->pendingSources.length);

		F32x4 pos = F32x4_create3(1, 0, 0), nudged = F32x4_create3(1.000001f, 0, 0);

		Test_assert(t, "source position", AudioSourceRef_updatePosition3D(source, pos, t->alloc, &err));
		Test_assert(t, "flush source position", AudioDeviceRef_update(devRef, t->alloc, &err));
		Test_assert(t, "source position nudge", AudioSourceRef_updatePosition3D(source, nudged, t->alloc, &err));
		Test_assert(t, "source position nudge queued", (src->dirtyMask & 4) && dev->pendingSources.length == 1);
		Test_assert(t, "flush source position nudge", AudioDeviceRef_update(devRef, t->alloc, &err));

		F32x4 vel = F32x4_create3(0, 2, 0), fasterVel = F32x4_create3(0, 2.000001f, 0);

		Test_assert(t, "source velocity", AudioSourceRef_updateVelocity3D(source, vel, t->alloc, &err));
		Test_assert(t, "flush source velocity", AudioDeviceRef_update(devRef, t->alloc, &err));
		Test_assert(t, "source velocity nudge", AudioSourceRef_updateVelocity3D(source, fasterVel, t->alloc, &err));
		Test_assert(t, "source velocity nudge queued", (src->dirtyMask & 8) && dev->pendingSources.length == 1);
		Test_assert(t, "flush source velocity nudge", AudioDeviceRef_update(devRef, t->alloc, &err));

		Test_assert(t, "listener gain 0.5", AudioDeviceRef_updateListenerGain(devRef, 0.5f, &err));
		Test_assert(t, "flush listener gain", AudioDeviceRef_update(devRef, t->alloc, &err));
		Test_assert(t, "listener gain 0.505", AudioDeviceRef_updateListenerGain(devRef, 0.505f, &err));
		Test_assert(t, "listener gain nudge queued", (dev->pendingDirtyMask & 16) && dev->info.gain == 0.505f);
		Test_assert(t, "flush listener gain nudge", AudioDeviceRef_update(devRef, t->alloc, &err));

		Test_assert(t, "listener position", AudioDeviceRef_updateListenerPosition(devRef, pos, &err));
		Test_assert(t, "flush listener position", AudioDeviceRef_update(devRef, t->alloc, &err));
		Test_assert(t, "listener position nudge", AudioDeviceRef_updateListenerPosition(devRef, nudged, &err));
		Test_assert(t, "listener position nudge queued", dev->pendingDirtyMask & 1);
		Test_assert(t, "flush listener position nudge", AudioDeviceRef_update(devRef, t->alloc, &err));

		Test_assert(t, "listener velocity", AudioDeviceRef_updateListenerVelocity(devRef, vel, &err));
		Test_assert(t, "flush listener velocity", AudioDeviceRef_update(devRef, t->alloc, &err));
		Test_assert(t, "listener velocity nudge", AudioDeviceRef_updateListenerVelocity(devRef, fasterVel, &err));
		Test_assert(t, "listener velocity nudge queued", dev->pendingDirtyMask & 8);
		Test_assert(t, "flush listener velocity nudge", AudioDeviceRef_update(devRef, t->alloc, &err));

		//Bits that select nothing are not kept

		Test_assert(t, "unused mask bits", AudioDeviceRef_updateListenerTransform(
			devRef, F32x4_zero(), F32x4_zero(), F32x4_zero(), F32x4_zero(), 0, 0xE0, &err
		));

		Test_assert(t, "unused mask bits are not pending", dev->pendingDirtyMask == 0);
	}

	RefPtr_dec(&source);
	RefPtr_dec(&stream);
	AudioIfCtx_free(&ctx);
}

//A value the backend would refuse is refused up front and changes nothing

void Test_audioRefusedValues(Test *t) {

	Test_setModule(t, "AudioSource_refusedValues");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL;
	AudioSourceRef *source = NULL;

	const F32 nan = F32_fromU32Bits(0x7FC00000);
	const F32 inf = F32_fromU32Bits(0x7F800000);

	AudioModifier unit = (AudioModifier) { .gain = 1 };

	if (
		Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err)) &&
		Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, AudioIfTest_monoInfo(32), 0, &stream)) &&
		Test_assert(t, "create source", AudioIfTest_makeSource3D(
			t, &ctx, &types, stream, unit, (AudioPoint3D) { 0 }, &source, &err
		))
	) {
		AudioDevice *dev = AudioDeviceRef_ptr(ctx.device);
		AudioSource *src = AudioSourceRef_ptr(source);
		AudioDeviceRef *devRef = ctx.device;

		Test_assert(t, "flush", AudioDeviceRef_update(devRef, t->alloc, &err));

		AudioPoint3D point = src->point;
		AudioModifier modifier = src->modifier;
		F32x4 listenerPos = dev->info.position;
		F32 listenerGain = dev->info.gain;

		F32x4 nanX = F32x4_create3(nan, 0, 0), infY = F32x4_create3(0, inf, 0);

		Test_assert(t, "negative gain refused", !AudioSourceRef_updateGain(source, -1, t->alloc, NULL));
		Test_assert(t, "NaN gain refused", !AudioSourceRef_updateGain(source, nan, t->alloc, NULL));
		Test_assert(t, "infinite gain refused", !AudioSourceRef_updateGain(source, inf, t->alloc, NULL));
		Test_assert(t, "NaN position refused", !AudioSourceRef_updatePosition3D(source, nanX, t->alloc, NULL));
		Test_assert(t, "infinite position refused", !AudioSourceRef_updatePosition3D(source, infY, t->alloc, NULL));
		Test_assert(t, "NaN velocity refused", !AudioSourceRef_updateVelocity3D(source, nanX, t->alloc, NULL));
		Test_assert(t, "NaN point refused", !AudioSourceRef_updatePoint3D(
			source, (AudioPoint3D) { .pos = F32x4_create3(2, 0, 0), .velocity = nanX }, t->alloc, NULL
		));

		Test_assert(t, "NaN modifier refused", !AudioSourceRef_updateModifierExt(
			source, (AudioModifier) { .gain = nan, .pitch = modifier.pitch }, t->alloc, NULL
		));

		Test_assert(t, "refused source values are not stored",
			src->modifier.gain == modifier.gain && src->modifier.pitch == modifier.pitch &&
			F32x4_eqExact4(src->point.pos, point.pos) && F32x4_eqExact4(src->point.velocity, point.velocity)
		);

		Test_assert(t, "refused source values are not queued", !src->dirtyMask && !dev->pendingSources.length);

		Test_assert(t, "negative listener gain refused", !AudioDeviceRef_updateListenerGain(devRef, -1, NULL));
		Test_assert(t, "NaN listener gain refused", !AudioDeviceRef_updateListenerGain(devRef, nan, NULL));
		Test_assert(t, "NaN listener position refused", !AudioDeviceRef_updateListenerPosition(devRef, nanX, NULL));
		Test_assert(t, "infinite listener velocity refused", !AudioDeviceRef_updateListenerVelocity(devRef, infY, NULL));
		Test_assert(t, "NaN listener orientation refused", !AudioDeviceRef_updateListenerOrientation(
			devRef, F32x4_create3(0, 0, -1), F32x4_create3(0, nan, 0), NULL
		));

		Test_assert(t, "refused listener values are not stored",
			dev->info.gain == listenerGain && F32x4_eqExact4(dev->info.position, listenerPos)
		);

		Test_assert(t, "refused listener values are not queued", !dev->pendingDirtyMask);
		Test_assert(t, "update after refusals", AudioDeviceRef_update(devRef, t->alloc, &err));

		//The pitch belongs to the stream

		Test_assert(t, "pitch update refused", !AudioSourceRef_updatePitchExt(source, 2, t->alloc, NULL));
		Test_assert(t, "NaN pitch update refused", !AudioSourceRef_updatePitchExt(source, nan, t->alloc, NULL));
		Test_assert(t, "pitch is kept", src->modifier.pitch == 1 && !src->dirtyMask && !dev->pendingSources.length);

		Test_assert(t, "modifier with another pitch refused", !AudioSourceRef_updateModifierExt(
			source, (AudioModifier) { .gain = 0.25f, .pitch = 2 }, t->alloc, NULL
		));

		Test_assert(t, "modifier with another pitch keeps the gain", src->modifier.gain == 1 && !src->dirtyMask);

		Test_assert(t, "modifier with the same pitch", AudioSourceRef_updateModifierExt(
			source, (AudioModifier) { .gain = 0.25f, .pitch = 1 }, t->alloc, &err
		));

		Test_assert(t, "modifier with the same pitch applies", src->modifier.gain == 0.25f && (src->dirtyMask & 1));
		Test_assert(t, "same pitch is a no-op", AudioSourceRef_updatePitchExt(source, 1, t->alloc, &err));
		Test_assert(t, "update after pitch", AudioDeviceRef_update(devRef, t->alloc, &err));

		//Creation takes the same rules

		AudioSourceRef *refused = NULL;
		AudioPoint3D badPos = (AudioPoint3D) { .pos = nanX }, badVel = (AudioPoint3D) { .velocity = infY };

		Test_assert(t, "negative gain refused at creation", !AudioDeviceRef_createSource(
			devRef, stream, (AudioModifier) { .gain = -1 }, t->alloc, &types.sourceType, &refused, NULL
		));

		Test_assert(t, "NaN gain refused at creation", !AudioDeviceRef_createSource(
			devRef, stream, (AudioModifier) { .gain = nan }, t->alloc, &types.sourceType, &refused, NULL
		));

		Test_assert(t, "pitch refused at creation", !AudioDeviceRef_createSource(
			devRef, stream, (AudioModifier) { .gain = 1, .pitch = 2 }, t->alloc, &types.sourceType, &refused, NULL
		));

		Test_assert(t, "NaN pitch refused at creation", !AudioDeviceRef_createSource(
			devRef, stream, (AudioModifier) { .gain = 1, .pitch = nan }, t->alloc, &types.sourceType, &refused, NULL
		));

		Test_assert(t, "NaN position refused at creation", !AudioIfTest_makeSource3D(
			t, &ctx, &types, stream, unit, badPos, &refused, NULL
		));

		Test_assert(t, "infinite velocity refused at creation", !AudioIfTest_makeSource3D(
			t, &ctx, &types, stream, unit, badVel, &refused, NULL
		));

		Test_assert(t, "creation refusals leave no source or pending entry", !refused && dev->pendingSources.length == 0);
	}

	RefPtr_dec(&source);
	RefPtr_dec(&stream);
	AudioIfCtx_free(&ctx);
}

//A sound can only be heard through the device it was made on

void Test_audioSourceOtherDevice(Test *t) {

	Test_setModule(t, "AudioSource_otherDevice");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL;
	AudioSourceRef *source = NULL;
	AudioDeviceRef *other = NULL;
	AudioStreamRef *otherStream = NULL;

	if (
		Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err)) &&
		Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, AudioIfTest_monoInfo(32), 0, &stream))
	) {
		AudioDeviceInfo info = AudioDeviceRef_ptr(ctx.device)->info;
		AudioModifier unit = (AudioModifier) { .gain = 1 };

		if (!AudioDeviceRef_create(ctx.interf, &info, false, t->alloc, &types.devType, &other, NULL))
			Log_debugLn(t->alloc, "Couldn't open the output twice; skipping the other device check");

		else {

			//The other device gets a stream of its own, so the backend's voice ids of both devices can coincide.
			//Without the check the source could then be made and silently drive the wrong voice.

			Test_assert(t, "create stream on the other device", AudioIfTest_makeStream(
				t, other, &types, AudioIfTest_monoInfo(32), 0, &otherStream
			));

			Test_assert(t, "a sound from another device is refused", !AudioDeviceRef_createSource(
				other, stream, unit, t->alloc, &types.sourceType, &source, NULL
			));

			Test_assert(t, "no source on refusal", !source && AudioDeviceRef_ptr(other)->pendingSources.length == 0);

			Test_assert(t, "the device of the sound accepts it", AudioDeviceRef_createSource(
				ctx.device, stream, unit, t->alloc, &types.sourceType, &source, &err
			));
		}
	}

	RefPtr_dec(&source);
	RefPtr_dec(&otherStream);
	RefPtr_dec(&stream);
	RefPtr_dec(&other);
	AudioIfCtx_free(&ctx);
}

//A stream without IsLoop plays once, so loops can only be 1; looping needs data to wrap around

void Test_audioStreamLoopsZero(Test *t) {

	Test_setModule(t, "AudioStream_loopsZero");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL;

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		AudioStreamInfo info = AudioIfTest_monoInfo(32);
		info.loops = 0;

		Test_assert(t, "0 loops without IsLoop refused", !AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream));
		Test_assert(t, "no stream on refusal", !stream);

		info.loops = 2;
		Test_assert(t, "2 loops without IsLoop refused", !AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream));
		Test_assert(t, "no stream on second refusal", !stream);

		info.loops = 0;
		info.flags4_format4 |= EAudioStreamInfoFlags_IsLoop;
		Test_assert(t, "0 loops with IsLoop is endless", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream));
		RefPtr_dec(&stream);

		info.loops = 3;
		Test_assert(t, "3 loops with IsLoop", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream));
		RefPtr_dec(&stream);

		AudioStreamInfo empty = AudioIfTest_monoInfo(0);
		empty.flags4_format4 |= EAudioStreamInfoFlags_IsLoop;
		empty.loops = 0;
		Test_assert(t, "empty data refused", !AudioIfTest_makeStream(t, ctx.device, &types, empty, 0, &stream));
		Test_assert(t, "no stream for empty data", !stream);

		info.flags4_format4 &= ~EAudioStreamInfoFlags_IsLoop;
		info.loops = 1;
		Test_assert(t, "1 loop without IsLoop", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream));
		RefPtr_dec(&stream);
	}

	AudioIfCtx_free(&ctx);
}

//Seeking stays inside the data, also when the data doesn't end on a frame

void Test_audioStreamSeekClamp(Test *t) {

	Test_setModule(t, "AudioStream_seekClamp");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL, *stream24 = NULL;

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		//Stereo 16 bit is 4 bytes a frame, so 10 bytes ends half way through a frame.
		//The duration is 10 bytes at 176400 bytes per second (56689.1 ns), rounded up.

		AudioStreamInfo info = AudioIfTest_monoInfo(10);
		info.flags4_format4 = EAudioStreamFormat_Stereo16 << 4;
		info.bytesPerSecond = 44100 * 4;
		info.duration = 56690;

		if (Test_assert(t, "create stream at the end", AudioIfTest_makeStream(t, ctx.device, &types, info, 56690, &stream))) {

			AudioStream *s = AudioStreamRef_ptr(stream);
			Test_assert(t, "stream offset stays in the data", s->streamOffset <= 10);

			Test_assert(t, "seek to the start", AudioStreamRef_seekTime(stream, 0, &err) && s->streamOffset == 0);
			Test_assert(t, "seek to the end", AudioStreamRef_seekTime(stream, info.duration, &err));
			Test_assert(t, "seek to the end stays in the data", s->streamOffset <= 10);
			Test_assert(t, "seek past the duration refused", !AudioStreamRef_seekTime(stream, info.duration + 1, NULL));
		}

		//24 bit frames are 3 bytes, which is not a power of two

		info = AudioIfTest_monoInfo(30);
		info.flags4_format4 = EAudioStreamFormat_Mono24Ext << 4;
		info.bytesPerSecond = 44100 * 3;
		info.duration = 200000;

		if (Test_assert(t, "create 24 bit stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 100000, &stream24)))
			Test_assert(t, "seek lands on a frame", AudioStreamRef_ptr(stream24)->streamOffset % 3 == 0);
	}

	RefPtr_dec(&stream24);
	RefPtr_dec(&stream);
	AudioIfCtx_free(&ctx);
}

//A stream queues 4 buffers of streamLength, and a seek is applied by the next update

void Test_audioStreamBuffers(Test *t) {

	Test_setModule(t, "AudioStream_buffers");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL, *stream24 = NULL;

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		const U64 length = 128 * 1024;

		AudioStreamInfo info = AudioIfTest_monoInfo(1024 * 1024);
		info.streamLength = (U32) length;
		info.duration = 11 * SECOND;

		if (Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream))) {

			AudioStream *s = AudioStreamRef_ptr(stream);

			Test_assert(t, "play", AudioStreamRef_play(stream, t->alloc, &err));
			Test_assert(t, "update", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "4 buffers of streamLength are read", s->streamOffset == 4 * length);

			//Half a second is 44100 bytes of 16 bit mono

			Test_assert(t, "seek", AudioStreamRef_seekTime(stream, SECOND / 2, &err));
			Test_assert(t, "seek makes no read", s->streamOffset == 44100);
			Test_assert(t, "update after seek", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "the seek is applied by the update", s->streamOffset == 44100 + 4 * length);
			Test_assert(t, "the stream keeps playing", s->isPlaying);

			Test_assert(t, "stop", AudioStreamRef_stop(stream, &err));
			Test_assert(t, "seek while stopped", AudioStreamRef_seekTime(stream, 0, &err));
			Test_assert(t, "update while stopped", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "a stopped stream stays stopped", !s->isPlaying && s->streamOffset == 0);
		}

		//24 bit frames are 3 bytes. The device converts them, so a buffer holds 3/2 as much source data as it plays.

		info = AudioIfTest_monoInfo(1024 * 1024);
		info.flags4_format4 = EAudioStreamFormat_Mono24Ext << 4;
		info.bytesPerSecond = 44100 * 3;
		info.streamLength = 100000;
		info.duration = 7 * SECOND;

		if (Test_assert(t, "create 24 bit stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream24))) {

			AudioStream *s = AudioStreamRef_ptr(stream24);

			Test_assert(t, "length is whole frames", s->info.streamLength == 99999);
			Test_assert(t, "play 24 bit", AudioStreamRef_play(stream24, t->alloc, &err));
			Test_assert(t, "update 24 bit", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "24 bit reads whole frames", s->streamOffset && s->streamOffset % 3 == 0);

			//Converted to 16 bit, the 99999 byte buffer plays 99999 bytes, which is 149997 bytes of source

			if (s->format != AudioStreamInfo_format(&s->info))
				Test_assert(t, "24 bit buffers are full", s->streamOffset == 4 * 149997);
		}
	}

	RefPtr_dec(&stream24);
	RefPtr_dec(&stream);
	AudioIfCtx_free(&ctx);
}

//Updates until the stream stops playing, but never longer than the bound. Returns whether it stopped; *updated is false
// if an update failed.

static Bool AudioIfTest_updateUntilEnded(Test *t, AudioIfCtx *ctx, AudioStream *s, Ns bound, Bool *updated) {

	Error err = Error_none();
	Ns start = Time_now();
	*updated = true;

	while (s->isPlaying && Time_now() - start < bound) {

		if (!AudioDeviceRef_update(ctx->device, t->alloc, &err)) {
			*updated = false;
			return false;
		}

		Thread_sleep(5 * MS);
	}

	return !s->isPlaying;
}

//A seek after the last pass was read must keep the stream going to its end instead of leaving it playing nothing

void Test_audioStreamSeekAfterLastPass(Test *t) {

	Test_setModule(t, "AudioStream_seekAfterLastPass");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL;

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		//20000 bytes of 16 bit mono, so one 64 KiB buffer holds both passes

		AudioStreamInfo info = AudioIfTest_monoInfo(20000);
		info.flags4_format4 |= EAudioStreamInfoFlags_IsLoop;
		info.loops = 2;
		info.streamLength = 64 * 1024;
		info.duration = 20000 * SECOND / info.bytesPerSecond;

		if (Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream))) {

			AudioStream *s = AudioStreamRef_ptr(stream);
			Bool updated = false;

			Test_assert(t, "play", AudioStreamRef_play(stream, t->alloc, &err));
			Test_assert(t, "update", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "both passes are read", s->loops == 2);

			//The middle is 10000 bytes in, which is 5000 frames of 44100 per second

			Test_assert(t, "seek", AudioStreamRef_seekTime(stream, 5000 * SECOND / 44100, &err));
			Test_assert(t, "update after seek", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "the seek refills from the middle", s->isPlaying && s->streamOffset != 10000 && s->loops == 2);

			Test_assert(t, "the stream ends", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated) && updated);
			Test_assert(t, "stream is not listed after it ends", AudioDeviceRef_ptr(ctx.device)->streams.length == 0);

			//Played to its end, seeking and playing again plays every pass again

			Test_assert(t, "seek to start", AudioStreamRef_seekTime(stream, 0, &err) && s->loops == 0);
			Test_assert(t, "play again", AudioStreamRef_play(stream, t->alloc, &err));
			Test_assert(t, "update again", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "it is refilled", s->isPlaying && s->loops == 2);
			Test_assert(t, "it ends again", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated) && updated);

			//Without a seek it stays at its end and a play ends at the next updates

			Test_assert(t, "play at the end", AudioStreamRef_play(stream, t->alloc, &err));
			Test_assert(t, "plays nothing more", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated) && updated);
		}
	}

	RefPtr_dec(&stream);
	AudioIfCtx_free(&ctx);
}

//A stream that played to its end can be played again after a seek, and a play without one ends again

void Test_audioStreamReplayAfterEnd(Test *t) {

	Test_setModule(t, "AudioStream_replayAfterEnd");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);
	AudioStreamRef *stream = NULL;

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		AudioStreamInfo info = AudioIfTest_monoInfo(20000);
		info.duration = 20000 * SECOND / info.bytesPerSecond;

		if (Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream))) {

			AudioStream *s = AudioStreamRef_ptr(stream);
			Bool updated = false;

			Test_assert(t, "play", AudioStreamRef_play(stream, t->alloc, &err));
			Test_assert(t, "plays to its end", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated) && updated);
			Test_assert(t, "stream is at its end", s->streamOffset == 20000);

			//The processed buffers are dropped when it ends, so a play can't replay them

			ALAudioStream *ext = AudioStream_ext(s, AL);
			Test_assert(t, "nothing stays queued", !ext->filledStream);

			Test_assert(t, "play at the end", AudioStreamRef_play(stream, t->alloc, &err));
			Test_assert(t, "ends without sound", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated) && updated);

			Test_assert(t, "seek to start", AudioStreamRef_seekTime(stream, 0, &err));
			Test_assert(t, "play after seek", AudioStreamRef_play(stream, t->alloc, &err));
			Test_assert(t, "update after seek", AudioDeviceRef_update(ctx.device, t->alloc, &err));
			Test_assert(t, "it is refilled from the start", s->isPlaying && s->streamOffset == 20000);
			Test_assert(t, "it ends again", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated) && updated);
		}
	}

	RefPtr_dec(&stream);
	AudioIfCtx_free(&ctx);
}

//Data that doesn't end on a frame is cut to whole frames, so the last buffer never holds a partial one

void Test_audioStreamPartialFrame(Test *t) {

	Test_setModule(t, "AudioStream_partialFrame");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		//Mono 24 and stereo 24 are converted to 16 bit, stereo 8 and 16 bit are read directly.

		struct { EAudioStreamFormat format; U32 length; U32 expected; } cases[] = {
			{ EAudioStreamFormat_Mono24Ext,    70001, 69999 },
			{ EAudioStreamFormat_Stereo24Ext,  70001, 69996 },
			{ EAudioStreamFormat_Stereo16,     70003, 70000 },
			{ EAudioStreamFormat_Stereo8,      70001, 70000 }
		};

		for (U64 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {

			AudioStreamRef *stream = NULL;
			U8 frame = EAudioStreamFormat_getSize(cases[i].format);

			AudioStreamInfo info = AudioIfTest_monoInfo(cases[i].length);
			info.flags4_format4 = (U8) (cases[i].format << 4);
			info.bytesPerSecond = 44100 * frame;
			info.streamLength = 64 * 1024;
			info.duration = (Ns) cases[i].expected * SECOND / info.bytesPerSecond;

			if (Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream))) {

				AudioStream *s = AudioStreamRef_ptr(stream);
				Bool updated = false;

				Test_assert(t, "data length is whole frames", AudioStreamInfo_dataLength(&s->info) == cases[i].expected);
				Test_assert(t, "play", AudioStreamRef_play(stream, t->alloc, &err));
				Test_assert(t, "plays to its end", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated));
				Test_assert(t, "no update failed", updated);
				Test_assert(t, "all data is read", s->streamOffset == cases[i].expected);
			}

			RefPtr_dec(&stream);
		}

		//Less than a frame is no data

		AudioStreamRef *stream = NULL;
		AudioStreamInfo info = AudioIfTest_monoInfo(1);
		info.flags4_format4 = EAudioStreamFormat_Stereo16 << 4;
		info.bytesPerSecond = 44100 * 4;

		Test_assert(t, "under one frame is refused", !AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream));
		RefPtr_dec(&stream);
	}

	AudioIfCtx_free(&ctx);
}

//A stereo stream folded to mono still reads streamLength bytes of output a buffer, which is more source data

void Test_audioStreamFlatten(Test *t) {

	Test_setModule(t, "AudioStream_flatten");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		const U64 length = 128 * 1024;

		EAudioStreamFormat formats[] = {
			EAudioStreamFormat_Stereo8, EAudioStreamFormat_Stereo16, EAudioStreamFormat_Stereo24Ext,
			EAudioStreamFormat_Stereo32Ext, EAudioStreamFormat_Stereo32fExt, EAudioStreamFormat_Stereo64fExt
		};

		const C8 *names[] = { "8b", "16b", "24b", "32b", "32f", "64f" };

		for (U64 i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {

			AudioStreamRef *stream = NULL;
			U8 frame = EAudioStreamFormat_getSize(formats[i]);

			AudioStreamInfo info = AudioIfTest_monoInfo(1792 * 1024);
			info.flags4_format4 = (U8) ((formats[i] << 4) | EAudioStreamInfoFlags_FlattenSound);
			info.bytesPerSecond = 44100 * frame;
			info.streamLength = (U32) length;
			info.duration = 10 * SECOND;

			if (Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream))) {

				AudioStream *s = AudioStreamRef_ptr(stream);

				Test_assert(t, "the format is mono", EAudioStreamFormat_getChannels(s->format) == 1);
				Test_assert(t, "play", AudioStreamRef_play(stream, t->alloc, &err));
				Test_assert(t, "update", AudioDeviceRef_update(ctx.device, t->alloc, &err));

				//Each buffer plays streamLength bytes (rounded to the source frame) of mono, from more bytes of source

				U64 cvt = (U64) s->info.streamLength * frame / EAudioStreamFormat_getSize(s->format);
				cvt -= cvt % frame;

				U64 expected = U64_min(4 * cvt, AudioStreamInfo_dataLength(&s->info));
				Test_assert(t, names[i], s->streamOffset == expected);

				if (formats[i] == EAudioStreamFormat_Stereo16)
					Test_assert(t, "stereo 16 bit reads 4 x 256 KiB", s->streamOffset == 4 * 256 * 1024);
			}

			RefPtr_dec(&stream);
		}
	}

	AudioIfCtx_free(&ctx);
}

//A pass that ends exactly where the buffer fills is still counted, so it isn't read again

void Test_audioStreamExactPass(Test *t) {

	Test_setModule(t, "AudioStream_exactPass");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		//24 bit is converted on every device. A 64 KiB buffer is cut to 65535 bytes, which holds 98301 bytes of source.

		U32 cvt = 65535 * 3 / 2;
		cvt -= cvt % 3;

		for (U8 looped = 0; looped < 2; ++looped) {

			AudioStreamRef *stream = NULL;

			AudioStreamInfo info = AudioIfTest_monoInfo(cvt);
			info.flags4_format4 = (U8) ((EAudioStreamFormat_Mono24Ext << 4) | (looped ? EAudioStreamInfoFlags_IsLoop : 0));
			info.bytesPerSecond = 44100 * 3;
			info.streamLength = 64 * 1024;
			info.loops = looped ? 2 : 1;
			info.duration = (Ns) cvt * SECOND / info.bytesPerSecond;

			if (Test_assert(t, "create stream", AudioIfTest_makeStream(t, ctx.device, &types, info, 0, &stream))) {

				AudioStream *s = AudioStreamRef_ptr(stream);
				ALAudioStream *ext = AudioStream_ext(s, AL);
				Bool updated = false;

				Test_assert(t, "the data is one buffer", Buffer_length(ext->tmpCvt) == cvt);
				Test_assert(t, "play", AudioStreamRef_play(stream, t->alloc, &err));
				Test_assert(t, "update", AudioDeviceRef_update(ctx.device, t->alloc, &err));

				//Every pass is read once, so the read position is at the end after the last pass

				Test_assert(t, "passes are counted", s->loops == (U32) (looped ? 2 : 1));
				Test_assert(t, "read position is at the end", s->streamOffset == cvt);
				Test_assert(t, "the stream ends", AudioIfTest_updateUntilEnded(t, &ctx, s, 5 * SECOND, &updated) && updated);
				Test_assert(t, "no pass is added", s->loops == (U32) (looped ? 2 : 1) && s->streamOffset == cvt);
			}

			RefPtr_dec(&stream);
		}
	}

	AudioIfCtx_free(&ctx);
}

//Folding stereo float to mono stores floats of the format's width, each the average of the two channels

void Test_audioStreamFlattenFloat(Test *t) {

	Test_setModule(t, "AudioStream_flattenFloat");

	Error err = Error_none();
	AudioIfCtx ctx = (AudioIfCtx) { 0 };
	AudioIfTypes types = AudioIfTypes_create(t->alloc);

	static F32 data32[32768];
	static F64 data64[16384];

	for (U32 i = 0; i < 16384; ++i) {
		data32[i * 2] = (F32) (i % 16) / 32;
		data32[i * 2 + 1] = -(F32) (i % 8) / 64;
	}

	for (U32 i = 0; i < 8192; ++i) {
		data64[i * 2] = (F64) (i % 16) / 32;
		data64[i * 2 + 1] = -(F64) (i % 8) / 64;
	}

	if (Test_assert(t, "create ctx", AudioIfCtx_create(&ctx, t->alloc, &types.ifType, &types.devType, &err))) {

		EAudioDeviceFlags flags = AudioDeviceRef_ptr(ctx.device)->info.flags;

		for (U8 wide = 0; wide < 2; ++wide) {

			if (!(flags & (wide ? EAudioDeviceFlags_HasF64Ext : EAudioDeviceFlags_HasF32Ext))) {
				Log_debugLn(t->alloc, "Device doesn't report the float extension; skipping the float flatten check");
				continue;
			}

			AudioStreamRef *stream = NULL;
			U8 frame = wide ? 16 : 8;

			AudioStreamInfo info = AudioIfTest_monoInfo(131072);
			info.flags4_format4 = (U8) (
				((wide ? EAudioStreamFormat_Stereo64fExt : EAudioStreamFormat_Stereo32fExt) << 4) |
				EAudioStreamInfoFlags_FlattenSound
			);

			info.bytesPerSecond = 44100 * frame;
			info.streamLength = 64 * 1024;
			info.duration = 131072 * SECOND / info.bytesPerSecond;

			if (Test_assert(t, "create stream", AudioIfTest_makeStreamData(
				t, ctx.device, &types, info, 0, wide ? (const void*) data64 : (const void*) data32, 131072, &stream
			))) {

				AudioStream *s = AudioStreamRef_ptr(stream);
				ALAudioStream *ext = AudioStream_ext(s, AL);

				Test_assert(t, "play", AudioStreamRef_play(stream, t->alloc, &err));
				Test_assert(t, "update", AudioDeviceRef_update(ctx.device, t->alloc, &err));

				U32 count = wide ? 8192 : 16384;
				Bool ok = Buffer_length(ext->tmp) == 65536;

				for (U32 i = 0; i < count && ok; ++i) {

					F64 expected = wide ?
						(data64[i * 2] + data64[i * 2 + 1]) / 2 : ((F64) data32[i * 2] + (F64) data32[i * 2 + 1]) / 2;

					F64 got;

					if (wide) {
						F64 v;
						Buffer_memcpy(Buffer_createRef(&v, 8), Buffer_createRefConst(ext->tmp.ptr + i * 8, 8));
						got = v;
					}

					else {
						F32 v;
						Buffer_memcpy(Buffer_createRef(&v, 4), Buffer_createRefConst(ext->tmp.ptr + i * 4, 4));
						got = v;
					}

					ok = got - expected < 1e-6 && expected - got < 1e-6;
				}

				Test_assert(t, wide ? "64 bit samples are averaged" : "32 bit samples are averaged", ok);
			}

			RefPtr_dec(&stream);
		}
	}

	AudioIfCtx_free(&ctx);
}

//The preferred device is the main output if it qualifies, else the first device that does

void Test_audioInterfacePreferredIsFirst(Test *t) {

	Test_setModule(t, "AudioInterface_preferredIsFirst");

	Error err = Error_none();
	AudioInterfaceRef *interf = NULL;
	ListAudioDeviceInfo devices = (ListAudioDeviceInfo) { 0 };
	RefPtrType type = AudioInterface_makeType(t->alloc);

	if (
		Test_assert(t, "create", AudioInterface_create(&interf, t->alloc, &type, &err)) &&
		Test_assert(t, "devices", AudioInterface_getDeviceInfos(AudioInterfaceRef_ptr(interf), t->alloc, &devices, &err))
	) {
		for (U32 required = 0; required < 64; ++required) {

			U64 expected = U64_MAX;

			for (U64 i = 0; i < devices.length; ++i) {

				if ((devices.ptr[i].flags & required) != required)
					continue;

				if (devices.ptr[i].flags & EAudioDeviceFlags_MainOutput) {
					expected = i;
					break;
				}

				if (expected == U64_MAX)
					expected = i;
			}

			AudioDeviceInfo info = (AudioDeviceInfo) { 0 };
			EAudioDeviceFlags flags = (EAudioDeviceFlags) required;

			Bool found = AudioInterface_getPreferredDevice(AudioInterfaceRef_ptr(interf), flags, t->alloc, &info, NULL);

			Test_assert(t, "preferred device found if any qualifies", found == (expected != U64_MAX));

			if (found && expected != U64_MAX)
				Test_assert(t, "preferred is the main output or the first that qualifies", info.id == devices.ptr[expected].id);
		}
	}

	ListAudioDeviceInfo_free(&devices, t->alloc);
	RefPtr_dec(&interf);
}

OXC3_TEST_MAIN(audio_interface) {

	const Allocator alloc = BasicAllocator_instance;

	Test t = (Test) { 0 };
	t.alloc = &alloc;

	#ifdef AUDIO_TEST_DEBUG
		if(!Platform_create(0, NULL, NULL, NULL, false, NULL))
			Log_errorLn(&alloc, "Couldn't create platform for debug utils");
		else Log_debugLn(&alloc, "Created platform for debug utils");
	#endif

	//API/error-handling tests that don't need real audio hardware (always run).
	Test_audioInterfaceCreateDestroy(&t);
	Test_audioInterfaceDoubleDecSafe(&t);
	Test_audioInterfaceInvalidType(&t);

	Test_audioInterfaceGetDeviceInfos(&t);          //Sets hasAudioDevice
	Test_audioInterfaceGetPreferredDeviceNullOut(&t);

	Test_audioDeviceNullInfo(&t);
	Test_audioDeviceListenerTransformNull(&t);
	Test_audioDeviceInfoPrint(&t);

	//Tests that require an actual audio device.
	//Headless environments (CI without audio hardware) enumerate zero devices,
	// so skip these there rather than failing every device-dependent assertion.
	if (hasAudioDevice) {
		Test_audioInterfaceGetPreferredDevice(&t);
		Test_audioDeviceCreateDestroy(&t);
		Test_audioDeviceCreateDebug(&t);
		Test_audioDeviceInvalidType(&t);
		Test_audioDeviceListenerTransform(&t);
		Test_audioDeviceListenerPositionSteps(&t);
		Test_audioSourceStereoSpatialRejected(&t);
		Test_audioDeviceFromFileAtOffset(&t);
		Test_audioSourceReleasedBeforeUpdate(&t);
		Test_audioSmallChanges(&t);
		Test_audioRefusedValues(&t);
		Test_audioSourceOtherDevice(&t);
		Test_audioStreamLoopsZero(&t);
		Test_audioStreamSeekClamp(&t);
		Test_audioStreamBuffers(&t);
		Test_audioStreamSeekAfterLastPass(&t);
		Test_audioStreamReplayAfterEnd(&t);
		Test_audioStreamPartialFrame(&t);
		Test_audioStreamFlatten(&t);
		Test_audioStreamExactPass(&t);
		Test_audioStreamFlattenFloat(&t);
		Test_audioInterfacePreferredIsFirst(&t);
	}

	BasicAllocator_checkLeakedMem(&t);

	return Test_end(&t);
}
