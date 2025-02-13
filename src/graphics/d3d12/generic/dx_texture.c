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

#include "platforms/ext/listx.h"
#include "graphics/generic/interface.h"
#include "graphics/d3d12/dx_interface.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/generic/texture.h"
#include "graphics/generic/device.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/instance.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "types/container/texture_format.h"

Bool DX_WRAP_FUNC(UnifiedTexture_free)(TextureRef *textureRef) {

	const UnifiedTexture utex = TextureRef_getUnifiedTexture(textureRef, NULL);

	for(U8 i = 0; i < utex.images; ++i) {

		const DxUnifiedTexture *image = TextureRef_getImgExtT(textureRef, Dx, 0, i);

		if(image->image && utex.resource.type != EResourceType_Swapchain)
			image->image->lpVtbl->Release(image->image);
	}

	return true;
}

UnifiedTexture *TextureRef_getUnifiedTextureIntern(TextureRef *tex, DeviceResourceVersion *version);

Error DX_WRAP_FUNC(UnifiedTexture_create)(TextureRef *textureRef, CharString name) {

	UnifiedTexture *texture = TextureRef_getUnifiedTextureIntern(textureRef, NULL);

	//Prepare temporary free-ables and extended data.

	Error err = Error_none();
	ListU16 temp16 = (ListU16) { 0 };
	ELockAcquire acq = ELockAcquire_Invalid;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(texture->resource.device);
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	DXGI_FORMAT dxFormat = DXGI_FORMAT_UNKNOWN;

	if(texture->depthFormat)
		dxFormat = EDepthStencilFormat_toDXFormat(texture->depthFormat);

	else dxFormat = ETextureFormatId_toDXFormat(texture->textureFormatId);

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

	#if D3D12_PREVIEW_SDK_VERSION >= 716
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
			gotoIfError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't query allocInfo"))

		//TODO: versioned image

		DxUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Dx, 0, 0);
		
		Bool cpuSided = !!(texture->resource.flags & EGraphicsResourceFlag_CPUAllocatedBit);

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
				.allocationTypeExt = !cpuSided,		//Don't share dedicated and non dedicated allocations
				.isDedicated = true
			};

			if(device->flags & EGraphicsDeviceFlags_IsDebug)
				Log_captureStackTracex(block.stackTrace, sizeof(block.stackTrace) / sizeof(void*), 1);

			if(allocInfo.SizeInBytes > device->info.capabilities.maxAllocationSize)
				gotoIfError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't allocate resource size!"))

			U64 usedMem = DX_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(device, !cpuSided);
			U64 maxAlloc = cpuSided ? device->info.capabilities.sharedMemory : device->info.capabilities.dedicatedMemory;

			if(usedMem != U64_MAX && usedMem + allocInfo.SizeInBytes > maxAlloc)
				gotoIfError(clean, Error_outOfMemory(0, "Dedicated memory block allocation would exceed available memory"))

			acq = SpinLock_lock(&device->allocator.lock, U64_MAX);

			if(device->allocator.blocks.length >= U32_MAX)
				gotoIfError(clean, Error_invalidState(0, "D3D12UnifiedTexture_create() couldn't allocate dedicated block"))

			U32 blockId = (U32) device->allocator.blocks.length;
			gotoIfError(clean, ListDeviceMemoryBlock_pushBackx(&device->allocator.blocks, block))

			AllocationBuffer *allocBuf = &device->allocator.blocks.ptrNonConst[blockId].allocations;

			gotoIfError(clean, AllocationBuffer_createx(allocInfo.SizeInBytes, true, 0, allocBuf))

			U8 *dummy = NULL;
			gotoIfError(clean, AllocationBuffer_allocateBlockx(
				allocBuf,
				allocInfo.SizeInBytes,
				allocInfo.Alignment,
				false,
				(const U8**) &dummy
			))

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(&device->allocator.lock);

			acq = ELockAcquire_Invalid;

			D3D12_HEAP_DESC heap = getDxHeapDesc(device, &cpuSided, allocInfo.Alignment);

			D3D12_CLEAR_VALUE clearValue = (D3D12_CLEAR_VALUE) { .Format = dxFormat };
			
			if(device->flags & EGraphicsDeviceFlags_IsDebug)
				Log_debugLnx(
					"-- Graphics: Allocating dedicated memory block (%"PRIu32" with size %"PRIu64")\n"
					"\t%s (Memory left: %"PRIu64")",
					blockId,
					allocInfo.SizeInBytes,
					cpuSided ? "Cpu sided allocation" : "Gpu sided allocation",
					maxAlloc - usedMem
				);

			gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateCommittedResource3(
				deviceExt->device,
				&heap.Properties,
				heap.Flags,
				&resourceDesc,
				D3D12_BARRIER_LAYOUT_COMMON,
				texture->resource.type == EResourceType_DeviceTexture ? NULL : &clearValue,
				NULL, 0, NULL,
				&IID_ID3D12Resource,
				(void**)&managedImageExt->image
			)))

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
			gotoIfError(clean, DX_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
				&device->allocator,
				&req,
				cpuSided,
				&texture->resource.blockId,
				&texture->resource.blockOffset,
				texture->resource.type,
				name,
				&block
			))

			texture->resource.allocated = true;

			D3D12_CLEAR_VALUE clearValue = (D3D12_CLEAR_VALUE) { .Format = dxFormat };

			gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreatePlacedResource2(
				deviceExt->device,
				block.ext,
				texture->resource.blockOffset,
				&resourceDesc,
				D3D12_BARRIER_LAYOUT_COMMON,
				texture->resource.type == EResourceType_DeviceTexture ? NULL : &clearValue,
				0, NULL,
				&IID_ID3D12Resource,
				(void**)&managedImageExt->image
			)))
		}
	}

	//Image views

	const DxDescriptorHeapSingle *heap = &DescriptorHeap_ext(DescriptorHeapRef_ptr(device->descriptorHeaps), Dx)->resourcesHeap;

	for(U8 i = 0; i < texture->images; ++i) {

		DxUnifiedTexture *managedImageExt = TextureRef_getImgExtT(textureRef, Dx, 0, i);
		UnifiedTextureImage managedImage = TextureRef_getImage(textureRef, 0, i);

		managedImageExt->lastAccess = D3D12_BARRIER_ACCESS_NO_ACCESS;

		if (texture->resource.flags & EGraphicsResourceFlag_ShaderRead) {

			D3D12_SHADER_RESOURCE_VIEW_DESC srv = (D3D12_SHADER_RESOURCE_VIEW_DESC) {
				.Format = dxFormat,
				.Shader4ComponentMapping =  D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
			};

			U64 offset = ResourceHandle_getId(managedImage.readHandle);

			switch(texture->type) {

				case ETextureType_3D:
					offset += EDescriptorTypeOffsets_Texture3D;
					srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
					srv.Texture3D = (D3D12_TEX3D_SRV) { .MipLevels = texture->levels };
					break;

				case ETextureType_Cube:
					offset += EDescriptorTypeOffsets_TextureCube;
					srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					srv.TextureCube = (D3D12_TEXCUBE_SRV) { .MipLevels = texture->levels };
					break;

				default:
					offset += EDescriptorTypeOffsets_Texture2D;
					srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srv.Texture2D = (D3D12_TEX2D_SRV) { .MipLevels = texture->levels };
					break;
			}

			deviceExt->device->lpVtbl->CreateShaderResourceView(
				deviceExt->device,
				managedImageExt->image,
				&srv,
				(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap->cpuHandle.ptr + heap->cpuIncrement * offset }
			);
		}

		if (texture->resource.flags & EGraphicsResourceFlag_ShaderWrite) {

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = (D3D12_UNORDERED_ACCESS_VIEW_DESC) { .Format = dxFormat };

			U64 offset = ResourceHandle_getId(managedImage.writeHandle);
			offset += EDescriptorTypeOffsets_values[UnifiedTexture_getWriteDescriptorType(*texture)];

			switch(texture->type) {

				case ETextureType_3D:
					uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
					uav.Texture3D = (D3D12_TEX3D_UAV) { .WSize = texture->length };
					break;

				case ETextureType_Cube:
					uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					uav.Texture2DArray = (D3D12_TEX2D_ARRAY_UAV) { .ArraySize = texture->length };
					break;

				default:
					uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					uav.Texture2D = (D3D12_TEX2D_UAV) { 0 };
					break;
			}

			deviceExt->device->lpVtbl->CreateUnorderedAccessView(
				deviceExt->device,
				managedImageExt->image,
				NULL,
				&uav,
				(D3D12_CPU_DESCRIPTOR_HANDLE) { .ptr = heap->cpuHandle.ptr + heap->cpuIncrement * offset }
			);
		}

		if((device->flags & EGraphicsDeviceFlags_IsDebug) && CharString_length(name)) {
			gotoIfError(clean, CharString_toUTF16x(name, &temp16))
			gotoIfError(clean, dxCheck(managedImageExt->image->lpVtbl->SetName(managedImageExt->image, temp16.ptr)))
			ListU16_freex(&temp16);
		}
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->allocator.lock);

	ListU16_freex(&temp16);
	return err;
}

Error DxUnifiedTexture_transition(
	DxUnifiedTexture *image,
	D3D12_BARRIER_SYNC sync,
	D3D12_BARRIER_ACCESS access,
	D3D12_BARRIER_LAYOUT layout,
	const D3D12_BARRIER_SUBRESOURCE_RANGE *range,
	ListD3D12_TEXTURE_BARRIER *imageBarriers,
	D3D12_BARRIER_GROUP *dependency
) {

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
		return Error_none();

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

	const Error err = ListD3D12_TEXTURE_BARRIER_pushBackx(imageBarriers, imageBarrier);

	if(err.genericError)
		return err;

	image->lastLayout = imageBarrier.LayoutAfter;
	image->lastSync = imageBarrier.SyncAfter;
	image->lastAccess = imageBarrier.AccessAfter;

	dependency->pTextureBarriers = imageBarriers->ptr;
	dependency->NumBarriers = (U32) imageBarriers->length;

	return Error_none();
}
