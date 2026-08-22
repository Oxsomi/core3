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

//graphics/generic/pipeline_structs.h

#pragma once
#include "formats/oiSP/sp_state.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EPipelineType {
	EPipelineType_Graphics,
	EPipelineType_Compute,
	EPipelineType_RaytracingExt,
	EPipelineType_Count
} EPipelineType;

typedef enum EPipelineStage {

	EPipelineStage_Vertex,
	EPipelineStage_Pixel,
	EPipelineStage_Compute,
	EPipelineStage_GeometryExt,            //Query graphics feature GeometryShader
	EPipelineStage_Hull,
	EPipelineStage_Domain,

	//Query graphics feature RayPipeline

	EPipelineStage_RaygenExt,
	EPipelineStage_CallableExt,
	EPipelineStage_MissExt,
	EPipelineStage_ClosestHitExt,
	EPipelineStage_AnyHitExt,
	EPipelineStage_IntersectionExt,

	EPipelineStage_RtStart    = EPipelineStage_RaygenExt,
	EPipelineStage_RtEnd      = EPipelineStage_IntersectionExt,
	EPipelineStage_RtHitStart = EPipelineStage_ClosestHitExt,
	EPipelineStage_RtHitEnd   = EPipelineStage_IntersectionExt,

	EPipelineStage_Count,

	EPipelineStage_RTASBuild = 0x100        //Only for use in transitions at RTAS build stage

} EPipelineStage;

extern const C8 *EPipelineStage_names[];

typedef struct PipelineStage {

	EPipelineStage stageType;    //Runtime only
	U32 binaryId;                //For non compute indicates offset in SHFile (contains both binaryId and entryId)

	U32 localShaderId;           //RT only at runtime
	U32 groupId;                 //RT only at runtime

	U16 shFileId;                //For non compute, indicates SHFile id
	U16 padding;

} PipelineStage;

//A pipeline's state is defined once by the format and aliased here rather than declared twice.
//Most of it is stored exactly as it's bound; blend and the vertex layout are stored in a packed form instead,
// which is why those two alias oiSP's Runtime structs. Either way sp_file.h pins the sizes with a static_assert.

typedef SPRasterizerState Rasterizer;
typedef SPDepthState DepthStencilState;
typedef SPBlendAttachment BlendStateAttachment;
typedef SPBlendStateRuntime BlendState;
typedef SPVertexAttribute VertexAttribute;
typedef SPVertexLayoutRuntime VertexBindingLayout;

#ifdef __cplusplus
	}
#endif
