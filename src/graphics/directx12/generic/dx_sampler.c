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

#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "graphics/directx12/dx_device.h"
#include "types/string.h"

const U64 SamplerExt_size = 1;		//No real info needed, it's just a descriptor :)

Bool Sampler_freeExt(Sampler *sampler) { (void)sampler; return true; }

D3D12_TEXTURE_ADDRESS_MODE mapDxAddressMode(ESamplerAddressMode addressMode) {
	switch (addressMode) {
		case ESamplerAddressMode_MirrorRepeat:		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case ESamplerAddressMode_ClampToEdge:		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case ESamplerAddressMode_ClampToBorder:		return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		default:									return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

Error GraphicsDeviceRef_createSamplerExt(GraphicsDeviceRef *dev, Sampler *sampler, CharString name) {

	(void)name;

	const GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	const DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	const SamplerInfo sinfo = sampler->info;

	//Allocate descriptor

	D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC;

	if(!sinfo.aniso)
		switch(sinfo.filter) {
			case ESamplerFilterMode_Nearest:					filter = D3D12_FILTER_MIN_MAG_MIP_POINT;				break;
			case ESamplerFilterMode_LinearMagNearestMinMip:		filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;	break;
			case ESamplerFilterMode_LinearMinNearestMagMip:		filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;			break;
			case ESamplerFilterMode_LinearMagMinNearestMip:		filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;			break;
			case ESamplerFilterMode_LinearMipNearestMagMin:		filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;			break;
			case ESamplerFilterMode_LinearMagMipNearestMin:		filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;			break;
			case ESamplerFilterMode_LinearMinMipNearestMag:		filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;	break;
			case ESamplerFilterMode_Linear:						filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;				break;
		}

	D3D12_SAMPLER_DESC samplerView = (D3D12_SAMPLER_DESC) {
		.Filter = filter,
		.AddressU = mapDxAddressMode(sinfo.addressU),
		.AddressV = mapDxAddressMode(sinfo.addressV),
		.AddressW = mapDxAddressMode(sinfo.addressW),
		.MipLODBias = F16_castF32(sinfo.mipBias),
		.MaxAnisotropy = sinfo.aniso,
		.MinLOD = F16_castF32(sinfo.minLod),
		.MinLOD = F16_castF32(sinfo.maxLod)
	};

	const U32 one = 1;

	switch(sinfo.borderColor) {

		case ESamplerBorderColor_OpaqueBlackFloat:
			samplerView.BorderColor[3] = 1.f;
			break;

		case ESamplerBorderColor_OpaqueWhiteFloat:
			samplerView.BorderColor[0] = samplerView.BorderColor[1] = samplerView.BorderColor[2] = 1.f;
			samplerView.BorderColor[3] = 1.f;
			break;

		case ESamplerBorderColor_OpaqueWhiteInt:
			samplerView.BorderColor[0] = samplerView.BorderColor[1] = samplerView.BorderColor[2] = *(const F32*)&one;
			samplerView.BorderColor[3] = *(const F32*)&one;
			break;

		case ESamplerBorderColor_OpaqueBlackInt:
			samplerView.BorderColor[3] = *(const F32*)&one;
			break;

		default:
			break;		//Already null initialized
	}

	if(sinfo.enableComparison) {
		samplerView.ComparisonFunc = mapDxCompareOp(sinfo.comparisonFunction);
		samplerView.Filter |= 0x80;		//Signal we want comparison sampler
	}

	const DxHeap heap = deviceExt->heaps[EDescriptorHeapType_Sampler];
	const U64 id = ResourceHandle_getId(sampler->samplerLocation);

	deviceExt->device->lpVtbl->CreateSampler(
		deviceExt->device,
		&samplerView,
		(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap.cpuHandle.ptr + heap.cpuIncrement * id }
	);

	return Error_none();
}
