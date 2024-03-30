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
#include "graphics/directx12/directx12.h"
#include "graphics/generic/command_list.h"
#include "types/list.h"
#include "types/vec.h"

typedef RefPtr PipelineRef;

//Special features that are only important for implementation, but we do want to be cached.

typedef enum EDxCommandQueue {

	EDxCommandQueue_Copy,					//Queue for dedicated host -> device copies
	EDxCommandQueue_Compute,
	EDxCommandQueue_Graphics,

	//EDxCommandQueue_VideoDecode,			//TODO:
	//EDxCommandQueue_VideoEncode

	EDxCommandQueue_Count

} EDxCommandQueue;

typedef struct DxCommandQueue {

	ID3D12CommandQueue *queue;

	EDxCommandQueue type;
	U32 pad;

} DxCommandQueue;

typedef enum EDescriptorHeapType {
	EDescriptorHeapType_Sampler,		//Sampler heap
	EDescriptorHeapType_Resources,		//SRV UAV heap
	EDescriptorHeapType_DSV,			//Depth stencils
	EDescriptorHeapType_RTV,			//Render targets
	EDescriptorHeapType_Count
} EDescriptorHeapType;

typedef ID3D12GraphicsCommandList10 DxCommandBuffer;

typedef struct DxCommandAllocator {
	ID3D12CommandAllocator *pool;
	DxCommandBuffer *cmd;
} DxCommandAllocator;

TList(DxCommandAllocator);
TListNamed(ID3D12Fence*, ListID3D12Fence);

typedef struct DxHeap {

	ID3D12DescriptorHeap *heap;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

	U64 cpuIncrement, gpuIncrement;

} DxHeap;

typedef enum EExecuteIndirectCommand {
	EExecuteIndirectCommand_Dispatch,
	EExecuteIndirectCommand_DispatchRays,
	EExecuteIndirectCommand_DrawIndexedCount,
	EExecuteIndirectCommand_DrawCount,
	EExecuteIndirectCommand_DrawIndexed,
	EExecuteIndirectCommand_Draw,
	EExecuteIndirectCommand_Count
} EExecuteIndirectCommand;

typedef struct DxGraphicsDevice {

	ID3D12Device5 *device;
	DxCommandQueue queues[EDxCommandQueue_Count];		//Don't have to be unique queues! Indexed by EVkCommandQueue

	//3D as 1D flat List: resolvedQueueId + (backBufferId * threadCount + threadId) * resolvedQueues
	ListDxCommandAllocator commandPools;

	ListID3D12Fence submitSemaphores;
	ListID3D12Fence commitSemaphore;

	ID3D12CommandSignature *commandSigs[EExecuteIndirectCommand_Count];

	DxHeap *heaps[EDescriptorHeapType_Count];
	ID3D12RootSignature *defaultLayout;								//Default layout if push constants aren't present

	//Temporary storage for submit time stuff

	ListD3D12_BUFFER_BARRIER bufferTransitions;
	ListD3D12_TEXTURE_BARRIER imageTransitions;

} DxGraphicsDevice;

typedef struct DxCommandBufferState {

	RefPtr *tempPipelines[EPipelineType_Count];		//Pipelines that were set via command, but not bound yet
	RefPtr *pipeline;

	ImageAndRange boundTargets[9];					//All 8 RTVs and DSV
	ImageAndRange resolveTargets[9];				//Dst MSAA targets

	F32x4 blendConstants, tempBlendConstants;

	U8 stencilRef, tempStencilRef;
	U8 tempPrimitiveTopology;
	U8 padding;

	U32 scopeCounter;

	DxCommandBuffer *buffer;

	SetPrimitiveBuffersCmd boundBuffers, tempBoundBuffers;

	D3D12_VIEWPORT boundViewport, tempViewport;
	D3D12_RECT boundScissor, tempScissor;

} DxCommandBufferState;

DxCommandAllocator *VkGraphicsDevice_getCommandAllocator(DxGraphicsDevice *device, U32 queueId, U64 threadId, U8 backBufferId);

Error DxDeviceMemoryAllocator_findMemory(
	DxGraphicsDevice *deviceExt,
	Bool cpuSided,
	U32 memoryBits,
	U32 *memoryId,
	U64 *size
);

Error DxGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, DxCommandBuffer *commandBuffer);
