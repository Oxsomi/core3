/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/texture.h"
#include "graphics/directx12/dx_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/buffer.h"
#include "types/string.h"
#include "types/error.h"
#include "types/math.h"

TList(D3D12_STATE_SUBOBJECT);
TList(D3D12_DXIL_LIBRARY_DESC);
TList(D3D12_EXPORT_DESC);
TList(D3D12_HIT_GROUP_DESC);
TList(ListU16);
TListNamed(wchar_t*, ListWCSTR);

TListImpl(D3D12_STATE_SUBOBJECT);
TListImpl(D3D12_DXIL_LIBRARY_DESC);
TListImpl(D3D12_EXPORT_DESC);
TListImpl(D3D12_HIT_GROUP_DESC);
TListImpl(ListU16);
TListNamedImpl(ListWCSTR);

Error GraphicsDevice_createPipelinesRaytracingInternalExt(
	GraphicsDeviceRef *deviceRef,
	ListCharString names,
	ListPipelineRef *pipelines,
	U64 stageCounter,
	U64 binaryCounter,
	U64 groupCounter
) {

	(void)stageCounter;
	(void)binaryCounter;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

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

	const wchar_t main[] = L"main";
	ListU16 mainUTF16 = (ListU16) { 0 };

	Error err = ListU16_createRefConst((const U16*)main, sizeof(main), &mainUTF16);

	if(err.genericError)
		return err;

	gotoIfError(clean, Buffer_createEmptyBytesx(raytracingShaderAlignment * stageCounter, &shaderTable));

	//Convert

	groupCounter = 0;

	for(U64 i = 0; i < pipelines->length; ++i) {

		Pipeline *pipeline = PipelineRef_ptr(pipelines->ptr[i]);
		PipelineRaytracingInfo *rtPipeline = Pipeline_info(pipeline, PipelineRaytracingInfo);

		//Reserve mem

		gotoIfError(clean, ListD3D12_HIT_GROUP_DESC_resizex(&hitGroups, rtPipeline->groupCount))
		gotoIfError(clean, ListD3D12_DXIL_LIBRARY_DESC_resizex(&libraries, rtPipeline->binaryCount))
		gotoIfError(clean, ListD3D12_EXPORT_DESC_resizex(&exportDescs, rtPipeline->stageCount))
		gotoIfError(clean, ListD3D12_STATE_SUBOBJECT_resizex(&stateObjects, 3 + hitGroups.length + libraries.length))
		gotoIfError(clean, ListU32_resizex(&binaryOffset, libraries.length))

		U64 groupNameStart = rtPipeline->stageCount * 2;		//Only if group size > stageCount, otherwise reuses strings
		U64 strings = groupNameStart + (I64)rtPipeline->stageCount;

		for(U64 j = strings; j < nameArr.length; ++j)		//Stop memleaks
			ListU16_freex(&nameArr.ptrNonConst[j]);

		gotoIfError(clean, ListListU16_resizex(&nameArr, strings))

		//Easy properties

		U64 stateObjectOff = 0;

		D3D12_RAYTRACING_PIPELINE_CONFIG1 config1 = (D3D12_RAYTRACING_PIPELINE_CONFIG1) {
			.MaxTraceRecursionDepth = rtPipeline->maxRecursionDepth,
			.Flags = (rtPipeline->flags & 3) << 8
		};

		stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
			.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1,
			.pDesc = &config1
		};

		D3D12_RAYTRACING_SHADER_CONFIG sizes = (D3D12_RAYTRACING_SHADER_CONFIG) {
			.MaxPayloadSizeInBytes = rtPipeline->maxPayloadSize,
			.MaxAttributeSizeInBytes = rtPipeline->maxAttributeSize,
		};

		stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
			.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
			.pDesc = &sizes
		};

		D3D12_GLOBAL_ROOT_SIGNATURE rootSig = (D3D12_GLOBAL_ROOT_SIGNATURE) {
			.pGlobalRootSignature =  deviceExt->defaultLayout
		};

		stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
			.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
			.pDesc = &rootSig
		};

		//Create binaries

		for(U32 j = 0; j < rtPipeline->binaryCount; ++j) {

			libraries.ptrNonConst[j] = (D3D12_DXIL_LIBRARY_DESC) {
				.DXILLibrary = (D3D12_SHADER_BYTECODE) {
					.pShaderBytecode = rtPipeline->binaries.ptr[j].ptr,
					.BytecodeLength = Buffer_length(rtPipeline->binaries.ptr[j])
				}
			};

			stateObjects.ptrNonConst[stateObjectOff++] = (D3D12_STATE_SUBOBJECT) {
				.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
				.pDesc = &libraries.ptr[j]
			};
		}

		//Count exports and create export names

		for(U32 j = 0; j < rtPipeline->stageCount; ++j) {

			gotoIfError(clean, CharString_formatx(&tmp, "#%"PRIu32, j))
			gotoIfError(clean, CharString_toUTF16x(tmp, &nameArr.ptrNonConst[j * 2]));
			CharString_freex(&tmp);

			if(rtPipeline->entrypoints.length && CharString_length(rtPipeline->entrypoints.ptr[j]))
				gotoIfError(clean, CharString_toUTF16x(rtPipeline->entrypoints.ptr[j], &nameArr.ptrNonConst[j * 2 + 1]))

			else nameArr.ptrNonConst[j * 2 + 1] = mainUTF16;

			++libraries.ptrNonConst[pipeline->stages.ptr[j].binaryId].NumExports;

			PipelineStage *stage = &pipeline->stages.ptrNonConst[j];

			if(stage->stageType == EPipelineStage_MissExt) {
				stage->localShaderId = rtPipeline->missCount;
				stage->groupId = rtPipeline->missCount + rtPipeline->groupCount;				//Resolve later
				++rtPipeline->missCount;
			}

			else if(stage->stageType == EPipelineStage_RaygenExt) {
				stage->localShaderId = rtPipeline->raygenCount;
				stage->groupId = rtPipeline->raygenCount + rtPipeline->groupCount;			//Resolve later
				++rtPipeline->raygenCount;
			}

			else if(stage->stageType == EPipelineStage_CallableExt) {
				stage->localShaderId = rtPipeline->callableCount;
				stage->groupId = rtPipeline->callableCount + rtPipeline->groupCount;			//Resolve later
				++rtPipeline->callableCount;
			}
		}

		//Resolve offsets so binaries export linearly

		U64 tempCounter = 0;

		for(U64 j = 0; j < rtPipeline->binaryCount; ++j) {

			D3D12_DXIL_LIBRARY_DESC *lib = &libraries.ptrNonConst[j];

			lib->pExports = exportDescs.ptr + tempCounter;

			tempCounter += lib->NumExports;
			lib->NumExports = 0;		//Reset for next iteration
		}

		//Resolve stage exports

		for(U32 j = 0; j < rtPipeline->stageCount; ++j) {

			PipelineStage *stage = &pipeline->stages.ptrNonConst[j];
			D3D12_DXIL_LIBRARY_DESC *lib = &libraries.ptrNonConst[stage->binaryId];

			((D3D12_EXPORT_DESC*)lib->pExports)[lib->NumExports] = (D3D12_EXPORT_DESC) {
				.Name = (const wchar_t*) nameArr.ptrNonConst[j * 2].ptr,
				.ExportToRename = (const wchar_t*) nameArr.ptrNonConst[j * 2 + 1].ptr
			};

			++lib->NumExports;

			if(stage->stageType == EPipelineStage_RaygenExt)
				stage->groupId += rtPipeline->missCount;

			else if(stage->stageType == EPipelineStage_CallableExt)
				stage->groupId += rtPipeline->raygenCount;
		}

		//Resolve hit groups

		for(U32 j = 0; j < rtPipeline->groupCount; ++j) {

			const PipelineRaytracingGroup group = rtPipeline->groups.ptr[j];

			ListU16 loc;

			gotoIfError(clean, CharString_formatx(&tmp, "H%"PRIu32, j))
			gotoIfError(clean, CharString_toUTF16x(tmp, &nameArr.ptrNonConst[groupNameStart + j]));
			CharString_freex(&tmp);
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

		gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateStateObject(
			deviceExt->device,
			&stateObjectInfo,
			&IID_ID3D12StateObject,
			(void**) stateObject
		)))

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && names.length && CharString_length(names.ptr[i])) {
			gotoIfError(clean, CharString_toUTF16x(names.ptr[i], &tmp16))
			gotoIfError(clean, dxCheck((*stateObject)->lpVtbl->SetName(*stateObject, (const wchar_t*) tmp16.ptr)))
			ListU16_freex(&tmp16);
		}

		gotoIfError(clean, dxCheck((*stateObject)->lpVtbl->QueryInterface(
			*stateObject, &IID_ID3D12StateObjectProperties, (void**) &dxPipeline->stateObjectProps
		)))

		//Resolve shader ids in SBT (individual shaders: raygen, callable and miss)

		for(U32 j = 0; j < rtPipeline->stageCount; ++j) {

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
			gotoIfError(clean, Buffer_createSubset(
				shaderTable,
				(stage.groupId + groupCounter) * raytracingShaderAlignment,
				raytracingShaderIdSize,
				false,
				&dst
			))

			Buffer_copy(dst, Buffer_createRefConst(shaderId, raytracingShaderIdSize));
		}

		//Insert individual hit groups

		for(U32 j = 0; j < rtPipeline->groupCount; ++j) {

			const void *shaderId = dxPipeline->stateObjectProps->lpVtbl->GetShaderIdentifier(
				dxPipeline->stateObjectProps,
				(const wchar_t*) nameArr.ptrNonConst[groupNameStart + j].ptr
			);
			//raytracingShaderAlignment

			Buffer dst = Buffer_createNull();
			gotoIfError(clean, Buffer_createSubset(
				shaderTable,
				(j + groupCounter) * raytracingShaderAlignment,
				raytracingShaderIdSize,
				false,
				&dst
			))

			Buffer_copy(dst, Buffer_createRefConst(shaderId, raytracingShaderIdSize));
		}

		rtPipeline->sbtOffset = groupCounter * raytracingShaderAlignment;
		groupCounter += rtPipeline->stageCount;
	}

	//Create one big SBT with all handles in it.
	//The SBT has a stride of 64 as well (since that's expected).
	//Then we link the SBT per pipeline.

	gotoIfError(clean, GraphicsDeviceRef_createBufferData(
		deviceRef, EDeviceBufferUsage_SBTExt, EGraphicsResourceFlag_None,
		CharString_createRefCStrConst("Shader binding table"),
		&shaderTable,
		&sbt
	))

	for (U64 i = 0; i < pipelines->length; ++i) {
		gotoIfError(clean, DeviceBufferRef_inc(sbt))
		Pipeline_info(PipelineRef_ptr(pipelines->ptr[i]), PipelineRaytracingInfo)->shaderBindingTable = sbt;
	}

clean:

	DeviceBufferRef_dec(&sbt);

	Buffer_freex(&shaderTable);

	for(U64 i = 0; i < nameArr.length; ++i)
		ListU16_freex(&nameArr.ptrNonConst[i]);

	ListListU16_freex(&nameArr);
	ListU32_freex(&binaryOffset);
	ListU16_freex(&tmp16);
	CharString_freex(&tmp);
	ListD3D12_STATE_SUBOBJECT_freex(&stateObjects);
	ListD3D12_DXIL_LIBRARY_DESC_freex(&libraries);
	ListD3D12_HIT_GROUP_DESC_freex(&hitGroups);
	ListD3D12_EXPORT_DESC_freex(&exportDescs);
	return err;
}
