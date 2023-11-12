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
#include "types/buffer.h"

typedef enum EPipelineType {

	EPipelineType_Graphics,
	EPipelineType_Compute,
	EPipelineType_Count

} EPipelineType;

typedef enum EPipelineStage {

	EPipelineStage_Vertex,
	EPipelineStage_Pixel,
	EPipelineStage_Compute,
	EPipelineStage_GeometryExt,			//Query graphics feature GeometryShader
	EPipelineStage_HullExt,				//Query graphics feature TessellationShader
	EPipelineStage_DomainExt,			//^

	EPipelineStage_Count

} EPipelineStage;

typedef enum ECullMode {

	ECullMode_Back,
	ECullMode_None,
	ECullMode_Front

} ECullMode;

typedef enum ERasterizerFlags {

	ERasterizerFlags_IsClockWise			= 1 << 0,		//Winding order
	ERasterizerFlags_IsWireframeExt			= 1 << 1,		//Fill mode (only available with wireframe extension)
	ERasterizerFlags_EnableDepthClamp		= 1 << 2,
	ERasterizerFlags_EnableDepthBias		= 1 << 3

} ERasterizerFlags;

typedef enum ECompareOp {

	ECompareOp_Gt,
	ECompareOp_Geq,
	ECompareOp_Eq,
	ECompareOp_Neq,
	ECompareOp_Leq,
	ECompareOp_Lt,
	ECompareOp_Always,
	ECompareOp_Never

} ECompareOp;

typedef enum EStencilOp {

	EStencilOp_Keep,
	EStencilOp_Zero,
	EStencilOp_Replace,
	EStencilOp_IncClamp,
	EStencilOp_DecClamp,
	EStencilOp_Invert,
	EStencilOp_IncWrap,
	EStencilOp_DecWrap

} EStencilOp;

typedef enum EDepthStencilFlags {

	EDepthStencilFlags_DepthTest		= 1 << 0,
	EDepthStencilFlags_DepthWriteBit	= 1 << 1,		//Use DepthWrite instead.
	EDepthStencilFlags_StencilTest		= 1 << 2,

	EDepthStencilFlags_DepthWrite		= EDepthStencilFlags_DepthTest | EDepthStencilFlags_DepthWriteBit

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
	ELogicOpExt_OrInvert

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
	EBlend_InvSrc1AlphaExt

} EBlend;

typedef enum EBlendOp {

	EBlendOp_Add,
	EBlendOp_Subtract,
	EBlendOp_ReverseSubtract,
	EBlendOp_Min,
	EBlendOp_Max

} EBlendOp;

typedef enum EWriteMask {

	EWriteMask_R	= 1 << 0,
	EWriteMask_G	= 1 << 1,
	EWriteMask_B	= 1 << 2,
	EWriteMask_A	= 1 << 3,

	EWriteMask_All	= 0xF,
	EWriteMask_RGBA	= 0xF,
	EWriteMask_RGB	= 0x7,
	EWriteMask_RG	= 0x3

} EWriteMask;

typedef enum EMSAASamples {

	EMSAASamples_Off,		//Turn off MSAA ("x1")
	EMSAASamples_x2Ext,		//Query MSAA2x data types from device
	EMSAASamples_x4,		//4x Always supported
	EMSAASamples_x8Ext,		//Query MSAA8x data types from device
	EMSAASamples_x16Ext		//Query MSAA16x data types from device

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
	ETopologyMode_LineStripAdj

} ETopologyMode;

typedef struct PipelineStage {

	EPipelineStage stageType;
	U32 padding;

	Buffer shaderBinary;

} PipelineStage;

typedef struct Rasterizer {

	U16 cullMode;				//ECullMode
	U16 flags;					//ERasterizerFlags
	F32 depthBiasClamp;
	F32 depthBiasConstantFactor;
	F32 depthBiasSlopeFactor;

} Rasterizer;

typedef struct DepthStencil {

	EDepthStencilFlags flags;
	ECompareOp depthCompare;
	ECompareOp stencilCompare;
	EStencilOp stencilFail;

	EStencilOp stencilPass;
	EStencilOp stencilDepthFail;
	U8 stencilWriteMask, stencilReadMask, padding0[2];
	U32 padding1;

} DepthStencil;

typedef struct BlendStateAttachment {

	U8 srcBlend, dstBlend, srcBlendAlpha, dstBlendAlpha;		//EBlend

	U8 blendOp, blendOpAlpha;									//EBlendOp

} BlendStateAttachment;

typedef struct BlendState {

	Bool enable;
	Bool allowIndependentBlend;
	U8 renderTargetMask, padding;

	ELogicOpExt logicOpExt;

	U8 writeMask[8];						//EWriteMask

	BlendStateAttachment attachments[8];

} BlendState;

typedef struct VertexAttribute {

	U16 offset11;						//11-bit offset
	U8 bufferId4;						//4-bit buffer id
	U8 format;							//ETextureFormatId (must be no compression!)

} VertexAttribute;

typedef struct VertexBindingLayout {

	U16 bufferStrides12_isInstance1[16];	//<=2048
	VertexAttribute attributes[16];

} VertexBindingLayout;

typedef enum EDepthStencilFormat {

	EDepthStencilFormat_None,
	EDepthStencilFormat_D32,		//Prefer this if stencil isn't needed. Less memory on AMD than D24S8!
	EDepthStencilFormat_D24S8,		//On AMD this and D32S8 are the same thing. Prefer D32S8 for better depth precision.
	EDepthStencilFormat_D32S8

} EDepthStencilFormat;