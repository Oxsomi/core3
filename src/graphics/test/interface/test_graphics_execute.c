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

//graphics/test/interface/test_graphics_execute.c

//Submission and real GPU execution modules: submit state machine, memory upkeep, execution
// with readback round trips and acceleration structures (16, 28, 29, 30).

#include "graphics/generic/instance.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/graphics_types.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/test/test.h"
#include "types/container/texture_format.h"
#include "types/container/buffer.h"
#include "types/base/string_base.h"
#include "test_graphics_shared.h"

// -- 16. Submit ------------------------------------------------------------------

//Submit is the only path that reaches GraphicsDevice_rebindDescriptors, which binds the descriptor tables and the
// globals constant buffer for the frame.
//On Vulkan without VK_KHR_push_descriptor that's also the path that allocates and binds the emulated per frame set,
// so nothing else in the suite exercises it.
//An empty but closed command list is enough, since the descriptors are bound before any recorded command is replayed.

void Test_graphicsSubmit(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/submit");

	CommandListRef *commandList = NULL;
	ListCommandListRef lists = (ListCommandListRef) { 0 };

	if(!Test_assert(t, "create", GraphicsDeviceRef_createCommandList(
		deviceRef, 2 * KIBI, 64, 16, true, &commandList, &t->err
	)))
		return;

	Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
	Test_assert(t, "stateOpen", CommandListRef_ptr(commandList)->state == ECommandListState_Open);
	Test_assert(t, "beginTwice", !CommandListRef_begin(commandList, true, U64_MAX, NULL));
	Test_assert(t, "end", CommandListRef_end(commandList, &t->err));
	Test_assert(t, "stateClosed", CommandListRef_ptr(commandList)->state == ECommandListState_Closed);

	//Nothing to submit at all is the one combination that has to be refused

	Test_assert(t, "submitNothing", !GraphicsDeviceRef_submitCommands(deviceRef, NULL, NULL, NULL, 0, 0, NULL));
	Test_assert(t, "submitNullDevice", !GraphicsDeviceRef_submitCommands(NULL, NULL, NULL, NULL, 0, 0, NULL));

	if(Test_assert(t, "listRef", ListCommandListRef_createRefConst(&commandList, 1, &lists, &t->err))) {

		//Submitted twice so a second frame in flight is used, which is what picks a different emulated set

		Test_assert(t, "submit", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));
		Test_assert(t, "submitAgain", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));
		Test_assert(t, "wait", GraphicsDeviceRef_wait(deviceRef, &t->err));
	}

	RefPtr_dec(&commandList);
}

// -- 28. Memory budget, staging buffer and frame upkeep --------------------------

//Runs after submit on purpose: pendingResources has been flushed by then, which is what makes calling
// handleNextFrame without a real command buffer safe, and the staging buffer has actually seen use.

void Test_graphicsDeviceMemory(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/memory");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Test_assert(t, "budgetNullDevice", GraphicsDeviceRef_getMemoryBudget(NULL, false) == U64_MAX);

	const U64 shared = GraphicsDeviceRef_getMemoryBudget(deviceRef, false);
	const U64 local = GraphicsDeviceRef_getMemoryBudget(deviceRef, true);

	//Vulkan needs VK_EXT_memory_budget to report usage; without it (e.g. the Android emulator) skip the checks

	const Bool hasBudget =
		GraphicsInstanceRef_ptr(device->instance)->api != EGraphicsApi_Vulkan ||
		(device->info.capabilities.featuresExt & EVkGraphicsFeatures_MemoryBudget);

	if (hasBudget) {

		//WARP legitimately reports 0 in use, so only the error sentinel is wrong

		Test_assert(t, "budgetShared", shared != U64_MAX);

		//Non dedicated devices report 0 device local by contract, since everything is the same memory

		if(device->info.type == EGraphicsDeviceType_Dedicated)
			Test_assert(t, "budgetLocalDedicated", local && local != U64_MAX);

		else Test_assert(t, "budgetLocalShared", !local);
	}

	else Log_debugLnx("-- GraphicsDevice/memory: Device can't report memory budget, skipping budget checks");

	//The staging buffer can be resized at runtime; the size is aligned to three whole pages, one per frame

	Test_assert(t, "stagingNullDevice", !GraphicsDeviceRef_resizeStagingBuffer(NULL, 12288, NULL));
	Test_assert(t, "stagingExists", device->staging != NULL);

	const U64 oldSize = device->staging ? DeviceBufferRef_ptr(device->staging)->resource.size : 0;

	if(Test_assert(t, "stagingResize", GraphicsDeviceRef_resizeStagingBuffer(deviceRef, 12288, &t->err)))
		Test_assert(t, "stagingResized",
			device->staging && DeviceBufferRef_ptr(device->staging)->resource.size == 12288
		);

	//Restored so later frames keep the size the device was tuned with at creation

	if(oldSize)
		Test_assert(t, "stagingRestore", GraphicsDeviceRef_resizeStagingBuffer(deviceRef, oldSize, &t->err));

	//A resize that can't be satisfied fails the resize, not the device: the replacement is built before the swap,
	// so the old staging buffer has to still be there and still be its old size.
	//A PiB stays comfortably impossible even on machines with TiBs of memory.

	Test_assert(t, "stagingResizeFailsSafe", !GraphicsDeviceRef_resizeStagingBuffer(deviceRef, PEBI, NULL));

	Test_assert(t, "stagingSurvivesFailure",
		device->staging && DeviceBufferRef_ptr(device->staging)->resource.size == oldSize
	);

	//handleNextFrame is a frame step the submit path drives, so it demands the device lock is already held

	Test_assert(t, "frameNullDevice", !GraphicsDeviceRef_handleNextFrame(NULL, NULL, NULL));
	Test_assert(t, "frameNeedsLock", !GraphicsDeviceRef_handleNextFrame(deviceRef, NULL, NULL));

	//After submit and wait nothing is pending, so no flush runs and no command buffer is needed

	Test_assert(t, "nothingPending", !device->pendingResources.length);

	if (!device->pendingResources.length) {

		const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

		Test_assert(t, "frameLocked", GraphicsDeviceRef_handleNextFrame(deviceRef, NULL, &t->err));
		Test_assert(t, "inFlightReleased", !device->resourcesInFlight[device->fifId].length);

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->lock);
	}
}

// -- 29. GPU execution -----------------------------------------------------------

//Everything before this only ever submitted empty command lists, so the backend replay of clears, copies, scope
// barriers and the initial data upload had never actually run on a device.
//Results can't be asserted yet because pullRegion (the GPU to CPU readback the docs describe) isn't implemented,
// so this verifies the recording executes and the device survives it, which is still the whole replay path.

static void Test_pullCallback(RefPtr *resource, void *context) {
	(void) resource;
	++*(U32*)context;
}

//Render target pulls hand over an owned buffer; the callback inspects it without taking ownership

typedef struct TestTexturePull {
	U32 count;
	U32 expected, matching;
	U64 len;
} TestTexturePull;

static void Test_texturePullCallback(RefPtr *resource, Buffer *data, void *context) {

	(void) resource;

	TestTexturePull *result = (TestTexturePull*) context;

	++result->count;
	result->len = data ? Buffer_length(*data) : 0;
	result->matching = 0;

	for(U64 i = 0; data && (i + 1) * 4 <= Buffer_length(*data); ++i)
		result->matching += ((const U32*)data->ptr)[i] == result->expected;
}

void Test_graphicsGpuExecute(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/execute");

	const Allocator *alloc = Platform_instance->alloc;

	RenderTextureRef *target = NULL;
	DeviceTextureRef *texture = NULL;
	CommandListRef *commandList = NULL;
	CommandListRef *emptyList = NULL;

	const CharString targetName = CharString_createRefCStrConst("Execute target");
	const CharString textureName = CharString_createRefCStrConst("Execute texture");

	const ImageRange all = (ImageRange) { .levelId = U32_MAX, .layerId = U32_MAX };

	if(!Test_assert(t, "createTarget", GraphicsDeviceRef_createRenderTexture(
		deviceRef, ETextureType_2D, 4, 4, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
		EMSAASamples_Off, NULL, &targetName, &target, &t->err
	)))
		return;

	//With initial data, so the first submit also runs the staging upload for real

	Buffer texData = Buffer_createNull();
	Test_assert(t, "texData", Buffer_createEmptyBytes(4 * 4 * 4, alloc, &texData, &t->err));

	Test_assert(t, "createTexture", GraphicsDeviceRef_createTexture(
		deviceRef, ETextureType_2D, ETextureFormatId_RGBA8, EGraphicsResourceFlag_CPUBacked,
		4, 4, 1, NULL, &textureName, &texData, &texture, &t->err
	));

	Buffer_free(&texData, alloc);

	//A CPU backed buffer with a known pattern, so the pull below can prove the bytes made a real GPU round trip

	DeviceBufferRef *pattern = NULL;
	const CharString patternName = CharString_createRefCStrConst("Execute pattern buffer");

	U8 patternData[32];

	for(U8 i = 0; i < 32; ++i)
		patternData[i] = i;

	Buffer patternRef = Buffer_createRefConst(patternData, sizeof(patternData));

	Test_assert(t, "createPattern", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_None, EGraphicsResourceFlag_CPUBacked, NULL,
		&patternName, &patternRef, &pattern, &t->err
	));

	if(texture && Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	))) {

		Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));

		//Clear in one scope, copy in the next, so the cross scope barrier is replayed too

		Test_assert(t, "clearScope", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));

		Test_assert(t, "clear", CommandListRef_clearImagef(
			commandList, F32x4_create4(1, 0, 0, 1), all, target, &t->err
		));

		Test_assert(t, "endClearScope", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "copyScope", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));

		Test_assert(t, "copy", CommandListRef_copyImage(
			commandList, target, texture, (CopyImageRegion) { 0 }, &t->err
		));

		Test_assert(t, "endCopyScope", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

		ListCommandListRef lists = (ListCommandListRef) { 0 };
		ListCommandListRef_createRefConst(&commandList, 1, &lists, NULL);

		//Twice, so the second replay starts from state the first one left behind

		Test_assert(t, "submit", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));
		Test_assert(t, "submitAgain", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));
		Test_assert(t, "wait", GraphicsDeviceRef_wait(deviceRef, &t->err));

		//Read the results back: the copied texture has to hold the clear color and the pattern buffer its bytes.
		//The buffer's cpuData is scribbled over first, so only a real GPU to CPU copy can make the assert pass.

		U32 pulled = 0;

		if (pattern) {

			DeviceBuffer *patternPtr = DeviceBufferRef_ptr(pattern);

			for(U8 i = 0; i < 32; ++i)
				patternPtr->cpuData.ptrNonConst[i] = 0xCC;

			Test_assert(t, "pullBuffer", DeviceBufferRef_pullRegion(
				pattern, 0, 0, Test_pullCallback, &pulled, &t->err
			));
		}

		Test_assert(t, "pullTexture", DeviceTextureRef_pullRegion(
			texture, 0, 0, 0, 0, 0, 0, Test_pullCallback, &pulled, &t->err
		));

		//Wrong type and out of bounds regions are refused before anything is queued

		Test_assert(t, "pullWrongType", !DeviceTextureRef_pullRegion(target, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL));
		Test_assert(t, "pullOOB", pattern && !DeviceBufferRef_pullRegion(pattern, 64, 0, NULL, NULL, NULL));
		Test_assert(t, "pullOOBRegion", !DeviceTextureRef_pullRegion(texture, 1, 0, 0, 4, 0, 0, NULL, NULL, NULL));

		//Nothing completes before the frame that carries the copies has provably finished

		Test_assert(t, "pullNotYet", !pulled);

		Test_assert(t, "submitPull", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));

		//Growing the ring while pulls are in flight swaps the buffers; the pulls have to survive on the old one

		Test_assert(t, "reserveWhileInFlight", GraphicsDeviceRef_reserveReadback(deviceRef, 64 * KIBI, &t->err));

		Test_assert(t, "waitPull", GraphicsDeviceRef_wait(deviceRef, &t->err));

		Test_assert(t, "pullCompleted", pulled == (pattern ? 2u : 1u));

		//Reserving less than what's already there never shrinks, and a NULL device is refused

		Test_assert(t, "reserveNoShrink", GraphicsDeviceRef_reserveReadback(deviceRef, 0, &t->err));
		Test_assert(t, "reserveInvalid", !GraphicsDeviceRef_reserveReadback(NULL, 0, NULL));

		//RGBA8 red is FF 00 00 FF in memory, 0xFF0000FF read as little endian

		const DeviceTexture *texPtr = DeviceTextureRef_ptr(texture);
		Bool allRed = Buffer_length(texPtr->cpuData) == 4 * 4 * 4;

		for(U64 i = 0; i < 16 && allRed; ++i)
			allRed &= ((const U32*)texPtr->cpuData.ptr)[i] == 0xFF0000FFu;

		Test_assert(t, "pixelsMatchClear", allRed);

		if (pattern) {

			const DeviceBuffer *patternPtr = DeviceBufferRef_ptr(pattern);
			Bool matches = true;

			for(U8 i = 0; i < 32; ++i)
				matches &= patternPtr->cpuData.ptr[i] == i;

			Test_assert(t, "patternSurvivedRoundTrip", matches);
		}

		//An op-less list, so upload and readback rounds don't replay the clear and copy above

		ListCommandListRef emptyLists = (ListCommandListRef) { 0 };

		if(
			Test_assert(t, "createEmptyList", GraphicsDeviceRef_createCommandList(
				deviceRef, 4 * KIBI, 64, 16, true, &emptyList, &t->err
			)) &&
			Test_assert(t, "beginEmptyList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err)) &&
			Test_assert(t, "endEmptyList", CommandListRef_end(emptyList, &t->err))
		) {

			ListCommandListRef_createRefConst(&emptyList, 1, &emptyLists, NULL);

			//A partial upload followed by a partial pull, so the region row math is proven in both directions

			DeviceTexture *texPtr2 = DeviceTextureRef_ptr(texture);
			U32 pulledRegion = 0;

			for(U64 j = 0; j < 2; ++j)
				for(U64 i = 0; i < 2; ++i)
					((U32*)texPtr2->cpuData.ptrNonConst)[(1 + j) * 4 + (1 + i)] = 0xAABBCCDDu;

			Test_assert(t, "regionMarkDirty", DeviceTextureRef_markDirty(texture, 1, 1, 0, 2, 2, 1, &t->err));

			Test_assert(t, "regionUploadSubmit", GraphicsDeviceRef_submitCommands(
				deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
			));

			Test_assert(t, "regionUploadWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

			for(U64 i = 0; i < 16; ++i)
				((U32*)texPtr2->cpuData.ptrNonConst)[i] = 0x5C5C5C5Cu;

			Test_assert(t, "regionPull", DeviceTextureRef_pullRegion(
				texture, 1, 1, 0, 2, 2, 1, Test_pullCallback, &pulledRegion, &t->err
			));

			Test_assert(t, "regionPullSubmit", GraphicsDeviceRef_submitCommands(
				deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
			));

			Test_assert(t, "regionPullWait", GraphicsDeviceRef_wait(deviceRef, &t->err));
			Test_assert(t, "regionPullCompleted", pulledRegion == 1);

			//The pulled region came back from the GPU while everything around it kept the scribble

			Bool regionOk = true;

			for(U64 j = 0; j < 4; ++j)
				for(U64 i = 0; i < 4; ++i) {

					const U32 got = ((const U32*)texPtr2->cpuData.ptr)[j * 4 + i];
					const Bool inRegion = i >= 1 && i < 3 && j >= 1 && j < 3;

					regionOk &= got == (inRegion ? 0xAABBCCDDu : 0x5C5C5C5Cu);
				}

			Test_assert(t, "regionRoundTrip", regionOk);

			//Compressed formats go through the same math in block rows, proven end to end with BC7

			if (GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.dataTypes & EGraphicsDataTypes_BCn) {

				DeviceTextureRef *bcTex = NULL;
				const CharString bcName = CharString_createRefCStrConst("Execute BC7 texture");

				Buffer bcData = Buffer_createNull();
				Test_assert(t, "bcData", Buffer_createEmptyBytes(64, alloc, &bcData, &t->err));

				for(U8 i = 0; i < 64; ++i)
					bcData.ptrNonConst[i] = (U8)(i * 3);

				Test_assert(t, "bcCreate", GraphicsDeviceRef_createTexture(
					deviceRef, ETextureType_2D, ETextureFormatId_BC7, EGraphicsResourceFlag_CPUBacked,
					8, 8, 1, NULL, &bcName, &bcData, &bcTex, &t->err
				));

				Buffer_free(&bcData, alloc);

				if (bcTex) {

					U32 pulledBc = 0;
					DeviceTexture *bcPtr = DeviceTextureRef_ptr(bcTex);

					Test_assert(t, "bcUploadSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "bcUploadWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

					for(U8 i = 0; i < 64; ++i)
						bcPtr->cpuData.ptrNonConst[i] = 0xEE;

					Test_assert(t, "bcPull", DeviceTextureRef_pullRegion(
						bcTex, 0, 0, 0, 0, 0, 0, Test_pullCallback, &pulledBc, &t->err
					));

					Test_assert(t, "bcPullSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "bcPullWait", GraphicsDeviceRef_wait(deviceRef, &t->err));
					Test_assert(t, "bcPullCompleted", pulledBc == 1);

					Bool bcMatches = true;

					for(U8 i = 0; i < 64; ++i)
						bcMatches &= bcPtr->cpuData.ptr[i] == (U8)(i * 3);

					Test_assert(t, "bcRoundTrip", bcMatches);

					//One block from the middle of the block grid; an 8x8 BC7 is 2x2 blocks of 16 bytes

					for(U8 i = 0; i < 64; ++i)
						bcPtr->cpuData.ptrNonConst[i] = 0xEE;

					Test_assert(t, "bcPullBlock", DeviceTextureRef_pullRegion(
						bcTex, 4, 4, 0, 4, 4, 1, Test_pullCallback, &pulledBc, &t->err
					));

					Test_assert(t, "bcPullBlockSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "bcPullBlockWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

					Bool bcBlock = true;

					for(U8 i = 0; i < 64; ++i)
						bcBlock &= bcPtr->cpuData.ptr[i] == (i >= 48 ? (U8)(i * 3) : 0xEE);

					Test_assert(t, "bcBlockRegion", bcBlock);
				}

				RefPtr_dec(&bcTex);
			}

			else Test_print(t, "Device lacks BCn, skipping compressed readback tests");

			//Render targets have no cpuData; their pulls hand the region to the callback as an owned buffer.
			//The target still holds the clear color from the replay above, which is what proves the transport.

			TestTexturePull targetPull = (TestTexturePull) { .expected = 0xFF0000FFu };

			Test_assert(t, "targetPull", TextureRef_pullRegion(
				target, 0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &targetPull, &t->err
			));

			Test_assert(t, "targetPullSubmit", GraphicsDeviceRef_submitCommands(
				deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
			));

			Test_assert(t, "targetPullWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

			Test_assert(t, "targetPullCompleted", targetPull.count == 1);
			Test_assert(t, "targetPullLen", targetPull.len == 4 * 4 * 4);
			Test_assert(t, "targetPullRed", targetPull.matching == 16);

			//Depth has to be initialized before its first read (D3D12 demands a clear, discard or copy first),
			// so a draw less render pass clears it; that same pass also regression tests that a clear load
			// alone keeps its scope alive instead of being rewound as an empty scope

			DepthStencilRef *depth = NULL;
			const CharString depthName = CharString_createRefCStrConst("Execute depth");

			Test_assert(t, "depthCreate", GraphicsDeviceRef_createDepthStencil(
				deviceRef, 4, 4, EDepthStencilFormat_D32, false, EMSAASamples_Off, NULL, &depthName, &depth, &t->err
			));

			if (depth && (GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {

				CommandListRef *depthList = NULL;

				if (Test_assert(t, "depthListCreate", GraphicsDeviceRef_createCommandList(
					deviceRef, 4 * KIBI, 64, 16, true, &depthList, &t->err
				))) {

					Test_assert(t, "depthListBegin", CommandListRef_begin(depthList, true, U64_MAX, &t->err));
					Test_assert(t, "depthScope", CommandListRef_startScope(depthList, NULL, 1, NULL, &t->err));

					AttachmentInfo color = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
					ListAttachmentInfo colors = (ListAttachmentInfo) { 0 };
					ListAttachmentInfo_createRefConst(&color, 1, &colors, NULL);

					const DepthStencilAttachmentInfo depthAttach = (DepthStencilAttachmentInfo) {
						.image = depth,
						.depthLoad = ELoadAttachmentType_Clear,
						.clearDepth = 0.5f
					};

					Test_assert(t, "depthRenderStart", CommandListRef_startRenderExt(
						depthList, I32x2_zero, I32x2_create2(4, 4), &colors, &depthAttach, &t->err
					));

					Test_assert(t, "depthRenderEnd", CommandListRef_endRenderExt(depthList, &t->err));
					Test_assert(t, "depthScopeEnd", CommandListRef_endScope(depthList, &t->err));

					//The clear only scope must survive endScope, or the clear silently never runs

					Test_assert(t, "depthScopeKept", CommandListRef_ptr(depthList)->activeScopes.length == 1);

					Test_assert(t, "depthListEnd", CommandListRef_end(depthList, &t->err));

					ListCommandListRef depthLists = (ListCommandListRef) { 0 };
					ListCommandListRef_createRefConst(&depthList, 1, &depthLists, NULL);

					Test_assert(t, "depthClearSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &depthLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "depthClearWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

					//A 0.5f clear reads back as its exact bit pattern in every D32 texel

					TestTexturePull depthPull = (TestTexturePull) { .expected = 0x3F000000u };

					Test_assert(t, "depthPull", TextureRef_pullRegion(
						depth, 0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &depthPull, &t->err
					));

					Test_assert(t, "depthPullSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "depthPullWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

					Test_assert(t, "depthPullCompleted", depthPull.count == 1);
					Test_assert(t, "depthPullLen", depthPull.len == 4 * 4 * 4);
					Test_assert(t, "depthPullHalf", depthPull.matching == 16);
				}

				RefPtr_dec(&depthList);
			}

			else Test_print(t, "Device lacks direct rendering (or depth), skipping depth pull test");

			RefPtr_dec(&depth);

			//The stencil variant of the same clear + pull: a stencil bearing format pulls its STENCIL plane at
			// one byte per texel, so a 0xAB clear reads back as 16 bytes of 0xAB, which the U32 comparison sees as
			// four words of 0xABABABAB.
			//D32S8 rather than D24S8, since D3D12 suppresses D24S8 on some hardware by design.
			//Skipped rather than failed when absent, since neither combined format is required of an adapter.

			if (GraphicsDeviceInfo_supportsDepthStencilFormat(
				&GraphicsDeviceRef_ptr(deviceRef)->info, EDepthStencilFormat_D32S8X24Ext
			) && (GraphicsDeviceRef_ptr(deviceRef)->info.capabilities.features & EGraphicsFeatures_DirectRendering)) {

				DepthStencilRef *stencilTex = NULL;
				CommandListRef *stencilList = NULL;
				const CharString stencilName = CharString_createRefCStrConst("Execute stencil");

				Test_assert(t, "stencilCreate", GraphicsDeviceRef_createDepthStencil(
					deviceRef, 4, 4, EDepthStencilFormat_D32S8X24Ext, false, EMSAASamples_Off, NULL,
					&stencilName, &stencilTex, &t->err
				));

				if (stencilTex && Test_assert(t, "stencilListCreate", GraphicsDeviceRef_createCommandList(
					deviceRef, 4 * KIBI, 64, 16, true, &stencilList, &t->err
				))) {

					Test_assert(t, "stencilListBegin", CommandListRef_begin(stencilList, true, U64_MAX, &t->err));
					Test_assert(t, "stencilScope", CommandListRef_startScope(stencilList, NULL, 1, NULL, &t->err));

					AttachmentInfo stColor = (AttachmentInfo) { .image = target, .load = ELoadAttachmentType_Clear };
					ListAttachmentInfo stColors = (ListAttachmentInfo) { 0 };
					ListAttachmentInfo_createRefConst(&stColor, 1, &stColors, NULL);

					//Both planes are cleared, since leaving depth to a load would read uninitialized memory on the
					// APIs that demand a clear, discard or copy before the first use.

					const DepthStencilAttachmentInfo stencilAttach = (DepthStencilAttachmentInfo) {
						.image = stencilTex,
						.depthLoad = ELoadAttachmentType_Clear,
						.stencilLoad = ELoadAttachmentType_Clear,
						.clearDepth = 1,
						.clearStencil = 0xAB
					};

					Test_assert(t, "stencilRenderStart", CommandListRef_startRenderExt(
						stencilList, I32x2_zero, I32x2_create2(4, 4), &stColors, &stencilAttach, &t->err
					));

					Test_assert(t, "stencilRenderEnd", CommandListRef_endRenderExt(stencilList, &t->err));
					Test_assert(t, "stencilScopeEnd", CommandListRef_endScope(stencilList, &t->err));
					Test_assert(t, "stencilListEnd", CommandListRef_end(stencilList, &t->err));

					ListCommandListRef stencilLists = (ListCommandListRef) { 0 };
					ListCommandListRef_createRefConst(&stencilList, 1, &stencilLists, NULL);

					Test_assert(t, "stencilClearSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &stencilLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "stencilClearWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

					TestTexturePull stencilPull = (TestTexturePull) { .expected = 0xABABABABu };

					Test_assert(t, "stencilPull", TextureRef_pullRegion(
						stencilTex, 0, 0, 0, 0, 0, 0, 1, Test_texturePullCallback, &stencilPull, &t->err
					));

					Test_assert(t, "stencilPullSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "stencilPullWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

					Test_assert(t, "stencilPullCompleted", stencilPull.count == 1);
					Test_assert(t, "stencilPullLen", stencilPull.len == 4 * 4);
					Test_assert(t, "stencilPullValue", stencilPull.matching == 4);

					//The DEPTH plane of the same combined texture: the 1.0f clear reads back as its exact bit
					// pattern, proving both planes of one format are separately reachable.

					TestTexturePull depthPlanePull = (TestTexturePull) { .expected = 0x3F800000u };

					Test_assert(t, "stencilDepthPull", TextureRef_pullRegion(
						stencilTex, 0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &depthPlanePull, &t->err
					));

					Test_assert(t, "stencilDepthPullSubmit", GraphicsDeviceRef_submitCommands(
						deviceRef, &emptyLists, NULL, NULL, 0, 0, &t->err
					));

					Test_assert(t, "stencilDepthPullWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

					Test_assert(t, "stencilDepthPullCompleted", depthPlanePull.count == 1);
					Test_assert(t, "stencilDepthPullLen", depthPlanePull.len == 4 * 4 * 4);
					Test_assert(t, "stencilDepthPullValue", depthPlanePull.matching == 16);
				}

				RefPtr_dec(&stencilList);
				RefPtr_dec(&stencilTex);
			}

			else Test_print(t, "Device lacks D32S8 (or direct rendering), skipping stencil pull test");

			//The refusals: wrong type, missing callback and MSAA (which also can't be copied, only resolved)

			Test_assert(t, "pullDeviceTexRefused", !TextureRef_pullRegion(
				texture, 0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &targetPull, NULL
			));

			//A plane the format doesn't have is refused rather than silently pulled as plane 0: plane 1 only
			// exists on combined depth stencil formats, so a color target rejects it.

			Test_assert(t, "pullBadPlaneRefused", !TextureRef_pullRegion(
				target, 0, 0, 0, 0, 0, 0, 1, Test_texturePullCallback, &targetPull, NULL
			));

			Test_assert(t, "pullNoCallbackRefused", !TextureRef_pullRegion(target, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL));

			RenderTextureRef *msaaTarget = NULL;
			const CharString msaaName = CharString_createRefCStrConst("Execute MSAA target");

			Test_assert(t, "msaaCreate", GraphicsDeviceRef_createRenderTexture(
				deviceRef, ETextureType_2D, 4, 4, 1, ETextureFormatId_RGBA8, EGraphicsResourceFlag_None,
				EMSAASamples_x4, NULL, &msaaName, &msaaTarget, &t->err
			));

			if (msaaTarget) {

				Test_assert(t, "pullMsaaRefused", !TextureRef_pullRegion(
					msaaTarget, 0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &targetPull, NULL
				));

				//Copying MSAA is refused at record time now; the failed command invalidates the recording,
				// which is why end isn't asserted and the list isn't reused afterwards

				Test_assert(t, "msaaCopyBegin", CommandListRef_begin(emptyList, true, U64_MAX, &t->err));
				Test_assert(t, "msaaCopyScope", CommandListRef_startScope(emptyList, NULL, 1, NULL, &t->err));

				Test_assert(t, "copyMsaaRefused", !CommandListRef_copyImage(
					emptyList, msaaTarget, target, (CopyImageRegion) { 0 }, NULL
				));

				CommandListRef_end(emptyList, NULL);
			}

			RefPtr_dec(&msaaTarget);
		}
	}

	RefPtr_dec(&emptyList);
	RefPtr_dec(&commandList);
	RefPtr_dec(&pattern);
	RefPtr_dec(&texture);
	RefPtr_dec(&target);
}

// -- 30. Acceleration structures -------------------------------------------------

//BLAS and TLAS creation queue their build for the next submit, so submitting after recording the updates is what
// actually builds them on the device.
//Everything here needs the raytracing feature, so software adapters skip with a message rather than fail.

void Test_graphicsAccelerationStructures(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "Raytracing/AS");

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!(device->info.capabilities.features & EGraphicsFeatures_Raytracing)) {
		Test_print(t, "Device lacks raytracing, skipping acceleration structure tests");
		return;
	}

	const Allocator *alloc = Platform_instance->alloc;

	DeviceBufferRef *positions = NULL;
	DeviceBufferRef *plain = NULL;
	BLASRef *blas = NULL;
	BLASRef *badBlas = NULL;
	TLASRef *tlas = NULL;
	CommandListRef *commandList = NULL;

	const CharString positionsName = CharString_createRefCStrConst("AS positions");
	const CharString plainName = CharString_createRefCStrConst("AS plain buffer");
	const CharString blasName = CharString_createRefCStrConst("Test BLAS");
	const CharString tlasName = CharString_createRefCStrConst("Test TLAS");

	//One triangle in RGBA32f, which every raytracing device accepts

	const F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	Buffer triData = Buffer_createRefConst(triangle, sizeof(triangle));

	if(!Test_assert(t, "createPositions", GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_ASReadExt, EGraphicsResourceFlag_None, NULL,
		&positionsName, &triData, &positions, &t->err
	)))
		goto clean;

	Test_assert(t, "createPlain", GraphicsDeviceRef_createBuffer(
		deviceRef, EDeviceBufferUsage_Vertex, EGraphicsResourceFlag_None, NULL, &plainName, 48, &plain, &t->err
	));

	const DeviceData positionData = (DeviceData) { .buffer = positions };

	//A buffer without ASReadExt usage can't feed an AS build, and a zero stride can't address vertices

	if (plain) {

		const DeviceData plainData = (DeviceData) { .buffer = plain };

		Test_assert(t, "blasWrongUsage", !GraphicsDeviceRef_createBLASExt(
			deviceRef, ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0,
			ETextureFormatId_Undefined, 16, plainData, (DeviceData) { 0 }, NULL, &blasName, &badBlas, NULL
		));
	}

	Test_assert(t, "blasZeroStride", !GraphicsDeviceRef_createBLASExt(
		deviceRef, ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0,
		ETextureFormatId_Undefined, 0, positionData, (DeviceData) { 0 }, NULL, &blasName, &badBlas, NULL
	));

	Test_assert(t, "blasRejectedNothing", !badBlas);

	if(!Test_assert(t, "createBlas", GraphicsDeviceRef_createBLASExt(
		deviceRef, ERTASBuildFlags_None, EBLASFlag_None, ETextureFormatId_RGBA32f, 0,
		ETextureFormatId_Undefined, 16, positionData, (DeviceData) { 0 }, NULL, &blasName, &blas, &t->err
	)))
		goto clean;

	Test_assert(t, "blasTypeId", blas->refPtrType->typeId == (TypeId) EGraphicsTypeId_BLASExt);

	//One instance at identity, pointing at the BLAS just made

	TLASInstanceStatic instance = (TLASInstanceStatic) {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = (TLASInstanceData) {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (U32) ETLASInstanceFlag_Default << 24,
			.blasCpu = blas
		}
	};

	ListTLASInstanceStatic instances = (ListTLASInstanceStatic) { 0 };
	ListTLASInstanceStatic_createRefConst(&instance, 1, &instances, NULL);

	if(!Test_assert(t, "createTlas", GraphicsDeviceRef_createTLASExt(
		deviceRef, ERTASBuildFlags_DefaultTLAS, NULL, &instances, true, NULL, &tlasName, &tlas, &t->err
	)))
		goto clean;

	Test_assert(t, "tlasTypeId", tlas->refPtrType->typeId == (TypeId) EGraphicsTypeId_TLASExt);

	//The CPU side instance plumbing hands back what went in

	TLASInstanceData roundTrip = (TLASInstanceData) { 0 };

	Test_assert(t, "instanceData", TLAS_getInstanceDataCpu(TLASRef_ptr(tlas), 0, &roundTrip));
	Test_assert(t, "instanceBlas", roundTrip.blasCpu == blas);
	Test_assert(t, "instanceOOB", !TLAS_getInstanceDataCpu(TLASRef_ptr(tlas), 1, &roundTrip));

	//Recording the updates and submitting is what runs the actual builds on the device

	if(Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
		deviceRef, 4 * KIBI, 64, 16, true, &commandList, &t->err
	))) {

		Test_assert(t, "begin", CommandListRef_begin(commandList, true, U64_MAX, &t->err));
		//The TLAS update transitions the BLASes it references, so it can't share a scope with the BLAS update;
		// the split also expresses the real dependency between the two builds.

		Test_assert(t, "scope", CommandListRef_startScope(commandList, NULL, 1, NULL, &t->err));
		Test_assert(t, "updateBlas", CommandListRef_updateBLASExt(commandList, blas, &t->err));
		Test_assert(t, "endScope", CommandListRef_endScope(commandList, &t->err));

		Test_assert(t, "scopeTlas", CommandListRef_startScope(commandList, NULL, 2, NULL, &t->err));
		Test_assert(t, "updateTlas", CommandListRef_updateTLASExt(commandList, tlas, &t->err));
		Test_assert(t, "endScopeTlas", CommandListRef_endScope(commandList, &t->err));
		Test_assert(t, "end", CommandListRef_end(commandList, &t->err));

		ListCommandListRef lists = (ListCommandListRef) { 0 };
		ListCommandListRef_createRefConst(&commandList, 1, &lists, NULL);

		Test_assert(t, "submit", GraphicsDeviceRef_submitCommands(deviceRef, &lists, NULL, NULL, 0, 0, &t->err));
		Test_assert(t, "wait", GraphicsDeviceRef_wait(deviceRef, &t->err));
	}

clean:

	RefPtr_dec(&commandList);
	RefPtr_dec(&tlas);
	RefPtr_dec(&blas);
	RefPtr_dec(&plain);
	RefPtr_dec(&positions);

	(void) alloc;
}
