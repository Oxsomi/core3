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
#include "platforms/ref_ptr.h"

typedef RefPtr GraphicsDeviceRef;
typedef enum ETextureFormat ETextureFormat;

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
	ERasterizerFlags_enableDepthBias		= 1 << 3,
	ERasterizerFlags_enableDepthBiasClamp	= 1 << 4

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

typedef enum EBlendStateFlags {

	EBlendStateFlags_allowIndependentBlend	= 1 << 0,
	EBlendStateFlags_logicOpEnableExt		= 1 << 1		//Requires logicOp graphics feature

} EBlendStateFlags;

typedef enum ELogicOpExt {

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

	ETopologyMode_PointList,

	ETopologyMode_LineList,
	ETopologyMode_LineStrip,
	ETopologyMode_TriangleList,
	ETopologyMode_TriangleStrip,

	ETopologyMode_LineListAdj,
	ETopologyMode_LineStripAdj,
	ETopologyMode_TriangleListAdj,
	ETopologyMode_TriangleStripAdj

} ETopologyMode;

typedef struct PipelineStage {
	
	EPipelineStage stageType;
	Buffer shaderBinary;

} PipelineStage;

typedef struct Rasterizer {

	ECullMode cullMode;
	ERasterizerFlags flags;
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

	EBlendStateFlags flags;
	ELogicOpExt logicOpExt;
	U8 renderTargetsCount, renderTargetMask, padding[2];

	BlendStateAttachment attachments[8];

} BlendState;

typedef struct GraphicsBufferAttribute {

	C8 semanticName[12];

	U16 offset12_bufferId4;				//12-bit offset, 4-bit bufferId
	U8 bindingId4_isInstanceData1;		//4-bit binding id, Bool isInstanceData
	U8 format;							//ETextureFormatId

} GraphicsBufferAttribute;

typedef struct GraphicsBufferLayout {

	U16 bufferStrides[16];
	GraphicsBufferAttribute attributes[16];

} GraphicsBufferLayout;

typedef enum EDepthStencilFormat {

	EDepthStencilFormat_none,
	EDepthStencilFormat_D16,
	EDepthStencilFormat_D32,
	EDepthStencilFormat_D24S8,
	EDepthStencilFormat_D32S8

} EDepthStencilFormat;

typedef struct PipelineGraphicsInfo {

	GraphicsBufferLayout vertexLayout;			//Can be empty if pipeline generates all vertices itself

	Rasterizer rasterizer;

	DepthStencil depthStencil;
	BlendState blendState;

	EMSAASamples msaa;
	ETopologyMode topologyMode;

	U32 patchControlPointsExt;		//Only if TessellationShader feature is enabled.
	U32 stageCount;					//Non zero used to determine where stages start/end.

	//If DirectRendering is on (used in between start render).

	ETextureFormat attachmentFormats[8];

	EDepthStencilFormat depthFormat;
	U32 attachmentCount;

	//If DirectRendering is off (used in between start render pass).

	RefPtr *renderPass;				//Required only if DirectRendering is not on.

	U32 subPass;					//^
	U32 padding2;

} PipelineGraphicsInfo;

typedef struct Pipeline {

	GraphicsDeviceRef *device;

	EPipelineType type;
	U32 padding;

	List stages;					//<PipelineStage>

	const void *extraInfo;			//Null or points to after ext data for extraInfo (e.g. PipelineGraphicsInfo)

} Pipeline;

typedef RefPtr PipelineRef;

#define Pipeline_ext(ptr, T) (!ptr ? NULL : (T##Pipeline*)(ptr + 1))		//impl
#define PipelineRef_ptr(ptr) RefPtr_data(ptr, Pipeline)

Error PipelineRef_dec(PipelineRef **pipeline);
Error PipelineRef_add(PipelineRef *pipeline);

Error PipelineRef_decAll(List *list);					//Decrements all refs and frees list

PipelineRef *PipelineRef_at(List list, U64 index);

//List<Buffer> *shaderBinaries, List<PipelineRef*> *pipelines
//shaderBinaries will be moved (shaderBinaries will be cleared if moved)
impl Error GraphicsDeviceRef_createPipelinesCompute(GraphicsDeviceRef *deviceRef, List *shaderBinaries, List *pipelines);

//List<PipelineStage> stages, List<PipelineGraphicsInfo> infos, List<PipelineRef*> *pipelines
impl Error GraphicsDeviceRef_createPipelinesGraphics(GraphicsDeviceRef *deviceRef, List stages, List info, List *pipelines);
