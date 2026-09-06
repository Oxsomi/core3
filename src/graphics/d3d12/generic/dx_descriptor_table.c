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

//graphics/d3d12/generic/dx_descriptor_table.c

#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/d3d12/dx_buffer.h"
#include "types/container/string.h"
#include "types/container/string.h"

//Building a texture's SRV or UAV is the same work whether it lands in a descriptor table or in the push
// ring a texture push descriptor takes its single entry table from.
//Kept in one place so the two can never disagree about a dimension, a mip range or a depth plane's
// format.

void DxDescriptorTable_writeTexture(
	DxGraphicsDevice *deviceExt,
	const Descriptor *d,
	EGfxRegisterType registerType,
	D3D12_CPU_DESCRIPTOR_HANDLE dst
) {

	const Bool isWrite = (registerType & EGfxRegisterType_IsWrite) != 0;

	DXGI_FORMAT dxFormat = DXGI_FORMAT_UNKNOWN;
	UnifiedTexture tex = (UnifiedTexture) { 0 };
	DxUnifiedTexture *texExt = NULL;

	if (d->resource) {
		tex = TextureRef_getUnifiedTexture(d->resource, NULL);
		texExt = d->resource ? TextureRef_getImgExtT(d->resource, Dx, 0, d->texture.imageId) : NULL;
	}

	if(tex.depthFormat)
		switch(tex.depthFormat) {

			default:
			case EDepthStencilFormat_D32:  dxFormat = DXGI_FORMAT_R32_FLOAT;  break;
			case EDepthStencilFormat_D16:  dxFormat = DXGI_FORMAT_R16_UNORM;  break;

			case EDepthStencilFormat_D32S8X24Ext:
				dxFormat = d->texture.planeId ? DXGI_FORMAT_X32_TYPELESS_G8X24_UINT : DXGI_FORMAT_R32_FLOAT;
				break;

			case EDepthStencilFormat_D24S8Ext:
				dxFormat = d->texture.planeId ?
					DXGI_FORMAT_X24_TYPELESS_G8_UINT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
		}

	else dxFormat = ETextureFormatId_toDXFormat(tex.textureFormatId);

	if (!isWrite) {

		D3D12_SHADER_RESOURCE_VIEW_DESC srv = (D3D12_SHADER_RESOURCE_VIEW_DESC) {
			.Format = dxFormat,
			.Shader4ComponentMapping =  D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
		};

		switch(registerType & (EGfxRegisterType_TypeMask | EGfxRegisterType_IsArray)) {

			case EGfxRegisterType_Texture3D:
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				srv.Texture3D = (D3D12_TEX3D_SRV) {
					.MostDetailedMip = d->texture.mipId,
					.MipLevels = d->texture.mipCount
				};
				break;

			case EGfxRegisterType_TextureCube | EGfxRegisterType_IsArray:
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				srv.TextureCubeArray = (D3D12_TEXCUBE_ARRAY_SRV) {
					.MostDetailedMip = d->texture.mipId,
					.MipLevels = d->texture.mipCount,
					.First2DArrayFace = d->texture.arrayId,
					.NumCubes = d->texture.arrayCount / 6
				};
				break;

			case EGfxRegisterType_TextureCube:
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srv.TextureCube = (D3D12_TEXCUBE_SRV) {
					.MostDetailedMip = d->texture.mipId,
					.MipLevels = d->texture.mipCount
				};
				break;

			case EGfxRegisterType_Texture2DMS | EGfxRegisterType_IsArray:
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
				srv.Texture2DMSArray = (D3D12_TEX2DMS_ARRAY_SRV) {
					.FirstArraySlice = d->texture.arrayId,
					.ArraySize = d->texture.arrayCount
				};
				break;

			case EGfxRegisterType_Texture2DMS:
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
				srv.Texture2DMS = (D3D12_TEX2DMS_SRV) { 0 };
				break;

			case EGfxRegisterType_Texture2D | EGfxRegisterType_IsArray:
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				srv.Texture2DArray = (D3D12_TEX2D_ARRAY_SRV) {
					.MostDetailedMip = d->texture.mipId,
					.MipLevels = d->texture.mipCount,
					.FirstArraySlice = d->texture.arrayId,
					.ArraySize = d->texture.arrayCount,
					.PlaneSlice = d->texture.planeId
				};
				break;

			case EGfxRegisterType_Texture2D:
			default:
				srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srv.Texture2D = (D3D12_TEX2D_SRV) {
					.MostDetailedMip = d->texture.mipId,
					.MipLevels = d->texture.mipCount,
					.PlaneSlice = d->texture.planeId
				};
				break;
		}

		deviceExt->device->lpVtbl->CreateShaderResourceView(
			deviceExt->device,
			texExt ? texExt->image : NULL,
			&srv,
			dst
		);

	} else {

		D3D12_UNORDERED_ACCESS_VIEW_DESC uav = (D3D12_UNORDERED_ACCESS_VIEW_DESC) { .Format = dxFormat };

		switch(registerType & (EGfxRegisterType_TypeMask | EGfxRegisterType_IsArray)) {

			case EGfxRegisterType_Texture3D:
				uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
				uav.Texture3D = (D3D12_TEX3D_UAV) {
					.MipSlice = d->texture.mipId,
					.FirstWSlice = d->texture.arrayId,
					.WSize = d->texture.arrayCount
				};
				break;

			case EGfxRegisterType_Texture2DMS | EGfxRegisterType_IsArray:
				uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY;
				uav.Texture2DMSArray = (D3D12_TEX2DMS_ARRAY_UAV) {
					.FirstArraySlice = d->texture.arrayId,
					.ArraySize = d->texture.arrayCount
				};
				break;

			case EGfxRegisterType_Texture2DMS:
				uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMS;
				uav.Texture2DMS = (D3D12_TEX2DMS_UAV) { 0 };
				break;

			case EGfxRegisterType_Texture2D | EGfxRegisterType_IsArray:
				uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uav.Texture2DArray = (D3D12_TEX2D_ARRAY_UAV) {
					.MipSlice = d->texture.mipId,
					.FirstArraySlice = d->texture.arrayId,
					.ArraySize = d->texture.arrayCount
				};
				break;

			case EGfxRegisterType_Texture2D:
			default:
				uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uav.Texture2D = (D3D12_TEX2D_UAV) { .MipSlice = d->texture.mipId };
				break;
		}

		deviceExt->device->lpVtbl->CreateUnorderedAccessView(
			deviceExt->device,
			texExt ? texExt->image : NULL,
			NULL,
			&uav,
			dst
		);
	}

}

void DX_WRAP_FUNC(DescriptorTable_free)(DescriptorTable *table, const Allocator *alloc) {
	(void) alloc;
	DxDescriptorHeap *heapExt = DescriptorHeap_ext(DescriptorHeapRef_ptr(table->parent), Dx);
	DxDescriptorHeap_freeTable(heapExt, DescriptorTable_ext(table, Dx));
}

Bool DX_WRAP_FUNC(DescriptorHeap_createDescriptorTable)(
	DescriptorHeapRef *heap,
	DescriptorTable *table,
	const CharString *name,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(DescriptorHeapRef_ptr(heap)->device);

	(void) name;

	DxDescriptorHeap *heapExt = DescriptorHeap_ext(DescriptorHeapRef_ptr(heap), Dx);
	DxDescriptorTable *tableExt = DescriptorTable_ext(table, Dx);

	DescriptorLayout *layout = DescriptorLayoutRef_ptr(table->layout);

	U64 samplers = 0, srvCbvUav = 0;

	for (U64 i = 0; i < table->bindings.length; ++i) {

		DescriptorBinding b = layout->info.bindings.ptr[i];

		switch (b.registerType & EGfxRegisterType_TypeMask) {

			case EGfxRegisterType_Sampler:
			case EGfxRegisterType_SamplerComparisonState:
				samplers += b.count;
				break;

			case EGfxRegisterType_SubpassInput:        //Non existent
				break;

			default:
				srvCbvUav += b.count;
				break;
		}
	}

	tableExt->allocationSizes[0] = srvCbvUav;
	tableExt->allocationSizes[1] = samplers;

	gotoIfError3(clean, DxDescriptorHeap_allocTable(heapExt, tableExt, alloc, e_rr));

clean:
	return s_uccess;
}

D3D12_TEXTURE_ADDRESS_MODE mapDxAddressMode(ESamplerAddressMode addressMode) {
	switch (addressMode) {
		case ESamplerAddressMode_MirrorRepeat:   return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case ESamplerAddressMode_ClampToEdge:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case ESamplerAddressMode_ClampToBorder:  return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		default:                                 return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

//The filter a SamplerInfo maps to, comparison bit included.
//Shared because a sampler written into a descriptor table and one baked into a root signature as a
// static sampler have to describe the same thing.

D3D12_FILTER DxSampler_toFilter(SamplerInfo sinfo) {

	D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC;

	if(!sinfo.aniso)
		switch(sinfo.filter) {

			case ESamplerFilterMode_Nearest:
				filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				break;

			case ESamplerFilterMode_LinearMagNearestMinMip:
				filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
				break;

			case ESamplerFilterMode_LinearMinNearestMagMip:
				filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				break;

			case ESamplerFilterMode_LinearMagMinNearestMip:
				filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				break;

			case ESamplerFilterMode_LinearMipNearestMagMin:
				filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
				break;

			case ESamplerFilterMode_LinearMagMipNearestMin:
				filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
				break;

			case ESamplerFilterMode_LinearMinMipNearestMag:
				filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
				break;

			case ESamplerFilterMode_Linear:
				filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				break;
		}

	if(sinfo.enableComparison)
		filter |= 0x80;        //Signal we want comparison sampler

	return filter;
}

//A static sampler costs none of the root signature's 64 DWORDs: it is described by value in the signature
// itself rather than being a root parameter.
//Border color is an enum here rather than a float4, which is exactly what OxC3's ESamplerBorderColor
// already is, so every sampler it can express is expressible as a static one.

D3D12_STATIC_SAMPLER_DESC DxSampler_toStaticDesc(SamplerInfo sinfo, GfxBinding binding, U32 visibility) {

	D3D12_STATIC_BORDER_COLOR border = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

	switch (sinfo.borderColor) {

		case ESamplerBorderColor_OpaqueBlackFloat:
		case ESamplerBorderColor_OpaqueBlackInt:
			border = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
			break;

		case ESamplerBorderColor_OpaqueWhiteFloat:
		case ESamplerBorderColor_OpaqueWhiteInt:
			border = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			break;

		default:
			break;        //Transparent black
	}

	return (D3D12_STATIC_SAMPLER_DESC) {
		.Filter = DxSampler_toFilter(sinfo),
		.AddressU = mapDxAddressMode(sinfo.addressU),
		.AddressV = mapDxAddressMode(sinfo.addressV),
		.AddressW = mapDxAddressMode(sinfo.addressW),
		.MipLODBias = F16_castF32(sinfo.mipBias),
		.MaxAnisotropy = sinfo.aniso,
		.ComparisonFunc = sinfo.enableComparison ?
			mapDxCompareOp(sinfo.comparisonFunction) : D3D12_COMPARISON_FUNC_NEVER,
		.BorderColor = border,
		.MinLOD = F16_castF32(sinfo.minLod),
		.MaxLOD = F16_castF32(sinfo.maxLod),
		.ShaderRegister = binding.binding,
		.RegisterSpace = binding.space,
		.ShaderVisibility = DxDescriptorLayout_convertVisibility(visibility)
	};
}

//Takes a count and not a ListDescriptor: the interface table's slot is
//DescriptorTable_unsetDescriptorsImpl, which the Vulkan side already matched.
//A pointer and a U64 arrive in the same register on x64, so this went unnoticed until
//-fsanitize=function compared the two types at the call.

Bool DX_WRAP_FUNC(DescriptorTable_unsetDescriptors)(
	DescriptorTable *table,
	U64 bindId,
	U64 arrayId,
	U64 count,
	Error *e_rr
) {
	(void) table; (void) bindId; (void) arrayId; (void) count; (void) e_rr;
	return true;
}

Bool DX_WRAP_FUNC(DescriptorTable_setDescriptors)(
	DescriptorTable *table,
	U64 bindId,
	U64 arrayId,
	const ListDescriptor *darr,
	Error *e_rr
) {

	(void) e_rr;

	const DescriptorHeap *heap = DescriptorHeapRef_ptr(table->parent);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(GraphicsDeviceRef_ptr(heap->device), Dx);
	DxDescriptorHeap *heapExt = DescriptorHeap_ext(heap, Dx);

	DxDescriptorTable *tableExt = DescriptorTable_ext(table, Dx);
	DescriptorLayout *layout = DescriptorLayoutRef_ptr(table->layout);
	const ListDescriptorBinding *bindings = &layout->info.bindings;
	const DescriptorBinding *binding = &bindings->ptr[bindId];
	EGfxRegisterType registerType = binding->registerType;

	U64 offsetCbvSrvUav = arrayId + DescriptorLayout_ext(layout, Dx)->bindingOffsets.ptr[bindId];
	U64 offsetSamplers = offsetCbvSrvUav;

	offsetCbvSrvUav += tableExt->allocationLocations[0];
	offsetSamplers += tableExt->allocationLocations[1];

	EGfxRegisterType type = registerType & EGfxRegisterType_TypeMask;
	Bool isWrite = registerType & EGfxRegisterType_IsWrite;
	Bool isArrayType = registerType & EGfxRegisterType_IsArray;

	U64 heapPtrRes = heapExt->resourcesHeap.cpuHandle.ptr;
	U64 heapIncRes = heapExt->resourcesHeap.cpuIncrement;

	for(U64 i = 0; i < darr->length; ++i) {

		Descriptor d = darr->ptr[i];

		if(d.resource) {

			if (type >= EGfxRegisterType_BufferStart && type < EGfxRegisterType_BufferEnd) {

				U64 len = DeviceBufferRef_ptr(d.resource)->resource.size;

				if(!Descriptor_endBuffer(&d))
					d.buffer.endRegionAndCounterOffset.region48 |= len - Descriptor_startBuffer(&d);

			} else if(type >= EGfxRegisterType_TextureStart && type < EGfxRegisterType_TextureEnd) {

				UnifiedTexture tex = TextureRef_getUnifiedTexture(d.resource, NULL);

				if(isArrayType && !d.texture.arrayCount)
					d.texture.arrayCount = tex.length - d.texture.arrayId;

				if (!isWrite) {
					if(!d.texture.mipCount)
						d.texture.mipCount = tex.levels - d.texture.mipId;
				}

				else if(!d.texture.mipCount)
					d.texture.mipCount = 1;
			}
		}

		switch (type) {

			case EGfxRegisterType_ConstantBuffer: {

				D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = (D3D12_CONSTANT_BUFFER_VIEW_DESC) { 0 };

				if (d.resource) {
					cbv.BufferLocation = DeviceBufferRef_ptr(d.resource)->resource.deviceAddress + Descriptor_startBuffer(&d);
					cbv.SizeInBytes = (U32) Descriptor_bufferLength(&d);
				}

				deviceExt->device->lpVtbl->CreateConstantBufferView(
					deviceExt->device,
					d.resource ? &cbv : NULL,
					(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heapPtrRes + heapIncRes * offsetCbvSrvUav++ }
				);

				break;
			}

			case EGfxRegisterType_Sampler:
			case EGfxRegisterType_SamplerComparisonState: {

				D3D12_SAMPLER_DESC samplerView;

				if(d.resource) {

					const SamplerInfo sinfo = SamplerRef_ptr(d.resource)->info;

					//Allocate descriptor

					const D3D12_FILTER filter = DxSampler_toFilter(sinfo);

					samplerView = (D3D12_SAMPLER_DESC) {
						.Filter = filter,
						.AddressU = mapDxAddressMode(sinfo.addressU),
						.AddressV = mapDxAddressMode(sinfo.addressV),
						.AddressW = mapDxAddressMode(sinfo.addressW),
						.MipLODBias = F16_castF32(sinfo.mipBias),
						.MaxAnisotropy = sinfo.aniso,
						.MinLOD = F16_castF32(sinfo.minLod),
						.MaxLOD = F16_castF32(sinfo.maxLod)
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

							samplerView.BorderColor[0] = samplerView.BorderColor[1] = samplerView.BorderColor[2] =
								*(const F32*)&one;

							samplerView.BorderColor[3] = *(const F32*)&one;
							break;

						case ESamplerBorderColor_OpaqueBlackInt:
							samplerView.BorderColor[3] = *(const F32*)&one;
							break;

						default:
							break;        //Already null initialized
					}

					//The comparison bit already rode in on the filter above

					if(sinfo.enableComparison)
						samplerView.ComparisonFunc = mapDxCompareOp(sinfo.comparisonFunction);
				}

				else samplerView = (D3D12_SAMPLER_DESC) {
					.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
					.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
					.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
					.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
					.MipLODBias = 0,
					.MaxAnisotropy = 1,
					.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
					.BorderColor = { 0, 0, 0, 0 },
					.MinLOD = 0,
					.MaxLOD = D3D12_FLOAT32_MAX
				};

				deviceExt->device->lpVtbl->CreateSampler(
					deviceExt->device,
					&samplerView,
					(D3D12_CPU_DESCRIPTOR_HANDLE) {
						.ptr = heapExt->samplerHeap.cpuHandle.ptr + heapExt->samplerHeap.cpuIncrement * offsetSamplers++
					}
				);

				break;
			}

			case EGfxRegisterType_AccelerationStructure: {

				D3D12_GPU_VIRTUAL_ADDRESS dstAS = 0;

				if(d.resource)
					dstAS = DeviceBufferRef_ptr(TLASRef_ptr(d.resource)->base.asBuffer)->resource.deviceAddress;

				D3D12_SHADER_RESOURCE_VIEW_DESC resourceView = (D3D12_SHADER_RESOURCE_VIEW_DESC) {
					.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
					.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
					.RaytracingAccelerationStructure = (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV) {
						.Location = dstAS
					}
				};

				deviceExt->device->lpVtbl->CreateShaderResourceView(
					deviceExt->device,
					NULL,
					&resourceView,
					(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heapPtrRes + heapIncRes * offsetCbvSrvUav++ }
				);

				break;
			}

			case EGfxRegisterType_ByteAddressBuffer: {

				DxDeviceBuffer *deviceBuffer = d.resource ? DeviceBuffer_ext(DeviceBufferRef_ptr(d.resource), Dx) : NULL;

				if (isWrite) {

					D3D12_UNORDERED_ACCESS_VIEW_DESC uav = (D3D12_UNORDERED_ACCESS_VIEW_DESC) {
						.Format = DXGI_FORMAT_R32_TYPELESS,
						.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
						.Buffer = (D3D12_BUFFER_UAV) {
							.FirstElement = Descriptor_startBuffer(&d) / 4,
							.NumElements = (U32)(Descriptor_bufferLength(&d) / 4),
							.Flags = D3D12_BUFFER_UAV_FLAG_RAW
						}
					};

					deviceExt->device->lpVtbl->CreateUnorderedAccessView(
						deviceExt->device,
						deviceBuffer ? deviceBuffer->buffer : NULL,
						NULL,
						&uav,
						(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heapPtrRes + heapIncRes * offsetCbvSrvUav++ }
					);
				}

				else {

					D3D12_SHADER_RESOURCE_VIEW_DESC srv = (D3D12_SHADER_RESOURCE_VIEW_DESC) {
						.Format = DXGI_FORMAT_R32_TYPELESS,
						.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
						.Shader4ComponentMapping =  D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
						.Buffer = (D3D12_BUFFER_SRV) {
							.FirstElement = Descriptor_startBuffer(&d) / 4,
							.NumElements = (U32)(Descriptor_bufferLength(&d) / 4),
							.Flags = D3D12_BUFFER_SRV_FLAG_RAW
						}
					};

					deviceExt->device->lpVtbl->CreateShaderResourceView(
						deviceExt->device,
						deviceBuffer ? deviceBuffer->buffer : NULL,
						&srv,
						(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heapPtrRes + heapIncRes * offsetCbvSrvUav++ }
					);
				}

				break;
			}

			case EGfxRegisterType_StructuredBuffer:
			case EGfxRegisterType_StructuredBufferAtomic: {

				DxDeviceBuffer *deviceBuffer = d.resource ? DeviceBuffer_ext(DeviceBufferRef_ptr(d.resource), Dx) : NULL;
				U32 reflStride = binding->structedBufferStride;

				if (isWrite) {

					D3D12_UNORDERED_ACCESS_VIEW_DESC uav = (D3D12_UNORDERED_ACCESS_VIEW_DESC) {
						.Format = DXGI_FORMAT_UNKNOWN,
						.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
						.Buffer = (D3D12_BUFFER_UAV) {
							.FirstElement = Descriptor_startBuffer(&d) / reflStride,
							.StructureByteStride = reflStride,
							.NumElements = (U32)(Descriptor_bufferLength(&d) / reflStride),
							.CounterOffsetInBytes = Descriptor_counterOffset(&d)
						}
					};

					deviceExt->device->lpVtbl->CreateUnorderedAccessView(
						deviceExt->device,
						deviceBuffer ? deviceBuffer->buffer : NULL,
						d.buffer.counter ? DeviceBuffer_ext(DeviceBufferRef_ptr(d.buffer.counter), Dx)->buffer : NULL,
						&uav,
						(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heapPtrRes + heapIncRes * offsetCbvSrvUav++ }
					);
				}

				else {

					D3D12_SHADER_RESOURCE_VIEW_DESC srv = (D3D12_SHADER_RESOURCE_VIEW_DESC) {
						.Format = DXGI_FORMAT_UNKNOWN,
						.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
						.Shader4ComponentMapping =  D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
						.Buffer = (D3D12_BUFFER_SRV) {
							.FirstElement = Descriptor_startBuffer(&d) / reflStride,
							.StructureByteStride = reflStride,
							.NumElements = (U32)(Descriptor_bufferLength(&d) / reflStride)
						}
					};

					deviceExt->device->lpVtbl->CreateShaderResourceView(
						deviceExt->device,
						deviceBuffer ? deviceBuffer->buffer : NULL,
						&srv,
						(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heapPtrRes + heapIncRes * offsetCbvSrvUav++ }
					);
				}

				break;
			}

			case EGfxRegisterType_Texture1D:
			case EGfxRegisterType_Texture2D:
			case EGfxRegisterType_Texture3D:
			case EGfxRegisterType_TextureCube:
			case EGfxRegisterType_Texture2DMS: {

				DxDescriptorTable_writeTexture(
					deviceExt, &d, registerType,
					(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heapPtrRes + heapIncRes * offsetCbvSrvUav++ }
				);

				break;
			}

			case EGfxRegisterType_StorageBuffer:
			case EGfxRegisterType_StorageBufferAtomic:
			case EGfxRegisterType_SubpassInput:                //Doesn't do anything
			default:
				break;
		}
	}

	return true;
}
