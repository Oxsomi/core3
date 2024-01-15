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
#include "platforms/ref_ptr.h"
#include "pipeline_structs.h"

typedef RefPtr GraphicsDeviceRef;
typedef enum ETextureFormat ETextureFormat;

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

TList(PipelineGraphicsInfo);
TList(PipelineStage);

typedef struct Pipeline {

	GraphicsDeviceRef *device;

	EPipelineType type;
	U32 padding;

	ListPipelineStage stages;

	const void *extraInfo;					//Null or points to after ext data for extraInfo (e.g. PipelineGraphicsInfo)

} Pipeline;

typedef RefPtr PipelineRef;

#define Pipeline_ext(ptr, T) (!ptr ? NULL : (T##Pipeline*)(ptr + 1))		//impl
#define PipelineRef_ptr(ptr) RefPtr_data(ptr, Pipeline)

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
