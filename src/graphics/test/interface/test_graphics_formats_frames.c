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

//graphics/test/interface/test_graphics_formats_frames.c
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

#include "types/test/test.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "platforms/logx.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "platforms/platform.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "test_graphics_shared.h"

//34. Per format upload -> readback round trip.

//A DeviceTexture pull writes into the resource's own cpuData.
//DevicePullCallback only reports completion and does not hand over a buffer, unlike the TexturePullCallback
// that RenderTextures use.
//So the callback just counts, and the comparison happens after the wait.

static void Test_pullCompleted(RefPtr *resource, void *context) {
	(void) resource;
	++*(U32*)context;
}

//Formats worth round tripping, deliberately not all 76.
//This runs per adapter, so the point is to cover the shapes that the upload and readback paths treat
// differently (channel counts, texel sizes, signedness, block compression) rather than every permutation.
//Compressed formats are included because their row pitch is computed from block size, which is exactly where
//a copy path gets it wrong.
//Every entry is one OxC3 requires of any adapter it runs on, so they are all expected to round trip; there is
//deliberately no support check here, because a format quietly losing support should fail rather than skip.

static const ETextureFormatId testRoundTripFormats[] = {
	ETextureFormatId_R8,      ETextureFormatId_RG8,     ETextureFormatId_RGBA8,   ETextureFormatId_BGRA8,
	ETextureFormatId_R8u,     ETextureFormatId_RGBA8u,  ETextureFormatId_R8i,     ETextureFormatId_RGBA8i,
	ETextureFormatId_R16,     ETextureFormatId_RGBA16,  ETextureFormatId_R16f,    ETextureFormatId_RGBA16f,
	ETextureFormatId_R32u,    ETextureFormatId_RGBA32u, ETextureFormatId_R32f,    ETextureFormatId_RGBA32f,
	ETextureFormatId_BGR10A2,
	ETextureFormatId_BC4,     ETextureFormatId_BC5,     ETextureFormatId_BC7
};

//Optional formats: present only when the adapter claims the matching capability bit, unlike the required
//table above.
//ASTC and BCn are alternatives rather than both being required (the enum comments say so: if one is absent
// the other has to be there), and BC4/BC5/BC7 already sit in the required table, so ASTC is the half that
// had no coverage at all.

typedef struct TestOptionalFormat {
	ETextureFormatId formatId;
	EGraphicsDataTypes dataType;
} TestOptionalFormat;

static const TestOptionalFormat testOptionalFormats[] = {
	{ ETextureFormatId_RGB32f,   EGraphicsDataTypes_RGB32f },
	{ ETextureFormatId_RGB32i,   EGraphicsDataTypes_RGB32i },
	{ ETextureFormatId_RGB32u,   EGraphicsDataTypes_RGB32u },
	{ ETextureFormatId_ASTC_4x4, EGraphicsDataTypes_ASTC   },
	{ ETextureFormatId_ASTC_8x8, EGraphicsDataTypes_ASTC   }
};

//One format's upload / readback / compare, shared by the required table and the optional one below.
//seed varies the byte pattern per call so a stale readback from a previous format can't masquerade as a pass.

static Bool Test_roundTripFormat(
	Test *t,
	GraphicsDeviceRef *deviceRef,
	const ListCommandListRef *lists,
	ETextureFormatId formatId,
	U16 w,
	U16 h,
	U64 seed
) {

	const Allocator *alloc = Platform_instance->alloc;

	const ETextureFormat format = ETextureFormatId_unpack[formatId];
	const U64 texSize = ETextureFormat_getSize(format, w, h, 1);

	//createTexture demands the initial data be exactly the format's size for these dimensions, so this
	//doubles as a check that getSize agrees with what the resource layer expects.

	Buffer src = Buffer_createNull();

	if(!Test_assert(t, "srcAlloc", Buffer_createUninitializedBytes(texSize, alloc, &src, &t->err)))
		return false;

	for(U64 j = 0; j < texSize; ++j)
		src.ptrNonConst[j] = (U8)((j * 7 + seed * 31 + 1) & 0xFF);

	Buffer upload = Buffer_createNull();
	Bool ok = Test_assert(t, "uploadCopy", Buffer_createCopy(src, alloc, &upload, &t->err));

	DeviceTextureRef *texture = NULL;
	const CharString name = CharString_createRefCStrConst("Format round trip");

	if(ok)
		ok = Test_assert(t, "create", GraphicsDeviceRef_createTexture(
			deviceRef, ETextureType_2D, formatId, EGraphicsResourceFlag_CPUBacked,
			w, h, 1, NULL, &name, &upload, &texture, &t->err
		));

	//createTexture takes ownership of upload on success.
	//On failure it is still ours.

	if(!ok)
		Buffer_free(&upload, alloc);

	U32 pulled = 0;

	if(ok)
		ok = Test_assert(t, "pull", DeviceTextureRef_pullRegion(
			texture, 0, 0, 0, 0, 0, 0, Test_pullCompleted, &pulled, &t->err
		));

	//The upload is issued by the same submit that services the pull, so one round trip is enough.

	if(ok)
		ok = Test_assert(t, "submit", GraphicsDeviceRef_submitCommands(
			deviceRef, lists, NULL, NULL, 0, 0, &t->err
		));

	if(ok)
		ok = Test_assert(t, "wait", GraphicsDeviceRef_wait(deviceRef, &t->err));

	Bool matched = false;

	if (ok) {

		Test_assert(t, "pullCalled", pulled == 1);

		//The readback lands in the texture's own cpuData.
		//Compare it byte for byte against what went up.

		const DeviceTexture *texPtr = DeviceTextureRef_ptr(texture);

		const Bool sameLen = Buffer_length(texPtr->cpuData) == texSize;
		Test_assert(t, "pullLength", sameLen);

		const Bool same = sameLen && Buffer_eq(
			Buffer_createRefConst(texPtr->cpuData.ptr, texSize),
			Buffer_createRefConst(src.ptr, texSize)
		);

		matched = Test_assert(t, "pullMatches", same);
	}

	RefPtr_dec(&texture);
	Buffer_free(&src, alloc);
	return matched;
}

void Test_graphicsFormatRoundTrip(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/formatRoundTrip");

	const Allocator *alloc = Platform_instance->alloc;

	//submitCommands refuses a submit that carries neither a command list nor a swapchain,
	// so an empty list is what lets these round trips ride a real frame.
	//The uploads and pulls themselves are queued on the device, not recorded into this list.

	CommandListRef *emptyList = NULL;
	ListCommandListRef lists = (ListCommandListRef) { 0 };

	if(!(
		Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
			deviceRef, 4 * KIBI, 64, 16, true, &emptyList, &t->err
		)) &&
		Test_assert(t, "beginList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err)) &&
		Test_assert(t, "endList", CommandListRef_end(emptyList, &t->err)) &&
		Test_assert(t, "listRef", ListCommandListRef_createRefConst(&emptyList, 1, &lists, &t->err))
	)) {
		RefPtr_dec(&emptyList);
		return;
	}

	//A multiple of 4 in both axes so every block compressed format covers whole blocks.

	const U16 w = 8, h = 8;

	const U64 formatCount = sizeof(testRoundTripFormats) / sizeof(testRoundTripFormats[0]);
	U32 roundTripped = 0;

	for (U64 i = 0; i < formatCount; ++i)
		if(Test_roundTripFormat(t, deviceRef, &lists, testRoundTripFormats[i], w, h, i))
			++roundTripped;

	//Every format in the table is required, so anything less than all of them is a failure rather than a skip.

	Test_assert(t, "roundTrippedAll", roundTripped == formatCount);

	Log_debugLnx("-- formatRoundTrip: %"PRIu32" / %"PRIu64" formats round tripped", roundTripped, formatCount);

	//Optional formats, each paired with the capability bit that promises it.
	//Unlike the table above these are skipped when the device doesn't claim them, which is a legitimate answer
	// rather than a regression.
	//When the bit IS claimed the round trip has to work,
	// and that is what turns the claim into something checkable instead of a bit reporting on itself.

	const GraphicsDeviceCapabilities caps = GraphicsDeviceRef_ptr(deviceRef)->info.capabilities;

	U32 optionalRun = 0, optionalSkipped = 0;

	for (U64 i = 0; i < sizeof(testOptionalFormats) / sizeof(testOptionalFormats[0]); ++i) {

		const TestOptionalFormat opt = testOptionalFormats[i];

		if(!(caps.dataTypes & opt.dataType)) {
			++optionalSkipped;
			continue;
		}

		if(Test_assert(t, "optionalFormat", Test_roundTripFormat(
			t, deviceRef, &lists, opt.formatId, w, h, formatCount + i
		)))
			++optionalRun;
	}

	Log_debugLnx(
		"-- formatRoundTrip: %"PRIu32" optional formats round tripped, %"PRIu32" not claimed by this adapter",
		optionalRun, optionalSkipped
	);

	ListCommandListRef_free(&lists, alloc);
	RefPtr_dec(&emptyList);
}

//35. Texture shape gates.
//These assert the resource layer's current limits rather than a feature.
//Each one is a TODO in UnifiedTexture_create.
//When one is lifted the matching assert here fails and points at what to update.

void Test_graphicsTextureShapes(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/textureShapes");

	const Allocator *alloc = Platform_instance->alloc;
	const CharString name = CharString_createRefCStrConst("Shape gate");

	//Helper shaped inline rather than as a function: each case wants a different size of initial data, and
	//createTexture insists the data length matches the format size exactly before it reaches the shape gates.

	const ETextureFormatId formatId = ETextureFormatId_RGBA8;
	const ETextureFormat format = ETextureFormatId_unpack[formatId];

	//3D is refused outright today, even though the descriptor layer already understands Texture3D views.

	{
		const U64 size = ETextureFormat_getSize(format, 4, 4, 4);
		Buffer dat = Buffer_createNull();

		if (Test_assert(t, "alloc3D", Buffer_createEmptyBytes(size, alloc, &dat, &t->err))) {

			DeviceTextureRef *tex = NULL;

			Test_assert(t, "refuse3D", !GraphicsDeviceRef_createTexture(
				deviceRef, ETextureType_3D, formatId, EGraphicsResourceFlag_CPUBacked,
				4, 4, 4, NULL, &name, &dat, &tex, NULL
			));

			Test_assert(t, "refuse3DNoLeak", !tex);
			Buffer_free(&dat, alloc);
		}
	}

	//Cube is equally refused, and for the same reason (only 2D passes the type gate).

	{
		const U64 size = ETextureFormat_getSize(format, 4, 4, 1);
		Buffer dat = Buffer_createNull();

		if (Test_assert(t, "allocCube", Buffer_createEmptyBytes(size, alloc, &dat, &t->err))) {

			DeviceTextureRef *tex = NULL;

			Test_assert(t, "refuseCube", !GraphicsDeviceRef_createTexture(
				deviceRef, ETextureType_Cube, formatId, EGraphicsResourceFlag_CPUBacked,
				4, 4, 1, NULL, &name, &dat, &tex, NULL
			));

			Test_assert(t, "refuseCubeNoLeak", !tex);
			Buffer_free(&dat, alloc);
		}
	}

	//length > 1 on a 2D texture is refused by the length gate before the type gate is reached, so this is a
	//distinct path from refuse3D even though both end in a rejection.

	{
		const U64 size = ETextureFormat_getSize(format, 4, 4, 2);
		Buffer dat = Buffer_createNull();

		if (Test_assert(t, "allocArray", Buffer_createEmptyBytes(size, alloc, &dat, &t->err))) {

			DeviceTextureRef *tex = NULL;

			Test_assert(t, "refuse2DArray", !GraphicsDeviceRef_createTexture(
				deviceRef, ETextureType_2D, formatId, EGraphicsResourceFlag_CPUBacked,
				4, 4, 2, NULL, &name, &dat, &tex, NULL
			));

			Test_assert(t, "refuse2DArrayNoLeak", !tex);
			Buffer_free(&dat, alloc);
		}
	}

	//A zero extent is refused on every axis; length is the one most likely to be left at 0 by a caller that
	//thinks of a 2D texture as having no depth.

	{
		DeviceTextureRef *tex = NULL;
		Buffer empty = Buffer_createNull();

		Test_assert(t, "refuseZeroLength", !GraphicsDeviceRef_createTexture(
			deviceRef, ETextureType_2D, formatId, EGraphicsResourceFlag_CPUBacked,
			4, 4, 0, NULL, &name, &empty, &tex, NULL
		));

		Test_assert(t, "refuseZeroWidth", !GraphicsDeviceRef_createTexture(
			deviceRef, ETextureType_2D, formatId, EGraphicsResourceFlag_CPUBacked,
			0, 4, 1, NULL, &name, &empty, &tex, NULL
		));

		Test_assert(t, "refuseZeroNoLeak", !tex);
	}

	//Above the documented limits of 16384 / 16384 / 256.
	//It uses no initial data, so the size check can't be what rejects it; the extent gate is.

	{
		DeviceTextureRef *tex = NULL;
		Buffer empty = Buffer_createNull();

		Test_assert(t, "refuseTooWide", !GraphicsDeviceRef_createTexture(
			deviceRef, ETextureType_2D, formatId, EGraphicsResourceFlag_CPUBacked,
			16385, 4, 1, NULL, &name, &empty, &tex, NULL
		));

		Test_assert(t, "refuseTooWideNoLeak", !tex);
	}
}

//36. Frames in flight.
//The other modules submit once or twice; this one cycles the ring several times over so the per frame
//resources (command allocators, the globals buffer, the staging and readback rings, resourcesInFlight) are
//reused rather than touched once.

void Test_graphicsFramesInFlight(Test *t, GraphicsDeviceRef *deviceRef) {

	Test_setModule(t, "GraphicsDevice/framesInFlight");

	const Allocator *alloc = Platform_instance->alloc;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	//Read the live count rather than assuming MAX_FRAMES_IN_FLIGHT: it is 2 on android and 3 elsewhere.

	const U8 fif = device->framesInFlight;

	Test_assert(t, "framesInFlightSane", fif >= 2 && fif <= MAX_FRAMES_IN_FLIGHT);

	//As in formatRoundTrip: a submit needs to carry something, so every frame below rides this empty list.

	CommandListRef *emptyList = NULL;
	ListCommandListRef lists = (ListCommandListRef) { 0 };

	if(!(
		Test_assert(t, "createList", GraphicsDeviceRef_createCommandList(
			deviceRef, 4 * KIBI, 64, 16, true, &emptyList, &t->err
		)) &&
		Test_assert(t, "beginList", CommandListRef_begin(emptyList, true, U64_MAX, &t->err)) &&
		Test_assert(t, "endList", CommandListRef_end(emptyList, &t->err)) &&
		Test_assert(t, "listRef", ListCommandListRef_createRefConst(&emptyList, 1, &lists, &t->err))
	)) {
		RefPtr_dec(&emptyList);
		return;
	}

	const U64 startSubmit = device->submitId;

	//Enough submits to wrap the ring more than twice, so a frame index is reused while an earlier one is
	//still being retired.

	const U32 submits = (U32) fif * 3;

	for (U32 i = 0; i < submits; ++i) {

		if(!Test_assert(t, "cycleSubmit", GraphicsDeviceRef_submitCommands(
			deviceRef, &lists, NULL, NULL, 0, 0, &t->err
		)))
			break;
	}

	Test_assert(t, "cycleWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

	//submitId advances once per submit, and fifId must land back inside the ring.

	Test_assert(t, "submitIdAdvanced", device->submitId == startSubmit + submits);
	Test_assert(t, "fifIdInRange", device->fifId < fif);

	//Uploads across consecutive frames: each iteration creates a CPU backed texture with a distinct pattern
	//and pulls it back in the same frame, so the staging and readback rings are exercised on every frame
	//index rather than just the first.

	const CharString name = CharString_createRefCStrConst("Frame ring texture");
	U32 verified = 0;

	for (U32 i = 0; i < (U32) fif * 2; ++i) {

		const U32 pattern = 0xA5000000u | i;

		Buffer dat = Buffer_createNull();

		if(!Test_assert(t, "ringAlloc", Buffer_createUninitializedBytes(4 * 4 * 4, alloc, &dat, &t->err)))
			break;

		for(U64 j = 0; j < 4 * 4; ++j)
			((U32*)dat.ptrNonConst)[j] = pattern;

		DeviceTextureRef *tex = NULL;

		Bool ok = Test_assert(t, "ringCreate", GraphicsDeviceRef_createTexture(
			deviceRef, ETextureType_2D, ETextureFormatId_RGBA8, EGraphicsResourceFlag_CPUBacked,
			4, 4, 1, NULL, &name, &dat, &tex, &t->err
		));

		if(!ok) {
			Buffer_free(&dat, alloc);
			break;
		}

		U32 pulled = 0;

		ok = Test_assert(t, "ringPull", DeviceTextureRef_pullRegion(
			tex, 0, 0, 0, 0, 0, 0, Test_pullCompleted, &pulled, &t->err
		));

		if(ok)
			ok = Test_assert(t, "ringSubmit", GraphicsDeviceRef_submitCommands(
				deviceRef, &lists, NULL, NULL, 0, 0, &t->err
			));

		if(ok)
			ok = Test_assert(t, "ringWait", GraphicsDeviceRef_wait(deviceRef, &t->err));

		//Verify from cpuData rather than from the callback: the pattern proves this frame's own upload came
		//back, not a leftover from the previous frame index.

		if (ok && pulled == 1) {

			const DeviceTexture *texPtr = DeviceTextureRef_ptr(tex);

			if(Buffer_length(texPtr->cpuData) >= sizeof(U32) && ((const U32*)texPtr->cpuData.ptr)[0] == pattern)
				++verified;
		}

		RefPtr_dec(&tex);
	}

	Test_assert(t, "ringRoundTrips", verified == (U32) fif * 2);

	//Readback reservations are idempotent in the shrinking direction and reject a null device; doing this
	//after the ring has wrapped means the reserve hits an already populated readback buffer.

	Test_assert(t, "reserveGrow", GraphicsDeviceRef_reserveReadback(deviceRef, 64 * KIBI, &t->err));
	Test_assert(t, "reserveNoShrink", GraphicsDeviceRef_reserveReadback(deviceRef, 0, &t->err));
	Test_assert(t, "reserveNullDevice", !GraphicsDeviceRef_reserveReadback(NULL, 0, NULL));

	//The ring must be drained by the wait above; nothing should still be pending.

	Test_assert(t, "noPendingPulls", !device->pendingPulls.length);

	ListCommandListRef_free(&lists, alloc);
	RefPtr_dec(&emptyList);
}
