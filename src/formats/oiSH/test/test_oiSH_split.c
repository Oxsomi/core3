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

//formats/oiSH/test/test_oiSH_split.c

#include "test_oiSH_shared.h"
#include "types/container/list_basic_types.h"
#include "types/base/string_read_helper.h"

static const U8 kDummyDXIL[5] = { 0x44, 0x58, 0x42, 0x43, 0x00 };

//A combine puts the registers back in whichever order the two halves supplied them, which is not necessarily the
// order they had before being split, so both are looked up by name rather than by position.

static const SHRegisterRuntime *Test_SHSplitFindRegister(const SHBinaryInfo *info, const CharString *name) {

	for(U64 i = 0; i < info->registers.length; ++i)
		if(CharString_equalsStringSensitive(&info->registers.ptr[i].name, name))
			return &info->registers.ptr[i];

	return NULL;
}

static Bool Test_SHSplitHasRegisterNamed(const SHBinaryInfo *info, const C8 *name) {
	const CharString str = CharString_createRefCStrConst(name);
	return Test_SHSplitFindRegister(info, &str) != NULL;
}

//Two registers are the same one when everything the format stores about them agrees.
//The bindings are compared for every binary type, which is what pins that a split keeps the binding belonging to
// the type it dropped.

static Bool Test_SHSplitRegisterEquals(const SHRegisterRuntime *a, const SHRegisterRuntime *b) {

	if(!a || !b)
		return false;

	if(a->reg.registerType != b->reg.registerType || a->reg.isUsedFlag != b->reg.isUsedFlag)
		return false;

	if(a->reg.arrayDimOrId != b->reg.arrayDimOrId)
		return false;

	for(U8 i = 0; i < EGfxBinaryType_Count; ++i)
		if(a->reg.bindings.arrU64[i] != b->reg.bindings.arrU64[i])
			return false;

	return true;
}

//One compute binary carrying both binary types, with a register bound by both, one bound by SPIRV alone and one
// bound by DXIL alone.
//That is the shape a merged oiSH has, down to the standalone DXIL sampler that SPIRV never had.

static Bool Test_SHSplitMakeMerged(Test *t, SHFile *sh) {

	if(!Test_SHFileCreate(t, sh))
		return false;

	SHBinaryInfo info = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("cs"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = EGfxPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
	};

	info.binaries[EGfxBinaryType_SPIRV] = Buffer_createRefConst(kDummySPIRV, sizeof(kDummySPIRV));
	info.binaries[EGfxBinaryType_DXIL]  = Buffer_createRefConst(kDummyDXIL,  sizeof(kDummyDXIL));

	CharString sharedName = CharString_createRefCStrConst("gShared");

	if(!ListSHRegisterRuntime_addSampler(
		&info.registers, 0x3, false, &sharedName, NULL, makeDualBinding(0, 0, 0), t->alloc, &t->err
	))
		return false;

	CharString spvName = CharString_createRefCStrConst("gSpvOnly");
	GfxBindings spvOnly = GfxBindings_dummy();
	spvOnly.arr[EGfxBinaryType_SPIRV] = (GfxBinding) { .binding = 1, .space = 0 };

	if(!ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &spvName, NULL, spvOnly, t->alloc, &t->err))
		return false;

	CharString dxilName = CharString_createRefCStrConst("gDxilOnly");
	GfxBindings dxilOnly = GfxBindings_dummy();
	dxilOnly.arr[EGfxBinaryType_DXIL] = (GfxBinding) { .binding = 1, .space = 0 };

	if(!ListSHRegisterRuntime_addSampler(&info.registers, 0x2, false, &dxilName, NULL, dxilOnly, t->alloc, &t->err))
		return false;

	if(!SHFile_addBinary(sh, &info, t->alloc, &t->err))
		return false;

	const U16 binId = 0;
	return addComputeEntry(sh, "cs", 8, 8, 1, t, false, &binId);
}

void Test_SHFileSplitNullGuards(Test *t) {

	Test_setModule(t, "SHFile split: null guards");

	SHFile sh = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	Test_assert(t, "null a",   !SHFile_split(NULL, EGfxBinaryType_SPIRV, t->alloc, &out, NULL));
	Test_assert(t, "null out", !SHFile_split(&sh,  EGfxBinaryType_SPIRV, t->alloc, NULL, NULL));
	Test_assert(t, "bad type", !SHFile_split(&sh,  EGfxBinaryType_Count, t->alloc, &out, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileSplitReflectionOnlyFiltersRegisters(Test *t) {

	Test_setModule(t, "SHFile split: a reflection only oiSH splits per register, keeping every binary");

	SHFile sh = (SHFile) { 0 }, spv = (SHFile) { 0 }, dxil = (SHFile) { 0 };

	Test_assert(t, "create", SHFile_create(
		ESHSettingsFlags_ReflectionOnly, OXC3_VERSION, 0xCAFE, t->alloc, &sh, &t->err
	));

	//A reflection only file is allowed to carry a binary with no compiled code at all

	SHBinaryInfo info = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("cs"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = EGfxPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
	};

	CharString sharedName = CharString_createRefCStrConst("gShared");

	Test_assert(t, "add shared", ListSHRegisterRuntime_addSampler(
		&info.registers, 0x3, false, &sharedName, NULL, makeDualBinding(0, 0, 0), t->alloc, &t->err
	));

	CharString spvName = CharString_createRefCStrConst("gSpvOnly");
	GfxBindings spvOnly = GfxBindings_dummy();
	spvOnly.arr[EGfxBinaryType_SPIRV] = (GfxBinding) { .binding = 1, .space = 0 };

	Test_assert(t, "add spv only", ListSHRegisterRuntime_addSampler(
		&info.registers, 0x1, false, &spvName, NULL, spvOnly, t->alloc, &t->err
	));

	CharString dxilName = CharString_createRefCStrConst("gDxilOnly");
	GfxBindings dxilOnly = GfxBindings_dummy();
	dxilOnly.arr[EGfxBinaryType_DXIL] = (GfxBinding) { .binding = 1, .space = 0 };

	Test_assert(t, "add dxil only", ListSHRegisterRuntime_addSampler(
		&info.registers, 0x2, false, &dxilName, NULL, dxilOnly, t->alloc, &t->err
	));

	Test_assert(t, "addBin", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	const U16 binId = 0;
	Test_assert(t, "addEntry", addComputeEntry(&sh, "cs", 8, 8, 1, t, false, &binId));

	Test_assert(t, "split spv",  SHFile_split(&sh, EGfxBinaryType_SPIRV, t->alloc, &spv,  &t->err));
	Test_assert(t, "split dxil", SHFile_split(&sh, EGfxBinaryType_DXIL,  t->alloc, &dxil, &t->err));

	//Which binary was for which backend went with the compiled code, so all of them stay

	Test_assert(t, "spv keeps the binary",  spv.binaries.length  == 1 && spv.entries.length  == 1);
	Test_assert(t, "dxil keeps the binary", dxil.binaries.length == 1 && dxil.entries.length == 1);

	Test_assert(t, "spv stays reflection only",  (spv.flags  & ESHSettingsFlags_ReflectionOnly) != 0);
	Test_assert(t, "dxil stays reflection only", (dxil.flags & ESHSettingsFlags_ReflectionOnly) != 0);

	//The registers do still know which backend bound them, which is the whole point of splitting one of these

	Test_assert(t, "spv register count",  spv.binaries.ptr[0].registers.length  == 2);
	Test_assert(t, "dxil register count", dxil.binaries.ptr[0].registers.length == 2);

	Test_assert(t, "spv keeps own",       Test_SHSplitHasRegisterNamed(&spv.binaries.ptr[0],  "gSpvOnly"));
	Test_assert(t, "spv drops dxil only", !Test_SHSplitHasRegisterNamed(&spv.binaries.ptr[0], "gDxilOnly"));

	Test_assert(t, "dxil keeps own",     Test_SHSplitHasRegisterNamed(&dxil.binaries.ptr[0],  "gDxilOnly"));
	Test_assert(t, "dxil drops spv only", !Test_SHSplitHasRegisterNamed(&dxil.binaries.ptr[0], "gSpvOnly"));

	SHFile_free(&sh, t->alloc);
	SHFile_free(&spv, t->alloc);
	SHFile_free(&dxil, t->alloc);
}

void Test_SHFileSplitMissingTypeRejected(Test *t) {

	Test_setModule(t, "SHFile split: asking for a type the file never carried fails");

	SHFile sh = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(EGfxPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBin", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	const U16 binId = 0;
	Test_assert(t, "addEntry", addComputeEntry(&sh, "cs", 8, 8, 1, t, false, &binId));

	Test_assert(t, "no DXIL to split off", !SHFile_split(&sh, EGfxBinaryType_DXIL, t->alloc, &out, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileSplitKeepsOnlyRequestedType(Test *t) {

	Test_setModule(t, "SHFile split: each half keeps its own binary and drops the other");

	SHFile sh = (SHFile) { 0 }, spv = (SHFile) { 0 }, dxil = (SHFile) { 0 };
	Test_assert(t, "merged input", Test_SHSplitMakeMerged(t, &sh));

	Test_assert(t, "split spv",  SHFile_split(&sh, EGfxBinaryType_SPIRV, t->alloc, &spv,  &t->err));
	Test_assert(t, "split dxil", SHFile_split(&sh, EGfxBinaryType_DXIL,  t->alloc, &dxil, &t->err));

	Test_assert(t, "spv binary count",  spv.binaries.length  == 1);
	Test_assert(t, "dxil binary count", dxil.binaries.length == 1);

	Test_assert(t, "spv keeps SPIRV",
		Buffer_length(spv.binaries.ptr[0].binaries[EGfxBinaryType_SPIRV]) == sizeof(kDummySPIRV)
	);

	Test_assert(t, "spv drops DXIL", !Buffer_length(spv.binaries.ptr[0].binaries[EGfxBinaryType_DXIL]));

	Test_assert(t, "dxil keeps DXIL",
		Buffer_length(dxil.binaries.ptr[0].binaries[EGfxBinaryType_DXIL]) == sizeof(kDummyDXIL)
	);

	Test_assert(t, "dxil drops SPIRV", !Buffer_length(dxil.binaries.ptr[0].binaries[EGfxBinaryType_SPIRV]));

	//Everything that isn't per binary type is carried over untouched

	Test_assert(t, "spv keeps flags",   spv.flags == sh.flags);
	Test_assert(t, "spv keeps version", spv.compilerVersion == sh.compilerVersion);
	Test_assert(t, "spv keeps source",  spv.sourceHash == sh.sourceHash);

	Test_assert(t, "spv keeps entry",  spv.entries.length  == 1);
	Test_assert(t, "dxil keeps entry", dxil.entries.length == 1);

	SHFile_free(&sh, t->alloc);
	SHFile_free(&spv, t->alloc);
	SHFile_free(&dxil, t->alloc);
}

void Test_SHFileSplitDropsUnboundRegisters(Test *t) {

	Test_setModule(t, "SHFile split: a register the other type bound alone is dropped");

	SHFile sh = (SHFile) { 0 }, spv = (SHFile) { 0 }, dxil = (SHFile) { 0 };
	Test_assert(t, "merged input", Test_SHSplitMakeMerged(t, &sh));

	Test_assert(t, "3 registers before", sh.binaries.ptr[0].registers.length == 3);

	Test_assert(t, "split spv",  SHFile_split(&sh, EGfxBinaryType_SPIRV, t->alloc, &spv,  &t->err));
	Test_assert(t, "split dxil", SHFile_split(&sh, EGfxBinaryType_DXIL,  t->alloc, &dxil, &t->err));

	const SHBinaryInfo *spvBin = &spv.binaries.ptr[0], *dxilBin = &dxil.binaries.ptr[0];

	Test_assert(t, "spv register count",  spvBin->registers.length  == 2);
	Test_assert(t, "dxil register count", dxilBin->registers.length == 2);

	Test_assert(t, "spv keeps shared",   Test_SHSplitHasRegisterNamed(spvBin,  "gShared"));
	Test_assert(t, "spv keeps own",      Test_SHSplitHasRegisterNamed(spvBin,  "gSpvOnly"));
	Test_assert(t, "spv drops dxil only", !Test_SHSplitHasRegisterNamed(spvBin, "gDxilOnly"));

	Test_assert(t, "dxil keeps shared",  Test_SHSplitHasRegisterNamed(dxilBin, "gShared"));
	Test_assert(t, "dxil keeps own",     Test_SHSplitHasRegisterNamed(dxilBin, "gDxilOnly"));
	Test_assert(t, "dxil drops spv only", !Test_SHSplitHasRegisterNamed(dxilBin, "gSpvOnly"));

	//A surviving register is carried over exactly, down to the binding of the type that was dropped, so the
	// reflection both halves gained from the merge stays put

	const CharString sharedName = CharString_createRefCStrConst("gShared");
	const SHRegisterRuntime *before = Test_SHSplitFindRegister(&sh.binaries.ptr[0], &sharedName);

	Test_assert(t, "shared register unchanged in spv",
		Test_SHSplitRegisterEquals(before, Test_SHSplitFindRegister(spvBin, &sharedName))
	);

	Test_assert(t, "shared register unchanged in dxil",
		Test_SHSplitRegisterEquals(before, Test_SHSplitFindRegister(dxilBin, &sharedName))
	);

	//The hash goes along with it, since a copy has to keep answering for the same register

	Test_assert(t, "shared register hash kept",
		before && Test_SHSplitFindRegister(spvBin, &sharedName)->hash == before->hash
	);

	SHFile_free(&sh, t->alloc);
	SHFile_free(&spv, t->alloc);
	SHFile_free(&dxil, t->alloc);
}

//A constant buffer sized shader buffer, which every CBV shaped register needs to carry

static SBFile Test_SHSplitMakeCBuffer(Test *t) {

	SBFile sb = (SBFile) { 0 };

	if(!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
		Test_assert(t, "cbuffer create", false);
		return sb;
	}

	CharString name = CharString_createRefCStrConst("time");

	if(!SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)) {
		Test_assert(t, "cbuffer addVar", false);
		SBFile_free(&sb, t->alloc);
		return (SBFile) { 0 };
	}

	return sb;
}

void Test_SHFileSplitKeepsSpirvPushConstant(Test *t) {

	Test_setModule(t, "SHFile split: a SPIRV push constant survives even though it has no binding");

	SHFile sh = (SHFile) { 0 }, spv = (SHFile) { 0 }, dxil = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("cs"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = EGfxPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
	};

	info.binaries[EGfxBinaryType_SPIRV] = Buffer_createRefConst(kDummySPIRV, sizeof(kDummySPIRV));
	info.binaries[EGfxBinaryType_DXIL]  = Buffer_createRefConst(kDummyDXIL,  sizeof(kDummyDXIL));

	//This is the shape a merged oiSH has for push constants.
	//SPIRV keeps them out of any descriptor set, so the register carries no binding at all and only the used flag
	// says it is there, while DXIL reflects the same constants as a plain $Globals constant buffer.
	//The two have different names, so SHFile_combine never matched them and both are present side by side.

	SBFile pushSB = Test_SHSplitMakeCBuffer(t);
	CharString pushName = CharString_createRefCStrConst("constants");

	Test_assert(t, "add push constants",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_PushConstants, false, 1 << EGfxBinaryType_SPIRV,
			&pushName, NULL, &pushSB, GfxBindings_dummy(), t->alloc, &t->err
		)
	);

	SBFile globalsSB = Test_SHSplitMakeCBuffer(t);
	CharString globalsName = CharString_createRefCStrConst("$Globals");
	GfxBindings globalsBinding = GfxBindings_dummy();
	globalsBinding.arr[EGfxBinaryType_DXIL] = (GfxBinding) { .binding = 0, .space = 0 };

	Test_assert(t, "add $Globals",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_ConstantBuffer, false, 1 << EGfxBinaryType_DXIL,
			&globalsName, NULL, &globalsSB, globalsBinding, t->alloc, &t->err
		)
	);

	SBFile_free(&pushSB, t->alloc);
	SBFile_free(&globalsSB, t->alloc);

	Test_assert(t, "addBin", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	const U16 binId = 0;
	Test_assert(t, "addEntry", addComputeEntry(&sh, "cs", 64, 1, 1, t, false, &binId));

	Test_assert(t, "split spv",  SHFile_split(&sh, EGfxBinaryType_SPIRV, t->alloc, &spv,  &t->err));
	Test_assert(t, "split dxil", SHFile_split(&sh, EGfxBinaryType_DXIL,  t->alloc, &dxil, &t->err));

	Test_assert(t, "spv keeps push constants", Test_SHSplitHasRegisterNamed(&spv.binaries.ptr[0], "constants"));
	Test_assert(t, "spv drops $Globals",      !Test_SHSplitHasRegisterNamed(&spv.binaries.ptr[0], "$Globals"));

	Test_assert(t, "dxil keeps $Globals",      Test_SHSplitHasRegisterNamed(&dxil.binaries.ptr[0], "$Globals"));
	Test_assert(t, "dxil drops push constants", !Test_SHSplitHasRegisterNamed(&dxil.binaries.ptr[0], "constants"));

	SHFile_free(&sh, t->alloc);
	SHFile_free(&spv, t->alloc);
	SHFile_free(&dxil, t->alloc);
}

void Test_SHFileSplitDropsEntrypointAndRemapsIds(Test *t) {

	Test_setModule(t, "SHFile split: an entrypoint that only compiled to the other type is dropped");

	SHFile sh = (SHFile) { 0 }, spv = (SHFile) { 0 }, dxil = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	//Binary 0 is SPIRV only, binary 1 is DXIL only, which is what a per entrypoint [[oxc::binary()]] produces

	SHBinaryInfo spvInfo = makeBinaryInfo(EGfxPipelineStage_Compute, "csA", false);
	Test_assert(t, "addBin spv", SHFile_addBinary(&sh, &spvInfo, t->alloc, &t->err));

	SHBinaryInfo dxilInfo = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("csB"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = EGfxPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
	};

	dxilInfo.binaries[EGfxBinaryType_DXIL] = Buffer_createRefConst(kDummyDXIL, sizeof(kDummyDXIL));
	Test_assert(t, "addBin dxil", SHFile_addBinary(&sh, &dxilInfo, t->alloc, &t->err));

	const U16 idA = 0, idB = 1;
	Test_assert(t, "addEntry A", addComputeEntry(&sh, "csA", 8, 8, 1, t, false, &idA));
	Test_assert(t, "addEntry B", addComputeEntry(&sh, "csB", 8, 8, 1, t, false, &idB));

	Test_assert(t, "split spv",  SHFile_split(&sh, EGfxBinaryType_SPIRV, t->alloc, &spv,  &t->err));
	Test_assert(t, "split dxil", SHFile_split(&sh, EGfxBinaryType_DXIL,  t->alloc, &dxil, &t->err));

	const CharString csA = CharString_createRefCStrConst("csA"), csB = CharString_createRefCStrConst("csB");

	Test_assert(t, "spv has one binary", spv.binaries.length == 1);
	Test_assert(t, "spv has one entry",  spv.entries.length  == 1);
	Test_assert(t, "spv kept csA",       CharString_equalsStringSensitive(&spv.entries.ptr[0].name, &csA));

	Test_assert(t, "dxil has one binary", dxil.binaries.length == 1);
	Test_assert(t, "dxil has one entry",  dxil.entries.length  == 1);
	Test_assert(t, "dxil kept csB",       CharString_equalsStringSensitive(&dxil.entries.ptr[0].name, &csB));

	//Binary 1 became binary 0 once binary 0 was dropped, so csB has to point at the new id

	Test_assert(t, "dxil binaryIds remapped",
		dxil.entries.ptr[0].binaryIds.length == 1 && !dxil.entries.ptr[0].binaryIds.ptr[0]
	);

	SHFile_free(&sh, t->alloc);
	SHFile_free(&spv, t->alloc);
	SHFile_free(&dxil, t->alloc);
}

void Test_SHFileSplitCombineRoundTrip(Test *t) {

	Test_setModule(t, "SHFile split: combining the two halves gives the original back");

	SHFile sh = (SHFile) { 0 }, spv = (SHFile) { 0 }, dxil = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "merged input", Test_SHSplitMakeMerged(t, &sh));

	Test_assert(t, "split spv",  SHFile_split(&sh, EGfxBinaryType_SPIRV, t->alloc, &spv,  &t->err));
	Test_assert(t, "split dxil", SHFile_split(&sh, EGfxBinaryType_DXIL,  t->alloc, &dxil, &t->err));

	Test_assert(t, "combine halves", SHFile_combine(&spv, &dxil, t->alloc, &out, &t->err));

	Test_assert(t, "one binary", out.binaries.length == 1);
	Test_assert(t, "one entry",  out.entries.length  == 1);

	const SHBinaryInfo *before = &sh.binaries.ptr[0], *after = &out.binaries.ptr[0];

	Test_assert(t, "SPIRV back",
		Buffer_length(after->binaries[EGfxBinaryType_SPIRV]) == sizeof(kDummySPIRV)
	);

	Test_assert(t, "DXIL back",
		Buffer_length(after->binaries[EGfxBinaryType_DXIL]) == sizeof(kDummyDXIL)
	);

	Test_assert(t, "register count back", after->registers.length == before->registers.length);

	//Compared by content rather than by hash, because SHFile_combine leaves the hash of a register it merely
	// copied at zero, so a hash comparison would be testing that gap instead of the round trip

	Bool allBack = true;

	for(U64 i = 0; i < before->registers.length; ++i) {

		const SHRegisterRuntime *orig = &before->registers.ptr[i];

		if(!Test_SHSplitRegisterEquals(orig, Test_SHSplitFindRegister(after, &orig->name)))
			allBack = false;
	}

	Test_assert(t, "every register back", allBack);
	Test_assert(t, "extensions back",     after->identifier.extensions == before->identifier.extensions);
	Test_assert(t, "dormant back",        after->dormantExtensions == before->dormantExtensions);
	Test_assert(t, "vendorMask back",     after->vendorMask == before->vendorMask);

	SHFile_free(&sh, t->alloc);
	SHFile_free(&spv, t->alloc);
	SHFile_free(&dxil, t->alloc);
	SHFile_free(&out, t->alloc);
}
