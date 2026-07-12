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

//graphics/d3d12/generic/dx_texture.c

#include "types/container/list.h"
#include "graphics/generic/interface.h"
#include "graphics/d3d12/dx_interface.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/instance.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "platforms/logx.h"
#include "types/container/texture_format.h"
#include "types/base/constants.h"
#include "types/container/string_unicode.h"

void DX_WRAP_FUNC(UnifiedTexture_free)(TextureRef *textureRef) {

	const UnifiedTexture utex = TextureRef_getUnifiedTexture(textureRef, NULL);

	for(U8 i = 0; i < utex.images; ++i) {

		const DxUnifiedTexture *image = TextureRef_getImgExtT(textureRef, Dx, 0, i);

		if(image->image && utex.resource.type != EResourceType_Swapchain)
			image->image->lpVtbl->Release(image->image);
	}
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Bool DX_WRAP_FUNC(UnifiedTexture_create)(TextureRef *textureRef, CharString name, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = textureRef ?
		GraphicsDeviceRef_getAlloc(TextureRef_getUnifiedTexture(textureRef, NULL).resource.device) : NULL;

	UnifiedTexture *texture = TextureRef_getUnifiedTextureIntern(textureRef, NULL);

	//Prepare temporary free-ables and extended data.

	ListU16 temp16 = (ListU16) { 0 };
	ELockAcquire acq = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->resource.device);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DXGI_FORMAT dxFormat = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT dxFormatFullyQualified = DXGI_FORMAT_UNKNOWN;

	if(texture->depthFormat)
		dxFormatFullyQualified = dxFormat = EDepthStencilFormat_toDXFormat(texture->depthFormat);

	//We make everything typeless to allow an easy copy image for example

	else switch(texture->textureFormatId) {

		case ETextureFormatId_R8 : case ETextureFormatId_R8s: case ETextureFormatId_R8u: case ETextureFormatId_R8i:
			dxFormat = DXGI_FORMAT_R8_TYPELESS;
			break;

		case ETextureFormatId_RG8 : case ETextureFormatId_RG8s: case ETextureFormatId_RG8u: case ETextureFormatId_RG8i:
			dxFormat = DXGI_FORMAT_R8G8_TYPELESS;
			break;

		case ETextureFormatId_RGBA8 : case ETextureFormatId_RGBA8s: case ETextureFormatId_RGBA8u: case ETextureFormatId_RGBA8i:
			dxFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
			break;

		case ETextureFormatId_BGRA8:
			dxFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
			break;

		case ETextureFormatId_R16 : case ETextureFormatId_R16s:
		case ETextureFormatId_R16u: case ETextureFormatId_R16i: case ETextureFormatId_R16f:
			dxFormat = DXGI_FORMAT_R16_TYPELESS;
			break;

		case ETextureFormatId_RG16 : case ETextureFormatId_RG16s:
		case ETextureFormatId_RG16u: case ETextureFormatId_RG16i: case ETextureFormatId_RG16f:
			dxFormat = DXGI_FORMAT_R16G16_TYPELESS;
			break;

		case ETextureFormatId_RGBA16 : case ETextureFormatId_RGBA16s:
		case ETextureFormatId_RGBA16u: case ETextureFormatId_RGBA16i: case ETextureFormatId_RGBA16f:
			dxFormat = DXGI_FORMAT_R16G16B16A16_TYPELESS;
			break;

		case ETextureFormatId_R32u: case ETextureFormatId_R32i: case ETextureFormatId_R32f:
			dxFormat = DXGI_FORMAT_R32_TYPELESS;
			break;

		case ETextureFormatId_RG32u: case ETextureFormatId_RG32i: case ETextureFormatId_RG32f:
			dxFormat = DXGI_FORMAT_R32G32_TYPELESS;
			break;

		case ETextureFormatId_RGB32u: case ETextureFormatId_RGB32i: case ETextureFormatId_RGB32f:
			dxFormat = DXGI_FORMAT_R32G32B32_TYPELESS;
			break;

		case ETextureFormatId_RGBA32u: case ETextureFormatId_RGBA32i: case ETextureFormatId_RGBA32f:
			dxFormat = DXGI_FORMAT_R32G32B32A32_TYPELESS;
			break;

		case ETextureFormatId_BGR10A2:                                dxFormat = DXGI_FORMAT_R10G10B10A2_TYPELESS;    break;
		case ETextureFormatId_BC6H:                                    dxFormat = DXGI_FORMAT_BC6H_TYPELESS;            break;

		case ETextureFormatId_BC4:    case ETextureFormatId_BC4s:        dxFormat = DXGI_FORMAT_BC4_TYPELESS;            break;
		case ETextureFormatId_BC5:    case ETextureFormatId_BC5s:        dxFormat = DXGI_FORMAT_BC5_TYPELESS;            break;

		case ETextureFormatId_BC7: case ETextureFormatId_BC7_sRGB:    dxFormat = DXGI_FORMAT_BC7_TYPELESS;            break;

		default:
			retError(clean, Error_unsupportedOperation(
				0,
				"UnifiedTexture_create() was called with unsupported texture format"
			));
	}

	if(!texture->depthFormat)
		dxFormatFullyQualified = ETextureFormatId_toDXFormat(texture->textureFormatId);

	Bool isDeviceTexture = texture->resource.type == EResourceType_DeviceTexture;

	//Query about alignment and size

	D3D12_RESOURCE_DESC1 resourceDesc = (D3D12_RESOURCE_DESC1) {

		.Dimension =
			texture->type == ETextureType_3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D,

		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = texture->width,
		.Height = texture->height,
		.DepthOrArraySize = texture->length,
		.MipLevels = texture->levels,
		.Format = dxFormat,
		.SampleDesc = (DXGI_SAMPLE_DESC) { .Count = 1 << texture->sampleCount, .Quality = 0 }
	};

	if(!isDeviceTexture) {

		if(texture->depthFormat)
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		else resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	if(texture->resource.flags & EGraphicsResourceFlag_ShaderWrite)
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if(!(texture->resource.flags & EGraphicsResourceFlag_ShaderRead) && texture->depthFormat)
		resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	#if D3D12_SDK_VERSION >= 618
		if(device->info.capabilities.featuresExt & EDxGraphicsFeatures_TightAlignment)
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;
	#endif

	//Allocate memory

	if(texture->resource.type != EResourceType_Swapchain) {

		D3D12_RESOURCE_ALLOCATION_INFO1 allocInfo = (D3D12_RESOURCE_ALLOCATION_INFO1) { 0 };
		D3D12_RESOURCE_ALLOCATION_INFO retVal = (D3D12_RESOURCE_ALLOCATION_INFO) { 0 };
		D3D12_RESOURCE_ALLOCATION_INFO *res = NULL;

		//Small texture optimization:
		//DeviceTextures with certain properties allow 4KiB alignment rather than 64KiB.
		// (This generally happens when the mip 0 size is <64KiB)
		//RenderTextures or DepthStencils can't.
		//This saves some space.

		if (texture->resource.type == EResourceType_DeviceTexture) {

			allocInfo.Alignment = 4 * KIBI;

			res = deviceExt->device->lpVtbl->GetResourceAllocationInfo2(
				deviceExt->device, &retVal, 0, 1, &resourceDesc, &allocInfo
			);

			//Small alignment failed, try again with default alignment.

			if (!res || allocInfo.Alignment != 4 * KIBI || allocInfo.SizeInBytes == U64_MAX) {

				allocInfo = (D3D12_RESOURCE_ALLOCATION_INFO1) { 0 };
				retVal = (D3D12_RESOURCE_ALLOCATION_INFO) { 0 };

				res = deviceExt->device->lpVtbl->GetResourceAllocationInfo2(
					deviceExt->device, &retVal, 0, 1, &resourceDesc, &allocInfo
				);
			}
		}

		else res = deviceExt->device->lpVtbl->GetResourceAllocationInfo2(
			deviceExt->device, &retVal, 0, 1, &resourceDesc, &allocInfo
		);

		if(!res)
			retError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't query allocInfo"));

		//TODO: versioned image

		DxUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Dx, 0, 0);

		Bool cpuSided = texture->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit;

		//Dedicated allocations for depth stencil and render targets that are >=512px
		//Magic number from NV best practices and seems to match Vulkan's behavior closely.

		if(
			!isDeviceTexture &&
			texture->width >= 512 && texture->height >= 512 &&
			device->info.type == EGraphicsDeviceType_Dedicated
		) {

			DeviceMemoryBlock block = (DeviceMemoryBlock) {
				.isActive = true,
				.typeExt = (U32) allocInfo.Alignment,
				.allocationTypeExt = !cpuSided,        //Don't share dedicated and non dedicated allocations
				.isDedicated = true
			};

			if(device->flags & EGraphicsDeviceFlags_IsDebug)
				Error_captureStackTrace(block.stackTrace, (U8)(sizeof(block.stackTrace) / sizeof(void*)), 1);

			if(allocInfo.SizeInBytes > device->info.capabilities.maxAllocationSize)
				retError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't allocate resource size!"));

			U64 usedMem = DX_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(device, !cpuSided);
			U64 maxAlloc = cpuSided ? device->info.capabilities.sharedMemory : device->info.capabilities.dedicatedMemory;

			if(usedMem != U64_MAX && usedMem + allocInfo.SizeInBytes > maxAlloc)
				retError(clean, Error_outOfMemory(0, "Dedicated memory block allocation would exceed available memory"));

			acq = SpinLock_lock(&device->allocator.lock, U64_MAX);

			if(device->allocator.blocks.length >= U32_MAX)
				retError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't allocate dedicated block"));

			U32 blockId = (U32) device->allocator.blocks.length;
			gotoIfError3(clean, ListDeviceMemoryBlock_pushBack(&device->allocator.blocks, block, alloc, e_rr));

			AllocationBuffer *allocBuf = &device->allocator.blocks.ptrNonConst[blockId].allocations;

			gotoIfError3(clean, AllocationBuffer_create(
				&(AllocationBufferCreate) {
					.size = allocInfo.SizeInBytes,
					.nonLinearAlignment = 0,
					.alloc = alloc,
					.allocationBuffer = allocBuf
				},
				true, e_rr
			));

			U8 *dummy = NULL;
			gotoIfError3(clean, AllocationBuffer_allocateBlock(
				&(AllocationBufferAllocate) {
					.allocationBuffer = allocBuf,
					.alignment = allocInfo.Alignment,
					.isNonLinearResource = false,
					.alloc = alloc
				},
				allocInfo.SizeInBytes, (const U8**) &dummy, e_rr
			));

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(&device->allocator.lock);

			acq = ELockAcquire_Invalid;

			D3D12_HEAP_DESC heap = getDxHeapDesc(device, &cpuSided, allocInfo.Alignment, EResourceType_Undefined);

			D3D12_CLEAR_VALUE clearValue = (D3D12_CLEAR_VALUE) { .Format = dxFormatFullyQualified };

			if(device->flags & EGraphicsDeviceFlags_IsDebug)
				Log_debugLnx(
					"-- Graphics: Allocating dedicated memory block (%"PRIu32" with size %"PRIu64")\n"
					"\t%s (Memory left: %"PRIu64")",
					blockId,
					allocInfo.SizeInBytes,
					cpuSided ? "Cpu sided allocation" : "Gpu sided allocation",
					maxAlloc - usedMem
				);

			gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommittedResource3(
				deviceExt->device,
				&heap.Properties,
				heap.Flags,
				&resourceDesc,
				D3D12_BARRIER_LAYOUT_COMMON,
				texture->resource.type == EResourceType_DeviceTexture ? NULL : &clearValue,
				NULL, 0, NULL,
				&IID_ID3D12Resource,
				(void**)&managedImageExt->image
			), e_rr));

			texture->resource.allocated = true;
			texture->resource.blockId = blockId;
			texture->resource.blockOffset = 0;
		}

		else {

			DxBlockRequirements req = (DxBlockRequirements) {
				.flags = EDxBlockFlags_None,
				.alignment = (U32) allocInfo.Alignment,
				.length = allocInfo.SizeInBytes
			};

			DeviceMemoryBlock block;
			gotoIfError3(clean, DX_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
				&device->allocator,
				&req,
				cpuSided,
				&texture->resource.blockId,
				&texture->resource.blockOffset,
				texture->resource.type,
				name,
				&block, e_rr));

			texture->resource.allocated = true;

			D3D12_CLEAR_VALUE clearValue = (D3D12_CLEAR_VALUE) { .Format = dxFormatFullyQualified };

			gotoIfError3(clean, dxCheck(deviceExt->device->lpVtbl->CreatePlacedResource2(
				deviceExt->device,
				block.ext,
				texture->resource.blockOffset,
				&resourceDesc,
				D3D12_BARRIER_LAYOUT_COMMON,
				texture->resource.type == EResourceType_DeviceTexture ? NULL : &clearValue,
				0, NULL,
				&IID_ID3D12Resource,
				(void**)&managedImageExt->image
			), e_rr));
		}
	}

	//Name resources

	for(U8 i = 0; i < texture->images; ++i) {

		DxUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Dx, 0, i);

		managedImageExt->lastAccess = D3D12_BARRIER_ACCESS_NO_ACCESS;

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name)) {
			gotoIfError3(clean, CharString_toUTF16(name, alloc, &temp16, e_rr));
			gotoIfError3(clean, dxCheck(managedImageExt->image->lpVtbl->SetName(managedImageExt->image, temp16.ptr), e_rr));
			ListU16_free(&temp16, alloc);
		}
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->allocator.lock);

	ListU16_free(&temp16, alloc);
	return s_uccess;
}

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
) {

	Bool s_uccess = true;

	//Avoid duplicate barriers except in one case:
	//direct3d12.has the concept of UAVBarriers, which always need to be inserted in-between two compute calls.
	//Otherwise, it's not synchronized correctly.

	if(
		image->lastSync == sync && image->lastAccess == access &&
		access != D3D12_BARRIER_ACCESS_UNORDERED_ACCESS &&
		access != D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE &&
		access != D3D12_BARRIER_ACCESS_RENDER_TARGET &&
		access != D3D12_BARRIER_ACCESS_COPY_DEST &&
		access != D3D12_BARRIER_ACCESS_RESOLVE_DEST
	)
		return s_uccess;

	//Handle image barrier

	const D3D12_TEXTURE_BARRIER imageBarrier = (D3D12_TEXTURE_BARRIER) {

		.SyncBefore = image->lastSync,
		.SyncAfter = sync,

		.AccessBefore = image->lastAccess,
		.AccessAfter = access,

		.LayoutBefore = image->lastLayout,
		.LayoutAfter = layout,

		.pResource = image->image,

		.Subresources = *range
	};

	gotoIfError3(clean, ListD3D12_TEXTURE_BARRIER_pushBack(imageBarriers, imageBarrier, alloc, e_rr));

	image->lastLayout = imageBarrier.LayoutAfter;
	image->lastSync = imageBarrier.SyncAfter;
	image->lastAccess = imageBarrier.AccessAfter;

	dependency->pTextureBarriers = imageBarriers->ptr;
	dependency->NumBarriers = (U32) imageBarriers->length;

clean:
	return s_uccess;
}
