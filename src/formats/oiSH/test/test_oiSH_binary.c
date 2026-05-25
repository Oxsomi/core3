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

//formats/oiSH/test/test_oiSH_binary.c

#include "test_oiSH_shared.h"
#include "types/container/list_basic_types.h"

void Test_SHFileAddBinNullGuards(Test *t) {

	Test_setModule(t, "SHFile addBinary: null guards");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	Test_assert(t, "null shFile",    !SHFile_addBinary(NULL, &info, t->alloc, NULL));
	Test_assert(t, "null binaries",  !SHFile_addBinary(&sh,  NULL,  t->alloc, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinNoBinaryData(Test *t) {

	Test_setModule(t, "SHFile addBinary: at least one binary buffer required");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("main"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = ESHPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
		//binaries[] all empty
	};

	Test_assert(t, "no binary data rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinZeroVendorMask(Test *t) {

	Test_setModule(t, "SHFile addBinary: zero vendorMask rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHBinaryInfo info  = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	info.vendorMask    = 0;
	Test_assert(t, "vendorMask = 0 rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinVendorMaskNormalized(Test *t) {

	Test_setModule(t, "SHFile addBinary: U16_MAX vendorMask normalized to all-vendors bitmask");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	info.vendorMask   = U16_MAX;
	Test_assert(t, "U16_MAX accepted", SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinVendorMaskOutOfBounds(Test *t) {

	Test_setModule(t, "SHFile addBinary: vendorMask with bits beyond ESHVendor_Count rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);

	//Set a bit beyond the valid vendor range but not the U16_MAX special case
	info.vendorMask = (U16)(1u << ESHVendor_Count);
	Test_assert(t, "oob vendorMask rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinInvalidExtensions(Test *t) {

	Test_setModule(t, "SHFile addBinary: extension bits beyond Count rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	info.identifier.extensions = (ESHExtension)(1u << ESHExtension_Count);
	Test_assert(t, "bad extensions rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinInvalidStageType(Test *t) {

	Test_setModule(t, "SHFile addBinary: stageType >= Count rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	info.identifier.stageType = ESHPipelineStage_Count;
	Test_assert(t, "bad stageType rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinShaderVersionOutOfRange(Test *t) {

	Test_setModule(t, "SHFile addBinary: shaderVersion outside [MIN, MAX] rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo low  = makeBinaryInfo(ESHPipelineStage_Compute, "lo", false);
	SHBinaryInfo high = makeBinaryInfo(ESHPipelineStage_Compute, "hi", false);
	low.identifier.shaderVersion  = OISH_SHADER_MODEL(5, 0);	//below MIN
	high.identifier.shaderVersion = OISH_SHADER_MODEL(7, 0);	//above MAX

	Test_assert(t, "below MIN rejected", !SHFile_addBinary(&sh, &low,  t->alloc, NULL));
	Test_assert(t, "above MAX rejected", !SHFile_addBinary(&sh, &high, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinShaderVersionAtBoundaries(Test *t) {

	Test_setModule(t, "SHFile addBinary: shaderVersion at MIN and MAX accepted");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo mn = makeBinaryInfo(ESHPipelineStage_Compute, "mainMin", false);
	SHBinaryInfo mx = makeBinaryInfo(ESHPipelineStage_Pixel,   "mainMax", false);
	mn.identifier.shaderVersion = OISH_SHADER_MODEL_MIN;
	mx.identifier.shaderVersion = OISH_SHADER_MODEL_MAX;

	Test_assert(t, "MIN accepted", SHFile_addBinary(&sh, &mn, t->alloc, &t->err));
	Test_assert(t, "MAX accepted", SHFile_addBinary(&sh, &mx, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinSPIRVMisaligned(Test *t) {

	Test_setModule(t, "SHFile addBinary: SPIRV buffer not multiple-of-4 rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	static const U8 oddSPIRV[5] = { 0x03, 0x02, 0x23, 0x07, 0x00 };
	SHBinaryInfo info = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("main"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = ESHPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
	};

	info.binaries[ESHBinaryType_SPIRV] = Buffer_createRefConst(oddSPIRV, 5);
	Test_assert(t, "misaligned SPIRV rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinEntrypointAnnotationMutuallyExclusive(Test *t) {

	Test_setModule(t, "SHFile addBinary: entrypoint and hasShaderAnnotation are mutually exclusive");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	//annotation = true but entrypoint non-empty -> reject
	SHBinaryInfo bad1 = makeBinaryInfo(ESHPipelineStage_Compute, "main", true);
	Test_assert(t, "annotation+entrypoint rejected", !SHFile_addBinary(&sh, &bad1, t->alloc, NULL));

	//annotation = false but entrypoint empty -> reject
	SHBinaryInfo bad2 = makeBinaryInfo(ESHPipelineStage_Compute, NULL, false);
	Test_assert(t, "no-annot+no-entry rejected",     !SHFile_addBinary(&sh, &bad2, t->alloc, NULL));

	//lib binary: annotation = true + empty entrypoint -> accept
	SHBinaryInfo lib = makeBinaryInfo(ESHPipelineStage_Compute, NULL, true);
	Test_assert(t, "lib binary accepted",             SHFile_addBinary(&sh, &lib, t->alloc, &t->err));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinOddDefinesRejected(Test *t) {

	Test_setModule(t, "SHFile addBinary: odd defines.length rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	CharString strs[3] = {
		CharString_createRefCStrConst("FOO"),
		CharString_createRefCStrConst("1"),
		CharString_createRefCStrConst("BAR")	//no value -> odd count
	};

	Test_assert(t, "createRef", ListCharString_createRefConst(strs, 3, &info.identifier.defines, &t->err));
	Test_assert(t, "odd defines rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinDuplicateDefineNameRejected(Test *t) {

	Test_setModule(t, "SHFile addBinary: duplicate define name in same binary rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	CharString strs[4] = {
		CharString_createRefCStrConst("FOO"), CharString_createRefCStrConst("1"),
		CharString_createRefCStrConst("FOO"), CharString_createRefCStrConst("2")
	};

	Test_assert(t, "createRef", ListCharString_createRefConst(strs, 4, &info.identifier.defines, &t->err));
	Test_assert(t, "duplicate define rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinValidDefines(Test *t) {

	Test_setModule(t, "SHFile addBinary: two distinct defines accepted");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	CharString strs[4] = {
		CharString_createRefCStrConst("FOO"), CharString_createRefCStrConst("1"),
		CharString_createRefCStrConst("BAR"), CharString_createRefCStrConst("2")
	};

	Test_assert(t, "createRef", ListCharString_createRefConst(strs, 4, &info.identifier.defines, &t->err));
	Test_assert(t, "two defines accepted", SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinUniformEmptyName(Test *t) {

	Test_setModule(t, "SHFile addBinary: uniform with empty name rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	SHUniformRuntime uni = { .name = CharString_createNull(), .typeIdShort = 0, .dataOffset = 0 };
	static U8 udata[4] = { 0 };
	Test_assert(t, "createRef", ListU8_createRefConst(udata, 4, &info.identifier.uniformData, &t->err));
	Test_assert(t, "createRef(1)", ListSHUniformRuntime_createRefConst(&uni, 1, &info.identifier.uniforms, &t->err));
	Test_assert(t, "empty uniform name rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinUniformDigitFirstChar(Test *t) {

	Test_setModule(t, "SHFile addBinary: uniform name starting with digit rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	SHUniformRuntime uni = { .name = CharString_createRefCStrConst("9bad"), .typeIdShort = 0, .dataOffset = 0 };
	static U8 udata[4] = { 0 };
	Test_assert(t, "createRef", ListU8_createRefConst(udata, 4, &info.identifier.uniformData, &t->err));
	Test_assert(t, "createRef(1)", ListSHUniformRuntime_createRefConst(&uni, 1, &info.identifier.uniforms, &t->err));
	Test_assert(t, "digit-first name rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinUniformUnderscoreStart(Test *t) {

	Test_setModule(t, "SHFile addBinary: uniform name starting with underscore accepted");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	SHUniformRuntime uni = { .name = CharString_createRefCStrConst("_myUni"), .typeIdShort = 0, .dataOffset = 0 };
	static U8 udata[4] = { 0 };
	Test_assert(t, "createRef", ListU8_createRefConst(udata, 4, &info.identifier.uniformData, &t->err));
	Test_assert(t, "createRef(1)", ListSHUniformRuntime_createRefConst(&uni, 1, &info.identifier.uniforms, &t->err));
	Test_assert(t, "underscore-start accepted", SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinUniformInvalidBodyChar(Test *t) {

	Test_setModule(t, "SHFile addBinary: uniform name with invalid body character rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);

	//Space inside name is invalid
	SHUniformRuntime uni = { .name = CharString_createRefCStrConst("my Uni"), .typeIdShort = 0, .dataOffset = 0 };
	static U8 udata[4] = { 0 };

	Test_assert(t, "createRef", ListU8_createRefConst(udata, 4, &info.identifier.uniformData, &t->err));
	Test_assert(t, "createRef(1)", ListSHUniformRuntime_createRefConst(&uni, 1, &info.identifier.uniforms, &t->err));
	Test_assert(t, "space-in-name rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinUniformDuplicateName(Test *t) {

	Test_setModule(t, "SHFile addBinary: duplicate uniform name rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	SHUniformRuntime unis[2] = {
		{ .name = CharString_createRefCStrConst("myVal"), .typeIdShort = 0, .dataOffset = 0 },
		{ .name = CharString_createRefCStrConst("myVal"), .typeIdShort = 0, .dataOffset = 0 }
	};

	static U8 udata[4] = { 0 };
	Test_assert(t, "createRef", ListU8_createRefConst(udata, 4, &info.identifier.uniformData, &t->err));
	Test_assert(t, "createRef(1)", ListSHUniformRuntime_createRefConst(unis, 2, &info.identifier.uniforms, &t->err));
	Test_assert(t, "duplicate uniform rejected", !SHFile_addBinary(&sh, &info, t->alloc, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinValid(Test *t) {

	Test_setModule(t, "SHFile addBinary: minimal valid binary stored correctly");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	Test_assert(t, "addBinary ok",       SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	Test_assert(t, "binaries count = 1", sh.binaries.length == 1);
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinDuplicateIdentifierRejected(Test *t) {

	Test_setModule(t, "SHFile addBinary: duplicate binary identifier rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo i1 = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	SHBinaryInfo i2 = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	Test_assert(t, "first accepted",      SHFile_addBinary(&sh, &i1, t->alloc, &t->err));
	Test_assert(t, "duplicate rejected", !SHFile_addBinary(&sh, &i2, t->alloc, NULL));
	Test_assert(t, "still 1 binary",      sh.binaries.length == 1);

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinMultipleDistinct(Test *t) {

	Test_setModule(t, "SHFile addBinary: multiple binaries with distinct identifiers accepted");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	//Same entrypoint but different stages are distinct identifiers
	SHBinaryInfo cs = makeBinaryInfo(ESHPipelineStage_Compute, "main",    false);
	SHBinaryInfo vs = makeBinaryInfo(ESHPipelineStage_Vertex,  "vsMain",  false);
	SHBinaryInfo ps = makeBinaryInfo(ESHPipelineStage_Pixel,   "psMain",  false);

	Test_assert(t, "compute accepted", SHFile_addBinary(&sh, &cs, t->alloc, &t->err));
	Test_assert(t, "vertex accepted",  SHFile_addBinary(&sh, &vs, t->alloc, &t->err));
	Test_assert(t, "pixel accepted",   SHFile_addBinary(&sh, &ps, t->alloc, &t->err));
	Test_assert(t, "count == 3",       sh.binaries.length == 3);

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddBinExtensionFlagsStored(Test *t) {

	Test_setModule(t, "SHFile addBinary: valid extension flags stored correctly");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "main", false);
	info.identifier.extensions = ESHExtension_F64 | ESHExtension_I64;
	Test_assert(t, "addBinary ok", SHFile_addBinary(&sh, &info, t->alloc, &t->err));
	Test_assert(t, "F64 flag stored", (sh.binaries.ptr[0].identifier.extensions & ESHExtension_F64) != 0);
	Test_assert(t, "I64 flag stored", (sh.binaries.ptr[0].identifier.extensions & ESHExtension_I64) != 0);

	SHFile_free(&sh, t->alloc);
}
