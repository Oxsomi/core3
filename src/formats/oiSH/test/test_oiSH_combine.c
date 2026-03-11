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

void Test_SHFileCombineNullGuards(Test *t) {

	Test_setModule(t, "SHFile combine: null guards");

	SHFile sh = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	Test_assert(t, "null a",   !SHFile_combine(NULL, &sh,  t->alloc, &out, NULL));
	Test_assert(t, "null b",   !SHFile_combine(&sh,  NULL, t->alloc, &out, NULL));
	Test_assert(t, "null out", !SHFile_combine(&sh,  &sh,  t->alloc, NULL, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileCombineFlagsMismatch(Test *t) {

	Test_setModule(t, "SHFile combine: mismatched ESHSettingsFlags rejected");

	SHFile shA = (SHFile) { 0 }, shB = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create A", Test_SHFileCreate(t, &shA));
	Test_assert(t, "create B", SHFile_create(ESHSettingsFlags_HideMagicNumber, OXC3_VERSION, 0, t->alloc, &shB, &t->err));

	Test_assert(t, "flags mismatch rejected", !SHFile_combine(&shA, &shB, t->alloc, &out, NULL));

	SHFile_free(&shA, t->alloc);
	SHFile_free(&shB, t->alloc);
}

void Test_SHFileCombineCompilerVersionMismatch(Test *t) {

	Test_setModule(t, "SHFile combine: mismatched compilerVersion rejected");

	SHFile shA = (SHFile) { 0 }, shB = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create A", SHFile_create(ESHSettingsFlags_None, OXC3_VERSION, 0, t->alloc, &shA, &t->err));
	Test_assert(t, "create B", SHFile_create(ESHSettingsFlags_None, OXC3_VERSION + 1, 0, t->alloc, &shB, &t->err));

	Test_assert(t, "version mismatch rejected", !SHFile_combine(&shA, &shB, t->alloc, &out, NULL));

	SHFile_free(&shA, t->alloc);
	SHFile_free(&shB, t->alloc);
}

void Test_SHFileCombineSourceHashMismatch(Test *t) {

	Test_setModule(t, "SHFile combine: mismatched sourceHash rejected");

	SHFile shA = (SHFile) { 0 }, shB = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create A", SHFile_create(ESHSettingsFlags_None, OXC3_VERSION, 0x1111, t->alloc, &shA, &t->err));
	Test_assert(t, "create B", SHFile_create(ESHSettingsFlags_None, OXC3_VERSION, 0x2222, t->alloc, &shB, &t->err));

	Test_assert(t, "hash mismatch rejected", !SHFile_combine(&shA, &shB, t->alloc, &out, NULL));

	SHFile_free(&shA, t->alloc);
	SHFile_free(&shB, t->alloc);
}

void Test_SHFileCombineEntryGroupMismatch(Test *t) {

	Test_setModule(t, "SHFile combine: same entry name but different group rejected");

	SHFile shA = (SHFile) { 0 }, shB = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create A", Test_SHFileCreate(t, &shA));
	Test_assert(t, "create B", Test_SHFileCreate(t, &shB));

	SHBinaryInfo csA = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	SHBinaryInfo csB = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "A addBin", SHFile_addBinary(&shA, &csA, t->alloc, &t->err));
	Test_assert(t, "B addBin", SHFile_addBinary(&shB, &csB, t->alloc, &t->err));

	//Both have entry "cs" but with different group sizes
	Test_assert(t, "A addEntry 8x8", addComputeEntry(&shA, "cs",  8, 8, 1, t, false));
	Test_assert(t, "B addEntry 4x4", addComputeEntry(&shB, "cs",  4, 4, 1, t, false));

	Test_assert(t, "group mismatch rejected", !SHFile_combine(&shA, &shB, t->alloc, &out, NULL));

	SHFile_free(&shA, t->alloc);
	SHFile_free(&shB, t->alloc);
}

void Test_SHFileCombineMergesSPIRVAndDXIL(Test *t) {

	Test_setModule(t, "SHFile combine: SPIRV binary from A and DXIL binary from B merged into one");

	SHFile shA = (SHFile) { 0 }, shB = (SHFile) { 0 }, out = (SHFile) { 0 };
	Test_assert(t, "create A", Test_SHFileCreate(t, &shA));
	Test_assert(t, "create B", Test_SHFileCreate(t, &shB));

	//A: SPIRV only

	SHBinaryInfo infoA = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("main"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = ESHPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
	};

	infoA.binaries[ESHBinaryType_SPIRV] = Buffer_createRefConst(kDummySPIRV, sizeof(kDummySPIRV));
	Test_assert(t, "A addBin",   SHFile_addBinary(&shA, &infoA, t->alloc, &t->err));
	Test_assert(t, "A addEntry", addComputeEntry(&shA, "main", 8, 8, 1, t, false));

	//B: DXIL only, same identifier (length mismatches spirv to check)

	static const U8 dxil[5] = { 0x44, 0x58, 0x42, 0x43, 0x00 };
	SHBinaryInfo infoB = (SHBinaryInfo) {
		.identifier = {
			.entrypoint    = CharString_createRefCStrConst("main"),
			.shaderVersion = OISH_SHADER_MODEL_MIN,
			.stageType     = ESHPipelineStage_Compute
		},
		.vendorMask = (U16)((1u << ESHVendor_Count) - 1)
	};
	infoB.binaries[ESHBinaryType_DXIL] = Buffer_createRefConst(dxil, sizeof(dxil));

	Test_assert(t, "B addBin",   SHFile_addBinary(&shB, &infoB, t->alloc, &t->err));

	Test_assert(t, "B addEntry", addComputeEntry(&shB, "main", 8, 8, 1, t, false));

	Test_assert(t, "combine ok",      SHFile_combine(&shA, &shB, t->alloc, &out, &t->err));
	Test_assert(t, "1 merged binary", out.binaries.length == 1);
	Test_assert(t, "SPIRV present", Buffer_length(out.binaries.ptr[0].binaries[ESHBinaryType_SPIRV]) == sizeof(kDummySPIRV));
	Test_assert(t, "DXIL present", Buffer_length(out.binaries.ptr[0].binaries[ESHBinaryType_DXIL]) == sizeof(dxil));

	SHFile_free(&shA, t->alloc);
	SHFile_free(&shB, t->alloc);
	SHFile_free(&out, t->alloc);
}

void Test_SHFileCombineEquivalentToSequential(Test *t) {

	Test_setModule(t, "SHFile combine: combine(A,B) has same binary/entry count and metadata as sequential file");

	//Sequential reference
	SHFile seq = (SHFile) { 0 };
	Test_assert(t, "create seq", Test_SHFileCreate(t, &seq));

	SHBinaryInfo csSeq = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	SHBinaryInfo vsSeq = makeBinaryInfo(ESHPipelineStage_Vertex,  "vsMain", false);
	Test_assert(t, "seq add cs", SHFile_addBinary(&seq, &csSeq, t->alloc, &t->err));
	Test_assert(t, "seq add vs", SHFile_addBinary(&seq, &vsSeq, t->alloc, &t->err));
	Test_assert(t, "seq entry cs", addComputeEntry(&seq, "csMain", 8, 8, 1, t, false));

	{
		SHEntry ev = (SHEntry) { 0 };
		ev.name  = CharString_createRefCStrConst("vsMain");
		ev.stage = ESHPipelineStage_Vertex;
		U16 bid = 1;
		Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &ev.binaryIds, &t->err));
		Test_assert(t, "seq entry vs", SHFile_addEntrypoint(&seq, &ev, t->alloc, &t->err));
	}

	//A (compute only) and B (vertex only)

	SHFile shA = (SHFile) { 0 }, shB = (SHFile) { 0 }, combined = (SHFile) { 0 };
	Test_assert(t, "create A", Test_SHFileCreate(t, &shA));
	Test_assert(t, "create B", Test_SHFileCreate(t, &shB));

	SHBinaryInfo csA = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	Test_assert(t, "A addBin",   SHFile_addBinary(&shA, &csA, t->alloc, &t->err));
	Test_assert(t, "A addEntry", addComputeEntry(&shA, "csMain", 8, 8, 1, t, false));

	SHBinaryInfo vsB = makeBinaryInfo(ESHPipelineStage_Vertex, "vsMain", false);
	Test_assert(t, "B addBin", SHFile_addBinary(&shB, &vsB, t->alloc, &t->err));
	{
		SHEntry ev = (SHEntry) { 0 };
		ev.name  = CharString_createRefCStrConst("vsMain");
		ev.stage = ESHPipelineStage_Vertex;
		U16 bid = 0;
		Test_assert(t, "createRef", ListU16_createRefConst(&bid, 1, &ev.binaryIds, &t->err));
		Test_assert(t, "B addEntry", SHFile_addEntrypoint(&shB, &ev, t->alloc, &t->err));
	}

	Test_assert(t, "combine ok", SHFile_combine(&shA, &shB, t->alloc, &combined, &t->err));

	Test_assert(t, "seq 2 binaries",      seq.binaries.length      == 2);
	Test_assert(t, "combined 2 binaries", combined.binaries.length == 2);
	Test_assert(t, "seq 2 entries",       seq.entries.length       == 2);
	Test_assert(t, "combined 2 entries",  combined.entries.length  == 2);
	Test_assert(t, "compilerVersion eq",  combined.compilerVersion == seq.compilerVersion);
	Test_assert(t, "sourceHash eq",       combined.sourceHash      == seq.sourceHash);

	SHFile_free(&seq,      t->alloc);
	SHFile_free(&shA,      t->alloc);
	SHFile_free(&shB,      t->alloc);
	SHFile_free(&combined, t->alloc);
}

void Test_SHFileCombineIncludesMatchSequential(Test *t) {

	Test_setModule(t, "SHFile combine: combined includes count matches sequential file");

	SHFile seq = (SHFile) { 0 };
	Test_assert(t, "create seq", Test_SHFileCreate(t, &seq));
	SHBinaryInfo seqBin = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "seq addBin",   SHFile_addBinary(&seq, &seqBin, t->alloc, &t->err));
	Test_assert(t, "seq addEntry", addComputeEntry(&seq, "cs", 4, 4, 1, t, false));
	SHInclude iA = { .relativePath = CharString_createRefCStrConst("a.hlsli"), .crc32c = 0x0001 };
	SHInclude iB = { .relativePath = CharString_createRefCStrConst("b.hlsli"), .crc32c = 0x0002 };
	Test_assert(t, "seq inc A", SHFile_addInclude(&seq, &iA, t->alloc, &t->err));
	Test_assert(t, "seq inc B", SHFile_addInclude(&seq, &iB, t->alloc, &t->err));

	SHFile shA = (SHFile) { 0 }, shB = (SHFile) { 0 }, combined = (SHFile) { 0 };
	Test_assert(t, "create A", Test_SHFileCreate(t, &shA));
	Test_assert(t, "create B", Test_SHFileCreate(t, &shB));

	SHBinaryInfo csA = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	SHBinaryInfo csB = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "A addBin",   SHFile_addBinary(&shA, &csA, t->alloc, &t->err));
	Test_assert(t, "A addEntry", addComputeEntry(&shA, "cs", 4, 4, 1, t, false));
	Test_assert(t, "B addBin",   SHFile_addBinary(&shB, &csB, t->alloc, &t->err));
	Test_assert(t, "B addEntry", addComputeEntry(&shB, "cs", 4, 4, 1, t, false));

	SHInclude incA = { .relativePath = CharString_createRefCStrConst("a.hlsli"), .crc32c = 0x0001 };
	SHInclude incB = { .relativePath = CharString_createRefCStrConst("b.hlsli"), .crc32c = 0x0002 };
	Test_assert(t, "A inc", SHFile_addInclude(&shA, &incA, t->alloc, &t->err));
	Test_assert(t, "B inc", SHFile_addInclude(&shB, &incB, t->alloc, &t->err));

	Test_assert(t, "combine ok", SHFile_combine(&shA, &shB, t->alloc, &combined, &t->err));

	Test_assert(t, "seq 2 includes",      seq.includes.length      == 2);
	Test_assert(t, "combined 2 includes", combined.includes.length == 2);

	SHFile_free(&seq,      t->alloc);
	SHFile_free(&shA,      t->alloc);
	SHFile_free(&shB,      t->alloc);
	SHFile_free(&combined, t->alloc);
}
