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

//graphics/test/interface/test_graphics_virtual_swapchain.cpp

//A swapchain over a VIRTUAL window: no surface, no presentation engine, images this device owns,
// and a frame that ends in memory rather than on a screen.
//That is what a machine with GPUs and no display can run, and it is the same thing an encoder or an image sequence
// consumes.
//
//What this proves is that nothing above the backend has to know:
// the swapchain is created, cleared and read back through the ordinary swapchain calls,
// and the pixels that come back are the ones the frame wrote.

#include <stdlib.h>

#include "test_graphics_shared.hpp"

namespace oxc { namespace c {
	#include "types/base/string_base.h"
	#include "types/container/buffer.h"
	#include "types/container/texture_format.h"
	#include "types/test/test.h"
	#include "platforms/platform.h"
	#include "platforms/window.h"
	#include "platforms/window_manager.h"
	#include "graphics/generic/command_list.h"
	#include "graphics/generic/commands.h"
	#include "graphics/generic/device.h"
	#include "graphics/generic/swapchain.h"
	#include "graphics/generic/texture.h"
}}

namespace {

	//The pull hands over a tight row buffer of the whole image, so the check is every texel rather than a sample.

	struct PullResult {
		oxc::c::U32 count;
		oxc::c::U32 mismatches;
		oxc::c::U32 first;
		oxc::c::U64 length;
	};

	void onPulled(oxc::c::RefPtr *resource, oxc::c::Buffer *data, void *context) {

		(void) resource;

		PullResult *result = (PullResult*) context;
		++result->count;

		if(!data) {
			result->length = 0;
			return;
		}

		result->length = oxc::c::Buffer_length(*data);

		const oxc::c::U32 *texels = (const oxc::c::U32*) data->ptr;

		result->first = result->length >= 4 ? texels[0] : 0;

		for(oxc::c::U64 i = 0; i * 4 + 4 <= result->length; ++i)
			result->mismatches += texels[i] != texels[0];
	}
}

extern "C" void Test_graphicsVirtualSwapchain(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/virtualSwapchain");

	Device dev = Device::share(deviceRef);

	c::WindowManager manager = (c::WindowManager) { 0 };
	c::WindowRef *windowRef = NULL;

	const c::WindowManagerCallbacks managerCallbacks = (c::WindowManagerCallbacks) { 0 };

	if(!c::Test_assert(t, "createManager", c::WindowManager_create(managerCallbacks, 0, &manager, e_rr)))
		return;

	//A virtual window is always available, which is the whole point: no display server has to exist.

	const c::WindowCallbacks windowCallbacks = (c::WindowCallbacks) { 0 };
	const c::CharString title = c::CharString_createRefCStrConst("Virtual swapchain test");

	const c::I32x2 pos = c::I32x2_create2(0, 0);
	const c::I32x2 size = c::EResolution_get(c::EResolution_SD);
	const c::I32x2 maxSize = c::I32x2_create2(4096, 4096);

	if(!c::Test_assert(t, "createVirtualWindow", c::WindowManager_createWindow(
		&manager, c::EWindowType_Virtual, pos, size, size, maxSize,
		c::EWindowHint_None, title, windowCallbacks, c::EWindowFormat_AutoRGBA8, 0, &windowRef, e_rr
	)))
		goto clean;

	{
		c::Window *window = RefPtr_data(windowRef, c::Window);

		Swapchain swapchain;

		//The call a windowed app already makes; a virtual window simply has no native handle to acquire from.

		if(!c::Test_assert(t, "createSwapchain", dev.createSwapchain(window, false, swapchain, {}, e_rr)))
			goto clean;

		const c::UnifiedTexture tex = c::TextureRef_getUnifiedTexture(swapchain.handle(), NULL);

		c::Test_assert(t, "swapchainSized",
			tex.width == (c::U16) c::I32x2_x(window->size) && tex.height == (c::U16) c::I32x2_y(window->size)
		);

		//A ring, so frames in flight do not write the image a previous frame is still reading.
		//A virtual swapchain owns its images rather than taking them from a presentation engine, so it holds
		// EXACTLY framesInFlight of them, one per frame that can be in flight, with no driver to add slack.

		c::Test_assert(t, "swapchainRing", tex.images == RefPtr_data(deviceRef, c::GraphicsDevice)->framesInFlight);
		c::Test_assert(t, "swapchainOwnsImages", !!(tex.resource.flags & c::EGraphicsResourceFlag_InternalOwnsImages));

		//Always an RGBA order for a virtual swapchain, never a BGR one, so a pull reads back in natural channel
		// order and can be compared to a clear byte for byte rather than against both orderings.

		c::Test_assert(t, "swapchainRGBA8", tex.textureFormatId == c::ETextureFormatId_RGBA8);

		CommandList commandList;

		if(c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr))) {

			const c::ImageRange all = (c::ImageRange) { 0 };

			c::Test_assert(t, "begin", commandList.begin(true, e_rr));

			{
				CommandScope scope = commandList.scope({}, 1, {}, e_rr);
				c::Test_assert(t, "scope", (c::Bool) scope);

				c::Test_assert(t, "clear", scope.clearImagef(
					c::F32x4_create4(1, 0, 0, 1), all, swapchain.handle(), e_rr
				));

				c::Test_assert(t, "scopeEnd", scope.end(e_rr));
			}

			c::Test_assert(t, "end", commandList.end(e_rr));

			//Submitted WITH the swapchain, which is the path that acquires and presents for a physical one.
			//Twice, so the ring advances and the second frame lands on a different image than the first.

			c::Test_assert(t, "submit", dev.submit({ &commandList }, { &swapchain }, 0, 0, e_rr));
			c::Test_assert(t, "submitAgain", dev.submit({ &commandList }, { &swapchain }, 0, 0, e_rr));
			c::Test_assert(t, "wait", dev.wait(e_rr));

			//And the frame is readable, which is what makes this an output rather than a discard:
			// the same pull an encoder or an image writer would use.

			PullResult pulled = (PullResult) { 0, 0, 0, 0 };

			c::Test_assert(t, "queuePull", c::TextureRef_pullRegion(
				swapchain.handle(), 0, 0, 0, 0, 0, 0, 0, onPulled, &pulled, e_rr
			));

			CommandList emptyList;

			if(c::Test_assert(t, "createEmptyList", dev.createCommandList(c::KIBI, 16, 8, emptyList, true, e_rr))) {

				c::Test_assert(t, "beginEmpty", emptyList.begin(true, e_rr));
				c::Test_assert(t, "endEmpty", emptyList.end(e_rr));

				c::Test_assert(t, "submitPull", dev.submit({ &emptyList }, {}, 0, 0, e_rr));
				c::Test_assert(t, "waitPull", dev.wait(e_rr));
			}

			c::Test_assert(t, "pullCompleted", pulled.count == 1);
			c::Test_assert(t, "pullSized", pulled.length == (c::U64) c::I32x2_x(size) * c::I32x2_y(size) * 4);
			c::Test_assert(t, "pullUniform", !pulled.mismatches);

			//Against the CLEAR, rather than merely against itself: an image nothing ever wrote reads back uniform too,
			// and that is exactly the result this test exists to refuse.
			//A virtual swapchain is RGBA8, so opaque red reads back in exactly one channel order and the other is
			// no longer accepted, which is what owning the format rather than taking a surface's buys.

			c::Test_assert(t, "pullIsClearColor", pulled.first == 0xFF0000FFu);
		}

		//Before the window, since the swapchain holds a WEAK reference to it.

		swapchain.release();
	}

clean:

	if(windowRef)
		RefPtr_dec(&windowRef);

	c::WindowManager_free(&manager);
}

//A PHYSICAL swapchain acquires from and presents to a presentation engine.
//
//Nothing else in this suite creates one, so this is the only guard over the physical acquire and present paths.
//
//OPT IN, through OXC3_TEST_PRESENT=1, because presenting means a real window appears and takes the keyboard from
// whoever is typing.
//The rest of this suite is headless-safe and stays that way.
//Leaving the variable clear is not a way of detecting a headless machine either,
// since the backend falls back to the default display socket when WAYLAND_DISPLAY is unset and would open a window
// anyway.

extern "C" void Test_graphicsPhysicalSwapchain(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;
	using namespace oxc::gfxtest;

	c::Error *e_rr = &t->err;

	c::Test_setModule(t, "GraphicsDevice/physicalSwapchain");

	const c::C8 *present = getenv("OXC3_TEST_PRESENT");

	if(!present || present[0] == '0') {
		c::Test_print(t, "OXC3_TEST_PRESENT is unset, skipping the leg that opens a window and presents");
		return;
	}

	Device dev = Device::share(deviceRef);

	c::WindowManager manager = (c::WindowManager) { 0 };
	c::WindowRef *windowRef = NULL;

	const c::WindowManagerCallbacks managerCallbacks = (c::WindowManagerCallbacks) { 0 };

	if(!c::Test_assert(t, "createManager", c::WindowManager_create(managerCallbacks, 0, &manager, e_rr)))
		return;

	const c::WindowCallbacks windowCallbacks = (c::WindowCallbacks) { 0 };
	const c::CharString title = c::CharString_createRefCStrConst("Physical swapchain test");

	const c::I32x2 pos = c::I32x2_create2(0, 0);
	const c::I32x2 size = c::EResolution_get(c::EResolution_SD);
	const c::I32x2 maxSize = c::I32x2_create2(4096, 4096);

	//A refusal here is the expected result on a machine with no display,
	// so the error is swallowed rather than asserted on:
	// it is the difference between "cannot run" and "ran and failed".

	if(!c::WindowManager_createWindow(
		&manager, c::EWindowType_Physical, pos, size, size, maxSize,
		c::EWindowHint_None, title, windowCallbacks, c::EWindowFormat_AutoRGBA8, 0, &windowRef, NULL
	)) {
		c::Test_print(t, "No physical window available, skipping the presented swapchain leg");
		c::WindowManager_free(&manager);
		return;
	}

	{
		c::Window *window = RefPtr_data(windowRef, c::Window);

		Swapchain swapchain;

		if(!c::Test_assert(t, "createSwapchain", dev.createSwapchain(window, false, swapchain, {}, e_rr)))
			goto clean;

		{
			const c::UnifiedTexture tex = c::TextureRef_getUnifiedTexture(swapchain.handle(), NULL);

			//The mirror of the virtual assert: these images came from a presentation engine,
			// so the device does not own them and must not free them.

			c::Test_assert(t, "swapchainBorrowsImages", !(tex.resource.flags & c::EGraphicsResourceFlag_InternalOwnsImages));

			CommandList commandList;

			if(c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, commandList, true, e_rr))) {

				const c::ImageRange all = (c::ImageRange) { 0 };

				c::Test_assert(t, "begin", commandList.begin(true, e_rr));

				{
					CommandScope scope = commandList.scope({}, 1, {}, e_rr);
					c::Test_assert(t, "scope", (c::Bool) scope);

					c::Test_assert(t, "clear", scope.clearImagef(
						c::F32x4_create4(0, 0, 1, 1), all, swapchain.handle(), e_rr
					));

					c::Test_assert(t, "scopeEnd", scope.end(e_rr));
				}

				c::Test_assert(t, "end", commandList.end(e_rr));

				//Several frames, so acquire actually cycles the presentation engine's images rather than handing back
				// the same one, and the present semaphore is signalled and waited more than once.

				for(c::U32 i = 0; i < 4; ++i)
					c::Test_assert(t, "submit", dev.submit({ &commandList }, { &swapchain }, 0, 0, e_rr));

				c::Test_assert(t, "wait", dev.wait(e_rr));
			}
		}

		//Before the window, since the swapchain holds a WEAK reference to it.

		swapchain.release();
	}

clean:

	if(windowRef)
		RefPtr_dec(&windowRef);

	c::WindowManager_free(&manager);
}
