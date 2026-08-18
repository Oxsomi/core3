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

//graphics/d3d12/generic/dx_raytracing_pipeline.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/d3d12/dx_device.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/container/string_unicode.h"
#include "types/base/error.h"
#include "types/base/mathi.h"
#include "types/base/constants.h"

TList(D3D12_STATE_SUBOBJECT);
TList(D3D12_DXIL_LIBRARY_DESC);
TList(D3D12_EXPORT_DESC);
TList(D3D12_HIT_GROUP_DESC);
TListNamed(wchar_t*, ListWCSTR);

TListImpl(D3D12_STATE_SUBOBJECT);
TListImpl(D3D12_DXIL_LIBRARY_DESC);
TListImpl(D3D12_EXPORT_DESC);
TListImpl(D3D12_HIT_GROUP_DESC);
TListNamedImpl(ListWCSTR);

Bool DX_WRAP_FUNC(GraphicsDevice_createPipelineRaytracingInternal)(
	GraphicsDeviceRef *deviceRef,
	const ListSHFile *binaries,
	const CharString *name,
	U8 maxPayloadSize,
	U8 maxAttributeSize,
	const ListU32 *binaryIndices,
	Pipeline *pipeline,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	PipelineRaytracingInfo *pipelineRt = Pipeline_info(pipeline, PipelineRaytracingInfo);

	ListD3D12_STATE_SUBOBJECT stateObjects = (ListD3D12_STATE_SUBOBJECT) { 0 };
	ListD3D12_DXIL_LIBRARY_DESC libraries = (ListD3D12_DXIL_LIBRARY_DESC) { 0 };
	ListD3D12_HIT_GROUP_DESC hitGroups = (ListD3D12_HIT_GROUP_DESC) { 0 };
	ListD3D12_EXPORT_DESC exportDescs = (ListD3D12_EXPORT_DESC) { 0 };
	ListListU16 nameArr = (ListListU16) { 0 };
	ListU32 binaryOffset = (ListU32) { 0 };
	CharString tmp = CharString_createNull();
	ListU16 tmp16 = (ListU16) { 0 };
	Buffer shaderTable = Buffer_createNull();
	DeviceBufferRef *sbt = NULL;

	U64 stageCount = pipeline->stages.length;
	U32 binaryCount = (U32) binaryIndices->length;
	U32 groupCount = (U32) pipelineRt->groups.length;

	gotoIfError3(clean, Buffer_createEmptyBytes(raytracingShaderAlignment * stageCount, alloc, &shaderTable, e_rr));

	//Reserve mem

	gotoIfError3(clean, ListD3D12_HIT_GROUP_DESC_resize(&hitGroups, groupCount, alloc, e_rr));
	gotoIfError3(clean, ListD3D12_DXIL_LIBRARY_DESC_resize(&libraries, binaryCount, alloc, e_rr));
	gotoIfError3(clean, ListD3D12_EXPORT_DESC_resize(&exportDescs, stageCount, alloc, e_rr));
	gotoIfError3(clean, ListD3D12_STATE_SUBOBJECT_resize(&stateObjects, 3 + hitGroups.length + libraries.length, alloc, e_rr));
	gotoIfError3(clean, ListU32_resize(&binaryOffset, libraries.length, alloc, e_rr));

	U64 groupNameStart = stageCount * 2;        //Only if group size > stageCount, otherwise reuses strings
	U64 strings = groupNameStart + (I64)stageCount;

	gotoIfError3(clean, ListListU16_resize(&nameArr, strings, alloc, e_rr));

	//Easy properties

	U64 stateObjectOff = 0;

	D3D12_RAYTRACING_PIPELINE_CONFIG1 config1 = (D3D12_RAYTRACING_PIPELINE_CONFIG1) {
		.MaxTraceRecursionDepth = pipelineRt->maxRecursionDepth,
		.Flags = ((U16)(pipelineRt->flags & 3)) << 8
	};

	//Not foldable into the shift above: that trick only lines up because SkipTriangles/SkipAABBs are bits 0 and
	// 1 and map to 0x100/0x200, while ALLOW_OPACITY_MICROMAPS is 0x400 and our bit for it is 7, not 2.

	if(pipelineRt->flags & EPipelineRaytracingFlags_AllowOpacityMicromapExt)
		config1.Flags |= D3D12_RAYTRACING_PIPELINE_FLAG_ALLOW_OPACITY_MICROMAPS;

	stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
		.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1,
		.pDesc = &config1
	};

	D3D12_RAYTRACING_SHADER_CONFIG sizes = (D3D12_RAYTRACING_SHADER_CONFIG) {
		.MaxPayloadSizeInBytes = maxPayloadSize,
		.MaxAttributeSizeInBytes = maxAttributeSize
	};

	stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
		.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
		.pDesc = &sizes
	};

	D3D12_GLOBAL_ROOT_SIGNATURE rootSig = (D3D12_GLOBAL_ROOT_SIGNATURE) {
		.pGlobalRootSignature = PipelineLayout_ext(PipelineLayoutRef_ptr(pipeline->layout), Dx)->rootSig
	};

	stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
		.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
		.pDesc = &rootSig
	};

	//Create binaries

	for(U32 j = 0; j < binaryCount; ++j) {

		U32 identifier = binaryIndices->ptr[j];
		U16 globalId = (U16)(identifier >> 16);
		U16 localId = (U16) identifier;

		const SHBinaryInfo *info = &binaries->ptr[globalId].binaries.ptr[localId];
		Buffer bin = info->binaries[ESHBinaryType_DXIL];

		libraries.ptrNonConst[j] = (D3D12_DXIL_LIBRARY_DESC) {
			.DXILLibrary = (D3D12_SHADER_BYTECODE) {
				.pShaderBytecode = bin.ptr,
				.BytecodeLength = Buffer_length(bin)
			}
		};

		stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
			.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
			.pDesc = &libraries.ptr[j]
		};
	}

	//Count exports and create export names

	for(U32 j = 0; j < stageCount; ++j) {

		gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, "#%"PRIu32, j));
		gotoIfError3(clean, CharString_toUTF16(tmp, alloc, &nameArr.ptrNonConst[j * 2], e_rr));
		CharString_free(&tmp, alloc);

		PipelineStage *stage = &pipeline->stages.ptrNonConst[j];

		U32 binId = stage->binaryId;
		U16 shaderId = stage->shFileId;

		U16 entrypointId = (U16) binId;
		U16 binaryId = (U16) (binId >> 16);

		SHFile bin = binaries->ptr[shaderId];
		SHEntry entry = bin.entries.ptr[entrypointId];
		U32 resolvedId = entry.binaryIds.ptr[binaryId] | ((U32) shaderId << 16);

		gotoIfError3(clean, CharString_toUTF16(entry.name, alloc, &nameArr.ptrNonConst[j * 2 + 1], e_rr));

		U64 libId = ListU32_findFirst(*binaryIndices, resolvedId, 0, NULL);
		++libraries.ptrNonConst[libId].NumExports;

		if(stage->stageType == EPipelineStage_MissExt) {
			stage->localShaderId = pipelineRt->missCount;
			stage->groupId = pipelineRt->missCount + groupCount;                //Resolve later
			++pipelineRt->missCount;
		}

		else if(stage->stageType == EPipelineStage_RaygenExt) {
			stage->localShaderId = pipelineRt->raygenCount;
			stage->groupId = pipelineRt->raygenCount + groupCount;                //Resolve later
			++pipelineRt->raygenCount;
		}

		else if(stage->stageType == EPipelineStage_CallableExt) {
			stage->localShaderId = pipelineRt->callableCount;
			stage->groupId = pipelineRt->callableCount + groupCount;            //Resolve later
			++pipelineRt->callableCount;
		}
	}

	//Resolve offsets so binaries export linearly

	U64 tempCounter = 0;

	for(U64 j = 0; j < binaryCount; ++j) {

		D3D12_DXIL_LIBRARY_DESC *lib = &libraries.ptrNonConst[j];

		lib->pExports = exportDescs.ptr + tempCounter;

		tempCounter += lib->NumExports;
		lib->NumExports = 0;        //Reset for next iteration
	}

	//Resolve stage exports

	for(U32 j = 0; j < stageCount; ++j) {

		PipelineStage *stage = &pipeline->stages.ptrNonConst[j];

		U32 binId = stage->binaryId;
		U16 shaderId = stage->shFileId;

		U16 entrypointId = (U16) binId;
		U16 binaryId = (U16) (binId >> 16);

		const SHFile *bin = &binaries->ptr[shaderId];
		const SHEntry *entry = &bin->entries.ptr[entrypointId];
		U32 resolvedId = entry->binaryIds.ptr[binaryId] | ((U32) shaderId << 16);
		U64 libId = ListU32_findFirst(*binaryIndices, resolvedId, 0, NULL);

		D3D12_DXIL_LIBRARY_DESC *lib = &libraries.ptrNonConst[libId];

		((D3D12_EXPORT_DESC*)lib->pExports)[lib->NumExports] = (D3D12_EXPORT_DESC) {
			.Name = (const wchar_t*) nameArr.ptrNonConst[j * 2].ptr,
			.ExportToRename = (const wchar_t*) nameArr.ptrNonConst[j * 2 + 1].ptr
		};

		++lib->NumExports;

		if(stage->stageType == EPipelineStage_RaygenExt)
			stage->groupId += pipelineRt->missCount;

		else if(stage->stageType == EPipelineStage_CallableExt)
			stage->groupId += pipelineRt->raygenCount;
	}

	//Resolve hit groups

	for(U32 j = 0; j < groupCount; ++j) {

		const PipelineRaytracingGroup group = pipelineRt->groups.ptr[j];

		ListU16 loc;

		gotoIfError3(clean, CharString_format(alloc, &tmp, e_rr, "H%"PRIu32, j));
		gotoIfError3(clean, CharString_toUTF16(tmp, alloc, &nameArr.ptrNonConst[groupNameStart + j], e_rr));
		CharString_free(&tmp, alloc);
		loc = nameArr.ptr[groupNameStart + j];

		hitGroups.ptrNonConst[j] = (D3D12_HIT_GROUP_DESC) {
			.HitGroupExport = (const wchar_t*)loc.ptr,
			.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES
		};

		if(group.anyHit != U32_MAX)
			hitGroups.ptrNonConst[j].AnyHitShaderImport = nameArr.ptr[group.anyHit * 2].ptr;

		if(group.closestHit != U32_MAX)
			hitGroups.ptrNonConst[j].ClosestHitShaderImport = nameArr.ptr[group.closestHit * 2].ptr;

		if(group.intersection != U32_MAX) {
			hitGroups.ptrNonConst[j].Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
			hitGroups.ptrNonConst[j].IntersectionShaderImport = nameArr.ptr[group.intersection * 2].ptr;
		}

		stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
			.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
			.pDesc = &hitGroups.ptr[j]
		};
	}

	//Create state object

	D3D12_STATE_OBJECT_DESC stateObjectInfo = (D3D12_STATE_OBJECT_DESC) {
		.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
		.NumSubobjects = (U32) stateObjects.length,
		.pSubobjects = stateObjects.ptr
	};

	DxPipeline *dxPipeline = Pipeline_ext(pipeline, Dx);
	ID3D12StateObject **stateObject = &dxPipeline->stateObject;

	gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateStateObject(
		deviceExt->device,
		&stateObjectInfo,
		&IID_ID3D12StateObject,
		(void**) stateObject
	), e_rr));

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && name && CharString_length(*name)) {
		gotoIfError3(clean, CharString_toUTF16(*name, alloc, &tmp16, e_rr));
		gotoIfError3(clean, dxCheck((*stateObject)->lpVtbl->SetName(*stateObject, (const wchar_t*) tmp16.ptr), e_rr));
	}

	gotoIfError3(clean, dxCheck((*stateObject)->lpVtbl->QueryInterface(
		*stateObject, &IID_ID3D12StateObjectProperties, (void**) &dxPipeline->stateObjectProps
	), e_rr));

	//Resolve shader ids in SBT (individual shaders: raygen, callable and miss)

	for(U32 j = 0; j < stageCount; ++j) {

		PipelineStage stage = pipeline->stages.ptrNonConst[j];

		//Skip all hit group individual shaders, these don't go in the SBT.
		//We need the actual hit groups

		if(stage.stageType >= EPipelineStage_RtHitStart && stage.stageType <= EPipelineStage_RtHitEnd)
			continue;

		const void *shaderId = dxPipeline->stateObjectProps->lpVtbl->GetShaderIdentifier(
			dxPipeline->stateObjectProps,
			(const wchar_t*) nameArr.ptrNonConst[j * 2].ptr
		);

		Buffer dst = Buffer_createNull();
		gotoIfError3(clean, Buffer_createSubset(
			shaderTable,
			stage.groupId * raytracingShaderAlignment,
			raytracingShaderIdSize,
			false,
			&dst, e_rr
		));

		Buffer_memcpy(dst, Buffer_createRefConst(shaderId, raytracingShaderIdSize));
	}

	//Insert individual hit groups

	for(U32 j = 0; j < groupCount; ++j) {

		const void *shaderId = dxPipeline->stateObjectProps->lpVtbl->GetShaderIdentifier(
			dxPipeline->stateObjectProps,
			(const wchar_t*) nameArr.ptrNonConst[groupNameStart + j].ptr
		);

		Buffer dst = Buffer_createNull();
		gotoIfError3(clean, Buffer_createSubset(
			shaderTable,
			j * raytracingShaderAlignment,
			raytracingShaderIdSize,
			false,
			&dst, e_rr
		));

		Buffer_memcpy(dst, Buffer_createRefConst(shaderId, raytracingShaderIdSize));
	}

	//Create one big SBT with all handles in it.
	//The SBT has a stride of 64 as well (since that's expected).
	//Then we link the SBT per pipeline.

	CharString sbtName = CharString_createRefCStrConst("Shader binding table");

	gotoIfError3(clean, GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_SBTExt, EGraphicsResourceFlag_None,
		NULL,
		&sbtName,
		&shaderTable,
		&sbt,
		e_rr
	));

	pipelineRt->shaderBindingTable = sbt;
	sbt = NULL;

clean:

	RefPtr_dec(&sbt);

	Buffer_free(&shaderTable, alloc);

	ListListU16_freeUnderlying(&nameArr, alloc);
	ListU32_free(&binaryOffset, alloc);
	ListU16_free(&tmp16, alloc);
	CharString_free(&tmp, alloc);
	ListD3D12_STATE_SUBOBJECT_free(&stateObjects, alloc);
	ListD3D12_DXIL_LIBRARY_DESC_free(&libraries, alloc);
	ListD3D12_HIT_GROUP_DESC_free(&hitGroups, alloc);
	ListD3D12_EXPORT_DESC_free(&exportDescs, alloc);
	return s_uccess;
}
