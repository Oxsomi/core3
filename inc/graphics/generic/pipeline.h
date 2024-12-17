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

#pragma once
#include "types/container/list.h"
#include "types/container/string.h"
#include "types/container/texture_format.h"
#include "types/container/ref_ptr.h"
#include "pipeline_structs.h"

#ifdef __cplusplus
	extern "C" {
#endif

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

	U32 patchControlPoints;					//Only if TessellationShader feature is enabled.
	F32 msaaMinSampleShading;				//MSAA quality improvement (but extra perf overhead), set to > 0 to enable

	//One of these can be used but not together.

	//If DirectRendering is on (used in between start render).
	//Otherwise, this is ignored.

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
	//This is extra validation, but might also signal to the API that access to all stages are safe.

	EPipelineRaytracingFlags_NoNullAnyHit			= 1 << 3,
	EPipelineRaytracingFlags_NoNullClosestHit		= 1 << 4,
	EPipelineRaytracingFlags_NoNullMiss				= 1 << 5,
	EPipelineRaytracingFlags_NoNullIntersection		= 1 << 6,

	EPipelineRaytracingFlags_Count					= 7,

	EPipelineRaytracingFlags_Default				= EPipelineRaytracingFlags_SkipAABBs,

	EPipelineRaytracingFlags_DefaultStrict			=
		EPipelineRaytracingFlags_SkipAABBs | EPipelineRaytracingFlags_NoNullClosestHit | EPipelineRaytracingFlags_NoNullMiss

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
	U8 maxRecursionDepth;					//1 or 2. For multiple bounces, use for loop in raygen shader.
	U16 padding0;

	//Only valid after construction

	ListPipelineRaytracingGroup groups;

	DeviceBufferRef *shaderBindingTable;	//Auto generated SBT

	//Layout: [ hits (groupCount), misses (missCount), raygens (raygenCount), callables (callableCount) ]

	U32 raygenCount, missCount;
	U32 callableCount, padding1;

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

void *Pipeline_infoOffset(Pipeline *ref);
#define Pipeline_info(ptr, T) ((T*)Pipeline_infoOffset(ptr))

Error PipelineRef_dec(PipelineRef **pipeline);
Error PipelineRef_inc(PipelineRef *pipeline);

Bool Pipeline_free(Pipeline *pipeline, Allocator alloc);

typedef struct SHFile SHFile;
typedef struct ListSHFile ListSHFile;

typedef enum ESHExtension ESHExtension;

//Get the first shader entry that's compatible with the current capabilities (extensions, shader model).
//And that has the same entrypointName and uniforms.
//Returns (U16 entryId, U16 binaryId) or U32_MAX if invalid
U32 GraphicsDeviceRef_getFirstShaderEntry(
	GraphicsDeviceRef *deviceRef,
	SHFile shaderBinary,
	CharString entrypointName,
	ListCharString uniforms,				//[ key, value ][]
	ESHExtension disallow,					//Extensions that should be disallowed (only find with extension disabled)
	ESHExtension require					//Extensions that should be required (only find with extension enabled)
);

Bool GraphicsDeviceRef_createPipelineCompute(
	GraphicsDeviceRef *deviceRef,
	SHFile shaderBinary,
	CharString name,						//Temporary name for debugging
	U32 entryId,							//Identifier from getFirstShaderEntry
	PipelineRef **pipeline,
	Error *e_rr
);

Bool GraphicsDeviceRef_createPipelineGraphics(
	GraphicsDeviceRef *deviceRef,
	ListSHFile shaderBinary,
	ListPipelineStage *stages,				//Will be moved
	PipelineGraphicsInfo info,
	CharString name,						//Temporary name for debugging
	PipelineRef **pipelines,
	Error *e_rr
);

//stages, groups will be freed
Bool GraphicsDeviceRef_createPipelineRaytracingExt(
	GraphicsDeviceRef *deviceRef,
	ListPipelineStage *stages,				//Will be moved
	ListSHFile binaries,
	ListPipelineRaytracingGroup *groups,	//Will be moved
	PipelineRaytracingInfo info,
	CharString name,						//Temporary name for debugging
	PipelineRef **pipeline,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
