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
#include "types/base/string.h"
#include "types/base/string_read_helper.h"

static inline Bool cmpstr(const C8 *a, const C8 *b) {
	const CharString str = CharString_createRefCStrConst(a);
	return CharString_equalsCStringSensitive(&str, b);
}

//Simple checks

void Test_SHFileCreateFree(Test *t) {

	Test_setModule(t, "SHFile create/free");

	SHFile sh = (SHFile) { 0 };

	Test_assert(t, "create succeeds", SHFile_create(ESHSettingsFlags_None, OXC3_VERSION, 0xDEADBEEF, t->alloc, &sh, &t->err));

	Test_assert(t, "entries list allocated",  sh.entries.ptr  != NULL);
	Test_assert(t, "binaries list allocated", sh.binaries.ptr != NULL);
	Test_assert(t, "includes list allocated", sh.includes.ptr != NULL);
	Test_assert(t, "zero entries",   sh.entries.length  == 0);
	Test_assert(t, "zero binaries",  sh.binaries.length == 0);
	Test_assert(t, "zero includes",  sh.includes.length == 0);
	Test_assert(t, "flags stored",   sh.flags == ESHSettingsFlags_None);
	Test_assert(t, "version stored", sh.compilerVersion == OXC3_VERSION);
	Test_assert(t, "hash stored",    sh.sourceHash == 0xDEADBEEF);

	SHFile_free(&sh, t->alloc);
	Test_assert(t, "entries cleared after free", sh.entries.ptr == NULL);

	//Double-free must be a no-op
	SHFile_free(&sh, t->alloc);
	Test_assert(t, "double free safe", sh.entries.ptr == NULL);
}

void Test_SHFileCreateAlreadyAllocated(Test *t) {

	Test_setModule(t, "SHFile create: already-allocated shFile rejected (memleak guard)");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "first create", Test_SHFileCreate(t, &sh));

	Bool ok = SHFile_create(ESHSettingsFlags_None, OXC3_VERSION, 0, t->alloc, &sh, NULL);
	Test_assert(t, "second create rejected", !ok);

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileCreateInvalidFlags(Test *t) {

	Test_setModule(t, "SHFile create: invalid flags rejected");

	SHFile sh = (SHFile) { 0 };
	Bool ok = SHFile_create((ESHSettingsFlags)(ESHSettingsFlags_Invalid), 0, 0, t->alloc, &sh, NULL);
	Test_assert(t, "invalid flags rejected", !ok);
	Test_assert(t, "shFile not allocated", sh.entries.ptr == NULL);
}

void Test_SHPipelineStagePrefixes(Test *t) {

	Test_setModule(t, "ESHPipelineStage getStagePrefix");

	Test_assert(t, "vs", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_Vertex),       "vs"));
	Test_assert(t, "ps", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_Pixel),        "ps"));
	Test_assert(t, "cs", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_Compute),      "cs"));
	Test_assert(t, "gs", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_GeometryExt),  "gs"));
	Test_assert(t, "hs", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_Hull),         "hs"));
	Test_assert(t, "ds", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_Domain),       "ds"));
	Test_assert(t, "ms", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_MeshExt),      "ms"));
	Test_assert(t, "as", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_TaskExt),      "as"));

	//All RT + workgraph stages -> "lib"

	Test_assert(t, "raygen=lib",       cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_RaygenExt),      "lib"));
	Test_assert(t, "miss=lib",         cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_MissExt),        "lib"));
	Test_assert(t, "callable=lib",     cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_CallableExt),    "lib"));
	Test_assert(t, "closesthit=lib",   cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_ClosestHitExt),  "lib"));
	Test_assert(t, "anyhit=lib",       cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_AnyHitExt),      "lib"));
	Test_assert(t, "intersection=lib", cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_IntersectionExt),"lib"));
	Test_assert(t, "workgraph=lib",    cmpstr(ESHPipelineStage_getStagePrefix(ESHPipelineStage_WorkgraphExt),   "lib"));

	//Every stage must return a non-null prefix

	for (U8 i = 0; i < ESHPipelineStage_Count; ++i)
		Test_assert(t, "prefix non-null",
			ESHPipelineStage_getStagePrefix((ESHPipelineStage)i) != NULL);
}

void Test_SHEntryStageName(Test *t) {

	Test_setModule(t, "SHEntry stageName");

	static const struct { ESHPipelineStage stage; const C8 *name; } cases[] = {
		{ ESHPipelineStage_Vertex,         "vertex"         },
		{ ESHPipelineStage_Pixel,          "pixel"          },
		{ ESHPipelineStage_Compute,        "compute"        },
		{ ESHPipelineStage_GeometryExt,    "geometry"       },
		{ ESHPipelineStage_Hull,           "hull"           },
		{ ESHPipelineStage_Domain,         "domain"         },
		{ ESHPipelineStage_RaygenExt,      "raygeneration"  },
		{ ESHPipelineStage_CallableExt,    "callable"       },
		{ ESHPipelineStage_MissExt,        "miss"           },
		{ ESHPipelineStage_ClosestHitExt,  "closesthit"     },
		{ ESHPipelineStage_AnyHitExt,      "anyhit"         },
		{ ESHPipelineStage_IntersectionExt,"intersection"   },
		{ ESHPipelineStage_MeshExt,        "mesh"           },
		{ ESHPipelineStage_TaskExt,        "task"           },
		{ ESHPipelineStage_WorkgraphExt,   "node"           },
	};

	SHEntry e = (SHEntry) { 0 };
	for (U64 i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		e.stage = cases[i].stage;
		Test_assert(t, cases[i].name, cmpstr(SHEntry_stageName(&e), cases[i].name));
	}

	Test_assert(t, "null entry -> null", SHEntry_stageName(NULL) == NULL);
}

void Test_SHExtensionSetSanity(Test *t) {

	Test_setModule(t, "ESHExtension sets sanity");

	Test_assert(t, "DxilNative subset of All",  (ESHExtension_DxilNative  & ~ESHExtension_All) == 0);
	Test_assert(t, "SpirvNative subset of All", (ESHExtension_SpirvNative & ~ESHExtension_All) == 0);

	//Bit-count of All must equal Count

	U32 bits = 0;
	for (U32 i = 0; i < 32; ++i)
		bits += (ESHExtension_All >> i) & 1;

	Test_assert(t, "popcount(All) == Count", bits == ESHExtension_Count);

	//Both name and define arrays fully populated

	for (U32 i = 0; i < ESHExtension_Count; ++i) {
		Test_assert(t, "define non-null", ESHExtension_defines[i] != NULL);
		Test_assert(t, "name non-null",   ESHExtension_names[i]   != NULL);
	}
}

void Test_SHVendorNames(Test *t) {

	Test_setModule(t, "ESHVendor names array");

	for (U8 i = 0; i <= ESHVendor_Count; ++i)
		Test_assert(t, "name non-null", ESHVendor_names[i] != NULL);

	Test_assert(t, "last = Unknown", cmpstr(ESHVendor_names[ESHVendor_Count], "Unknown"));
	Test_assert(t, "NV first",       cmpstr(ESHVendor_names[ESHVendor_NV],    "NV"));
	Test_assert(t, "AMD second",     cmpstr(ESHVendor_names[ESHVendor_AMD],   "AMD"));
}

void Test_SHBinaryTypeNames(Test *t) {

	Test_setModule(t, "ESHBinaryType names array");

	for (U8 i = 0; i < ESHBinaryType_Count; ++i)
		Test_assert(t, "name non-null", ESHBinaryType_names[i] != NULL);

	Test_assert(t, "SPIRV = SPV",  cmpstr(ESHBinaryType_names[ESHBinaryType_SPIRV], "SPV"));
	Test_assert(t, "DXIL = DXIL",  cmpstr(ESHBinaryType_names[ESHBinaryType_DXIL],  "DXIL"));
}

void Test_SHBindingsDummy(Test *t) {

	Test_setModule(t, "SHBindings_dummy: all slots invalid");

	SHBindings b = SHBindings_dummy();
	for (U8 i = 0; i < ESHBinaryType_Count; ++i) {
		Test_assert(t, "binding = U32_MAX", b.arr[i].binding == U32_MAX);
		Test_assert(t, "space = U32_MAX",   b.arr[i].space   == U32_MAX);
	}
}

//Add include (relatively simple logic)

void Test_SHFileAddIncNullGuards(Test *t) {

	Test_setModule(t, "SHFile addInclude: null guards");

	SHInclude inc = { .relativePath = CharString_createRefCStrConst("a.hlsli"), .crc32c = 1 };
	Test_assert(t, "null shFile",  !SHFile_addInclude(NULL, &inc, t->alloc, NULL));

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	Test_assert(t, "null include", !SHFile_addInclude(&sh, NULL, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddIncEmptyPathRejected(Test *t) {

	Test_setModule(t, "SHFile addInclude: empty path rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHInclude inc = { .relativePath = CharString_createNull(), .crc32c = 1 };
	Test_assert(t, "empty path rejected", !SHFile_addInclude(&sh, &inc, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddIncZeroCrcRejected(Test *t) {

	Test_setModule(t, "SHFile addInclude: zero crc32c rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHInclude inc = { .relativePath = CharString_createRefCStrConst("file.hlsli"), .crc32c = 0 };
	Test_assert(t, "zero crc rejected", !SHFile_addInclude(&sh, &inc, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddIncDuplicateSameCrc(Test *t) {

	Test_setModule(t, "SHFile addInclude: duplicate with same CRC silently accepted (idempotent)");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create",
		Test_SHFileCreate(t, &sh));

	SHInclude a = { .relativePath = CharString_createRefCStrConst("common.hlsli"), .crc32c = 0xAABBCCDD };
	SHInclude b = { .relativePath = CharString_createRefCStrConst("common.hlsli"), .crc32c = 0xAABBCCDD };
	Test_assert(t, "first insert",       SHFile_addInclude(&sh, &a, t->alloc, &t->err));
	Test_assert(t, "duplicate accepted", SHFile_addInclude(&sh, &b, t->alloc, &t->err));
	Test_assert(t, "still 1 include",    sh.includes.length == 1);

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddIncDuplicateDifferentCrc(Test *t) {

	Test_setModule(t, "SHFile addInclude: duplicate with different CRC rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHInclude a = { .relativePath = CharString_createRefCStrConst("a.hlsli"), .crc32c = 0x11111111 };
	SHInclude b = { .relativePath = CharString_createRefCStrConst("a.hlsli"), .crc32c = 0x22222222 };
	Test_assert(t, "first insert",            SHFile_addInclude(&sh, &a, t->alloc, &t->err));
	Test_assert(t, "crc mismatch rejected",  !SHFile_addInclude(&sh, &b, t->alloc, NULL));
	Test_assert(t, "count still 1",           sh.includes.length == 1);

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddIncSortedOrder(Test *t) {

	Test_setModule(t, "SHFile addInclude: insertion order is always sorted");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	const C8 *paths[] = { "z_last.hlsli", "a_first.hlsli", "m_middle.hlsli" };
	for (U8 i = 0; i < 3; ++i) {
		SHInclude inc = { .relativePath = CharString_createRefCStrConst(paths[i]), .crc32c = (U32)(i + 1) };
		Test_assert(t, "insert ok", SHFile_addInclude(&sh, &inc, t->alloc, &t->err));
	}

	Test_assert(t, "count 3",           sh.includes.length == 3);
	Test_assert(t, "idx0 = a_first",    cmpstr(sh.includes.ptr[0].relativePath.ptr, "a_first.hlsli"));
	Test_assert(t, "idx1 = m_middle",   cmpstr(sh.includes.ptr[1].relativePath.ptr, "m_middle.hlsli"));
	Test_assert(t, "idx2 = z_last",     cmpstr(sh.includes.ptr[2].relativePath.ptr, "z_last.hlsli"));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddIncMultipleDistinct(Test *t) {

	Test_setModule(t, "SHFile addInclude: five distinct includes all stored");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	const C8 *paths[] = { "a.hlsli","b.hlsli","c.hlsli","d.hlsli","e.hlsli" };
	for (U8 i = 0; i < 5; ++i) {
		SHInclude inc = { .relativePath = CharString_createRefCStrConst(paths[i]), .crc32c = (U32)(i + 1) };
		Test_assert(t, "insert ok", SHFile_addInclude(&sh, &inc, t->alloc, &t->err));
	}

	Test_assert(t, "count 5", sh.includes.length == 5);

	SHFile_free(&sh, t->alloc);
}

void Test_SHBinaryIdentifierEquals(Test *t) {

	Test_setModule(t, "SHBinaryIdentifier equals");

	SHBinaryIdentifier a = (SHBinaryIdentifier) {
		.entrypoint    = CharString_createRefCStrConst("main"),
		.extensions    = ESHExtension_F64,
		.shaderVersion = OISH_SHADER_MODEL_MIN,
		.stageType     = ESHPipelineStage_Compute
	};

	SHBinaryIdentifier b = a;

	Test_assert(t, "identical equal",     SHBinaryIdentifier_equals(&a, &b));
	Test_assert(t, "NULL == NULL",        SHBinaryIdentifier_equals(NULL, NULL));
	Test_assert(t, "NULL != non-null",    !SHBinaryIdentifier_equals(NULL, &a));
	Test_assert(t, "non-null != NULL",    !SHBinaryIdentifier_equals(&a, NULL));

	//Different entrypoint
	b.entrypoint = CharString_createRefCStrConst("other");
	Test_assert(t, "diff entry",          !SHBinaryIdentifier_equals(&a, &b));
	b.entrypoint = a.entrypoint;

	//Different shader version
	b.shaderVersion = OISH_SHADER_MODEL_MAX;
	Test_assert(t, "diff version",        !SHBinaryIdentifier_equals(&a, &b));
	b.shaderVersion = a.shaderVersion;

	//Bindless ignored
	b.extensions = ESHExtension_F64 | ESHExtension_Bindless;
	Test_assert(t, "Bindless ignored",    SHBinaryIdentifier_equals(&a, &b));

	//UnboundArraySize ignored
	b.extensions = ESHExtension_F64 | ESHExtension_UnboundArraySize;
	Test_assert(t, "UnboundAS ignored",   SHBinaryIdentifier_equals(&a, &b));

	//Both special flags ignored together
	b.extensions = ESHExtension_F64 | ESHExtension_Bindless | ESHExtension_UnboundArraySize;
	Test_assert(t, "bindless x2 ignored", SHBinaryIdentifier_equals(&a, &b));

	//Different meaningful extension
	b.extensions = ESHExtension_I64;
	Test_assert(t, "diff ext",            !SHBinaryIdentifier_equals(&a, &b));
	b.extensions = a.extensions;

	//RT stage grouping: all RT stages treated as RtStart for comparisons
	a.stageType  = ESHPipelineStage_RaygenExt;
	b.stageType  = ESHPipelineStage_MissExt;
	a.entrypoint = CharString_createNull();
	b.entrypoint = CharString_createNull();
	Test_assert(t, "raygen == miss (RT group)", SHBinaryIdentifier_equals(&a, &b));

	b.stageType = ESHPipelineStage_ClosestHitExt;
	Test_assert(t, "raygen == closesthit",      SHBinaryIdentifier_equals(&a, &b));

	//RT vs non-RT must differ
	b.stageType = ESHPipelineStage_Compute;
	Test_assert(t, "RT != compute",             !SHBinaryIdentifier_equals(&a, &b));

	//Different defines.length
	a.stageType = ESHPipelineStage_Compute;
	b.stageType = ESHPipelineStage_Compute;
	a.entrypoint = b.entrypoint = CharString_createRefCStrConst("main");
	a.extensions = b.extensions = ESHExtension_None;

	CharString strs[2] = { CharString_createRefCStrConst("A"), CharString_createRefCStrConst("1") };
	if(!ListCharString_createRefConst(strs, 2, &a.defines, NULL))
		goto skip;

	Test_assert(t, "diff defines.length",    !SHBinaryIdentifier_equals(&a, &b));
	a.defines = (ListCharString) { 0 };

skip:;
}

void Test_SHEntryRuntimeGetCombinations(Test *t) {

	Test_setModule(t, "SHEntryRuntime getCombinations / getCombinationsCompiled");

	Test_assert(t, "null combined == 0",  SHEntryRuntime_getCombinations(NULL)         == 0);
	Test_assert(t, "null compiled == 0",  SHEntryRuntime_getCombinationsCompiled(NULL) == 0);

	SHEntryRuntime rt = (SHEntryRuntime) { 0 };

	//All empty lists: each dimension treated as 1, product == 1

	Test_assert(t, "empty combined == 1", SHEntryRuntime_getCombinations(&rt)         == 1);
	Test_assert(t, "empty compiled == 1", SHEntryRuntime_getCombinationsCompiled(&rt) == 1);

	//Versions

	U16 versions[2] = { OISH_SHADER_MODEL_MIN, OISH_SHADER_MODEL_MAX };
	Test_assert(t, "Create ref versions", ListU16_createRefConst(versions, 2, &rt.shaderVersions, NULL));

	Test_assert(t, "versions combined == 2", SHEntryRuntime_getCombinations(&rt) == 2);
	Test_assert(t, "versions compiled == 2", SHEntryRuntime_getCombinationsCompiled(&rt) == 2);
	rt = (SHEntryRuntime) { 0 };

	//Extensions

	U32 exts[3] = { ESHExtension_None, ESHExtension_F64, ESHExtension_I64 };
	Test_assert(t, "Create ref extensions", ListU32_createRefConst(exts, 3, &rt.extensions, NULL));

	Test_assert(t, "extensions combined == 3", SHEntryRuntime_getCombinations(&rt)         == 3);
	Test_assert(t, "extensions compiled == 3", SHEntryRuntime_getCombinationsCompiled(&rt) == 3);
	rt = (SHEntryRuntime) { 0 };

	//Defines

	U8 dpc[2] = { 1, 1 };
	Test_assert(t, "Create ref defines", ListU8_createRefConst(dpc, 2, &rt.definesPerCompilation, NULL));

	Test_assert(t, "defines combined == 2", SHEntryRuntime_getCombinations(&rt) == 2);
	Test_assert(t, "defines compiled == 2", SHEntryRuntime_getCombinationsCompiled(&rt) == 2);
	rt = (SHEntryRuntime) { 0 };

	//Uniforms: uniformStride = 4, uniformData holds 3 * 4 = 12 bytes = 3 uniform variants

	static U8 udata[12] = { 0 };
	Test_assert(t, "Create uniform data", ListU8_createRefConst(udata, 12, &rt.uniformData, NULL));
	rt.uniformStride = 4;

	Test_assert(t, "uniforms combined == 3", SHEntryRuntime_getCombinations(&rt) == 3);
	Test_assert(t, "uniforms compiled == 1", SHEntryRuntime_getCombinationsCompiled(&rt) == 1);
	rt = (SHEntryRuntime) { 0 };
	
	//Complex: 2 versions, 3 extensions, 2 define groups, 4 uniform variants (stride of 3)
	
	Test_assert(t, "Create complex versions", ListU16_createRefConst(versions, 2, &rt.shaderVersions, NULL));

	Test_assert(t, "Create complex extensions", ListU32_createRefConst(exts, 3, &rt.extensions, NULL));

	Test_assert(t, "Create complex defines", ListU8_createRefConst(dpc, 2, &rt.definesPerCompilation, NULL));

	Test_assert(t, "Create complex uniforms", ListU8_createRefConst(udata, 12, &rt.uniformData, NULL));
	rt.uniformStride = 3;

	Test_assert(t, "complex combined == 48", SHEntryRuntime_getCombinations(&rt)         == 48);
	Test_assert(t, "complex compiled == 12", SHEntryRuntime_getCombinationsCompiled(&rt) == 12);
	rt = (SHEntryRuntime) { 0 };

	//Safety check with zero division

	Test_assert(t, "Create /0 safety uniforms", ListU8_createRefConst(udata, 12, &rt.uniformData, NULL));
	rt.uniformStride = 0;

	Test_assert(t, "create /0 safety combined == 1", SHEntryRuntime_getCombinations(&rt) == 1);
	Test_assert(t, "create /0 safety compiled == 1", SHEntryRuntime_getCombinationsCompiled(&rt) == 1);
}
