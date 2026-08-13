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

//formats/oiSH/test/test_oiSH_entrypoint.c

#include "test_oiSH_shared.h"
#include "types/container/list_basic_types.h"

void Test_SHFileAddEntryNullGuards(Test *t) {

	Test_setModule(t, "SHFile addEntry: null guards");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("main");
	e.stage  = ESHPipelineStage_Compute;
	e.groupX = e.groupY = e.groupZ = 4;

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "null shFile", !SHFile_addEntrypoint(NULL, &e,   t->alloc, NULL));
	Test_assert(t, "null entry",  !SHFile_addEntrypoint(&sh,  NULL, t->alloc, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryEmptyName(Test *t) {

	Test_setModule(t, "SHFile addEntry: empty name rejected");

	U16 binId = 0;
	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	Test_assert(t, "empty name rejected", !addComputeEntry(&sh, "", 4, 4, 1, t, true, &binId));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryInvalidStage(Test *t) {

	Test_setModule(t, "SHFile addEntry: stage >= Count rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHEntry e = (SHEntry) { 0 };
	e.name  = CharString_createRefCStrConst("bad");
	e.stage = (SHPipelineStage)ESHPipelineStage_Count;

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "out-of-range stage rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryComputeInvalidGroup(Test *t) {

	Test_setModule(t, "SHFile addEntry: compute zero group rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	U16 binId = 0;

	Test_assert(t, "0x0x0 rejected",          !addComputeEntry(&sh, "main", 0,   0,   0, t, true, &binId));

	Test_assert(t, "32x32x1 = 1024 rejected", !addComputeEntry(&sh, "big",  32,  32,  1, t, true, &binId));
	Test_assert(t, "512x1x1 = 512  accepted",  addComputeEntry(&sh, "max", 512,   1,  1, t, false, &binId));
	Test_assert(t, "1x512x1      accepted",    addComputeEntry(&sh, "max2",  1, 512,  1, t, false, &binId));

	Test_assert(t, "groupZ = 64 accepted",     addComputeEntry(&sh,  "ok",   1,   1, 64, t, false, &binId));
	Test_assert(t, "groupZ = 65 rejected",    !addComputeEntry(&sh, "bad",   1,   1, 65, t, true, &binId));

	Test_assert(t, "8x8x1",                    addComputeEntry(&sh, "cs1",   8,   8,  1, t, false, &binId));
	Test_assert(t, "64x1x1",                   addComputeEntry(&sh, "cs2",  64,   1,  1, t, false, &binId));
	Test_assert(t, "1x1x64",                   addComputeEntry(&sh, "cs3",   1,   1, 64, t, false, &binId));

	Test_assert(t, "count 6", sh.entries.length == 6);

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryGraphicsCannotHaveGroup(Test *t) {

	Test_setModule(t, "SHFile addEntry: group size on pure-graphics stages rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	const ESHPipelineStage gfx[] = {
		ESHPipelineStage_Vertex, ESHPipelineStage_Pixel,
		ESHPipelineStage_GeometryExt, ESHPipelineStage_Hull, ESHPipelineStage_Domain
	};

	for (U8 i = 0; i < 5; ++i) {
		SHEntry e = (SHEntry) { 0 };
		e.name   = CharString_createRefCStrConst("g");
		e.stage  = gfx[i];
		e.groupX = 8;
		Test_assert(t, "group on gfx rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	}

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryMeshNeedsGroup(Test *t) {

	Test_setModule(t, "SHFile addEntry: mesh requires group size");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHEntry e = (SHEntry) { 0 };
	e.name  = CharString_createRefCStrConst("meshMain");
	e.stage = ESHPipelineStage_MeshExt;

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_MeshExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "zero group rejected",  !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	e.groupX = 32; e.groupY = 1; e.groupZ = 1;
	Test_assert(t, "valid group accepted",  SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryTaskNeedsGroup(Test *t) {

	Test_setModule(t, "SHFile addEntry: task requires group size");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));
	SHEntry e = (SHEntry) { 0 };
	e.name  = CharString_createRefCStrConst("taskMain");
	e.stage = ESHPipelineStage_TaskExt;

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_TaskExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "zero group rejected",  !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	e.groupX = 64; e.groupY = 1; e.groupZ = 1;
	Test_assert(t, "valid group accepted",  SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryWaveSizeOnNonCompute(Test *t) {

	Test_setModule(t, "SHFile addEntry: waveSize on non-compute/workgraph rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHEntry e = (SHEntry) { 0 };
	e.name     = CharString_createRefCStrConst("vsMain");
	e.stage    = ESHPipelineStage_Vertex;
	e.waveSize = 0x0003;

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Vertex, "vs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "waveSize on vertex rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryWaveSizeInvalidNibbles(Test *t) {

	Test_setModule(t, "SHFile addEntry: any waveSize nibble > 9 rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "cs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	//Each nibble position loaded with 0xA (=10 > 9)
	const U16 badWaves[] = { 0x000A, 0x00A0, 0x0A00, 0xA000 };

	for (U8 i = 0; i < 4; ++i) {

		SHEntry e = (SHEntry) { 0 };
		e.name     = CharString_createRefCStrConst("cs");
		e.stage    = ESHPipelineStage_Compute;
		e.groupX   = 8; e.groupY = 8; e.groupZ = 1;
		e.waveSize = badWaves[i];

		U16 binId = 0;
		e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

		Test_assert(t, "nibble>9 rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	}

	//nibble value 3 (maps to wave-8) is valid
	SHEntry good = (SHEntry) { 0 };
	good.name     = CharString_createRefCStrConst("cs");
	good.stage    = ESHPipelineStage_Compute;
	good.groupX   = 8; good.groupY = 8; good.groupZ = 1;
	good.waveSize = 0x0003;

	U16 binId = 0;
	good.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "valid waveSize accepted", SHFile_addEntrypoint(&sh, &good, t->alloc, &t->err));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryWaveSizeWorkgraph(Test *t) {

	Test_setModule(t, "SHFile addEntry: waveSize accepted on workgraph");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_WorkgraphExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name     = CharString_createRefCStrConst("nodeMain");
	e.stage    = ESHPipelineStage_WorkgraphExt;
	e.groupX   = 16; e.groupY = 1; e.groupZ = 1;
	e.waveSize = 0x0003;

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "workgraph waveSize accepted", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryMissMustHavePayload(Test *t) {

	Test_setModule(t, "SHFile addEntry: miss payloadSize rules");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_MissExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name  = CharString_createRefCStrConst("missMain");
	e.stage = ESHPipelineStage_MissExt;

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "zero payload rejected",     !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	e.payloadSize = 200;
	Test_assert(t, "payload > 128 rejected",    !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	e.payloadSize = 128;
	Test_assert(t, "payload == 128 accepted",    SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryCallablePayload(Test *t) {

	Test_setModule(t, "SHFile addEntry: callable payloadSize rules");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_CallableExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name  = CharString_createRefCStrConst("callMain");
	e.stage = ESHPipelineStage_CallableExt;

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "zero payload rejected",  !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	e.payloadSize = 16;
	Test_assert(t, "payload = 16 accepted",   SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryHitStageSizes(Test *t) {

	Test_setModule(t, "SHFile addEntry: closesthit/anyhit/intersection payload+intersection sizes");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_ClosestHitExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	const ESHPipelineStage hitStages[] = {
		ESHPipelineStage_ClosestHitExt,
		ESHPipelineStage_AnyHitExt,
		ESHPipelineStage_IntersectionExt
	};

	for (U8 i = 0; i < 3; ++i) {

		Bool isIntersection = hitStages[i] == ESHPipelineStage_IntersectionExt;

		SHEntry e = (SHEntry) { 0 };
		e.name        = CharString_createRefCStrConst("hit");
		e.stage       = hitStages[i];

		//Intersection shaders never receive a ray payload (they only produce a hit attribute via ReportHit),
		// so payloadSize must be 0 there and non-zero is rejected.

		e.payloadSize = isIntersection ? 0 : 16;

		U16 binId = 0;
		e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

		if (isIntersection) {
			e.payloadSize = 16;
			Test_assert(t, "payloadSize on intersection rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
			e.payloadSize = 0;
		}

		//Missing intersectionSize (required for closesthit/anyhit, optional for intersection)
		if (!isIntersection)
			Test_assert(t, "no intersectionSize rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));

		//intersectionSize > 32
		e.intersectionSize = 64;
		Test_assert(t, "intersectionSize > 32 rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));

		//Valid boundary
		e.intersectionSize = 32;
		Test_assert(t, "intersectionSize == 32 accepted", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));
	}

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryIntersectionOnWrongStage(Test *t) {

	Test_setModule(t, "SHFile addEntry: intersectionSize on miss/callable rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_MissExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	const ESHPipelineStage wrong[] = { ESHPipelineStage_MissExt, ESHPipelineStage_CallableExt };

	for (U8 i = 0; i < 2; ++i) {

		SHEntry e = (SHEntry) { 0 };
		e.name             = CharString_createRefCStrConst("rt");
		e.stage            = wrong[i];
		e.payloadSize      = 16;
		e.intersectionSize = 8;

		U16 binId = 0;
		e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

		Test_assert(t, "intersectionSize on wrong stage", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	}

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryRaygenNoPayload(Test *t) {

	Test_setModule(t, "SHFile addEntry: raygen accepts rejects payload");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_RaygenExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name  = CharString_createRefCStrConst("rgen");
	e.stage = ESHPipelineStage_RaygenExt;

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "raygen accepted",             SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));

	SHEntry e2 = (SHEntry) { 0 };
	e2.name        = CharString_createRefCStrConst("rgen2");
	e2.stage       = ESHPipelineStage_RaygenExt;
	e2.payloadSize = 16;
	e2.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "raygen + payload rejected",  !SHFile_addEntrypoint(&sh, &e2, t->alloc, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryNonRTPayloadRejected(Test *t) {

	Test_setModule(t, "SHFile addEntry: payloadSize on non-RT rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Vertex, "vs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	info = makeBinaryInfo(ESHPipelineStage_Compute, "comp", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	//Vertex + payload
	SHEntry ev = (SHEntry) { 0 };
	ev.name = CharString_createRefCStrConst("vs"); ev.stage = ESHPipelineStage_Vertex;
	ev.payloadSize = 16;

	U16 binId = 0;
	ev.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "vertex + payload rejected", !SHFile_addEntrypoint(&sh, &ev, t->alloc, NULL));

	//Compute + payload
	SHEntry ec = (SHEntry) { 0 };
	ec.name = CharString_createRefCStrConst("cs"); ec.stage = ESHPipelineStage_Compute;
	ec.groupX = ec.groupY = ec.groupZ = 4;
	ec.payloadSize = 16;

	U16 binId0 = 1;
	ev.binaryIds = (ListU16) { .ptr = &binId0, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "compute + payload rejected", !SHFile_addEntrypoint(&sh, &ec, t->alloc, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryIOOnlyGraphics(Test *t) {

	Test_setModule(t, "SHFile addEntry: outputs/inputs only valid for graphics stages");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "comp", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	info = makeBinaryInfo(ESHPipelineStage_RaygenExt, NULL, true);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	//Compute with non-zero output
	SHEntry ec = (SHEntry) { 0 };
	ec.name = CharString_createRefCStrConst("cs"); ec.stage = ESHPipelineStage_Compute;
	ec.groupX = ec.groupY = ec.groupZ = 4;
	ec.outputsU64[0] = 0x01;

	U16 binId = 0;
	ec.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "outputs on compute rejected", !SHFile_addEntrypoint(&sh, &ec, t->alloc, NULL));

	//Raygen with non-zero input
	SHEntry er = (SHEntry) { 0 };
	er.name = CharString_createRefCStrConst("rgen"); er.stage = ESHPipelineStage_RaygenExt;
	er.inputsU64[0] = 0x01;

	U16 binId0 = 1;
	er.binaryIds = (ListU16) { .ptr = &binId0, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "inputs on raygen rejected", !SHFile_addEntrypoint(&sh, &er, t->alloc, NULL));

	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryUniqueInputSemanticsLimit(Test *t) {

	Test_setModule(t, "SHFile addEntry: uniqueInputSemantics >= 16 rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Vertex, "vs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name                 = CharString_createRefCStrConst("vsMain");
	e.stage                = ESHPipelineStage_Vertex;
	e.uniqueInputSemantics = 16;

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	Test_assert(t, "uniqueInputSemantics = 16 rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntrySemanticNamesLessThanUniqueInputs(Test *t) {

	Test_setModule(t, "SHFile addEntry: semanticNames.length < uniqueInputSemantics rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Vertex, "vs", false);
	Test_assert(t, "addBinary", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name                 = CharString_createRefCStrConst("vsMain");
	e.stage                = ESHPipelineStage_Vertex;
	e.uniqueInputSemantics = 2;

	U16 binId = 0;
	e.binaryIds = (ListU16) { .ptr = &binId, .length = 1, .capacityAndRefInfo = U64_MAX };

	//Only 1 name but 2 unique inputs claimed
	CharString sem = CharString_createRefCStrConst("TEXCOORD");
	Test_assert(t, "createRef semantic", ListCharString_createRefConst(&sem, 1, &e.semanticNames, NULL));
	Test_assert(t, "too-few semantic names rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileAddEntryBinaryIdOutOfBounds(Test *t) {

	Test_setModule(t, "SHFile addEntry: binaryId referencing non-existent binary rejected");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	SHEntry e = (SHEntry) { 0 };
	e.name   = CharString_createRefCStrConst("cs");
	e.stage  = ESHPipelineStage_Compute;
	e.groupX = e.groupY = e.groupZ = 4;

	//Point to binary index 0 even though no binaries exist

	U16 badId = 0;
	Test_assert(t, "createRef binaryIds", ListU16_createRefConst(&badId, 1, &e.binaryIds, NULL));

	Test_assert(t, "out-of-bounds binaryId rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	e.binaryIds = (ListU16) { 0 };
	Test_assert(t, "no binaryIds rejected", !SHFile_addEntrypoint(&sh, &e, t->alloc, NULL));
	SHFile_free(&sh, t->alloc);
}

void Test_SHFileEntryAndBinaryLinked(Test *t) {

	Test_setModule(t, "SHFile: entry referencing a valid binary stored correctly");

	SHFile sh = (SHFile) { 0 };
	Test_assert(t, "create", Test_SHFileCreate(t, &sh));

	//Add binary first, then entry that references it at index 0
	SHBinaryInfo info = makeBinaryInfo(ESHPipelineStage_Compute, "csMain", false);
	Test_assert(t, "addBinary ok", SHFile_addBinary(&sh, &info, t->alloc, &t->err));

	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst("csMain");
	e.stage = ESHPipelineStage_Compute;
	e.groupX = 8; e.groupY = 8; e.groupZ = 1;
	U16 binaryId = 0;
	Test_assert(t, "createRef", ListU16_createRefConst(&binaryId, 1, &e.binaryIds, &t->err));
	Test_assert(t, "addEntrypoint ok", SHFile_addEntrypoint(&sh, &e, t->alloc, &t->err));
	Test_assert(t, "1 entry", sh.entries.length == 1);
	Test_assert(t, "1 binary", sh.binaries.length == 1);
	Test_assert(t, "binaryId correct", sh.entries.ptr[0].binaryIds.ptr[0] == 0);

	SHFile_free(&sh, t->alloc);
}
