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

//audio/test/functional/test_audio_functional_main.c

#include "audio/audio_interface.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "audio/audio_source.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/ref_ptr.h"
#include "types/container/stream.h"
#include "types/container/buffer.h"
#include "types/container/log.h"
#include "types/base/thread.h"
#include "types/base/error.h"
#include "types/base/constants.h"
#include "types/base/mathf.h"
#include "types/base/time.h"

#include <stdio.h>

//Minimal fopen-backed OxStream (can't use OxC3_platforms)
//Follows the same pattern as MemoryStream: OxStream is the base,
//FileStream embeds it as 'parent' and appends its own data after.

typedef struct FileStream {
	OxStream parent;
	FILE *f;
} FileStream;

typedef RefPtr FileStreamRef;

static Bool FileStream_read(
	OxStream *stream,
	U64 offset,
	U64 length,
	Buffer buf,
	const Allocator *alloc,
	Error *e_rr
) {
	(void)alloc;
	Bool s_uccess = true;
	FileStream *fs = (FileStream *)stream;

	if (offset + length > stream->size)
		retError(clean, Error_outOfBounds(0, offset + length, stream->size, "FileStream_read() out of bounds"));

	if (fseek(fs->f, (long)offset, SEEK_SET) != 0)
		retError(clean, Error_invalidState(0, "FileStream_read() fseek failed"));

	if (fread(buf.ptrNonConst, 1, length, fs->f) != length)
		retError(clean, Error_invalidState(0, "FileStream_read() fread failed"));

clean:
	return s_uccess;
}

static void FileStream_close(OxStream *stream, const Allocator *alloc) {
	(void)alloc;
	FileStream *fs = (FileStream *)stream;
	if (fs->f) {
		fclose(fs->f);
		fs->f = NULL;
	}
}

static Bool FileStream_open(
	const C8 *path,
	FileStreamRef **outRef,
	const RefPtrType *type,
	Error *e_rr
) {
	Bool s_uccess = true;
	FILE *f = NULL;

	f = fopen(path, "rb");
	if (!f)
		retError(clean, Error_invalidState(0, "FileStream_open() fopen failed"));

	fseek(f, 0, SEEK_END);
	U64 size = (U64)ftell(f);
	fseek(f, 0, SEEK_SET);

	gotoIfError3(clean, Stream_create(
		FileStream_read,
		NULL,        //write
		NULL,        //reserve
		FileStream_close,
		size,
		EStreamType_File,
		type,
		outRef,
		e_rr
	));

	FileStream *fs = RefPtr_data(*outRef, FileStream);
	fs->f = f;
	f = NULL;    //Ownership transferred

clean:
	if (f)
		fclose(f);

	return s_uccess;
}

//Shared context

typedef struct Types {
	RefPtrType fsType;
	RefPtrType ifType;
	RefPtrType devType;
	RefPtrType streamType;
	RefPtrType sourceType;
} Types;

typedef struct AudioFuncCtx {
	AudioInterfaceRef *interf;
	AudioDeviceRef *device;
	const Allocator *alloc;
	Types *types;
} AudioFuncCtx;

static Bool AudioFuncCtx_create(
	AudioFuncCtx *ctx,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	AudioDeviceInfo info = (AudioDeviceInfo){ 0 };

	ctx->alloc = alloc;

	gotoIfError3(clean, AudioInterface_create(&ctx->interf, alloc, &ctx->types->ifType, e_rr));
	gotoIfError3(clean, AudioInterface_getPreferredDevice(
		AudioInterfaceRef_ptr(ctx->interf), EAudioDeviceFlags_None, alloc, &info, e_rr
	));

	gotoIfError3(clean, AudioDeviceRef_create(
		ctx->interf, &info, false, alloc, &ctx->types->devType, &ctx->device, e_rr
	));

clean:
	return s_uccess;
}

void Types_create(Types *types, const Allocator *alloc) {
	types->fsType = Stream_inheritType(alloc, sizeof(FileStream) - sizeof(OxStream));
	types->ifType = AudioInterface_makeType(alloc);
	types->devType = AudioDevice_makeType(alloc);
	types->streamType = AudioStream_makeType(alloc);
	types->sourceType = AudioSource_makeType(alloc);
}

static void AudioFuncCtx_free(AudioFuncCtx *ctx) {
	RefPtr_dec(&ctx->device);
	RefPtr_dec(&ctx->interf);
}

//Stream open helper.
//Always creates a default source (gain=1, no spatial) since AudioSource is required for playback.
//Pass NULL for source only if you intend to create your own source with custom settings immediately after.

static Bool openWav(
	AudioFuncCtx *ctx,
	const C8 *path,
	U16 loops,
	Bool flattenSound,
	FileStreamRef **fileStream,
	AudioStreamRef **audioStream,
	AudioSourceRef **source,        //Required -- pass your own AudioSourceRef*, gets a default gain=1 source
	Error *e_rr
) {
	Bool s_uccess = true;

	gotoIfError3(clean, FileStream_open(path, fileStream, &ctx->types->fsType, e_rr));

	gotoIfError3(clean, AudioDeviceRef_createFromFile(
		ctx->device,
		*fileStream,
		0,                //inputOffset
		loops,
		0,                //startOffset (Ns)
		1,                //pitch
		flattenSound,
		ctx->alloc,
		&ctx->types->streamType,
		audioStream,
		e_rr
	));

	if (source) {
		AudioModifier modifier = (AudioModifier){ .gain = 1 };
		gotoIfError3(clean, AudioDeviceRef_createSource(
			ctx->device, *audioStream, modifier, ctx->alloc, &ctx->types->sourceType, source, e_rr
		));
	}

clean:
	return s_uccess;
}

static const C8 *shortTracks[] = {
	"short_8b_mono.wav",
	"short_8b_stereo.wav",
	"short_16b_mono.wav",
	"short_16b_stereo.wav",
	"short_24b_mono.wav",
	"short_24b_stereo.wav",
	"short_32b_mono.wav",
	"short_32b_stereo.wav",
	"short_32f_mono.wav",
	"short_32f_stereo.wav",
	"short_64f_mono.wav",
	"short_64f_stereo.wav"
};

static const C8 *longTracks[] = {
	"long_8b_mono.wav",
	"long_8b_stereo.wav",
	"long_16b_mono.wav",
	"long_16b_stereo.wav",
	"long_24b_mono.wav",
	"long_24b_stereo.wav",
	"long_32b_mono.wav",
	"long_32b_stereo.wav",
	"long_32f_mono.wav",
	"long_32f_stereo.wav",
	"long_64f_mono.wav",
	"long_64f_stereo.wav"
};

void Test_audioPlayOnce(AudioFuncCtx *ctx, const C8 *path) {

	Log_debugLn(ctx->alloc, "TEST: %s, play once", path);

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, path, 1, false, &fs, &as, &source, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	if (!AudioDeviceRef_wait(ctx->device, false, ctx->alloc, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source);
	RefPtr_dec(&as);
	RefPtr_dec(&fs);
}

void Test_audioPlayLoop(AudioFuncCtx *ctx, const C8 *path, U16 targetLoops) {

	Log_debugLn(ctx->alloc, "TEST: %s, loop %u times", path, (U32)targetLoops);

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, path, targetLoops, false, &fs, &as, &source, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	if (!AudioDeviceRef_wait(ctx->device, false, ctx->alloc, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS (looped %u times)", targetLoops);
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source);
	RefPtr_dec(&as);
	RefPtr_dec(&fs);
}

//Seek test

void Test_audioSeekMidTrack(AudioFuncCtx *ctx) {

	Log_debugLn(ctx->alloc, "TEST: seek to 50%% into track, should start from the middle");

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, "long_64f_stereo.wav", 1, false, &fs, &as, &source, &err))
		goto fail;

	Ns midPoint = AudioStreamRef_ptr(as)->info.duration / 2;
	if (!AudioStreamRef_seekTime(as, midPoint, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	if (!AudioDeviceRef_wait(ctx->device, false, ctx->alloc, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

//Gain sweep
//Play a looping track while stepping gain down then back up.
//Audibly should fade out then fade in.
//Tests both listener gain and source gain.

void Test_audioSourceGainSweep(AudioFuncCtx *ctx) {

	Log_debugLn(ctx->alloc, "TEST: gain sweep 1.0->0.1->1.0, audibly fades out then in");

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, "long_64f_stereo.wav", 0, false, &fs, &as, &source, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	F32 gains[] = { 1.0f, 0.75f, 0.5f, 0.25f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };

	//Gain through listener
	Log_debugLn(ctx->alloc, "  listener gain sweep...");
	for (U64 i = 0; i < 9; ++i) {
		if (!AudioDeviceRef_updateListenerGain(ctx->device, gains[i], &err))
			goto fail;
		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;
		Thread_sleep(500 * MS);
	}

	//Reset listener gain
	if (!AudioDeviceRef_updateListenerGain(ctx->device, 1, &err))
		goto fail;

	//Gain through source
	//TODO: Fix source gain
	Log_debugLn(ctx->alloc, "  source gain sweep...");
	for (U64 i = 0; i < 9; ++i) {
		if (!AudioSourceRef_updateGain(source, gains[i], ctx->alloc, &err))
			goto fail;
		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;
		Thread_sleep(500 * MS);
	}

	if (!AudioStreamRef_stop(as, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

//Gain of 0
//Play a stream at gain 0, should be complete silence.

void Test_audioGainZero(AudioFuncCtx *ctx) {

	Log_debugLn(ctx->alloc, "TEST: gain 0, should be complete silence");

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	//Open without default source so we can set gain=0 ourselves
	if (!openWav(ctx, "short_16b_mono.wav", 1, false, &fs, &as, NULL, &err))
		goto fail;

	AudioModifier modifier = (AudioModifier) { .gain = 0 };
	if (!AudioDeviceRef_createSource(ctx->device, as, modifier, ctx->alloc, &ctx->types->sourceType, &source, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	if (!AudioDeviceRef_wait(ctx->device, false, ctx->alloc, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS (verify silence above)");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

//Spatial sweep
//Move source from left to right while playing, listener at origin.
//Audibly should pan from left to right.

void Test_audioSourceSpatialSweep(AudioFuncCtx *ctx, Bool stereo) {

	Log_debugLn(ctx->alloc,
		"TEST: spatial sweep left to right, audibly pans left->right (%s)", stereo ? "stereo+flatten" : "mono"
	);

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	//Open without default source so we can create a 3D source ourselves
	if (!openWav(ctx, stereo ? "long_16b_stereo.wav" : "long_16b_mono.wav", 0, stereo, &fs, &as, NULL, &err))
		goto fail;

	AudioModifier modifier = (AudioModifier){ .gain = 1 };
	AudioPoint3D startPoint = (AudioPoint3D){
		.pos = F32x4_create3(-20, 0, 0),
		.velocity = F32x4_zero()
	};

	if (!AudioDeviceRef_createSource3D(ctx->device, as, modifier, startPoint, ctx->alloc, &ctx->types->sourceType, &source, &err))
		goto fail;

	if (!AudioDeviceRef_updateListenerPosition(ctx->device, F32x4_zero(), &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	for (I32 i = -20; i <= 20; ++i) {
		if (!AudioSourceRef_updatePosition3D(source, F32x4_create3((F32)i, 0, 0), ctx->alloc, &err))
			goto fail;
		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;
		Thread_sleep(200 * MS);
	}

	if (!AudioStreamRef_stop(as, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

//Listener position sweep
//Source stays fixed, listener moves toward and away from it.
//Audibly should get louder as listener approaches, quieter as it moves away.

void Test_audioListenerPositionSweep(AudioFuncCtx *ctx) {

	Log_debugLn(ctx->alloc, "TEST: listener position sweep, source at (5,0,0), listener moves 0->5->0");

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, "long_16b_mono.wav", 0, false, &fs, &as, NULL, &err))
		goto fail;

	AudioModifier modifier = (AudioModifier){ .gain = 1 };
	AudioPoint3D point = (AudioPoint3D){
		.pos = F32x4_create3(5, 0, 0),
		.velocity = F32x4_zero()
	};

	if (!AudioDeviceRef_createSource3D(ctx->device, as, modifier, point, ctx->alloc, &ctx->types->sourceType, &source, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	F32 positions[] = { 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
	for (U64 i = 0; i < 11; ++i) {
		if (!AudioDeviceRef_updateListenerPosition(ctx->device, F32x4_create3(positions[i], 0, 0), &err))
			goto fail;
		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;
		Thread_sleep(500 * MS);
	}

	if (!AudioStreamRef_stop(as, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

//Listener orientation sweep
//Source fixed at (5,0,0), listener rotates forward vector.
//Audibly the source should sweep from left to right to left as the listener turns.

void Test_audioListenerOrientationSweep(AudioFuncCtx *ctx) {

	Log_debugLn(
		ctx->alloc, "TEST: listener orientation sweep, source fixed, listener rotates, should sweep left->right->left"
	);

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, "long_16b_mono.wav", 0, false, &fs, &as, NULL, &err))
		goto fail;

	AudioModifier modifier = (AudioModifier) { .gain = 1 };
	AudioPoint3D point = (AudioPoint3D) {
		.pos = F32x4_create3(5, 0, 0)
	};

	if (!AudioDeviceRef_createSource3D(ctx->device, as, modifier, point, ctx->alloc, &ctx->types->sourceType, &source, &err))
		goto fail;

	if (!AudioDeviceRef_updateListenerPosition(ctx->device, F32x4_zero(), &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	//Rotate forward from (0,0,-1) through (1,0,0) to (0,0,1)
	for (I32 i = -20; i <= 20; ++i) {
		F32 t = (F32)i / 10.0f;
		F32 angle = t * 3.14159265f / 2.0f;
		F32x4 fwd = F32x4_create3(F32_sin(angle), 0, -F32_cos(angle));

		if (!AudioDeviceRef_updateListenerForward(ctx->device, fwd, &err))
			goto fail;

		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;

		Log_debugLn(ctx->alloc, "Facing %f", angle * F32_RAD_TO_DEG);
		Thread_sleep(500 * MS);
	}

	if (!AudioStreamRef_stop(as, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

//Doppler effect
//Source moves toward listener at high velocity, then away.
//Audibly should hear pitch shift up as it approaches, down as it recedes.

void Test_audioDoppler(AudioFuncCtx *ctx) {

	Log_debugLn(ctx->alloc, "TEST: Doppler, source velocity toward/away from listener, pitch shifts up then down");

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, "short_16b_mono.wav", 0, false, &fs, &as, NULL, &err))
		goto fail;

	AudioModifier modifier = (AudioModifier) { .gain = 1 };
	AudioPoint3D point = (AudioPoint3D) {
		.pos = F32x4_create3(0, 0, 2),
		.velocity = F32x4_create3(0, 0, -50)    //Approaching
	};

	if (!AudioDeviceRef_createSource3D(ctx->device, as, modifier, point, ctx->alloc, &ctx->types->sourceType, &source, &err))
		goto fail;

	if (!AudioDeviceRef_updateListenerPosition(ctx->device, F32x4_zero(), &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  approaching (pitch up)...");

	Ns then = Time_now();

	while ((Time_now() - then) < 4 * SECOND) {

		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;

		Thread_sleep(10 * MS);
	}

	//Flip to receding
	if (!AudioSourceRef_updateVelocity3D(source, F32x4_create3(0, 0, 50), ctx->alloc, &err))
		goto fail;

	if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  receding (pitch down)...");

	then = Time_now();

	while ((Time_now() - then) < 4 * SECOND) {

		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;

		Thread_sleep(10 * MS);
	}

	if (!AudioStreamRef_stop(as, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

//Multiple simultaneous sources
//Two streams play at once with different gains.
//Audibly should hear both, one louder than the other.

void Test_audioMultipleSources(AudioFuncCtx *ctx) {

	Log_debugLn(ctx->alloc, "TEST: two simultaneous sources, one loud (1.0), one quiet (0.2), both audible");

	Error err = Error_none();
	FileStreamRef *fs0 = NULL, *fs1 = NULL;
	AudioStreamRef *as0 = NULL, *as1 = NULL;
	AudioSourceRef *src0 = NULL, *src1 = NULL;

	//Open without default source so we can set different gains
	if (!openWav(ctx, "long_16b_mono.wav", 0, false, &fs0, &as0, NULL, &err))
		goto fail;

	if (!openWav(ctx, "short_16b_mono.wav", 0, false, &fs1, &as1, NULL, &err))
		goto fail;

	AudioModifier mod0 = (AudioModifier) { .gain = 1.0f };
	AudioModifier mod1 = (AudioModifier) { .gain = 0.2f };

	if (!AudioDeviceRef_createSource(ctx->device, as0, mod0, ctx->alloc, &ctx->types->sourceType, &src0, &err))
		goto fail;

	if (!AudioDeviceRef_createSource(ctx->device, as1, mod1, ctx->alloc, &ctx->types->sourceType, &src1, &err))
		goto fail;

	if (!AudioStreamRef_play(as0, ctx->alloc, &err))
		goto fail;

	if (!AudioStreamRef_play(as1, ctx->alloc, &err))
		goto fail;

	Ns then = Time_now();

	while ((Time_now() - then) < 2 * SECOND) {

		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;

		Thread_sleep(10 * MS);
	}

	if (!AudioStreamRef_stop(as0, &err))
		goto fail;

	if (!AudioStreamRef_stop(as1, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&src0); RefPtr_dec(&as0); RefPtr_dec(&fs0);
	RefPtr_dec(&src1); RefPtr_dec(&as1); RefPtr_dec(&fs1);
}

//Stop and restart
//Play a stream, stop quarterway, seek to start, play again.
//Audibly should hear first quarter, silence, then full track from beginning.

void Test_audioStopAndRestart(AudioFuncCtx *ctx) {

	Log_debugLn(ctx->alloc, "TEST: stop and restart, play quarter, stop, seek to 0, play again from start");

	Error err = Error_none();
	FileStreamRef *fs = NULL;
	AudioStreamRef *as = NULL;
	AudioSourceRef *source = NULL;

	if (!openWav(ctx, "short_16b_mono.wav", 1, false, &fs, &as, &source, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	//Update until roughly 1/4 the data has been consumed
	AudioStream *stream = AudioStreamRef_ptr(as);
	Ns qrtData = stream->info.duration / 4;
	Ns then = Time_now();

	while ((Time_now() - then) < qrtData) {

		if (!AudioDeviceRef_update(ctx->device, ctx->alloc, &err))
			goto fail;

		Thread_sleep(10 * MS);
	}

	if (!AudioStreamRef_stop(as, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  stopped midway, seeking to 0...");

	if (!AudioStreamRef_seekTime(as, 0, &err))
		goto fail;

	if (!AudioStreamRef_play(as, ctx->alloc, &err))
		goto fail;

	if (!AudioDeviceRef_wait(ctx->device, false, ctx->alloc, &err))
		goto fail;

	Log_debugLn(ctx->alloc, "  PASS");
	goto clean;
fail:
	Error_print(ctx->alloc, &err, ELogLevel_Error, ELogOptions_Default);
clean:
	RefPtr_dec(&source); RefPtr_dec(&as); RefPtr_dec(&fs);
}

int main() {

	const Allocator *alloc = &BasicAllocator_instance;
	AudioFuncCtx ctx = (AudioFuncCtx){ 0 };
	Error err = Error_none();

	Log_debugLn(alloc, "=== OxC3 Audio Functional Tests ===");
	Log_debugLn(alloc, "Verify audio quality manually by listening.");
	Log_debugLn(alloc, "");

	Types types;
	Types_create(&types, alloc);
	ctx.types = &types;

	if (!AudioFuncCtx_create(&ctx, alloc, &err)) {
		Error_print(alloc, &err, ELogLevel_Error, ELogOptions_Default);
		return 1;
	}

	//Short tracks, all bit depths, play once
	for (U64 i = 0; i < sizeof(shortTracks) / sizeof(*shortTracks); ++i)
		Test_audioPlayOnce(&ctx, shortTracks[i]);

	//Long tracks
	for (U64 i = 0; i < sizeof(longTracks) / sizeof(*longTracks); ++i)
		Test_audioPlayOnce(&ctx, longTracks[i]);

	//Loop each short track thrice
	for (U64 i = 0; i < sizeof(shortTracks) / sizeof(*shortTracks); ++i)
		Test_audioPlayLoop(&ctx, shortTracks[i], 3);

	//Loop each long track once
	for (U64 i = 0; i < sizeof(longTracks) / sizeof(*longTracks); ++i)
		Test_audioPlayLoop(&ctx, longTracks[i], 2);

	//Seek (plays second half of long track)
	Test_audioSeekMidTrack(&ctx);

	//Gain sweep (listener + source)
	Test_audioSourceGainSweep(&ctx);

	//Gain of 0 (silence)
	Test_audioGainZero(&ctx);

	//Spatial sweeps
	Test_audioSourceSpatialSweep(&ctx, false);    //Mono
	Test_audioSourceSpatialSweep(&ctx, true);    //Stereo with flattenSound

	//Listener sweeps
	Test_audioListenerPositionSweep(&ctx);
	Test_audioListenerOrientationSweep(&ctx);

	//Doppler
	Test_audioDoppler(&ctx);

	//Multiple simultaneous sources
	Test_audioMultipleSources(&ctx);

	//Stop and restart
	Test_audioStopAndRestart(&ctx);

	Log_debugLn(alloc, "");
	Log_debugLn(alloc, "=== Done ===");

	AudioFuncCtx_free(&ctx);
	return 0;
}
