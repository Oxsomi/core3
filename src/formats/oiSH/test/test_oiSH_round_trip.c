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

//formats/oiSH/test/test_oiSH_round_trip.c

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

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "csMain", false);
	SHBinaryInfo infoCopy = info;

	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("csMain");
	e.stage  = EGfxPipelineStage_Compute;
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

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst("cs");
	e.stage = EGfxPipelineStage_Compute;
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

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
	info.identifier.extensions = ESHExtension_F64 | ESHExtension_I64;
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst("cs");
	e.stage = EGfxPipelineStage_Compute;
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

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("cs");
	e.stage  = EGfxPipelineStage_Compute;
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

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_RaygenExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	//raygen (no payload)

	SHEntry rg = (SHEntry) { 0 };
	rg.name = CharString_createRefCStrConst("rgen");
	rg.stage = EGfxPipelineStage_RaygenExt;
	U16 raygenId = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&raygenId, 1, &rg.binaryIds, &t->err));

	SHEntry e = rg;
	Test_assert(t, "addRaygen", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	//miss: payloadSize = 64

	SHEntry rm = (SHEntry) { 0 };
	rm.name = CharString_createRefCStrConst("rmiss");
	rm.stage = EGfxPipelineStage_MissExt;
	rm.payloadSize = 64;
	U16 missId = 0;
	Test_assert(t, "createRef(0)", ListU16_createRefConst(&missId, 1, &rm.binaryIds, &t->err));

	e = rm;
	Test_assert(t, "addMiss", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	//closesthit: payloadSize = 32, intersectionSize = 16

	SHEntry rh = (SHEntry) { 0 };
	rh.name = CharString_createRefCStrConst("rchit");
	rh.stage = EGfxPipelineStage_ClosestHitExt;
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

	const struct { EGfxPipelineStage stage; const C8 *name; } cases[3] = {
		{ EGfxPipelineStage_Compute, "csMain" },
		{ EGfxPipelineStage_Vertex,  "vsMain" },
		{ EGfxPipelineStage_Pixel,   "psMain" }
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
		if (cases[i].stage == EGfxPipelineStage_Compute)
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

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	CharString sampName = CharString_createRefCStrConst("gSampler");
	GfxBindings sampB = makeDualBinding(0, 0, 0);
	Test_assert(t, "add sampler",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x3, false, &sampName, NULL, sampB, t->alloc, &t->err)
	);

	SHRegister sampReg;

	if (info.registers.length)
		sampReg = info.registers.ptr[0].reg;

	CharString texName = CharString_createRefCStrConst("gTexture");
	GfxBindings texB = makeDualBinding(0, 1, 0);
	Test_assert(t, "add texture",
		ListSHRegisterRuntime_addTexture(
			&info.registers, ESHTextureType_Texture2D, false, false, 0x3,
			EGfxTexturePrimitive_Float | EGfxTexturePrimitive_Component4,
			&texName, NULL, texB, t->alloc, &t->err
		)
	);

	SHRegister texReg;

	if (info.registers.length == 2)
		texReg = info.registers.ptr[1].reg;

	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("cs");
	e.stage  = EGfxPipelineStage_Compute;
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
void Test_SHFileRoundTripUniforms(Test *t) {

	Test_setModule(t, "SHFile round-trip: uniforms (name + type + value) survive");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	//Build two uniforms: a float scalar and a U32

	SHUniformRuntime u0 = (SHUniformRuntime) {
		.name        = CharString_createRefCStrConst("uScale"),
		.typeIdShort = ETypeId_toShortId((TypeId) ETypeId_F32),
		.dataOffset  = 0
	};

	SHUniformRuntime u1 = (SHUniformRuntime) {
		.name        = CharString_createRefCStrConst("uCount"),
		.typeIdShort = ETypeId_toShortId((TypeId) ETypeId_U32),
		.dataOffset  = 4
	};

	Test_assert(t, "pushBack",    ListSHUniformRuntime_pushBack(&info.identifier.uniforms, u0, t->alloc, &t->err));
	Test_assert(t, "pushBack(0)", ListSHUniformRuntime_pushBack(&info.identifier.uniforms, u1, t->alloc, &t->err));
	Test_assert(t, "resize",      ListU8_resize(&info.identifier.uniformData, 8, t->alloc, &t->err));

	F32 scale = 2.5f;
	U32 count = 42;
	Buffer_memcpy(
		Buffer_createRef(info.identifier.uniformData.ptrNonConst,     4),
		Buffer_createRefConst(&scale, 4)
	);
	Buffer_memcpy(
		Buffer_createRef(info.identifier.uniformData.ptrNonConst + 4, 4),
		Buffer_createRefConst(&count, 4)
	);

	U16 binId = 0;
	Test_assert(t, "addBinary",  SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	Test_assert(t, "addEntry",   addComputeEntry(&sh, "cs", 8, 8, 1, t, false, &binId));
	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "1 binary", rt.binaries.length == 1);

	if (rt.binaries.length == 1) {

		ListSHUniformRuntime rtu = rt.binaries.ptr[0].identifier.uniforms;
		Test_assert(t, "2 uniforms", rtu.length == 2);

		if (rtu.length == 2) {

			Test_assert(t, "u0 name",  cmpstr(rtu.ptr[0].name.ptr, "uScale"));
			Test_assert(t, "u1 name",  cmpstr(rtu.ptr[1].name.ptr, "uCount"));
			Test_assert(t, "u0 typeId eq", rtu.ptr[0].typeIdShort == u0.typeIdShort);
			Test_assert(t, "u1 typeId eq", rtu.ptr[1].typeIdShort == u1.typeIdShort);

			ListU8 ud = rt.binaries.ptr[0].identifier.uniformData;
			Test_assert(t, "uniformData length", ud.length == 8);

			if (ud.length == 8) {
				F32 rtScale = 0;
				U32 rtCount = 0;
				Buffer_memcpy(Buffer_createRef(&rtScale, 4), Buffer_createRefConst(ud.ptr,     4));
				Buffer_memcpy(Buffer_createRef(&rtCount, 4), Buffer_createRefConst(ud.ptr + 4, 4));
				Test_assert(t, "uScale value preserved", rtScale == 2.5f);
				Test_assert(t, "uCount value preserved", rtCount == 42);
			}
		}
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripDefines(Test *t) {

	Test_setModule(t, "SHFile round-trip: defines (name = value pairs) survive");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	CharString defName0  = CharString_createRefCStrConst("ENABLE_SHADOWS");
	CharString defVal0   = CharString_createRefCStrConst("1");
	CharString defName1  = CharString_createRefCStrConst("MAX_LIGHTS");
	CharString defVal1   = CharString_createRefCStrConst("64");

	Test_assert(t, "pushBack",    ListCharString_pushBack(&info.identifier.defines, defName0, t->alloc, &t->err));
	Test_assert(t, "pushBack(0)", ListCharString_pushBack(&info.identifier.defines, defVal0,  t->alloc, &t->err));
	Test_assert(t, "pushBack(1)", ListCharString_pushBack(&info.identifier.defines, defName1, t->alloc, &t->err));
	Test_assert(t, "pushBack(2)", ListCharString_pushBack(&info.identifier.defines, defVal1,  t->alloc, &t->err));

	U16 binId = 0;
	Test_assert(t, "addBinary",  SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	Test_assert(t, "addEntry",   addComputeEntry(&sh, "cs", 8, 8, 1, t, false, &binId));
	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "1 binary", rt.binaries.length == 1);

	if (rt.binaries.length == 1) {

		ListCharString rtd = rt.binaries.ptr[0].identifier.defines;
		Test_assert(t, "4 define strings (2 pairs)", rtd.length == 4);

		if (rtd.length == 4) {
			Test_assert(t, "define name 0",  cmpstr(rtd.ptr[0].ptr, "ENABLE_SHADOWS"));
			Test_assert(t, "define value 0", cmpstr(rtd.ptr[1].ptr, "1"));
			Test_assert(t, "define name 1",  cmpstr(rtd.ptr[2].ptr, "MAX_LIGHTS"));
			Test_assert(t, "define value 1", cmpstr(rtd.ptr[3].ptr, "64"));
		}
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripArrayRegisters(Test *t) {

	Test_setModule(t, "SHFile round-trip: array-dimensioned registers survive");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	//1-D array [4], triggers the "small array" path (arrayDimOrId = dim directly)

	U32 dims1D[1] = { 4 };
	ListU32 arr1D = (ListU32) { 0 };
	Test_assert(t, "createRef", ListU32_createRefConst(dims1D, 1, &arr1D, &t->err));

	CharString tex1DName = CharString_createRefCStrConst("gTexArray");
	GfxBindings tex1DB = makeDualBinding(0, 0, 0);

	Test_assert(t, "add tex[4]",
		ListSHRegisterRuntime_addTexture(
			&info.registers, ESHTextureType_Texture2D, false, false, 0x3,
			EGfxTexturePrimitive_Float | EGfxTexturePrimitive_Component4,
			&tex1DName, &arr1D, tex1DB, t->alloc, &t->err
		)
	);

	//2-D array [3][3], triggers the large-array-dim path

	U32 dims2D[2] = { 3, 3 };
	ListU32 arr2D = (ListU32) { 0 };
	Test_assert(t, "createRef(0)", ListU32_createRefConst(dims2D, 2, &arr2D, &t->err));

	CharString tex2DName = CharString_createRefCStrConst("gTex2DArray");
	GfxBindings tex2DB = makeDualBinding(0, 4, 4);

	Test_assert(t, "add tex[3][3]",
		ListSHRegisterRuntime_addTexture(
			&info.registers, ESHTextureType_Texture2D, false, false, 0x3,
			EGfxTexturePrimitive_Float | EGfxTexturePrimitive_Component4,
			&tex2DName, &arr2D, tex2DB, t->alloc, &t->err
		)
	);

	U16 binId = 0;
	Test_assert(t, "addBinary",  SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	Test_assert(t, "addEntry",   addComputeEntry(&sh, "cs", 8, 8, 1, t, false, &binId));
	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "1 binary", rt.binaries.length == 1);

	if (rt.binaries.length == 1) {

		ListSHRegisterRuntime regs = rt.binaries.ptr[0].registers;
		Test_assert(t, "2 registers", regs.length == 2);

		if (regs.length == 2) {

			Test_assert(t, "reg[0] name",          cmpstr(regs.ptr[0].name.ptr, "gTexArray"));
			Test_assert(t, "reg[0] arrays.length", regs.ptr[0].arrays.length == 1);

			if (regs.ptr[0].arrays.length == 1)
				Test_assert(t, "reg[0] dim[0] == 4", regs.ptr[0].arrays.ptr[0] == 4);

			Test_assert(t, "reg[1] name",          cmpstr(regs.ptr[1].name.ptr, "gTex2DArray"));
			Test_assert(t, "reg[1] arrays.length", regs.ptr[1].arrays.length == 2);

			if (regs.ptr[1].arrays.length == 2) {
				Test_assert(t, "reg[1] dim[0] == 3", regs.ptr[1].arrays.ptr[0] == 3);
				Test_assert(t, "reg[1] dim[1] == 3", regs.ptr[1].arrays.ptr[1] == 3);
			}
		}
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

void Test_SHFileRoundTripSemanticNames(Test *t) {

	Test_setModule(t, "SHFile round-trip: input/output semantic names survive");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Vertex, "vsMain", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	//Vertex entry: 2 inputs (POSITION, TEXCOORD0), 1 output (SV_POSITION)

	SHEntry e = (SHEntry) { 0 };
	e.name  = CharString_createRefCStrConst("vsMain");
	e.stage = EGfxPipelineStage_Vertex;

	//inputs[0] = float4 (POSITION), inputs[1] = float2 (TEXCOORD)

	e.inputs[0] = ESBType_F32x4;
	e.inputs[1] = ESBType_F32x2;
	e.outputs[0] = ESBType_F32x4;

	CharString semPOSITION  = CharString_createRefCStrConst("POSITION");
	CharString semTEXCOORD  = CharString_createRefCStrConst("TEXCOORD");
	CharString semSV_POS    = CharString_createRefCStrConst("SV_POSITION");

	Test_assert(t, "pushBack",   ListCharString_pushBack(&e.semanticNames, semPOSITION, t->alloc, &t->err));
	Test_assert(t, "pushBack(0)", ListCharString_pushBack(&e.semanticNames, semTEXCOORD, t->alloc, &t->err));
	Test_assert(t, "pushBack(1)", ListCharString_pushBack(&e.semanticNames, semSV_POS,   t->alloc, &t->err));

	e.uniqueInputSemantics = 2;

	//inputSemanticNames[slot] = (nameIndex+1) << 4 | semanticIndex
	e.inputSemanticNames[0] = (1 << 4) | 0;    //POSITION[0]
	e.inputSemanticNames[1] = (2 << 4) | 0;    //TEXCOORD[0]

	//outputSemanticNames[slot]: nameIndex is relative to uniqueInputSemantics
	e.outputSemanticNames[0] = (1 << 4) | 0;    //SV_POSITION[0]

	U16 bid = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &e.binaryIds, &t->err));
	Test_assert(t, "addEntry",  SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "1 entry", rt.entries.length == 1);

	if (rt.entries.length == 1) {

		SHEntry rte = rt.entries.ptr[0];
		Test_assert(t, "3 semanticNames", rte.semanticNames.length == 3);

		if (rte.semanticNames.length == 3) {
			Test_assert(t, "sem[0] POSITION",   cmpstr(rte.semanticNames.ptr[0].ptr, "POSITION"));
			Test_assert(t, "sem[1] TEXCOORD",   cmpstr(rte.semanticNames.ptr[1].ptr, "TEXCOORD"));
			Test_assert(t, "sem[2] SV_POSITION", cmpstr(rte.semanticNames.ptr[2].ptr, "SV_POSITION"));
		}

		Test_assert(t, "uniqueInputSemantics preserved", rte.uniqueInputSemantics == 2);
		Test_assert(t, "inputSemanticNames[0]",  rte.inputSemanticNames[0]  == ((1 << 4) | 0));
		Test_assert(t, "inputSemanticNames[1]",  rte.inputSemanticNames[1]  == ((2 << 4) | 0));
		Test_assert(t, "outputSemanticNames[0]", rte.outputSemanticNames[0] == ((1 << 4) | 0));
	}

	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}

static inline SBFile makeCBufferSBFileWithFields(Test *t) {

	SBFile sb = { 0 };

	if (!SBFile_create(ESBSettingsFlags_None, 8, t->alloc, &sb, &t->err)) {
		Test_assert(t, "makeTightSBFileWithFields: create", false);
		return sb;
	}

	CharString nameA = CharString_createRefCStrConst("time");
	CharString nameB = CharString_createRefCStrConst("pad");

	if (
		!SBFile_addVariableAsType(&sb, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err) ||
		!SBFile_addVariableAsType(&sb, &nameB, 4, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
	) {
		Test_assert(t, "makeTightSBFileWithFields: addVar", false);
		SBFile_free(&sb, t->alloc);
		return (SBFile) { 0 };
	}

	return sb;
}

void Test_SHFileRoundTripShaderBufferInRegister(Test *t) {

	Test_setModule(t, "SHFile round-trip: ConstantBuffer with oiSB survives");

	const RefPtrType msType = MemoryStream_makeType(t->alloc);
	SHFile sh = (SHFile) { 0 }, rt = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);

	SBFile cbSB = makeCBufferSBFileWithFields(t);

	if (!cbSB.vars.ptr)
		goto clean;

	U64 originalHash = cbSB.hash;
	U64 originalBufSize = cbSB.bufferSize;

	CharString cbName = CharString_createRefCStrConst("PerFrameCB");
	GfxBindings cbB = makeDualBinding(0, 0, 0);

	Test_assert(t, "add ConstantBuffer",
		ListSHRegisterRuntime_addBuffer(
			&info.registers,
			ESHBufferType_ConstantBuffer, false, 0x3,
			&cbName, NULL, &cbSB, cbB, t->alloc, &t->err
		)
	);

	Test_assert(t, "addBinary",  SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	U16 binId = 0;
	Test_assert(t, "addEntry",   addComputeEntry(&sh, "cs", 8, 8, 1, t, false, &binId));
	Test_assert(t, "round-trip", roundTrip(&sh, t->alloc, &msType, &rt, &t->err));

	Test_assert(t, "1 binary", rt.binaries.length == 1);

	if (rt.binaries.length == 1) {

		ListSHRegisterRuntime regs = rt.binaries.ptr[0].registers;
		Test_assert(t, "1 register", regs.length == 1);

		if (regs.length == 1) {

			SHRegisterRuntime reg = regs.ptr[0];

			Test_assert(t, "reg name",              cmpstr(reg.name.ptr, "PerFrameCB"));
			Test_assert(t, "shaderBuffer present",  reg.shaderBuffer.vars.ptr != NULL);
			Test_assert(t, "shaderBuffer hash",     reg.shaderBuffer.hash == originalHash);
			Test_assert(t, "shaderBuffer bufSize",  reg.shaderBuffer.bufferSize == originalBufSize);

			if (reg.shaderBuffer.vars.ptr) {

				Test_assert(t, "2 vars in CB", reg.shaderBuffer.vars.length == 2);

				if (reg.shaderBuffer.vars.length == 2) {
					Test_assert(t, "var0 type F32",   reg.shaderBuffer.vars.ptr[0].type   == ESBType_F32);
					Test_assert(t, "var0 offset 0",   reg.shaderBuffer.vars.ptr[0].offset == 0);
					Test_assert(t, "var1 type F32",   reg.shaderBuffer.vars.ptr[1].type   == ESBType_F32);
					Test_assert(t, "var1 offset 4",   reg.shaderBuffer.vars.ptr[1].offset == 4);
				}
			}
		}
	}

clean:
	SBFile_free(&cbSB, t->alloc);
	SHFile_free(&sh, t->alloc);
	SHFile_free(&rt, t->alloc);
}
