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

//graphics/test/interface/test_graphics_refused_buffer.c
//
//GraphicsDevice/refusedBuffer: a buffer create that fails gives back everything it took.
//A buffer the device refuses (bigger than its largest buffer) must not keep the device alive.
//The second half fails one allocation at a time inside the create, which reaches the failures an argument check cannot:
// the device has to be released again after each, whichever allocation was the one that failed,
// and the memory block that was set aside for the buffer or the render texture has to be given back.
//That half runs on a device of its own, whose instance allocates through FailingAllocator; the suite's device is untouched.
//GraphicsDevice/refusedReadback runs on that device too: a readback over a third of the largest buffer is refused, and the
// device reads back what is queued after it.

#include "test_graphics_shared.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/render_texture.h"
#include "types/container/buffer.h"
#include "types/base/allocator.h"
#include "types/base/atomic.h"
#include "types/base/error.h"
#include "types/base/thread.h"
#include "types/base/constants.h"
#include "platforms/platform.h"

//Forwards to the platform allocator, except that while armed the Nth allocation of the arming thread fails.
//Only that thread counts: the device's workers allocate on their own schedule.

typedef struct FailingAllocator {

	Allocator alloc;
	const Allocator *inner;

	U64 thread;
	U64 failAt;        //1 based; 0 is disarmed
	U64 count;
	Bool hit;
	U8 padding[7];

} FailingAllocator;

static Bool FailingAllocator_alloc(void *allocator, U64 length, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;
	FailingAllocator *failing = (FailingAllocator*) allocator;

	if(failing->failAt && Thread_getId() == failing->thread && ++failing->count == failing->failAt) {
		failing->hit = true;
		retError(clean, Error_outOfMemory(0, "FailingAllocator_alloc() fails this allocation on purpose"));
	}

	gotoIfError3(clean, failing->inner->alloc(failing->inner->ptr, length, output, e_rr));

clean:
	return s_uccess;
}

static void FailingAllocator_free(void *allocator, Buffer buf) {
	const FailingAllocator *failing = (const FailingAllocator*) allocator;
	failing->inner->free(failing->inner->ptr, buf);
}

//The blocks the device's allocator holds and how many of them are active
// (a dedicated block that nothing owns stays active, and a block freed from the end of the list is gone)

static void TestGraphics_blocks(GraphicsDeviceRef *deviceRef, U64 *length, U64 *active) {

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	*length = device->allocator.blocks.length;
	*active = 0;

	for(U64 i = 0; i < device->allocator.blocks.length; ++i)
		*active += !!device->allocator.blocks.ptr[i].isActive;
}

static void FailingAllocator_arm(FailingAllocator *failing, U64 failAt) {
	failing->thread = Thread_getId();
	failing->failAt = failAt;
	failing->count = 0;
	failing->hit = false;
}

//A device on the same adapter as the suite's, whose instance allocates through failing

static Bool TestGraphics_failingDevice(
	Test *t,
	GraphicsDeviceRef *suiteDevice,
	FailingAllocator *failing,
	RefPtrType *instanceType,
	GraphicsInstanceRef **instanceRef,
	GraphicsDeviceRef **deviceRef
) {

	const GraphicsDevice *suite = GraphicsDeviceRef_ptr(suiteDevice);
	const GraphicsInstance *suiteInstance = GraphicsInstanceRef_ptr(suite->instance);
	Bool created = false;

	failing->inner = Platform_instance->alloc;
	failing->alloc = (Allocator) { .ptr = failing, .alloc = FailingAllocator_alloc, .free = FailingAllocator_free };

	const GraphicsApplicationInfo appInfo = (GraphicsApplicationInfo) {
		.name = CharString_createRefCStrConst("OxC3 refused buffer test"),
		.version = 1
	};

	*instanceType = GraphicsInstance_makeType(suiteInstance->api, &failing->alloc);

	if(!Test_assert(t, "failingInstance", GraphicsInstance_create(
		&appInfo, suiteInstance->api, suiteInstance->flags | EGraphicsInstanceFlags_DisableGPUBV, &failing->alloc,
		instanceType, instanceRef, &t->err
	)))
		return false;

	//The same adapter as the suite's, matched by name

	ListGraphicsDeviceInfo infos = (ListGraphicsDeviceInfo) { 0 };

	if(Test_assert(t, "failingInfos", GraphicsInstance_getDeviceInfos(
		GraphicsInstanceRef_ptr(*instanceRef), &infos, NULL, &t->err
	))) {

		for(U64 i = 0; i < infos.length; ++i)
			if(Buffer_eq(
				Buffer_createRefConst(infos.ptr[i].name, sizeof(infos.ptr[i].name)),
				Buffer_createRefConst(suite->info.name, sizeof(suite->info.name))
			)) {

				created = Test_assert(t, "failingDevice", GraphicsDeviceRef_create(
					*instanceRef, &infos.ptr[i], EGraphicsDeviceFlags_None, EGraphicsBufferingMode_Default,
					NULL, NULL, deviceRef, &t->err
				));

				break;
			}
	}

	ListGraphicsDeviceInfo_free(&infos, Platform_instance->alloc);

	if(!created)
		RefPtr_dec(instanceRef);

	return created;
}

//Fails one allocation at a time inside the create of a buffer or a render texture of a size that gets a memory block of
// its own, where the device has one to give, until a create goes through.
//Every refused create has to leave the device's reference count and its memory blocks as they were.

static void TestGraphics_sweepCreate(Test *t, FailingAllocator *failing, GraphicsDeviceRef *failingDevice, Bool texture) {

	const CharString name = CharString_createRefCStrConst("refusedBuffer");
	Error err = Error_none();

	const U64 size = 64 * MIBI;        //A 4096 x 4096 RGBA8 texture is the same size
	const I64 before = AtomicI64_load(&failingDevice->refCount);

	U64 blocksBefore, activeBefore;
	TestGraphics_blocks(failingDevice, &blocksBefore, &activeBefore);

	Bool swept = false;

	for(U64 failAt = 1; failAt <= 512; ++failAt) {

		RefPtr *resource = NULL;

		FailingAllocator_arm(failing, failAt);

		const Bool created = texture ?
			GraphicsDeviceRef_createRenderTexture(
				failingDevice, ETextureType_2D, 4096, 4096, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
				EMSAASamples_Off, NULL, &name, &resource, &err
			) :
			GraphicsDeviceRef_createBuffer(
				failingDevice, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name, size, &resource, &err
			);

		const Bool hit = failing->hit;
		FailingAllocator_arm(failing, 0);
		err = Error_none();

		//Fewer allocations than failAt: the sweep has covered every one and the create went through

		if(!hit) {
			swept = Test_assert(t, texture ? "sweepTextureCreated" : "sweepCreated", created && resource);
			RefPtr_dec(&resource);
			break;
		}

		Test_assert(t, texture ? "sweepTextureRefused" : "sweepRefused", !created && !resource);
		Test_assert(
			t, texture ? "sweepTextureReference" : "sweepReference", AtomicI64_load(&failingDevice->refCount) == before
		);

		U64 blocks, active;
		TestGraphics_blocks(failingDevice, &blocks, &active);
		Test_assert(t, texture ? "sweepTextureBlocks" : "sweepBlocks", blocks == blocksBefore && active == activeBefore);
	}

	Test_assert(t, texture ? "sweepTextureComplete" : "sweepComplete", swept);

	Test_assert(
		t, texture ? "sweepTextureReferenceAfter" : "sweepReferenceAfter", AtomicI64_load(&failingDevice->refCount) == before
	);

	U64 blocksAfter, activeAfter;
	TestGraphics_blocks(failingDevice, &blocksAfter, &activeAfter);

	Test_assert(
		t, texture ? "sweepTextureBlocksAfter" : "sweepBlocksAfter", blocksAfter == blocksBefore && activeAfter == activeBefore
	);
}

static void TestGraphics_countPull(void *resource, void *context) {
	(void) resource;
	++*(U32*) context;
}

//Submits one recorded empty list, which is what moves the queued uploads and pulls, and waits for it

static Bool TestGraphics_submitEmpty(GraphicsDeviceRef *deviceRef, CommandListRef *list, Error *e_rr) {

	CommandListRef *lists[1] = { list };
	ListCommandListRef refs = (ListCommandListRef) { 0 };

	return
		ListCommandListRef_createRefConst(lists, 1, &refs, e_rr) &&
		GraphicsDeviceRef_submitCommands(deviceRef, &refs, NULL, -1, 0, e_rr) &&
		GraphicsDeviceRef_wait(deviceRef, e_rr);
}

//A readback that needs more than a third of the largest buffer is refused by the submit that records it, with an out of
// bounds error, and the device still reads back what is queued after that.
//The device's largest buffer is lowered so the refused pull is a few MiB.
//The upload sites cannot be reached the same way: a transfer of a quarter of the staging buffer or more bypasses it,
// and the staging buffer never passes the cap.

static void TestGraphics_refusedReadback(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/refusedReadback");

	const CharString name = CharString_createRefCStrConst("refusedReadback");
	Error err = Error_none();

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	device->info.capabilities.maxBufferSize = 48 * MIBI;        //Thirds of 16 MiB, so a pull over 16 MiB is refused

	CommandListRef *list = NULL;
	DeviceBufferRef *small = NULL;
	DeviceBufferRef *large = NULL;
	U32 pulled = 0;

	const EGraphicsResourceFlag flags = EGraphicsResourceFlag_ShaderRead | EGraphicsResourceFlag_CPUBacked;

	if (
		!Test_assert(t, "readbackList", GraphicsDeviceRef_createCommandList(deviceRef, KIBI, 16, 8, true, &list, &err)) ||
		!Test_assert(t, "readbackListBegin", CommandListRef_begin(list, true, U64_MAX, &err)) ||
		!Test_assert(t, "readbackListEnd", CommandListRef_end(list, &err)) ||
		!Test_assert(t, "readbackSmall", GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_None, flags, NULL, &name, 4096, &small, &err
		)) ||
		!Test_assert(t, "readbackLarge", GraphicsDeviceRef_createBuffer(
			deviceRef, EDeviceBufferUsage_None, flags, NULL, &name, 20 * MIBI, &large, &err
		)) ||
		!Test_assert(t, "readbackUpload", TestGraphics_submitEmpty(deviceRef, list, &err))
	)
		goto clean;

	//The first pull creates the readback staging buffer at the size it needs, which is far below the large pull

	Test_assert(t, "readbackSmallPull", DeviceBufferRef_pullRegion(small, 0, 0, TestGraphics_countPull, &pulled, &err));
	Test_assert(t, "readbackSmallSubmit", TestGraphics_submitEmpty(deviceRef, list, &err));
	Test_assert(t, "readbackSmallPulled", pulled == 1);

	err = Error_none();

	Test_assert(t, "readbackLargePull", DeviceBufferRef_pullRegion(large, 0, 0, TestGraphics_countPull, &pulled, &err));

	const Bool refused = !TestGraphics_submitEmpty(deviceRef, list, &err);

	Test_assert(t, "readbackRefused", refused);
	Test_assert(t, "readbackOutOfBounds", err.genericError == EGenericError_OutOfBounds);
	Test_assert(t, "readbackNotCalled", pulled == 1);

	err = Error_none();

	//Nothing of the refused pull stays queued, so the next one is read back as usual

	Test_assert(t, "readbackAfterPull", DeviceBufferRef_pullRegion(small, 0, 0, TestGraphics_countPull, &pulled, &err));
	Test_assert(t, "readbackAfterSubmit", TestGraphics_submitEmpty(deviceRef, list, &err));
	Test_assert(t, "readbackAfterPulled", pulled == 2);

clean:

	GraphicsDeviceRef_wait(deviceRef, NULL);
	RefPtr_dec(&large);
	RefPtr_dec(&small);
	RefPtr_dec(&list);
}

void Test_graphicsRefusedBuffer(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/refusedBuffer");

	const CharString name = CharString_createRefCStrConst("refusedBuffer");
	Error err = Error_none();

	//Refused for its size: the argument checks run after the device is referenced

	{
		const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
		const I64 before = AtomicI64_load(&deviceRef->refCount);

		for(U64 i = 0; i < 4; ++i) {

			DeviceBufferRef *buffer = NULL;

			const Bool created = GraphicsDeviceRef_createBuffer(
				deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_ShaderRead, NULL, &name,
				device->info.capabilities.maxBufferSize + 1, &buffer, &err
			);

			Test_assert(t, "refusedSize", !created && !buffer);
			err = Error_none();
		}

		Test_assert(t, "refusedReference", AtomicI64_load(&deviceRef->refCount) == before);
	}

	//An allocation that fails inside the create, whichever one it is

	FailingAllocator failing = (FailingAllocator) { 0 };
	RefPtrType instanceType = (RefPtrType) { 0 };
	GraphicsInstanceRef *instanceRef = NULL;
	GraphicsDeviceRef *failingDevice = NULL;

	if(!TestGraphics_failingDevice(t, deviceRef, &failing, &instanceType, &instanceRef, &failingDevice))
		return;

	TestGraphics_sweepCreate(t, &failing, failingDevice, false);
	TestGraphics_sweepCreate(t, &failing, failingDevice, true);
	TestGraphics_refusedReadback(t, failingDevice);

	RefPtr_dec(&failingDevice);
	RefPtr_dec(&instanceRef);
}
