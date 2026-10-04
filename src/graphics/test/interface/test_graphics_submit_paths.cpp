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

//graphics/test/interface/test_graphics_submit_paths.cpp
//
//The submit paths an ordinary frame never takes:
//
//  GraphicsDevice/submitSwapchainOnly - a frame carrying swapchains and no command lists
//  GraphicsDevice/submitFlush         - uploads crossing flushThreshold, which splits the submit mid recording
//  GraphicsDevice/submitFailure       - a frame whose recording fails, and the frames in flight after it
//
//The flush and failure modules reach into the device's own bookkeeping (flushThreshold, completedSubmitId, the
//throttled log counters), which the C++ layer deliberately does not expose; those go through the C struct.

#include <stdlib.h>

#include "test_graphics_shared.hpp"

//Log::errorLn is the C++ front for Log_errorLnx; the x macros name ELogOptions_NewLine unqualified and so
//cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/atomic.h"
	#include "types/base/thread.h"
	#include "types/base/time.h"
	#include "types/container/buffer.h"
	#include "types/container/memory_stream.h"
	#include "types/container/ref_ptr.h"
	#include "platforms/window.h"
	#include "platforms/window_manager.h"
	#include "graphics/generic/device_texture.h"
	#include "graphics/generic/swapchain.h"
	#include "graphics/generic/texture.h"
}}

namespace {

	using namespace oxc;

	c::Bool makeEmptyList(c::Test *t, gfx::Device &dev, gfx::CommandList &emptyList) {
		return
			c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, emptyList, true, &t->err)) &&
			c::Test_assert(t, "beginList", emptyList.begin(true, &t->err)) &&
			c::Test_assert(t, "endList", emptyList.end(&t->err));
	}

	void pullCompleted(void *resource, void *context) {
		(void) resource;
		++*(c::U32*)context;
	}

	void streamPulled(void *resource, c::Bool ok, void *context) {
		(void) resource;
		*(c::U32*)context += ok ? 1 : 0x100;
	}

	//The render target pull hands over a tight row buffer; only the first texel and the mismatches are kept.

	struct PixelPull {
		c::U32 count;
		c::U32 first;
		c::U32 mismatches;
	};

	void pixelsPulled(void *resource, void *dataPtr, void *context) {

		(void) resource;

		const c::Buffer *data = (const c::Buffer*) dataPtr;
		PixelPull *result = (PixelPull*) context;
		++result->count;

		if(!data || c::Buffer_length(*data) < 4)
			return;

		const c::U32 *texels = (const c::U32*) data->ptr;
		result->first = texels[0];

		for(c::U64 i = 0; i * 4 + 4 <= c::Buffer_length(*data); ++i)
			result->mismatches += texels[i] != texels[0];
	}

	//The bit position GraphicsDevice_logThrottled files a message under, which indexes logLast and logFolded.

	c::U8 messageSlot(c::EGraphicsDeviceMessage message) {

		c::U8 slot = 0;

		for(c::U64 bit = (c::U64) message; bit > 1; bit >>= 1)
			++slot;

		return slot;
	}

	//A hang is what a regression of the failure path looks like, and a hung suite reports nothing at all.
	//So the legs that could hang run under this: past the limit it names the leg and ends the process, which fails
	// the run where a wait would only have stalled it.

	struct Watchdog {
		c::AtomicI64 done;
		c::Ns limit;
		const c::Allocator *alloc;
		const c::C8 *what;
	};

	void watchdogRun(void *ptr) {

		Watchdog *w = (Watchdog*) ptr;
		const c::Ns start = c::Time_now();

		while(!c::AtomicI64_load(&w->done)) {

			if(c::Time_now() - start > w->limit) {
				Log::errorLn(*w->alloc, "%s didn't finish within its time limit, the device is likely wedged", w->what);
				_Exit(1);
			}

			c::Thread_sleep(50 * c::MS);
		}
	}

	//An upload source that refuses, which is what makes a recording fail after the frame slot was waited on and
	// reset: the uploads are read while the frame records.

	c::U32 refusedReads = 0;

	c::Bool refusingRead(
		c::OxStream *stream, c::U64 offset, c::U64 length, c::Buffer buf, const c::Allocator *alloc, c::Error *e_rr
	) {

		(void) stream;
		(void) offset;
		(void) length;
		(void) buf;
		(void) alloc;

		++refusedReads;

		if(e_rr)
			*e_rr = c::Error_invalidState(0, "refusingRead() refuses on purpose, to fail a recording");

		return false;
	}
}

//A frame of only swapchains is accepted by GraphicsDeviceRef_submitCommands, so it has to work on every backend:
// nothing is recorded, nothing executes, and the ring still advances.
//Over a VIRTUAL window, so it runs headless; what it covers is the execute, which is the same for both kinds.

extern "C" void Test_graphicsSubmitSwapchainOnly(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/submitSwapchainOnly");

	Device dev = Device::share(deviceRef);

	c::WindowManager manager = {};
	c::WindowRef *windowRef = NULL;

	const c::WindowManagerCallbacks managerCallbacks = {};

	if(!c::Test_assert(t, "createManager", c::WindowManager_create(managerCallbacks, 0, &manager, e_rr)))
		return;

	const c::WindowCallbacks windowCallbacks = {};
	const c::CharString title = c::CharString_createRefCStrConst("Swapchain only submit test");

	const c::I32x2 pos = c::I32x2_create2(0, 0);
	const c::I32x2 size = c::EResolution_get(c::EResolution_SD);
	const c::I32x2 maxSize = c::I32x2_create2(4096, 4096);

	if(!c::Test_assert(t, "createVirtualWindow", c::WindowManager_createWindow(
		&manager, c::EWindowType_Virtual, pos, size, size, maxSize,
		c::EWindowHint_None, title, windowCallbacks, c::EWindowFormat_AutoRGBA8, 0, &windowRef, e_rr
	)))
		goto clean;

	{
		Swapchain swapchain;

		if(!c::Test_assert(t, "createSwapchain", dev.createSwapchain(
			RefPtr_data(windowRef, c::Window), false, swapchain, {}, e_rr
		)))
			goto clean;

		const c::U8 fif = dev.framesInFlight();
		const c::U8 images = c::TextureRef_getUnifiedTexture(swapchain.handle(), NULL).images;
		const c::U8 startImage = c::TextureRef_getUnifiedTexture(swapchain.handle(), NULL).currentImageId;

		//More than framesInFlight, so every frame slot is reused at least once by a frame that recorded nothing.

		for(c::U32 i = 0; i < (c::U32) fif + 1; ++i)
			if(!c::Test_assert(t, "submitSwapchainOnly", dev.submit({}, { &swapchain }, 0, 0, e_rr)))
				break;

		c::Test_assert(t, "waitSwapchainOnly", dev.wait(e_rr));

		c::Test_assert(
			t, "ringAdvanced",
			c::TextureRef_getUnifiedTexture(swapchain.handle(), NULL).currentImageId == (startImage + fif + 1) % images
		);

		//And a frame that does record still lands afterwards, read back against what it wrote.

		CommandList commandList;

		if(c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr))) {

			const c::ImageRange all = {};

			c::Test_assert(t, "begin", commandList.begin(true, e_rr));

			{
				CommandScope scope = commandList.scope({}, 1, {}, e_rr);
				c::Test_assert(t, "scope", (c::Bool) scope);
				c::Test_assert(t, "clear", scope.clearImagef(c::F32x4_create4(0, 1, 0, 1), all, swapchain.handle(), e_rr));
				c::Test_assert(t, "scopeEnd", scope.end(e_rr));
			}

			c::Test_assert(t, "end", commandList.end(e_rr));

			PixelPull pulled = {};

			c::Test_assert(t, "queuePull", c::TextureRef_pullRegion(
				swapchain.handle(), 0, 0, 0, 0, 0, 0, 0, pixelsPulled, &pulled, e_rr
			));

			c::Test_assert(t, "submitAfter", dev.submit({ &commandList }, { &swapchain }, 0, 0, e_rr));
			c::Test_assert(t, "waitAfter", dev.wait(e_rr));

			c::Test_assert(t, "pullCompleted", pulled.count == 1);
			c::Test_assert(t, "pullIsClearColor", pulled.first == 0xFF00FF00u && !pulled.mismatches);
		}

		//Before the window, since the swapchain holds a WEAK reference to it.

		swapchain.release();
	}

clean:

	if(windowRef)
		c::RefPtr_dec(&windowRef);

	c::WindowManager_free(&manager);
}

//Uploads crossing flushThreshold split the submit mid recording: what was recorded so far executes and is waited
// on, and recording resumes in the same submit. Lowered here, since the real threshold is a fraction of the heap.
//What has to hold afterwards: every upload landed, on both sides of every split, and the split marked exactly the
// submits before this one complete (completedSubmitId stops one short of submitId) until a real wait.
//The pulls ride the part that executes last, so they also catch a wait that returns before that part has run, which
// is what a submit signalling a fence value its split already completed does.

extern "C" void Test_graphicsSubmitFlush(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Test_setModule(t, "GraphicsDevice/submitFlush");

	Device dev = Device::share(deviceRef);
	c::GraphicsDevice *device = c::deviceOf(dev.handle());
	const c::Allocator *alloc = dev.alloc();

	CommandList emptyList;

	if(!makeEmptyList(t, dev, emptyList) || !c::Test_assert(t, "idle", dev.wait(&t->err)))
		return;

	//A submit left in flight, so the split's wait is what proves it complete

	if(!c::Test_assert(t, "submitBefore", dev.submit({ &emptyList }, {}, 0, 0, &t->err)))
		return;

	const c::U64 submitBefore = device->submitId;

	c::Test_assert(t, "beforeNotComplete", device->completedSubmitId < submitBefore);

	//Eight 16 KiB textures against a 40 KiB threshold: a split after every third, so the last two upload after the
	// second split, in the part of the submit that executes last.

	constexpr c::U32 count = 8;
	constexpr c::U16 side = 64;

	DeviceTexture textures[count];
	c::U32 pulled[count] = {};
	c::Bool made = true;

	for (c::U32 i = 0; made && i < count; ++i) {

		c::Buffer dat = c::Buffer_createNull();

		made = c::Test_assert(t, "texelAlloc", c::Buffer_createUninitializedBytes(
			(c::U64) side * side * 4, alloc, &dat, &t->err
		));

		if(!made)
			break;

		for(c::U32 j = 0; j < (c::U32) side * side; ++j)
			((c::U32*)dat.ptrNonConst)[j] = (i << 24) | j;

		made = c::Test_assert(t, "textureCreate", dev.createTexture(
			c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_CPUBacked,
			side, side, 1, "Flush split texture", &dat, textures[i], nullptr, &t->err
		));

		if(!made) {
			c::Buffer_free(&dat, alloc);
			break;
		}

		//Recorded after the frame's commands, so after every split, and completed only by a real wait

		made = c::Test_assert(t, "queuePull", textures[i].pullRegion(0, 0, 0, 0, 0, 0, pullCompleted, &pulled[i], &t->err));
	}

	if(!made)
		return;

	const c::U8 slot = messageSlot(c::EGraphicsDeviceMessage_SubmitFlushed);
	const c::I64 foldedBefore = c::AtomicI64_load(&device->logFolded[slot]);
	const c::I64 lastBefore = c::AtomicI64_load(&device->logLast[slot]);

	const c::U64 threshold = device->flushThreshold;
	device->flushThreshold = 40 * c::KIBI;

	const c::Bool submitted = c::Test_assert(t, "submitSplit", dev.submit({ &emptyList }, {}, 0, 0, &t->err));

	device->flushThreshold = threshold;

	if(!submitted)
		return;

	//Every flush counts itself through logThrottled: it folds into the count, or it claims the window and stamps it

	c::Test_assert(
		t, "splitHappened",
		c::AtomicI64_load(&device->logFolded[slot]) != foldedBefore || c::AtomicI64_load(&device->logLast[slot]) != lastBefore
	);

	c::Test_assert(t, "splitCompletedBefore", device->completedSubmitId == submitBefore);
	c::Test_assert(t, "splitNotSelf", device->submitId == submitBefore + 1);

	//The pulls land at the wait below and overwrite cpuData, so clearing the host copy now makes the comparison
	// one against what the device holds rather than against the source the upload was read from.

	for(c::U32 i = 0; i < count; ++i)
		c::Test_assert(t, "clearHostCopy", c::Buffer_unsetAllBits(textures[i].data()->cpuData, &t->err));

	if(!c::Test_assert(t, "waitSplit", dev.wait(&t->err)))
		return;

	c::Test_assert(t, "waitCompletesAll", device->completedSubmitId == device->submitId);

	c::U32 intact = 0;

	for (c::U32 i = 0; i < count; ++i) {

		const c::Buffer data = textures[i].data()->cpuData;

		if(pulled[i] != 1 || c::Buffer_length(data) < (c::U64) side * side * 4)
			continue;

		c::Bool match = true;

		for(c::U32 j = 0; match && j < (c::U32) side * side; ++j)
			match = ((const c::U32*)data.ptr)[j] == ((i << 24) | j);

		intact += match;
	}

	c::Test_assert(t, "uploadsIntact", intact == count);
}

//A recording that fails after its frame slot was waited on and reset (here: an upload whose source refuses to be
// read) must leave that slot as reusable as a successful frame does.
//The failure mode is a hang rather than an error: the slot's fence left reset while still marked pending, which the
// next submit at that slot and every device wait would wait on forever. So this runs under a watchdog, the frames
// after the failure reuse every slot, and a device wait ends it.

extern "C" void Test_graphicsSubmitFailure(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Test_setModule(t, "GraphicsDevice/submitFailure");

	Device dev = Device::share(deviceRef);
	const c::Allocator *alloc = dev.alloc();
	const c::U8 fif = dev.framesInFlight();

	CommandList emptyList;

	if(!makeEmptyList(t, dev, emptyList))
		return;

	//Every slot carries a pending frame first, so the failing frame is one that waited on its slot and reset it

	for(c::U32 i = 0; i < fif; ++i)
		if(!c::Test_assert(t, "warmup", dev.submit({ &emptyList }, {}, 0, 0, &t->err)))
			return;

	constexpr c::U32 elems = 64;
	c::U32 source[elems];

	for(c::U32 i = 0; i < elems; ++i)
		source[i] = 0x5A000000u | (i * 3);

	const c::RefPtrType streamType = c::MemoryStream_makeType(alloc);
	c::MemoryStreamRef *stream = nullptr, *sink = nullptr;
	c::Thread *watchdogThread = nullptr;
	Watchdog watchdog = { {}, 120 * c::SECOND, alloc, "GraphicsDevice/submitFailure" };

	{
		DeviceBuffer buffer;

		if(
			!c::Test_assert(t, "sourceStream", c::MemoryStream_createFromBufferRegion(
				c::Buffer_createRefConst(source, sizeof(source)), 0, sizeof(source),
				c::EMemoryStreamFlags_None, &streamType, &stream, &t->err
			)) ||
			!c::Test_assert(t, "bufferCreate", dev.createBufferStream(
				c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
				"Refused upload", (c::StreamRef*) stream, sizeof(source), buffer, false, nullptr, &t->err
			)) ||
			!c::Test_assert(t, "watchdog", c::Thread_create(alloc, watchdogRun, &watchdog, &watchdogThread, &t->err))
		)
			goto clean;

		{
			c::OxStream *str = RefPtr_data((c::StreamRef*) stream, c::OxStream);
			const c::StreamFunc read = str->read;

			str->read = refusingRead;
			refusedReads = 0;

			c::Error refused = c::Error_none();

			c::Test_assert(t, "failedFrameFails", !dev.submit({ &emptyList }, {}, 0, 0, &refused));
			c::Test_assert(t, "failedInRecording", refusedReads != 0);
			c::Test_assert(t, "failedNotLost", !dev.isLost());

			str->read = read;
		}

		//Where there are breadcrumbs, the failed frame is made to look as if a flush ran a begin whose end never
		// executed: its first slot at 1. Reusing that slot must not keep it as the frame a later loss happened in.

		c::GraphicsDevice *device = c::deviceOf(dev.handle());
		const c::U8 failedFif = device->fifId;

		const c::Bool planted = device->breadcrumbs && device->breadcrumbScopes[failedFif].length;

		if(planted)
			device->breadcrumbs[(c::U64) failedFif * GRAPHICS_BREADCRUMBS] = 1;

		else c::Test_print(t, "Submit failure: no breadcrumbs for the failed frame, failedNotUnfinished is skipped");

		//Each slot reused at least once after the failure, the failed one included, then a device wait

		for(c::U32 i = 0; i < (c::U32) fif + 1; ++i)
			if(!c::Test_assert(t, "submitAfterFailure", dev.submit({ &emptyList }, {}, 0, 0, &t->err)))
				break;

		c::Test_assert(t, "waitAfterFailure", dev.wait(&t->err));
		if(planted)
			c::Test_assert(t, "failedNotUnfinished", !device->breadcrumbUnfinishedScopes.length);

		//The upload the failure refused stayed pending and went with the next frame, so the buffer holds the source

		if(c::Test_assert(t, "sinkStream", c::MemoryStream_create(
			sizeof(source), c::EMemoryStreamFlags_IsWritable, &streamType, &sink, &t->err
		))) {

			c::U32 landed = 0;

			c::Test_assert(t, "queuePull", c::DeviceBufferRef_pullRegionStream(
				buffer.handle(), 0, sizeof(source), (c::StreamRef*) sink, 0, streamPulled, &landed, &t->err
			));

			c::Test_assert(t, "pullSubmit", dev.submit({ &emptyList }, {}, 0, 0, &t->err));
			c::Test_assert(t, "pullWait", dev.wait(&t->err));
			c::Test_assert(t, "pullLanded", landed == 1);

			c::U32 got[elems] = {};
			c::OxStream *out = RefPtr_data((c::StreamRef*) sink, c::OxStream);

			if(c::Test_assert(t, "sinkRead", out->read(
				out, 0, sizeof(got), c::Buffer_createRef(got, sizeof(got)), alloc, &t->err
			))) {

				c::Bool match = true;

				for(c::U32 i = 0; i < elems; ++i)
					match &= got[i] == source[i];

				c::Test_assert(t, "retriedUploadIntact", match);
			}
		}
	}

clean:

	c::AtomicI64_store(&watchdog.done, 1);

	if(watchdogThread)
		c::Test_assert(t, "watchdogJoin", c::Thread_waitAndCleanup(alloc, &watchdogThread, &t->err));

	//Freed while streamType, which they point at, is still on this stack

	c::RefPtr_dec((c::RefPtr**) &sink);
	c::RefPtr_dec((c::RefPtr**) &stream);
}
