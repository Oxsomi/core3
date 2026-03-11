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

#include "test_oiSH_shared.h"
#include "types/container/memory_stream.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"

static inline Bool cmpstr(const C8 *a, const C8 *b) {
	const CharString str = CharString_createRefCStrConst(a);
	return CharString_equalsCStringSensitive(&str, b);
}

static inline Bool roundTrip(
	const SHFile *sh, const Allocator *alloc, const RefPtrType *msType, SHFile *out, Error *e_rr
) {
	Bool s_uccess = true;
	U64 writeOff = 0, readOff = 0;
	MemoryStreamRef *ms = NULL;

	gotoIfError3(clean, MemoryStream_create(0, EMemoryStreamFlags_WriteResize, msType, &ms, e_rr));

	StreamRef *ref = (StreamRef*)ms;
	gotoIfError3(clean, SHFile_write(ref, &writeOff, sh, alloc, e_rr));
	gotoIfError3(clean, SHFile_read(ref, &readOff, false, alloc, out, e_rr));

clean:
	RefPtr_dec(&ms);
	return s_uccess;
}

void Test_SHFileRoundTripBasic(Test *t) {

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "SHFile round-trip: minimal file");

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	SHBinaryInfo infoCopy = info;

	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("csMain");
	e.stage  = ESHPipelineStage_Compute;
	e.groupX = e.groupY = e.groupZ = 8;

	U16 bid = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &e.binaryIds, &t->err));

	SHEntry eCopy = e;

	Test_assert(t, "addEntry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "compilerVersion preserved", rt.compilerVersion == OXC3_VERSION);
	Test_assert(t, "sourceHash preserved",      rt.sourceHash      == 0xCAFE);
	Test_assert(t, "1 binary",                  rt.binaries.length == 1);

	if(rt.binaries.length == 1)
		Test_assert(t, "Binary contents",       SHBinaryInfo_equalsExact(&rt.binaries.ptr[0], &infoCopy));

	Test_assert(t, "1 entry",                   rt.entries.length  == 1);

	if (rt.entries.length == 1)
		Test_assert(t, "Entry contents",        SHEntry_equals(&rt.entries.ptr[0], &eCopy));

	Test_assert(t, "0 includes",                rt.includes.length == 0);

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripInclude(Test *t) {

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "SHFile round-trip: includes (path + CRC) preserved");

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst("cs");
	e.stage = ESHPipelineStage_Compute;
	e.groupX = e.groupY = e.groupZ = 4;
	U16 bid = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &e.binaryIds, &t->err));
	Test_assert(t, "addEntry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	SHInclude incA = { .relativePath = CharString_createRefCStrConst("a.hlsli"), .crc32c = 0xAAAA };
	SHInclude incB = { .relativePath = CharString_createRefCStrConst("b.hlsli"), .crc32c = 0xBBBB };
	Test_assert(t, "addIncA", SHFile_addInclude(&sh, &incA, t->alloc, &t->err));
	Test_assert(t, "addIncB", SHFile_addInclude(&sh, &incB, t->alloc, &t->err));

	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));
	Test_assert(t, "2 includes", rt.includes.length == 2);

	if (rt.includes.length == 2) {
		Test_assert(t, "include A crc preserved", rt.includes.ptr[0].crc32c == 0xAAAA);
		Test_assert(t, "include B crc preserved", rt.includes.ptr[1].crc32c == 0xBBBB);
		Test_assert(t, "include A path preserved", cmpstr(rt.includes.ptr[0].relativePath.ptr, "a.hlsli"));
		Test_assert(t, "include B path preserved", cmpstr(rt.includes.ptr[1].relativePath.ptr, "b.hlsli"));
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripBinaryExtensions(Test *t) {

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "SHFile round-trip: binary extension flags (F64|I64) preserved");

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	info.identifier.extensions = ESHExtension_F64 | ESHExtension_I64;
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst("cs");
	e.stage = ESHPipelineStage_Compute;
	e.groupX = e.groupY = e.groupZ = 8;
	U16 bid = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &e.binaryIds, &t->err));
	Test_assert(t, "addEntry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "F64 preserved", (rt.binaries.ptr[0].identifier.extensions & ESHExtension_F64) != 0);
	Test_assert(t, "I64 preserved", (rt.binaries.ptr[0].identifier.extensions & ESHExtension_I64) != 0);

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripComputeGroupSize(Test *t) {

	const RefPtrType msType = MemoryStream_makeType(t->alloc);

	Test_setModule(t, "SHFile round-trip: compute groupX/Y/Z preserved");

	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("cs");
	e.stage  = ESHPipelineStage_Compute;
	e.groupX = 16; e.groupY = 8; e.groupZ = 2;
	U16 bid = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &e.binaryIds, &t->err));
	Test_assert(t, "addEntry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "groupX preserved", rt.entries.ptr[0].groupX == 16);
	Test_assert(t, "groupY preserved", rt.entries.ptr[0].groupY == 8);
	Test_assert(t, "groupZ preserved", rt.entries.ptr[0].groupZ == 2);

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripRTStages(Test *t) {

	Test_setModule(t, "SHFile round-trip: RT payloadSize and intersectionSize preserved");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	//One lib binary shared by all RT stages

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_RaygenExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	//raygen (no payload)

	SHEntry rg = (SHEntry) { 0 };
	rg.name = CharString_createRefCStrConst("rgen");
	rg.stage = ESHPipelineStage_RaygenExt;
	U16 raygenId = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&raygenId, 1, &rg.binaryIds, &t->err));

	SHEntry e = rg;
	Test_assert(t, "addRaygen", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	//miss: payloadSize = 64

	SHEntry rm = (SHEntry) { 0 };
	rm.name = CharString_createRefCStrConst("rmiss");
	rm.stage = ESHPipelineStage_MissExt;
	rm.payloadSize = 64;
	U16 missId = 0;
	Test_assert(t, "createRef(0)", ListU16_createRefConst(&missId, 1, &rm.binaryIds, &t->err));

	e = rm;
	Test_assert(t, "addMiss", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	//closesthit: payloadSize = 32, intersectionSize = 16

	SHEntry rh = (SHEntry) { 0 };
	rh.name = CharString_createRefCStrConst("rchit");
	rh.stage = ESHPipelineStage_ClosestHitExt;
	rh.payloadSize = 32;
	rh.intersectionSize = 16;
	U16 hitId = 0;
	Test_assert(t, "createRef(1)", ListU16_createRefConst(&hitId, 1, &rh.binaryIds, &t->err));

	e = rh;
	Test_assert(t, "addCHit", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));
	Test_assert(t, "3 entries",  rt.entries.length == 3);

	if (rt.entries.length == 3) {
		Test_assert(t, "Entry contents 0", SHEntry_equals(&rt.entries.ptr[0], &rg));
		Test_assert(t, "Entry contents 1", SHEntry_equals(&rt.entries.ptr[1], &rm));
		Test_assert(t, "Entry contents 2", SHEntry_equals(&rt.entries.ptr[2], &rh));
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripMultipleBinaries(Test *t) {

	Test_setModule(t, "SHFile round-trip: 3 binaries and 3 entries all survive");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	const struct { ESHPipelineStage stage; const C8 *name; } cases[3] = {
		{ ESHPipelineStage_Compute, "csMain" },
		{ ESHPipelineStage_Vertex,  "vsMain" },
		{ ESHPipelineStage_Pixel,   "psMain" }
	};

	SHBinaryInfo copyBin[3];

	for (U8 i = 0; i < 3; ++i) {
		SHBinaryInfo info = makeBinaryInfo(cases[i].stage, cases[i].name, false);
		copyBin[i] = info;
		Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	}

	SHEntry copy[3];
	U16 ids[3];

	for (U8 i = 0; i < 3; ++i) {

		SHEntry e = (SHEntry) { 0 };
		e.name  = CharString_createRefCStrConst(cases[i].name);
		e.stage = cases[i].stage;
		if (cases[i].stage == ESHPipelineStage_Compute)
			e.groupX = e.groupY = e.groupZ = 8;
		ids[i] = i;

		Test_assert(t, "createRef", ListU16_createRefConst(&ids[i], 1, &e.binaryIds, &t->err));
		copy[i] = e;
		Test_assert(t, "addEntry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));
	}

	Test_assert(t, "round-trip",    roundTrip(&sh, t->alloc, &msType, &rt, &t->err));
	Test_assert(t, "3 binaries rt", rt.binaries.length == 3);
	Test_assert(t, "3 entries rt",  rt.entries.length  == 3);

	for(U64 i = 0; i < rt.binaries.length; ++i)
		Test_assert(t, "Binary contents", SHBinaryInfo_equalsExact(&rt.binaries.ptr[i], &copyBin[i]));

	for(U64 i = 0; i < rt.entries.length; ++i)
		Test_assert(t, "Entry contents", SHEntry_equals(&rt.entries.ptr[i], &copy[i]));

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripRegisterSurvives(Test *t) {

	Test_setModule(t, "SHFile round-trip: sampler and texture registers inside a binary survive");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);

	CharString sampName = CharString_createRefCStrConst("gSampler");
	SHBindings sampB = makeDualBinding(0, 0, 0);
	Test_assert(t, "add sampler",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x3, false, &sampName, NULL, sampB, t->alloc, &t->err)
	);

	SHRegister sampReg;

	if (info.registers.length)
		sampReg = info.registers.ptr[0].reg;

	CharString texName = CharString_createRefCStrConst("gTexture");
	SHBindings texB = makeDualBinding(0, 1, 0);
	Test_assert(t, "add texture",
		ListSHRegisterRuntime_addTexture(
			&info.registers, ESHTextureType_Texture2D, false, false, 0x3,
			ESHTexturePrimitive_Float | ESHTexturePrimitive_Component4,
			&texName, NULL, texB, t->alloc, &t->err
		)
	);

	SHRegister texReg;

	if (info.registers.length == 2)
		texReg = info.registers.ptr[1].reg;

	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("cs");
	e.stage  = ESHPipelineStage_Compute;
	e.groupX = e.groupY = e.groupZ = 8;
	U16 bid = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &e.binaryIds, &t->err));
	Test_assert(t, "addEntry", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	Test_assert(t, "round-trip",     roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "1 binary", rt.binaries.length == 1);

	if (rt.binaries.length == 1) {

		ListSHRegisterRuntime regs = rt.binaries.ptr[0].registers;
		Test_assert(t, "2 registers rt", regs.length == 2);

		if (regs.length == 2) {

			Test_assert(t, "sampler eq", Buffer_eq(
				Buffer_createRefConst(&regs.ptr[0].reg, sizeof(sampReg)), Buffer_createRefConst(&sampReg, sizeof(sampReg))
			));

			Test_assert(t, "sampler name", cmpstr(regs.ptr[0].name.ptr, "gSampler"));

			Test_assert(t, "texture eq", Buffer_eq(
				Buffer_createRefConst(&regs.ptr[1].reg, sizeof(texReg)), Buffer_createRefConst(&texReg, sizeof(texReg))
			));

			Test_assert(t, "texture name", cmpstr(regs.ptr[1].name.ptr, "gTexture"));
		}
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}
