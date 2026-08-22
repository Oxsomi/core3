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

//formats/oiSP/test/test_oiSP_main.c

#include "formats/oiSP/sp_file.h"
#include "formats/oiSH/sh_file.h"
#include "formats/oiSB/sb_variable.h"
#include "types/test/test.h"
#include "types/container/memory_stream.h"
#include "types/container/test/basic_alloc.h"
#include "types/container/texture_format.h"
#include "types/container/string.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"

//The shader entries are built by hand and referenced, rather than added through SHFile_addEntrypoint, so a case can
// describe exactly the signature it wants without satisfying every unrelated rule that adding an entrypoint enforces.

static SHEntry entryOf(const C8 *name, ESHPipelineStage stage) {
	SHEntry e = (SHEntry) { 0 };
	e.name = CharString_createRefCStrConst(name);
	e.stage = (SHPipelineStage) stage;
	return e;
}

static SHFile fileOf(SHEntry *entries, U64 count) {
	SHFile sh = (SHFile) { 0 };
	ListSHEntry_createRefConst(entries, count, &sh.entries, NULL);
	return sh;
}

static SPStageRef refOf(U16 fileId, U16 entryId) {
	return (SPStageRef) { .fileId = fileId, .entryId = entryId };
}

static const SPSpecialization *findField(const SPFile *sp, U32 pipelineId, ESPField field, U8 index) {

	const SPPipelineBase pipeline = sp->pipelines.ptr[pipelineId];

	for (U32 i = 0; i < pipeline.specializationCount; ++i) {

		const SPSpecialization *spec = &sp->specializations.ptr[pipeline.specializationStart + i];

		if(spec->field == (U8) field && spec->index == index)
			return spec;
	}

	return NULL;
}

//Write then read back an SPFile.
//Caller owns *archive (RefPtr_dec) and *result (SPFile_free).

static Bool spRoundTrip(
	Test *t, const SPFile *src, Bool subFile, StreamRef **archive, SPFile *result, const RefPtrType *type
) {

	StreamRef *sr = NULL;

	if(!MemoryStream_create(0, EMemoryStreamFlags_WriteResize, type, &sr, &t->err))
		return false;

	U64 off = 0;

	if (!SPFile_write(src, t->alloc, sr, &off, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	U64 readOff = 0;

	if (!SPFile_read(sr, &readOff, subFile, t->alloc, result, &t->err)) {
		RefPtr_dec(&sr);
		return false;
	}

	*archive = sr;
	return true;
}

//A graphics pipeline survives a write/read cycle with its state, stages and provenance intact.

void Test_SPFileRoundTrip(Test *t) {

	Test_setModule(t, "SPFile: a graphics pipeline round trips");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	SHEntry entries[2] = { entryOf("mainVS", ESHPipelineStage_Vertex), entryOf("mainPS", ESHPipelineStage_Pixel) };
	entries[0].inputs[0] = ESBType_F32x3;
	entries[1].outputs[0] = ESBType_F32x4;

	SHFile sh = fileOf(entries, 2);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	ListCharString names = (ListCharString) { 0 };
	CharString shaderName = CharString_createRefCStrConst("scene.oiSH");
	ListCharString_createRefConst(&shaderName, 1, &names, NULL);

	SPFile sp = (SPFile) { 0 };
	SPFile read = (SPFile) { 0 };
	StreamRef *archive = NULL;
	U32 pipelineId = U32_MAX;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[2] = { refOf(0, 0), refOf(0, 1) };
	CharString pipeName = CharString_createRefCStrConst("opaque");

	if(!Test_assert(t, "derive", SPFile_derivePipeline(
		&sp, &files, &names, pipeName, stages, 2, t->alloc, &pipelineId, &t->err
	)))
		goto clean;

	//A supplied field has to survive as supplied, not collapse back to assumed.

	Test_assert(t, "supply", SPFile_supply(
		&sp, pipelineId, ESPField_RenderTargetFormat, 0, (U32) ETextureFormatId_RGBA16f, &t->err
	));

	const U64 assumedBefore = SPFile_assumedCount(&sp, pipelineId);
	Test_assert(t, "finalize", SPFile_finalize(&sp, t->alloc, &t->err));

	if(!Test_assert(t, "roundTrip", spRoundTrip(t, &sp, false, &archive, &read, &type)))
		goto clean;

	Test_assert(t, "onePipeline", read.pipelines.length == 1);
	Test_assert(t, "twoStages", read.stages.length == 2);
	Test_assert(t, "sameHash", read.hash == sp.hash);
	Test_assert(t, "sameAssumed", SPFile_assumedCount(&read, 0) == assumedBefore);

	const SPPipelineBase pipeline = read.pipelines.ptr[0];

	Test_assert(t, "isGraphics", pipeline.type == (U8) ESPPipelineType_Graphics);
	Test_assert(t, "stageCount", pipeline.stageCount == 2);
	const SPGraphicsState *readGfx = SPFile_graphicsState(&read, 0);

	if (Test_assert(t, "hasGraphicsState", readGfx != NULL)) {
		Test_assert(t, "targetFormatKept", readGfx->renderTargetFormats[0] == (U8) ETextureFormatId_RGBA16f);
		Test_assert(
			t, "vertexFormatDerived",
			readGfx->inputAssembler.vertexLayout.attributes[0].format == (U8) ETextureFormatId_RGB32f
		);
	}

	const SPSpecialization *rtv = findField(&read, 0, ESPField_RenderTargetFormat, 0);

	if(Test_assert(t, "rtvPresent", rtv != NULL))
		Test_assert(t, "rtvStillSupplied", rtv->source == ESPFieldSource_Supplied);

	//The stage's shader name and entrypoint are what let a loader resolve it again.

	const SPStage stage0 = read.stages.ptr[pipeline.stageStart];

	Test_assert(t, "shaderNameStored", stage0.shaderFile != U32_MAX);
	Test_assert(t, "entrypointStored", stage0.entrypoint != U32_MAX);

	if (stage0.shaderFile != U32_MAX && stage0.entrypoint != U32_MAX) {

		const CharString file = read.names.entryStrings.ptr[stage0.shaderFile];
		const CharString entry = read.names.entryStrings.ptr[stage0.entrypoint];
		const CharString expectFile = CharString_createRefCStrConst("scene.oiSH");
		const CharString expectEntry = CharString_createRefCStrConst("mainVS");

		Test_assert(t, "shaderName", CharString_equalsStringSensitive(&file, &expectFile));
		Test_assert(t, "entrypoint", CharString_equalsStringSensitive(&entry, &expectEntry));
	}

clean:
	RefPtr_dec(&archive);
	SPFile_free(&read, t->alloc);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListCharString_free(&names, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

//Compute is exact, ray tracing needs only its own limits, and several pipelines share one file.

void Test_SPFileManyPipelines(Test *t) {

	Test_setModule(t, "SPFile: compute, graphics and ray tracing pipelines share one file");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	SHEntry entries[4] = {
		entryOf("mainCompute", ESHPipelineStage_Compute),
		entryOf("mainVS", ESHPipelineStage_Vertex),
		entryOf("mainPS", ESHPipelineStage_Pixel),
		entryOf("mainRaygen", ESHPipelineStage_RaygenExt)
	};

	entries[2].outputs[0] = ESBType_F32x4;

	SHFile sh = fileOf(entries, 4);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	SPFile read = (SPFile) { 0 };
	StreamRef *archive = NULL;
	U32 computeId = U32_MAX, gfxId = U32_MAX, rtId = U32_MAX;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef computeStage[1] = { refOf(0, 0) };
	const SPStageRef gfxStages[2] = { refOf(0, 1), refOf(0, 2) };
	const SPStageRef rtStage[1] = { refOf(0, 3) };
	CharString none = CharString_createNull();

	Test_assert(t, "deriveCompute", SPFile_derivePipeline(
		&sp, &files, NULL, none, computeStage, 1, t->alloc, &computeId, &t->err
	));

	Test_assert(t, "deriveGraphics", SPFile_derivePipeline(
		&sp, &files, NULL, none, gfxStages, 2, t->alloc, &gfxId, &t->err
	));

	Test_assert(t, "deriveRaytracing", SPFile_derivePipeline(
		&sp, &files, NULL, none, rtStage, 1, t->alloc, &rtId, &t->err
	));

	Test_assert(t, "computeExact", SPFile_isExact(&sp, computeId));
	Test_assert(t, "computeHasNoState", !SPFile_graphicsState(&sp, computeId) && !SPFile_raytracingState(&sp, computeId));
	Test_assert(t, "graphicsHasState", SPFile_graphicsState(&sp, gfxId) != NULL);
	Test_assert(t, "raytracingHasState", SPFile_raytracingState(&sp, rtId) != NULL);
	Test_assert(t, "onlyOneOfEachState", sp.graphicsStates.length == 1 && sp.raytracingStates.length == 1);
	Test_assert(t, "graphicsNotExact", !SPFile_isExact(&sp, gfxId));
	Test_assert(t, "raytracingTwoFields", SPFile_assumedCount(&sp, rtId) == 2);

	//Mixing kinds in one pipeline can't be a pipeline at all.

	const SPStageRef mixed[2] = { refOf(0, 0), refOf(0, 1) };
	U32 mixedId = U32_MAX;

	Test_assert(t, "mixRefused", !SPFile_derivePipeline(&sp, &files, NULL, none, mixed, 2, t->alloc, &mixedId, NULL));

	//A refused derive must not leave orphaned stages or specializations behind.

	const U64 stagesAfterRefusal = sp.stages.length;
	Test_assert(t, "noOrphanStages", stagesAfterRefusal == 4);

	Test_assert(t, "finalize", SPFile_finalize(&sp, t->alloc, &t->err));

	if(!Test_assert(t, "roundTrip", spRoundTrip(t, &sp, false, &archive, &read, &type)))
		goto clean;

	Test_assert(t, "threePipelines", read.pipelines.length == 3);
	Test_assert(t, "computeKept", read.pipelines.ptr[computeId].type == (U8) ESPPipelineType_Compute);
	Test_assert(t, "graphicsKept", read.pipelines.ptr[gfxId].type == (U8) ESPPipelineType_Graphics);
	Test_assert(t, "raytracingKept", read.pipelines.ptr[rtId].type == (U8) ESPPipelineType_Raytracing);
	Test_assert(t, "sameHash", read.hash == sp.hash);

clean:
	RefPtr_dec(&archive);
	SPFile_free(&read, t->alloc);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

//Stages may come from several oiSH files, the same shape a pipeline is created from.

void Test_SPFileStagesAcrossFiles(Test *t) {

	Test_setModule(t, "SPFile: stages can come from different shader files");

	SHEntry vsEntries[1] = { entryOf("mainVS", ESHPipelineStage_Vertex) };
	SHEntry psEntries[1] = { entryOf("mainPS", ESHPipelineStage_Pixel) };
	psEntries[0].outputs[0] = ESBType_F32x4;

	SHFile shs[2] = { fileOf(vsEntries, 1), fileOf(psEntries, 1) };
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(shs, 2, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	U32 pipelineId = U32_MAX;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[2] = { refOf(0, 0), refOf(1, 0) };
	CharString none = CharString_createNull();

	if (Test_assert(t, "derive", SPFile_derivePipeline(
		&sp, &files, NULL, none, stages, 2, t->alloc, &pipelineId, &t->err
	))) {
		Test_assert(t, "twoStages", sp.pipelines.ptr[pipelineId].stageCount == 2);
		Test_assert(t, "isGraphics", sp.pipelines.ptr[pipelineId].type == (U8) ESPPipelineType_Graphics);
	}

	//A ref pointing past the files is refused rather than read.

	const SPStageRef bad[1] = { refOf(2, 0) };
	U32 badId = U32_MAX;

	Test_assert(t, "badFileRefused", !SPFile_derivePipeline(&sp, &files, NULL, none, bad, 1, t->alloc, &badId, NULL));

clean:
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&shs[0].entries, t->alloc);
	ListSHEntry_free(&shs[1].entries, t->alloc);
}

//Validation catches a pipeline that disagrees with the shader before a driver ever sees it.

void Test_SPFileValidate(Test *t) {

	Test_setModule(t, "SPFile: validation catches state that disagrees with the shader");

	SHEntry entries[2] = { entryOf("mainVS", ESHPipelineStage_Vertex), entryOf("mainPS", ESHPipelineStage_Pixel) };
	entries[1].outputs[0] = ESBType_F32x4;
	entries[1].outputs[1] = ESBType_F32x4;        //Writes two targets

	SHFile sh = fileOf(entries, 2);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	ListCharString issues = (ListCharString) { 0 };
	U32 pipelineId = U32_MAX;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[2] = { refOf(0, 0), refOf(0, 1) };
	CharString none = CharString_createNull();

	if (Test_assert(t, "derive", SPFile_derivePipeline(
		&sp, &files, NULL, none, stages, 2, t->alloc, &pipelineId, &t->err
	))) {

		Test_assert(t, "validateClean", SPFile_validate(&sp, pipelineId, &files, stages, t->alloc, &issues, &t->err));
		Test_assert(t, "noIssues", issues.length == 0);

		//Declaring fewer targets than the pixel stage writes can't be compiled.

		SPFile_graphicsStateMut(&sp, pipelineId)->renderTargetCount = 1;

		Test_assert(t, "validateTooFew", SPFile_validate(&sp, pipelineId, &files, stages, t->alloc, &issues, &t->err));
		Test_assert(t, "targetMismatchReported", issues.length == 1);

		ListCharString_freeUnderlying(&issues, t->alloc);

		//Depth state without a depth attachment is the same kind of mismatch.

		SPFile_graphicsStateMut(&sp, pipelineId)->renderTargetCount = 2;
		SPFile_graphicsStateMut(&sp, pipelineId)->depth.flags = 1;
		SPFile_graphicsStateMut(&sp, pipelineId)->depthFormat = 0;

		Test_assert(t, "validateDepth", SPFile_validate(&sp, pipelineId, &files, stages, t->alloc, &issues, &t->err));
		Test_assert(t, "depthMismatchReported", issues.length == 1);
	}

clean:
	ListCharString_freeUnderlying(&issues, t->alloc);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

//The report mirrors PipelineGraphicsInfo field for field, so supplying everything it lists is a whole pipeline.

void Test_SPFileReportCoversWholePipeline(Test *t) {

	Test_setModule(t, "SPFile: supplying every reported field makes a pipeline exact");

	SHEntry entries[4] = {
		entryOf("mainVS", ESHPipelineStage_Vertex),
		entryOf("mainHS", ESHPipelineStage_Hull),
		entryOf("mainDS", ESHPipelineStage_Domain),
		entryOf("mainPS", ESHPipelineStage_Pixel)
	};

	entries[0].inputs[0] = ESBType_F32x3;
	entries[3].outputs[0] = ESBType_F32x4;

	SHFile sh = fileOf(entries, 4);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	ListCharString issues = (ListCharString) { 0 };
	U32 pipelineId = U32_MAX;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[4] = { refOf(0, 0), refOf(0, 1), refOf(0, 2), refOf(0, 3) };
	CharString none = CharString_createNull();

	if (Test_assert(t, "derive", SPFile_derivePipeline(
		&sp, &files, NULL, none, stages, 4, t->alloc, &pipelineId, &t->err
	))) {

		//The rasterizer, stencil, sample shading and tessellation fields all have to be reported, since a caller that
		// supplies only what's listed still has to end up with a whole pipeline.

		Test_assert(t, "patchReported", findField(&sp, pipelineId, ESPField_PatchControlPoints, 0) != NULL);
		Test_assert(t, "cullReported", findField(&sp, pipelineId, ESPField_CullMode, 0) != NULL);
		Test_assert(t, "stencilReported", findField(&sp, pipelineId, ESPField_StencilCompare, 0) != NULL);
		Test_assert(t, "sampleShadingReported", findField(&sp, pipelineId, ESPField_MsaaMinSampleShading, 0) != NULL);
		Test_assert(t, "blendOpReported", findField(&sp, pipelineId, ESPField_BlendOp, 0) != NULL);
		Test_assert(t, "strideReported", findField(&sp, pipelineId, ESPField_VertexBufferStride, 0) != NULL);

		//Supply each reported field with its own assumed value; nothing may remain assumed afterwards.

		const SPPipelineBase pipeline = sp.pipelines.ptr[pipelineId];
		Bool supplied = true;

		for (U32 i = 0; i < pipeline.specializationCount; ++i) {

			const SPSpecialization spec = sp.specializations.ptr[pipeline.specializationStart + i];

			supplied &= SPFile_supply(&sp, pipelineId, (ESPField) spec.field, spec.index, spec.value, &t->err);
		}

		Test_assert(t, "allSupplied", supplied);
		Test_assert(t, "exact", SPFile_isExact(&sp, pipelineId));

		//And that exact pipeline is valid.

		Test_assert(t, "validate", SPFile_validate(&sp, pipelineId, &files, stages, t->alloc, &issues, &t->err));
		Test_assert(t, "noIssues", issues.length == 0);
	}

clean:
	ListCharString_freeUnderlying(&issues, t->alloc);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

//Embedded in a parent format the magic number is hidden, and the length-only write has to agree with the real one.

void Test_SPFileSubFileAndSize(Test *t) {

	Test_setModule(t, "SPFile: sub-file round trip and length-only write");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	SHEntry entries[1] = { entryOf("mainCompute", ESHPipelineStage_Compute) };
	SHFile sh = fileOf(entries, 1);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	SPFile read = (SPFile) { 0 };
	StreamRef *archive = NULL;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_HideMagicNumber, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[1] = { refOf(0, 0) };
	CharString none = CharString_createNull();

	Test_assert(t, "derive", SPFile_derivePipeline(&sp, &files, NULL, none, stages, 1, t->alloc, NULL, &t->err));
	Test_assert(t, "finalize", SPFile_finalize(&sp, t->alloc, &t->err));

	//Length-only write has to match what an actual write produces.

	U64 sizeOnly = 0;
	Test_assert(t, "lengthOnlyWrite", SPFile_write(&sp, t->alloc, NULL, &sizeOnly, &t->err));

	if (Test_assert(t, "subFileRoundTrip", spRoundTrip(t, &sp, true, &archive, &read, &type))) {

		Test_assert(t, "onePipeline", read.pipelines.length == 1);
		Test_assert(t, "isCompute", read.pipelines.ptr[0].type == (U8) ESPPipelineType_Compute);
		Test_assert(t, "exact", SPFile_isExact(&read, 0));

		MemoryStream *ms = RefPtr_data(archive, MemoryStream);
		Test_assert(t, "sizeMatches", sizeOnly == Buffer_length(ms->data));
	}

clean:
	RefPtr_dec(&archive);
	SPFile_free(&read, t->alloc);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

//A tampered header or an out of range record has to be refused rather than trusted.

//Only the attachments a blend state can reach are stored, so what a file costs follows from the blend state itself.

void Test_SPFileBlendAttachmentPacking(Test *t) {

	Test_setModule(t, "SPFile: a blend state only stores the attachments it can reach");

	SHEntry entries[2] = {
		entryOf("mainVS", ESHPipelineStage_Vertex),
		entryOf("mainPS", ESHPipelineStage_Pixel)
	};

	entries[1].outputs[0] = ESBType_F32x4;

	SHFile sh = fileOf(entries, 2);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	SPFile read = (SPFile) { 0 };
	StreamRef *archive = NULL;
	U32 pipelineId = U32_MAX;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[2] = { refOf(0, 0), refOf(0, 1) };
	CharString none = CharString_createNull();
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	if(!Test_assert(t, "derive", SPFile_derivePipeline(
		&sp, &files, NULL, none, stages, 2, t->alloc, &pipelineId, &t->err
	)))
		goto clean;

	//Blending off means no attachment is reachable, whatever was written into them.

	Test_assert(t, "supplySrc", SPFile_supply(&sp, pipelineId, ESPField_BlendSrc, 0, EBlend_One, &t->err));

	SPGraphicsState *gfx = SPFile_graphicsStateMut(&sp, pipelineId);

	if(!Test_assert(t, "hasState", gfx != NULL))
		goto clean;

	Test_assert(t, "disabledStoresNone", !SPBlendStateRuntime_storedAttachmentCount(gfx->blend));

	Test_assert(t, "finalize", SPFile_finalize(&sp, t->alloc, &t->err));
	Test_assert(t, "disabledCleared", !gfx->blend.attachments[0].srcBlend);

	U64 sizeNone = 0;
	Test_assert(t, "lengthOnlyWriteNone", SPFile_write(&sp, t->alloc, NULL, &sizeNone, &t->err));

	//With independent blending off a single attachment covers every target.

	Test_assert(t, "supplyEnable", SPFile_supply(&sp, pipelineId, ESPField_BlendEnable, 0, 1, &t->err));
	Test_assert(t, "supplyMask", SPFile_supply(&sp, pipelineId, ESPField_BlendTargetMask, 0, 0xB, &t->err));
	Test_assert(t, "sharedStoresOne", SPBlendStateRuntime_storedAttachmentCount(gfx->blend) == 1);

	//With it on only the masked targets are, so 0xB stores three rather than eight.

	Test_assert(t, "supplyIndependent", SPFile_supply(&sp, pipelineId, ESPField_BlendIndependent, 0, 1, &t->err));
	Test_assert(t, "maskedStoresThree", SPBlendStateRuntime_storedAttachmentCount(gfx->blend) == 3);

	for(U8 i = 0; i < 8; ++i)
		Test_assert(t, "supplyPerTarget", SPFile_supply(&sp, pipelineId, ESPField_BlendSrc, i, (U32) (i + 1), &t->err));

	Test_assert(t, "finalizePacked", SPFile_finalize(&sp, t->alloc, &t->err));

	//An unreachable target keeps nothing, so the file can't be carrying it.

	Test_assert(t, "unmaskedCleared", !gfx->blend.attachments[2].srcBlend);

	U64 sizePacked = 0;
	Test_assert(t, "lengthOnlyWrite", SPFile_write(&sp, t->alloc, NULL, &sizePacked, &t->err));

	if (Test_assert(t, "roundTrip", spRoundTrip(t, &sp, false, &archive, &read, &type))) {

		const SPGraphicsState *readGfx = SPFile_graphicsState(&read, 0);

		if (Test_assert(t, "readHasState", readGfx != NULL)) {

			//Each stored attachment has to come back on the target it was written for, not the slot it was stored in.

			Test_assert(t, "target0", readGfx->blend.attachments[0].srcBlend == 1);
			Test_assert(t, "target1", readGfx->blend.attachments[1].srcBlend == 2);
			Test_assert(t, "target3", readGfx->blend.attachments[3].srcBlend == 4);
			Test_assert(t, "target2Empty", !readGfx->blend.attachments[2].srcBlend);
		}

		Test_assert(t, "sameHash", read.hash == sp.hash);

		MemoryStream *ms = RefPtr_data(archive, MemoryStream);
		Test_assert(t, "sizeMatches", sizePacked == Buffer_length(ms->data));

	}

	//Reaching all eight targets costs exactly eight attachments more than reaching none, and 8 of them are a
	// whole number of 16-byte units, so the file's own alignment can't absorb any of it.

	Test_assert(t, "supplyFullMask", SPFile_supply(&sp, pipelineId, ESPField_BlendTargetMask, 0, 0xFF, &t->err));
	Test_assert(t, "finalizeFull", SPFile_finalize(&sp, t->alloc, &t->err));
	Test_assert(t, "fullStoresEight", SPBlendStateRuntime_storedAttachmentCount(gfx->blend) == 8);

	U64 sizeFull = 0;
	Test_assert(t, "lengthOnlyWriteFull", SPFile_write(&sp, t->alloc, NULL, &sizeFull, &t->err));
	Test_assert(t, "packingSavesTheRest", sizeFull == sizeNone + 8 * sizeof(SPBlendAttachment));
	Test_assert(t, "maskedIsSmallerStill", sizePacked < sizeFull);

clean:
	RefPtr_dec(&archive);
	SPFile_free(&read, t->alloc);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

//A vertex layout is sparse by input location, so what a file costs follows from which entries are real.

void Test_SPFileVertexLayoutPacking(Test *t) {

	Test_setModule(t, "SPFile: a vertex layout only stores the entries it fills in");

	SHEntry entries[2] = {
		entryOf("mainVS", ESHPipelineStage_Vertex),
		entryOf("mainPS", ESHPipelineStage_Pixel)
	};

	//Location 1 stays empty, so the layout can't be stored as a prefix.

	entries[0].inputs[0] = ESBType_F32x3;
	entries[0].inputs[2] = ESBType_F32x4;
	entries[1].outputs[0] = ESBType_F32x4;

	SHFile sh = fileOf(entries, 2);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	SPFile read = (SPFile) { 0 };
	StreamRef *archive = NULL;
	U32 pipelineId = U32_MAX;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[2] = { refOf(0, 0), refOf(0, 1) };
	CharString none = CharString_createNull();
	const RefPtrType type = MemoryStream_makeType(t->alloc);

	if(!Test_assert(t, "derive", SPFile_derivePipeline(
		&sp, &files, NULL, none, stages, 2, t->alloc, &pipelineId, &t->err
	)))
		goto clean;

	SPGraphicsState *gfx = SPFile_graphicsStateMut(&sp, pipelineId);

	if(!Test_assert(t, "hasState", gfx != NULL))
		goto clean;

	Test_assert(t, "finalize", SPFile_finalize(&sp, t->alloc, &t->err));

	//Only the two locations that carry a format, and only the one buffer that carries a stride.

	Test_assert(
		t, "attributeMask", SPVertexLayoutRuntime_attributeMask(gfx->inputAssembler.vertexLayout) == 0x5
	);

	Test_assert(t, "bufferMask", SPVertexLayoutRuntime_bufferMask(gfx->inputAssembler.vertexLayout) == 0x1);

	if (Test_assert(t, "roundTrip", spRoundTrip(t, &sp, false, &archive, &read, &type))) {

		const SPGraphicsState *readGfx = SPFile_graphicsState(&read, 0);

		if (Test_assert(t, "readHasState", readGfx != NULL)) {

			const SPVertexLayoutRuntime layout = readGfx->inputAssembler.vertexLayout;

			//Each stored attribute has to come back on its own location, not on the slot it was stored in.

			Test_assert(t, "location0", layout.attributes[0].format == (U8) ETextureFormatId_RGB32f);
			Test_assert(t, "location1Empty", !layout.attributes[1].format);
			Test_assert(t, "location2", layout.attributes[2].format == (U8) ETextureFormatId_RGBA32f);
			Test_assert(
				t, "buffer0",
				layout.bufferStrides12_isInstance1[0] ==
					gfx->inputAssembler.vertexLayout.bufferStrides12_isInstance1[0]
			);
		}

		Test_assert(t, "sameHash", read.hash == sp.hash);
	}

	//Eight more buffers is a whole number of 16-byte units, so the file's alignment can't absorb the difference.

	for(U8 i = 0; i < 8; ++i)
		Test_assert(t, "supplyStride", SPFile_supply(&sp, pipelineId, ESPField_VertexBufferStride, i, 16, &t->err));

	Test_assert(t, "finalizeEight", SPFile_finalize(&sp, t->alloc, &t->err));

	U64 sizeEight = 0;
	Test_assert(t, "lengthOnlyWriteEight", SPFile_write(&sp, t->alloc, NULL, &sizeEight, &t->err));

	for(U8 i = 8; i < 16; ++i)
		Test_assert(t, "supplyStrideRest", SPFile_supply(&sp, pipelineId, ESPField_VertexBufferStride, i, 16, &t->err));

	Test_assert(t, "finalizeSixteen", SPFile_finalize(&sp, t->alloc, &t->err));
	Test_assert(t, "sixteenBuffers", SPVertexLayoutRuntime_bufferMask(gfx->inputAssembler.vertexLayout) == 0xFFFF);

	U64 sizeSixteen = 0;
	Test_assert(t, "lengthOnlyWriteSixteen", SPFile_write(&sp, t->alloc, NULL, &sizeSixteen, &t->err));
	Test_assert(t, "packingSavesTheRest", sizeSixteen == sizeEight + 8 * sizeof(U16));

clean:
	RefPtr_dec(&archive);
	SPFile_free(&read, t->alloc);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

void Test_SPReadTamper(Test *t) {

	Test_setModule(t, "SPFile: a tampered file is refused");

	const RefPtrType type = MemoryStream_makeType(t->alloc);

	SHEntry entries[1] = { entryOf("mainCompute", ESHPipelineStage_Compute) };
	SHFile sh = fileOf(entries, 1);
	ListSHFile files = (ListSHFile) { 0 };
	ListSHFile_createRefConst(&sh, 1, &files, NULL);

	SPFile sp = (SPFile) { 0 };
	StreamRef *stream = NULL;

	if(!Test_assert(t, "create", SPFile_create(ESPSettingsFlags_None, t->alloc, &sp, &t->err)))
		goto clean;

	const SPStageRef stages[1] = { refOf(0, 0) };
	CharString none = CharString_createNull();

	Test_assert(t, "derive", SPFile_derivePipeline(&sp, &files, NULL, none, stages, 1, t->alloc, NULL, &t->err));

	if(!Test_assert(t, "stream", MemoryStream_create(0, EMemoryStreamFlags_WriteResize, &type, &stream, &t->err)))
		goto clean;

	U64 off = 0;

	if(!Test_assert(t, "write", SPFile_write(&sp, t->alloc, stream, &off, &t->err)))
		goto clean;

	MemoryStream *ms = RefPtr_data(stream, MemoryStream);

	if(!Test_assert(t, "wroteBytes", Buffer_length(ms->data) > 8))
		goto clean;

	//A wrong magic number can't be read as an oiSP.

	SPFile bad = (SPFile) { 0 };
	U64 readOff = 0;
	ms->data.ptrNonConst[0] ^= 0xFF;

	Test_assert(t, "badMagicRefused", !SPFile_read(stream, &readOff, false, t->alloc, &bad, NULL));
	SPFile_free(&bad, t->alloc);
	ms->data.ptrNonConst[0] ^= 0xFF;

	//An unsupported version can't either (byte 4 is the version, right after the magic).

	readOff = 0;
	ms->data.ptrNonConst[4] |= 0xFF;

	Test_assert(t, "badVersionRefused", !SPFile_read(stream, &readOff, false, t->alloc, &bad, NULL));
	SPFile_free(&bad, t->alloc);

clean:
	RefPtr_dec(&stream);
	SPFile_free(&sp, t->alloc);
	ListSHFile_free(&files, t->alloc);
	ListSHEntry_free(&sh.entries, t->alloc);
}

//Field names are the vocabulary the report, a template and any sweep all share, so they have to stay stable.

void Test_SPFieldNames(Test *t) {

	Test_setModule(t, "SPFile: field names are stable and indexed fields are marked");

	Bool named = true, explained = true;

	for (U64 i = 0; i < ESPField_Count; ++i) {

		const CharString name = CharString_createRefCStrConst(ESPField_name((ESPField) i));
		const CharString reason = CharString_createRefCStrConst(ESPField_reason((ESPField) i));
		const CharString domain = CharString_createRefCStrConst(ESPField_domain((ESPField) i));

		named &= CharString_length(name) > 0;
		explained &= CharString_length(reason) > 0 && CharString_length(domain) > 0;
	}

	Test_assert(t, "everyFieldNamed", named);
	Test_assert(t, "everyFieldExplained", explained);

	Test_assert(t, "rtvIsIndexed", ESPField_isIndexed(ESPField_RenderTargetFormat));
	Test_assert(t, "strideIsIndexed", ESPField_isIndexed(ESPField_VertexBufferStride));
	Test_assert(t, "msaaIsNotIndexed", !ESPField_isIndexed(ESPField_Msaa));
}

int main() {

	const Allocator alloc = BasicAllocator_instance;

	Test t = (Test) { 0 };
	t.alloc = &alloc;

	Test_SPFileRoundTrip(&t);
	Test_SPFileManyPipelines(&t);
	Test_SPFileStagesAcrossFiles(&t);
	Test_SPFileValidate(&t);
	Test_SPFileReportCoversWholePipeline(&t);
	Test_SPFileSubFileAndSize(&t);
	Test_SPFileBlendAttachmentPacking(&t);
	Test_SPFileVertexLayoutPacking(&t);
	Test_SPReadTamper(&t);
	Test_SPFieldNames(&t);

	BasicAllocator_checkLeakedMem(&t);
	return Test_end(&t);
}
