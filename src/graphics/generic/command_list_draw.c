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

//graphics/generic/command_list_draw.c

#include "graphics/generic/interface.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/commands.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "formats/oiSH/sh_binaries.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/texture_format.h"
#include "types/base/string_base.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"
#include "command_list_internal.h"

//Ray triangle position fetch requires every BLAS it reads to be built with ERTASBuildFlags_AllowDataAccessExt,
// but the shader picks its TLAS descriptor at runtime, so the exact target is unknowable at record time.
//The candidates are the TLASes this scope transitioned, since declaring that access is already required for
// correct synchronization; a scope without any TLAS transition has nothing to judge.
//The error only fires when every candidate provably fails the requirement (that includes the common single
// TLAS scene), so a legitimate multi TLAS mix or a device built TLAS is never accused.

//Bindful: the work ops are the validators, binds only set state, so bind order never matters and
// only recorded work has to be consistent.
//A pipeline on the device's default (bindless) layout is always fine: submit binds that table for the
// whole frame. A custom layout requires the bound table to be built from the exact DescriptorLayout the
// pipeline layout references, which is what makes the backend's lazy bind at this op provably valid.

static Bool CommandList_validateBindState(CommandList *commandList, PipelineRef *pipelineRef, Error *e_rr) {

	Bool s_uccess = true;

	PipelineLayoutRef *layoutRef = PipelineRef_ptr(pipelineRef)->layout;

	//Push constants are checked ahead of the cache below, because unlike the three identities it keys on,
	// their size is mutable state: a later setPushConstants can change it without rebinding anything.

	if (layoutRef != GraphicsDeviceRef_ptr(commandList->device)->defaultPipelineLayout) {

		const U32 declared = PipelineLayoutRef_ptr(layoutRef)->info.pushConstants.count ?
			PipelineLayoutRef_ptr(layoutRef)->info.pushConstants.constantBufferSize : 0;

		if(declared && commandList->pushConstantSize != declared)
			retError(clean, Error_invalidOperation(
				4,
				"CommandList_validateBindState() the pipeline's layout declares push constants that weren't "
				"written at this size (CommandListRef_setPushConstants)"
			));

		//Writing push constants a layout never declared can only mean the wrong pipeline is bound

		if(!declared && commandList->pushConstantSize)
			retError(clean, Error_invalidOperation(
				5,
				"CommandList_validateBindState() push constants were written but the pipeline's layout "
				"doesn't declare any"
			));

		//Push descriptors are mutable in the same way, so they're checked here rather than in the cache.
		//All or nothing: the layout's whole set has to have been written, since the backends emit the whole
		// set and a partial one would leave the rest pointing at whatever the last pipeline bound.

		//The runtime's own globals are a push descriptor the SUBMIT fills, so a layout carrying them asks
		// nothing of the caller; requiring a write here would make push constants and _frameId/_time
		// mutually exclusive, since wanting the former is what forces a custom layout in the first place.

		DescriptorLayoutRef *pushRef = PipelineLayoutRef_ptr(layoutRef)->info.pushDescriptors;

		const U64 pushCount =
			(pushRef && !PipelineLayout_hasRuntimeGlobals(PipelineLayoutRef_ptr(layoutRef))) ?
			DescriptorLayoutRef_ptr(pushRef)->info.bindings.length : 0;

		//Buffer class push descriptors are root descriptors on both backends, one raw GPU virtual address.
		//A texture cannot be one, since an address has nowhere to carry a format, mip or swizzle, so D3D12
		// routes it through a single entry descriptor table whose slot comes out of the bound heap's push
		// ring (DescriptorHeapInfo::maxPushDescriptors).
		//Samplers stay refused: they would need the same again in the separate SAMPLER heap.

		if (pushCount) {

			if(pushCount > OXC3_MAX_PUSH_DESCRIPTORS)
				retError(clean, Error_outOfBounds(
					8, pushCount, OXC3_MAX_PUSH_DESCRIPTORS,
					"CommandList_validateBindState() the pipeline's layout declares more push descriptors than "
					"OXC3_MAX_PUSH_DESCRIPTORS, which can never be written"
				));

			const ListDescriptorBinding pushBindings = DescriptorLayoutRef_ptr(pushRef)->info.bindings;

			//A scope names only the FIRST stage that reaches a resource, so the earliest stage of the bound
			// pipeline's type is the conservative answer: the backends expand it into every later stage.

			const EPipelineStage stage =
				PipelineRef_ptr(pipelineRef)->type == EPipelineType_Compute ? EPipelineStage_Compute : (
					PipelineRef_ptr(pipelineRef)->type == EPipelineType_Graphics ?
					EPipelineStage_Vertex : EPipelineStage_RtStart
				);

			//Bindings that need a descriptor, which is every one except the baked samplers.
			//Checked BEFORE the loop below reads any of them: an under written push would otherwise be
			// reported as whatever the stale descriptor happens to fail, rather than as the missing write.

			U64 pushWrites = pushBindings.length;

			for(U64 i = 0; i < pushBindings.length; ++i)
				if(DescriptorBinding_immutableSamplerId(pushBindings.ptr[i]))
					--pushWrites;

			if(commandList->pushDescriptorCount != pushWrites)
				retError(clean, Error_invalidOperation(
					6,
					"CommandList_validateBindState() the pipeline's layout declares push descriptors that "
					"weren't all written (CommandListRef_setPushDescriptors); immutable samplers take no write"
				));

			U32 texturePushes = 0;

			//An immutable sampler is baked into the layout, so it takes a binding but NO descriptor: the
			// caller writes only the bindings that need one, and writeId is what the descriptors are indexed
			// by while i indexes the bindings.

			U64 writeId = 0;

			for(U64 i = 0; i < pushBindings.length; ++i) {

				const DescriptorBinding binding = pushBindings.ptr[i];
				const ESHRegisterType type = (ESHRegisterType)(binding.registerType & ESHRegisterType_TypeMask);

				if(DescriptorBinding_immutableSamplerId(binding))
					continue;

				//A sampler has no push form on D3D12 at all.
				//It would need a slot in the SAMPLER heap, which is a second ring this does not carry, so it
				// stays refused rather than working on one backend.

				//Only a BAKED sampler is expressible; createDescriptorLayout already refuses the rest, so
				// reaching here means a layout built before that check.

				if(type == ESHRegisterType_Sampler || type == ESHRegisterType_SamplerComparisonState)
					retError(clean, Error_unsupportedOperation(
						1,
						"CommandList_validateBindState() a sampler push descriptor has to be immutable; "
						"there is no root sampler to push one into"
					));

				//A subpass input is only meaningful inside a render pass' attachment set, never as a push.

				if(type == ESHRegisterType_SubpassInput)
					retError(clean, Error_unsupportedOperation(
						1, "CommandList_validateBindState() a subpass input cannot be a push descriptor"
					));

				const Bool isTexture = type >= ESHRegisterType_TextureStart && type < ESHRegisterType_SubpassInput;

				if(!isTexture && (type < ESHRegisterType_BufferStart || type > ESHRegisterType_BufferEnd))
					retError(clean, Error_unsupportedOperation(
						1,
						"CommandList_validateBindState() a push descriptor has to be a buffer class resource "
						"(constant, byte address, structured or acceleration structure) or a texture"
					));

				//The count was already proven to match the layout, so every binding has its descriptor

				const Descriptor d = commandList->pushDescriptors[writeId++];

				const ETransitionType transition =
					binding.registerType & ESHRegisterType_IsWrite ?
					ETransitionType_ShaderWrite : ETransitionType_ShaderRead;

				//A texture push is a single entry descriptor TABLE on D3D12, not a root descriptor, so it
				// needs a shader visible slot in the heap that is already bound.
				//The texture also has to allow the access the shader will make of it,
				// since ALLOW_UNORDERED_ACCESS and VK_IMAGE_USAGE_STORAGE_BIT are only set when it was
				// created asking for them.

				if (isTexture) {

					const Bool isWrite = (binding.registerType & ESHRegisterType_IsWrite) != 0;

					UnifiedTexture tex = TextureRef_getUnifiedTexture(d.resource, NULL);

					const EGraphicsResourceFlag needed =
						isWrite ? EGraphicsResourceFlag_ShaderWrite : EGraphicsResourceFlag_ShaderRead;

					if(!(tex.resource.flags & needed))
						retError(clean, Error_unsupportedOperation(
							1,
							"CommandList_validateBindState() a texture push descriptor requires "
							"EGraphicsResourceFlag_ShaderWrite when the binding writes and "
							"EGraphicsResourceFlag_ShaderRead when it reads"
						));

					++texturePushes;

					gotoIfError3(clean, CommandListRef_transitionImage(
						commandList, d.resource, (ImageRange) { 0 }, transition, stage, e_rr
					));

					continue;
				}

				if (type == ESHRegisterType_AccelerationStructure) {
					gotoIfError3(clean, CommandListRef_transitionRTAS(commandList, d.resource, transition, stage, e_rr));
					continue;
				}

				const BufferRange range = (BufferRange) {
					.startRange = Descriptor_startBuffer(&d),
					.endRange = Descriptor_endBuffer(&d)
				};

				gotoIfError3(clean, CommandListRef_transitionBuffer(commandList, d.resource, range, transition, stage, e_rr));
			}

			//Which heap the slots come from, resolved the same way the D3D12 backend resolves it, so a
			// layout that records here is one the backend can actually emit: the caller's own bound heap,
			// or the device's bindless heap when the layout uses the runtime bindless set, since that
			// pipeline binds the device heap for itself either way and the push adds no switch.
			//No other fallback exists on purpose: it would hide a heap switch, which is heavy on NV.

			if (texturePushes) {

				DescriptorHeapRef *pushHeapRef = commandList->boundDescriptorHeap;

				if(!pushHeapRef && PipelineLayout_usesRuntimeBindless(PipelineLayoutRef_ptr(layoutRef)))
					pushHeapRef = GraphicsDeviceRef_ptr(commandList->device)->defaultDescriptorHeaps;

				if(!pushHeapRef)
					retError(clean, Error_invalidOperation(
						8,
						"CommandList_validateBindState() a texture push descriptor takes a shader visible "
						"slot from the bound descriptor heap and no heap is bound "
						"(CommandListRef_bindDescriptorHeap); binding one behind the caller's back would "
						"hide a heap switch"
					));

				const U32 maxPush = DescriptorHeapRef_ptr(pushHeapRef)->info.maxPushDescriptors;

				if(maxPush < texturePushes)
					retError(clean, Error_outOfBounds(
						0, texturePushes, maxPush,
						"CommandList_validateBindState() the bound descriptor heap's maxPushDescriptors is "
						"too small for the texture push descriptors this layout declares"
					));
			}
		}

		if(!pushCount && commandList->pushDescriptorCount)
			retError(clean, Error_invalidOperation(
				7,
				"CommandList_validateBindState() push descriptors were written but the pipeline's layout "
				"doesn't declare any"
			));
	}

	//The rest of the outcome only depends on these three identities (layouts are immutable and descriptor
	// CONTENTS are not validated here), so a matching triple was already proven valid and long runs of work
	// ops between binds skip everything below.

	if(
		commandList->validatedPipeline == pipelineRef &&
		commandList->validatedTable == commandList->boundDescriptorTable &&
		commandList->validatedHeap == commandList->boundDescriptorHeap
	)
		return s_uccess;

	if(layoutRef == GraphicsDeviceRef_ptr(commandList->device)->defaultPipelineLayout)
		goto clean;

	const PipelineLayout *layout = PipelineLayoutRef_ptr(layoutRef);

	//A layout whose bindings are the device's own bindless set is served by the device's heap and table, so
	// the caller has nothing to bind: requiring it would make the bindless set and push constants mutually
	// exclusive, the same way requiring a write for the runtime globals would have.

	if (layout->info.bindings && !PipelineLayout_usesRuntimeBindless(layout)) {

		//The heap bind is explicit because switching heaps is expensive; a table silently implying its heap
		// would hide exactly the cost that made it explicit

		if(!commandList->boundDescriptorHeap)
			retError(clean, Error_invalidOperation(
				2,
				"CommandList_validateBindState() a custom layout needs its descriptor heap bound "
				"(CommandListRef_bindDescriptorHeap)"
			));

		if(!commandList->boundDescriptorTable)
			retError(clean, Error_invalidOperation(
				0,
				"CommandList_validateBindState() the pipeline's layout needs a descriptor table, but none is bound "
				"(CommandListRef_bindDescriptorTable)"
			));

		if(DescriptorTableRef_ptr(commandList->boundDescriptorTable)->parent != commandList->boundDescriptorHeap)
			retError(clean, Error_invalidOperation(
				3,
				"CommandList_validateBindState() the bound descriptor table doesn't belong to the bound heap"
			));

		if(DescriptorTableRef_ptr(commandList->boundDescriptorTable)->layout != layout->info.bindings)
			retError(clean, Error_invalidOperation(
				1,
				"CommandList_validateBindState() the bound descriptor table wasn't created from the DescriptorLayout "
				"the pipeline's layout references"
			));
	}

clean:

	if (s_uccess) {
		commandList->validatedPipeline = pipelineRef;
		commandList->validatedTable = commandList->boundDescriptorTable;
		commandList->validatedHeap = commandList->boundDescriptorHeap;
	}

	return s_uccess;
}

static Bool CommandList_validateRayTriPosition(CommandList *commandList, PipelineRef *pipelineRef, Error *e_rr) {

	Bool s_uccess = true;
	U64 candidates = 0;
	U64 provablyBad = 0;

	if(!(PipelineRef_ptr(pipelineRef)->extensions & (U32) ESHExtension_RayTriPosition))
		goto clean;

	for(U64 i = 0; i < commandList->pendingTransitions.length; ++i) {

		RefPtr *res = commandList->pendingTransitions.ptr[i].resource;

		if(!res || res->refPtrType->typeId != (TypeId) EGraphicsTypeId_TLASExt)
			continue;

		++candidates;

		const TLAS *tlas = TLASRef_ptr(res);

		if(TLAS_hasFlag(tlas, ETLASFlag_BlasDataAccessKnown) && !TLAS_hasFlag(tlas, ETLASFlag_BlasDataAccessAll))
			++provablyBad;
	}

	if(candidates && candidates == provablyBad)
		retError(clean, Error_invalidState(
			0,
			"CommandList_validateRayTriPosition() the pipeline uses RayTriPosition, but every TLAS transitioned "
			"in this scope has a BLAS built without ERTASBuildFlags_AllowDataAccessExt"
		));

clean:
	return s_uccess;
}

Bool CommandList_validateGraphicsPipeline(
	Pipeline *pipeline,
	ImageAndRange images[8],
	U8 imageCount,
	EDepthStencilFormat depthFormat,
	EMSAASamples boundSampleCount,
	Error *e_rr
) {

	Bool s_uccess = true;
	PipelineGraphicsInfo *info = Pipeline_info(pipeline, PipelineGraphicsInfo);

	//Depth stencil state can be set to None to ignore writing to depth stencil

	if (info->depthFormatExt != EDepthStencilFormat_None && depthFormat != info->depthFormatExt)
		retError(clean, Error_invalidState(0, "CommandList_validateGraphicsPipeline()::depthFormat was incompatible"));

	if(info->msaa != boundSampleCount)
		retError(clean, Error_invalidState(
			0,
			"CommandList_validateGraphicsPipeline()::boundSampleCount is incompatible with pipeline MSAA setting"
		));

	//Validate attachments

	if (info->attachmentCountExt != imageCount)
		retError(clean, Error_invalidState(1, "CommandList_validateGraphicsPipeline()::imageCount was incompatible"));

	for (U8 i = 0; i < imageCount && i < 8; ++i) {

		//Undefined is used to ignore the currently bound slot and to avoid writing to it

		if (!info->attachmentFormatsExt[i])
			continue;

		//Validate if formats are the same

		RefPtr *ref = images[i].image;

		if (!ref)
			retError(clean, Error_nullPointer(1, "CommandList_validateGraphicsPipeline()::images[i] is required by pipeline"));

		if (!TextureRef_isRenderTargetWritable(ref))
			retError(clean, Error_invalidParameter(1, i, "CommandList_validateGraphicsPipeline()::images[i] is invalid type"));

		DeviceResourceVersion v;
		const UnifiedTexture tex = TextureRef_getUnifiedTexture(ref, &v);

		if(info->attachmentFormatsExt[i] != tex.textureFormatId)
			retError(clean, Error_invalidState(i + 2, "CommandList_validateGraphicsPipeline()::images[i] is invalid format"));

		if(info->msaa != tex.sampleCount)
			retError(clean, Error_invalidState(
				i + 2,
				"CommandList_validateGraphicsPipeline()::images[i] has mismatching MSAA between pipeline and RenderTarget"
			));
	}

clean:
	return s_uccess;
}

Bool CommandListRef_setPipeline(CommandListRef *commandListRef, PipelineRef *pipelineRef, EPipelineType type, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validateScope(commandListRef, clean)

	if (!pipelineRef || pipelineRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_Pipeline)
		retError(clean, Error_nullPointer(1, "CommandListRef_setPipeline()::pipelineRef is required"));

	const Pipeline *pipeline = PipelineRef_ptr(pipelineRef);

	if(pipeline->device != commandList->device)
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_setPipeline()::pipelineRef is owned by different device"
		));

	if(pipeline->type != type)
		retError(clean, Error_unsupportedOperation(
			1, "CommandListRef_setPipeline()::pipeline's type is incompatible with type"
		));

	ECommandOp op = ECommandOp_SetComputePipeline;

	if(type == EPipelineType_Graphics)
		op = ECommandOp_SetGraphicsPipeline;

	else if(type == EPipelineType_RaytracingExt)
		op = ECommandOp_SetRaytracingPipelineExt;

	PipelineRef *commandOp[2] = { pipelineRef, NULL };        //Padding to 16-byte

	gotoIfError3(clean, CommandList_append(
		commandList, op, Buffer_createRefConst(commandOp, sizeof(commandOp)), 0, e_rr
	));

	if(!ListRefPtr_contains(commandList->resources, pipelineRef, 0, NULL)) {                        //TODO: hashSet
		RefPtr_inc(pipelineRef);        //CommandList will keep resource alive.
		gotoIfError3(clean, ListRefPtr_pushBack(&commandList->resources, pipelineRef, alloc, e_rr));
	}

	commandList->pipeline[type] = pipelineRef;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_setComputePipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_Compute, e_rr);
}

Bool CommandListRef_setGraphicsPipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_Graphics, e_rr);
}

Bool CommandListRef_setRaytracingPipeline(CommandListRef *commandList, PipelineRef *pipeline, Error *e_rr) {
	return CommandListRef_setPipeline(commandList, pipeline, EPipelineType_RaytracingExt, e_rr);
}

Bool CommandListRef_validateBufferDesc(
	GraphicsDeviceRef *device,
	DeviceBufferRef *buffer,
	EDeviceBufferUsage usage,
	U64 maxSize,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!buffer)
		return s_uccess;

	if(buffer->refPtrType->typeId != (TypeId) EGraphicsTypeId_DeviceBuffer)
		retError(clean, Error_unsupportedOperation(0, "CommandListRef_validateBufferDesc()::buffer has invalid type"));

	DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(buf->resource.device != device)
		retError(clean, Error_unsupportedOperation(
			1, "CommandListRef_validateBufferDesc()::buffer is owned by different device"
		));

	if((buf->usage & usage) != usage)
		retError(clean, Error_unsupportedOperation(
			2, "CommandListRef_validateBufferDesc()::buffer is missing required usage flag"
		));

	if(buf->resource.size > maxSize)
		retError(clean, Error_outOfBounds(
			1, buf->resource.size, maxSize,
			"CommandListRef_validateBufferDesc()::buffer is bigger than max limit"
		));

clean:
	return s_uccess;
}

Bool CommandListRef_setPrimitiveBuffers(CommandListRef *commandListRef, const SetPrimitiveBuffersCmd *buffers, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			0, "CommandListRef_setPrimitiveBuffers() is only available if render is started"
		));

	if(!buffers)
		retError(clean, Error_nullPointer(1, "CommandListRef_setPrimitiveBuffers()::buffers are required"));

	//Validate index and vertex buffers

	GraphicsDeviceRef *device = commandList->device;
	gotoIfError3(clean, CommandListRef_validateBufferDesc(
		device,
		buffers->indexBuffer,
		EDeviceBufferUsage_Index,
		U32_MAX,
		e_rr
	));

	if(!s_uccess)
		return s_uccess;

	for(U8 i = 0; i < 8; ++i)
		gotoIfError3(clean, CommandListRef_validateBufferDesc(
			device, buffers->vertexBuffers[i], EDeviceBufferUsage_Vertex, U32_MAX, e_rr
		));

	//Transition

	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList, buffers->indexBuffer, (BufferRange) { 0 }, ETransitionType_Index, EPipelineStage_Count, e_rr
	));

	for(U8 i = 0; i < 8; ++i)
		gotoIfError3(clean, CommandListRef_transitionBuffer(
			commandList, buffers->vertexBuffers[i], (BufferRange) { 0 }, ETransitionType_Vertex, EPipelineStage_Count, e_rr
		));

	//Issue command

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_SetPrimitiveBuffers, Buffer_createRefConst(buffers, sizeof(*buffers)), 0, e_rr
	));

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_drawBase(CommandListRef *commandListRef, Buffer buf, ECommandOp op, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(I32x2_any(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(0, "CommandListRef_drawBase() is only available if render is started"));

	PipelineRef *pipelineRef = commandList->pipeline[EPipelineType_Graphics];

	if (!pipelineRef)
		retError(clean, Error_invalidOperation(1, "CommandListRef_drawBase() requires bound graphics pipeline"));

	gotoIfError3(clean, CommandList_validateBindState(commandList, pipelineRef, e_rr));

	U32 flags = ECommandStateFlags_AnyScissor | ECommandStateFlags_AnyViewport;

	if ((commandList->tempStateFlags & flags) != flags)
		retError(clean, Error_invalidOperation(2, "CommandListRef_drawBase() requires viewport and scissor"));

	gotoIfError3(clean, CommandList_validateGraphicsPipeline(
		PipelineRef_ptr(pipelineRef),
		commandList->boundImages,
		commandList->boundImageCount,
		commandList->boundDepthFormat,
		commandList->boundSampleCount, e_rr
	));

	gotoIfError3(clean, CommandList_validateRayTriPosition(commandList, pipelineRef, e_rr));

	gotoIfError3(clean, CommandList_append(commandList, op, buf, 1, e_rr));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_draw(CommandListRef *commandListRef, const DrawCmd *draw, Error *e_rr) {

	Bool s_uccess = true;

	if(!draw)
		retError(clean, Error_nullPointer(1, "CommandListRef_draw()::draw is required"));

	if(!draw->count || !draw->instanceCount)        //No-op
		return s_uccess;

	if(draw->vertexOffset >> 31)
		retError(clean, Error_outOfBounds(
			1, draw->vertexOffset, (U32)I32_MAX, "CommandListRef_draw() vertexOffset out of bounds"
		));

	gotoIfError3(clean, CommandListRef_drawBase(
		commandListRef,
		Buffer_createRefConst(draw, sizeof(*draw)),
		ECommandOp_Draw,
		e_rr
	));

clean:
	return s_uccess;
}

Bool CommandListRef_drawIndexed(CommandListRef *commandList, U32 indexCount, U32 instanceCount, Error *e_rr) {
	const DrawCmd draw = (DrawCmd) { .count = indexCount, .instanceCount = instanceCount, .isIndexed = true };
	return CommandListRef_draw(commandList, &draw, e_rr);
}

Bool CommandListRef_drawIndexedAdv(
	CommandListRef *commandList,
	U32 indexCount, U32 instanceCount,
	U32 indexOffset, U32 instanceOffset,
	U32 vertexOffset,
	Error *e_rr
) {
	const DrawCmd draw = (DrawCmd) {
		.count = indexCount,
		.instanceCount = instanceCount,
		.indexOffset = indexOffset,
		.instanceOffset = instanceOffset,
		.vertexOffset = vertexOffset,
		.isIndexed = true
	};

	return CommandListRef_draw(commandList, &draw, e_rr);
}

Bool CommandListRef_drawUnindexed(CommandListRef *commandList, U32 vertexCount, U32 instanceCount, Error *e_rr) {
	const DrawCmd draw = (DrawCmd) { .count = vertexCount, .instanceCount = instanceCount };
	return CommandListRef_draw(commandList, &draw, e_rr);
}

Bool CommandListRef_drawUnindexedAdv(
	CommandListRef *commandList,
	U32 vertexCount, U32 instanceCount,
	U32 vertexOffset, U32 instanceOffset,
	Error *e_rr
) {
	const DrawCmd draw = (DrawCmd) {
		.count = vertexCount,
		.instanceCount = instanceCount,
		.vertexOffset = vertexOffset,
		.instanceOffset = instanceOffset
	};

	return CommandListRef_draw(commandList, &draw, e_rr);
}

Bool CommandListRef_dispatch(CommandListRef *commandListRef, DispatchCmd dispatch, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if (!commandList->pipeline[EPipelineType_Compute])
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatch() requires bound compute pipeline"));

	gotoIfError3(clean, CommandList_validateBindState(
		commandList, commandList->pipeline[EPipelineType_Compute], e_rr
	));

	gotoIfError3(clean, CommandList_validateRayTriPosition(
		commandList, commandList->pipeline[EPipelineType_Compute], e_rr
	));

	const U64 groupCountMax = U64_max(dispatch.groups[0], U64_max(dispatch.groups[1], dispatch.groups[2]));

	if(groupCountMax > U16_MAX)
		retError(clean, Error_outOfBounds(
			1, groupCountMax, U16_MAX, "CommandListRef_dispatch() groupCountMax out of bounds"
		));

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_Dispatch, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_dispatch1D(CommandListRef *commandList, U32 groupsX, Error *e_rr) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, 1, 1 } };
	return CommandListRef_dispatch(commandList, dispatch, e_rr);
}

Bool CommandListRef_dispatch2D(CommandListRef *commandList, U32 groupsX, U32 groupsY, Error *e_rr) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, groupsY, 1 } };
	return CommandListRef_dispatch(commandList, dispatch, e_rr);
}

Bool CommandListRef_dispatch3D(CommandListRef *commandList, U32 groupsX, U32 groupsY, U32 groupsZ, Error *e_rr) {
	const DispatchCmd dispatch = (DispatchCmd) { .groups = { groupsX, groupsY, groupsZ } };
	return CommandListRef_dispatch(commandList, dispatch, e_rr);
}

//Dispatch rays

Bool CommandListRef_dispatchRaysExt(CommandListRef *commandListRef, DispatchRaysExt dispatch, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	PipelineRef *rayPipeline = commandList->pipeline[EPipelineType_RaytracingExt];

	if (!rayPipeline)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() requires bound raytracing pipeline"));

	gotoIfError3(clean, CommandList_validateBindState(commandList, rayPipeline, e_rr));
	gotoIfError3(clean, CommandList_validateRayTriPosition(commandList, rayPipeline, e_rr));

	U64 total = dispatch.x * dispatch.y;

	if(total < dispatch.x || total * dispatch.z < total)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() overflow"));

	total *= dispatch.z;

	if(total > 1 * GIBI)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() is limited to 1Gibi rays"));

	if(!total)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() can't be executed with 0 count"));

	if(dispatch.raygenId >= Pipeline_info(PipelineRef_ptr(rayPipeline), PipelineRaytracingInfo)->raygenCount)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysExt() raygen index out of bounds"));

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_DispatchRaysExt, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_dispatch1DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 groupsX, Error *e_rr) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, 1, 1, raygenLocalId }, e_rr);
}

Bool CommandListRef_dispatch2DRaysExt(CommandListRef *commandList, U32 raygenLocalId, U32 groupsX, U32 groupsY, Error *e_rr) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, groupsY, 1, raygenLocalId }, e_rr);
}

Bool CommandListRef_dispatch3DRaysExt(
	CommandListRef *commandList,
	U32 raygenLocalId,
	U32 groupsX,
	U32 groupsY,
	U32 groupsZ,
	Error *e_rr
) {
	return CommandListRef_dispatchRaysExt(commandList, (DispatchRaysExt) { groupsX, groupsY, groupsZ, raygenLocalId }, e_rr);
}

//Indirect rendering

Bool CommandListRef_checkDispatchBuffer(GraphicsDeviceRef *device, DeviceBufferRef *buffer, U64 offset, U64 siz, Error *e_rr) {

	Bool s_uccess = true;

	if(!buffer || buffer->refPtrType->typeId != (TypeId) EGraphicsTypeId_DeviceBuffer)
		retError(clean, Error_nullPointer(1, "CommandListRef_checkDispatchBuffer()::buffer is required"));

	const DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(buf->resource.device != device)
		retError(clean, Error_unsupportedOperation(
			3, "CommandListRef_checkDispatchBuffer()::buffer is owned by different device"
		));

	if(offset + siz > buf->resource.size)
		retError(clean, Error_outOfBounds(
			1, offset + siz, buf->resource.size, "CommandListRef_checkDispatchBuffer()::offset + size is out of bounds"
		));

	if(!(buf->usage & EDeviceBufferUsage_Indirect))
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_checkDispatchBuffer()::buffer requires indirect buffer usage"
		));

clean:
	return s_uccess;
}

Bool CommandListRef_dispatchIndirect(CommandListRef *commandListRef, DeviceBufferRef *buffer, U64 offset, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean);
	GraphicsDeviceRef *device = commandList->device;

	if (!commandList->pipeline[EPipelineType_Compute])
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchIndirect() requires bound compute pipeline"));

	gotoIfError3(clean, CommandList_validateBindState(
		commandList, commandList->pipeline[EPipelineType_Compute], e_rr
	));

	gotoIfError3(clean, CommandList_validateRayTriPosition(
		commandList, commandList->pipeline[EPipelineType_Compute], e_rr
	));

	if(offset & 15)
		retError(clean, Error_invalidParameter(
			2, 0, "CommandListRef_dispatchIndirect()::offset has to be 16-byte aligned"
		));

	gotoIfError3(clean, CommandListRef_checkDispatchBuffer(device, buffer, offset, sizeof(U32) * 4, e_rr));

	const BufferRange range = (BufferRange) { .startRange = offset, .endRange = offset + sizeof(U32) * 4 };
	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList, buffer, range, ETransitionType_Indirect, EPipelineStage_Count, e_rr
	));

	const DispatchIndirectCmd dispatch = (DispatchIndirectCmd) { .buffer = buffer, .offset = offset };

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_DispatchIndirect, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_dispatchRaysIndirectExt(
	CommandListRef *commandListRef, DeviceBufferRef *buffer, U64 offset, U32 raygenLocalId, Error *e_rr
) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean);
	GraphicsDeviceRef *device = commandList->device;

	PipelineRef *rayPipeline = commandList->pipeline[EPipelineType_RaytracingExt];

	if(!rayPipeline)
		retError(clean, Error_invalidOperation(
			1, "CommandListRef_dispatchRaysIndirectExt() requires bound raytracing pipeline"
		));

	gotoIfError3(clean, CommandList_validateBindState(commandList, rayPipeline, e_rr));
	gotoIfError3(clean, CommandList_validateRayTriPosition(commandList, rayPipeline, e_rr));

	if(raygenLocalId >= Pipeline_info(PipelineRef_ptr(rayPipeline), PipelineRaytracingInfo)->raygenCount)
		retError(clean, Error_invalidOperation(1, "CommandListRef_dispatchRaysIndirectExt() raygen index out of bounds"));

	//The argument buffer holds three U32 thread counts the GPU wrote, VkTraceRaysIndirectCommandKHR on Vulkan and
	// the trailing Width, Height and Depth of D3D12_DISPATCH_RAYS_DESC on D3D12. Aligned to 4, the alignment
	// vkCmdTraceRaysIndirectKHR requires and the natural alignment of those counts on D3D12.

	if(offset & 3)
		retError(clean, Error_invalidParameter(
			2, 0, "CommandListRef_dispatchRaysIndirectExt()::offset has to be 4-byte aligned"
		));

	gotoIfError3(clean, CommandListRef_checkDispatchBuffer(device, buffer, offset, sizeof(U32) * 3, e_rr));

	const BufferRange range = (BufferRange) { .startRange = offset, .endRange = offset + sizeof(U32) * 3 };
	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList, buffer, range, ETransitionType_Indirect, EPipelineStage_Count, e_rr
	));

	const DispatchRaysIndirectExt dispatch =
		(DispatchRaysIndirectExt) { .buffer = buffer, .offset = offset, .raygenId = raygenLocalId };

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_DispatchRaysIndirect, Buffer_createRefConst(&dispatch, sizeof(dispatch)), 0, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandList_drawIndirectBase(
	CommandList *commandList,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 drawCalls,
	Bool indexed,
	Error *e_rr
) {

	Bool s_uccess = true;

	const U32 bufferStride = (U32)(indexed ? sizeof(DrawCallIndexed) : sizeof(DrawCallUnindexed));
	const DeviceBuffer *buf = DeviceBufferRef_ptr(buffer);

	if(!buf)
		retError(clean, Error_nullPointer(0, "CommandList_drawIndirectBase()::buffer is required"));

	if(bufferOffset & 15)
		retError(clean, Error_invalidParameter(
			2, 0, "CommandList_drawIndirectBase()::offset has to be 16-byte aligned"
		));

	if (!drawCalls)
		retError(clean, Error_invalidParameter(
			2, 2, "CommandList_drawIndirectBase() shouldn't be submitted if drawCalls is 0"
		));

	gotoIfError3(clean, CommandListRef_checkDispatchBuffer(
		commandList->device, buffer, bufferOffset, (U64)bufferStride * drawCalls, e_rr
	));

	const BufferRange range = (BufferRange) {
		.startRange = bufferOffset,
		.endRange = bufferOffset + bufferStride * drawCalls
	};

	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList,
		buffer,
		range,
		ETransitionType_Indirect,
		EPipelineStage_Count,
		e_rr
	));

clean:
	return s_uccess;
}

Bool CommandListRef_drawIndirect(
	CommandListRef *commandListRef,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	U32 drawCalls,
	Bool indexed,
	Error *e_rr
) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	gotoIfError3(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, drawCalls, indexed, e_rr));

	DrawIndirectCmd draw = (DrawIndirectCmd) {
		.buffer = buffer,
		.bufferOffset = bufferOffset,
		.drawCalls = drawCalls,
		.isIndexed = indexed
	};

	gotoIfError3(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createRefConst(&draw, sizeof(draw)), ECommandOp_DrawIndirect, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_drawIndirectCountExt(
	CommandListRef *commandListRef,
	DeviceBufferRef *buffer,
	U64 bufferOffset,
	DeviceBufferRef *countBuffer,
	U64 countOffset,
	U32 maxDrawCalls,
	Bool indexed,
	Error *e_rr
) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);
	gotoIfError3(clean, CommandList_drawIndirectBase(commandList, buffer, bufferOffset, maxDrawCalls, indexed, e_rr));

	if(!(device->info.capabilities.features & EGraphicsFeatures_MultiDrawIndirectCount))
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_drawIndirectCountExt() requires multiDrawIndirectCount extension, which was missing!"
		));

	gotoIfError3(clean, CommandListRef_checkDispatchBuffer(commandList->device, countBuffer, countOffset, sizeof(U32), e_rr));

	BufferRange range = (BufferRange) {
		.startRange = countOffset,
		.endRange = countOffset + sizeof(U32)
	};

	gotoIfError3(clean, CommandListRef_transitionBuffer(
		commandList, countBuffer, range, ETransitionType_Indirect, EPipelineStage_Count, e_rr
	));

	DrawIndirectCmd draw = (DrawIndirectCmd) {
		.buffer = buffer,
		.countBufferExt = countBuffer,
		.bufferOffset = bufferOffset,
		.countOffsetExt = countOffset,
		.drawCalls = maxDrawCalls,
		.isIndexed = indexed
	};

	gotoIfError3(clean, CommandListRef_drawBase(
		commandListRef, Buffer_createRefConst(&draw, sizeof(draw)), ECommandOp_DrawIndirectCount, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

//Feature RaytracingExt

Bool CommandListRef_updateRTASExt(CommandListRef *commandListRef, RTASRef *rtas, Bool isBLAS, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(!I32x2_all(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			0,
			"CommandListRef_updateRTASExt() is disallowed during render calls, as flushing might cause invalid render state"
		));

	if(!rtas || rtas->refPtrType->typeId != (TypeId)(isBLAS ? EGraphicsTypeId_BLASExt : EGraphicsTypeId_TLASExt))
		retError(clean, Error_unsupportedOperation(0, "CommandListRef_updateRTASExt() requires BLAS or TLAS"));

	gotoIfError3(clean, CommandListRef_transitionRTAS(
		commandList,
		rtas,
		ETransitionType_ShaderWrite,
		EPipelineStage_RTASBuild,
		e_rr
	));

	if (!isBLAS) {

		TLAS *tlas = TLASRef_ptr(rtas);

		if(!TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory))
			for (U64 j = 0; j < tlas->cpuInstances.length; ++j) {

				TLASInstanceData dat = (TLASInstanceData) { 0 };
				TLAS_getInstanceDataCpu(tlas, j, &dat);

				if (!dat.blasCpu)
					continue;

				gotoIfError3(clean, CommandListRef_transitionRTAS(
					commandList, dat.blasCpu,
					ETransitionType_ShaderRead, EPipelineStage_RTASBuild, e_rr
				));
			}
	}

	RTASRef *args[2] = { rtas, NULL };

	gotoIfError3(clean, CommandList_append(
		commandList,
		isBLAS ? ECommandOp_UpdateBLASExt : ECommandOp_UpdateTLASExt,
		Buffer_createRefConst(args, sizeof(args)),
		0, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_updateTLASExt(CommandListRef *commandList, TLASRef *tlas, Error *e_rr) {
	return CommandListRef_updateRTASExt(commandList, tlas, false, e_rr);
}

Bool CommandListRef_updateBLASExt(CommandListRef *commandList, BLASRef *blas, Error *e_rr) {
	return CommandListRef_updateRTASExt(commandList, blas, true, e_rr);
}

//The CPU half runs HERE rather than at submit: the size is what sizes the allocation, and every rule about
//when compaction is legal then reaches the caller instead of surfacing inside a submit.

Bool CommandListRef_compactBLASExt(CommandListRef *commandListRef, BLASRef *blas, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(!I32x2_all(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			0, "CommandListRef_compactBLASExt() is disallowed during render calls"
		));

	if(!blas || blas->refPtrType->typeId != (TypeId) EGraphicsTypeId_BLASExt)
		retError(clean, Error_unsupportedOperation(0, "CommandListRef_compactBLASExt() requires a BLAS"));

	//Everything about whether this may happen at all, plus the backend's allocation of the destination.
	//A structure that reports nothing to do leaves no op behind, so the recording stays empty rather than
	// carrying a copy that would do nothing.

	Bool recorded = false;

	//A COMPLETED TLAS without AllowUpdate can never re-resolve: its flush early outs forever, so marking
	// it stale would refuse every later submit with no way out. Refused here instead, while the caller can
	// still choose an updatable TLAS or compact before building.
	//A not yet completed TLAS is fine: its first build rides a later submit and resolves the new address.

	{
		GraphicsDevice *device = GraphicsDeviceRef_ptr(commandList->device);

		const ELockAcquire strandAcq = SpinLock_lock(&device->lock, U64_MAX);

		if(strandAcq < ELockAcquire_Success)
			retError(clean, Error_invalidState(
				2, "CommandListRef_compactBLASExt() couldn't acquire device lock to check dependent TLASes"
			));

		Bool stranded = false;

		for (U64 i = 0; i < device->liveTlases.length && !stranded; ++i) {

			const TLAS *tlas = TLASRef_ptr((TLASRef*) device->liveTlases.ptr[i]);

			if(TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory))
				continue;

			if(!tlas->base.isCompleted || (tlas->base.flags & ERTASBuildFlags_AllowUpdate))
				continue;

			for (U64 j = 0; j < tlas->cpuInstances.length && !stranded; ++j) {

				TLASInstanceData dat = (TLASInstanceData) { 0 };
				TLAS_getInstanceDataCpu(tlas, j, &dat);
				stranded = dat.blasCpu == blas;
			}
		}

		if(strandAcq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->lock);

		if(stranded)
			retError(clean, Error_invalidState(
				3,
				"CommandListRef_compactBLASExt() a completed TLAS without ERTASBuildFlags_AllowUpdate "
				"references this BLAS; compacting would strand it on the old address forever. Use an "
				"updatable TLAS or compact before building it"
			));
	}

	gotoIfError3(clean, GraphicsDeviceRef_prepareCompactBLAS(commandList->device, blas, &recorded, e_rr));

	if(!recorded)
		goto clean;

	//Every live TLAS that resolved this structure's address is about to be holding a stale one. Marking
	// them is what turns "traced a structure that moved" from a silently wrong frame into a refused submit;
	// recording an update of the TLAS clears it again, because that update re-resolves the addresses.
	//The adopt marks AGAIN at submit time (see GraphicsDeviceRef_markTlasesStaleForBLAS), because a pending
	// TLAS build riding the same submit resolves the old address and clears this mark before the move runs.

	gotoIfError3(clean, GraphicsDeviceRef_markTlasesStaleForBLAS(commandList->device, blas, false, e_rr));

	gotoIfError3(clean, CommandListRef_transitionRTAS(
		commandList, blas, ETransitionType_ShaderWrite, EPipelineStage_RTASBuild, e_rr
	));

	BLASRef *args[2] = { blas, NULL };

	gotoIfError3(clean, CommandList_append(
		commandList, ECommandOp_CompactBLASExt, Buffer_createRefConst(args, sizeof(args)), 0, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

//Bindful: only SETS state; the work ops validate it against the bound pipeline's layout.
//The KeepAlive transition is what makes end() collect the table into the command list's resources, which is
// also what carries it into the frame's resourcesInFlight at submit.

//Explicit rather than implied by the table: switching heaps can stall the GPU (D3D12 especially), so the
// switch has to be a visible command in the recording.

Bool CommandListRef_bindDescriptorHeap(CommandListRef *commandListRef, DescriptorHeapRef *heap, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validateScope(commandListRef, clean)

	if(!heap || heap->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorHeap)
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_bindDescriptorHeap() requires a descriptor heap"
		));

	if(DescriptorHeapRef_ptr(heap)->device != commandList->device)
		retError(clean, Error_unsupportedOperation(
			1, "CommandListRef_bindDescriptorHeap()::heap's device is incompatible"
		));

	if (!CommandListRef_isBound(commandList, heap, (ResourceRange) { 0 }, NULL)) {

		const TransitionInternal transition = (TransitionInternal) {
			.resource = heap, .type = ETransitionType_KeepAlive
		};

		gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));
	}

	DescriptorHeapRef *args[2] = { heap, NULL };

	gotoIfError3(clean, CommandList_append(
		commandList,
		ECommandOp_BindDescriptorHeap,
		Buffer_createRefConst(args, sizeof(args)),
		0, e_rr
	));

	commandList->boundDescriptorHeap = heap;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

//Push constants are pure state like the binds: the work op is what validates them against the bound
//pipeline's layout and the backends emit them there, which is also what makes a root signature switch
//between two work ops harmless (D3D12 drops all root arguments on such a switch).

Bool CommandListRef_setPushConstants(CommandListRef *commandListRef, Buffer data, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	const U64 size = Buffer_length(data);

	if(!size || size > sizeof(commandList->pushConstantData))
		retError(clean, Error_outOfBounds(
			1, size, sizeof(commandList->pushConstantData),
			"CommandListRef_setPushConstants()::data must be 1 to 128 bytes"
		));

	if(size & 3)
		retError(clean, Error_invalidParameter(
			1, 0, "CommandListRef_setPushConstants()::data must be a multiple of 4 bytes"
		));

	//The payload rides along in the command itself rather than as a pointer, so a replay doesn't depend on
	// the caller's buffer still being alive

	SetPushConstantsCmd cmd = (SetPushConstantsCmd) { .size = (U32) size };

	Buffer_memcpy(Buffer_createRef(cmd.data, size), data);

	gotoIfError3(clean, CommandList_append(
		commandList,
		ECommandOp_SetPushConstants,
		Buffer_createRefConst(&cmd, sizeof(cmd)),
		0, e_rr
	));

	Buffer_memcpy(Buffer_createRef(commandList->pushConstantData, size), data);

	commandList->pushConstantSize = (U8) size;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

//Push descriptors are pure state like the binds and the constants: the work op validates the written count
//against the bound pipeline's layout and the backends emit them there, so a root signature switch between
//two work ops is harmless and bind order never matters.

Bool CommandListRef_setPushDescriptors(CommandListRef *commandListRef, const ListDescriptor *descriptors, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;
	Buffer payload = Buffer_createNull();

	CommandListRef_validateScope(commandListRef, clean)

	const U64 count = !descriptors ? 0 : descriptors->length;

	if(!count || count > OXC3_MAX_PUSH_DESCRIPTORS)
		retError(clean, Error_outOfBounds(
			1, count, OXC3_MAX_PUSH_DESCRIPTORS,
			"CommandListRef_setPushDescriptors()::descriptors must hold 1 to OXC3_MAX_PUSH_DESCRIPTORS entries"
		));

	//No transition is recorded here on purpose: whether a push descriptor is read or written comes from the
	// layout's register type, and the layout isn't known until a pipeline is bound.
	//The work op transitions each of them (ShaderRead/ShaderWrite), which is also what keeps them alive.

	for (U64 i = 0; i < count; ++i)
		if(!descriptors->ptr[i].resource)
			retError(clean, Error_nullPointer(
				1, "CommandListRef_setPushDescriptors()::descriptors[i].resource is required"
			));

	//Header plus the descriptors themselves, so a replay doesn't depend on the caller's list still existing

	const U64 descriptorBytes = count * sizeof(Descriptor);

	gotoIfError3(clean, Buffer_createEmptyBytes(sizeof(SetPushDescriptorsCmd) + descriptorBytes, alloc, &payload, e_rr));

	*(SetPushDescriptorsCmd*) payload.ptrNonConst = (SetPushDescriptorsCmd) { .count = (U32) count };

	Buffer_memcpy(
		Buffer_createRef(payload.ptrNonConst + sizeof(SetPushDescriptorsCmd), descriptorBytes),
		Buffer_createRefConst(descriptors->ptr, descriptorBytes)
	);

	gotoIfError3(clean, CommandList_append(commandList, ECommandOp_SetPushDescriptors, payload, 0, e_rr));

	Buffer_memcpy(
		Buffer_createRef(commandList->pushDescriptors, descriptorBytes),
		Buffer_createRefConst(descriptors->ptr, descriptorBytes)
	);

	commandList->pushDescriptorCount = (U8) count;

clean:

	Buffer_free(&payload, alloc);

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_bindDescriptorTable(CommandListRef *commandListRef, DescriptorTableRef *table, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = commandListRef ? GraphicsDeviceRef_getAlloc(CommandListRef_ptr(commandListRef)->device) : NULL;

	CommandListRef_validateScope(commandListRef, clean)

	if(!table || table->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_bindDescriptorTable() requires a descriptor table"
		));

	if(DescriptorHeapRef_ptr(DescriptorTableRef_ptr(table)->parent)->device != commandList->device)
		retError(clean, Error_unsupportedOperation(
			1, "CommandListRef_bindDescriptorTable()::table's device is incompatible"
		));

	if (!CommandListRef_isBound(commandList, table, (ResourceRange) { 0 }, NULL)) {

		const TransitionInternal transition = (TransitionInternal) {
			.resource = table, .type = ETransitionType_KeepAlive
		};

		gotoIfError3(clean, ListTransitionInternal_pushBack(&commandList->pendingTransitions, transition, alloc, e_rr));
	}

	DescriptorTableRef *args[2] = { table, NULL };

	gotoIfError3(clean, CommandList_append(
		commandList,
		ECommandOp_BindDescriptorTable,
		Buffer_createRefConst(args, sizeof(args)),
		0, e_rr
	));

	commandList->boundDescriptorTable = table;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}

Bool CommandListRef_updateOmmExt(CommandListRef *commandListRef, OpacityMicromapRef *micromap, Error *e_rr) {

	Bool s_uccess = true;

	CommandListRef_validateScope(commandListRef, clean)

	if(!I32x2_all(I32x2_eq(commandList->currentSize, I32x2_zero)))
		retError(clean, Error_invalidOperation(
			0, "CommandListRef_updateOmmExt() is disallowed during render calls"
		));

	if(!micromap || micromap->refPtrType->typeId != (TypeId) EGraphicsTypeId_OpacityMicromapExt)
		retError(clean, Error_unsupportedOperation(
			0, "CommandListRef_updateOmmExt() requires an opacity micromap"
		));

	gotoIfError3(clean, CommandListRef_transitionRTAS(
		commandList,
		micromap,
		ETransitionType_ShaderWrite,
		EPipelineStage_RTASBuild,
		e_rr
	));

	RTASRef *args[2] = { micromap, NULL };

	gotoIfError3(clean, CommandList_append(
		commandList,
		ECommandOp_UpdateOmmExt,
		Buffer_createRefConst(args, sizeof(args)),
		0, e_rr
	));

	commandList->tempStateFlags |= ECommandStateFlags_HasModifyOp;

clean:

	if(!s_uccess && commandList)
		commandList->tempStateFlags |= ECommandStateFlags_InvalidState;

	return s_uccess;
}
