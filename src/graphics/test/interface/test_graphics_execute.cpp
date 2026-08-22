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

//graphics/test/interface/test_graphics_execute.cpp

//Submission and real GPU execution modules: submit state machine, memory upkeep, execution
// with readback round trips and acceleration structures (16, 28, 29, 30).

//The shared helpers in terms of the handle types. Both C++ headers come BEFORE the block below: a
//standard header included after the C headers landed in oxc::c finds its guard already tripped and
//leaves its symbols in that namespace.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and
//so cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_registers.h"
	#include "platforms/logx.h"
	#include "platforms/platform.h"
	#include "graphics/generic/blas.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/depth_stencil.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_info.h"
	#include "graphics/generic/device_texture.h"
	#include "graphics/generic/graphics_types.h"
	#include "graphics/generic/instance.h"
	#include "graphics/generic/opacity_micromap.h"
	#include "graphics/generic/render_texture.h"
	#include "graphics/generic/tlas.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

//Same namespace the C headers landed in, so the definitions here match the declarations in
//test_graphics_shared.h and the macros in those headers still expand to names that resolve.

// -- 16. Submit ------------------------------------------------------------------

//Submit is the only path that reaches GraphicsDevice_rebindDescriptors, which binds the descriptor tables and the
// globals constant buffer for the frame.
//On Vulkan without VK_KHR_push_descriptor that's also the path that allocates and binds the emulated per frame set,
// so nothing else in the suite exercises it.
//An empty but closed command list is enough, since the descriptors are bound before any recorded command is replayed.

extern "C" void Test_graphicsSubmit(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/submit");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfx::CommandList commandList;

	if(!Test_assert(t, "create", dev.createCommandList(2 * c::KIBI, 64, 16, commandList, true, e_rr)))
		return;

	Test_assert(t, "begin", commandList.begin(true, e_rr));
	Test_assert(t, "stateOpen", commandList.data()->state == c::ECommandListState_Open);
	Test_assert(t, "beginTwice", !commandList.begin(true, nullptr));
	Test_assert(t, "end", commandList.end(e_rr));
	Test_assert(t, "stateClosed", commandList.data()->state == c::ECommandListState_Closed);

	//Nothing to submit at all is the one combination that has to be refused.
	//A null device is not something a handle can hold, so that one stays on the C entry point.

	Test_assert(t, "submitNothing", !dev.submit({}, {}, 0, 0, nullptr));
	Test_assert(t, "submitNullDevice", !c::GraphicsDeviceRef_submitCommands(NULL, NULL, NULL, 0, 0, NULL));

	//Submitted twice so a second frame in flight is used, which is what picks a different emulated set.
	//The C form asserted that a ListCommandListRef could be built from the list as well; Device::submit
	//builds that list itself, so there is nothing left for that assert to have checked.

	Test_assert(t, "submit", dev.submit({ &commandList }, {}, 0, 0, e_rr));
	Test_assert(t, "submitAgain", dev.submit({ &commandList }, {}, 0, 0, e_rr));
	Test_assert(t, "wait", dev.wait(e_rr));
}

// -- 28. Memory budget, staging buffer and frame upkeep --------------------------

//Runs after submit on purpose: pendingResources has been flushed by then, which is what makes calling
// handleNextFrame without a real command buffer safe, and the staging buffer has actually seen use.

extern "C" void Test_graphicsDeviceMemory(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/memory");

	gfx::Device dev = gfx::Device::share(deviceRef);

	//The staging buffer, pendingResources, the lock and resourcesInFlight are the device's own frame
	//bookkeeping, which is what this module is about and is not something the handle layer exposes.

	c::GraphicsDevice *device = c::deviceOf(deviceRef);

	Test_assert(t, "budgetNullDevice", c::GraphicsDeviceRef_getMemoryBudget(NULL, false) == c::U64_MAX);

	const c::U64 shared = dev.getMemoryBudget(false);
	const c::U64 local = dev.getMemoryBudget(true);

	//Vulkan needs VK_EXT_memory_budget to report usage; without it (e.g. the Android emulator) skip the checks

	const c::Bool hasBudget =
		dev.api() != c::EGraphicsApi_Vulkan ||
		(dev.info().capabilities.featuresExt & c::EVkGraphicsFeatures_MemoryBudget);

	if (hasBudget) {

		//WARP legitimately reports 0 in use, so only the error sentinel is wrong

		Test_assert(t, "budgetShared", shared != c::U64_MAX);

		//Non dedicated devices report 0 device local by contract, since everything is the same memory

		if(dev.info().type == c::EGraphicsDeviceType_Dedicated)
			Test_assert(t, "budgetLocalDedicated", local && local != c::U64_MAX);

		else Test_assert(t, "budgetLocalShared", !local);
	}

	else Log::debugLn(
		*dev.alloc(), "-- GraphicsDevice/memory: Device can't report memory budget, skipping budget checks"
	);

	//The staging buffer can be resized at runtime; the size is aligned to three whole pages, one per frame

	Test_assert(t, "stagingNullDevice", !c::GraphicsDeviceRef_resizeStagingBuffer(NULL, 12288, NULL));
	Test_assert(t, "stagingExists", device->staging != NULL);

	const c::U64 oldSize = device->staging ? c::bufferOf(device->staging)->resource.size : 0;

	if(Test_assert(t, "stagingResize", c::GraphicsDeviceRef_resizeStagingBuffer(deviceRef, 12288, &t->err)))
		Test_assert(t, "stagingResized",
			device->staging && c::bufferOf(device->staging)->resource.size == 12288
		);

	//Restored so later frames keep the size the device was tuned with at creation

	if(oldSize)
		Test_assert(t, "stagingRestore", c::GraphicsDeviceRef_resizeStagingBuffer(deviceRef, oldSize, &t->err));

	//A resize that can't be satisfied fails the resize, not the device: the replacement is built before the swap,
	// so the old staging buffer has to still be there and still be its old size.
	//A PiB stays comfortably impossible even on machines with TiBs of memory.

	Test_assert(t, "stagingResizeFailsSafe", !c::GraphicsDeviceRef_resizeStagingBuffer(deviceRef, c::PEBI, NULL));

	Test_assert(t, "stagingSurvivesFailure",
		device->staging && c::bufferOf(device->staging)->resource.size == oldSize
	);

	//handleNextFrame is a frame step the submit path drives, so it demands the device lock is already held

	Test_assert(t, "frameNullDevice", !c::GraphicsDeviceRef_handleNextFrame(NULL, NULL, NULL));
	Test_assert(t, "frameNeedsLock", !c::GraphicsDeviceRef_handleNextFrame(deviceRef, NULL, NULL));

	//After submit and wait nothing is pending, so no flush runs and no command buffer is needed

	Test_assert(t, "nothingPending", !device->pendingResources.length);

	if (!device->pendingResources.length) {

		const c::ELockAcquire acq = c::SpinLock_lock(&device->lock, c::U64_MAX);

		Test_assert(t, "frameLocked", c::GraphicsDeviceRef_handleNextFrame(deviceRef, NULL, &t->err));
		Test_assert(t, "inFlightReleased", !device->resourcesInFlight[device->fifId].length);

		if(acq == c::ELockAcquire_Acquired)
			c::SpinLock_unlock(&device->lock);
	}
}

// -- 29. GPU execution -----------------------------------------------------------

//Everything before this only ever submitted empty command lists, so the backend replay of clears, copies, scope
// barriers and the initial data upload had never actually run on a device.
//Results can't be asserted yet because pullRegion (the GPU to CPU readback the docs describe) isn't implemented,
// so this verifies the recording executes and the device survives it, which is still the whole replay path.

static void Test_pullCallback(c::RefPtr *resource, void *context) {
	(void) resource;
	++*(c::U32*)context;
}

//Render target pulls hand over an owned buffer; the callback inspects it without taking ownership

typedef struct TestTexturePull {
	c::U32 count;
	c::U32 expected, matching;
	c::U64 len;
} TestTexturePull;

static void Test_texturePullCallback(c::RefPtr *resource, c::Buffer *data, void *context) {

	(void) resource;

	TestTexturePull *result = (TestTexturePull*) context;

	++result->count;
	result->len = data ? c::Buffer_length(*data) : 0;
	result->matching = 0;

	for(c::U64 i = 0; data && (i + 1) * 4 <= c::Buffer_length(*data); ++i)
		result->matching += ((const c::U32*)data->ptr)[i] == result->expected;
}

extern "C" void Test_graphicsGpuExecute(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/execute");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	const c::Allocator *alloc = c::Platform_instance->alloc;

	gfx::RenderTexture target;
	gfx::DeviceTexture texture;
	gfx::CommandList commandList;
	gfx::CommandList emptyList;

	const c::ImageRange all = { .levelId = c::U32_MAX, .layerId = c::U32_MAX };

	if(!Test_assert(t, "createTarget", dev.createRenderTexture(
		4, 4, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Execute target", target,
		c::EMSAASamples_Off, nullptr, e_rr
	)))
		return;

	//With initial data, so the first submit also runs the staging upload for real

	c::Buffer texData = c::Buffer_createNull();
	Test_assert(t, "texData", c::Buffer_createEmptyBytes(4 * 4 * 4, alloc, &texData, &t->err));

	Test_assert(t, "createTexture", dev.createTexture(
		c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_CPUBacked, 4, 4, 1,
		"Execute texture", &texData, texture, nullptr, e_rr
	));

	c::Buffer_free(&texData, alloc);

	//A CPU backed buffer with a known pattern, so the pull below can prove the bytes made a real GPU round trip

	gfx::DeviceBuffer pattern;

	c::U8 patternData[32];

	for(c::U8 i = 0; i < 32; ++i)
		patternData[i] = i;

	c::Buffer patternRef = c::Buffer_createRefConst(patternData, sizeof(patternData));

	Test_assert(t, "createPattern", dev.createBufferData(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_CPUBacked,
		"Execute pattern buffer", &patternRef, pattern, nullptr, e_rr
	));

	if(texture && Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr))) {

		Test_assert(t, "begin", commandList.begin(true, e_rr));

		//Clear in one scope, copy in the next, so the cross scope barrier is replayed too

		{
			gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "clearScope", (c::Bool) scope);

			Test_assert(t, "clear", scope.clearImagef(
				c::F32x4_create4(1, 0, 0, 1), all, target.handle(), e_rr
			));

			Test_assert(t, "endClearScope", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = commandList.scope({}, 2, {}, e_rr);
			Test_assert(t, "copyScope", (c::Bool) scope);

			Test_assert(t, "copy", scope.copyImage(target.handle(), texture.handle(), { 0 }, e_rr));

			Test_assert(t, "endCopyScope", scope.end(e_rr));
		}

		Test_assert(t, "end", commandList.end(e_rr));

		//Twice, so the second replay starts from state the first one left behind

		Test_assert(t, "submit", dev.submit({ &commandList }, {}, 0, 0, e_rr));
		Test_assert(t, "submitAgain", dev.submit({ &commandList }, {}, 0, 0, e_rr));
		Test_assert(t, "wait", dev.wait(e_rr));

		//Read the results back: the copied texture has to hold the clear color and the pattern buffer its bytes.
		//The buffer's cpuData is scribbled over first, so only a real GPU to CPU copy can make the assert pass.

		c::U32 pulled = 0;

		if (pattern) {

			c::DeviceBuffer *patternPtr = pattern.data();

			for(c::U8 i = 0; i < 32; ++i)
				patternPtr->cpuData.ptrNonConst[i] = 0xCC;

			Test_assert(t, "pullBuffer", pattern.pullRegion(0, 0, Test_pullCallback, &pulled, e_rr));
		}

		Test_assert(t, "pullTexture", texture.pullRegion(
			0, 0, 0, 0, 0, 0, Test_pullCallback, &pulled, e_rr
		));

		//Wrong type and out of bounds regions are refused before anything is queued.
		//Handing a render texture to the DeviceTexture entry point is the whole point of the first one, and the
		// handle layer routes each kind of texture to its own entry point, so that one stays on the C call.

		Test_assert(t, "pullWrongType", !c::DeviceTextureRef_pullRegion(
			target.handle(), 0, 0, 0, 0, 0, 0, NULL, NULL, NULL
		));

		Test_assert(t, "pullOOB", pattern && !pattern.pullRegion(64, 0, nullptr, nullptr, nullptr));
		Test_assert(t, "pullOOBRegion", !texture.pullRegion(1, 0, 0, 4, 0, 0, nullptr, nullptr, nullptr));

		//Nothing completes before the frame that carries the copies has provably finished

		Test_assert(t, "pullNotYet", !pulled);

		Test_assert(t, "submitPull", dev.submit({ &commandList }, {}, 0, 0, e_rr));

		//Growing the ring while pulls are in flight swaps the buffers; the pulls have to survive on the old one

		Test_assert(t, "reserveWhileInFlight", dev.reserveReadback(64 * c::KIBI, e_rr));

		Test_assert(t, "waitPull", dev.wait(e_rr));

		Test_assert(t, "pullCompleted", pulled == (pattern ? 2u : 1u));

		//Reserving less than what's already there never shrinks, and a NULL device is refused

		Test_assert(t, "reserveNoShrink", dev.reserveReadback(0, e_rr));
		Test_assert(t, "reserveInvalid", !c::GraphicsDeviceRef_reserveReadback(NULL, 0, NULL));

		//RGBA8 red is FF 00 00 FF in memory, 0xFF0000FF read as little endian

		const c::DeviceTexture *texPtr = texture.data();
		c::Bool allRed = c::Buffer_length(texPtr->cpuData) == 4 * 4 * 4;

		for(c::U64 i = 0; i < 16 && allRed; ++i)
			allRed &= ((const c::U32*)texPtr->cpuData.ptr)[i] == 0xFF0000FFu;

		Test_assert(t, "pixelsMatchClear", allRed);

		if (pattern) {

			const c::DeviceBuffer *patternPtr = pattern.data();
			c::Bool matches = true;

			for(c::U8 i = 0; i < 32; ++i)
				matches &= patternPtr->cpuData.ptr[i] == i;

			Test_assert(t, "patternSurvivedRoundTrip", matches);
		}

		//An op-less list, so upload and readback rounds don't replay the clear and copy above

		if(
			Test_assert(t, "createEmptyList", dev.createCommandList(4 * c::KIBI, 64, 16, emptyList, true, e_rr)) &&
			Test_assert(t, "beginEmptyList", emptyList.begin(true, e_rr)) &&
			Test_assert(t, "endEmptyList", emptyList.end(e_rr))
		) {

			//A partial upload followed by a partial pull, so the region row math is proven in both directions

			c::DeviceTexture *texPtr2 = texture.data();
			c::U32 pulledRegion = 0;

			for(c::U64 j = 0; j < 2; ++j)
				for(c::U64 i = 0; i < 2; ++i)
					((c::U32*)texPtr2->cpuData.ptrNonConst)[(1 + j) * 4 + (1 + i)] = 0xAABBCCDDu;

			Test_assert(t, "regionMarkDirty", texture.markDirty(1, 1, 0, 2, 2, 1, e_rr));

			Test_assert(t, "regionUploadSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

			Test_assert(t, "regionUploadWait", dev.wait(e_rr));

			for(c::U64 i = 0; i < 16; ++i)
				((c::U32*)texPtr2->cpuData.ptrNonConst)[i] = 0x5C5C5C5Cu;

			Test_assert(t, "regionPull", texture.pullRegion(
				1, 1, 0, 2, 2, 1, Test_pullCallback, &pulledRegion, e_rr
			));

			Test_assert(t, "regionPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

			Test_assert(t, "regionPullWait", dev.wait(e_rr));
			Test_assert(t, "regionPullCompleted", pulledRegion == 1);

			//The pulled region came back from the GPU while everything around it kept the scribble

			c::Bool regionOk = true;

			for(c::U64 j = 0; j < 4; ++j)
				for(c::U64 i = 0; i < 4; ++i) {

					const c::U32 got = ((const c::U32*)texPtr2->cpuData.ptr)[j * 4 + i];
					const c::Bool inRegion = i >= 1 && i < 3 && j >= 1 && j < 3;

					regionOk &= got == (inRegion ? 0xAABBCCDDu : 0x5C5C5C5Cu);
				}

			Test_assert(t, "regionRoundTrip", regionOk);

			//Compressed formats go through the same math in block rows, proven end to end with BC7

			if (dev.info().capabilities.dataTypes & c::EGraphicsDataTypes_BCn) {

				gfx::DeviceTexture bcTex;

				c::Buffer bcData = c::Buffer_createNull();
				Test_assert(t, "bcData", c::Buffer_createEmptyBytes(64, alloc, &bcData, &t->err));

				for(c::U8 i = 0; i < 64; ++i)
					bcData.ptrNonConst[i] = (c::U8)(i * 3);

				Test_assert(t, "bcCreate", dev.createTexture(
					c::ETextureType_2D, c::ETextureFormatId_BC7, c::EGraphicsResourceFlag_CPUBacked, 8, 8, 1,
					"Execute BC7 texture", &bcData, bcTex, nullptr, e_rr
				));

				c::Buffer_free(&bcData, alloc);

				if (bcTex) {

					c::U32 pulledBc = 0;
					c::DeviceTexture *bcPtr = bcTex.data();

					Test_assert(t, "bcUploadSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

					Test_assert(t, "bcUploadWait", dev.wait(e_rr));

					for(c::U8 i = 0; i < 64; ++i)
						bcPtr->cpuData.ptrNonConst[i] = 0xEE;

					Test_assert(t, "bcPull", bcTex.pullRegion(
						0, 0, 0, 0, 0, 0, Test_pullCallback, &pulledBc, e_rr
					));

					Test_assert(t, "bcPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

					Test_assert(t, "bcPullWait", dev.wait(e_rr));
					Test_assert(t, "bcPullCompleted", pulledBc == 1);

					c::Bool bcMatches = true;

					for(c::U8 i = 0; i < 64; ++i)
						bcMatches &= bcPtr->cpuData.ptr[i] == (c::U8)(i * 3);

					Test_assert(t, "bcRoundTrip", bcMatches);

					//One block from the middle of the block grid; an 8x8 BC7 is 2x2 blocks of 16 bytes

					for(c::U8 i = 0; i < 64; ++i)
						bcPtr->cpuData.ptrNonConst[i] = 0xEE;

					Test_assert(t, "bcPullBlock", bcTex.pullRegion(
						4, 4, 0, 4, 4, 1, Test_pullCallback, &pulledBc, e_rr
					));

					Test_assert(t, "bcPullBlockSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

					Test_assert(t, "bcPullBlockWait", dev.wait(e_rr));

					c::Bool bcBlock = true;

					for(c::U8 i = 0; i < 64; ++i)
						bcBlock &= bcPtr->cpuData.ptr[i] == (i >= 48 ? (c::U8)(i * 3) : 0xEE);

					Test_assert(t, "bcBlockRegion", bcBlock);
				}
			}

			else c::Test_print(t, "Device lacks BCn, skipping compressed readback tests");

			//Render targets have no cpuData; their pulls hand the region to the callback as an owned buffer.
			//The target still holds the clear color from the replay above, which is what proves the transport.

			TestTexturePull targetPull = { .expected = 0xFF0000FFu };

			Test_assert(t, "targetPull", target.pullRegion(
				0, 0, 0, 0, 0, 0, Test_texturePullCallback, &targetPull, e_rr
			));

			Test_assert(t, "targetPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

			Test_assert(t, "targetPullWait", dev.wait(e_rr));

			Test_assert(t, "targetPullCompleted", targetPull.count == 1);
			Test_assert(t, "targetPullLen", targetPull.len == 4 * 4 * 4);
			Test_assert(t, "targetPullRed", targetPull.matching == 16);

			//Depth has to be initialized before its first read (D3D12 demands a clear, discard or copy first),
			// so a draw less render pass clears it; that same pass also regression tests that a clear load
			// alone keeps its scope alive instead of being rewound as an empty scope

			gfx::DepthStencil depth;

			Test_assert(t, "depthCreate", dev.createDepthStencil(
				4, 4, c::EDepthStencilFormat_D32, false, "Execute depth", depth, c::EMSAASamples_Off, e_rr
			));

			if (depth && (dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {

				gfx::CommandList depthList;

				if (Test_assert(t, "depthListCreate", dev.createCommandList(
					4 * c::KIBI, 64, 16, depthList, true, e_rr
				))) {

					Test_assert(t, "depthListBegin", depthList.begin(true, e_rr));

					const c::DepthStencilAttachmentInfo depthAttach = {
						.image = depth.handle(),
						.depthLoad = c::ELoadAttachmentType_Clear,
						.clearDepth = 0.5f
					};

					{
						gfx::CommandScope depthScope = depthList.scope({}, 1, {}, e_rr);
						Test_assert(t, "depthScope", (c::Bool) depthScope);

						{
							gfx::CommandRender depthRender = depthScope.render(
								c::I32x2_zero, c::I32x2_create2(4, 4),
								{ { .image = target.handle(), .load = c::ELoadAttachmentType_Clear } },
								&depthAttach, e_rr
							);

							Test_assert(t, "depthRenderStart", (c::Bool) depthRender);
							Test_assert(t, "depthRenderEnd", depthRender.end(e_rr));
						}

						Test_assert(t, "depthScopeEnd", depthScope.end(e_rr));
					}

					//The clear only scope must survive endScope, or the clear silently never runs

					Test_assert(t, "depthScopeKept", depthList.data()->activeScopes.length == 1);

					Test_assert(t, "depthListEnd", depthList.end(e_rr));

					Test_assert(t, "depthClearSubmit", dev.submit({ &depthList }, {}, 0, 0, e_rr));

					Test_assert(t, "depthClearWait", dev.wait(e_rr));

					//A 0.5f clear reads back as its exact bit pattern in every D32 texel

					TestTexturePull depthPull = { .expected = 0x3F000000u };

					Test_assert(t, "depthPull", depth.pullRegion(
						0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &depthPull, e_rr
					));

					Test_assert(t, "depthPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

					Test_assert(t, "depthPullWait", dev.wait(e_rr));

					Test_assert(t, "depthPullCompleted", depthPull.count == 1);
					Test_assert(t, "depthPullLen", depthPull.len == 4 * 4 * 4);
					Test_assert(t, "depthPullHalf", depthPull.matching == 16);
				}
			}

			else c::Test_print(t, "Device lacks direct rendering (or depth), skipping depth pull test");

			//The stencil variant of the same clear + pull: a stencil bearing format pulls its STENCIL plane at
			// one byte per texel, so a 0xAB clear reads back as 16 bytes of 0xAB, which the U32 comparison sees as
			// four words of 0xABABABAB.
			//D32S8 rather than D24S8, since D3D12 suppresses D24S8 on some hardware by design.
			//Skipped rather than failed when absent, since neither combined format is required of an adapter.

			if (c::GraphicsDeviceInfo_supportsDepthStencilFormat(
				&dev.info(), c::EDepthStencilFormat_D32S8X24Ext
			) && (dev.info().capabilities.features & c::EGraphicsFeatures_DirectRendering)) {

				gfx::DepthStencil stencilTex;
				gfx::CommandList stencilList;

				Test_assert(t, "stencilCreate", dev.createDepthStencil(
					4, 4, c::EDepthStencilFormat_D32S8X24Ext, false, "Execute stencil", stencilTex,
					c::EMSAASamples_Off, e_rr
				));

				if (stencilTex && Test_assert(t, "stencilListCreate", dev.createCommandList(
					4 * c::KIBI, 64, 16, stencilList, true, e_rr
				))) {

					Test_assert(t, "stencilListBegin", stencilList.begin(true, e_rr));

					//Both planes are cleared, since leaving depth to a load would read uninitialized memory on the
					// APIs that demand a clear, discard or copy before the first use.

					const c::DepthStencilAttachmentInfo stencilAttach = {
						.image = stencilTex.handle(),
						.depthLoad = c::ELoadAttachmentType_Clear,
						.stencilLoad = c::ELoadAttachmentType_Clear,
						.clearDepth = 1,
						.clearStencil = 0xAB
					};

					{
						gfx::CommandScope stencilScope = stencilList.scope({}, 1, {}, e_rr);
						Test_assert(t, "stencilScope", (c::Bool) stencilScope);

						{
							gfx::CommandRender stencilRender = stencilScope.render(
								c::I32x2_zero, c::I32x2_create2(4, 4),
								{ { .image = target.handle(), .load = c::ELoadAttachmentType_Clear } },
								&stencilAttach, e_rr
							);

							Test_assert(t, "stencilRenderStart", (c::Bool) stencilRender);
							Test_assert(t, "stencilRenderEnd", stencilRender.end(e_rr));
						}

						Test_assert(t, "stencilScopeEnd", stencilScope.end(e_rr));
					}

					Test_assert(t, "stencilListEnd", stencilList.end(e_rr));

					Test_assert(t, "stencilClearSubmit", dev.submit({ &stencilList }, {}, 0, 0, e_rr));

					Test_assert(t, "stencilClearWait", dev.wait(e_rr));

					TestTexturePull stencilPull = { .expected = 0xABABABABu };

					Test_assert(t, "stencilPull", stencilTex.pullRegion(
						0, 0, 0, 0, 0, 0, 1, Test_texturePullCallback, &stencilPull, e_rr
					));

					Test_assert(t, "stencilPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

					Test_assert(t, "stencilPullWait", dev.wait(e_rr));

					Test_assert(t, "stencilPullCompleted", stencilPull.count == 1);
					Test_assert(t, "stencilPullLen", stencilPull.len == 4 * 4);
					Test_assert(t, "stencilPullValue", stencilPull.matching == 4);

					//The DEPTH plane of the same combined texture: the 1.0f clear reads back as its exact bit
					// pattern, proving both planes of one format are separately reachable.

					TestTexturePull depthPlanePull = { .expected = 0x3F800000u };

					Test_assert(t, "stencilDepthPull", stencilTex.pullRegion(
						0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &depthPlanePull, e_rr
					));

					Test_assert(t, "stencilDepthPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr));

					Test_assert(t, "stencilDepthPullWait", dev.wait(e_rr));

					Test_assert(t, "stencilDepthPullCompleted", depthPlanePull.count == 1);
					Test_assert(t, "stencilDepthPullLen", depthPlanePull.len == 4 * 4 * 4);
					Test_assert(t, "stencilDepthPullValue", depthPlanePull.matching == 16);
				}
			}

			else c::Test_print(t, "Device lacks D32S8 (or direct rendering), skipping stencil pull test");

			//The refusals: wrong type, missing callback and MSAA (which also can't be copied, only resolved).
			//The first two stay on the C entry point: a DeviceTexture arriving at TextureRef_pullRegion and a
			// plane 1 pull of a color target are exactly what the handle layer routes away from, so neither is
			// something a handle can express.

			Test_assert(t, "pullDeviceTexRefused", !c::TextureRef_pullRegion(
				texture.handle(), 0, 0, 0, 0, 0, 0, 0, Test_texturePullCallback, &targetPull, NULL
			));

			//A plane the format doesn't have is refused rather than silently pulled as plane 0: plane 1 only
			// exists on combined depth stencil formats, so a color target rejects it.

			Test_assert(t, "pullBadPlaneRefused", !c::TextureRef_pullRegion(
				target.handle(), 0, 0, 0, 0, 0, 0, 1, Test_texturePullCallback, &targetPull, NULL
			));

			Test_assert(t, "pullNoCallbackRefused", !target.pullRegion(0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr));

			gfx::RenderTexture msaaTarget;

			Test_assert(t, "msaaCreate", dev.createRenderTexture(
				4, 4, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "Execute MSAA target", msaaTarget,
				c::EMSAASamples_x4, nullptr, e_rr
			));

			if (msaaTarget) {

				Test_assert(t, "pullMsaaRefused", !msaaTarget.pullRegion(
					0, 0, 0, 0, 0, 0, Test_texturePullCallback, &targetPull, nullptr
				));

				//Copying MSAA is refused at record time now; the failed command invalidates the recording,
				// which is why end isn't asserted and the list isn't reused afterwards.
				//The scope closes on its way out of the block, which the C form left to the list teardown; an
				// endScope after an invalidated recording rewinds it and reports success either way.

				Test_assert(t, "msaaCopyBegin", emptyList.begin(true, e_rr));

				{
					gfx::CommandScope msaaScope = emptyList.scope({}, 1, {}, e_rr);
					Test_assert(t, "msaaCopyScope", (c::Bool) msaaScope);

					Test_assert(t, "copyMsaaRefused", !msaaScope.copyImage(
						msaaTarget.handle(), target.handle(), { 0 }, nullptr
					));
				}

				(void) emptyList.end(nullptr);
			}
		}
	}
}

// -- 30. Acceleration structures -------------------------------------------------

//BLAS and TLAS creation queue their build for the next submit, so submitting after recording the updates is what
// actually builds them on the device.
//Everything here needs the raytracing feature, so software adapters skip with a message rather than fail.

extern "C" void Test_graphicsAccelerationStructures(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "Raytracing/AS");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	if(!(dev.info().capabilities.features & c::EGraphicsFeatures_Raytracing)) {
		c::Test_print(t, "Device lacks raytracing, skipping acceleration structure tests");
		return;
	}

	gfx::DeviceBuffer positions;
	gfx::DeviceBuffer plain;
	gfx::Blas blas;
	gfx::Blas badBlas;
	gfx::Tlas tlas;
	gfx::CommandList commandList;

	//One triangle in RGBA32f, which every raytracing device accepts

	const c::F32 triangle[12] = {
		0, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1
	};

	c::Buffer triData = c::Buffer_createRefConst(triangle, sizeof(triangle));

	if(!Test_assert(t, "createPositions", dev.createBufferData(
		c::EDeviceBufferUsage_ASReadExt, c::EGraphicsResourceFlag_None, "AS positions", &triData, positions,
		nullptr, e_rr
	)))
		return;

	Test_assert(t, "createPlain", dev.createBuffer(
		c::EDeviceBufferUsage_Vertex, c::EGraphicsResourceFlag_None, "AS plain buffer", 48, plain, nullptr, e_rr
	));

	const c::DeviceData positionData = positions.region();

	//A buffer without ASReadExt usage can't feed an AS build, and a zero stride can't address vertices

	if (plain) {

		const c::BLASCreateInfo wrongUsage = c::BLASCreateInfo_unindexed(
			c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, plain.region()
		);

		Test_assert(t, "blasWrongUsage", !dev.createBlas(wrongUsage, "Test BLAS", badBlas, nullptr));
	}

	const c::BLASCreateInfo zeroStride = c::BLASCreateInfo_unindexed(
		c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 0, positionData
	);

	Test_assert(t, "blasZeroStride", !dev.createBlas(zeroStride, "Test BLAS", badBlas, nullptr));

	//An OMM index is per triangle, so asking for one without triangle indices has nothing to index against.
	//Rejected regardless of device support, since it is malformed rather than unsupported.

	const c::BLASCreateInfo ommWithoutIndices = c::BLASCreateInfo_indexedWithOmmIndicesExt(
		c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positionData,
		c::ETextureFormatId_Undefined, { 0 },
		c::ETextureFormatId_R16u, positionData
	);

	Test_assert(t, "blasOmmWithoutIndices", !dev.createBlas(ommWithoutIndices, "Test BLAS", badBlas, nullptr));

	//A micromap without an index buffer has nothing to link it to the triangles

	{
		c::BLASCreateInfo ommNoIndices = c::BLASCreateInfo_unindexed(
			c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positionData
		);

		ommNoIndices.ommMicromap = (c::OpacityMicromapRef*) positionData.buffer;        //Wrong type too, format first

		Test_assert(t, "blasOmmMicromapWithoutFormat", !dev.createBlas(ommNoIndices, "Test BLAS", badBlas, nullptr));
	}

	//An OMM index format of Undefined means no OMM at all, so carrying a buffer anyway is contradictory.

	c::BLASCreateInfo ommBufferNoFormat = c::BLASCreateInfo_unindexed(
		c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positionData
	);

	ommBufferNoFormat.ommIndexBuffer = positionData;

	Test_assert(t, "blasOmmBufferWithoutFormat", !dev.createBlas(ommBufferNoFormat, "Test BLAS", badBlas, nullptr));

	Test_assert(t, "blasRejectedNothing", !badBlas);

	//Opacity micromap create validation.
	//Every one of these must be refused, so the buffers can be any valid ASReadExt DeviceData; what is under
	// test is the create info, not the geometry.
	//A device without RayMicromapOpacity refuses all of them for that reason instead, which still passes but
	// proves nothing there.

	{
		gfx::OpacityMicromap badOmm;

		const c::OpacityMicromapUsage usage = {
			.count = 1, .subdivisionLevel = 1, .format = c::EOpacityMicromapFormat_Opacity2State
		};

		//Micromaps have no update mode at all, so an update flag is not merely ignored.

		c::OpacityMicromapCreateInfo badFlags = c::OpacityMicromapCreateInfo_uniform(
			c::ERTASBuildFlags_AllowUpdate, &positionData, &positionData, sizeof(c::OpacityMicromapEntry), &usage
		);

		Test_assert(t, "ommBadBuildFlags", !dev.createOpacityMicromap(badFlags, "Test BLAS", badOmm, nullptr));

		//A stride that cannot hold one record, and one that is not 4 byte aligned.

		c::OpacityMicromapCreateInfo shortStride = c::OpacityMicromapCreateInfo_uniform(
			c::ERTASBuildFlags_None, &positionData, &positionData, 4, &usage
		);

		Test_assert(t, "ommStrideTooSmall", !dev.createOpacityMicromap(shortStride, "Test BLAS", badOmm, nullptr));

		c::OpacityMicromapCreateInfo oddStride = c::OpacityMicromapCreateInfo_uniform(
			c::ERTASBuildFlags_None, &positionData, &positionData, 9, &usage
		);

		Test_assert(t, "ommStrideUnaligned", !dev.createOpacityMicromap(oddStride, "Test BLAS", badOmm, nullptr));

		//No usages at all: the entry count would be 0, so there is nothing to build.

		c::OpacityMicromapCreateInfo noUsages = c::OpacityMicromapCreateInfo_uniform(
			c::ERTASBuildFlags_None, &positionData, &positionData, sizeof(c::OpacityMicromapEntry), NULL
		);

		Test_assert(t, "ommNoUsages", !dev.createOpacityMicromap(noUsages, "Test BLAS", badOmm, nullptr));

		//A usage that describes zero entries, and one with a format neither API defines.

		const c::OpacityMicromapUsage zeroCount = { .count = 0, .subdivisionLevel = 1, .format = 1 };

		c::OpacityMicromapCreateInfo zeroUsage = c::OpacityMicromapCreateInfo_uniform(
			c::ERTASBuildFlags_None, &positionData, &positionData, sizeof(c::OpacityMicromapEntry), &zeroCount
		);

		Test_assert(t, "ommZeroCount", !dev.createOpacityMicromap(zeroUsage, "Test BLAS", badOmm, nullptr));

		const c::OpacityMicromapUsage badFormat = { .count = 1, .subdivisionLevel = 1, .format = 7 };

		c::OpacityMicromapCreateInfo badFormatInfo = c::OpacityMicromapCreateInfo_uniform(
			c::ERTASBuildFlags_None, &positionData, &positionData, sizeof(c::OpacityMicromapEntry), &badFormat
		);

		Test_assert(t, "ommBadFormat", !dev.createOpacityMicromap(badFormatInfo, "Test BLAS", badOmm, nullptr));

		//A null create info is not something a reference parameter can hold, so that one negative stays on the C
		// entry point, with a raw handle of its own for it to refuse into.

		const c::CharString ommName = c::CharString_createRefCStrConst("Test BLAS");
		c::OpacityMicromapRef *rawOmm = NULL;

		Test_assert(t, "ommNullInfo", !c::GraphicsDeviceRef_createOpacityMicromapExt(
			deviceRef, NULL, &ommName, &rawOmm, NULL
		));

		Test_assert(t, "ommRejectedNothing", !badOmm && !rawOmm);
	}

	const c::BLASCreateInfo blasInfo = c::BLASCreateInfo_unindexed(
		c::ERTASBuildFlags_None, c::EBLASFlag_None, c::ETextureFormatId_RGBA32f, 0, 16, positionData
	);

	if(!Test_assert(t, "createBlas", dev.createBlas(blasInfo, "Test BLAS", blas, e_rr)))
		return;

	Test_assert(t, "blasTypeId", blas.handle()->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_BLASExt);

	//One instance at identity, pointing at the BLAS just made

	const c::TLASInstance instance = {
		.transform = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } },
		.data = {
			.instanceId24_mask8 = 0xFFu << 24,
			.sbtOffset24_flags8 = (c::U32) c::ETLASInstanceFlag_Default << 24,
			.blasCpu = blas.handle()
		}
	};

	if(!Test_assert(t, "createTlas", dev.createTlas(
		c::ERTASBuildFlags_DefaultTLAS, &instance, 1, "Test TLAS", tlas, true, e_rr
	)))
		return;

	Test_assert(t, "tlasTypeId", tlas.handle()->refPtrType->typeId == (c::TypeId) c::EGraphicsTypeId_TLASExt);

	//Refit validation, which now lives on the setter rather than on create: a refit updates the TLAS that is
	// already there, so there is no second object to reject.
	//The TLAS above used DefaultTLAS, which is FastBuild only, so it never allowed updates.

	{
		//A null TLAS and a null instance list are neither of them expressible through a handle or a reference
		// parameter, so those two negatives keep the C entry point.

		c::ListTLASInstance instances {};
		c::ListTLASInstance_createRefConst(&instance, 1, &instances, NULL);

		Test_assert(t, "setInstancesNullTlas", !c::TLASRef_setInstancesExt(NULL, &instances, NULL));
		Test_assert(t, "setInstancesNullList", !c::TLASRef_setInstancesExt(tlas.handle(), NULL, NULL));
		Test_assert(t, "setInstancesWithoutAllowUpdate", !tlas.setInstances(&instance, 1, nullptr));

		//One that does allow updates, so the rest of the rules have something valid to run against.

		gfx::Tlas updatable;

		if (Test_assert(t, "createTlasUpdatable", dev.createTlas(
			(c::ERTASBuildFlags) (c::ERTASBuildFlags_DefaultTLAS | c::ERTASBuildFlags_AllowUpdate),
			&instance, 1, "Test TLAS", updatable, true, e_rr
		))) {

			//An update refits the structure that is there instead of sizing a new one, so the instance count is
			// fixed for the lifetime of the TLAS.

			const c::TLASInstance pairData[2] = { instance, instance };

			Test_assert(t, "setInstancesWrongLength", !updatable.setInstances(pairData, 2, nullptr));
			Test_assert(t, "setInstancesRejectedClean", !c::TLAS_hasFlag(updatable.data(), c::ETLASFlag_InstancesDirty));

			Test_assert(t, "setInstances", updatable.setInstances(&instance, 1, e_rr));
			Test_assert(t, "setInstancesDirty", c::TLAS_hasFlag(updatable.data(), c::ETLASFlag_InstancesDirty));
		}
	}

	//The CPU side instance plumbing hands back what went in

	c::TLASInstanceData roundTrip {};

	Test_assert(t, "instanceData", c::TLAS_getInstanceDataCpu(tlas.data(), 0, &roundTrip));
	Test_assert(t, "instanceBlas", roundTrip.blasCpu == blas.handle());
	Test_assert(t, "instanceOOB", !c::TLAS_getInstanceDataCpu(tlas.data(), 1, &roundTrip));

	//Recording the updates and submitting is what runs the actual builds on the device

	if(Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr))) {

		Test_assert(t, "begin", commandList.begin(true, e_rr));

		//The TLAS update transitions the BLASes it references, so it can't share a scope with the BLAS update;
		// the split also expresses the real dependency between the two builds.

		{
			gfx::CommandScope scope = commandList.scope({}, 1, {}, e_rr);
			Test_assert(t, "scope", (c::Bool) scope);
			Test_assert(t, "updateBlas", scope.updateBlas(blas, e_rr));
			Test_assert(t, "endScope", scope.end(e_rr));
		}

		{
			gfx::CommandScope scope = commandList.scope({}, 2, {}, e_rr);
			Test_assert(t, "scopeTlas", (c::Bool) scope);
			Test_assert(t, "updateTlas", scope.updateTlas(tlas, e_rr));
			Test_assert(t, "endScopeTlas", scope.end(e_rr));
		}

		Test_assert(t, "end", commandList.end(e_rr));

		Test_assert(t, "submit", dev.submit({ &commandList }, {}, 0, 0, e_rr));
		Test_assert(t, "wait", dev.wait(e_rr));
	}
}
