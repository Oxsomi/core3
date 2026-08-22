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

//formats/oiSP/sp_write.c

#include "formats/oiSP/sp_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/container/stream.h"
#include "types/base/buffer_base.h"
#include "types/base/error.h"
#include "types/base/allocator.h"

Bool SPFile_write(const SPFile *spFile, const Allocator *alloc, StreamRef *streamRef, U64 *offset, Error *e_rr) {

	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };

	if(!spFile)
		retError(clean, Error_nullPointer(0, "SPFile_write()::spFile is undefined"));

	if(!offset)
		retError(clean, Error_nullPointer(!streamRef ? 2 : 3, "SPFile_write()::offset is required"));

	if(streamRef && streamRef->refPtrType->typeId != (TypeId)EContainerTypeId_Stream)
		retError(clean, Error_invalidOperation(0, "SPFile_write()::streamRef invalid type"));

	if(*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SPFile_write() at misaligned offset is unsupported (16-byte)"));

	if(spFile->pipelines.length >> 32)
		retError(clean, Error_invalidOperation(0, "SPFile_write()::pipelines out of bounds"));

	if(spFile->stages.length >> 32)
		retError(clean, Error_invalidOperation(0, "SPFile_write()::stages out of bounds"));

	if(spFile->specializations.length >> 32)
		retError(clean, Error_invalidOperation(0, "SPFile_write()::specializations out of bounds"));

	//Each pipeline's ranges have to sit inside the pools, or a reader would index out of them.

	for (U64 i = 0; i < spFile->pipelines.length; ++i) {

		const SPPipelineBase pipeline = spFile->pipelines.ptr[i];

		if((U64)pipeline.stageStart + pipeline.stageCount > spFile->stages.length)
			retError(clean, Error_invalidState(0, "SPFile_write()::pipeline stage range out of bounds"));

		if((U64)pipeline.specializationStart + pipeline.specializationCount > spFile->specializations.length)
			retError(clean, Error_invalidState(0, "SPFile_write()::pipeline specialization range out of bounds"));

		if(pipeline.type == (U8) ESPPipelineType_Graphics && pipeline.stateIndex >= spFile->graphicsStates.length)
			retError(clean, Error_invalidState(0, "SPFile_write()::graphics pipeline stateIndex out of bounds"));

		if(pipeline.type == (U8) ESPPipelineType_Raytracing && pipeline.stateIndex >= spFile->raytracingStates.length)
			retError(clean, Error_invalidState(0, "SPFile_write()::ray tracing pipeline stateIndex out of bounds"));
	}

	//Only the attachments a blend state can reach are stored, so the section's length follows from the states.

	U64 blendAttachmentCount = 0, vertexBufferCount = 0, vertexAttributeCount = 0;

	for (U64 i = 0; i < spFile->graphicsStates.length; ++i) {

		const SPGraphicsState state = spFile->graphicsStates.ptr[i];

		blendAttachmentCount += SPBlendStateRuntime_storedAttachmentCount(state.blend);

		for (U8 j = 0; j < 16; ++j) {

			if(SPVertexLayoutRuntime_bufferMask(state.inputAssembler.vertexLayout) & ((U16)1 << j))
				++vertexBufferCount;

			if(SPVertexLayoutRuntime_attributeMask(state.inputAssembler.vertexLayout) & ((U16)1 << j))
				++vertexAttributeCount;
		}
	}

	if((blendAttachmentCount >> 32) || (vertexBufferCount >> 32) || (vertexAttributeCount >> 32))
		retError(clean, Error_invalidOperation(0, "SPFile_write()::graphics state entries out of bounds"));

	OxStream *stream = streamRef ? RefPtr_data(streamRef, OxStream) : NULL;

	//Get header size

	U64 headerSize = sizeof(SPHeader);

	if(!(spFile->flags & ESPSettingsFlags_HideMagicNumber))        //Magic number (can be hidden by a parent format)
		headerSize += sizeof(U32);

	headerSize += ListSPPipelineBase_bytes(spFile->pipelines);
	headerSize += ListSPStage_bytes(spFile->stages);
	headerSize += ListSPSpecialization_bytes(spFile->specializations);
	headerSize += spFile->graphicsStates.length * sizeof(SPGraphicsStateStored);
	headerSize += ListSPRaytracingState_bytes(spFile->raytracingStates);
	headerSize += blendAttachmentCount * sizeof(SPBlendAttachment);
	headerSize += vertexBufferCount * sizeof(U16);
	headerSize += vertexAttributeCount * sizeof(SPVertexAttribute);

	headerSize = (headerSize + 15) & ~15;

	if (!stream) {
		*offset += headerSize;
		gotoIfError3(clean, DLFile_write(&spFile->names, alloc, NULL, NULL, I32x4_zero(), offset, e_rr));
		goto clean;
	}

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, true, alloc, &cursor, e_rr));

	if(stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, *offset + headerSize, alloc, e_rr));

	const SPHeader header = (SPHeader) {
		.version = ESPVersion_V1_1,
		.flags = ESPFlag_None,
		.pipelineCount = (U32) spFile->pipelines.length,
		.stageCount = (U32) spFile->stages.length,
		.specializationCount = (U32) spFile->specializations.length,
		.graphicsStateCount = (U32) spFile->graphicsStates.length,
		.raytracingStateCount = (U32) spFile->raytracingStates.length,
		.blendAttachmentCount = (U32) blendAttachmentCount,
		.vertexBufferCount = (U32) vertexBufferCount,
		.vertexAttributeCount = (U32) vertexAttributeCount
	};

	if(!(spFile->flags & ESPSettingsFlags_HideMagicNumber))
		gotoIfError3(clean, StreamCursor_appendU32(&cursor, offset, SPHeader_MAGIC, alloc, e_rr));

	gotoIfError3(clean, StreamCursor_append(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	gotoIfError3(clean, StreamCursor_appendBuffer(
		&cursor, offset, ListSPPipelineBase_bufferConst(spFile->pipelines), alloc, e_rr
	));

	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSPStage_bufferConst(spFile->stages), alloc, e_rr));

	gotoIfError3(clean, StreamCursor_appendBuffer(
		&cursor, offset, ListSPSpecialization_bufferConst(spFile->specializations), alloc, e_rr
	));

	//A graphics state is written without its blend attachments, which follow the ray tracing states as their own
	// section, holding only the entries the blend state can reach.

	for (U64 i = 0; i < spFile->graphicsStates.length; ++i) {

		const SPGraphicsStateStored stored = SPGraphicsState_store(spFile->graphicsStates.ptr[i]);

		gotoIfError3(clean, StreamCursor_append(&cursor, offset, &stored, sizeof(stored), alloc, e_rr));
	}

	gotoIfError3(clean, StreamCursor_appendBuffer(
		&cursor, offset, ListSPRaytracingState_bufferConst(spFile->raytracingStates), alloc, e_rr
	));

	for (U64 i = 0; i < spFile->graphicsStates.length; ++i) {

		const SPBlendStateRuntime blend = spFile->graphicsStates.ptr[i].blend;

		for(U8 j = 0; j < 8; ++j)
			if(SPBlendStateRuntime_storesAttachment(blend, j))
				gotoIfError3(clean, StreamCursor_append(
					&cursor, offset, &blend.attachments[j], sizeof(SPBlendAttachment), alloc, e_rr
				));
	}

	for (U64 i = 0; i < spFile->graphicsStates.length; ++i) {

		const SPVertexLayoutRuntime layout = spFile->graphicsStates.ptr[i].inputAssembler.vertexLayout;
		const U16 bufferMask = SPVertexLayoutRuntime_bufferMask(layout);

		for(U8 j = 0; j < 16; ++j)
			if(bufferMask & ((U16)1 << j))
				gotoIfError3(clean, StreamCursor_appendU16(
					&cursor, offset, layout.bufferStrides12_isInstance1[j], alloc, e_rr
				));
	}

	for (U64 i = 0; i < spFile->graphicsStates.length; ++i) {

		const SPVertexLayoutRuntime layout = spFile->graphicsStates.ptr[i].inputAssembler.vertexLayout;
		const U16 attributeMask = SPVertexLayoutRuntime_attributeMask(layout);

		for(U8 j = 0; j < 16; ++j)
			if(attributeMask & ((U16)1 << j))
				gotoIfError3(clean, StreamCursor_append(
					&cursor, offset, &layout.attributes[j], sizeof(SPVertexAttribute), alloc, e_rr
				));
	}

	//Need to make sure the names oiDL is 16-byte aligned

	U8 pad[16] = { 0 };
	const U64 utilized = *offset & 15;

	if(utilized)
		gotoIfError3(clean, StreamCursor_append(&cursor, offset, pad, 16 - utilized, alloc, e_rr));

	//Append names

	StreamCursor_close(&cursor, alloc);
	gotoIfError3(clean, DLFile_write(&spFile->names, alloc, streamRef, NULL, I32x4_zero(), offset, e_rr));

clean:
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
