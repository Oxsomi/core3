/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "graphics/generic/interface.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/device.h"
#include "graphics/generic/texture.h"
#include "graphics/d3d12/dx_device.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "types/container/texture_format.h"
#include "formats/oiSH/sh_file.h"
#include "types/container/string.h"
#include "types/base/error.h"

D3D12_STENCIL_OP mapDxStencilOp(EStencilOp op) {
	switch (op) {
		default:								return D3D12_STENCIL_OP_KEEP;
		case EStencilOp_Zero:					return D3D12_STENCIL_OP_ZERO;
		case EStencilOp_Replace:				return D3D12_STENCIL_OP_REPLACE;
		case EStencilOp_IncClamp:				return D3D12_STENCIL_OP_INCR_SAT;
		case EStencilOp_DecClamp:				return D3D12_STENCIL_OP_DECR_SAT;
		case EStencilOp_Invert:					return D3D12_STENCIL_OP_INVERT;
		case EStencilOp_IncWrap:				return D3D12_STENCIL_OP_INCR;
		case EStencilOp_DecWrap:				return D3D12_STENCIL_OP_DECR;
	}
}

D3D12_BLEND_OP mapDxBlendOp(EBlendOp op) {
	switch (op) {
		default:								return D3D12_BLEND_OP_ADD;
		case EBlendOp_Subtract:					return D3D12_BLEND_OP_SUBTRACT;
		case EBlendOp_ReverseSubtract:			return D3D12_BLEND_OP_REV_SUBTRACT;
		case EBlendOp_Min:						return D3D12_BLEND_OP_MIN;
		case EBlendOp_Max:						return D3D12_BLEND_OP_MAX;
	}
}

D3D12_BLEND mapDxBlend(EBlend op) {

	switch (op) {

		default:								return D3D12_BLEND_ZERO;
		case EBlend_One:						return D3D12_BLEND_ONE;
		case EBlend_SrcColor:					return D3D12_BLEND_SRC_COLOR;
		case EBlend_InvSrcColor:				return D3D12_BLEND_INV_SRC_COLOR;
		case EBlend_DstColor:					return D3D12_BLEND_DEST_COLOR;
		case EBlend_InvDstColor:				return D3D12_BLEND_INV_DEST_COLOR;
		case EBlend_SrcAlpha:					return D3D12_BLEND_SRC_ALPHA;
		case EBlend_InvSrcAlpha:				return D3D12_BLEND_INV_SRC_ALPHA;
		case EBlend_DstAlpha:					return D3D12_BLEND_DEST_ALPHA;
		case EBlend_InvDstAlpha:				return D3D12_BLEND_INV_DEST_ALPHA;
		case EBlend_BlendFactor:				return D3D12_BLEND_BLEND_FACTOR;
		case EBlend_InvBlendFactor:				return D3D12_BLEND_INV_BLEND_FACTOR;
		case EBlend_AlphaFactor:				return D3D12_BLEND_ALPHA_FACTOR;
		case EBlend_InvAlphaFactor:				return D3D12_BLEND_INV_ALPHA_FACTOR;
		case EBlend_SrcAlphaSat:				return D3D12_BLEND_SRC_ALPHA_SAT;

		case EBlend_Src1ColorExt:				return D3D12_BLEND_SRC1_COLOR;
		case EBlend_Src1AlphaExt:				return D3D12_BLEND_SRC1_ALPHA;
		case EBlend_InvSrc1ColorExt:			return D3D12_BLEND_INV_SRC1_COLOR;
		case EBlend_InvSrc1AlphaExt:			return D3D12_BLEND_INV_SRC1_ALPHA;
	}
}

Bool DX_WRAP_FUNC(GraphicsDevice_createPipelineGraphics)(
	GraphicsDevice *device,
	ListSHFile binaries,
	CharString name,
	Pipeline *pipeline,
	Error *e_rr
) {

	Bool s_uccess = true;

	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	ListU16 tmp = (ListU16) { 0 };

	//TODO: Push constants

	D3D12_INPUT_ELEMENT_DESC elements[8];

	const PipelineGraphicsInfo *info = Pipeline_info(pipeline, PipelineGraphicsInfo);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics = (D3D12_GRAPHICS_PIPELINE_STATE_DESC) {

		.pRootSignature = deviceExt->defaultLayout,

		.BlendState = (D3D12_BLEND_DESC) { .IndependentBlendEnable = info->blendState.allowIndependentBlend },

		.RasterizerState = (D3D12_RASTERIZER_DESC) {
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = !(info->rasterizer.flags & ERasterizerFlags_IsClockWise),
			.MultisampleEnable = !!info->msaa
		},

		.DepthStencilState = (D3D12_DEPTH_STENCIL_DESC) {
			.DepthEnable = !!(info->depthStencil.flags & EDepthStencilFlags_DepthTest),
			.DepthWriteMask = !!(info->depthStencil.flags & EDepthStencilFlags_DepthWrite),
			.DepthFunc = mapDxCompareOp(info->depthStencil.depthCompare),
			.StencilEnable = !!(info->depthStencil.flags & EDepthStencilFlags_StencilTest),
			.StencilReadMask = info->depthStencil.stencilReadMask,
			.StencilReadMask = info->depthStencil.stencilWriteMask
		},

		.InputLayout = (D3D12_INPUT_LAYOUT_DESC ) { .pInputElementDescs = elements },

		.SampleMask = U32_MAX,

		.NumRenderTargets = info->attachmentCountExt,
		.DSVFormat = EDepthStencilFormat_toDXFormat(info->depthFormatExt),

		.SampleDesc = (DXGI_SAMPLE_DESC) { .Count = 1 }
	};

	//Rasterizer

	if(info->rasterizer.flags & ERasterizerFlags_IsWireframeExt)
		graphics.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

	if(info->rasterizer.flags & ERasterizerFlags_EnableDepthBias) {
		graphics.RasterizerState.DepthBias = info->rasterizer.depthBiasConstantFactor;
		graphics.RasterizerState.SlopeScaledDepthBias = info->rasterizer.depthBiasSlopeFactor;
	}

	if(info->rasterizer.flags & ERasterizerFlags_EnableDepthClamp)
		graphics.RasterizerState.DepthBiasClamp = info->rasterizer.depthBiasClamp;

	switch(info->rasterizer.cullMode) {
		default:																			break;
		case ECullMode_None:	graphics.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;	break;
		case ECullMode_Front:	graphics.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;	break;
	}

	//Depth stencil

	graphics.DepthStencilState.BackFace = (D3D12_DEPTH_STENCILOP_DESC) {
		.StencilFailOp = mapDxStencilOp(info->depthStencil.stencilFail),
		.StencilDepthFailOp = mapDxStencilOp(info->depthStencil.stencilDepthFail),
		.StencilPassOp = mapDxStencilOp(info->depthStencil.stencilPass),
		.StencilFunc = mapDxCompareOp(info->depthStencil.stencilCompare)
	};

	graphics.DepthStencilState.FrontFace = graphics.DepthStencilState.BackFace;

	//Blend state

	for(U8 j = 0; j < 8; ++j) {

		U8 realJ =  info->blendState.allowIndependentBlend ? j : 0;
		D3D12_RENDER_TARGET_BLEND_DESC *dst = &graphics.BlendState.RenderTarget[j];
		BlendStateAttachment src = info->blendState.attachments[realJ];

		if(!info->blendState.enable) {
			*dst = (D3D12_RENDER_TARGET_BLEND_DESC) { .RenderTargetWriteMask = EWriteMask_All };
			continue;
		}

		*dst = (D3D12_RENDER_TARGET_BLEND_DESC) {

			.BlendEnable			= (info->blendState.renderTargetMask >> realJ) & 1,
			.LogicOpEnable			= !!info->blendState.logicOpExt,
			.RenderTargetWriteMask	= info->blendState.writeMask[realJ],

			.BlendOp				= mapDxBlendOp(src.blendOp),
			.SrcBlend				= mapDxBlend(src.srcBlend),
			.DestBlend				= mapDxBlend(src.dstBlend),

			.BlendOpAlpha			= mapDxBlendOp(src.blendOpAlpha),
			.SrcBlendAlpha			= mapDxBlend(src.srcBlendAlpha),
			.DestBlendAlpha			= mapDxBlend(src.dstBlendAlpha)
		};

		switch(info->blendState.logicOpExt) {
			case ELogicOpExt_Clear:			dst->LogicOp = D3D12_LOGIC_OP_CLEAR;			break;
			case ELogicOpExt_Set:			dst->LogicOp = D3D12_LOGIC_OP_SET;				break;
			case ELogicOpExt_Copy:			dst->LogicOp = D3D12_LOGIC_OP_COPY;				break;
			case ELogicOpExt_CopyInvert:	dst->LogicOp = D3D12_LOGIC_OP_COPY_INVERTED;	break;
			case ELogicOpExt_None:			dst->LogicOp = D3D12_LOGIC_OP_NOOP;				break;
			case ELogicOpExt_Invert:		dst->LogicOp = D3D12_LOGIC_OP_INVERT;			break;
			case ELogicOpExt_And:			dst->LogicOp = D3D12_LOGIC_OP_AND;				break;
			case ELogicOpExt_Nand:			dst->LogicOp = D3D12_LOGIC_OP_NAND;				break;
			case ELogicOpExt_Or:			dst->LogicOp = D3D12_LOGIC_OP_OR;				break;
			case ELogicOpExt_Nor:			dst->LogicOp = D3D12_LOGIC_OP_NOR;				break;
			case ELogicOpExt_Xor:			dst->LogicOp = D3D12_LOGIC_OP_XOR;				break;
			case ELogicOpExt_Equiv:			dst->LogicOp = D3D12_LOGIC_OP_EQUIV;			break;
			case ELogicOpExt_AndReverse:	dst->LogicOp = D3D12_LOGIC_OP_AND_REVERSE;		break;
			case ELogicOpExt_AndInvert:		dst->LogicOp = D3D12_LOGIC_OP_AND_INVERTED;		break;
			case ELogicOpExt_OrReverse:		dst->LogicOp = D3D12_LOGIC_OP_OR_REVERSE;		break;
			case ELogicOpExt_OrInvert:		dst->LogicOp = D3D12_LOGIC_OP_OR_INVERTED;		break;
			default:																		break;
		}
	}

	//Init vertex + instance buffers

	for(U8 j = 0; j < 16; ++j) {

		VertexAttribute attrib = info->vertexLayout.attributes[j];

		if(!attrib.format)
			continue;

		U32 k = graphics.InputLayout.NumElements++;
		D3D12_INPUT_ELEMENT_DESC *desc = (D3D12_INPUT_ELEMENT_DESC*) &graphics.InputLayout.pInputElementDescs[k];
		*desc = (D3D12_INPUT_ELEMENT_DESC) {
			.SemanticName = "TEXCOORD",		//Everything is a texcoord		TODO:
			.SemanticIndex = j,
			.Format = ETextureFormatId_toDXFormat(attrib.format),
			.InputSlot = attrib.bufferId4 & 0xF,
			.AlignedByteOffset = attrib.offset11 & 2047
		};

		if(info->vertexLayout.bufferStrides12_isInstance1[desc->InputSlot] & (1 << 12)) {
			desc->InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
			desc->InstanceDataStepRate = 1;
		}
	}

	//Init topology

	switch(info->topologyMode) {

		case ETopologyMode_TriangleListAdj:
		case ETopologyMode_TriangleStripAdj:
		case ETopologyMode_TriangleStrip:
		default:
			graphics.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			break;

		case ETopologyMode_LineListAdj:
		case ETopologyMode_LineStripAdj:
		case ETopologyMode_LineStrip:
		case ETopologyMode_LineList:
			graphics.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			break;

		case ETopologyMode_PointList:
			graphics.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			break;
	}

	if(info->patchControlPoints)
		graphics.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;

	//Init render targets

	for(U64 j = 0; j < info->attachmentCountExt; ++j)
		graphics.RTVFormats[j] = ETextureFormatId_toDXFormat(info->attachmentFormatsExt[j]);

	//Init pipeline stages

	for(U64 j = 0; j < pipeline->stages.length; ++j) {

		PipelineStage stage = pipeline->stages.ptr[j];

		U16 entrypointId = (U16) stage.binaryId;
		U16 binaryId = (U16) (stage.binaryId >> 16);

		SHFile binary = binaries.ptr[stage.shFileId];
		SHBinaryInfo binj = binary.binaries.ptr[binary.entries.ptr[entrypointId].binaryIds.ptr[binaryId]];
		Buffer bin = binj.binaries[ESHBinaryType_DXIL];

		D3D12_SHADER_BYTECODE bytecode = (D3D12_SHADER_BYTECODE) {
			.pShaderBytecode = bin.ptr,
			.BytecodeLength = Buffer_length(bin)
		};

		switch(stage.stageType) {
			default:							graphics.PS = bytecode;		break;
			case EPipelineStage_Vertex:			graphics.VS = bytecode;		break;
			case EPipelineStage_Domain:			graphics.DS = bytecode;		break;
			case EPipelineStage_Hull:			graphics.HS = bytecode;		break;
			case EPipelineStage_GeometryExt:	graphics.GS = bytecode;		break;
		}
	}

	//Create

	ID3D12PipelineState **pipelinei = &Pipeline_ext(pipeline, Dx)->pso;

	gotoIfError2(clean, dxCheck(deviceExt->device->lpVtbl->CreateGraphicsPipelineState(
		deviceExt->device,
		&graphics,
		&IID_ID3D12PipelineState,
		(void**) pipelinei
	)))

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name)) {
		gotoIfError2(clean, CharString_toUTF16x(name, &tmp))
		gotoIfError2(clean, dxCheck((*pipelinei)->lpVtbl->SetName(*pipelinei, (const wchar_t*) tmp.ptr)))
	}

clean:
	ListU16_freex(&tmp);
	return s_uccess;
}
