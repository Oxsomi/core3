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

//formats/oiSP/sp_read.c

#include "formats/oiSP/sp_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/container/stream.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include <stddef.h>

//Every count in the header sizes an allocation, so each is held to the bytes the stream still has, at the least a
// record can take, before anything is allocated: a 44 byte file must not be able to ask for gigabytes.

static Bool SPFile_fits(
	const OxStream *stream, U64 offset, U64 count, U64 recordBytes, const C8 *what, Error *e_rr
) {

	Bool s_uccess = true;

	if(!stream || offset > stream->size || (stream->size - offset) / recordBytes < count)
		retError(clean, Error_outOfBounds(
			0, count, stream && offset <= stream->size ? (stream->size - offset) / recordBytes : 0, what
		));

clean:
	return s_uccess;
}

//A stored graphics state is checked as the raw bytes it is: its enums have to name something, its flags only set
// bits that exist and its padding be zero, since a value that reads back differently than it was written is how
// a file smuggles state past the reader.

static Bool SPFile_validateStored(const SPGraphicsStateStored *s, Error *e_rr) {

	Bool s_uccess = true;

	if(s->padding || s->inputAssembler.padding[0] || s->inputAssembler.padding[1] || s->inputAssembler.padding[2])
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state padding has to be zero"));

	if(s->msaa >= EMSAASamples_Count)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state msaa is unknown"));

	if(s->depthFormat >= EDepthStencilFormat_Count)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state depth format is unknown"));

	for(U8 i = 0; i < 8; ++i)
		if(s->renderTargetFormats[i] >= ETextureFormatId_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() graphics state render target format is unknown"));

	if(s->inputAssembler.topologyMode >= EToplogyMode_Count)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state topology is unknown"));

	if(s->inputAssembler.patchControlPoints > 32)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state patch control points exceed 32"));

	const U8 *blendRaw = (const U8*) &s->blend;

	if (
		blendRaw[offsetof(SPBlendStateStored, enable)] > 1 ||
		blendRaw[offsetof(SPBlendStateStored, allowIndependentBlend)] > 1
	)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state blend flags have to be 0 or 1"));

	if(s->blend.logicOpExt >= ELogicOpExt_Count)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state logic op is unknown"));

	for(U8 i = 0; i < 8; ++i)
		if(s->blend.writeMask[i] & ~(U8) EWriteMask_All)
			retError(clean, Error_invalidState(0, "SPFile_read() graphics state write mask carries unknown bits"));

	if(s->depth.flags & ~(U8) (EDepthStencilFlags_DepthWrite | EDepthStencilFlags_StencilTest))
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state depth flags carry unknown bits"));

	if(s->depth.depthCompare >= ECompareOp_Count || s->depth.stencilCompare >= ECompareOp_Count)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state compare op is unknown"));

	if (
		s->depth.stencilFail >= EStencilOp_Count || s->depth.stencilPass >= EStencilOp_Count ||
		s->depth.stencilDepthFail >= EStencilOp_Count
	)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state stencil op is unknown"));

	if(s->rasterizer.cullMode >= ECullMode_Count)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state cull mode is unknown"));

	if (
		s->rasterizer.flags & ~(U16) (
			ERasterizerFlags_IsClockWise | ERasterizerFlags_IsWireframeExt |
			ERasterizerFlags_EnableDepthClamp | ERasterizerFlags_EnableDepthBias
		)
	)
		retError(clean, Error_invalidState(0, "SPFile_read() graphics state rasterizer flags carry unknown bits"));

clean:
	return s_uccess;
}

Bool SPFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SPFile *spFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool didAllocate = false;
	StreamCursor cursor = (StreamCursor) { 0 };

	//Read into locals first so a validation failure can't leave a half-built spFile

	ListSPPipelineBase pipelines = (ListSPPipelineBase) { 0 };
	ListSPStage stages = (ListSPStage) { 0 };
	ListSPSpecialization specializations = (ListSPSpecialization) { 0 };
	ListSPGraphicsState graphicsStates = (ListSPGraphicsState) { 0 };
	ListSPRaytracingState raytracingStates = (ListSPRaytracingState) { 0 };
	DLFile names = (DLFile) { 0 };
	ListSPVertexLayoutStored vertexMasks = (ListSPVertexLayoutStored) { 0 };
	ListPLFile layouts = (ListPLFile) { 0 };

	if(!offset || !spFile)
		retError(clean, Error_nullPointer(!offset ? 1 : 4, "SPFile_read()::spFile and offset are required"));

	if(spFile->pipelines.length || spFile->names.entryStrings.length)
		retError(clean, Error_invalidParameter(4, 0, "SPFile_read()::spFile is already present, possible memleak"));

	if(*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SPFile_read() at misaligned offset is unsupported (16-byte)"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursor, e_rr));

	if (!isSubFile) {

		U32 magic = 0;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, offset, &magic, alloc, e_rr));

		if(magic != SPHeader_MAGIC)
			retError(clean, Error_invalidState(0, "SPFile_read()::file didn't start with oiSP"));
	}

	SPHeader header;
	gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	if(header.version != ESPVersion_V1_1 || (header.flags & ESPFlag_Unsupported))
		retError(clean, Error_invalidState(0, "SPFile_read()::file didn't have the right version or flags"));

	//Consume the fixed-size POD arrays

	const OxStream *stream = RefPtr_data(streamRef, OxStream);

	gotoIfError3(clean, SPFile_fits(
		stream, *offset, header.pipelineCount, sizeof(SPPipelineBase), "SPFile_read() pipelineCount exceeds the stream", e_rr
	));

	gotoIfError3(clean, ListSPPipelineBase_resize(&pipelines, header.pipelineCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSPPipelineBase_buffer(pipelines), alloc, e_rr));

	gotoIfError3(clean, SPFile_fits(
		stream, *offset, header.stageCount, sizeof(SPStage), "SPFile_read() stageCount exceeds the stream", e_rr
	));

	gotoIfError3(clean, ListSPStage_resize(&stages, header.stageCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSPStage_buffer(stages), alloc, e_rr));

	gotoIfError3(clean, SPFile_fits(
		stream, *offset, header.specializationCount, sizeof(SPSpecialization),
		"SPFile_read() specializationCount exceeds the stream", e_rr
	));

	gotoIfError3(clean, ListSPSpecialization_resize(&specializations, header.specializationCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(
		&cursor, offset, ListSPSpecialization_buffer(specializations), alloc, e_rr
	));

	//A graphics state is stored without its blend attachments, so it's consumed as the two halves either side of them.

	gotoIfError3(clean, SPFile_fits(
		stream, *offset, header.graphicsStateCount, sizeof(SPGraphicsStateStored),
		"SPFile_read() graphicsStateCount exceeds the stream", e_rr
	));

	gotoIfError3(clean, ListSPGraphicsState_resize(&graphicsStates, header.graphicsStateCount, alloc, e_rr));

	//The masks are kept, since expanding drops them and the sections below are scattered by them.

	gotoIfError3(clean, ListSPVertexLayoutStored_resize(&vertexMasks, graphicsStates.length, alloc, e_rr));

	for (U64 i = 0; i < graphicsStates.length; ++i) {

		SPGraphicsStateStored stored = (SPGraphicsStateStored) { 0 };

		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &stored, sizeof(stored), alloc, e_rr));

		gotoIfError3(clean, SPFile_validateStored(&stored, e_rr));

		graphicsStates.ptrNonConst[i] = SPGraphicsStateStored_expand(stored);
		vertexMasks.ptrNonConst[i] = stored.inputAssembler.vertexLayout;
	}

	gotoIfError3(clean, SPFile_fits(
		stream, *offset, header.raytracingStateCount, sizeof(SPRaytracingState),
		"SPFile_read() raytracingStateCount exceeds the stream", e_rr
	));

	gotoIfError3(clean, ListSPRaytracingState_resize(&raytracingStates, header.raytracingStateCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(
		&cursor, offset, ListSPRaytracingState_buffer(raytracingStates), alloc, e_rr
	));

	//How many attachments each state stored follows from the state itself, so the header's count has to agree with it
	// before anything is read.

	U64 derivedAttachments = 0;

	for(U64 i = 0; i < graphicsStates.length; ++i)
		derivedAttachments += SPBlendStateRuntime_storedAttachmentCount(graphicsStates.ptr[i].blend);

	if(derivedAttachments != header.blendAttachmentCount)
		retError(clean, Error_invalidState(0, "SPFile_read() blendAttachmentCount doesn't match the blend states"));

	U64 derivedBuffers = 0, derivedAttributes = 0;

	for (U64 i = 0; i < graphicsStates.length; ++i)
		for (U8 j = 0; j < 16; ++j) {

			if(vertexMasks.ptr[i].bufferMask & ((U16) 1 << j))
				++derivedBuffers;

			if(vertexMasks.ptr[i].attributeMask & ((U16) 1 << j))
				++derivedAttributes;
		}

	if(derivedBuffers != header.vertexBufferCount || derivedAttributes != header.vertexAttributeCount)
		retError(clean, Error_invalidState(0, "SPFile_read() vertex counts don't match the stored masks"));

	for (U64 i = 0; i < graphicsStates.length; ++i) {

		SPGraphicsState *gfx = &graphicsStates.ptrNonConst[i];

		for(U8 j = 0; j < 8; ++j) {

			if(!SPBlendStateRuntime_storesAttachment(gfx->blend, j))
				continue;

			gotoIfError3(clean, StreamCursor_consume(
				&cursor, offset, &gfx->blend.attachments[j], sizeof(SPBlendAttachment), alloc, e_rr
			));

			const SPBlendAttachment att = gfx->blend.attachments[j];

			if (
				att.srcBlend >= EBlend_Count || att.dstBlend >= EBlend_Count ||
				att.srcBlendAlpha >= EBlend_Count || att.dstBlendAlpha >= EBlend_Count
			)
				retError(clean, Error_invalidState(0, "SPFile_read() blend factor is unknown"));

			if(att.blendOp >= EBlendOp_Count || att.blendOpAlpha >= EBlendOp_Count)
				retError(clean, Error_invalidState(0, "SPFile_read() blend op is unknown"));
		}
	}

	for (U64 i = 0; i < graphicsStates.length; ++i) {

		SPVertexLayoutRuntime *layout = &graphicsStates.ptrNonConst[i].inputAssembler.vertexLayout;

		for(U8 j = 0; j < 16; ++j) {

			if(!(vertexMasks.ptr[i].bufferMask & ((U16) 1 << j)))
				continue;

			gotoIfError3(clean, StreamCursor_consumeU16(
				&cursor, offset, &layout->bufferStrides12_isInstance1[j], alloc, e_rr
			));

			if(layout->bufferStrides12_isInstance1[j] >> 13)
				retError(clean, Error_invalidState(0, "SPFile_read() vertex buffer stride carries unknown bits"));
		}
	}

	for (U64 i = 0; i < graphicsStates.length; ++i) {

		SPVertexLayoutRuntime *layout = &graphicsStates.ptrNonConst[i].inputAssembler.vertexLayout;

		for(U8 j = 0; j < 16; ++j) {

			if(!(vertexMasks.ptr[i].attributeMask & ((U16) 1 << j)))
				continue;

			gotoIfError3(clean, StreamCursor_consume(
				&cursor, offset, &layout->attributes[j], sizeof(SPVertexAttribute), alloc, e_rr
			));

			const SPVertexAttribute attribute = layout->attributes[j];

			if(attribute.offset11 >> 11 || attribute.bufferId4 >> 4 || attribute.format >= ETextureFormatId_Count)
				retError(clean, Error_invalidState(0, "SPFile_read() vertex attribute is out of range"));
		}
	}

	//The names oiDL starts 16-byte aligned

	const U64 aligned = (*offset + 15) & ~15;

	if (aligned != *offset) {
		U8 padDiscard[16];
		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, padDiscard, aligned - *offset, alloc, e_rr));
	}

	gotoIfError3(clean, DLFile_read(streamRef, offset, NULL, I32x4_zero(), true, false, alloc, NULL, &names, e_rr));
	didAllocate = true;

	gotoIfError3(clean, SPFile_fits(
		stream, *offset, header.layoutCount, sizeof(PLHeader), "SPFile_read() layoutCount exceeds the stream", e_rr
	));

	gotoIfError3(clean, ListPLFile_resize(&layouts, header.layoutCount, alloc, e_rr));

	for(U64 i = 0; i < header.layoutCount; ++i)
		gotoIfError3(clean, PLFile_read(streamRef, offset, true, alloc, &layouts.ptrNonConst[i], e_rr));

	if (
		names.settings.dataType != EDLDataType_String ||
		names.settings.encryptionType ||
		names.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SPFile_read() names didn't match expectations"));

	const U64 nameCount = names.entryStrings.length;

	for (U64 i = 0; i < nameCount; ++i) {

		if(!DLFile_isFullyLoaded(&names, i))
			retError(clean, Error_invalidParameter(0, 1, "SPFile_read() one of the strings wasn't fully loaded"));

		//Cap string length, matching the producer's limit (and the oiSB/oiSH/oiSR house style)

		if(CharString_length(names.entryStrings.ptr[i]) >= 32768)
			retError(clean, Error_invalidParameter(0, 1, "SPFile_read() one of the strings exceeded the length limit"));
	}

	//Validate the stage records

	for (U64 i = 0; i < stages.length; ++i) {

		const SPStage stage = stages.ptr[i];

		if(stage.padding[0] || stage.padding[1] || stage.padding[2])
			retError(clean, Error_invalidState(0, "SPFile_read() stage padding has to be zero"));

		if(stage.stage >= EGfxPipelineStage_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() stage had an invalid pipeline stage"));

		if(stage.shaderFile != U32_MAX && stage.shaderFile >= nameCount)
			retError(clean, Error_invalidState(0, "SPFile_read() stage.shaderFile out of bounds"));

		if(stage.entrypoint != U32_MAX && stage.entrypoint >= nameCount)
			retError(clean, Error_invalidState(0, "SPFile_read() stage.entrypoint out of bounds"));
	}

	//Validate the specialization records

	for (U64 i = 0; i < specializations.length; ++i) {

		const SPSpecialization spec = specializations.ptr[i];

		if(spec.padding)
			retError(clean, Error_invalidState(0, "SPFile_read() specialization padding has to be zero"));

		if(spec.field >= ESPField_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() specialization had an invalid field"));

		if(spec.source >= ESPFieldSource_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() specialization had an invalid source"));

		//An index past the field's range (or any index on a field that has none) addresses state that doesn't exist

		if(spec.index >= ESPField_indexCount((ESPField) spec.field))
			retError(clean, Error_invalidState(0, "SPFile_read() specialization index is out of range for its field"));
	}

	for (U64 i = 0; i < raytracingStates.length; ++i)
		if(raytracingStates.ptr[i].padding)
			retError(clean, Error_invalidState(0, "SPFile_read() ray tracing state padding has to be zero"));

	//Validate the pipelines and that their ranges sit inside the pools

	for (U64 i = 0; i < pipelines.length; ++i) {

		const SPPipelineBase pipeline = pipelines.ptr[i];

		if(pipeline.type >= ESPPipelineType_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline had an invalid type"));

		if(pipeline.flags & ESPPipelineFlag_Unsupported)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline had unsupported flags"));

		if(pipeline.name != U32_MAX && pipeline.name >= nameCount)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline.name out of bounds"));

		if((U64) pipeline.stageStart + pipeline.stageCount > stages.length)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline stage range out of bounds"));

		if((U64) pipeline.specializationStart + pipeline.specializationCount > specializations.length)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline specialization range out of bounds"));

		//The kind specific state a pipeline points at has to exist, since every lowering dereferences it.

		if(pipeline.type == (U8) ESPPipelineType_Graphics && pipeline.stateIndex >= graphicsStates.length)
			retError(clean, Error_invalidState(0, "SPFile_read() graphics pipeline stateIndex out of bounds"));

		if(pipeline.type == (U8) ESPPipelineType_Raytracing && pipeline.stateIndex >= raytracingStates.length)
			retError(clean, Error_invalidState(0, "SPFile_read() ray tracing pipeline stateIndex out of bounds"));

		if (
			pipeline.type == (U8) ESPPipelineType_Graphics &&
			graphicsStates.ptr[pipeline.stateIndex].renderTargetCount > 8
		)
			retError(clean, Error_invalidState(0, "SPFile_read() graphics state renderTargetCount out of bounds"));

		//A pipeline with no stage couldn't be created from

		if(!pipeline.stageCount)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline has no stages"));
	}

	for (U64 i = 0; i < pipelines.length; ++i)
		if(pipelines.ptr[i].layoutIndex != U32_MAX && pipelines.ptr[i].layoutIndex >= layouts.length)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline layoutIndex out of bounds"));

	*spFile = (SPFile) {
		.names = names,
		.pipelines = pipelines,
		.stages = stages,
		.specializations = specializations,
		.graphicsStates = graphicsStates,
		.raytracingStates = raytracingStates,
		.layouts = layouts,
		.flags = isSubFile ? ESPSettingsFlags_HideMagicNumber : ESPSettingsFlags_None
	};

	names = (DLFile) { 0 };
	pipelines = (ListSPPipelineBase) { 0 };
	stages = (ListSPStage) { 0 };
	specializations = (ListSPSpecialization) { 0 };
	graphicsStates = (ListSPGraphicsState) { 0 };
	raytracingStates = (ListSPRaytracingState) { 0 };
	layouts = (ListPLFile) { 0 };

	gotoIfError3(clean, SPFile_finalize(spFile, alloc, e_rr));

clean:

	if(didAllocate)
		DLFile_free(&names, alloc);

	ListSPPipelineBase_free(&pipelines, alloc);
	ListSPStage_free(&stages, alloc);
	ListSPSpecialization_free(&specializations, alloc);
	ListSPGraphicsState_free(&graphicsStates, alloc);
	ListSPRaytracingState_free(&raytracingStates, alloc);
	ListSPVertexLayoutStored_free(&vertexMasks, alloc);
	ListPLFile_freeUnderlying(&layouts, alloc);
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
