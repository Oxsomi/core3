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

//graphics/d3d12/direct3d12.h

#pragma once
#include "types/container/list.h"
#include "types/container/string.h"
#include "d3d12.h"
#include <dxgi1_6.h>

#ifndef GRAPHICS_API_DYNAMIC
	#define DX_WRAP_FUNC(name) name##Ext
#else
	#define DX_WRAP_FUNC(name) D3D12##name
#endif

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct GraphicsDevice GraphicsDevice;
typedef enum EResourceType EResourceType;

static const D3D12_BARRIER_ACCESS D3D12BarrierAccess_Write =
	D3D12_BARRIER_ACCESS_RENDER_TARGET |
	D3D12_BARRIER_ACCESS_UNORDERED_ACCESS |
	D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE |
	D3D12_BARRIER_ACCESS_STREAM_OUTPUT |
	D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT |
	D3D12_BARRIER_ACCESS_COPY_DEST |
	D3D12_BARRIER_ACCESS_RESOLVE_DEST |
	D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE |
	D3D12_BARRIER_ACCESS_VIDEO_DECODE_WRITE |
	D3D12_BARRIER_ACCESS_VIDEO_PROCESS_WRITE |
	D3D12_BARRIER_ACCESS_VIDEO_ENCODE_WRITE;

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
	EDxGraphicsInstanceFlags_HasNVApi         = 1 << 0,
	EDxGraphicsInstanceFlags_HasAMDAgs        = 1 << 1
} EDxGraphicsInstanceFlags;

typedef struct AGSContext AGSContext;

typedef struct DxGraphicsInstance {

	IDXGIFactory6 *factory;

	ID3D12SDKConfiguration1 *config;

	ID3D12Debug1 *debug1NoSingleton;
	ID3D12Debug1 *debug1Singleton;

	ID3D12DeviceFactory *deviceFactoryNoSingleton;
	ID3D12DeviceFactory *deviceFactorySingleton;

	AGSContext *agsContext;

	CharString nvDriverVersion;
	CharString amdDriverVersion;

	U32 flags, padding;

} DxGraphicsInstance;

typedef enum EDxBlockFlags {
	EDxBlockFlags_None               = 0,
	EDxBlockFlags_IsDedicated        = 1 << 0,
	EDxBlockFlags_Readback           = 1 << 1        //CPU readable heap for pullRegion staging
} EDxBlockFlags;

typedef struct DxBlockRequirements {

	EDxBlockFlags flags;
	U32 alignment;

	U64 length;

} DxBlockRequirements;

typedef enum ECompareOp ECompareOp;

typedef union DxPipeline {

	struct {
		ID3D12StateObject *stateObject;                    //For anything else (RTPSO, workgraphs, mesh shaders, etc.)
		ID3D12StateObjectProperties *stateObjectProps;
	};

	ID3D12PipelineState *pso;                              //For graphics & compute shaders

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

typedef struct DeviceData DeviceData;

static const U32 raytracingShaderIdSize = 32;
static const U32 raytracingShaderAlignment = 64;

TList(D3D12_TEXTURE_BARRIER);
TList(D3D12_BUFFER_BARRIER);
TList(ID3D12PipelineState);

Bool dxCheck(HRESULT result, Error *e_rr);

D3D12_COMPARISON_FUNC mapDxCompareOp(ECompareOp op);

D3D12_GPU_VIRTUAL_ADDRESS getDxDeviceAddress(DeviceData data);
D3D12_GPU_VIRTUAL_ADDRESS getDxLocation(DeviceData data, U64 localOffset);

D3D12_HEAP_DESC getDxHeapDesc(GraphicsDevice *device, Bool *cpuSided, U64 alignment, EResourceType resourceType, Bool readback);

//Transitions entire resource rather than sub-resources

Bool DxUnifiedTexture_transition(
	DxUnifiedTexture *image,
	D3D12_BARRIER_SYNC sync,
	D3D12_BARRIER_ACCESS access,
	D3D12_BARRIER_LAYOUT layout,
	const D3D12_BARRIER_SUBRESOURCE_RANGE *range,
	ListD3D12_TEXTURE_BARRIER *imageBarriers,
	D3D12_BARRIER_GROUP *dependency,
	const Allocator *alloc,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
