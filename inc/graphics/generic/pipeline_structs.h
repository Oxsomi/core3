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
	EPipelineType_Compute

} EPipelineType;

typedef enum EPipelineStage {

	EPipelineStage_Vertex,
	EPipelineStage_Pixel,
	EPipelineStage_Compute,
	EPipelineStage_GeometryExt,			//Query graphics feature GeometryShader
	EPipelineStage_HullExt,				//Query graphics feature TessellationShader
	EPipelineStage_DomainExt			//^

} EPipelineStage;

typedef enum ECullMode {

	ECullMode_none,
	ECullMode_back,
	ECullMode_front

} ECullMode;

typedef enum ERasterizerFlags {

	ERasterizerFlags_isClockWise			= 1 << 0,		//Winding order
	ERasterizerFlags_isWireframeExt			= 1 << 1,		//Fill mode (only available with wireframe extension)
	ERasterizerFlags_enableDepthClamp		= 1 << 2,
	ERasterizerFlags_enableDepthBias		= 1 << 3

} ERasterizerFlags;

typedef enum ECompareOp {

	ECompareOp_gt,
	ECompareOp_geq,
	ECompareOp_eq,
	ECompareOp_neq,
	ECompareOp_leq,
	ECompareOp_lt,
	ECompareOp_always,
	ECompareOp_never

} ECompareOp;

typedef enum EStencilOp {

	EStencilOp_keep,
	EStencilOp_zero,
	EStencilOp_replace,
	EStencilOp_incClamp,
	EStencilOp_decClamp,
	EStencilOp_invert,
	EStencilOp_incWrap,
	EStencilOp_decWrap

} EStencilOp;

typedef enum EDepthStencilFlags {

	EDepthStencilFlags_depthTest		= 1 << 0,
	EDepthStencilFlags_depthWriteBit	= 1 << 1,		//Use depthWrite instead.
	EDepthStencilFlags_stencilTest		= 1 << 2,

	EDepthStencilFlags_depthWrite		= EDepthStencilFlags_depthTest | EDepthStencilFlags_depthWriteBit

} EDepthStencilFlags;

typedef enum ELogicOpExt {

	ELogicOpExt_off,
	ELogicOpExt_clear,
	ELogicOpExt_set,
	ELogicOpExt_copy,
	ELogicOpExt_copyInvert,
	ELogicOpExt_none,
	ELogicOpExt_invert,
	ELogicOpExt_and,
	ELogicOpExt_nand,
	ELogicOpExt_or,
	ELogicOpExt_nor,
	ELogicOpExt_xor,
	ELogicOpExt_equiv,
	ELogicOpExt_andReverse,
	ELogicOpExt_andInvert,
	ELogicOpExt_orReverse,
	ELogicOpExt_orInvert

} ELogicOpExt;

typedef enum EBlend {

	EBlend_zero,
	EBlend_one,

	EBlend_srcColor,
	EBlend_invSrcColor,
	EBlend_dstColor,
	EBlend_invDstColor,

	EBlend_srcAlpha,
	EBlend_invSrcAlpha,
	EBlend_dstAlpha,
	EBlend_invDstAlpha,

	EBlend_blendFactor,
	EBlend_invBlendFactor,
	EBlend_alphaFactor,
	EBlend_invAlphaFactor,
	EBlend_srcAlphaSat,

	//Check dualSrcBlend feature

	EBlend_src1ColorExt,
	EBlend_src1AlphaExt,
	EBlend_invSrc1ColorExt,
	EBlend_invSrc1AlphaExt

} EBlend;

typedef enum EBlendOp {

	EBlendOp_add,
	EBlendOp_subtract,
	EBlendOp_reverseSubtract,
	EBlendOp_min,
	EBlendOp_max

} EBlendOp;

typedef enum EWriteMask {

	EWriteMask_r	= 1 << 0,
	EWriteMask_g	= 1 << 1,
	EWriteMask_b	= 1 << 2,
	EWriteMask_a	= 1 << 3,

	EWriteMask_all	= 0xF,
	EWriteMask_rgba	= 0xF,
	EWriteMask_rgb	= 0x7

} EWriteMask;

typedef enum EMSAASamples {

	EMSAASamples_Off,		//Turn off MSAA
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
	U8 writeMask;												//EWriteMask
	U8 padding;

} BlendStateAttachment;

typedef struct BlendState {

	Bool allowIndependentBlend;
	ELogicOpExt logicOpExt;
	U8 renderTargetsCount, renderTargetMask, padding[2];

	BlendStateAttachment attachments[8];

} BlendState;

typedef struct GraphicsBufferAttribute {

	C8 semanticName[13];						//Not necessarily null terminated!

	U8 format;									//ETextureFormatId (must be no compression!)
	U16 offset11_bufferId4;						//11-bit offset, 4-bit bufferId

} GraphicsBufferAttribute;

typedef struct GraphicsBufferLayout {

	U16 bufferStrides12_isInstance1[16];	//<=2048
	GraphicsBufferAttribute attributes[16];

} GraphicsBufferLayout;

typedef enum EDepthStencilFormat {

	EDepthStencilFormat_none,
	EDepthStencilFormat_D16,
	EDepthStencilFormat_D32,		//Prefer this if stencil isn't needed.
	EDepthStencilFormat_D24S8,		//TODO: Validate if NV, AMD, Intel and ARM actually allocate this as D32S8, else remove.
	EDepthStencilFormat_D32S8

} EDepthStencilFormat;