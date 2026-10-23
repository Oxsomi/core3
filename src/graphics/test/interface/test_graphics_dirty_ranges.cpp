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

//graphics/test/interface/test_graphics_dirty_ranges.cpp
//
//GraphicsDevice/dirtyRanges: markDirty on a range that rounds out to the whole resource.
//A buffer rounds its ranges to 256 bytes and a texture its regions to 4 pixels,
// so a mark inside a small resource covers all of it after rounding.
//The resource then has one full pending copy and later marks are skipped as already covered,
// so that one pending range has to be the whole resource and not the caller's range.
//The buffer is 200 bytes and the texture 4 x 4: two marks far apart are made after one upload,
// the resource is submitted, poisoned and pulled back, and every byte is compared
// (the marked ones with what was written, the rest with what the first upload left).

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "types/container/buffer.h"
	#include "types/test/test.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/device_buffer.h"
	#include "graphics/generic/device_texture.h"
	#include "graphics/generic/graphics_types.h"
	#include "test_graphics_shared.h"
} }

using namespace oxc;

namespace {

	void countDirtyPull(void *resource, void *context) {
		(void) resource;
		++*(oxc::c::U32*) context;
	}
}

extern "C" void Test_graphicsDirtyRanges(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	c::Test_setModule(t, "GraphicsDevice/dirtyRanges");

	gfx::Device dev = gfx::Device::share(deviceRef);
	c::Error *e_rr = &t->err;

	gfx::CommandList emptyList;

	if(
		!c::Test_assert(t, "dirtyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr)) ||
		!c::Test_assert(t, "dirtyListBegin", emptyList.begin(true, e_rr)) ||
		!c::Test_assert(t, "dirtyListEnd", emptyList.end(e_rr))
	)
		return;

	//---- The buffer

	{
		gfx::DeviceBuffer buffer;

		if(!c::Test_assert(t, "dirtyBuffer", dev.createBuffer(
			c::EDeviceBufferUsage_None,
			(c::EGraphicsResourceFlag) (c::EGraphicsResourceFlag_ShaderRead | c::EGraphicsResourceFlag_CPUBacked),
			"Dirty range buffer", 200, buffer, nullptr, e_rr
		)))
			return;

		//The first upload: 200 zero bytes

		if(
			!c::Test_assert(t, "dirtyBufferUpload", dev.submit({ &emptyList }, {}, 0, 0, e_rr)) ||
			!c::Test_assert(t, "dirtyBufferUploadWait", dev.wait(e_rr))
		)
			return;

		c::U8 *bytes = buffer.data()->cpuData.ptrNonConst;

		for(c::U32 j = 10; j < 30; ++j)
			bytes[j] = (c::U8) (0xA0 + j);

		for(c::U32 j = 100; j < 120; ++j)
			bytes[j] = (c::U8) (0xB0 + j);

		//Two marks far apart: the first rounds to the whole buffer, the second must still reach the device

		c::U32 pulled = 0;

		const c::Bool marked =
			c::Test_assert(t, "dirtyMarkFirst", buffer.markDirty(10, 20, e_rr)) &&
			c::Test_assert(t, "dirtyMarkSecond", buffer.markDirty(100, 20, e_rr)) &&
			c::Test_assert(t, "dirtyBufferSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr)) &&
			c::Test_assert(t, "dirtyBufferWait", dev.wait(e_rr));

		for(c::U32 j = 0; j < 200; ++j)
			bytes[j] = 0xCC;

		const c::Bool back =
			marked &&
			c::Test_assert(t, "dirtyBufferPull", buffer.pullRegion(0, 0, countDirtyPull, &pulled, e_rr)) &&
			c::Test_assert(t, "dirtyBufferPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr)) &&
			c::Test_assert(t, "dirtyBufferPullWait", dev.wait(e_rr));

		c::Bool equal = back && pulled == 1;

		for(c::U32 j = 0; back && j < 200; ++j) {

			c::U8 expected = 0;

			if(j >= 10 && j < 30)
				expected = (c::U8) (0xA0 + j);

			else if(j >= 100 && j < 120)
				expected = (c::U8) (0xB0 + j);

			equal &= buffer.data()->cpuData.ptr[j] == expected;
		}

		c::Test_assert(t, "dirtyBufferFullRange", equal);
	}

	//---- The texture: 4 x 4 pixels, two marks of one pixel each in opposite corners

	{
		gfx::DeviceTexture texture;
		c::Buffer texData = c::Buffer_createNull();

		if(!c::Test_assert(t, "dirtyTexData", c::Buffer_createEmptyBytes(4 * 4 * 4, dev.alloc(), &texData, e_rr)))
			return;

		const c::Bool created = c::Test_assert(t, "dirtyTexture", dev.createTexture(
			c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_CPUBacked, 4, 4, 1,
			"Dirty range texture", &texData, texture, nullptr, e_rr
		));

		c::Buffer_free(&texData, dev.alloc());

		if(!created)
			return;

		if(
			!c::Test_assert(t, "dirtyTextureUpload", dev.submit({ &emptyList }, {}, 0, 0, e_rr)) ||
			!c::Test_assert(t, "dirtyTextureUploadWait", dev.wait(e_rr))
		)
			return;

		c::U32 *pixels = (c::U32*) texture.data()->cpuData.ptrNonConst;
		pixels[0] = 0xAABBCCDDu;
		pixels[3 * 4 + 3] = 0x11223344u;

		c::U32 pulled = 0;

		const c::Bool marked =
			c::Test_assert(t, "dirtyTexMarkFirst", texture.markDirty(0, 0, 0, 1, 1, 1, e_rr)) &&
			c::Test_assert(t, "dirtyTexMarkSecond", texture.markDirty(3, 3, 0, 1, 1, 1, e_rr)) &&
			c::Test_assert(t, "dirtyTexSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr)) &&
			c::Test_assert(t, "dirtyTexWait", dev.wait(e_rr));

		for(c::U32 j = 0; j < 16; ++j)
			pixels[j] = 0x5C5C5C5Cu;

		const c::Bool back =
			marked &&
			c::Test_assert(t, "dirtyTexPull", texture.pullRegion(0, 0, 0, 4, 4, 1, countDirtyPull, &pulled, e_rr)) &&
			c::Test_assert(t, "dirtyTexPullSubmit", dev.submit({ &emptyList }, {}, 0, 0, e_rr)) &&
			c::Test_assert(t, "dirtyTexPullWait", dev.wait(e_rr));

		c::Bool equal = back && pulled == 1;

		for(c::U32 j = 0; back && j < 16; ++j) {

			const c::U32 expected = j == 0 ? 0xAABBCCDDu : (j == 15 ? 0x11223344u : 0);
			equal &= ((const c::U32*) texture.data()->cpuData.ptr)[j] == expected;
		}

		c::Test_assert(t, "dirtyTextureFullRange", equal);
	}
}
