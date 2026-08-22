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

	gotoIfError3(clean, ListSPPipelineBase_resize(&pipelines, header.pipelineCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSPPipelineBase_buffer(pipelines), alloc, e_rr));

	gotoIfError3(clean, ListSPStage_resize(&stages, header.stageCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSPStage_buffer(stages), alloc, e_rr));

	gotoIfError3(clean, ListSPSpecialization_resize(&specializations, header.specializationCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(
		&cursor, offset, ListSPSpecialization_buffer(specializations), alloc, e_rr
	));

	//A graphics state is stored without its blend attachments, so it's consumed as the two halves either side of them.

	gotoIfError3(clean, ListSPGraphicsState_resize(&graphicsStates, header.graphicsStateCount, alloc, e_rr));

	//The masks are kept, since expanding drops them and the sections below are scattered by them.

	gotoIfError3(clean, ListSPVertexLayoutStored_resize(&vertexMasks, graphicsStates.length, alloc, e_rr));

	for (U64 i = 0; i < graphicsStates.length; ++i) {

		SPGraphicsStateStored stored = (SPGraphicsStateStored) { 0 };

		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &stored, sizeof(stored), alloc, e_rr));

		graphicsStates.ptrNonConst[i] = SPGraphicsStateStored_expand(stored);
		vertexMasks.ptrNonConst[i] = stored.inputAssembler.vertexLayout;
	}

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

			if(vertexMasks.ptr[i].bufferMask & ((U16)1 << j))
				++derivedBuffers;

			if(vertexMasks.ptr[i].attributeMask & ((U16)1 << j))
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
		}
	}

	for (U64 i = 0; i < graphicsStates.length; ++i) {

		SPVertexLayoutRuntime *layout = &graphicsStates.ptrNonConst[i].inputAssembler.vertexLayout;

		for(U8 j = 0; j < 16; ++j) {

			if(!(vertexMasks.ptr[i].bufferMask & ((U16)1 << j)))
				continue;

			gotoIfError3(clean, StreamCursor_consumeU16(
				&cursor, offset, &layout->bufferStrides12_isInstance1[j], alloc, e_rr
			));
		}
	}

	for (U64 i = 0; i < graphicsStates.length; ++i) {

		SPVertexLayoutRuntime *layout = &graphicsStates.ptrNonConst[i].inputAssembler.vertexLayout;

		for(U8 j = 0; j < 16; ++j) {

			if(!(vertexMasks.ptr[i].attributeMask & ((U16)1 << j)))
				continue;

			gotoIfError3(clean, StreamCursor_consume(
				&cursor, offset, &layout->attributes[j], sizeof(SPVertexAttribute), alloc, e_rr
			));
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

	if(
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

		if(stage.stage >= ESHPipelineStage_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() stage had an invalid pipeline stage"));

		if(stage.shaderFile != U32_MAX && stage.shaderFile >= nameCount)
			retError(clean, Error_invalidState(0, "SPFile_read() stage.shaderFile out of bounds"));

		if(stage.entrypoint != U32_MAX && stage.entrypoint >= nameCount)
			retError(clean, Error_invalidState(0, "SPFile_read() stage.entrypoint out of bounds"));
	}

	//Validate the specialization records

	for (U64 i = 0; i < specializations.length; ++i) {

		const SPSpecialization spec = specializations.ptr[i];

		if(spec.field >= ESPField_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() specialization had an invalid field"));

		if(spec.source >= ESPFieldSource_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() specialization had an invalid source"));

		//An index past the field's range (or any index on a field that has none) addresses state that doesn't exist

		if(spec.index >= ESPField_indexCount((ESPField) spec.field))
			retError(clean, Error_invalidState(0, "SPFile_read() specialization index is out of range for its field"));
	}

	//Validate the pipelines and that their ranges sit inside the pools

	for (U64 i = 0; i < pipelines.length; ++i) {

		const SPPipelineBase pipeline = pipelines.ptr[i];

		if(pipeline.type >= ESPPipelineType_Count)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline had an invalid type"));

		if(pipeline.flags & ESPPipelineFlag_Unsupported)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline had unsupported flags"));

		if(pipeline.name != U32_MAX && pipeline.name >= nameCount)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline.name out of bounds"));

		if((U64)pipeline.stageStart + pipeline.stageCount > stages.length)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline stage range out of bounds"));

		if((U64)pipeline.specializationStart + pipeline.specializationCount > specializations.length)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline specialization range out of bounds"));

		//The kind specific state a pipeline points at has to exist, since every lowering dereferences it.

		if(pipeline.type == (U8) ESPPipelineType_Graphics && pipeline.stateIndex >= graphicsStates.length)
			retError(clean, Error_invalidState(0, "SPFile_read() graphics pipeline stateIndex out of bounds"));

		if(pipeline.type == (U8) ESPPipelineType_Raytracing && pipeline.stateIndex >= raytracingStates.length)
			retError(clean, Error_invalidState(0, "SPFile_read() ray tracing pipeline stateIndex out of bounds"));

		if(
			pipeline.type == (U8) ESPPipelineType_Graphics &&
			graphicsStates.ptr[pipeline.stateIndex].renderTargetCount > 8
		)
			retError(clean, Error_invalidState(0, "SPFile_read() graphics state renderTargetCount out of bounds"));

		//A pipeline with no stage couldn't be created from

		if(!pipeline.stageCount)
			retError(clean, Error_invalidState(0, "SPFile_read() pipeline has no stages"));
	}

	*spFile = (SPFile) {
		.names = names,
		.pipelines = pipelines,
		.stages = stages,
		.specializations = specializations,
		.graphicsStates = graphicsStates,
		.raytracingStates = raytracingStates,
		.flags = isSubFile ? ESPSettingsFlags_HideMagicNumber : ESPSettingsFlags_None
	};

	names = (DLFile) { 0 };
	pipelines = (ListSPPipelineBase) { 0 };
	stages = (ListSPStage) { 0 };
	specializations = (ListSPSpecialization) { 0 };
	graphicsStates = (ListSPGraphicsState) { 0 };
	raytracingStates = (ListSPRaytracingState) { 0 };

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
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
