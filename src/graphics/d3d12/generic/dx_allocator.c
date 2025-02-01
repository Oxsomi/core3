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
#include "graphics/generic/allocator.h"
#include "graphics/d3d12/dx_device.h"
#include "graphics/generic/interface.h"
#include "graphics/d3d12/dx_interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "types/base/error.h"
#include "types/math/math.h"
#include "types/container/string.h"

D3D12_HEAP_DESC getDxHeapDesc(GraphicsDevice *device, Bool *cpuSided, U64 alignment) {

	Bool hasReBAR = device->info.capabilities.featuresExt & EDxGraphicsFeatures_ReBAR;
	Bool isGpu = device->info.type == EGraphicsDeviceType_Dedicated;
	Bool forceCpuSided = *cpuSided;

	if(!isGpu || hasReBAR)			//Force shared allocations if not dedicated or if ReBAR is available
		*cpuSided = true;
	
	D3D12_HEAP_DESC heapDesc = (D3D12_HEAP_DESC) {
		.Properties = (D3D12_HEAP_PROPERTIES) {
			.Type = forceCpuSided ? D3D12_HEAP_TYPE_UPLOAD : (hasReBAR ? D3D12_HEAP_TYPE_CUSTOM : D3D12_HEAP_TYPE_DEFAULT),
			.MemoryPoolPreference = isGpu && hasReBAR ? D3D12_MEMORY_POOL_L1 : D3D12_MEMORY_POOL_L0,
			.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE
		},
		.Alignment = alignment,
		.Flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED		//Equal to vulkan behavior, clear manually
	};

	if (isGpu && !hasReBAR) {
		heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	}

	if (!isGpu || hasReBAR)
		heapDesc.Properties.Type = D3D12_HEAP_TYPE_CUSTOM;

	if (
		(device->info.capabilities.featuresExt & EDxGraphicsFeatures_ReallyReportReBARWrites) ==
		EDxGraphicsFeatures_ReallyReportReBARWrites
	)
		heapDesc.Flags |= D3D12_HEAP_FLAG_TOOLS_USE_MANUAL_WRITE_TRACKING;

	return heapDesc;
}

Error DX_WRAP_FUNC(DeviceMemoryAllocator_allocate)(
	DeviceMemoryAllocator *allocator,
	void *requirementsExt,
	Bool cpuSided,
	U32 *blockId,
	U64 *blockOffset,
	EResourceType resourceType,
	CharString objectName,
	DeviceMemoryBlock *resultBlock
) {

	(void) resourceType;

	if(!allocator || !requirementsExt || !blockId || !blockOffset)
		return Error_nullPointer(
			!allocator ? 0 : (!requirementsExt ? 1 : (!blockId ? 2 : 3)),
			"D3D12DeviceMemoryAllocator_allocate()::allocator, requirementsExt, blockId and blockOffset are required"
		);

	GraphicsDevice *device = allocator->device;
	DxGraphicsDevice *deviceExt = GraphicsDevice_ext(device, Dx);

	Bool hasReBAR = device->info.capabilities.featuresExt & EDxGraphicsFeatures_ReBAR;

	DxBlockRequirements req = *(DxBlockRequirements*) requirementsExt;
	D3D12_HEAP_DESC heapDesc = getDxHeapDesc(device, &cpuSided, req.alignment);

	U64 maxAllocationSize = device->info.capabilities.maxAllocationSize;

	if(req.length > maxAllocationSize)
		return Error_outOfBounds(
			2, req.length, maxAllocationSize,
			"D3D12DeviceMemoryAllocator_allocate() allocation length exceeds max allocation size"
		);

	//We lock this early to avoid other mem alloc from allocating too many memory blocks at once.
	//Maybe what we end up allocating now can be used for the next.

	ELockAcquire acq = SpinLock_lock(&allocator->lock, U64_MAX);

	CharString temp = CharString_createNull();
	ListU16 temp16 = (ListU16) { 0 };

	ID3D12Heap *heap = NULL;
	Error err = Error_none();

	//Find an existing allocation

	for(U64 i = 0; i < allocator->blocks.length; ++i) {

		DeviceMemoryBlock *block = &allocator->blocks.ptrNonConst[i];

		if(
			!block->ext ||
			block->isDedicated ||
			!!(block->allocationTypeExt & 1) != !cpuSided ||
			block->typeExt != req.alignment						//Alignment is baked into heap
		)
			continue;

		const U8 *alloc = NULL;
		const Error err1 = AllocationBuffer_allocateBlockx(&block->allocations, req.length, req.alignment, false, &alloc);

		if(err1.genericError)
			continue;

		if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
			Log_debugLnx(
				"-- Graphics: Allocating into existing memory block "
				"(%"PRIu64" from allocation of size %"PRIu64" at offset %"PRIx64" and alignment %"PRIu32")",
				i,
				req.length,
				(U64) alloc,
				req.alignment
			);

		*blockId = (U32) i;
		*blockOffset = (U64) alloc;
		*resultBlock = *block;
		goto clean;
	}

	//Allocate memory

	U64 blockSize = cpuSided ? device->blockSizeCpu : device->blockSizeGpu;
	U64 realBlockSize = U64_min(
		(U64_max(blockSize, req.length * 2) + blockSize - 1) / blockSize * blockSize,
		maxAllocationSize
	);

	heapDesc.SizeInBytes = realBlockSize;
	
	U64 usedMem = DX_WRAP_FUNC(GraphicsDevice_getMemoryBudget)(allocator->device, !cpuSided);
	U64 maxAlloc = 
		cpuSided ? allocator->device->info.capabilities.sharedMemory :
		allocator->device->info.capabilities.dedicatedMemory;

	if(usedMem != U64_MAX && usedMem + heapDesc.SizeInBytes > maxAlloc)
		gotoIfError(clean, Error_outOfMemory(0, "Memory block allocation would exceed available memory"))

	if(allocator->device->flags & EGraphicsDeviceFlags_IsDebug)
		Log_debugLnx(
			"-- Graphics: Allocating new memory block (%"PRIu64" with size %"PRIu64" from allocation with size %"PRIu64")\n"
			"\t%s (Available memory: %"PRIu64")",
			allocator->blocks.length,
			realBlockSize,
			req.length,
			cpuSided ? "Cpu sided allocation" : "Gpu sided allocation",
			maxAlloc - usedMem
		);
	
	gotoIfError(clean, dxCheck(deviceExt->device->lpVtbl->CreateHeap(
		deviceExt->device, &heapDesc, &IID_ID3D12Heap, (void**) &heap
	)))

	//Initialize block

	DeviceMemoryBlock block = (DeviceMemoryBlock) {
		.isActive = true,
		.typeExt = req.alignment,			//Only place things with the same alignment in this block
		.allocationTypeExt = !cpuSided,		//Don't share dedicated and non dedicated allocations
		.isDedicated = false,
		.ext = heap
	};

	gotoIfError(clean, AllocationBuffer_createx(realBlockSize, true, 0, &block.allocations))

	if(device->flags & EGraphicsDeviceFlags_IsDebug)
		Log_captureStackTracex(block.stackTrace, sizeof(block.stackTrace) / sizeof(void*), 1);

	//Find a spot in the blocks list

	U64 i = 0;

	for(; i < allocator->blocks.length; ++i)
		if (!allocator->blocks.ptr[i].isActive)
			break;

	const U8 *allocLoc = NULL;
	gotoIfError(clean, AllocationBuffer_allocateBlockx(&block.allocations, req.length, req.alignment, false, &allocLoc))

	if(i == allocator->blocks.length) {

		if(i == U32_MAX)
			gotoIfError(clean, Error_outOfBounds(0, i, U32_MAX, "D3D12DeviceMemoryAllocator_allocate() block out of bounds"))

		gotoIfError(clean, ListDeviceMemoryBlock_pushBackx(&allocator->blocks, block))
	}

	else allocator->blocks.ptrNonConst[i] = block;

	*blockId = (U32) i;
	*blockOffset = (U64) allocLoc;
	*resultBlock = block;

	if(CharString_length(objectName) && (device->flags & EGraphicsDeviceFlags_IsDebug)) {

		gotoIfError(clean, CharString_formatx(
			&temp,
			"Memory block %"PRIu32" (host: %s, device: %s)",
			(U32) i,
			cpuSided ? "true" : "false",
			hasReBAR || !cpuSided ? "true" : "false"
		))

		gotoIfError(clean, CharString_toUTF16x(temp, &temp16));
		gotoIfError(clean, dxCheck(heap->lpVtbl->SetName(heap, temp16.ptr)))
		CharString_freex(&temp);
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&allocator->lock);

	ListU16_freex(&temp16);
	CharString_freex(&temp);

	if(err.genericError) {

		AllocationBuffer_freex(&block.allocations);

		if(heap)
			heap->lpVtbl->Release(heap);
	}

	return err;
}

Bool DX_WRAP_FUNC(DeviceMemoryAllocator_freeAllocation)(GraphicsDevice *device, void *ext) {

	(void)device;

	if(!ext)
		return true;

	((ID3D12Heap*)ext)->lpVtbl->Release((ID3D12Heap*)ext);
	return true;
}
