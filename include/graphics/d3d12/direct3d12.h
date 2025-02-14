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

#pragma once
#include <dxgi1_6.h>

#include "types/container/list.h"
#include "types/container/string.h"
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
	U32 padding[3];
} DxUnifiedTexture;

//Graphics instance doesn't really exist for Direct3D12.
//It's only used for defining SDK version and that we're using Direct3D12.
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

typedef struct DxBlockRequirements {

	EDxBlockFlags flags;
	U32 alignment;

	U64 length;

} DxBlockRequirements;

typedef enum ECompareOp ECompareOp;

typedef union DxPipeline {

	struct {
		ID3D12StateObject *stateObject;					//For anything else (RTPSO, workgraphs, mesh shaders, etc.)
		ID3D12StateObjectProperties *stateObjectProps;
	};

	ID3D12PipelineState *pso;							//For graphics & compute shaders

} DxPipeline;

typedef struct DxBLAS {
	D3D12_RAYTRACING_GEOMETRY_DESC geometry;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
	U32 primitives, padding[3];
} DxBLAS;

typedef struct DxTLAS {
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
	U64 padding;
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

D3D12_HEAP_DESC getDxHeapDesc(GraphicsDevice *device, Bool *cpuSided, U64 alignment);

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
