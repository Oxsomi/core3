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
#include "graphics/d3d12/direct3d12.h"
#include "graphics/generic/command_list.h"
#include "types/container/list.h"
#include "types/math/vec.h"

#ifdef __cplusplus
	extern "C" {
#endif

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

typedef struct D3D12DispatchRaysIndirect {		//Intermediate, this one is created from a more sparse version
	D3D12_DISPATCH_RAYS_DESC desc;
	U32 padding[5];
} D3D12DispatchRaysIndirect;

typedef struct DxCommandQueue {

	ID3D12CommandQueue *queue;

	EDxCommandQueue type;
	U32 resolvedQueueId;

} DxCommandQueue;

typedef enum ECPUDescriptorHeapType {
	ECPUDescriptorHeapType_DSV,			//Depth stencils
	ECPUDescriptorHeapType_RTV,			//Render targets
	ECPUDescriptorHeapType_Count
} ECPUDescriptorHeapType;

typedef ID3D12GraphicsCommandList10 DxCommandBuffer;

typedef struct DxCommandAllocator {
	ID3D12CommandAllocator *pool;
	DxCommandBuffer *cmd;
} DxCommandAllocator;

TList(DxCommandAllocator);
TListNamed(ID3D12Fence*, ListID3D12Fence);

typedef struct DxDescriptorHeapSingle {

	ID3D12DescriptorHeap *heap;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;

	U64 cpuIncrement, gpuIncrement;

	U64 padding;

} DxDescriptorHeapSingle;

typedef struct DxDescriptorHeap {
	DxDescriptorHeapSingle samplerHeap, resourcesHeap;
} DxDescriptorHeap;

TList(D3D12_DESCRIPTOR_RANGE1);

//DxDescriptorLayout is no real DX object, only root signature is.
//But by abstracting it like this we map more closely to Vk while also allowing splitting of root signature and desc layout.
// And reducing unnecessary conversions of ListDescriptorBinding -> DxDescriptorLayout.
//For example; we might make the same root sig multiple times but with different root constants or IA/streamout flags.
typedef struct DxDescriptorLayout {
	ListD3D12_DESCRIPTOR_RANGE1 rangesResources;
	ListD3D12_DESCRIPTOR_RANGE1 rangesSamplers;
	ListU32 bindingOffsets;
} DxDescriptorLayout;

typedef struct DxDescriptorTable {
	U64 padding[2];				//TODO:
} DxDescriptorTable;

typedef enum EExecuteIndirectCommand {
	EExecuteIndirectCommand_Dispatch,
	EExecuteIndirectCommand_DispatchRays,
	EExecuteIndirectCommand_DrawIndexed,
	EExecuteIndirectCommand_Draw,
	EExecuteIndirectCommand_Count
} EExecuteIndirectCommand;

typedef struct DxGraphicsDevice {

	ID3D12Device10 *device;
	ID3D12DebugDevice *debugDevice;
	ID3D12InfoQueue1 *infoQueue1;
	ID3D12InfoQueue1 *infoQueue0;

	ID3D12DeviceConfiguration1 *deviceConfig;

	DxCommandQueue queues[EDxCommandQueue_Count];		//Don't have to be unique queues! Indexed by EVkCommandQueue

	//3D as 1D flat List: resolvedQueueId + (backBufferId * threadCount + threadId) * resolvedQueues
	ListDxCommandAllocator commandPools;

	ID3D12Fence *commitSemaphore;

	ID3D12CommandSignature *commandSigs[EExecuteIndirectCommand_Count];

	DxDescriptorHeapSingle cpuHeaps[ECPUDescriptorHeapType_Count];

	IDXGIAdapter4 *adapter4;

	//Temporary storage for submit time stuff

	ListD3D12_BUFFER_BARRIER bufferTransitions;
	ListD3D12_TEXTURE_BARRIER imageTransitions;

	U64 fenceId;

} DxGraphicsDevice;

typedef struct DxCommandBufferState {

	RefPtr *tempPipelines[EPipelineType_Count];		//Pipelines that were set via command, but not bound yet
	RefPtr *pipeline;

	ImageAndRange boundTargets[9];					//All 8 RTVs and DSV
	ImageAndRange resolveTargets[9];				//Dst MSAA targets

	F32x4 blendConstants, tempBlendConstants;

	U8 stencilRef, tempStencilRef;
	U8 boundPrimitiveTopology;
	U8 inRender;

	U32 scopeCounter;

	DxCommandBuffer *buffer;

	SetPrimitiveBuffersCmd boundBuffers, tempBoundBuffers;

	D3D12_VIEWPORT boundViewport, tempViewport;
	D3D12_RECT boundScissor, tempScissor;

} DxCommandBufferState;

Error DxGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, DxCommandBufferState *commandBuffer);

Error DxGraphicsDevice_createDescriptorHeapSingle(
	DxGraphicsDevice *deviceExt,
	D3D12_DESCRIPTOR_HEAP_DESC desc,
	CharString *name,
	DxDescriptorHeapSingle *heap,
	Bool reqGpuHandle
);

#ifdef __cplusplus
	}
#endif
