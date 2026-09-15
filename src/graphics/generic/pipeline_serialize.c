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

//graphics/generic/pipeline_serialize.c

#include "graphics/generic/pipeline_serialize.h"
#include "types/math/type_cast.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/sampler.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/string.h"
#include "types/base/buffer_base.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"

Bool SPFile_toComputeStage(
	const SPFile *spFile, U32 pipelineId, const ListPipelineStage *stages, PipelineStage *stage, Error *e_rr
) {

	Bool s_uccess = true;

	if(!spFile || !stages || !stage)
		retError(clean, Error_nullPointer(
			!spFile ? 0 : (!stages ? 2 : 3), "SPFile_toComputeStage()::spFile, stages and stage are required"
		));

	if(pipelineId >= spFile->pipelines.length)
		retError(clean, Error_outOfBounds(
			1, pipelineId, spFile->pipelines.length, "SPFile_toComputeStage()::pipelineId out of bounds"
		));

	if(spFile->pipelines.ptr[pipelineId].type != (U8) ESPPipelineType_Compute)
		retError(clean, Error_invalidOperation(0, "SPFile_toComputeStage()::pipelineId isn't a compute pipeline"));

	if(stages->length != 1)
		retError(clean, Error_invalidState(0, "SPFile_toComputeStage() a compute pipeline needs one stage"));

	*stage = stages->ptr[0];

clean:
	return s_uccess;
}

Bool SPFile_toGraphicsInfo(const SPFile *spFile, U32 pipelineId, PipelineGraphicsInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	if(!spFile || !info)
		retError(clean, Error_nullPointer(!spFile ? 0 : 2, "SPFile_toGraphicsInfo()::spFile and info are required"));

	const SPGraphicsState *gfx = SPFile_graphicsState(spFile, pipelineId);

	if(!gfx)
		retError(clean, Error_invalidOperation(0, "SPFile_toGraphicsInfo()::pipelineId isn't a graphics pipeline"));

	*info = (PipelineGraphicsInfo) { 0 };

	//Every state struct is shared with oiSP, so lowering it is a copy rather than a repack.
	//Every field the specialization report lists has to land here, since a field that's reported but never
	// lowered is exactly the silent choice the report exists to surface.

	info->vertexLayout = gfx->inputAssembler.vertexLayout;
	info->rasterizer = gfx->rasterizer;
	info->depthStencil = gfx->depth;
	info->blendState = gfx->blend;

	for(U8 i = 0; i < 8; ++i)
		info->attachmentFormatsExt[i] = gfx->renderTargetFormats[i];

	info->attachmentCountExt = gfx->renderTargetCount;
	info->depthFormatExt = (EDepthStencilFormat) gfx->depthFormat;

	info->msaa = gfx->msaa;
	info->msaaMinSampleShading = gfx->msaaMinSampleShading;
	info->topologyMode = gfx->inputAssembler.topologyMode;
	info->patchControlPoints = gfx->inputAssembler.patchControlPoints;

clean:
	return s_uccess;
}

Bool SPFile_toRaytracingInfo(const SPFile *spFile, U32 pipelineId, PipelineRaytracingInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	if(!spFile || !info)
		retError(clean, Error_nullPointer(!spFile ? 0 : 2, "SPFile_toRaytracingInfo()::spFile and info are required"));

	const SPRaytracingState *rt = SPFile_raytracingState(spFile, pipelineId);

	if(!rt)
		retError(clean, Error_invalidOperation(0, "SPFile_toRaytracingInfo()::pipelineId isn't a ray tracing pipeline"));

	*info = (PipelineRaytracingInfo) {
		.flags = rt->raytracingFlags,
		.maxRecursionDepth = rt->maxRecursionDepth
	};

clean:
	return s_uccess;
}

Bool SPFile_fromGraphicsInfo(SPFile *spFile, U32 pipelineId, const PipelineGraphicsInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	if(!spFile || !info)
		retError(clean, Error_nullPointer(!spFile ? 0 : 2, "SPFile_fromGraphicsInfo()::spFile and info are required"));

	if(!SPFile_graphicsState(spFile, pipelineId))
		retError(clean, Error_invalidOperation(0, "SPFile_fromGraphicsInfo()::pipelineId isn't a graphics pipeline"));

	//Everything goes through supply(), so each field is recorded as the caller's rather than staying assumed; that's
	// what makes a dumped pipeline exact.

	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_RenderTargetCount, 0, info->attachmentCountExt, e_rr));

	//Only the declared targets have a format; supplying one past the count would grow the count to reach it.

	for(U8 i = 0; i < info->attachmentCountExt && i < 8; ++i)
		gotoIfError3(clean, SPFile_supply(
			spFile, pipelineId, ESPField_RenderTargetFormat, i, info->attachmentFormatsExt[i], e_rr
		));

	for (U8 i = 0; i < 8; ++i) {

		gotoIfError3(clean, SPFile_supply(
			spFile, pipelineId, ESPField_BlendWriteMask, i, info->blendState.writeMask[i], e_rr
		));

		const BlendStateAttachment attachment = info->blendState.attachments[i];

		gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendSrc, i, attachment.srcBlend, e_rr));
		gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendDst, i, attachment.dstBlend, e_rr));
		gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendSrcAlpha, i, attachment.srcBlendAlpha, e_rr));
		gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendDstAlpha, i, attachment.dstBlendAlpha, e_rr));
		gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendOp, i, attachment.blendOp, e_rr));
		gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendOpAlpha, i, attachment.blendOpAlpha, e_rr));
	}

	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendEnable, 0, info->blendState.enable, e_rr));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_BlendIndependent, 0, info->blendState.allowIndependentBlend, e_rr
	));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_BlendTargetMask, 0, info->blendState.renderTargetMask, e_rr
	));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_BlendLogicOp, 0, info->blendState.logicOpExt, e_rr));

	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_DepthFormat, 0, info->depthFormatExt, e_rr));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_DepthStencilFlags, 0, info->depthStencil.flags, e_rr));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_DepthCompare, 0, info->depthStencil.depthCompare, e_rr));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_StencilCompare, 0, info->depthStencil.stencilCompare, e_rr
	));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_StencilFail, 0, info->depthStencil.stencilFail, e_rr));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_StencilPass, 0, info->depthStencil.stencilPass, e_rr));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_StencilDepthFail, 0, info->depthStencil.stencilDepthFail, e_rr
	));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_StencilWriteMask, 0, info->depthStencil.stencilWriteMask, e_rr
	));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_StencilReadMask, 0, info->depthStencil.stencilReadMask, e_rr
	));

	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_CullMode, 0, info->rasterizer.cullMode, e_rr));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_RasterizerFlags, 0, info->rasterizer.flags, e_rr));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_DepthBiasConstant, 0, (U32) info->rasterizer.depthBiasConstantFactor, e_rr
	));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_DepthBiasClamp, 0, U32_fromF32Bits(info->rasterizer.depthBiasClamp), e_rr
	));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_DepthBiasSlope, 0,
		U32_fromF32Bits(info->rasterizer.depthBiasSlopeFactor), e_rr
	));

	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_Msaa, 0, info->msaa, e_rr));
	gotoIfError3(clean, SPFile_supply(
		spFile, pipelineId, ESPField_MsaaMinSampleShading, 0, U32_fromF32Bits(info->msaaMinSampleShading), e_rr
	));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_TopologyMode, 0, info->topologyMode, e_rr));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_PatchControlPoints, 0, info->patchControlPoints, e_rr));

	//The vertex layout isn't a specialization per location, so it's written into the state directly.

	SPGraphicsState *gfx = SPFile_graphicsStateMut(spFile, pipelineId);

	for (U8 i = 0; i < 16; ++i) {

		const VertexAttribute attribute = info->vertexLayout.attributes[i];

		gfx->inputAssembler.vertexLayout.attributes[i].format = attribute.format;
		gfx->inputAssembler.vertexLayout.attributes[i].offset11 = attribute.offset11;
		gfx->inputAssembler.vertexLayout.attributes[i].bufferId4 = attribute.bufferId4;

		const U16 stride = info->vertexLayout.bufferStrides12_isInstance1[i];

		gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_VertexBufferStride, i, stride & 4095, e_rr));
		gotoIfError3(clean, SPFile_supply(
			spFile, pipelineId, ESPField_VertexBufferRate, i, (stride >> 12) & 1, e_rr
		));
	}

clean:
	return s_uccess;
}

Bool SPFile_fromRaytracingInfo(SPFile *spFile, U32 pipelineId, const PipelineRaytracingInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	if(!spFile || !info)
		retError(clean, Error_nullPointer(!spFile ? 0 : 2, "SPFile_fromRaytracingInfo()::spFile and info are required"));

	if(!SPFile_raytracingState(spFile, pipelineId))
		retError(clean, Error_invalidOperation(0, "SPFile_fromRaytracingInfo()::pipelineId isn't a ray tracing pipeline"));

	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_MaxRecursionDepth, 0, info->maxRecursionDepth, e_rr));
	gotoIfError3(clean, SPFile_supply(spFile, pipelineId, ESPField_RaytracingFlags, 0, info->flags, e_rr));

clean:
	return s_uccess;
}

Bool Pipeline_toSPFile(
	PipelineRef *pipelineRef,
	const ListSHFile *files,
	const ListCharString *shaderNames,
	CharString name,
	const Allocator *alloc,
	SPFile *spFile,
	U32 *pipelineId,
	Error *e_rr
) {

	Bool s_uccess = true;
	U32 derivedId = U32_MAX;
	ListCharString runtimeNames = (ListCharString) { 0 };

	if(!pipelineRef || !files || !spFile)
		retError(clean, Error_nullPointer(0, "Pipeline_toSPFile()::pipelineRef, files and spFile are required"));

	Pipeline *pipeline = PipelineRef_ptr(pipelineRef);

	if(pipeline->stages.length > 16)
		retError(clean, Error_outOfBounds(0, pipeline->stages.length, 16, "Pipeline_toSPFile() too many stages"));

	//The pipeline keeps which (file, entry) each stage came from, which is exactly what deriving needs.

	SPStageRef stages[16];

	for (U64 i = 0; i < pipeline->stages.length; ++i) {

		const PipelineStage stage = pipeline->stages.ptr[i];

		stages[i] = (SPStageRef) {
			.fileId = (U16) stage.shFileId,
			.entryId = (U16) (stage.binaryId & U16_MAX)
		};
	}

	gotoIfError3(clean, GraphicsDeviceRef_runtimeRegisterNames(pipeline->device, &runtimeNames, alloc, e_rr));

	gotoIfError3(clean, SPFile_derivePipeline(
		spFile, files, shaderNames, name, stages, (U8) pipeline->stages.length, &runtimeNames, alloc, &derivedId, e_rr
	));

	//Deriving fills in what reflection proves and assumes the rest; recording the info the pipeline was really
	// created with then turns those assumptions into the caller's own values.

	switch (pipeline->type) {

		case EPipelineType_Graphics:
			gotoIfError3(clean, SPFile_fromGraphicsInfo(
				spFile, derivedId, Pipeline_info(pipeline, PipelineGraphicsInfo), e_rr
			));
			break;

		case EPipelineType_RaytracingExt:
			gotoIfError3(clean, SPFile_fromRaytracingInfo(
				spFile, derivedId, Pipeline_info(pipeline, PipelineRaytracingInfo), e_rr
			));
			break;

		//Compute carries no state beyond its shader, so deriving it is already exact.

		default:
			break;
	}

	if(pipelineId)
		*pipelineId = derivedId;

clean:

	ListCharString_free(&runtimeNames, alloc);
	return s_uccess;
}

//Finds the oiSH a stored stage names, so a pipeline can be rebuilt after a load.

static Bool PipelineSerialize_resolveStage(
	const SPFile *spFile,
	const SPStage stage,
	const ListSHFile *files,
	const ListCharString *shaderNames,
	U16 *fileId,
	U16 *entryId,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(stage.shaderFile == U32_MAX || stage.entrypoint == U32_MAX)
		retError(clean, Error_invalidState(0, "createPipelineFromSPFile() a stage has no shader or entrypoint name"));

	const CharString wantedFile = spFile->names.entryStrings.ptr[stage.shaderFile];
	const CharString wantedEntry = spFile->names.entryStrings.ptr[stage.entrypoint];

	U64 found = U64_MAX;

	for(U64 i = 0; i < files->length && i < (shaderNames ? shaderNames->length : 0); ++i)
		if (CharString_equalsStringSensitive(&shaderNames->ptr[i], &wantedFile)) {
			found = i;
			break;
		}

	if(found == U64_MAX)
		retError(clean, Error_notFound(0, 0, "createPipelineFromSPFile() a stage's shader file wasn't in the given files"));

	const SHFile *file = &files->ptr[found];

	//A shader that moved on since the pipeline was stored would silently compile into something else.

	if(stage.sourceHash && file->sourceHash && stage.sourceHash != file->sourceHash)
		retError(clean, Error_invalidState(
			1, "createPipelineFromSPFile() a stage's shader has changed since the pipeline was stored"
		));

	for (U64 i = 0; i < file->entries.length; ++i)
		if (CharString_equalsStringSensitive(&file->entries.ptr[i].name, &wantedEntry)) {
			*fileId = (U16) found;
			*entryId = (U16) i;
			goto clean;
		}

	retError(clean, Error_notFound(1, 0, "createPipelineFromSPFile() a stage's entrypoint wasn't in its shader"));

clean:
	return s_uccess;
}

//Whether two layouts each declare a binding in one and the same space.
//Only SPIRV can ever answer true: DXIL keeps the runtime's own registers in the reserved space, which a
// file's rows are refused from, while on SPIRV they are ordinary sets a bindful layout may legitimately own.

static Bool DescriptorLayouts_shareSpace(DescriptorLayoutRef *a, DescriptorLayoutRef *b) {

	if(!a || !b)
		return false;

	const ListDescriptorBinding lhs = DescriptorLayoutRef_ptr(a)->info.bindings;
	const ListDescriptorBinding rhs = DescriptorLayoutRef_ptr(b)->info.bindings;

	for(U64 i = 0; i < lhs.length; ++i)
		for(U64 j = 0; j < rhs.length; ++j)
			if(lhs.ptr[i].binding.space == rhs.ptr[j].binding.space)
				return true;

	return false;
}

//An unbounded array (count 0) is refused rather than guessed: its real capacity is a heap decision the file
// can't make, so the caller has to supply the count first.

Bool PLFile_createPipelineLayout(
	GraphicsDeviceRef *deviceRef,
	const PLFile *plFile,
	const Allocator *alloc,
	PipelineLayoutRef **layoutRef,
	Error *e_rr
) {

	Bool s_uccess = true;

	DescriptorLayoutInfo info = (DescriptorLayoutInfo) { 0 };
	ListDescriptorBinding bindings = (ListDescriptorBinding) { 0 };
	ListCharString bindingNames = (ListCharString) { 0 };
	DescriptorLayoutRef *descRef = NULL;
	SamplerRef *sampler = NULL;

	if(!deviceRef || !plFile || !layoutRef)
		retError(clean, Error_nullPointer(0, "PLFile_createPipelineLayout()::deviceRef, plFile and layoutRef required"));

	*layoutRef = NULL;

	//The device consumes its own numbering: SPIRV pairs on Vulkan, DXIL pairs on D3D12

	const Bool isDxil =
		GraphicsInstanceRef_ptr(GraphicsDeviceRef_ptr(deviceRef)->instance)->api == EGraphicsApi_Direct3D12;

	if (plFile->bindings.length) {

		for (U64 i = 0; i < plFile->bindings.length; ++i) {

			const PLDescriptorBinding row = plFile->bindings.ptr[i];
			const EGfxRegisterType classType = (EGfxRegisterType)(row.registerType & EGfxRegisterType_TypeMask);

			if(!row.count)
				retError(clean, Error_invalidState(
					0, "PLFile_createPipelineLayout() an unbounded array needs a supplied layout.binding.count"
				));

			const GfxBinding pair = row.bindings.arr[isDxil ? EGfxBinaryType_DXIL : EGfxBinaryType_SPIRV];

			if(pair.space == U32_MAX && pair.binding == U32_MAX)
				retError(clean, Error_invalidState(
					0, "PLFile_createPipelineLayout() a binding doesn't exist for the device's own binary type"
				));

			DescriptorBinding binding = (DescriptorBinding) {
				.registerType = (EGfxRegisterType) row.registerType,
				.count = row.count,
				.binding = pair,
				.visibility = row.visibility
			};

			switch (classType) {

				case EGfxRegisterType_ConstantBuffer:
					binding.constantBufferSize = row.strideOrLength;
					break;

				case EGfxRegisterType_StructuredBuffer:
				case EGfxRegisterType_StructuredBufferAtomic:
				case EGfxRegisterType_StorageBuffer:
				case EGfxRegisterType_StorageBufferAtomic:
					binding.structedBufferStride = row.strideOrLength;
					break;

				case EGfxRegisterType_Sampler:
				case EGfxRegisterType_SamplerComparisonState:

					if (row.samplerId) {

						if(row.samplerId - 1 >= plFile->samplers.length)
							retError(clean, Error_outOfBounds(
								0, row.samplerId - 1, plFile->samplers.length,
								"PLFile_createPipelineLayout() sampler row references a sampler the file hasn't got"
							));

						const CharString samplerName = CharString_createRefCStrConst("oiPL baked sampler");

						gotoIfError3(clean, GraphicsDeviceRef_createSampler(
							deviceRef, plFile->samplers.ptr[row.samplerId - 1], true, NULL, &samplerName, &sampler, e_rr
						));

						U32 immutableId = 0;

						gotoIfError3(clean, DescriptorLayoutInfo_addImmutableSampler(
							&info, sampler, &immutableId, alloc, e_rr
						));

						RefPtr_dec(&sampler);
						binding.immutableSamplerId = immutableId;
					}

					break;

				default:

					if(row.registerType & EGfxRegisterType_IsWrite && classType >= EGfxRegisterType_TextureStart)
						binding.textureFormat = row.texture;

					break;
			}

			CharString rowName = CharString_createNull();
			const U32 rowNameId = PLDescriptorBinding_name(row);

			if(rowNameId != PLDescriptorBinding_NAME_NONE && rowNameId < plFile->names.entryStrings.length)
				rowName = CharString_createRefStrConst(plFile->names.entryStrings.ptr[rowNameId]);

			gotoIfError3(clean, ListDescriptorBinding_pushBack(&bindings, binding, alloc, e_rr));
			gotoIfError3(clean, ListCharString_pushBack(&bindingNames, rowName, alloc, e_rr));
		}

		info.flags = (EDescriptorLayoutFlags) 0;
		info.bindings = bindings;
		info.bindingNames = bindingNames;
		bindings = (ListDescriptorBinding) { 0 };
		bindingNames = (ListCharString) { 0 };

		const CharString descName = CharString_createRefCStrConst("oiPL descriptor layout");
		gotoIfError3(clean, GraphicsDeviceRef_createDescriptorLayout(deviceRef, &info, &descName, &descRef, e_rr));
	}

	//An oiPL describes only the registers the caller owns.
	//The runtime's own (the bindless set and the per frame globals) were left out when the layout was derived,
	// since they belong to the device's layout rather than to a custom one, so putting them back is what makes
	// an instantiated layout describe the whole shader again.
	//Wanting push constants is exactly what makes a shader need a layout of its own, so without this it loses
	// the bindless set and _frameId/_time in the same breath and the driver rejects the pipeline for using
	// descriptors its layout never declared.
	//A file that declares bindings of its own is bindful and owns those spaces, so it keeps them and only the
	// device parts that can't collide with it are added.

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	PipelineLayoutInfo pipelineLayout = (PipelineLayoutInfo) {
		.bindings = descRef ? descRef : device->defaultDescLayout,
		.pushDescriptors =
			DescriptorLayouts_shareSpace(descRef, device->defaultCBufferLayout) ? NULL : device->defaultCBufferLayout
	};

	if (plFile->hasPushConstant) {

		//The root signature binds the constants at this exact register, so a D3D12 device can't guess it

		if(isDxil && plFile->pushConstant.bindings.arrU64[EGfxBinaryType_DXIL] == U64_MAX)
			retError(clean, Error_invalidState(
				0, "PLFile_createPipelineLayout() the push constants carry no DXIL b register for this device"
			));

		pipelineLayout.pushConstants = (DescriptorBinding) {
			.registerType = EGfxRegisterType_PushConstants,
			.count = 1,
			.binding = plFile->pushConstant.bindings.arr[isDxil ? EGfxBinaryType_DXIL : EGfxBinaryType_SPIRV],
			.visibility = plFile->pushConstant.visibility,
			.constantBufferSize = plFile->pushConstant.strideOrLength
		};
	}

	const CharString layoutName = CharString_createRefCStrConst("oiPL pipeline layout");
	gotoIfError3(clean, GraphicsDeviceRef_createPipelineLayout(deviceRef, &pipelineLayout, &layoutName, layoutRef, e_rr));

clean:

	//createDescriptorLayout zeroes info once it owns it, so this only frees what never made it in

	DescriptorLayoutInfo_free(&info, alloc);
	ListDescriptorBinding_free(&bindings, alloc);
	ListCharString_free(&bindingNames, alloc);
	RefPtr_dec(&sampler);
	RefPtr_dec(&descRef);
	return s_uccess;
}

Bool SPFile_createPipelineLayout(
	GraphicsDeviceRef *deviceRef,
	const SPFile *spFile,
	U32 pipelineId,
	const Allocator *alloc,
	PipelineLayoutRef **layoutRef,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!deviceRef || !spFile || !layoutRef)
		retError(clean, Error_nullPointer(0, "SPFile_createPipelineLayout()::deviceRef, spFile and layoutRef required"));

	if(pipelineId >= spFile->pipelines.length)
		retError(clean, Error_outOfBounds(
			2, pipelineId, spFile->pipelines.length, "SPFile_createPipelineLayout()::pipelineId invalid"
		));

	*layoutRef = NULL;

	const SPPipelineBase base = spFile->pipelines.ptr[pipelineId];

	if(base.layoutIndex != U32_MAX)
		gotoIfError3(clean, PLFile_createPipelineLayout(
			deviceRef, &spFile->layouts.ptr[base.layoutIndex], alloc, layoutRef, e_rr
		));

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_createPipelineFromSPFile(
	GraphicsDeviceRef *deviceRef,
	const SPFile *spFile,
	U32 pipelineId,
	const ListSHFile *files,
	const ListCharString *shaderNames,
	PipelineLayoutRef *layout,
	const Allocator *alloc,
	PipelineRef **pipelineRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	ListPipelineStage stageList = (ListPipelineStage) { 0 };
	ListPipelineRaytracingGroup groupList = (ListPipelineRaytracingGroup) { 0 };
	PipelineLayoutRef *ownedLayout = NULL;

	if(!deviceRef || !spFile || !files || !pipelineRef)
		retError(clean, Error_nullPointer(0, "createPipelineFromSPFile()::deviceRef, spFile, files and out are required"));

	if(pipelineId >= spFile->pipelines.length)
		retError(clean, Error_outOfBounds(
			2, pipelineId, spFile->pipelines.length, "createPipelineFromSPFile()::pipelineId invalid"
		));

	const SPPipelineBase base = spFile->pipelines.ptr[pipelineId];

	//Every stage is resolved by name first, so a missing shader is reported before anything is created.

	gotoIfError3(clean, ListPipelineStage_resize(&stageList, base.stageCount, alloc, e_rr));

	for (U8 i = 0; i < base.stageCount; ++i) {

		const SPStage stage = spFile->stages.ptr[base.stageStart + i];
		U16 fileId = 0, entryId = 0;

		gotoIfError3(clean, PipelineSerialize_resolveStage(spFile, stage, files, shaderNames, &fileId, &entryId, e_rr));

		const CharString entryName = files->ptr[fileId].entries.ptr[entryId].name;

		const U32 id = GraphicsDeviceRef_getFirstShaderEntry(
			deviceRef, &files->ptr[fileId], &entryName, NULL, NULL, ESHExtension_None, ESHExtension_None
		);

		if(id == U32_MAX)
			retError(clean, Error_invalidState(
				2, "createPipelineFromSPFile() no compatible binary for one of the pipeline's stages"
			));

		stageList.ptrNonConst[i] = (PipelineStage) { .binaryId = id, .shFileId = fileId };
	}

	CharString name = base.name != U32_MAX ? spFile->names.entryStrings.ptr[base.name] : CharString_createNull();

	//A caller supplied layout wins; otherwise the file's own is created, and NULL still means the device default

	if(!layout && base.layoutIndex != U32_MAX) {
		gotoIfError3(clean, SPFile_createPipelineLayout(deviceRef, spFile, pipelineId, alloc, &ownedLayout, e_rr));
		layout = ownedLayout;
	}

	switch (base.type) {

		case ESPPipelineType_Compute: {

			PipelineStage stage = (PipelineStage) { 0 };
			gotoIfError3(clean, SPFile_toComputeStage(spFile, pipelineId, &stageList, &stage, e_rr));

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineCompute(
				deviceRef, &files->ptr[stage.shFileId], &name, stage.binaryId, NULL,
				EPipelineFlags_None, layout, pipelineRef, e_rr
			));

			break;
		}

		case ESPPipelineType_Graphics: {

			PipelineGraphicsInfo info = (PipelineGraphicsInfo) { 0 };
			gotoIfError3(clean, SPFile_toGraphicsInfo(spFile, pipelineId, &info, e_rr));

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineGraphics(
				deviceRef, files, &stageList, &info, &name, EPipelineFlags_None, layout, pipelineRef, e_rr
			));

			break;
		}

		case ESPPipelineType_Raytracing: {

			PipelineRaytracingInfo info = (PipelineRaytracingInfo) { 0 };
			gotoIfError3(clean, SPFile_toRaytracingInfo(spFile, pipelineId, &info, e_rr));

			//Hit groups aren't stored yet, so one group is formed from whichever hit stages the pipeline binds, the
			// same pairing the deriving side used.

			U32 closestHit = U32_MAX, anyHit = U32_MAX, intersection = U32_MAX;

			for (U8 i = 0; i < base.stageCount; ++i)
				switch (spFile->stages.ptr[base.stageStart + i].stage) {
					case EGfxPipelineStage_ClosestHitExt:    closestHit = i;    break;
					case EGfxPipelineStage_AnyHitExt:        anyHit = i;        break;
					case EGfxPipelineStage_IntersectionExt:  intersection = i;  break;
					default:                                                    break;
				}

			if (closestHit != U32_MAX || anyHit != U32_MAX || intersection != U32_MAX) {

				const PipelineRaytracingGroup group = (PipelineRaytracingGroup) {
					.closestHit = closestHit,
					.anyHit = anyHit,
					.intersection = intersection
				};

				gotoIfError3(clean, ListPipelineRaytracingGroup_resize(&groupList, 1, alloc, e_rr));
				groupList.ptrNonConst[0] = group;
			}

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineRaytracingExt(
				deviceRef, &stageList, files, &groupList, &info, &name, EPipelineFlags_None, layout, pipelineRef, e_rr
			));

			break;
		}

		default:
			retError(clean, Error_unsupportedOperation(0, "createPipelineFromSPFile() unknown pipeline type"));
	}

clean:

	RefPtr_dec(&ownedLayout);

	//Both create calls move their lists, so anything left here is only from a path that never reached them.

	ListPipelineStage_free(&stageList, alloc);
	ListPipelineRaytracingGroup_free(&groupList, alloc);
	return s_uccess;
}
