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
#include <dxgi1_6.h>

#include "types/list.h"
#include "types/string.h"
#include "graphics/generic/device_buffer.h"
#include "d3d12.h"

#ifndef GRAPHICS_API_DYNAMIC
	#define DX_WRAP_FUNC(name) name##Ext
#else
	#define DX_WRAP_FUNC(name) D3D12##name
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct DxUnifiedTexture {
	ID3D12Resource *image;
	D3D12_BARRIER_SYNC lastSync;
	D3D12_BARRIER_ACCESS lastAccess;
	D3D12_BARRIER_LAYOUT lastLayout;
} DxUnifiedTexture;

//Graphics instance doesn't really exist for DirectX12.
//It's only used for defining SDK version and that we're using DirectX12.
//The closest thing we can map is a DXGI factory that's used to query adapters

typedef enum EDxGraphicsInstanceFlags {
	EDxGraphicsInstanceFlags_HasNVApi		= 1 << 0,
	EDxGraphicsInstanceFlags_HasAMDAgs		= 1 << 1
} EDxGraphicsInstanceFlags;

typedef struct AGSContext AGSContext;

typedef struct DxGraphicsInstance {

	IDXGIFactory6 *factory;

	ID3D12SDKConfiguration1 *config;

	ID3D12Debug1 *debug1;

	ID3D12DeviceFactory *deviceFactory;

	AGSContext *agsContext;

	CharString nvDriverVersion;
	CharString amdDriverVersion;

	U32 flags, padding;

} DxGraphicsInstance;

typedef enum EDxBlockFlags {
	EDxBlockFlags_None				= 0,
	EDxBlockFlags_IsDedicated		= 1 << 0
} EDxBlockFlags;

typedef enum EDescriptorTypeOffsets {

	EDescriptorTypeOffsets_Texture2D		= 0,
	EDescriptorTypeOffsets_TextureCube		= EDescriptorTypeOffsets_Texture2D + EDescriptorTypeCount_Texture2D,
	EDescriptorTypeOffsets_Texture3D		= EDescriptorTypeOffsets_TextureCube + EDescriptorTypeCount_TextureCube,
	EDescriptorTypeOffsets_Buffer			= EDescriptorTypeOffsets_Texture3D + EDescriptorTypeCount_Texture3D,
	EDescriptorTypeOffsets_TLASExt			= EDescriptorTypeOffsets_Buffer + EDescriptorTypeCount_Buffer,

	EDescriptorTypeOffsets_RWBuffer			= EDescriptorTypeOffsets_TLASExt + EDescriptorTypeCount_TLASExt,
	EDescriptorTypeOffsets_RWTexture3D		= EDescriptorTypeOffsets_RWBuffer + EDescriptorTypeCount_RWBuffer,
	EDescriptorTypeOffsets_RWTexture3Ds		= EDescriptorTypeOffsets_RWTexture3D + EDescriptorTypeCount_RWTexture3D,
	EDescriptorTypeOffsets_RWTexture3Df		= EDescriptorTypeOffsets_RWTexture3Ds + EDescriptorTypeCount_RWTexture3Ds,
	EDescriptorTypeOffsets_RWTexture3Di		= EDescriptorTypeOffsets_RWTexture3Df + EDescriptorTypeCount_RWTexture3Df,
	EDescriptorTypeOffsets_RWTexture3Du		= EDescriptorTypeOffsets_RWTexture3Di + EDescriptorTypeCount_RWTexture3Di,
	EDescriptorTypeOffsets_RWTexture2D		= EDescriptorTypeOffsets_RWTexture3Du + EDescriptorTypeCount_RWTexture3Du,
	EDescriptorTypeOffsets_RWTexture2Ds		= EDescriptorTypeOffsets_RWTexture2D + EDescriptorTypeCount_RWTexture2D,
	EDescriptorTypeOffsets_RWTexture2Df		= EDescriptorTypeOffsets_RWTexture2Ds + EDescriptorTypeCount_RWTexture2Ds,
	EDescriptorTypeOffsets_RWTexture2Di		= EDescriptorTypeOffsets_RWTexture2Df + EDescriptorTypeCount_RWTexture2Df,
	EDescriptorTypeOffsets_RWTexture2Du		= EDescriptorTypeOffsets_RWTexture2Di + EDescriptorTypeCount_RWTexture2Di,

	//Add one to the resource count to add an extension slot for NV
	EDescriptorTypeOffsets_ResourceCount	= EDescriptorTypeOffsets_RWTexture2Du + EDescriptorTypeCount_RWTexture2Du + 1,

	EDescriptorTypeOffsets_Sampler			= 0,
	EDescriptorTypeOffsets_SamplerCount		= EDescriptorTypeCount_Sampler,

	EDescriptorTypeOffsets_RTVCount			= 8,								//No more than 8 RTVs can be active at a time
	EDescriptorTypeOffsets_DSVCount			= 1,								//No more than 1 DSV can be active at a time

	EDescriptorTypeOffsets_SRVStart			= EDescriptorTypeOffsets_Texture2D,
	EDescriptorTypeOffsets_SRVEnd			= EDescriptorTypeOffsets_RWBuffer,

	EDescriptorTypeOffsets_UAVStart			= EDescriptorTypeOffsets_RWBuffer,
	EDescriptorTypeOffsets_UAVEnd			= EDescriptorTypeOffsets_ResourceCount,

} EDescriptorTypeOffsets;

EDescriptorTypeOffsets EDescriptorTypeOffsets_values[EDescriptorType_ResourceCount];

typedef struct DxBlockRequirements {

	EDxBlockFlags flags;
	U32 alignment;

	U64 length;

} DxBlockRequirements;

typedef enum ECompareOp ECompareOp;

typedef union DxPipeline {

	struct {
		ID3D12StateObject *stateObject;					//For anything else (RTPSO, workgraphs, etc.)
		ID3D12StateObjectProperties *stateObjectProps;
	};

	ID3D12PipelineState *pso;							//For graphics & compute shaders

} DxPipeline;

typedef struct DxBLAS {
	D3D12_RAYTRACING_GEOMETRY_DESC geometry;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
	U32 padding, primitives;
} DxBLAS;

typedef struct DxTLAS {
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
} DxTLAS;

static const U32 raytracingShaderIdSize = 32;
static const U32 raytracingShaderAlignment = 64;

TList(D3D12_TEXTURE_BARRIER);
TList(D3D12_BUFFER_BARRIER);
TList(ID3D12PipelineState);

Error dxCheck(HRESULT result);

D3D12_COMPARISON_FUNC mapDxCompareOp(ECompareOp op);

D3D12_GPU_VIRTUAL_ADDRESS getDxDeviceAddress(DeviceData data);
D3D12_GPU_VIRTUAL_ADDRESS getDxLocation(DeviceData data, U64 localOffset);

//Transitions entire resource rather than sub-resources

Error DxUnifiedTexture_transition(
	DxUnifiedTexture *image,
	D3D12_BARRIER_SYNC sync,
	D3D12_BARRIER_ACCESS access,
	D3D12_BARRIER_LAYOUT layout,
	const D3D12_BARRIER_SUBRESOURCE_RANGE *range,
	ListD3D12_TEXTURE_BARRIER *imageBarriers,
	D3D12_BARRIER_GROUP *dependency
);

#ifdef __cplusplus
	}
#endif
