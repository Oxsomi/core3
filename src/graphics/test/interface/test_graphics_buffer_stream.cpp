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

//graphics/test/interface/test_graphics_buffer_stream.cpp

//A device buffer whose upload SOURCE is a stream rather than bytes the caller holds. What has to be proven is
//that the bytes reach the GPU: a flush that read the wrong offset, or nothing at all, leaves a buffer that
// still binds and still dispatches, so only reading the result back can tell the difference.
//
//The structured shader is reused for that, with its input created from a MemoryStream. Every field of every
// element is transformed differently there, so an input that arrived shifted or short shows up as a specific
// field landing on a neighbor's value rather than as a wholesale failure.

#include "test_graphics_shared.hpp"
#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/list_basic_types.h"
	#include "types/container/memory_stream.h"
	#include "types/container/ref_ptr.h"
	#include "types/test/test.h"
	#include "formats/oiSH/sh_file.h"
	#include "platforms/file.h"
	#include "platforms/platform.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/descriptor_heap.h"
	#include "graphics/generic/descriptor_layout.h"
	#include "graphics/generic/descriptor_table.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/pipeline_layout.h"
	#include "graphics/generic/pipeline_structs.h"
	#include "types/container/job_queue.h"
}}

using namespace oxc;

typedef struct TestStreamElem {
	c::U32 a, b, c, d;
} TestStreamElem;

//The device objects the legs below share. They are built once because a pipeline, a heap and a descriptor
//table cost more to create than any leg costs to run, and rebuilding them per leg would only re-test the
// create path.
//
//commandList binds the stream backed input to the output and dispatches; emptyList records nothing, for a
// submit whose only job is to carry a flush.

typedef struct StreamFixture {
	gfx::Device &dev;
	gfx::DescriptorHeap &heap;
	gfx::DescriptorTable &table;
	gfx::Pipeline &pipeline;
	gfx::DeviceBuffer &output;
	gfx::CommandList &commandList;
	gfx::CommandList &emptyList;
	const c::RefPtrType &streamType;
} StreamFixture;

//The TAIL of a stream sourced buffer, uploaded and read back through the shader. A buffer past a quarter of
//the device's staging buffer takes the DEDICATED staging path, where the flush allocates its own staging and
// copies through that, and a truncated or misaddressed upload there leaves the beginning right and the end
// wrong, which nothing smaller can catch.
//
//The SOURCE is what varies between callers. A memory stream's read is a memcpy the process makes itself, where
//a file stream's is the kernel writing into the destination, and that destination is memory the graphics API
// handed out and mapped, so neither stands in for the other.
static void Test_streamedTail(
	c::Test *t, const StreamFixture &fx, c::StreamRef *stream, c::U64 bigLen,
	const c::C8 *name, const c::C8 *resultName
) {

	c::Error *e_rr = &t->err;

	const c::U64 count = bigLen / sizeof(TestStreamElem);
	const c::U64 tailAt = bigLen - 64 * sizeof(TestStreamElem);

	gfx::DeviceBuffer bigBuf;

	if(!Test_assert(t, name, fx.dev.createBufferStream(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
		name, stream, bigLen, bigBuf, false, nullptr, e_rr
	)))
		return;

	//A view of the LAST 64 elements, so what the shader transforms is the tail of the upload.
	//maintainRef, because the buffer is scoped to this call while the TABLE outlives it: a descriptor set
	// without one is a weak pointer the next set has to read to release, and this buffer would be gone by then.

	const c::Descriptor tailDesc = c::Descriptor_buffer(bigBuf.handle(), tailAt, bigLen, NULL, 0);

	if(!Test_assert(t, "setBigInput", fx.table.setByName("input", tailDesc, 0, true, e_rr)))
		return;

	gfx::CommandList bigList;

	if(!Test_assert(t, "bigList", fx.dev.createCommandList(2 * c::KIBI, 32, 16, bigList, true, e_rr)))
		return;

	Test_assert(t, "bigBegin", bigList.begin(true, e_rr));

	{
		gfx::CommandScope scope = bigList.scope({
			{ .resource = bigBuf.handle(), .stage = c::EPipelineStage_Compute },
			{ .resource = fx.output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
		}, 1, {}, e_rr);

		Test_assert(t, "bigScope", (c::Bool) scope);
		Test_assert(t, "bigHeap", scope.bindDescriptorHeap(fx.heap, e_rr));
		Test_assert(t, "bigTable", scope.bindDescriptorTable(fx.table, e_rr));
		Test_assert(t, "bigPipeline", scope.setComputePipeline(fx.pipeline, e_rr));
		Test_assert(t, "bigDispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "bigScopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "bigEnd", bigList.end(e_rr));

	if(!gfxtest::submitAndWait(t, fx.dev, bigList) || !gfxtest::pullBuffer(t, fx.dev, fx.emptyList, fx.output))
		return;

	const TestStreamElem *v = (const TestStreamElem*) fx.output.data()->cpuData.ptr;
	const c::U64 base = count - 64;

	c::Bool tailMatch = true;

	for(c::U32 i = 0; i < 64; ++i) {
		const c::U32 e = (c::U32) (base + i);
		tailMatch &= v[i].a == e * 2 && v[i].b == e * 7 + 100;
	}

	Test_assert(t, resultName, tailMatch);
}

//A stream source the flag cannot honor, refused at create rather than accepted and quietly ignored.
static void Test_streamRefusals(c::Test *t, gfx::Device &dev, c::StreamRef *source, c::U64 elemsSize) {

	//Refusals first, because both of them are the reason the flag exists rather than being a silent fallback.

	{
		gfx::DeviceBuffer refused;

		Test_assert(t, "cpuBackedRefused", !dev.createBufferStream(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderRead | c::EGraphicsResourceFlag_CPUBacked),
			"Stream CPUBacked", (c::StreamRef*) source, elemsSize, refused, false, nullptr, nullptr
		));

		Test_assert(t, "zeroSizeRefused", !dev.createBufferStream(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
			"Stream empty", (c::StreamRef*) source, 0, refused, false, nullptr, nullptr
		));

		Test_assert(t, "nullStreamRefused", !dev.createBufferStream(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
			"Stream null", nullptr, elemsSize, refused, false, nullptr, nullptr
		));
	}
}

//The whole point: the bytes reach the GPU and come back transformed.
static void Test_streamUpload(c::Test *t, const StreamFixture &fx) {

	if (gfxtest::submitAndWait(t, fx.dev, fx.commandList))
		if (gfxtest::pullBuffer(t, fx.dev, fx.emptyList, fx.output)) {

			const TestStreamElem *values = (const TestStreamElem*) fx.output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &=
					values[i].a == i * 2 &&
					values[i].b == i * 7 + 100 &&
					values[i].c == ((i * 3) ^ 0xFFu) &&
					values[i].d == i * 11 + i;

			//Every element right means the whole stream arrived at the right offset: a short or shifted read
			// lands a neighbor's value in at least one field.

			Test_assert(t, "streamUploadResults", allMatch);
		}
}

//Only the marked range is refreshed, out of a source that was rewritten in place.
static void Test_streamPartialUpdate(
	c::Test *t, const StreamFixture &fx, gfx::DeviceBuffer &input, TestStreamElem *elems, c::U64 elemsSize
) {

	c::Error *e_rr = &t->err;

	//A PARTIAL update, which is the reason the read is positional at all. The stream's bytes are rewritten in
	//place and only the SECOND half is marked dirty, so the first half must keep the values it already has and
	// the second must take the new ones. A flush that read from offset zero for every range would refresh the
	// first half instead, which is exactly the failure this catches.

	for(c::U32 i = 32; i < 64; ++i)
		elems[i] = { .a = i + 1000, .b = i * 7, .c = i * 3, .d = i * 11 };

	if(!Test_assert(t, "markSecondHalf", c::DeviceBufferRef_markDirty(
		input.handle(), elemsSize / 2, elemsSize / 2, e_rr
	)))
		return;

	if (gfxtest::submitAndWait(t, fx.dev, fx.commandList))
		if (gfxtest::pullBuffer(t, fx.dev, fx.emptyList, fx.output)) {

			const TestStreamElem *values = (const TestStreamElem*) fx.output.data()->cpuData.ptr;

			c::Bool lowUnchanged = true, highUpdated = true;

			for(c::U32 i = 0; i < 32; ++i)
				lowUnchanged &= values[i].a == i * 2;

			for(c::U32 i = 32; i < 64; ++i)
				highUpdated &= values[i].a == (i + 1000) * 2;

			Test_assert(t, "partialKeptTheRest", lowUnchanged);
			Test_assert(t, "partialUpdatedTheRange", highUpdated);
		}
}

//The same partial update with no source at all, straight into memory the device keeps mapped.
static void Test_streamMapped(c::Test *t, const StreamFixture &fx, c::U64 elemsSize) {

	c::Error *e_rr = &t->err;

	//The OTHER source a partial update can have: none at all. CPUAllocatedBit puts the resource in host visible
	//memory that stays mapped, so the caller writes into it directly and markDirty only has to record the range
	// for an incoherent fx.heap's flush. A copy would be the bytes onto themselves.

	gfx::DeviceBuffer mapped;

	if(!Test_assert(t, "mappedCreate", fx.dev.createBuffer(
		c::EDeviceBufferUsage_None,
		(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderRead | c::EGraphicsResourceFlag_CPUAllocatedBit),
		"Mapped input", elemsSize, mapped, nullptr, e_rr
	)))
		return;

	c::DeviceBuffer *mappedPtr = mapped.data();

	if(!mappedPtr->resource.mappedMemoryExt) {
		c::Test_print(t, "Device gave no mapped pointer for a host visible buffer, skipping the mapped leg");
		return;
	}

	//No cpuData and no stream, so nothing but this write can put the values there.

	Test_assert(t, "mappedHasNoHostCopy", !c::Buffer_length(mappedPtr->cpuData) && !mappedPtr->cpuStream);

	TestStreamElem *live = (TestStreamElem*) mappedPtr->resource.mappedMemoryExt;

	for(c::U32 i = 0; i < 64; ++i)
		live[i] = { .a = i + 7, .b = i, .c = i, .d = i };

	Test_assert(t, "markMapped", c::DeviceBufferRef_markDirty(mapped.handle(), 0, 0, e_rr));

	const c::Descriptor mappedDesc = c::Descriptor_buffer(mapped.handle(), 0, 0, NULL, 0);

	//maintainRef for the same reason the tail leg needs it: this buffer goes out of scope while the table is
	// still holding its descriptor.

	Test_assert(t, "setMappedInput", fx.table.setByName("input", mappedDesc, 0, true, e_rr));

	gfx::CommandList mappedList;

	if(!Test_assert(t, "mappedListCreate", fx.dev.createCommandList(2 * c::KIBI, 32, 16, mappedList, true, e_rr)))
		return;

	Test_assert(t, "mappedBegin", mappedList.begin(true, e_rr));

	{
		gfx::CommandScope scope = mappedList.scope(
			{
				{ .resource = mapped.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = fx.output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "mappedScope", (c::Bool) scope);
		Test_assert(t, "mappedBindHeap", scope.bindDescriptorHeap(fx.heap, e_rr));
		Test_assert(t, "mappedBindTable", scope.bindDescriptorTable(fx.table, e_rr));
		Test_assert(t, "mappedBindPipeline", scope.setComputePipeline(fx.pipeline, e_rr));
		Test_assert(t, "mappedDispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "mappedScopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "mappedEnd", mappedList.end(e_rr));

	if (gfxtest::submitAndWait(t, fx.dev, mappedList))
		if (gfxtest::pullBuffer(t, fx.dev, fx.emptyList, fx.output)) {

			const TestStreamElem *values = (const TestStreamElem*) fx.output.data()->cpuData.ptr;

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= values[i].a == (i + 7) * 2 && values[i].b == i + 100;

			Test_assert(t, "mappedUploadResults", allMatch);
		}

}

//Past a quarter of the device's staging buffer the flush allocates its own, a branch nothing above
//reaches.
static void Test_streamDedicatedStaging(c::Test *t, const StreamFixture &fx) {

	c::Error *e_rr = &t->err;

	//---- A stream backed buffer big enough to take the DEDICATED staging path, which is a different branch
	//from the one every test above exercises: past a quarter of the device's staging buffer the flush
	//allocates its own and copies through that.
	//
	//The same bytes go through it out of a memory stream and out of a FILE, since a file is the shape a mesh
	// or a texture actually arrives in and its read reaches the destination by a different route.

	{
		c::GraphicsDeviceRef *dr = fx.dev.handle();
		c::GraphicsDevice *dp = RefPtr_data(dr, c::GraphicsDevice);
		const c::U64 threshold = RefPtr_data(dp->staging, c::DeviceBuffer)->resource.size / 4;
		const c::U64 bigLen = (threshold * 2 + sizeof(TestStreamElem) - 1) /
			sizeof(TestStreamElem) * sizeof(TestStreamElem);

		const c::U64 count = bigLen / sizeof(TestStreamElem);

		c::Buffer backing = c::Buffer_createNull();

		if(Test_assert(t, "bigBacking", c::Buffer_createEmptyBytes(bigLen, fx.dev.alloc(), &backing, e_rr))) {

			TestStreamElem *src = (TestStreamElem*) backing.ptrNonConst;

			for(c::U64 i = 0; i < count; ++i)
				src[i] = { .a = (c::U32) i, .b = (c::U32) (i * 7), .c = (c::U32) (i * 3), .d = (c::U32) (i * 11) };

			c::MemoryStreamRef *bigStream = nullptr;

			if(Test_assert(t, "bigStream", c::MemoryStream_createFromBufferRegion(
				c::Buffer_createRefConst(backing.ptr, bigLen), 0, bigLen,
				c::EMemoryStreamFlags_None, &fx.streamType, &bigStream, e_rr
			))) {

				Test_streamedTail(t, fx, (c::StreamRef*) bigStream, bigLen, "Big stream", "bigTailUploaded");

				c::RefPtr_dec((c::RefPtr**) &bigStream);
			}

			const c::RefPtrType fileHandleType = c::FileHandle_makeType(fx.dev.alloc());
			const c::RefPtrType fileStreamType = c::FileStream_makeType(fx.dev.alloc());
			const c::CharString bigPath = c::CharString_createRefCStrConst("graphics_test_big_stream.bin");

			c::File_remove(&bigPath, 1 * c::SECOND, fx.dev.alloc(), NULL);

			if(Test_assert(t, "bigFileWrite", c::File_write(
				&backing, &bigPath, 0, 0, 1 * c::SECOND, true, &fileHandleType, e_rr
			))) {

				c::StreamRef *fileStream = nullptr;

				if(Test_assert(t, "bigFileStream", c::File_openStream(
					&bigPath, 1 * c::SECOND, c::EFileOpenType_Read, false,
					&fileHandleType, &fileStreamType, &fileStream, e_rr
				))) {

					//A read only file stream declares CONCURRENT reads, because its read is a positional pread
					//that never touches the handle's shared offset. That declaration is the whole reason a
					// large file backed upload can be split across a lent queue, so it is asserted rather
					// than assumed, and its cost is read off readPieces for the same reason.

					Test_assert(t, "fileDeclaresConcurrent", !!(
						RefPtr_data(fileStream, c::OxStream)->streamType & c::EStreamType_ConcurrentRead
					));

					Test_assert(t, "fileReadCostIsFile", RefPtr_data(fileStream, c::OxStream)->readCost ==
						(c::U16) c::EStreamReadCost_File);

					Test_streamedTail(t, fx, fileStream, bigLen, "Big file stream", "bigFileTailUploaded");

					c::RefPtr_dec((c::RefPtr**) &fileStream);
				}

				//A WRITABLE handle over the same file declares nothing: its size moves under a reader, and
				// the write path updates that size without synchronizing against one.

				c::StreamRef *rw = nullptr;

				if(Test_assert(t, "bigFileWritable", c::File_openStream(
					&bigPath, 1 * c::SECOND, c::EFileOpenType_ReadWrite, false,
					&fileHandleType, &fileStreamType, &rw, e_rr
				))) {

					Test_assert(t, "writableDeclaresNoConcurrent", !(
						RefPtr_data(rw, c::OxStream)->streamType & c::EStreamType_ConcurrentRead
					));

					c::RefPtr_dec((c::RefPtr**) &rw);
				}

				c::File_remove(&bigPath, 1 * c::SECOND, fx.dev.alloc(), NULL);
			}

			c::Buffer_free(&backing, fx.dev.alloc());
		}
	}

}

//A source whose length does not land on a word, which is what most files are.
static void Test_streamOddLength(c::Test *t, const StreamFixture &fx, c::U64 elemsSize) {

	c::Error *e_rr = &t->err;

	//---- A source whose length does not land on a word, which is what most files are. A raw buffer is WORD
	//addressed, so the resource is rounded up to one and the stream simply ends inside that padding.
	//
	//The bindless handle is what proves the descriptor was made: an allocation that fails leaves the handle at
	//ZERO, and zero is a handle a shader still reads, so the failure would have arrived as an unrelated
	// resource's bytes rather than as an error. Reading the content back is what proves the padding did not
	// cost the buffer its last element.

	{
		TestStreamElem oddSrc[65];

		for(c::U32 i = 0; i < 65; ++i)
			oddSrc[i] = { .a = i, .b = i * 7, .c = i * 3, .d = i * 11 };

		const c::U64 oddLen = elemsSize + 3;        //Three bytes into the 65th element

		c::MemoryStreamRef *oddStream = nullptr;
		gfx::DeviceBuffer oddBuf;

		if(Test_assert(t, "oddStream", c::MemoryStream_createFromBufferRegion(
			c::Buffer_createRefConst(oddSrc, oddLen), 0, oddLen,
			c::EMemoryStreamFlags_None, &fx.streamType, &oddStream, e_rr
		))) {

			if(Test_assert(t, "oddBuffer", fx.dev.createBufferStream(
				c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderReadBindless,
				"Odd stream", (c::StreamRef*) oddStream, oddLen, oddBuf, false, nullptr, e_rr
			))) {

				Test_assert(t, "oddBindlessHandle", oddBuf.readHandle() != 0);

				//The padding is the backend allocation's and the view's, never the resource's: what the
				// resource reports is what the caller asked for, since that is the length of the upload and
				// of every region taken from it.

				Test_assert(t, "oddSizeIsExact", oddBuf.data()->resource.size == oddLen);

				const c::Descriptor oddDesc = c::Descriptor_buffer(oddBuf.handle(), 0, elemsSize, NULL, 0);

				//maintainRef for the same reason the tail leg needs it: this buffer goes out of scope while
				// the fx.table is still holding its descriptor.

				if(Test_assert(t, "setOddInput", fx.table.setByName("input", oddDesc, 0, true, e_rr)))
					if(gfxtest::submitAndWait(t, fx.dev, fx.commandList))
						if(gfxtest::pullBuffer(t, fx.dev, fx.emptyList, fx.output)) {

							const TestStreamElem *v = (const TestStreamElem*) fx.output.data()->cpuData.ptr;

							c::Bool allMatch = true;

							for(c::U32 i = 0; i < 64; ++i)
								allMatch &=
									v[i].a == i * 2 &&
									v[i].b == i * 7 + 100 &&
									v[i].c == ((i * 3) ^ 0xFFu) &&
									v[i].d == i * 11 + i;

							Test_assert(t, "oddUploadResults", allMatch);
						}
			}

			c::RefPtr_dec((c::RefPtr**) &oddStream);
		}
	}

}

//A buffer that was never told to keep its source must not still be holding one.
static void Test_streamSourceReleased(c::Test *t, const StreamFixture &fx, const TestStreamElem *elems, c::U64 elemsSize) {

	c::Error *e_rr = &t->err;

	//---- The DEFAULT, which is to let the source go once the upload has consumed it. Holding a stream holds
	//a file descriptor or a cipher context, so a buffer that was never told to keep one must not.

	{
		c::MemoryStreamRef *once = nullptr;
		gfx::DeviceBuffer oneShot;

		if(Test_assert(t, "oneShotStream", c::MemoryStream_createFromBufferRegion(
			c::Buffer_createRefConst(elems, elemsSize), 0, elemsSize,
			c::EMemoryStreamFlags_None, &fx.streamType, &once, e_rr
		))) {

			if(Test_assert(t, "oneShotCreate", fx.dev.createBufferStream(
				c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
				"One shot", (c::StreamRef*) once, elemsSize, oneShot, false, nullptr, e_rr
			))) {

				//Still held before the upload runs, since that is what it will read from.

				Test_assert(t, "heldBeforeUpload", oneShot.data()->cpuStream != nullptr);

				if(Test_assert(t, "oneShotSubmit", gfxtest::submitAndWait(t, fx.dev, fx.emptyList))) {

					Test_assert(t, "droppedAfterUpload", !oneShot.data()->cpuStream);

					//And with no source left, a range is refused again, the same way it is for a buffer
					// whose cpuData was freed. Nothing special says so; it falls out of the same check.

					c::Error ignored = c::Error_none();

					Test_assert(t, "rangeRefusedWithoutSource", !c::DeviceBufferRef_markDirty(
						oneShot.handle(), 0, 64, &ignored
					));
				}
			}

			c::RefPtr_dec((c::RefPtr**) &once);
		}
	}
}

typedef struct PullResult {
	c::U32 fired;
	c::Bool ok;
} PullResult;

static void Test_streamPullDone(void *resource, c::Bool ok, void *context) {
	(void) resource;
	PullResult *r = (PullResult*) context;
	++r->fired;
	r->ok = ok;
}

//A readback whose destination is a STREAM. The buffer is deliberately not CPUBacked, since skipping the host
//copy the size of the resource is the reason the entry point exists.

static void Test_streamPullRegion(c::Test *t, const StreamFixture &fx, gfx::DeviceBuffer &input, c::U64 elemsSize) {

	c::Error *e_rr = &t->err;

	gfx::DeviceBuffer sink;

	if(!Test_assert(t, "sinkCreate", fx.dev.createBuffer(
		c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderWrite,
		"Pull sink", elemsSize, sink, nullptr, e_rr
	)))
		return;

	Test_assert(t, "sinkHasNoHostCopy", !c::Buffer_length(sink.data()->cpuData));

	//maintainRef: the sink is scoped to this call while the table outlives it.

	const c::Descriptor sinkDesc = c::Descriptor_buffer(sink.handle(), 0, 0, NULL, 0);

	if(!Test_assert(t, "setSinkOutput", fx.table.setByName("output", sinkDesc, 0, true, e_rr)))
		return;

	gfx::CommandList list;

	if(!Test_assert(t, "sinkListCreate", fx.dev.createCommandList(2 * c::KIBI, 32, 16, list, true, e_rr)))
		return;

	Test_assert(t, "sinkBegin", list.begin(true, e_rr));

	{
		gfx::CommandScope scope = list.scope(
			{
				{ .resource = input.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = sink.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "sinkScope", (c::Bool) scope);
		Test_assert(t, "sinkBindHeap", scope.bindDescriptorHeap(fx.heap, e_rr));
		Test_assert(t, "sinkBindTable", scope.bindDescriptorTable(fx.table, e_rr));
		Test_assert(t, "sinkBindPipeline", scope.setComputePipeline(fx.pipeline, e_rr));
		Test_assert(t, "sinkDispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "sinkScopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "sinkEnd", list.end(e_rr));

	if(!Test_assert(t, "sinkSubmit", gfxtest::submitAndWait(t, fx.dev, list)))
		return;

	c::MemoryStreamRef *dst = nullptr;

	if(Test_assert(t, "pullStream", c::MemoryStream_create(
		elemsSize, c::EMemoryStreamFlags_IsWritable, &fx.streamType, &dst, e_rr
	))) {

		PullResult res = {};

		Test_assert(t, "pullQueued", c::DeviceBufferRef_pullRegionStream(
			sink.handle(), 0, elemsSize, (c::StreamRef*) dst, 0, Test_streamPullDone, &res, e_rr
		));

		//The copy is recorded after the next submit's lists and completes once that frame finished

		Test_assert(t, "pullSubmit", gfxtest::submitAndWait(t, fx.dev, fx.emptyList));
		Test_assert(t, "pullFiredOnce", res.fired == 1);
		Test_assert(t, "pullLanded", res.ok);

		//What the shader wrote, read back out of the stream rather than out of a cpuData that never existed

		TestStreamElem got[64];
		c::OxStream *str = RefPtr_data((c::StreamRef*) dst, c::OxStream);

		if(Test_assert(t, "pullStreamRead", str->read(
			str, 0, sizeof(got), c::Buffer_createRef(got, sizeof(got)), fx.dev.alloc(), e_rr
		))) {

			c::Bool allMatch = true;

			for(c::U32 i = 0; i < 64; ++i)
				allMatch &= got[i].a == i * 2 && got[i].b == i * 7 + 100;

			Test_assert(t, "pullStreamContents", allMatch);
		}

		c::RefPtr_dec((c::RefPtr**) &dst);
	}

	//The submits put the sink in flight, so it is handed back before this frame's stack goes

	Test_assert(t, "pullDrained", fx.dev.wait(e_rr));
}

//What decides a split, asserted as a rule rather than inferred from a timing.
static void Test_streamFanOutPolicy(c::Test *t, const StreamFixture &fx) {

	c::Error *e_rr = &t->err;

	//---- The fan out POLICY, asserted as a rule rather than inferred from a timing.

	c::JobQueue queue = {};

	if(!Test_assert(t, "jobQueue", c::JobQueue_create(4, fx.dev.alloc(), &queue, e_rr)))
		return;

	//A range big enough to be worth splitting, out of two streams that differ only in whether they declare
	//concurrent reads: a plain memory stream does, a RESIZABLE one cannot, since a reserve can move the buffer
	// out from under a reader.

	const c::U64 big = 8 * c::MIBI;

	c::MemoryStreamRef *shared = nullptr, *resizable = nullptr;

	c::Bool madeStreams =
		Test_assert(t, "sharedStream", c::MemoryStream_create(
			big, c::EMemoryStreamFlags_None, &fx.streamType, &shared, e_rr
		)) &&
		Test_assert(t, "resizableStream", c::MemoryStream_create(
			big, c::EMemoryStreamFlags_IsResizable, &fx.streamType, &resizable, e_rr
		));

	if(madeStreams) {

		c::StreamRef *sharedRef = (c::StreamRef*) shared, *resizableRef = (c::StreamRef*) resizable;

		const c::OxStream *sharedStr = RefPtr_data(sharedRef, c::OxStream);
		const c::OxStream *resizableStr = RefPtr_data(resizableRef, c::OxStream);

		Test_assert(t, "declaresConcurrent", !!(sharedStr->streamType & c::EStreamType_ConcurrentRead));
		Test_assert(t, "resizableDeclaresNothing", !(resizableStr->streamType & c::EStreamType_ConcurrentRead));

		gfx::DeviceBuffer sharedBuf, resizableBuf;

		c::Bool madeBuffers =
			Test_assert(t, "sharedBuffer", fx.dev.createBufferStream(
				c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
				"Fan out shared", sharedRef, big, sharedBuf, true, nullptr, e_rr
			)) &&
			Test_assert(t, "resizableBuffer", fx.dev.createBufferStream(
				c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
				"Fan out resizable", resizableRef, big, resizableBuf, true, nullptr, e_rr
			));

		if(madeBuffers) {

			//With NO queue lent, everything reads on the calling thread whatever the stream says.

			Test_assert(t, "noQueueNoSplit", c::DeviceBuffer_readPieces(sharedBuf.data(), sharedStr, big) == 1);

			c::GraphicsDeviceRef *devRef = fx.dev.handle();
			c::GraphicsDevice *devPtr = RefPtr_data(devRef, c::GraphicsDevice);
			devPtr->uploadJobQueue = &queue;

			//THE rule: declared concurrent splits, not declared does not, at the identical size and cost.

			Test_assert(t, "concurrentSplits", c::DeviceBuffer_readPieces(sharedBuf.data(), sharedStr, big) > 1);

			Test_assert(
				t, "notConcurrentSerializes",
				c::DeviceBuffer_readPieces(resizableBuf.data(), resizableStr, big) == 1
			);

			//And a range too small to pay for the split stays whole even on a stream that allows one.

			Test_assert(t, "smallStaysWhole", c::DeviceBuffer_readPieces(sharedBuf.data(), sharedStr, 4096) == 1);

			devPtr->uploadJobQueue = nullptr;

			//The lend itself, and with it the only path that actually RUNS the split rather than asking what
			//it would do. A queue is gone again by the time the submit returns, on every path, since one left
			// behind would be read by a later flush after the lender had freed it.

			Test_assert(t, "lendSucceeds", fx.dev.submitJob({ &fx.emptyList }, {}, &queue, 0, 0, e_rr));
			Test_assert(t, "lendClearedAfterSubmit", !devPtr->uploadJobQueue);

			//That submit put these buffers IN FLIGHT, so the device holds them for frames yet to come. Their
			//streams carry a pointer to the RefPtrType above, which lives on this stack, so a buffer released
			// after this function returned would read a type that no longer exists. Waiting hands them back
			// while the type is still here.

			Test_assert(t, "fanOutDrained", fx.dev.wait(e_rr));
		}
	}

	if(shared)
		c::RefPtr_dec((c::RefPtr**) &shared);

	if(resizable)
		c::RefPtr_dec((c::RefPtr**) &resizable);

	c::JobQueue_free(&queue);
}

extern "C" void Test_graphicsBufferStream(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/bufferStream");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	TestStreamElem elems[64];

	for(c::U32 i = 0; i < 64; ++i)
		elems[i] = { .a = i, .b = i * 7, .c = i * 3, .d = i * 11 };

	const c::RefPtrType streamType = c::MemoryStream_makeType(dev.alloc());
	c::MemoryStreamRef *source = nullptr;

	if(!Test_assert(t, "sourceStream", c::MemoryStream_createFromBufferRegion(
		c::Buffer_createRefConst(elems, sizeof(elems)), 0, sizeof(elems),
		c::EMemoryStreamFlags_None, &streamType, &source, e_rr
	)))
		return;

	Test_streamRefusals(t, dev, (c::StreamRef*) source, sizeof(elems));
	gfxtest::OwnedSHFile file(dev.alloc());

	if (!gfxtest::loadFile(t, "//OxC3_gtest/test_shaders/test_bindful_structured.oiSH", file.list)) {
		c::Test_print(t, "Test shaders unavailable (built without shader compiler), skipping the executed leg");
		c::RefPtr_dec((c::RefPtr**) &source);
		return;
	}

	const c::U32 entryId = gfxtest::entry(t, dev, file.list, "main");

	if(entryId == c::U32_MAX) {
		c::RefPtr_dec((c::RefPtr**) &source);
		return;
	}

	gfx::DescriptorHeap heap;
	gfx::DescriptorLayout layout;
	gfx::DescriptorTable table;
	gfx::PipelineLayout pipelineLayout;
	gfx::Pipeline pipeline;
	gfx::DeviceBuffer input, output;
	gfx::CommandList commandList, emptyList;

	gfxtest::TableGuard tableGuard{ { &table } };
	gfxtest::OwnedLayoutInfo layoutInfo(dev.alloc());

	c::Bool ready =
		Test_assert(t, "detectLayout", dev.detectLayout(
			file.list, entryId, layoutInfo.list, nullptr, nullptr, {}, nullptr,
			c::EDescriptorLayoutFlags_None, (c::EDetectDescriptorLayoutFlags) 0, e_rr
		)) &&
		Test_assert(t, "layoutCreate", dev.createDescriptorLayout(layoutInfo.list, "Stream layout", layout, e_rr));

	c::DescriptorHeapInfo heapInfo = { .maxBuffersRW = 2, .maxDescriptorTables = 1 };

	ready = ready &&
		Test_assert(t, "heapCreate", dev.createDescriptorHeap(heapInfo, "Stream heap", heap, e_rr)) &&
		Test_assert(t, "tableCreate", heap.createTable(
			layout, "Stream table", table, c::EDescriptorTableFlags_None, e_rr
		)) &&

		//THE thing under test: the input never exists as a host buffer, only as the stream behind it.

		Test_assert(t, "inputFromStream", dev.createBufferStream(
			c::EDeviceBufferUsage_None, c::EGraphicsResourceFlag_ShaderRead,
			"Stream input", (c::StreamRef*) source, sizeof(elems), input, true, nullptr, e_rr
		)) &&

		Test_assert(t, "outputCreate", dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderWrite | c::EGraphicsResourceFlag_CPUBacked),
			"Stream output", sizeof(elems), output, nullptr, e_rr
		));

	if(!ready) {
		c::RefPtr_dec((c::RefPtr**) &source);
		return;
	}

	//The buffer holds its own reference now, so the caller's handle going away must not take the source with it.

	c::RefPtr_dec((c::RefPtr**) &source);

	const c::Descriptor inputDesc = c::Descriptor_buffer(input.handle(), 0, 0, NULL, 0);
	const c::Descriptor outputDesc = c::Descriptor_buffer(output.handle(), 0, 0, NULL, 0);

	Test_assert(t, "setInput", table.setByName("input", inputDesc, 0, false, e_rr));
	Test_assert(t, "setOutput", table.setByName("output", outputDesc, 0, false, e_rr));

	c::PipelineLayoutInfo pipelineLayoutInfo = { .bindings = layout.handle() };

	if(
		!Test_assert(t, "pipelineLayoutCreate", dev.createPipelineLayout(
			pipelineLayoutInfo, "Stream pipeline layout", pipelineLayout, e_rr
		)) ||
		!Test_assert(t, "pipelineCreate", dev.createComputePipeline(
			file.list, "main", "Stream pipeline", pipeline, {}, &pipelineLayout, e_rr
		)) ||
		!Test_assert(t, "listCreate", dev.createCommandList(2 * c::KIBI, 32, 16, commandList, true, e_rr)) ||
		!Test_assert(t, "emptyListCreate", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr))
	)
		return;

	Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
	Test_assert(t, "endEmpty", emptyList.end(e_rr));

	Test_assert(t, "begin", commandList.begin(true, e_rr));

	{
		gfx::CommandScope scope = commandList.scope(
			{
				{ .resource = input.handle(), .stage = c::EPipelineStage_Compute },
				{ .resource = output.handle(), .stage = c::EPipelineStage_Compute, .isWrite = true }
			},
			1, {}, e_rr
		);

		Test_assert(t, "scope", (c::Bool) scope);
		Test_assert(t, "bindHeap", scope.bindDescriptorHeap(heap, e_rr));
		Test_assert(t, "bindTable", scope.bindDescriptorTable(table, e_rr));
		Test_assert(t, "bindPipeline", scope.setComputePipeline(pipeline, e_rr));
		Test_assert(t, "dispatch", scope.dispatch1D(1, e_rr));
		Test_assert(t, "scopeEnd", scope.end(e_rr));
	}

	Test_assert(t, "end", commandList.end(e_rr));
	const StreamFixture fx = { dev, heap, table, pipeline, output, commandList, emptyList, streamType };

	Test_streamUpload(t, fx);
	Test_streamPartialUpdate(t, fx, input, elems, sizeof(elems));
	Test_streamMapped(t, fx, sizeof(elems));
	Test_streamDedicatedStaging(t, fx);
	Test_streamOddLength(t, fx, sizeof(elems));
	Test_streamSourceReleased(t, fx, elems, sizeof(elems));
	Test_streamPullRegion(t, fx, input, sizeof(elems));

	//The input above was created WITH keepSource, which is why its partial update worked at all.

	Test_assert(t, "keptSourceSurvived", input.data()->cpuStream != nullptr);
	Test_streamFanOutPolicy(t, fx);
}
