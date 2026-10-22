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

//graphics/test/interface/test_graphics_over_budget.cpp
//
//GraphicsDevice/overBudget: GraphicsDeviceRef_forbidOverBudgetFallback, on both backends.
//The forbids are counted: every take is given back by its own give back, a give back that was never taken does nothing and
// never forbids a later allocation.
//The budget is forced to one byte (GraphicsDevice::budgetOverride), so any new memory block is over it, and the buffer is
// bigger than half a block of either kind, so it needs a block of its own whatever the shared ones have free.
//Vulkan on a dedicated device: by default the block falls back to host memory, forbidden it stays in video memory.
//D3D12: by default the heap is created past the budget, forbidden the create is refused with out of memory and leaves the
// device as it was; on a dedicated device a render target and a depth stencil of 1024 x 1024 (committed resources of their
// own) are refused the same way.
//With the real budget a forbidden device still allocates, uploads and reads back: on D3D12 that runs every heap and
// dedicated resource through creation not resident and the residency request, which is the path an allocation takes when
// the OS is asked not to go past its budget.
//On D3D12 several forbidden allocations in a row on one device each wait for their own value of the device's residency
// fence.
//GraphicsDevice/overBudgetReal fills the device's real video memory budget on D3D12 to see the OS refuse the allocation
// that goes past it. It evicts other applications' memory if the OS does not refuse, so it only runs with
// OXC3_TEST_OVER_BUDGET set to 1 (see docs/graphics_api.md).

#include "graphics/graphics.hpp"
#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "types/base/constants.h"
	#include "types/base/atomic.h"
	#include "types/base/error.h"
	#include "graphics/generic/device_allocator.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/render_texture.h"
}}

using namespace oxc;

namespace {

	//Bigger than half a block of either kind

	c::U64 overBudgetBytes(const c::GraphicsDevice *device) noexcept {
		return (device->blockSizeGpu > device->blockSizeCpu ? device->blockSizeGpu : device->blockSizeCpu) / 2 + c::KIBI;
	}

	//The block of a buffer sits in video memory (bit 0 of its allocationTypeExt, on both backends)

	c::Bool overBudgetLocal(gfx::Device &dev, const gfx::DeviceBuffer &buffer) {
		const c::DeviceMemoryBlock &block = c::deviceOf(dev.handle())->allocator.blocks.ptr[buffer.data()->resource.blockId];
		return block.allocationTypeExt & 1;
	}

	c::I64 overBudgetHolds(gfx::Device &dev) {
		return c::AtomicI64_load(&c::deviceOf(dev.handle())->noOverBudgetFallback);
	}

	//A buffer that needs a block of its own, or false with the error

	c::Bool overBudgetCreate(gfx::Device &dev, const c::C8 *name, gfx::DeviceBuffer &out, c::Error *err) {
		return dev.createBuffer(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead, name,
			overBudgetBytes(c::deviceOf(dev.handle())), out, nullptr, err
		);
	}

	//A take or a give back of the forbid, which a graphics device always accepts

	void overBudgetForbid(c::Test *t, gfx::Device &dev, c::Bool forbid) {
		c::Test_assert(t, "forbidAccepted", dev.forbidOverBudgetFallback(forbid, &t->err));
	}

	c::U32 overBudgetWord(c::U64 i) noexcept {
		return (c::U32) (i * 2654435761u) ^ 0x0B5E55EDu;
	}
}

extern "C" void Test_graphicsOverBudget(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/overBudget");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::GraphicsDevice *device = c::deviceOf(deviceRef);
	const c::EGraphicsApi api = dev.api();
	const c::Bool dedicated = dev.info().type == c::EGraphicsDeviceType_Dedicated;

	//Counted: a second forbid survives one give back, the last give back allows the fallback again (the device is shared
	// with whoever else, and a caller that goes away gives its forbid back), and a give back that was never taken does not
	// forbid anything later

	//A null handle and one that is not a graphics device are refused with an error, and hold nothing

	for(c::Bool forbid : { true, false }) {

		c::Error nullErr = c::Error_none();
		c::Error typeErr = c::Error_none();

		c::Test_assert(
			t, "forbidNullRefused",
			!c::GraphicsDeviceRef_forbidOverBudgetFallback(nullptr, forbid, &nullErr) &&
			nullErr.genericError == c::EGenericError_NullPointer
		);

		c::Test_assert(
			t, "forbidWrongTypeRefused",
			!c::GraphicsDeviceRef_forbidOverBudgetFallback((c::GraphicsDeviceRef*) device->instance, forbid, &typeErr) &&
			typeErr.genericError == c::EGenericError_NullPointer
		);
	}

	c::Test_assert(t, "holdsStart", overBudgetHolds(dev) == 0);

	overBudgetForbid(t, dev, true);
	c::Test_assert(t, "holdsOne", overBudgetHolds(dev) == 1);

	overBudgetForbid(t, dev, true);
	c::Test_assert(t, "holdsTwo", overBudgetHolds(dev) == 2);

	overBudgetForbid(t, dev, false);
	c::Test_assert(t, "holdsOneLeft", overBudgetHolds(dev) == 1);

	overBudgetForbid(t, dev, false);
	c::Test_assert(t, "holdsNone", overBudgetHolds(dev) == 0);

	overBudgetForbid(t, dev, false);
	c::Test_assert(t, "holdsNeverNegative", overBudgetHolds(dev) == 0);

	overBudgetForbid(t, dev, true);
	c::Test_assert(t, "holdsAfterExtraGiveBack", overBudgetHolds(dev) == 1);

	overBudgetForbid(t, dev, false);
	c::Test_assert(t, "holdsEnd", overBudgetHolds(dev) == 0);

	//A forbid is a hold on the device, not on a resource: nothing was created above and nothing is owed

	const c::Bool vulkanLocal = api == c::EGraphicsApi_Vulkan && dedicated;
	const c::Bool direct3D = api == c::EGraphicsApi_Direct3D12;

	//Control: the real budget takes the buffer, in video memory where there is some

	{
		gfx::DeviceBuffer control;
		c::Error err = c::Error_none();

		if(c::Test_assert(t, "controlCreate", overBudgetCreate(dev, "overBudgetControl", control, &err)) && dedicated)
			c::Test_assert(t, "controlLocal", overBudgetLocal(dev, control));
	}

	if(vulkanLocal || direct3D) {

		device->budgetOverride = 1;

		//Allowed (the default): Vulkan falls back to host memory, D3D12 goes past the budget as it always did

		{
			gfx::DeviceBuffer allowed;
			c::Error err = c::Error_none();

			if(c::Test_assert(t, "allowedCreate", overBudgetCreate(dev, "overBudgetAllowed", allowed, &err)) && dedicated)
				c::Test_assert(t, "allowedPlacement", overBudgetLocal(dev, allowed) == direct3D);
		}

		//Forbidden: Vulkan keeps it in video memory, D3D12 refuses it and is as it was

		overBudgetForbid(t, dev, true);

		const c::I64 refs = c::AtomicI64_load(&deviceRef->refCount);
		const c::U64 blocks = device->allocator.blocks.length;

		for(c::U32 round = 0; round < 2; ++round) {

			//Round 0: one hold. Round 1: a second taken and one given back, so one is still held.

			if(round == 1) {
				overBudgetForbid(t, dev, true);
				overBudgetForbid(t, dev, false);
			}

			gfx::DeviceBuffer buffer;
			c::Error err = c::Error_none();
			const c::Bool created = overBudgetCreate(dev, "overBudgetForbidden", buffer, &err);

			if(direct3D) {

				c::Test_assert(t, "forbiddenRefused", !created && err.genericError == c::EGenericError_OutOfMemory);
				c::Test_assert(t, "forbiddenReference", c::AtomicI64_load(&deviceRef->refCount) == refs);
				c::Test_assert(t, "forbiddenBlocks", device->allocator.blocks.length == blocks);

				//A render target and a depth stencil of 512 x 512 or more on a dedicated device are committed resources of
				// their own (dx_texture.c), which are refused the same way before anything is set aside

				if(dedicated) {

					gfx::RenderTexture target;
					gfx::DepthStencil depth;
					c::Error targetErr = c::Error_none();
					c::Error depthErr = c::Error_none();

					const c::Bool targetMade = dev.createRenderTexture(
						1024, 1024, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "overBudgetTargetForbidden",
						target, c::EMSAASamples_Off, nullptr, &targetErr
					);

					const c::Bool depthMade = dev.createDepthStencil(
						1024, 1024, c::EDepthStencilFormat_D32, false, "overBudgetDepthForbidden", depth,
						c::EMSAASamples_Off, &depthErr
					);

					c::Test_assert(
						t, "forbiddenTargetRefused", !targetMade && targetErr.genericError == c::EGenericError_OutOfMemory
					);

					c::Test_assert(
						t, "forbiddenDepthRefused", !depthMade && depthErr.genericError == c::EGenericError_OutOfMemory
					);

					c::Test_assert(t, "forbiddenTextureReference", c::AtomicI64_load(&deviceRef->refCount) == refs);
					c::Test_assert(t, "forbiddenTextureBlocks", device->allocator.blocks.length == blocks);
				}
			}

			else if(c::Test_assert(t, "forbiddenCreate", created))
				c::Test_assert(t, "forbiddenStaysLocal", overBudgetLocal(dev, buffer));
		}

		//Given back (a take after an extra give back included): the fallback is allowed again

		overBudgetForbid(t, dev, false);
		overBudgetForbid(t, dev, false);
		overBudgetForbid(t, dev, true);

		{
			gfx::DeviceBuffer buffer;
			c::Error err = c::Error_none();
			const c::Bool created = overBudgetCreate(dev, "overBudgetExtra", buffer, &err);

			if(direct3D)
				c::Test_assert(t, "extraGiveBackRefused", !created && err.genericError == c::EGenericError_OutOfMemory);

			else if(c::Test_assert(t, "extraGiveBackCreate", created))
				c::Test_assert(t, "extraGiveBackLocal", overBudgetLocal(dev, buffer));
		}

		overBudgetForbid(t, dev, false);

		{
			gfx::DeviceBuffer buffer;
			c::Error err = c::Error_none();

			if(c::Test_assert(t, "givenBackCreate", overBudgetCreate(dev, "overBudgetGivenBack", buffer, &err)) && dedicated)
				c::Test_assert(t, "givenBackPlacement", overBudgetLocal(dev, buffer) == direct3D);
		}

		device->budgetOverride = 0;

		//The refusals left nothing behind: the buffer fits again with the real budget, forbidden or not

		overBudgetForbid(t, dev, true);

		{
			gfx::DeviceBuffer buffer;
			c::Error err = c::Error_none();
			c::Test_assert(t, "afterRefusalCreate", overBudgetCreate(dev, "overBudgetAfter", buffer, &err));
		}

		overBudgetForbid(t, dev, false);
	}

	else c::Test_print(t, "Neither a dedicated Vulkan device nor D3D12: only the counting is checked");

	c::Test_assert(t, "holdsBalanced", overBudgetHolds(dev) == 0);

	//The real budget with the forbid on: the allocations that were refused above go through creation not resident and the
	// residency request here, and what they made is usable. Three buffers are filled through createBufferData and read
	// back at their end by the GPU: one that needs a block of its own, a CPU sided one of the same size and a HostMemoryRead
	// one (host memory is a custom heap, a committed resource of its own on D3D12). A render texture of 1024 x 1024 takes
	// the dedicated resource path on a dedicated device.

	overBudgetForbid(t, dev, true);

	{
		struct Source {
			const c::C8 *readName;
			c::EGraphicsResourceFlag flags;
			c::U64 bytes;
			gfx::DeviceBuffer buffer, output;
		};

		const c::U64 bytes = overBudgetBytes(device);
		const c::U64 hostBytes = (c::U64) 4 << 20;
		const c::EGraphicsResourceFlag cpuFlags =
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderReadBindless | c::EGraphicsResourceFlag_CPUAllocatedBit);

		const c::EGraphicsResourceFlag hostFlags =
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderReadBindless | c::EGraphicsResourceFlag_HostMemoryRead);

		Source sources[3] = {
			{ "forbiddenOwnRead", c::EGraphicsResourceFlag_ShaderReadBindless, bytes, {}, {} },
			{ "forbiddenCpuRead", cpuFlags, bytes, {}, {} },
			{ "forbiddenHostRead", hostFlags, hostBytes, {}, {} }
		};

		const c::EGraphicsResourceFlag outputFlags =
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked);

		constexpr c::U64 window = (c::U64) 16 << 10;

		c::Buffer data = c::Buffer_createNull();
		gfx::RenderTexture target;
		gfx::CommandList commandList, emptyList;
		gfx::Pipeline pipeline;
		gfx::PipelineLayout layout;
		gfxtest::OwnedSHFile file(dev.alloc());
		c::Error *e_rr = &t->err;
		c::Bool created = true;

		const c::Bool shader = dev.hasBindlessTable() && gfxtest::loadFile(
			t, "//OxC3_gtest/test_shaders/test_read_bindless.oiSH", file.list
		);

		if(c::Test_assert(t, "forbiddenData", c::Buffer_createUninitializedBytes(bytes, dev.alloc(), &data, e_rr))) {

			c::U32 *words = (c::U32*) data.ptrNonConst;

			for(c::U64 i = 0; i < bytes / 4; ++i)
				words[i] = overBudgetWord(i);

			//Referenced, so each create copies the bytes it is given

			for(Source &source : sources) {

				c::Buffer ref = c::Buffer_createRefConst(data.ptr, source.bytes);

				created &= c::Test_assert(t, "forbiddenCreate", dev.createBufferData(
					c::EDeviceBufferUsage_None, source.flags, "overBudget source", &ref, source.buffer, nullptr, e_rr
				));

				created &= c::Test_assert(t, "forbiddenOutput", dev.createBuffer(
					c::EDeviceBufferUsage_None, outputFlags, "overBudget output", window, source.output, nullptr, e_rr
				));
			}

			c::Buffer_free(&data, dev.alloc());
		}

		else created = false;

		created &= c::Test_assert(t, "forbiddenTarget", dev.createRenderTexture(
			1024, 1024, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_None, "overBudget target", target,
			c::EMSAASamples_Off, nullptr, e_rr
		));

		if(
			created && shader && gfxtest::computePipelinePush(t, dev, file.list, pipeline, layout) &&
			c::Test_assert(t, "forbiddenList", dev.createCommandList(4 * c::KIBI, 32, 8, commandList, true, e_rr)) &&
			c::Test_assert(t, "forbiddenEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)) &&
			c::Test_assert(t, "forbiddenEmptyBegin", emptyList.begin(true, e_rr)) &&
			c::Test_assert(t, "forbiddenEmptyEnd", emptyList.end(e_rr)) &&
			c::Test_assert(t, "forbiddenBegin", commandList.begin(true, e_rr))
		) {

			//The end of each buffer: the last words are where a heap that is not all resident would show first

			for(c::U32 i = 0; i < 3; ++i) {

				const c::U64 offset = sources[i].bytes / 4 * 4 - window;

				const c::U32 push[4] = {
					sources[i].buffer.readHandle(), sources[i].output.writeHandle(), (c::U32) offset, 0
				};

				gfx::CommandScope scope = commandList.scope(
					{
						c::Transition{ .resource = sources[i].buffer.handle(), .stage = c::EPipelineStage_Compute },
						c::Transition{
							.resource = sources[i].output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true
						}
					},
					i + 1, {}, e_rr
				);

				c::Test_assert(t, "forbiddenScope", (c::Bool) scope);
				c::Test_assert(t, "forbiddenPipeline", scope.setComputePipeline(pipeline, e_rr));
				c::Test_assert(t, "forbiddenPush", scope.setPushConstants(push, e_rr));
				c::Test_assert(t, "forbiddenDispatch", scope.dispatch1D((c::U32) (window / 4 / 64), e_rr));
				c::Test_assert(t, "forbiddenScopeEnd", scope.end(e_rr));
			}

			if(c::Test_assert(t, "forbiddenEnd", commandList.end(e_rr)) && gfxtest::submitAndWait(t, dev, commandList))
				for(Source &source : sources) {

					if(!gfxtest::pullBuffer(t, dev, emptyList, source.output))
						break;

					const c::U64 offset = source.bytes / 4 * 4 - window;
					const c::U32 *read = (const c::U32*) source.output.data()->cpuData.ptr;
					c::Bool equal = true;

					for(c::U64 j = 0; j < window / 4; ++j)
						equal &= read[j] == overBudgetWord(offset / 4 + j);

					c::Test_assert(t, source.readName, equal);
				}
		}
	}

	//Several in a row on one device: host memory is a committed resource of its own on D3D12, so each buffer is made
	// resident with the next value of the device's residency fence, and each create returns once that value is reached

	{
		gfx::DeviceBuffer row[8];

		const c::EGraphicsResourceFlag rowFlags =
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderRead | c::EGraphicsResourceFlag_HostMemoryRead);

		for(gfx::DeviceBuffer &buffer : row)
			c::Test_assert(t, "forbiddenInARow", dev.createBuffer(
				c::EDeviceBufferUsage_None, rowFlags, "overBudget row", 64 * c::KIBI, buffer, nullptr, &t->err
			));
	}

	overBudgetForbid(t, dev, false);
	c::Test_assert(t, "holdsReleased", overBudgetHolds(dev) == 0);
}

//The OS refuses an allocation past its budget: video memory is filled in chunks until one is refused, which has to be an
// out of memory error that leaves the usage within the budget, since a refusal that came after the budget was passed
// would have evicted what other processes own.
//Only D3D12 refuses: Vulkan allocates past the budget when it is forbidden to fall back, which is what it is for.
//Opt in, because a driver that does not refuse takes the whole budget and the OS pages other applications' memory out for it.

extern "C" void Test_graphicsOverBudgetReal(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/overBudgetReal");

	c::Bool enabled = false;
	c::Platform_getEnvBool(c::CharString_createRefCStrConst("OXC3_TEST_OVER_BUDGET"), t->alloc, &enabled, nullptr);

	if(!enabled) {
		c::Test_print(t, "OXC3_TEST_OVER_BUDGET is unset, skipping the leg that fills the video memory budget");
		return;
	}

	gfx::Device dev = gfx::Device::share(deviceRef);

	if(dev.api() != c::EGraphicsApi_Direct3D12 || dev.info().type != c::EGraphicsDeviceType_Dedicated) {
		c::Test_print(t, "Only a dedicated D3D12 device refuses an allocation past the budget, skipping");
		return;
	}

	const c::U64 limit = dev.getMemoryLimit(true);

	if(!c::Test_assert(t, "limitKnown", limit && limit != c::U64_MAX))
		return;

	const c::U64 chunk = (c::U64) 256 << 20;
	const c::U64 maxChunks = limit / chunk + 4;

	gfx::DeviceBuffer chunks[96];
	c::U64 count = 0;
	c::Bool refused = false;

	//The device's own check against the budget is out of the way (an unlimited budget), so the refusal comes from the OS

	c::deviceOf(deviceRef)->budgetOverride = c::U64_MAX / 2;
	overBudgetForbid(t, dev, true);

	for(; count < maxChunks && count < 96; ++count) {

		c::Error err = c::Error_none();

		if(dev.createBuffer(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead, "overBudget chunk", chunk, chunks[count],
			nullptr, &err
		))
			continue;

		//Sub id 1 is DxGraphicsDevice_makeResident, where the OS refused; the device's own refusals (the budget it knows, the
		// total memory) are sub id 0

		c::Test_assert(t, "refusedOutOfMemory", err.genericError == c::EGenericError_OutOfMemory);
		c::Test_assert(t, "refusedByTheOs", err.errorSubId == 1);
		refused = true;
		break;
	}

	c::deviceOf(deviceRef)->budgetOverride = 0;

	const c::U64 used = dev.getMemoryBudget(true);
	const c::U64 limitAfter = dev.getMemoryLimit(true);

	overBudgetForbid(t, dev, false);

	c::Test_assert(t, "refusedBeforeTheEnd", refused);
	c::Test_assert(t, "usageWithinBudget", used != c::U64_MAX && limitAfter != c::U64_MAX && used <= limitAfter);
	c::Test_print(t, "Filled the video memory budget with the allocations it took before one was refused");
}
