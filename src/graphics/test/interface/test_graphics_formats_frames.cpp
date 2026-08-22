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

//graphics/test/interface/test_graphics_formats_frames.cpp
//
//Coverage group B: what a resource round trip and the frame ring do, rather than what a single shader computes.
//
//  34. GraphicsDevice/formatRoundTrip - upload known bytes per texture format, pull them back, compare
//  35. GraphicsDevice/textureShapes   - the 3D / array / mip creation gates, asserted where they are today
//  36. GraphicsDevice/framesInFlight  - cycling the frame ring harder than the other modules do
//
//Group B originally also called for 3D and texture array round trips, which are not reachable.
//UnifiedTexture_create refuses every type except 2D, refuses levels > 1, and refuses length > 1 for anything
// that isn't 3D, see src/graphics/generic/texture.c.
//Module 35 pins those refusals instead.
//So the day the resource layer grows 3D support the asserts flip and say so rather than silently passing.
//
//Written against the C++ layer (graphics/graphics.hpp), so every handle releases itself. Module 36 reads the
//frame ring's own counters, which are device internals the wrapper deliberately does not expose; those go
//through the C handle and are called out where they happen.

#include "test_graphics_shared.hpp"

//Log::debugLn is the C++ front for Log_debugLnx; the x macros name ELogOptions_NewLine unqualified and so
//cannot be reached through the c namespace.

#include "types/container/log.hpp"

namespace oxc { namespace c {
	#include "graphics/generic/device_texture.h"
}}

//34. Per format upload -> readback round trip.

//A DeviceTexture pull writes into the resource's own cpuData.
//DevicePullCallback only reports completion and does not hand over a buffer, unlike the TexturePullCallback
// that RenderTextures use.
//So the callback just counts, and the comparison happens after the wait.

namespace {

	using namespace oxc;

	void pullCompleted(c::RefPtr *resource, void *context) {
		(void) resource;
		++*(c::U32*)context;
	}

	//Formats worth round tripping, deliberately not all 76.
	//This runs per adapter, so the point is to cover the shapes that the upload and readback paths treat
	//differently (channel counts, texel sizes, signedness, block compression) rather than every permutation.
	//Compressed formats are included because their row pitch is computed from block size, which is exactly
	//where a copy path gets it wrong.
	//Every entry is one OxC3 requires of any adapter it runs on, so they are all expected to round trip;
	//there is deliberately no support check here, because a format quietly losing support should fail
	//rather than skip.

	const c::ETextureFormatId testRoundTripFormats[] = {
		c::ETextureFormatId_R8,   c::ETextureFormatId_RG8,     c::ETextureFormatId_RGBA8,  c::ETextureFormatId_BGRA8,
		c::ETextureFormatId_R8u,  c::ETextureFormatId_RGBA8u,  c::ETextureFormatId_R8i,    c::ETextureFormatId_RGBA8i,
		c::ETextureFormatId_R16,  c::ETextureFormatId_RGBA16,  c::ETextureFormatId_R16f,   c::ETextureFormatId_RGBA16f,
		c::ETextureFormatId_R32u, c::ETextureFormatId_RGBA32u, c::ETextureFormatId_R32f,   c::ETextureFormatId_RGBA32f,
		c::ETextureFormatId_BGR10A2,
		c::ETextureFormatId_BC4,  c::ETextureFormatId_BC5,     c::ETextureFormatId_BC7
	};

	//Optional formats: present only when the adapter claims the matching capability bit, unlike the required
	//table above.
	//ASTC and BCn are alternatives rather than both being required (the enum comments say so: if one is
	//absent the other has to be there), and BC4/BC5/BC7 already sit in the required table, so ASTC is the
	//half that had no coverage at all.

	struct TestOptionalFormat {
		c::ETextureFormatId formatId;
		c::EGraphicsDataTypes dataType;
	};

	const TestOptionalFormat testOptionalFormats[] = {
		{ c::ETextureFormatId_RGB32f,   c::EGraphicsDataTypes_RGB32f },
		{ c::ETextureFormatId_RGB32i,   c::EGraphicsDataTypes_RGB32i },
		{ c::ETextureFormatId_RGB32u,   c::EGraphicsDataTypes_RGB32u },
		{ c::ETextureFormatId_ASTC_4x4, c::EGraphicsDataTypes_ASTC   },
		{ c::ETextureFormatId_ASTC_8x8, c::EGraphicsDataTypes_ASTC   }
	};

	//One format's upload / readback / compare, shared by the required table and the optional one below.
	//seed varies the byte pattern per call so a stale readback from a previous format can't masquerade as
	//a pass.

	c::Bool roundTripFormat(
		c::Test *t,
		gfx::Device &dev,
		const gfx::CommandList &emptyList,
		c::ETextureFormatId formatId,
		c::U16 w,
		c::U16 h,
		c::U64 seed
	) {

		const c::Allocator *alloc = dev.alloc();

		const c::ETextureFormat format = c::ETextureFormatId_unpack[formatId];
		const c::U64 texSize = c::ETextureFormat_getSize(format, w, h, 1);

		//createTexture demands the initial data be exactly the format's size for these dimensions, so this
		//doubles as a check that getSize agrees with what the resource layer expects.

		c::Buffer src = c::Buffer_createNull();

		if(!c::Test_assert(t, "srcAlloc", c::Buffer_createUninitializedBytes(texSize, alloc, &src, &t->err)))
			return false;

		for(c::U64 j = 0; j < texSize; ++j)
			src.ptrNonConst[j] = (c::U8)((j * 7 + seed * 31 + 1) & 0xFF);

		c::Buffer upload = c::Buffer_createNull();
		c::Bool ok = c::Test_assert(t, "uploadCopy", c::Buffer_createCopy(src, alloc, &upload, &t->err));

		gfx::DeviceTexture texture;

		if(ok)
			ok = c::Test_assert(t, "create", dev.createTexture(
				c::ETextureType_2D, formatId, c::EGraphicsResourceFlag_CPUBacked,
				w, h, 1, "Format round trip", &upload, texture, nullptr, &t->err
			));

		//createTexture takes ownership of upload on success.
		//On failure it is still ours.

		if(!ok)
			c::Buffer_free(&upload, alloc);

		c::U32 pulled = 0;

		if(ok)
			ok = c::Test_assert(t, "pull", texture.pullRegion(0, 0, 0, 0, 0, 0, pullCompleted, &pulled, &t->err));

		//The upload is issued by the same submit that services the pull, so one round trip is enough.

		if(ok)
			ok = c::Test_assert(t, "submit", dev.submit({ &emptyList }, {}, 0, 0, &t->err));

		if(ok)
			ok = c::Test_assert(t, "wait", dev.wait(&t->err));

		c::Bool matched = false;

		if (ok) {

			c::Test_assert(t, "pullCalled", pulled == 1);

			//The readback lands in the texture's own cpuData.
			//Compare it byte for byte against what went up.

			const c::DeviceTexture *texPtr = texture.data();

			const c::Bool sameLen = c::Buffer_length(texPtr->cpuData) == texSize;
			c::Test_assert(t, "pullLength", sameLen);

			const c::Bool same = sameLen && c::Buffer_eq(
				c::Buffer_createRefConst(texPtr->cpuData.ptr, texSize),
				c::Buffer_createRefConst(src.ptr, texSize)
			);

			matched = c::Test_assert(t, "pullMatches", same);
		}

		c::Buffer_free(&src, alloc);
		return matched;
	}

	//submitCommands refuses a submit that carries neither a command list nor a swapchain, so an empty list
	//is what lets these round trips ride a real frame. The uploads and pulls themselves are queued on the
	//device, not recorded into this list.
	//
	//The C form asserted that a ListCommandListRef could be built from it as well; Device::submit builds
	//that list itself, so there is nothing left here for that assert to have checked.

	c::Bool makeEmptyList(c::Test *t, gfx::Device &dev, gfx::CommandList &emptyList) {
		return
			c::Test_assert(t, "createList", dev.createCommandList(4 * c::KIBI, 64, 16, emptyList, true, &t->err)) &&
			c::Test_assert(t, "beginList", emptyList.begin(true, &t->err)) &&
			c::Test_assert(t, "endList", emptyList.end(&t->err));
	}
}

extern "C" void Test_graphicsFormatRoundTrip(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Test_setModule(t, "GraphicsDevice/formatRoundTrip");

	Device dev = Device::share(deviceRef);

	CommandList emptyList;

	if(!makeEmptyList(t, dev, emptyList))
		return;

	//A multiple of 4 in both axes so every block compressed format covers whole blocks.

	const c::U16 w = 8, h = 8;

	const c::U64 formatCount = sizeof(testRoundTripFormats) / sizeof(testRoundTripFormats[0]);
	c::U32 roundTripped = 0;

	for (c::U64 i = 0; i < formatCount; ++i)
		if(roundTripFormat(t, dev, emptyList, testRoundTripFormats[i], w, h, i))
			++roundTripped;

	//Every format in the table is required, so anything less than all of them is a failure rather than a skip.

	c::Test_assert(t, "roundTrippedAll", roundTripped == formatCount);

	Log::debugLn(
		*dev.alloc(), "-- formatRoundTrip: %" PRIu32 " / %" PRIu64 " formats round tripped",
		roundTripped, formatCount
	);

	//Optional formats, each paired with the capability bit that promises it.
	//Unlike the table above these are skipped when the device doesn't claim them, which is a legitimate answer
	// rather than a regression.
	//When the bit IS claimed the round trip has to work,
	// and that is what turns the claim into something checkable instead of a bit reporting on itself.

	const c::GraphicsDeviceCapabilities caps = dev.info().capabilities;

	c::U32 optionalRun = 0, optionalSkipped = 0;

	for (c::U64 i = 0; i < sizeof(testOptionalFormats) / sizeof(testOptionalFormats[0]); ++i) {

		const TestOptionalFormat opt = testOptionalFormats[i];

		if(!(caps.dataTypes & opt.dataType)) {
			++optionalSkipped;
			continue;
		}

		if(c::Test_assert(t, "optionalFormat", roundTripFormat(
			t, dev, emptyList, opt.formatId, w, h, formatCount + i
		)))
			++optionalRun;
	}

	Log::debugLn(
		*dev.alloc(),
		"-- formatRoundTrip: %" PRIu32 " optional formats round tripped, %" PRIu32 " not claimed by this adapter",
		optionalRun, optionalSkipped
	);
}

//35. Texture shape gates.
//These assert the resource layer's current limits rather than a feature.
//Each one is a TODO in UnifiedTexture_create.
//When one is lifted the matching assert here fails and points at what to update.

extern "C" void Test_graphicsTextureShapes(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Test_setModule(t, "GraphicsDevice/textureShapes");

	Device dev = Device::share(deviceRef);
	const c::Allocator *alloc = dev.alloc();

	//Helper shaped inline rather than as a function: each case wants a different size of initial data, and
	//createTexture insists the data length matches the format size exactly before it reaches the shape gates.

	const c::ETextureFormatId formatId = c::ETextureFormatId_RGBA8;
	const c::ETextureFormat format = c::ETextureFormatId_unpack[formatId];

	//3D is refused outright today, even though the descriptor layer already understands Texture3D views.

	{
		const c::U64 size = c::ETextureFormat_getSize(format, 4, 4, 4);
		c::Buffer dat = c::Buffer_createNull();

		if (c::Test_assert(t, "alloc3D", c::Buffer_createEmptyBytes(size, alloc, &dat, &t->err))) {

			DeviceTexture tex;

			c::Test_assert(t, "refuse3D", !dev.createTexture(
				c::ETextureType_3D, formatId, c::EGraphicsResourceFlag_CPUBacked,
				4, 4, 4, "Shape gate", &dat, tex, nullptr, nullptr
			));

			c::Test_assert(t, "refuse3DNoLeak", !tex.valid());
			c::Buffer_free(&dat, alloc);
		}
	}

	//Cube is equally refused, and for the same reason (only 2D passes the type gate).

	{
		const c::U64 size = c::ETextureFormat_getSize(format, 4, 4, 1);
		c::Buffer dat = c::Buffer_createNull();

		if (c::Test_assert(t, "allocCube", c::Buffer_createEmptyBytes(size, alloc, &dat, &t->err))) {

			DeviceTexture tex;

			c::Test_assert(t, "refuseCube", !dev.createTexture(
				c::ETextureType_Cube, formatId, c::EGraphicsResourceFlag_CPUBacked,
				4, 4, 1, "Shape gate", &dat, tex, nullptr, nullptr
			));

			c::Test_assert(t, "refuseCubeNoLeak", !tex.valid());
			c::Buffer_free(&dat, alloc);
		}
	}

	//length > 1 on a 2D texture is refused by the length gate before the type gate is reached, so this is a
	//distinct path from refuse3D even though both end in a rejection.

	{
		const c::U64 size = c::ETextureFormat_getSize(format, 4, 4, 2);
		c::Buffer dat = c::Buffer_createNull();

		if (c::Test_assert(t, "allocArray", c::Buffer_createEmptyBytes(size, alloc, &dat, &t->err))) {

			DeviceTexture tex;

			c::Test_assert(t, "refuse2DArray", !dev.createTexture(
				c::ETextureType_2D, formatId, c::EGraphicsResourceFlag_CPUBacked,
				4, 4, 2, "Shape gate", &dat, tex, nullptr, nullptr
			));

			c::Test_assert(t, "refuse2DArrayNoLeak", !tex.valid());
			c::Buffer_free(&dat, alloc);
		}
	}

	//A zero extent is refused on every axis; length is the one most likely to be left at 0 by a caller that
	//thinks of a 2D texture as having no depth.

	{
		DeviceTexture tex;
		c::Buffer empty = c::Buffer_createNull();

		c::Test_assert(t, "refuseZeroLength", !dev.createTexture(
			c::ETextureType_2D, formatId, c::EGraphicsResourceFlag_CPUBacked,
			4, 4, 0, "Shape gate", &empty, tex, nullptr, nullptr
		));

		c::Test_assert(t, "refuseZeroWidth", !dev.createTexture(
			c::ETextureType_2D, formatId, c::EGraphicsResourceFlag_CPUBacked,
			0, 4, 1, "Shape gate", &empty, tex, nullptr, nullptr
		));

		c::Test_assert(t, "refuseZeroNoLeak", !tex.valid());
	}

	//Above the documented limits of 16384 / 16384 / 256.
	//It uses no initial data, so the size check can't be what rejects it; the extent gate is.

	{
		DeviceTexture tex;
		c::Buffer empty = c::Buffer_createNull();

		c::Test_assert(t, "refuseTooWide", !dev.createTexture(
			c::ETextureType_2D, formatId, c::EGraphicsResourceFlag_CPUBacked,
			16385, 4, 1, "Shape gate", &empty, tex, nullptr, nullptr
		));

		c::Test_assert(t, "refuseTooWideNoLeak", !tex.valid());
	}
}

//36. Frames in flight.
//The other modules submit once or twice; this one cycles the ring several times over so the per frame
//resources (command allocators, the globals buffer, the staging and readback rings, resourcesInFlight) are
//reused rather than touched once.

extern "C" void Test_graphicsFramesInFlight(oxc::c::Test *t, oxc::c::GraphicsDeviceRef *deviceRef) {

	using namespace oxc;
	using namespace oxc::gfx;

	c::Test_setModule(t, "GraphicsDevice/framesInFlight");

	Device dev = Device::share(deviceRef);
	const c::Allocator *alloc = dev.alloc();

	//submitId, fifId and pendingPulls are the frame ring's own bookkeeping, which is exactly what this
	//module is about and is not something graphics.hpp exposes, so they come off the device directly.

	const c::GraphicsDevice *device = c::deviceOf(dev.handle());

	//Read the live count rather than assuming MAX_FRAMES_IN_FLIGHT: it is 2 on android and 3 elsewhere.

	const c::U8 fif = dev.framesInFlight();

	c::Test_assert(t, "framesInFlightSane", fif >= 2 && fif <= MAX_FRAMES_IN_FLIGHT);

	//As in formatRoundTrip: a submit needs to carry something, so every frame below rides this empty list.

	CommandList emptyList;

	if(!makeEmptyList(t, dev, emptyList))
		return;

	const c::U64 startSubmit = device->submitId;

	//Enough submits to wrap the ring more than twice, so a frame index is reused while an earlier one is
	//still being retired.

	const c::U32 submits = (c::U32) fif * 3;

	for (c::U32 i = 0; i < submits; ++i)
		if(!c::Test_assert(t, "cycleSubmit", dev.submit({ &emptyList }, {}, 0, 0, &t->err)))
			break;

	c::Test_assert(t, "cycleWait", dev.wait(&t->err));

	//submitId advances once per submit, and fifId must land back inside the ring.

	c::Test_assert(t, "submitIdAdvanced", device->submitId == startSubmit + submits);
	c::Test_assert(t, "fifIdInRange", device->fifId < fif);

	//Uploads across consecutive frames: each iteration creates a CPU backed texture with a distinct pattern
	//and pulls it back in the same frame, so the staging and readback rings are exercised on every frame
	//index rather than just the first.

	c::U32 verified = 0;

	for (c::U32 i = 0; i < (c::U32) fif * 2; ++i) {

		const c::U32 pattern = 0xA5000000u | i;

		c::Buffer dat = c::Buffer_createNull();

		if(!c::Test_assert(t, "ringAlloc", c::Buffer_createUninitializedBytes(4 * 4 * 4, alloc, &dat, &t->err)))
			break;

		for(c::U64 j = 0; j < 4 * 4; ++j)
			((c::U32*)dat.ptrNonConst)[j] = pattern;

		DeviceTexture tex;

		c::Bool ok = c::Test_assert(t, "ringCreate", dev.createTexture(
			c::ETextureType_2D, c::ETextureFormatId_RGBA8, c::EGraphicsResourceFlag_CPUBacked,
			4, 4, 1, "Frame ring texture", &dat, tex, nullptr, &t->err
		));

		if(!ok) {
			c::Buffer_free(&dat, alloc);
			break;
		}

		c::U32 pulled = 0;

		ok = c::Test_assert(t, "ringPull", tex.pullRegion(0, 0, 0, 0, 0, 0, pullCompleted, &pulled, &t->err));

		if(ok)
			ok = c::Test_assert(t, "ringSubmit", dev.submit({ &emptyList }, {}, 0, 0, &t->err));

		if(ok)
			ok = c::Test_assert(t, "ringWait", dev.wait(&t->err));

		//Verify from cpuData rather than from the callback: the pattern proves this frame's own upload came
		//back, not a leftover from the previous frame index.

		if (ok && pulled == 1) {

			const c::DeviceTexture *texPtr = tex.data();

			if(
				c::Buffer_length(texPtr->cpuData) >= sizeof(c::U32) &&
				((const c::U32*)texPtr->cpuData.ptr)[0] == pattern
			)
				++verified;
		}
	}

	c::Test_assert(t, "ringRoundTrips", verified == (c::U32) fif * 2);

	//Readback reservations are idempotent in the shrinking direction and reject a null device; doing this
	//after the ring has wrapped means the reserve hits an already populated readback buffer.

	c::Test_assert(t, "reserveGrow", dev.reserveReadback(64 * c::KIBI, &t->err));
	c::Test_assert(t, "reserveNoShrink", dev.reserveReadback(0, &t->err));
	c::Test_assert(t, "reserveNullDevice", !c::GraphicsDeviceRef_reserveReadback(nullptr, 0, nullptr));

	//The ring must be drained by the wait above; nothing should still be pending.

	c::Test_assert(t, "noPendingPulls", !device->pendingPulls.length);
}
