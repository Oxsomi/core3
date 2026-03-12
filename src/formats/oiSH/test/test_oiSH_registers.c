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
#include "types/container/list_basic_types.h"

static inline SHBindings makeSPIRVBinding(U32 set, U32 binding) {
	SHBindings b = SHBindings_dummy();
	b.arr[ESHBinaryType_SPIRV].space = set;
	b.arr[ESHBinaryType_SPIRV].binding = binding;
	return b;
}

void Test_SHFileRegisterAddSampler(Test *t) {

	Test_setModule(t, "SHFile register: add sampler accepted");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	CharString name = CharString_createRefCStrConst("mySampler");
	SHBindings b = makeSPIRVBinding(0, 0);

	Test_assert(t, "add sampler",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &name, NULL, b, t->alloc, &t->err)
	);

	Test_assert(t, "count 1", info.registers.length == 1);
	Test_assert(t, "type is Sampler",
		(info.registers.ptr[0].reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_Sampler
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterAddSamplerComparisonState(Test *t) {

	Test_setModule(t, "SHFile register: add SamplerComparisonState accepted");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Pixel, "psMain", false);
	CharString name = CharString_createRefCStrConst("myCmpSampler");
	SHBindings b = makeSPIRVBinding(0, 1);

	Test_assert(t, "add cmp sampler",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x1, true, &name, NULL, b, t->alloc, &t->err)
	);

	Test_assert(t, "type is SamplerComparisonState",
		(info.registers.ptr[0].reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_SamplerComparisonState
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterDuplicateNameRejected(Test *t) {

	Test_setModule(t, "SHFile register: duplicate register name rejected");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);

	CharString n1 = CharString_createRefCStrConst("myBuf");
	SHBindings  b1 = makeSPIRVBinding(0, 0);
	CharString n2 = CharString_createRefCStrConst("myBuf");
	SHBindings  b2 = makeSPIRVBinding(0, 1);	//different binding, same name

	Test_assert(t, "first ok",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &n1, NULL, b1, t->alloc, NULL)
	);

	Test_assert(t, "duplicate name rejected",
		!ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &n2, NULL, b2, t->alloc, NULL)
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterDuplicateSPIRVBindingRejected(Test *t) {

	Test_setModule(t, "SHFile register: duplicate SPIRV set+binding rejected");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);

	CharString n1 = CharString_createRefCStrConst("sampA");
	CharString n2 = CharString_createRefCStrConst("sampB");
	SHBindings  b  = makeSPIRVBinding(0, 0);	//same set + binding for both

	Test_assert(t, "first ok",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &n1, NULL, b, t->alloc, NULL)
	);

	Test_assert(t, "duplicate binding rejected",
		!ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &n2, NULL, b, t->alloc, NULL)
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterAddTexture(Test *t) {

	Test_setModule(t, "SHFile register: add Texture2D accepted, IsWrite not set");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Pixel, "psMain", false);
	CharString name = CharString_createRefCStrConst("myTex");
	SHBindings b = makeSPIRVBinding(0, 0);

	Test_assert(t, "add texture",
		ListSHRegisterRuntime_addTexture(
			&info.registers,
			ESHTextureType_Texture2D,
			false,	//not array
			false,	//not combined sampler
			0x1,
			ESHTexturePrimitive_Float | ESHTexturePrimitive_Component4,
			&name, NULL, b, t->alloc, &t->err
		)
	);

	Test_assert(t, "count 1", info.registers.length == 1);
	Test_assert(t, "is Texture2D",
		(info.registers.ptr[0].reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_Texture2D
	);

	Test_assert(t, "not write",
		!(info.registers.ptr[0].reg.registerType & ESHRegisterType_IsWrite)
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterAddRWTexture(Test *t) {

	Test_setModule(t, "SHFile register: add RWTexture2D accepted, IsWrite is set");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	CharString name = CharString_createRefCStrConst("myRWTex");
	SHBindings b = makeSPIRVBinding(0, 0);

	Test_assert(t, "add RW texture",
		ListSHRegisterRuntime_addRWTexture(
			&info.registers,
			ESHTextureType_Texture2D,
			false,
			0x1,
			ESHTexturePrimitive_Count,	/* auto-detect from format */
			ETextureFormatId_RGBA8,
			&name, NULL, b, t->alloc, &t->err
		));

	Test_assert(t, "count 1", info.registers.length == 1);
	Test_assert(t, "is write",
		info.registers.ptr[0].reg.registerType & ESHRegisterType_IsWrite
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterArray(Test *t) {

	Test_setModule(t, "SHFile register: 1D array[4] accepted; >32 dimensions rejected");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);

	//Valid: 1 dimension with count 4
	CharString n1 = CharString_createRefCStrConst("arrSamp");
	SHBindings b1 = makeSPIRVBinding(0, 0);
	U32 dim4 = 4;
	ListU32 arr4 = (ListU32) { 0 };
	Test_assert(t, "createRef", ListU32_createRefConst(&dim4, 1, &arr4, &t->err));
	Test_assert(t, "array[4] accepted",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &n1, &arr4, b1, t->alloc, &t->err)
	);

	//Invalid: 33 dimensions (> 32)
	CharString n2 = CharString_createRefCStrConst("arrSamp2");
	SHBindings b2 = makeSPIRVBinding(0, 10);
	U32 dims[33];
	for (U8 i = 0; i < 33; ++i) dims[i] = 1;
	ListU32 arr33 = (ListU32) { 0 };
	Test_assert(t, "createRef(1)", ListU32_createRefConst(dims, 33, &arr33, &t->err));
	Test_assert(t, "33 dims rejected",
		!ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &n2, &arr33, b2, t->alloc, NULL)
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterHashDedup(Test *t) {

	Test_setModule(t, "SHFile register: identical register hash is silently skipped");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	CharString n1 = CharString_createRefCStrConst("myTex");
	CharString n2 = CharString_createRefCStrConst("myTex");
	SHBindings b = makeSPIRVBinding(0, 0);

	Test_assert(t, "first ok",
		ListSHRegisterRuntime_addTexture(
			&info.registers, ESHTextureType_Texture2D, false, false, 0x1,
			ESHTexturePrimitive_Float | ESHTexturePrimitive_Component4,
			&n1, NULL, b, t->alloc, &t->err
		)
	);

	//Exact same name + binding + type -> same hash -> silent skip (not an error)
	Test_assert(t, "identical silently skipped",
		ListSHRegisterRuntime_addTexture(
			&info.registers, ESHTextureType_Texture2D, false, false, 0x1,
			ESHTexturePrimitive_Float | ESHTexturePrimitive_Component4,
			&n2, NULL, b, t->alloc, &t->err
		)
	);

	Test_assert(t, "still only 1 register", info.registers.length == 1);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterSubpassInput(Test *t) {

	Test_setModule(t, "SHFile register: subpass input id 0..7 accepted, id >= 8 rejected");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Pixel, "psMain", false);

	CharString n1 = CharString_createRefCStrConst("subpass0");
	SHBindings b1 = makeSPIRVBinding(0, 0);
	Test_assert(t, "id = 0 accepted",
		ListSHRegisterRuntime_addSubpassInput(&info.registers, 0x1, &n1, b1, 0, t->alloc, &t->err)
	);

	CharString n2 = CharString_createRefCStrConst("subpass7");
	SHBindings b2 = makeSPIRVBinding(0, 1);
	Test_assert(t, "id = 7 accepted",
		ListSHRegisterRuntime_addSubpassInput(&info.registers, 0x1, &n2, b2, 7, t->alloc, &t->err)
	);

	CharString n3 = CharString_createRefCStrConst("subpass8");
	SHBindings b3 = makeSPIRVBinding(0, 2);
	Test_assert(t, "id = 8 rejected",
		!ListSHRegisterRuntime_addSubpassInput(&info.registers, 0x1, &n3, b3, 8, t->alloc, NULL)
	);

	Test_assert(t, "exactly 2 registered", info.registers.length == 2);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterStoredInBinary(Test *t) {

	Test_setModule(t, "SHFile register: registers attached to SHBinaryInfo are stored after addBinary");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);

	CharString sampName = CharString_createRefCStrConst("gSampler");
	SHBindings sampB = makeSPIRVBinding(0, 0);
	Test_assert(t, "add sampler to info",
		ListSHRegisterRuntime_addSampler(&info.registers, 0x1, false, &sampName, NULL, sampB, t->alloc, &t->err)
	);

	CharString texName = CharString_createRefCStrConst("gTexture");
	SHBindings texB = makeSPIRVBinding(0, 1);
	Test_assert(t, "add texture to info",
		ListSHRegisterRuntime_addTexture(
			&info.registers, ESHTextureType_Texture2D, false, false, 0x1,
			ESHTexturePrimitive_Float | ESHTexturePrimitive_Component4,
			&texName, NULL, texB, t->alloc, &t->err
		)
	);

	Test_assert(t, "addBinary ok", SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	Test_assert(t, "1 binary",     sh.binaries.length == 1);
	Test_assert(t, "2 registers",  sh.binaries.ptr[0].registers.length == 2);

	SHFile_free(&sh, t->alloc);
}

static SBFile makeCBufferSBFile(Test *t) {

	SBFile sb = { 0 };
	if (!SBFile_create(ESBSettingsFlags_None, 16, t->alloc, &sb, &t->err)) {
		Test_assert(t, "makeCBufferSBFile: create", false);
		return sb;
	}

	CharString nameA = CharString_createRefCStrConst("time");
	CharString nameB = CharString_createRefCStrConst("pad");

	if (
		!SBFile_addVariableAsType(&sb, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err) ||
		!SBFile_addVariableAsType(&sb, &nameB, 4, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
	) {
		Test_assert(t, "makeCBufferSBFile: addVar", false);
		SBFile_free(&sb, t->alloc);
		return (SBFile) { 0 };
	}

	return sb;
}

static SBFile makeTightSBFile(Test *t) {

	SBFile sb = { 0 };
	if (!SBFile_create(ESBSettingsFlags_IsTightlyPacked, 8, t->alloc, &sb, &t->err)) {
		Test_assert(t, "makeTightSBFile: create", false);
		return sb;
	}

	CharString nameA = CharString_createRefCStrConst("x");
	CharString nameB = CharString_createRefCStrConst("y");

	if (
		!SBFile_addVariableAsType(&sb, &nameA, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err) ||
		!SBFile_addVariableAsType(&sb, &nameB, 4, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)
	) {
		Test_assert(t, "makeTightSBFile: addVar", false);
		SBFile_free(&sb, t->alloc);
		return (SBFile) { 0 };
	}

	return sb;
}

static SBFile makeCBufferSBFileOfSize(U32 size, Test *t) {

	SBFile sb = { 0 };
	if (!SBFile_create(ESBSettingsFlags_None, size, t->alloc, &sb, &t->err)) {
		Test_assert(t, "makeCBufferSBFileOfSize: create", false);
		return sb;
	}

	CharString name = CharString_createRefCStrConst("data");
	if (!SBFile_addVariableAsType(&sb, &name, 0, U16_MAX, ESBType_F32, ESBVarFlag_None, NULL, t->alloc, &t->err)) {
		Test_assert(t, "makeCBufferSBFileOfSize: addVar", false);
		SBFile_free(&sb, t->alloc);
		return (SBFile) { 0 };
	}

	return sb;
}
void Test_SHFileRegisterAddConstantBuffer(Test *t) {

	Test_setModule(t, "SHFile register: add ConstantBuffer accepted, type and write flag correct");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	SBFile cbSB = makeCBufferSBFile(t);
	CharString name = CharString_createRefCStrConst("MyCB");
	SHBindings b = makeSPIRVBinding(0, 0);

	Test_assert(t, "add CB",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_ConstantBuffer, false, 0x1,
			&name, NULL, &cbSB, b, t->alloc, &t->err
		)
	);

	Test_assert(t, "count 1", info.registers.length == 1);
	Test_assert(t, "type is ConstantBuffer",
		(info.registers.ptr[0].reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_ConstantBuffer
	);

	Test_assert(t, "not write",
		!(info.registers.ptr[0].reg.registerType & ESHRegisterType_IsWrite)
	);

	SBFile_free(&cbSB, t->alloc);
	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterAddByteAddressBuffer(Test *t) {

	Test_setModule(t, "SHFile register: add ByteAddressBuffer (read and write variants)");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	CharString n1 = CharString_createRefCStrConst("myBAB");
	SHBindings b1 = makeSPIRVBinding(0, 0);

	Test_assert(t, "add BAB read",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_ByteAddressBuffer, false, 0x1,
			&n1, NULL, NULL, b1, t->alloc, &t->err
		)
	);

	Test_assert(t, "count 1", info.registers.length == 1);
	Test_assert(t, "BAB not write",
		!(info.registers.ptr[0].reg.registerType & ESHRegisterType_IsWrite)
	);

	CharString n2 = CharString_createRefCStrConst("myRWBAB");
	SHBindings b2 = makeSPIRVBinding(0, 1);
	Test_assert(t, "add BAB write",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_ByteAddressBuffer, true, 0x1,
			&n2, NULL, NULL, b2, t->alloc, &t->err
		)
	);

	Test_assert(t, "count 2", info.registers.length == 2);
	Test_assert(t, "RW BAB write flag set",
		info.registers.ptr[1].reg.registerType & ESHRegisterType_IsWrite
	);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterAddStructuredBuffer(Test *t) {

	Test_setModule(t, "SHFile register: add StructuredBuffer and RWStructuredBuffer accepted");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	SBFile sbRead = makeTightSBFile(t);
	CharString n1 = CharString_createRefCStrConst("mySB");
	SHBindings b1 = makeSPIRVBinding(0, 0);

	Test_assert(t, "add SB read",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_StructuredBuffer, false, 0x1,
			&n1, NULL, &sbRead, b1, t->alloc, &t->err
		)
	);

	Test_assert(t, "count 1", info.registers.length == 1);
	Test_assert(t, "SB type",
		(info.registers.ptr[0].reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_StructuredBuffer
	);

	SBFile sbWrite = makeTightSBFile(t);
	CharString n2 = CharString_createRefCStrConst("myRWSB");
	SHBindings b2 = makeSPIRVBinding(0, 1);
	Test_assert(t, "add RW SB",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_StructuredBuffer, true, 0x1,
			&n2, NULL, &sbWrite, b2, t->alloc, &t->err
		)
	);

	Test_assert(t, "count 2", info.registers.length == 2);
	Test_assert(t, "RW SB write flag set",
		info.registers.ptr[1].reg.registerType & ESHRegisterType_IsWrite
	);

	SBFile_free(&sbRead,  t->alloc);
	SBFile_free(&sbWrite, t->alloc);
	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterAddAccelerationStructure(Test *t) {

	Test_setModule(t, "SHFile register: add AccelerationStructure accepted; write flag rejected");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	CharString n1 = CharString_createRefCStrConst("myTLAS");
	SHBindings b1 = makeSPIRVBinding(0, 0);

	Test_assert(t, "AS read accepted",
		ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_AccelerationStructure, false, 0x1,
			&n1, NULL, NULL, b1, t->alloc, &t->err
		)
	);

	Test_assert(t, "count 1", info.registers.length == 1);
	Test_assert(t, "AS type",
		(info.registers.ptr[0].reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_AccelerationStructure
	);

	CharString n2 = CharString_createRefCStrConst("myRWTLAS");
	SHBindings b2 = makeSPIRVBinding(0, 1);

	Test_assert(t, "AS write rejected",
		!ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_AccelerationStructure, true, 0x1,
			&n2, NULL, NULL, b2, t->alloc, NULL
		)
	);

	Test_assert(t, "still count 1", info.registers.length == 1);

	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterBufferWriteFlagRejections(Test *t) {

	Test_setModule(t, "SHFile register: ConstantBuffer and PushConstants reject isWrite");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	SBFile cbSB = makeCBufferSBFile(t);
	CharString name = CharString_createRefCStrConst("badCB");
	SHBindings b = makeSPIRVBinding(0, 0);

	Test_assert(t, "CB + write rejected",
		!ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_ConstantBuffer, true, 0x1,
			&name, NULL, &cbSB, b, t->alloc, NULL
		)
	);

	Test_assert(t, "count still 0", info.registers.length == 0);

	SBFile_free(&cbSB, t->alloc);
	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}

void Test_SHFileRegisterConstantBufferSizeLimit(Test *t) {

	Test_setModule(t, "SHFile register: ConstantBuffer exceeding 64 KiB rejected");

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	SBFile bigCB = makeCBufferSBFileOfSize(64 * 1024 + 4, t);
	CharString name = CharString_createRefCStrConst("tooBig");
	SHBindings b = makeSPIRVBinding(0, 0);

	Test_assert(t, "oversized CB rejected",
		!ListSHRegisterRuntime_addBuffer(
			&info.registers, ESHBufferType_ConstantBuffer, false, 0x1,
			&name, NULL, &bigCB, b, t->alloc, NULL
		)
	);

	Test_assert(t, "count still 0", info.registers.length == 0);
	SBFile_free(&bigCB, t->alloc);
	ListSHRegisterRuntime_freeUnderlying(&info.registers, t->alloc);
}
