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

//graphics/test/interface/test_graphics_host_memory.cpp
//
//GraphicsDevice/hostMemory: DeviceBuffers with EGraphicsResourceFlag_HostMemory, host memory the device reads over the
// bus without a CPU copy.
//A small one (below the dedicated threshold of 64 MiB) and a large one are filled through createBufferData with a
// pattern; after the next submit neither keeps cpuData, and test_read_bindless.hlsl reads 64 KiB windows of them (at the
// start, the middle and the end) through their bindless read handles into CPUBacked outputs, which must hold the
// pattern word for word.
//On a dedicated device the large buffer's bytes land in the process's shared memory usage and not in its video memory
// usage (GraphicsDeviceRef_getMemoryBudget), which is what keeps it out of video memory where ReBAR would map it there.
//HostMemoryRead (HostMemory with HostMemoryReadBit) is the same memory with CPU cached pages that
// DeviceBufferRef_hostMemory hands to the CPU: a 4 MiB one is read by the CPU word for word and by the device through
// its window, and one created without data is filled through DeviceBufferRef_hostMemoryWritable and read by both.
//Both kinds are also filled from a stream through createBufferStream, which releases the stream after the upload; the
// device reads the bytes the stream held.
//Refused: the bit without CPUAllocatedBit, with CPUBacked, CPUReadBit, ShaderWrite or ExposeBindlessWrite, with a usage,
// on a DeviceTexture and through createBuffer (no data to fill it), keepSource of a stream source; and markDirty after
// the upload, since it keeps no CPU copy and no source.
//GraphicsDevice/hostPlacement: where the memory of a HostMemory and a HostMemoryRead buffer lives.

#include "graphics/graphics.hpp"
#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "types/base/constants.h"
	#include "types/container/buffer.h"
	#include "types/container/memory_stream.h"
	#include "types/container/ref_ptr.h"
	#include "graphics/generic/device_allocator.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_texture.h"
}}

using namespace oxc;

namespace {

	constexpr c::U64 hostSmallBytes = (c::U64) 256 << 10;
	constexpr c::U64 hostLargeBytes = (c::U64) 80 << 20;
	constexpr c::U64 hostWindowBytes = (c::U64) 64 << 10;
	constexpr c::U64 hostReadBytes = (c::U64) 4 << 20;
	constexpr c::U32 hostSmallSeed = 0x13572468, hostLargeSeed = 0x2468ACE0, hostReadSeed = 0x0BADF00D;
	constexpr c::U32 hostWrittenSeed = 0x5EED1234;
	constexpr c::U32 hostStreamSeed = 0x7A11FACE, hostStreamReadSeed = 0x0DDBA11;

	//Word i of the buffer with this seed

	c::U32 hostWord(c::U32 seed, c::U64 i) noexcept {
		return (c::U32) (i * 2654435761u) ^ seed;
	}

	//The text of the error holds needle

	c::Bool hostErrorHas(const c::Error &err, const c::C8 *needle) noexcept {

		if(!err.errorStr)
			return false;

		for (c::U64 i = 0; err.errorStr[i]; ++i) {

			c::U64 j = 0;

			while(needle[j] && err.errorStr[i + j] == needle[j])
				++j;

			if(!needle[j])
				return true;
		}

		return false;
	}

	//A buffer with these flags and usage and its data is refused, naming HostMemoryBit

	void hostRefused(
		c::Test *t, gfx::Device &dev, const c::C8 *name, c::EDeviceBufferUsage usage, c::EGraphicsResourceFlag flags
	) {

		static const c::U32 words[64] = {};
		c::Buffer data = c::Buffer_createRefConst(words, sizeof(words));
		gfx::DeviceBuffer refused;
		c::Error err = c::Error_none();
		const c::Bool created = dev.createBufferData(usage, flags, name, &data, refused, nullptr, &err);

		c::Test_assert(t, name, !created && hostErrorHas(err, "HostMemoryBit"));
	}

	//A HostMemory buffer of bytes filled with the pattern of seed, its data moved in

	c::Bool hostCreate(
		c::Test *t, gfx::Device &dev, c::U64 bytes, c::U32 seed, const c::C8 *name, gfx::DeviceBuffer &out,
		c::EGraphicsResourceFlag memory = c::EGraphicsResourceFlag_HostMemory
	) {

		c::Buffer data = c::Buffer_createNull();

		if(!c::Test_assert(t, "hostData", c::Buffer_createUninitializedBytes(bytes, dev.alloc(), &data, &t->err)))
			return false;

		c::U32 *words = (c::U32*) data.ptrNonConst;

		for(c::U64 i = 0; i < bytes / 4; ++i)
			words[i] = hostWord(seed, i);

		//Bindful on a device without a bindless table, which doesn't change where the memory lives

		const c::EGraphicsResourceFlag shaderRead =
			dev.hasBindlessTable() ? c::EGraphicsResourceFlag_ShaderReadBindless : c::EGraphicsResourceFlag_ShaderRead;

		const c::Bool created = dev.createBufferData(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (memory | shaderRead),
			name, &data, out, nullptr, &t->err
		);

		c::Buffer_free(&data, dev.alloc());
		return c::Test_assert(t, name, created);
	}

	//A memory stream of bytes filled with the pattern of seed, which owns them. The caller drops its reference once a
	// buffer has taken its own; the type has to outlive the stream.

	c::Bool hostStream(
		c::Test *t, gfx::Device &dev, c::U64 bytes, c::U32 seed, const c::RefPtrType *type, c::StreamRef *&stream
	) {

		c::Buffer data = c::Buffer_createNull();

		if(!c::Test_assert(t, "hostStreamData", c::Buffer_createUninitializedBytes(bytes, dev.alloc(), &data, &t->err)))
			return false;

		c::U32 *words = (c::U32*) data.ptrNonConst;

		for(c::U64 i = 0; i < bytes / 4; ++i)
			words[i] = hostWord(seed, i);

		c::MemoryStreamRef *memory = nullptr;
		const c::Bool created = c::MemoryStream_createFromBuffer(&data, c::EMemoryStreamFlags_None, type, &memory, &t->err);

		if(created)
			stream = (c::StreamRef*) memory;

		else c::Buffer_free(&data, dev.alloc());

		return c::Test_assert(t, "hostStream", created);
	}

	//A 64 KiB window of a buffer the read shader copies out, and the seed of the buffer's pattern

	struct HostWindow {
		const gfx::DeviceBuffer *buffer;
		c::U32 seed;
		c::U64 offset;
	};

	//The block a buffer's memory lives in

	const c::DeviceMemoryBlock &hostBlock(gfx::Device &dev, const gfx::DeviceBuffer &buffer) {
		return c::deviceOf(dev.handle())->allocator.blocks.ptr[buffer.data()->resource.blockId];
	}
}

extern "C" void Test_graphicsHostMemory(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/hostMemory");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	//The refusals, which need no bindless table.
	//Without one, exposing a buffer to it is refused before the host memory rules are checked, so these are bindful there.

	const c::Bool bindless = dev.hasBindlessTable();

	const c::EGraphicsResourceFlag shaderRead =
		bindless ? c::EGraphicsResourceFlag_ShaderReadBindless : c::EGraphicsResourceFlag_ShaderRead;

	const c::EGraphicsResourceFlag host =
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_HostMemory | shaderRead);

	hostRefused(
		t, dev, "hostBitAlone", c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_HostMemoryBit | shaderRead)
	);

	//The CPU read bit alone names HostMemoryBit as what it needs

	hostRefused(
		t, dev, "hostReadBitAlone", c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_HostMemoryReadBit | shaderRead)
	);

	hostRefused(
		t, dev, "hostCPUBacked", c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (host | c::EGraphicsResourceFlag_CPUBacked)
	);

	hostRefused(
		t, dev, "hostCPURead", c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (host | c::EGraphicsResourceFlag_CPUReadBit)
	);

	hostRefused(
		t, dev, "hostShaderWrite", c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_HostMemory | c::EGraphicsResourceFlag_ShaderRWBindful)
	);

	if(bindless)
		hostRefused(
			t, dev, "hostBindlessWrite", c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (host | c::EGraphicsResourceFlag_ExposeBindlessWrite)
		);

	else c::Test_print(t, "Device has no bindless descriptor table, skipping the ExposeBindlessWrite refusal");

	hostRefused(t, dev, "hostUsage", c::EDeviceBufferUsage_Vertex, host);

	//Without its data nothing could fill it

	{
		gfx::DeviceBuffer noData;
		c::Error err = c::Error_none();
		const c::Bool created =
			dev.createBuffer(c::EDeviceBufferUsage_None, host, "hostNoData", hostSmallBytes, noData, nullptr, &err);
		c::Test_assert(t, "hostNoData", !created && hostErrorHas(err, "createBufferData"));
	}

	{
		c::U32 texels[16] = {};
		c::Buffer texelRef = c::Buffer_createRefConst(texels, sizeof(texels));
		gfx::DeviceTexture texture;
		c::Error err = c::Error_none();

		const c::Bool created = dev.createTexture(
			c::ETextureType_2D, c::ETextureFormatId_RGBA8,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_HostMemory | c::EGraphicsResourceFlag_ShaderRead),
			4, 4, 1, "hostTexture", &texelRef, texture, nullptr, &err
		);

		c::Test_assert(t, "hostTexture", !created && hostErrorHas(err, "HostMemoryBit"));
	}

	//A stream source is filled in at the first upload and then released, so keepSource, which would hold it for later
	// ranges, is refused for either kind and takes no reference on the stream

	const c::RefPtrType streamType = c::MemoryStream_makeType(dev.alloc());

	{
		c::StreamRef *source = nullptr;

		if(hostStream(t, dev, hostSmallBytes, hostStreamSeed, &streamType, source)) {

			const c::EGraphicsResourceFlag kinds[2] = {
				host, (c::EGraphicsResourceFlag) (host | c::EGraphicsResourceFlag_HostMemoryRead)
			};

			for(const c::EGraphicsResourceFlag kind : kinds) {

				gfx::DeviceBuffer kept;
				c::Error err = c::Error_none();
				const c::I64 sourceRefs = c::AtomicI64_load(&source->refCount);

				const c::Bool created = dev.createBufferStream(
					c::EDeviceBufferUsage_None, kind, "hostKeepSource", source, hostSmallBytes, kept, true, nullptr, &err
				);

				c::Test_assert(t, "hostKeepSourceRefused", !created && hostErrorHas(err, "keepSource"));
				c::Test_assert(t, "hostKeepSourceNoReference", c::AtomicI64_load(&source->refCount) == sourceRefs);
			}

			c::RefPtr_dec((c::RefPtr**) &source);
		}
	}

	//A refused create gives back what it took: the device is referenced once per buffer that exists

	{
		const c::I64 before = c::AtomicI64_load(&deviceRef->refCount);
		hostRefused(t, dev, "hostRefusedReference", c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_HostMemoryBit);
		c::Test_assert(t, "hostRefusedReferenceKept", c::AtomicI64_load(&deviceRef->refCount) == before);
	}

	if (!dev.hasBindlessTable()) {
		c::Test_print(t, "Device has no bindless descriptor table, skipping the host memory reads");
		return;
	}

	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_read_bindless.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping the host memory reads");
		return;
	}

	gfx::Pipeline pipeline;
	gfx::PipelineLayout layout;

	if(!gfxtest::computePipelinePush(t, dev, file.list, pipeline, layout))
		return;

	gfx::CommandList commandList, emptyList;

	if (
		!c::Test_assert(t, "listCreate", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr)) ||
		!c::Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)) ||
		!c::Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr)) ||
		!c::Test_assert(t, "endEmpty", emptyList.end(e_rr))
	)
		return;

	//Both buffers, and what the process uses of either memory before and after the large one landed

	const c::Bool dedicated = dev.info().type == c::EGraphicsDeviceType_Dedicated;
	const c::U64 localBefore = dev.getMemoryBudget(true), sharedBefore = dev.getMemoryBudget(false);

	gfx::DeviceBuffer small, large, readable, streamed, streamedRead;

	//Two buffers filled from a stream, the way a model's weights come from a file: the stream is read into the host
	// memory at the upload and released, so each holds no source after it

	{
		c::StreamRef *plain = nullptr, *cached = nullptr;

		const c::Bool madeStreams =
			hostStream(t, dev, hostSmallBytes, hostStreamSeed, &streamType, plain) &&
			hostStream(t, dev, hostSmallBytes, hostStreamReadSeed, &streamType, cached);

		const c::Bool madeBuffers = madeStreams && c::Test_assert(t, "hostStreamedCreate", dev.createBufferStream(
			c::EDeviceBufferUsage_None, host, "hostStreamed", plain, hostSmallBytes, streamed, false, nullptr, e_rr
		)) && c::Test_assert(t, "hostStreamedReadCreate", dev.createBufferStream(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (host | c::EGraphicsResourceFlag_HostMemoryRead),
			"hostStreamedRead", cached, hostSmallBytes, streamedRead, false, nullptr, e_rr
		));

		if(plain)
			c::RefPtr_dec((c::RefPtr**) &plain);

		if(cached)
			c::RefPtr_dec((c::RefPtr**) &cached);

		if(!madeBuffers)
			return;
	}

	if (
		!hostCreate(t, dev, hostSmallBytes, hostSmallSeed, "hostSmall", small) ||
		!hostCreate(
			t, dev, hostReadBytes, hostReadSeed, "hostReadable", readable, c::EGraphicsResourceFlag_HostMemoryRead
		) ||
		!hostCreate(t, dev, hostLargeBytes, hostLargeSeed, "hostLarge", large) ||
		!gfxtest::submitAndWait(t, dev, emptyList)
	)
		return;

	//A HostMemoryRead buffer created without data, whose owner writes it through the writable view: the CPU reads its writes
	// back and a shader reads them (below); a plain HostMemory buffer is still refused without data and has no
	// writable view

	gfx::DeviceBuffer written;

	if(!c::Test_assert(t, "hostWrittenCreate", dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_HostMemoryRead | c::EGraphicsResourceFlag_ShaderReadBindless),
		"hostWritten", hostReadBytes, written, nullptr, e_rr
	)))
		return;

	//Memory the CPU can only read uncached (Vulkan without a host cached type) has no view for the CPU

	const c::Bool cachedRead = !readable.data()->resource.hostUncached;

	//A dedicated adapter has host cached and coherent system memory on both backends, so its buffer has the view

	if(dedicated)
		c::Test_assert(t, "hostReadViewOnDedicated", cachedRead && readable.hostMemory());

	c::Test_assert(t, "hostWrittenUncachedMatches", !written.data()->resource.hostUncached == cachedRead);

	if (cachedRead) {

		c::U32 *words = (c::U32*) written.hostMemoryWritable();
		c::Bool equal = !!words;

		for(c::U64 i = 0; words && i < hostReadBytes / 4; ++i)
			words[i] = hostWord(hostWrittenSeed, i);

		const c::U32 *back = (const c::U32*) written.hostMemory();
		equal &= !!back;

		for(c::U64 i = 0; back && i < hostReadBytes / 4; ++i)
			equal &= back[i] == hostWord(hostWrittenSeed, i);

		c::Test_assert(t, "hostWrittenCpuRead", equal);
	}

	else {
		c::Test_assert(t, "hostUncachedNoView", !written.hostMemory() && !written.hostMemoryWritable());
		c::Test_print(t, "No host cached memory type: the CPU views are absent, skipping the CPU reads");
	}

	{
		c::Test_assert(t, "hostWrittenNoCopy", !written.data()->cpuData.ptr);

		gfx::DeviceBuffer refused;

		c::Test_assert(t, "hostPlainWithoutDataRefused", !dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_HostMemory | c::EGraphicsResourceFlag_ShaderReadBindless),
			"hostPlain", hostReadBytes, refused, nullptr, nullptr
		));

		c::Test_assert(t, "hostPlainNoWritable", !small.hostMemoryWritable());
		c::Test_assert(t, "hostNullNoWritable", !c::DeviceBufferRef_hostMemoryWritable(nullptr));
	}

	const c::U64 localAfter = dev.getMemoryBudget(true), sharedAfter = dev.getMemoryBudget(false);

	c::Test_assert(t, "hostSmallNoCopy", !small.data()->cpuData.ptr);
	c::Test_assert(t, "hostLargeNoCopy", !large.data()->cpuData.ptr);

	const c::Bool budgets =
		localBefore != c::U64_MAX && localAfter != c::U64_MAX && sharedBefore != c::U64_MAX && sharedAfter != c::U64_MAX;

	if (dedicated && budgets) {
		c::Test_assert(t, "hostLargeInSharedMemory", sharedAfter >= sharedBefore + hostLargeBytes);
		c::Test_assert(t, "hostLargeNotInVideoMemory", localAfter < localBefore + hostLargeBytes / 4);
	}

	else c::Test_print(t, "Not a dedicated device or no memory budget: the placement checks are skipped");

	//HostMemoryRead: the CPU reads the bytes through the accessor, which a plain host buffer does not give

	if (cachedRead) {

		const c::U32 *words = (const c::U32*) readable.hostMemory();
		c::Bool equal = !!words;

		for(c::U64 i = 0; words && i < hostReadBytes / 4; ++i)
			equal &= words[i] == hostWord(hostReadSeed, i);

		c::Test_assert(t, "hostReadableCpuRead", equal);
	}

	c::Test_assert(t, "hostReadableNoCopy", !readable.data()->cpuData.ptr);
	c::Test_assert(t, "hostPlainNoAccessor", !small.hostMemory());
	c::Test_assert(t, "hostNullNoAccessor", !c::DeviceBufferRef_hostMemory(nullptr));

	//It keeps no CPU copy, so nothing can mark it dirty after the upload

	c::Test_assert(t, "hostMarkDirtyRefused", !small.markDirty(0, 0, nullptr));
	c::Test_assert(t, "hostMarkDirtyRangeRefused", !small.markDirty(256, 256, nullptr));
	c::Test_assert(t, "hostReadMarkDirtyRefused", !readable.markDirty(0, 0, nullptr));

	//A stream source is gone after the upload as well, so a mark on a streamed one is refused the same way

	c::Test_assert(t, "hostStreamedNoSource", !streamed.data()->cpuStream && !streamedRead.data()->cpuStream);
	c::Test_assert(t, "hostStreamedNoCopy", !streamed.data()->cpuData.ptr && !streamedRead.data()->cpuData.ptr);
	c::Test_assert(t, "hostStreamedMarkDirtyRefused", !streamed.markDirty(0, 0, nullptr));
	c::Test_assert(t, "hostStreamedRangeRefused", !streamed.markDirty(256, 256, nullptr));
	c::Test_assert(t, "hostStreamedReadMarkDirtyRefused", !streamedRead.markDirty(0, 0, nullptr));

	//The windows: the small buffer's start and end, the large one's start, middle and end

	const HostWindow windows[9] = {
		{ &small, hostSmallSeed, 0 },
		{ &small, hostSmallSeed, hostSmallBytes - hostWindowBytes },
		{ &large, hostLargeSeed, 0 },
		{ &large, hostLargeSeed, hostLargeBytes / 2 + 4 },
		{ &large, hostLargeSeed, hostLargeBytes - hostWindowBytes },
		{ &readable, hostReadSeed, hostReadBytes / 2 },
		{ &streamed, hostStreamSeed, hostSmallBytes - hostWindowBytes },
		{ &streamedRead, hostStreamReadSeed, 0 },
		{ &written, hostWrittenSeed, hostReadBytes / 2 }
	};

	//The refilled buffer holds its pattern only where the CPU could write it

	const c::U32 windowCount = cachedRead ? 9 : 8;

	gfx::DeviceBuffer outputs[9];

	for(c::U32 i = 0; i < windowCount; ++i)
		if(!c::Test_assert(t, "hostOutput", dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWriteBindless | c::EGraphicsResourceFlag_CPUBacked),
			"hostOutput", hostWindowBytes, outputs[i], nullptr, e_rr
		)))
			return;

	if(!c::Test_assert(t, "begin", commandList.begin(true, e_rr)))
		return;

	for (c::U32 i = 0; i < windowCount; ++i) {

		const c::U32 push[4] = { windows[i].buffer->readHandle(), outputs[i].writeHandle(), (c::U32) windows[i].offset, 0 };

		gfx::CommandScope scope = commandList.scope(
			{
				c::Transition{ .resource = windows[i].buffer->handle(), .stage = c::EPipelineStage_Compute },
				c::Transition{ .resource = outputs[i].handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			i + 1, {}, e_rr
		);

		c::Test_assert(t, "scope", (c::Bool) scope);
		c::Test_assert(t, "pipeline", scope.setComputePipeline(pipeline, e_rr));
		c::Test_assert(t, "push", scope.setPushConstants(push, e_rr));
		c::Test_assert(t, "dispatch", scope.dispatch1D((c::U32) (hostWindowBytes / 4 / 64), e_rr));
		c::Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	if(!c::Test_assert(t, "end", commandList.end(e_rr)) || !gfxtest::submitAndWait(t, dev, commandList))
		return;

	for (c::U32 i = 0; i < windowCount; ++i) {

		if(!gfxtest::pullBuffer(t, dev, emptyList, outputs[i]))
			return;

		const c::U32 *words = (const c::U32*) outputs[i].data()->cpuData.ptr;
		c::Bool equal = true;

		for(c::U64 j = 0; j < hostWindowBytes / 4; ++j)
			equal &= words[j] == hostWord(windows[i].seed, windows[i].offset / 4 + j);

		c::Test_assert(
			t,
			i < 2 ? "hostSmallRead" : (
				i < 5 ? "hostLargeRead" : (i == 5 ? "hostReadableRead" : (i < 8 ? "hostStreamedRead" : "hostWrittenRead"))
			),
			equal
		);
	}
}

//Where the memory of a HostMemory and a HostMemoryRead buffer lives, from the block each one got.
//Its allocationTypeExt tells video memory (bit 0, both backends) from shared memory, and on Vulkan the memory type's
// host cached bit (bit 3).
//Each one has a block of its own whatever its size, since shared blocks hold neither.
//A HostMemoryRead buffer on a dedicated adapter has its CPU view, which is what its CPU cached pages are for (D3D12: a
// custom heap in L0 with WRITE_BACK pages; Vulkan: a host cached and coherent memory type), a plain one has none.

extern "C" void Test_graphicsHostPlacement(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/hostPlacement");

	gfx::Device dev = gfx::Device::share(deviceRef);

	const c::U64 sizes[2] = { (c::U64) 64 << 10, hostReadBytes };
	const c::Bool dedicated = dev.info().type == c::EGraphicsDeviceType_Dedicated;

	for(c::U32 i = 0; i < 2; ++i) {

		gfx::DeviceBuffer read, plain;

		if (
			!hostCreate(t, dev, sizes[i], 1, "hostPlacementRead", read, c::EGraphicsResourceFlag_HostMemoryRead) ||
			!hostCreate(t, dev, sizes[i], 2, "hostPlacementPlain", plain)
		)
			return;

		const c::DeviceMemoryBlock &readBlock = hostBlock(dev, read);
		const c::DeviceMemoryBlock &plainBlock = hostBlock(dev, plain);

		c::Test_assert(t, "hostPlacementReadDedicated", readBlock.isDedicated);
		c::Test_assert(t, "hostPlacementPlainDedicated", plainBlock.isDedicated);

		if(dedicated) {
			c::Test_assert(t, "hostPlacementReadHost", !(readBlock.allocationTypeExt & 1));
			c::Test_assert(t, "hostPlacementPlainHost", !(plainBlock.allocationTypeExt & 1));
		}

		//The CPU's view exists exactly where the memory is cached

		const c::Bool hasView = !!read.hostMemory();

		c::Test_assert(t, "hostPlacementPlainNoView", !plain.hostMemory());

		if(dedicated)
			c::Test_assert(t, "hostPlacementReadCachedView", hasView && !read.data()->resource.hostUncached);

		if(dev.api() == c::EGraphicsApi_Vulkan)
			c::Test_assert(t, "hostPlacementReadCached", hasView == !!(readBlock.allocationTypeExt & 8));
	}
}
