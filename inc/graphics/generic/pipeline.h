/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/list.h"
#include "types/string.h"
#include "formats/texture.h"
#include "types/ref_ptr.h"
#include "pipeline_structs.h"

typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DeviceBufferRef;

//Graphics pipeline

typedef struct PipelineGraphicsInfo {

	VertexBindingLayout vertexLayout;		//Can be empty if pipeline generates all vertices itself

	Rasterizer rasterizer;

	DepthStencilState depthStencil;
	BlendState blendState;

	EMSAASamples msaa;
	ETopologyMode topologyMode;

	U32 patchControlPointsExt;				//Only if TessellationShader feature is enabled.
	U32 stageCount;							//Non zero used to determine where stages start/end.

	F32 msaaMinSampleShading;				//MSAA quality improvement (but extra perf overhead), set to > 0 to enable
	U32 padding;

	//One of these can be used but not together.

	//If DirectRendering is on (used in between start render).
	//Otherwise this is ignored.

	U8 attachmentFormatsExt[8];				//ETextureFormatId

	U32 attachmentCountExt;
	EDepthStencilFormat depthFormatExt;

	//If DirectRendering is off (used in between start render pass).

	RefPtr *renderPass;						//Required only if DirectRendering is not on.

	U32 subPass;							//^
	U32 padding2;

} PipelineGraphicsInfo;

//Raytracing pipeline

typedef enum EPipelineRaytracingFlags {

	EPipelineRaytracingFlags_SkipTriangles			= 1 << 0,
	EPipelineRaytracingFlags_SkipAABBs				= 1 << 1,

	EPipelineRaytracingFlags_AllowMotionBlurExt		= 1 << 2,		//Requires feature RayMotionBlur

	//Disallowing null shaders in stages.
	//This is extra validation, but might also signal to the API that library access is safe.

	EPipelineRaytracingFlags_NoNullAnyHit			= 1 << 3,
	EPipelineRaytracingFlags_NoNullClosestHit		= 1 << 4,
	EPipelineRaytracingFlags_NoNullMiss				= 1 << 5,
	EPipelineRaytracingFlags_NoNullIntersection		= 1 << 6,

	EPipelineRaytracingFlags_Count					= 7,

	EPipelineRaytracingFlags_Default				= EPipelineRaytracingFlags_SkipAABBs

} EPipelineRaytracingFlags;

typedef struct PipelineRaytracingGroup {

	//Indices into the local stage offset
	//Set to U32_MAX to indicate "no shader" (as long as EPipelineRaytracingFlags_NoNull<X> is not set).
	//Intersection can be U32_MAX anyways for triangle geometry.

	U32 closestHit, anyHit, intersection;

} PipelineRaytracingGroup;

TList(PipelineRaytracingGroup);

typedef struct PipelineRaytracingInfo {

	U8 flags;								//EPipelineRaytracingFlags
	U8 maxPayloadSize;						//In bytes; has to be 4-byte aligned.
	U8 maxAttributeSize;					//In bytes (>=8); has to be 4-byte aligned and <32.
	U8 maxRecursionDepth;					//<= 2. For multiple bounces, use for loop in raygen shader.

	U32 stageCount;							//Non zero used to determine where stages start/end.

	U32 binaryCount;						//Binaries that can be linked to by stages.

	U32 groupCount;							//Makes hit group indices from stages.

	//Only valid after construction

	ListBuffer binaries;
	ListPipelineRaytracingGroup groups;
	ListCharString entrypoints;

	DeviceBufferRef *shaderBindingTable;	//Auto generated SBT

	U64 sbtOffset;							//Offset in global SBT

	//Layout: [ hits (groupCount), misses (missCount), raygens (raygenCount), callables (callableCount) ]

	U32 raygenCount, missCount;
	U32 callableCount, padding;

} PipelineRaytracingInfo;

TList(PipelineGraphicsInfo);
TList(PipelineRaytracingInfo);
TList(PipelineStage);

typedef struct Pipeline {

	GraphicsDeviceRef *device;

	EPipelineType type;
	U32 padding;

	ListPipelineStage stages;

} Pipeline;

typedef RefPtr PipelineRef;

#define Pipeline_ext(ptr, T) (!(ptr) ? NULL : (T##Pipeline*)(ptr + 1))		//impl
#define PipelineRef_ptr(ptr) RefPtr_data(ptr, Pipeline)

impl extern const U64 PipelineExt_size;

#define Pipeline_info(ptr, T) ((T*)(!(ptr) ? NULL : ((const U8*)((const Pipeline*)(ptr) + 1) + PipelineExt_size)))

Error PipelineRef_dec(PipelineRef **pipeline);
Error PipelineRef_inc(PipelineRef *pipeline);

TListNamed(PipelineRef*, ListPipelineRef);

Bool PipelineRef_decAll(ListPipelineRef *list);					//Decrements all refs and frees list

Bool Pipeline_free(Pipeline *pipeline, Allocator alloc);

//shaderBinaries's Buffer will be moved (shaderBinaries will be cleared if moved)
Error GraphicsDeviceRef_createPipelinesCompute(
	GraphicsDeviceRef *deviceRef,
	ListBuffer *shaderBinaries,
	ListCharString names,					//Temporary names for debugging. Can be empty, else match shaderBinaries->length
	ListPipelineRef *pipelines
);

//stages and info will be freed and binaries of stages will be moved.
Error GraphicsDeviceRef_createPipelinesGraphics(
	GraphicsDeviceRef *deviceRef,
	ListPipelineStage *stages,
	ListPipelineGraphicsInfo *infos,
	ListCharString names,					//Temporary names for debugging. Can be empty, else match infos->length
	ListPipelineRef *pipelines
);

//stages, binaries, libraries and info will be freed and stages[i].binary be NULL.
Error GraphicsDeviceRef_createPipelineRaytracing(
	GraphicsDeviceRef *deviceRef,
	ListPipelineStage stages,
	ListBuffer *binaries,
	ListPipelineRaytracingGroup groups,
	ListPipelineRaytracingInfo infos,
	ListCharString *entrypoints,
	ListCharString names,					//Temporary names for debugging. Can be empty, else match infos->length
	ListPipelineRef *pipelines
);
