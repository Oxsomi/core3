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

#include "platforms/ext/listx_impl.h"
#include "graphics/generic/allocator.h"
#include "graphics/directx12/dx_device.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/error.h"
#include "types/math.h"
#include "types/string.h"

Error DeviceMemoryAllocator_allocate(
	DeviceMemoryAllocator *allocator,
	void *requirementsExt,
	Bool cpuSided,
	U32 *blockId,
	U64 *blockOffset,
	EResourceType resourceType,
	CharString objectName
) {

	U64 blockSize = DeviceMemoryBlock_defaultSize;

	if(!allocator || !requirementsExt || !blockId || !blockOffset)
		return Error_nullPointer(
			!allocator ? 0 : (!requirementsExt ? 1 : (!blockId ? 2 : 3)),
			"DeviceMemoryAllocator_allocate()::allocator, requirementsExt, blockId and blockOffset are required"
		);

	GraphicsDevice *device = allocator->device;
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);
	Bool hasReBAR = device->info.capabilities.featuresExt & EDxGraphicsFeatures_ReBAR;
	Bool isGpu = device->info.type == EGraphicsDeviceType_Dedicated;
	Bool forceCpuSided = cpuSided;

	if(!isGpu)				//Force shared allocations if not dedicated
		cpuSided = true;

	DxBlockRequirements req = *(DxBlockRequirements*) requirementsExt;
	Bool isDedicated = req.flags & EDxBlockFlags_IsDedicated;

	//Find an existing allocation

	if (!isDedicated) {

		for(U64 i = 0; i < allocator->blocks.length; ++i) {

			DeviceMemoryBlock *block = &allocator->blocks.ptrNonConst[i];

			if(
				!block->ext ||
				block->isDedicated ||
				(Bool)(block->allocationTypeExt & 1) != !cpuSided ||
				block->typeExt != req.alignment ||						//Alignment is baked into heap
				block->resourceType != resourceType
			)
				continue;

			const U8 *alloc = NULL;
			const Error err = AllocationBuffer_allocateBlockx(&block->allocations, req.length, req.alignment, &alloc);

			if(err.genericError)
				continue;

			*blockId = (U32) i;
			*blockOffset = (U64) alloc;
			return Error_none();
		}
	}

	//Allocate memory

	U64 realBlockSize = (U64_max(blockSize, req.length * 2) + blockSize - 1) / blockSize * blockSize;

	if(isDedicated)
		realBlockSize = req.length;

	DeviceMemoryBlock block = (DeviceMemoryBlock) { 0 };
	CharString temp = CharString_createNull();
	ListU16 temp16 = (ListU16) { 0 };

	D3D12_HEAP_DESC heapDesc = (D3D12_HEAP_DESC) {
		.SizeInBytes = realBlockSize,
		.Properties = (D3D12_HEAP_PROPERTIES) {
			.Type = forceCpuSided ? D3D12_HEAP_TYPE_UPLOAD : (hasReBAR ? D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_DEFAULT)
		},
		.Alignment = req.alignment,
	};

	if (!isGpu) {
		heapDesc.Properties.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	}

	switch(resourceType) {

		case EResourceType_DeviceTexture:
			heapDesc.Flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
			break;

		case EResourceType_RenderTargetOrDepthStencil:
			heapDesc.Flags |= D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
			break;

	case EResourceType_DeviceBuffer:

			heapDesc.Flags |=
				D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS | (!cpuSided || !isGpu ? D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS : 0);

			break;
	}

	ID3D12Heap *heap = NULL;
	Error err = dxCheck(deviceExt->device->lpVtbl->CreateHeap(
		deviceExt->device, &heapDesc, &IID_ID3D12Heap, (void**) &heap
	));

	if(err.genericError)
		return err;

	//Initialize block

	block = (DeviceMemoryBlock) {
		.typeExt = req.alignment,			//Only place things with the same alignment in this block
		.allocationTypeExt = !cpuSided,		//Don't share dedicated and non dedicated allocations
		.isDedicated = isDedicated,
		.ext = heap,
		.resourceType = (U8) resourceType
	};

	gotoIfError(clean, AllocationBuffer_createx(realBlockSize, true, &block.allocations))

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		Log_captureStackTracex(block.stackTrace, sizeof(block.stackTrace) / sizeof(void*), 1);

	//Find a spot in the blocks list

	U64 i = 0;

	for(; i < allocator->blocks.length; ++i)
		if (!allocator->blocks.ptr[i].ext)
			break;

	const U8 *allocLoc = NULL;
	gotoIfError(clean, AllocationBuffer_allocateBlockx(&block.allocations, req.length, req.alignment, &allocLoc))

	if(i == allocator->blocks.length) {

		if(i == U32_MAX)
			gotoIfError(clean, Error_outOfBounds(0, i, U32_MAX, "DeviceMemoryAllocator_allocate() block out of bounds"))

		gotoIfError(clean, ListDeviceMemoryBlock_pushBackx(&allocator->blocks, block))
	}

	else allocator->blocks.ptrNonConst[i] = block;

	*blockId = (U32) i;
	*blockOffset = (U64) allocLoc;

	if(CharString_length(objectName) && (device->flags & EGraphicsDeviceFlags_IsDebug)) {

		gotoIfError(clean, CharString_formatx(
			&temp,
			isDedicated ? "Memory block %"PRIu32" (host: %s, device: %s): %s" :
			"Memory block %"PRIu32" (host: %s, device: %s)",
			(U32) i,
			cpuSided ? "true" : "false",
			hasReBAR || !cpuSided ? "true" : "false",
			objectName.ptr
		))

		gotoIfError(clean, CharString_toUTF16x(temp, &temp16));
		gotoIfError(clean, dxCheck(heap->lpVtbl->SetName(heap, temp16.ptr)))
		CharString_freex(&temp);
	}

clean:

	ListU16_freex(&temp16);
	CharString_freex(&temp);

	if(err.genericError) {
		AllocationBuffer_freex(&block.allocations);
		heap->lpVtbl->Release(heap);
	}

	return err;
}

Bool DeviceMemoryAllocator_freeAllocationExt(GraphicsDevice *device, void *ext) {

	(void)device;

	if(!ext)
		return false;

	((ID3D12Heap*)ext)->lpVtbl->Release((ID3D12Heap*)ext);
	return true;
}