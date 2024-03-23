/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "graphics/generic/device_buffer.h"
#include "d3d12.h"

typedef struct DxUnifiedTexture {
	ID3D12Resource *image;
	D3D12_BARRIER_SYNC lastSync;
	D3D12_BARRIER_ACCESS lastAccess;
	D3D12_BARRIER_LAYOUT lastLayout;
} DxUnifiedTexture;

typedef enum ECompareOp ECompareOp;

typedef struct DxBLAS {
	U8 padding;				//The concept of a BLAS resource doesn't exist in DX12; it's just a buffer.
} DxBLAS;

typedef struct DxTLAS {
	U8 padding;				//The concept of a TLAS resource doesn't exist in DX12; it's just a buffer.
} DxTLAS;

static const U32 raytracingShaderIdSize = 32;
static const U32 raytracingShaderAlignment = 64;

TList(D3D12_TEXTURE_BARRIER);
TList(D3D12_BUFFER_BARRIER);
TList(ID3D12PipelineState);

Error dxCheck(HRESULT result);

//Pass types as non-NULL to allow validating if the texture format is supported.
//Sometimes you don't want this, for example when serializing.
DXFormat mapDxFormat(ETextureFormat format);

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
