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

//formats/oiPL/test/test_oiPL_main.c

#include "types/test/test.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/memory_stream.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include "formats/oiPL/pl_file.h"

static Bool buildLayout(Test *t, PLFile *pl) {

	if(!Test_assert(t, "create", PLFile_create(EPLSettingsFlags_None, t->alloc, pl, &t->err)))
		return false;

	CharString name = CharString_createRefCStrConst("verts");
	U32 nameId = U32_MAX;

	if(!Test_assert(t, "addString", PLFile_addString(pl, &name, t->alloc, &nameId, &t->err)))
		return false;

	PLDescriptorBinding unbounded = (PLDescriptorBinding) {
		.registerType = EGfxRegisterType_Texture2D,
		.count = 0,
		.bindings = GfxBindings_dummy(),
		.visibility = 1,
		.name24_source8 = PLDescriptorBinding_pack(PLDescriptorBinding_NAME_NONE, EPLSource_Supplied)
	};

	unbounded.bindings.arr[EGfxBinaryType_SPIRV] = (GfxBinding) { .space = 2, .binding = 0 };

	PLDescriptorBinding buffer = (PLDescriptorBinding) {
		.registerType = EGfxRegisterType_StructuredBuffer,
		.count = 1,
		.bindings = GfxBindings_dummy(),
		.visibility = 1,
		.name24_source8 = PLDescriptorBinding_pack(nameId, EPLSource_Derived)
	};

	buffer.bindings.arr[EGfxBinaryType_SPIRV] = (GfxBinding) { .space = 1, .binding = 3 };
	buffer.strideOrLength = 16;

	PLDescriptorBinding baked = (PLDescriptorBinding) {
		.registerType = EGfxRegisterType_Sampler,
		.count = 1,
		.bindings = GfxBindings_dummy(),
		.visibility = 1,
		.name24_source8 = PLDescriptorBinding_pack(PLDescriptorBinding_NAME_NONE, EPLSource_Supplied)
	};

	baked.bindings.arr[EGfxBinaryType_SPIRV] = (GfxBinding) { .space = 0, .binding = 2 };
	baked.samplerId = 1;               //1 + sampler pool index 0

	const PLSamplerInfo sampler = (PLSamplerInfo) { .filter = ESamplerFilterMode_Linear, .aniso = 8 };

	pl->pushConstant = (PLDescriptorBinding) {
		.registerType = EGfxRegisterType_PushConstants,
		.count = 1,
		.bindings = GfxBindings_dummy(),
		.visibility = 1,
		.name24_source8 = PLDescriptorBinding_pack(PLDescriptorBinding_NAME_NONE, EPLSource_Derived)
	};

	pl->pushConstant.strideOrLength = 32;
	pl->hasPushConstant = true;

	return
		Test_assert(t, "push0", ListPLDescriptorBinding_pushBack(&pl->bindings, unbounded, t->alloc, &t->err)) &&
		Test_assert(t, "push1", ListPLDescriptorBinding_pushBack(&pl->bindings, buffer, t->alloc, &t->err)) &&
		Test_assert(t, "push2", ListPLDescriptorBinding_pushBack(&pl->bindings, baked, t->alloc, &t->err)) &&
		Test_assert(t, "pushSampler", ListPLSamplerInfo_pushBack(&pl->samplers, sampler, t->alloc, &t->err));
}

//A parent format reserves through the NULL stream form, so it has to agree with the streamed write
//from any starting offset, including the unaligned one an embedded layout gets.

static void Test_PLSizeOnlyParity(Test *t) {

	Test_setModule(t, "PLFile: a NULL stream reserves exactly what a streamed write commits");

	PLFile pl = (PLFile) { 0 };
	StreamRef *stream = NULL;
	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	if(!buildLayout(t, &pl))
		goto clean;

	if(!Test_assert(t, "finalize", PLFile_finalize(&pl, t->alloc, &t->err)))
		goto clean;

	if(!Test_assert(t, "createStream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &stream, &t->err)))
		goto clean;

	U64 off = 0;

	if(!Test_assert(t, "firstWrite", PLFile_write(&pl, t->alloc, stream, &off, &t->err)))
		goto clean;

	const U64 mid = off;
	U64 sizeOnly = mid;

	Test_assert(t, "sizeOnly", PLFile_write(&pl, t->alloc, NULL, &sizeOnly, &t->err));
	Test_assert(t, "secondWrite", PLFile_write(&pl, t->alloc, stream, &off, &t->err));
	Test_assert(t, "parity", sizeOnly == off);

clean:
	RefPtr_dec(&stream);
	PLFile_free(&pl, t->alloc);
}

static void Test_PLRoundTrip(Test *t) {

	Test_setModule(t, "PLFile: round trip keeps rows, samplers, push constant and hash");

	PLFile pl = (PLFile) { 0 };

	if(!buildLayout(t, &pl))
		goto clean;

	Test_assert(t, "finalize", PLFile_finalize(&pl, t->alloc, &t->err));

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	StreamRef *stream = NULL;

	if(!Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &stream, &t->err)))
		goto clean;

	U64 off = 0;

	if (Test_assert(t, "write", PLFile_write(&pl, t->alloc, stream, &off, &t->err))) {

		PLFile reread = (PLFile) { 0 };
		U64 readOff = 0;

		if (Test_assert(t, "read", PLFile_read(stream, &readOff, false, t->alloc, &reread, &t->err))) {

			Test_assert(t, "rows", reread.bindings.length == 3);
			Test_assert(t, "unboundedSurvives", reread.bindings.ptr[0].count == 0);
			Test_assert(t, "sourceSurvives", PLDescriptorBinding_source(reread.bindings.ptr[1]) == EPLSource_Derived);
			Test_assert(t, "samplerSurvives", reread.samplers.length == 1 && reread.samplers.ptr[0].aniso == 8);
			Test_assert(t, "pcSurvives", reread.hasPushConstant && reread.pushConstant.strideOrLength == 32);
			Test_assert(t, "nameSurvives", reread.names.entryStrings.length == 1);
			Test_assert(t, "hashMatches", reread.hash == pl.hash);
		}

		PLFile_free(&reread, t->alloc);
	}

	RefPtr_dec(&stream);

clean:
	PLFile_free(&pl, t->alloc);
}

static void Test_PLCopy(Test *t) {

	Test_setModule(t, "PLFile: a copy is deep and hashes the same");

	PLFile pl = (PLFile) { 0 };
	PLFile copy = (PLFile) { 0 };

	if(!buildLayout(t, &pl))
		goto clean;

	Test_assert(t, "finalize", PLFile_finalize(&pl, t->alloc, &t->err));

	if (Test_assert(t, "copy", PLFile_copy(&pl, t->alloc, &copy, &t->err))) {

		Test_assert(t, "copyRows", copy.bindings.length == pl.bindings.length);
		Test_assert(t, "copyNames", copy.names.entryStrings.length == 1);
		Test_assert(t, "copyOwnsStrings", copy.names.entryStrings.ptr[0].ptr != pl.names.entryStrings.ptr[0].ptr);

		Test_assert(t, "refinalize", PLFile_finalize(&copy, t->alloc, &t->err));
		Test_assert(t, "copyHash", copy.hash == pl.hash);
	}

clean:
	PLFile_free(&copy, t->alloc);
	PLFile_free(&pl, t->alloc);
}

static Bool tamper(Test *t, const C8 *label, StreamRef *stream, U64 offset, U8 value) {

	MemoryStream *ms = RefPtr_data(stream, MemoryStream);

	if(offset >= Buffer_length(ms->data))
		return Test_assert(t, label, false);

	const U8 previous = ms->data.ptr[offset];
	ms->data.ptrNonConst[offset] = value;

	PLFile bad = (PLFile) { 0 };
	U64 readOff = 0;
	const Bool refused = !PLFile_read(stream, &readOff, false, t->alloc, &bad, NULL);

	PLFile_free(&bad, t->alloc);
	ms->data.ptrNonConst[offset] = previous;
	return Test_assert(t, label, refused);
}

static void Test_PLTamper(Test *t) {

	Test_setModule(t, "PLFile: tampered records are refused on read");

	PLFile pl = (PLFile) { 0 };
	StreamRef *stream = NULL;

	if(!buildLayout(t, &pl))
		goto clean;

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	if(!Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &msType, &stream, &t->err)))
		goto clean;

	U64 off = 0;

	if(!Test_assert(t, "write", PLFile_write(&pl, t->alloc, stream, &off, &t->err)))
		goto clean;

	{
		PLFile good = (PLFile) { 0 };
		U64 readOff = 0;
		Test_assert(t, "readsUntouched", PLFile_read(stream, &readOff, false, t->alloc, &good, &t->err));
		PLFile_free(&good, t->alloc);
	}

	const U64 H = 4;                          //PLHeader after the magic
	const U64 B = H + 4;                      //bindings[] (PLHeader is 4 bytes)

	tamper(t, "badVersion", stream, H + 0, 9);
	tamper(t, "unsupportedFlags", stream, H + 1, 0xFF);
	tamper(t, "bindingCountPastData", stream, H + 2, 0xFF);

	//The baked sampler row is the third (B + 80); samplerId at row offset 28 references sampler 1 + index

	tamper(t, "samplerRefOutOfBounds", stream, B + 80 + 28, 9);

	//The buffer row's packed name (low 24 bits at row offset 32) points past the single pooled string

	tamper(t, "nameOutOfBounds", stream, B + 40 + 32, 5);

	//The buffer row's source (the packed field's high byte) exceeds EPLSource_Count

	tamper(t, "sourceInvalid", stream, B + 40 + 35, 9);

	//The sampler pool sits after the three rows and the push constant row (B + 160)

	tamper(t, "samplerFilterInvalid", stream, B + 160 + 0, 0xFF);
	tamper(t, "samplerBoolInvalid", stream, B + 160 + 7, 2);

clean:
	RefPtr_dec(&stream);
	PLFile_free(&pl, t->alloc);
}

static void Test_PLValidate(Test *t) {

	Test_setModule(t, "PLFile: validate names duplicate rows, bad strides and unbounded arrays");

	PLFile pl = (PLFile) { 0 };
	ListCharString issues = (ListCharString) { 0 };

	if(!buildLayout(t, &pl))
		goto clean;

	//The built layout is clean apart from its deliberate unbounded array

	if(!Test_assert(t, "validate", PLFile_validate(&pl, t->alloc, &issues, &t->err)))
		goto clean;

	Test_assert(t, "unboundedIssue", issues.length == 1);
	ListCharString_freeUnderlying(&issues, t->alloc);

	//A zero stride and a same namespace collision each add their own line

	pl.bindings.ptrNonConst[1].strideOrLength = 0;

	const PLDescriptorBinding duplicate = pl.bindings.ptr[1];

	if(!Test_assert(t, "pushDuplicate", ListPLDescriptorBinding_pushBack(&pl.bindings, duplicate, t->alloc, &t->err)))
		goto clean;

	if(!Test_assert(t, "revalidate", PLFile_validate(&pl, t->alloc, &issues, &t->err)))
		goto clean;

	//Unbounded + two zero strides + the collision

	Test_assert(t, "namesEveryProblem", issues.length == 4);
	ListCharString_freeUnderlying(&issues, t->alloc);

	//A DXIL array occupies a register range, so an unbounded row reaches every register above it

	pl.bindings.ptrNonConst[0].bindings.arr[EGfxBinaryType_DXIL] = (GfxBinding) { .space = 0, .binding = 0 };
	pl.bindings.ptrNonConst[1].bindings.arr[EGfxBinaryType_DXIL] = (GfxBinding) { .space = 0, .binding = 5 };

	if(!Test_assert(t, "validateRanges", PLFile_validate(&pl, t->alloc, &issues, &t->err)))
		goto clean;

	Test_assert(t, "rangeCollision", issues.length == 5);

	//The write flag on a sampler is refused by the shared row rule, which the reader also runs

	pl.bindings.ptrNonConst[2].registerType |= EGfxRegisterType_IsWrite;
	Test_assert(t, "writeFlagRefused", !PLDescriptorBinding_validate(&pl.bindings.ptr[2], U64_MAX, U64_MAX, false, NULL));

	pl.bindings.ptrNonConst[2].registerType &= ~(U32)EGfxRegisterType_IsWrite;
	pl.bindings.ptrNonConst[2].bindings.arr[EGfxBinaryType_SPIRV].space = OXC3_RESERVED_SPACE;
	Test_assert(t, "reservedRefused", !PLDescriptorBinding_validate(&pl.bindings.ptr[2], U64_MAX, U64_MAX, false, NULL));
	Test_assert(t, "reservedExempt", PLDescriptorBinding_validate(&pl.bindings.ptr[2], U64_MAX, U64_MAX, true, NULL));

	PLDescriptorBinding half = pl.bindings.ptr[1];
	half.strideOrLength = 16;
	half.bindings.arr[EGfxBinaryType_SPIRV].space = U32_MAX;
	Test_assert(t, "halfPairRefused", !PLDescriptorBinding_validate(&half, U64_MAX, U64_MAX, false, NULL));

	half.bindings = GfxBindings_dummy();
	Test_assert(t, "absentEverywhereRefused", !PLDescriptorBinding_validate(&half, U64_MAX, U64_MAX, false, NULL));
	Test_assert(t, "pcNeedsNoPair", PLDescriptorBinding_validate(&pl.pushConstant, U64_MAX, U64_MAX, false, NULL));

	PLDescriptorBinding flags = pl.bindings.ptr[1];
	flags.strideOrLength = 16;
	flags.registerType |= 0x80000000;
	Test_assert(t, "unknownBitsRefused", !PLDescriptorBinding_validate(&flags, U64_MAX, U64_MAX, false, NULL));

	flags.registerType = EGfxRegisterType_StructuredBuffer | EGfxRegisterType_IsArray;
	Test_assert(t, "textureFlagOnBufferRefused", !PLDescriptorBinding_validate(&flags, U64_MAX, U64_MAX, false, NULL));

	//Only the whole pair is the absence sentinel; a lone U32_MAX member is a real, if odd, location

	const GfxBinding halfPair = (GfxBinding) { .space = U32_MAX, .binding = 3 };
	const GfxBinding absent = (GfxBinding) { .space = U32_MAX, .binding = U32_MAX };

	Test_assert(
		t, "halfPairStillCollides",
		GfxBinding_overlaps(
			halfPair, EGfxRegisterType_Texture2D, 1, halfPair, EGfxRegisterType_Texture2D, 1, EGfxBinaryType_SPIRV
		)
	);

	Test_assert(
		t, "absentNeverCollides",
		!GfxBinding_overlaps(
			absent, EGfxRegisterType_Texture2D, 1, absent, EGfxRegisterType_Texture2D, 1, EGfxBinaryType_SPIRV
		)
	);

clean:
	ListCharString_freeUnderlying(&issues, t->alloc);
	PLFile_free(&pl, t->alloc);
}

OXC3_TEST_MAIN(formats_oiPL) {

	const Allocator alloc = BasicAllocator_instance;

	Test t = (Test) { 0 };
	t.alloc = &alloc;

	Test_PLRoundTrip(&t);
	Test_PLSizeOnlyParity(&t);
	Test_PLCopy(&t);
	Test_PLTamper(&t);
	Test_PLValidate(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}
