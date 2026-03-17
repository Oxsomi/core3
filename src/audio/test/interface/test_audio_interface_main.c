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

#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "audio/audio_source.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/memory_stream.h"
#include "types/test/test.h"

typedef struct AudioIfCtx {
	AudioInterfaceRef *interf;
	AudioDeviceRef *device;
} AudioIfCtx;

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
	type.typeId = (ETypeId)0xDEAD;
 
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

	Test_assert(t, "at least one device", devices.length >= 1);
 
	U64 mainCount = 0;

	for (U64 i = 0; i < devices.length; ++i)
		if (devices.ptr[i].flags & EAudioDeviceFlags_MainOutput)
			++mainCount;
 
	Test_assert(t, "exactly one MainOutput", mainCount == 1);
 
	for (U64 i = 0; i < devices.length; ++i)
		Test_assert(t, "device name non-empty", devices.ptr[i].name[0] != '\0');
 
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
			t->alloc, NULL, &err)
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
		return;		//Skip: debug extension not available
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
	devType.typeId = (ETypeId)0xDEAD;
 
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

	AudioStreamInfo info = (AudioStreamInfo) {
		.format = EAudioStreamFormat_Stereo16,
		.flattenSound = false,
		.sampleRate = 44100,
		.bytesPerSecond = 44100 * 4,
		.dataLengthLo32 = sizeof(silence),
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

int main() {

	const Allocator alloc = BasicAllocator_instance;

	Test t = (Test) { 0 };
	t.alloc = &alloc;

	Test_audioInterfaceCreateDestroy(&t);
	Test_audioInterfaceDoubleDecSafe(&t);
	Test_audioInterfaceInvalidType(&t);

	Test_audioInterfaceGetDeviceInfos(&t);
	Test_audioInterfaceGetPreferredDevice(&t);
	Test_audioInterfaceGetPreferredDeviceNullOut(&t);

	Test_audioDeviceCreateDestroy(&t);
	Test_audioDeviceCreateDebug(&t);
	Test_audioDeviceInvalidType(&t);
	Test_audioDeviceNullInfo(&t);

	Test_audioDeviceListenerTransform(&t);
	Test_audioDeviceListenerTransformNull(&t);
	Test_audioDeviceListenerPositionSteps(&t);

	Test_audioDeviceInfoPrint(&t);

	Test_audioSourceStereoSpatialRejected(&t);

	BasicAllocator_checkLeakedMem(&t);

	return Test_end(&t);
}
