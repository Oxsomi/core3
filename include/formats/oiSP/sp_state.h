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

//formats/oiSP/sp_state.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//The values every field below is one of.
//They live with the format rather than with the graphics layer because a file has to be readable, printed and
// validated without a device, and a field stored as a raw integer is meaningless without the set it came from.

typedef enum ECullMode {
	ECullMode_Back,
	ECullMode_None,
	ECullMode_Front,
	ECullMode_Count
} ECullMode;

typedef enum ERasterizerFlags {
	ERasterizerFlags_IsClockWise            = 1 << 0,        //Winding order
	ERasterizerFlags_IsWireframeExt         = 1 << 1,        //Fill mode (only available with wireframe extension)
	ERasterizerFlags_EnableDepthClamp       = 1 << 2,
	ERasterizerFlags_EnableDepthBias        = 1 << 3
} ERasterizerFlags;

typedef enum ECompareOp {
	ECompareOp_Gt,
	ECompareOp_Geq,
	ECompareOp_Eq,
	ECompareOp_Neq,
	ECompareOp_Leq,
	ECompareOp_Lt,
	ECompareOp_Always,
	ECompareOp_Never,
	ECompareOp_Count
} ECompareOp;

typedef enum EStencilOp {
	EStencilOp_Keep,
	EStencilOp_Zero,
	EStencilOp_Replace,
	EStencilOp_IncClamp,
	EStencilOp_DecClamp,
	EStencilOp_Invert,
	EStencilOp_IncWrap,
	EStencilOp_DecWrap,
	EStencilOp_Count
} EStencilOp;

typedef enum EDepthStencilFlags {

	EDepthStencilFlags_DepthTest        = 1 << 0,
	EDepthStencilFlags_DepthWriteBit    = 1 << 1,        //Use DepthWrite instead.
	EDepthStencilFlags_StencilTest      = 1 << 2,

	EDepthStencilFlags_DepthWrite       = EDepthStencilFlags_DepthTest | EDepthStencilFlags_DepthWriteBit

} EDepthStencilFlags;

typedef enum ELogicOpExt {
	ELogicOpExt_Off,
	ELogicOpExt_Clear,
	ELogicOpExt_Set,
	ELogicOpExt_Copy,
	ELogicOpExt_CopyInvert,
	ELogicOpExt_None,
	ELogicOpExt_Invert,
	ELogicOpExt_And,
	ELogicOpExt_Nand,
	ELogicOpExt_Or,
	ELogicOpExt_Nor,
	ELogicOpExt_Xor,
	ELogicOpExt_Equiv,
	ELogicOpExt_AndReverse,
	ELogicOpExt_AndInvert,
	ELogicOpExt_OrReverse,
	ELogicOpExt_OrInvert,
	ELogicOpExt_Count
} ELogicOpExt;

typedef enum EBlend {

	EBlend_Zero,
	EBlend_One,

	EBlend_SrcColor,
	EBlend_InvSrcColor,
	EBlend_DstColor,
	EBlend_InvDstColor,

	EBlend_SrcAlpha,
	EBlend_InvSrcAlpha,
	EBlend_DstAlpha,
	EBlend_InvDstAlpha,

	EBlend_BlendFactor,
	EBlend_InvBlendFactor,
	EBlend_AlphaFactor,
	EBlend_InvAlphaFactor,
	EBlend_SrcAlphaSat,

	//Check dualSrcBlend feature

	EBlend_Src1ColorExt,
	EBlend_Src1AlphaExt,
	EBlend_InvSrc1ColorExt,
	EBlend_InvSrc1AlphaExt,

	EBlend_Count

} EBlend;

typedef enum EBlendOp {
	EBlendOp_Add,
	EBlendOp_Subtract,
	EBlendOp_ReverseSubtract,
	EBlendOp_Min,
	EBlendOp_Max,
	EBlendOp_Count
} EBlendOp;

typedef enum EWriteMask {

	EWriteMask_R    = 1 << 0,
	EWriteMask_G    = 1 << 1,
	EWriteMask_B    = 1 << 2,
	EWriteMask_A    = 1 << 3,

	EWriteMask_All  = 0xF,
	EWriteMask_RGBA = 0xF,
	EWriteMask_RGB  = 0x7,
	EWriteMask_RG   = 0x3

} EWriteMask;

typedef enum EMSAASamples {
	EMSAASamples_Off,       //Turn off MSAA ("x1")
	EMSAASamples_x2Ext,     //Query MSAA2x data types from device
	EMSAASamples_x4,        //4x Always supported
	EMSAASamples_x8Ext,     //Query MSAA8x data types from device
	EMSAASamples_Count
} EMSAASamples;

typedef enum ETopologyMode {

	ETopologyMode_TriangleList,
	ETopologyMode_TriangleStrip,

	ETopologyMode_LineList,
	ETopologyMode_LineStrip,

	ETopologyMode_PointList,

	ETopologyMode_TriangleListAdj,
	ETopologyMode_TriangleStripAdj,

	ETopologyMode_LineListAdj,
	ETopologyMode_LineStripAdj,

	EToplogyMode_Count

} ETopologyMode;

//How a ray tracing pipeline traces: which geometry it skips and which stages may be null.
//Stored by oiSP as rt.flags, so the default a pipeline assumes is named here rather than in the graphics layer.

typedef enum EPipelineRaytracingFlags {

	EPipelineRaytracingFlags_SkipTriangles            = 1 << 0,
	EPipelineRaytracingFlags_SkipAABBs                = 1 << 1,

	EPipelineRaytracingFlags_AllowMotionBlurExt       = 1 << 2,        //Requires feature RayMotionBlur

	//Disallowing null shaders in stages.
	//This is extra validation, but might also signal to the API that access to all stages are safe.

	EPipelineRaytracingFlags_NoNullAnyHit             = 1 << 3,
	EPipelineRaytracingFlags_NoNullClosestHit         = 1 << 4,
	EPipelineRaytracingFlags_NoNullMiss               = 1 << 5,
	EPipelineRaytracingFlags_NoNullIntersection       = 1 << 6,

	EPipelineRaytracingFlags_Count                    = 7,

	EPipelineRaytracingFlags_Default                  = EPipelineRaytracingFlags_SkipAABBs,

	EPipelineRaytracingFlags_DefaultStrict            =
		EPipelineRaytracingFlags_SkipAABBs | EPipelineRaytracingFlags_NoNullClosestHit | EPipelineRaytracingFlags_NoNullMiss

} EPipelineRaytracingFlags;

//The pipeline state a backend binds, which is the same state oiSP stores.
//It's defined here rather than in the graphics layer because a stored pipeline has to stay readable without a device,
// and the graphics layer aliases these, so state moves between a file and a live pipeline as a copy.
//Every field is fixed width and every enum is stored as its underlying integer, since these are serialized verbatim.
//The sizes are part of the format, so sp_file.h pins them with a static_assert.

typedef struct SPRasterizerState {

	U16 cullMode;                           //ECullMode
	U16 flags;                              //ERasterizerFlags

	F32 depthBiasClamp;

	I32 depthBiasConstantFactor;

	F32 depthBiasSlopeFactor;

} SPRasterizerState;

typedef struct SPDepthState {

	U8 flags;                               //EDepthStencilFlags
	U8 depthCompare;                        //ECompareOp
	U8 stencilCompare;                      //ECompareOp
	U8 stencilFail;                         //EStencilOp

	U8 stencilPass;                         //EStencilOp
	U8 stencilDepthFail;                    //EStencilOp
	U8 stencilWriteMask;
	U8 stencilReadMask;

} SPDepthState;

typedef struct SPBlendAttachment {          //How one render target combines with what's already there

	U8 srcBlend, dstBlend;                  //EBlend

	U8 srcBlendAlpha, dstBlendAlpha;        //EBlend

	U8 blendOp, blendOpAlpha;               //EBlendOp

} SPBlendAttachment;

typedef struct SPBlendStateRuntime {

	Bool enable;
	Bool allowIndependentBlend;             //0 = every target uses attachments[0]
	U8 renderTargetMask;                    //Bit per render target
	U8 logicOpExt;                          //ELogicOpExt; replaces blending when set

	U8 writeMask[8];                        //EWriteMask

	SPBlendAttachment attachments[8];

} SPBlendStateRuntime;

typedef struct SPVertexAttribute {          //One vertex input location

	U16 offset11;                           //Byte offset within its buffer, 11 bits
	U8 bufferId4;                           //Which buffer it fetches from, 4 bits
	U8 format;                              //ETextureFormatId; must not be compressed

} SPVertexAttribute;

typedef struct SPVertexLayoutRuntime {

	U16 bufferStrides12_isInstance1[16];    //Per buffer: stride in bits 0-11, per instance in bit 12

	SPVertexAttribute attributes[16];       //Per shader input location

} SPVertexLayoutRuntime;

#ifdef __cplusplus
	}
#endif
