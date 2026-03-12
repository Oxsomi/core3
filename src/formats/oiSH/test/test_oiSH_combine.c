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
#include "types/base/string_read_helper.h"

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

	U16 binId = 0;

	//Both have entry "cs" but with different group sizes
	Test_assert(t, "A addEntry 8x8", addComputeEntry(&shA, "cs",  8, 8, 1, t, false, &binId));
	Test_assert(t, "B addEntry 4x4", addComputeEntry(&shB, "cs",  4, 4, 1, t, false, &binId));

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

	U16 binId = 0;

	infoA.binaries[ESHBinaryType_SPIRV] = Buffer_createRefConst(kDummySPIRV, sizeof(kDummySPIRV));
	Test_assert(t, "A addBin",   SHFile_addBinary(&shA, &infoA, t->alloc, &t->err));
	Test_assert(t, "A addEntry", addComputeEntry(&shA, "main", 8, 8, 1, t, false, &binId));

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

	Test_assert(t, "B addEntry", addComputeEntry(&shB, "main", 8, 8, 1, t, false, &binId));

	Test_assert(t, "combine ok",      SHFile_combine(&shA, &shB, t->alloc, &out, &t->err));
	Test_assert(t, "1 merged binary", out.binaries.length == 1);
	Test_assert(t, "SPIRV present", Buffer_length(out.binaries.ptr[0].binaries[ESHBinaryType_SPIRV]) == sizeof(kDummySPIRV));
	Test_assert(t, "DXIL present", Buffer_length(out.binaries.ptr[0].binaries[ESHBinaryType_DXIL]) == sizeof(dxil));

	SHFile_free(&shA, t->alloc);
	SHFile_free(&shB, t->alloc);
	SHFile_free(&out, t->alloc);
}

static inline Bool cmpstr(const C8 *a, const C8 *b) {
	const CharString str = CharString_createRefCStrConst(a);
	return CharString_equalsCStringSensitive(&str, b);
}

void Test_SHFileCombineEquivalentToSequential(Test *t) {

	Test_setModule(t, "SHFile combine: combine(A,B) has same binary/entry count and metadata as sequential file");

	//Sequential reference
	SHFile seq = (SHFile) { 0 };
	Test_assert(t, "create seq", Test_SHFileCreate(t, &seq));

	U16 binId = 0;
	SHBinaryInfo csSeq = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	SHBinaryInfo vsSeq = makeBinaryInfo(ESHPipelineStage_Vertex,  "vsMain", false);
	Test_assert(t, "seq add cs", SHFile_addBinary(&seq, &csSeq, t->alloc, &t->err));
	Test_assert(t, "seq add vs", SHFile_addBinary(&seq, &vsSeq, t->alloc, &t->err));
	Test_assert(t, "seq entry cs", addComputeEntry(&seq, "csMain", 8, 8, 1, t, false, &binId));

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
	Test_assert(t, "A addEntry", addComputeEntry(&shA, "csMain", 8, 8, 1, t, false, &binId));

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

	//Validate binary order: seq[0]=cs, seq[1]=vs; combined must preserve that order

	if (seq.binaries.length == 2 && combined.binaries.length == 2) {

		Test_assert(t, "seq bin[0] stage cs", seq.binaries.ptr[0].identifier.stageType == ESHPipelineStage_Compute);
		Test_assert(t, "combined bin[0] stage cs", combined.binaries.ptr[0].identifier.stageType == ESHPipelineStage_Compute);

		Test_assert(t, "seq bin[1] stage vs", seq.binaries.ptr[1].identifier.stageType == ESHPipelineStage_Vertex);
		Test_assert(t, "combined bin[1] stage vs", combined.binaries.ptr[1].identifier.stageType == ESHPipelineStage_Vertex);

		Test_assert(t, "cs entrypoint eq",
			cmpstr(seq.binaries.ptr[0].identifier.entrypoint.ptr, "csMain") &&
			cmpstr(combined.binaries.ptr[0].identifier.entrypoint.ptr, "csMain")
		);

		Test_assert(t, "vs entrypoint eq",
			cmpstr(seq.binaries.ptr[1].identifier.entrypoint.ptr, "vsMain") &&
			cmpstr(combined.binaries.ptr[1].identifier.entrypoint.ptr, "vsMain")
		);

		Test_assert(t, "binary contents cs eq",  SHBinaryInfo_equalsExact(&combined.binaries.ptr[0], &seq.binaries.ptr[0]));
		Test_assert(t, "binary contents vs eq",  SHBinaryInfo_equalsExact(&combined.binaries.ptr[1], &seq.binaries.ptr[1]));
	}

	//Validate entry order: seq[0]=cs, seq[1]=vs

	if (seq.entries.length == 2 && combined.entries.length == 2) {

		Test_assert(t, "seq entry[0] stage cs", seq.entries.ptr[0].stage == ESHPipelineStage_Compute);
		Test_assert(t, "combined entry[0] stage cs", combined.entries.ptr[0].stage == ESHPipelineStage_Compute);

		Test_assert(t, "seq entry[1] stage vs", seq.entries.ptr[1].stage == ESHPipelineStage_Vertex);
		Test_assert(t, "combined entry[1] stage vs", combined.entries.ptr[1].stage == ESHPipelineStage_Vertex);

		Test_assert(t, "cs group eq",
			combined.entries.ptr[0].groupX == seq.entries.ptr[0].groupX &&
			combined.entries.ptr[0].groupY == seq.entries.ptr[0].groupY &&
			combined.entries.ptr[0].groupZ == seq.entries.ptr[0].groupZ
		);

		Test_assert(t, "combined entry[0] eq seq", SHEntry_equals(&combined.entries.ptr[0], &seq.entries.ptr[0]));
		Test_assert(t, "combined entry[1] eq seq", SHEntry_equals(&combined.entries.ptr[1], &seq.entries.ptr[1]));

		//binaryIds must resolve back to the right stage

		if (combined.entries.ptr[0].binaryIds.length == 1)
			Test_assert(t, "cs binaryId points to cs binary",
				combined.binaries.ptr[combined.entries.ptr[0].binaryIds.ptr[0]].identifier.stageType ==
				ESHPipelineStage_Compute
			);

		if (combined.entries.ptr[1].binaryIds.length == 1)
			Test_assert(t, "vs binaryId points to vs binary",
				combined.binaries.ptr[combined.entries.ptr[1].binaryIds.ptr[0]].identifier.stageType ==
				ESHPipelineStage_Vertex
			);
	}

	SHFile_free(&seq,      t->alloc);
	SHFile_free(&shA,      t->alloc);
	SHFile_free(&shB,      t->alloc);
	SHFile_free(&combined, t->alloc);
}

void Test_SHFileCombineIncludesMatchSequential(Test *t) {

	Test_setModule(t, "SHFile combine: combined includes count matches sequential file");

	U16 binId = 0;
	SHFile seq = (SHFile) { 0 };
	Test_assert(t, "create seq", Test_SHFileCreate(t, &seq));
	SHBinaryInfo seqBin = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "seq addBin",   SHFile_addBinary(&seq, &seqBin, t->alloc, &t->err));
	Test_assert(t, "seq addEntry", addComputeEntry(&seq, "cs", 4, 4, 1, t, false, &binId));
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
	Test_assert(t, "A addEntry", addComputeEntry(&shA, "cs", 4, 4, 1, t, false, &binId));
	Test_assert(t, "B addBin",   SHFile_addBinary(&shB, &csB, t->alloc, &t->err));
	Test_assert(t, "B addEntry", addComputeEntry(&shB, "cs", 4, 4, 1, t, false, &binId));

	SHInclude incA = { .relativePath = CharString_createRefCStrConst("a.hlsli"), .crc32c = 0x0001 };
	SHInclude incB = { .relativePath = CharString_createRefCStrConst("b.hlsli"), .crc32c = 0x0002 };
	Test_assert(t, "A inc", SHFile_addInclude(&shA, &incA, t->alloc, &t->err));
	Test_assert(t, "B inc", SHFile_addInclude(&shB, &incB, t->alloc, &t->err));

	Test_assert(t, "combine ok", SHFile_combine(&shA, &shB, t->alloc, &combined, &t->err));

	Test_assert(t, "seq 2 includes",      seq.includes.length      == 2);
	Test_assert(t, "combined 2 includes", combined.includes.length == 2);

	//Validate include content and order: a.hlsli first, b.hlsli second in both files
	//Note: SHFile_addInclude inserts sorted, so order is determined by path, not insertion order.

	if (seq.includes.length == 2 && combined.includes.length == 2) {

		//Sorted alphabetically: a.hlsli < b.hlsli

		Test_assert(t, "seq inc[0] path",      cmpstr(seq.includes.ptr[0].relativePath.ptr, "a.hlsli"));
		Test_assert(t, "seq inc[1] path",      cmpstr(seq.includes.ptr[1].relativePath.ptr, "b.hlsli"));
		Test_assert(t, "combined inc[0] path", cmpstr(combined.includes.ptr[0].relativePath.ptr, "a.hlsli"));
		Test_assert(t, "combined inc[1] path", cmpstr(combined.includes.ptr[1].relativePath.ptr, "b.hlsli"));

		Test_assert(t, "seq inc[0] crc",      seq.includes.ptr[0].crc32c == 0x0001);
		Test_assert(t, "seq inc[1] crc",      seq.includes.ptr[1].crc32c == 0x0002);
		Test_assert(t, "combined inc[0] crc", combined.includes.ptr[0].crc32c == 0x0001);
		Test_assert(t, "combined inc[1] crc", combined.includes.ptr[1].crc32c == 0x0002);
	}

	SHFile_free(&seq,      t->alloc);
	SHFile_free(&shA,      t->alloc);
	SHFile_free(&shB,      t->alloc);
	SHFile_free(&combined, t->alloc);
}
