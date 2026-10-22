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

//graphics/d3d12/dx_device.h

#pragma once
#include "graphics/d3d12/direct3d12.h"
#include "graphics/d3d12/dx_amd_shader_analyzer.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/device.h"
#include "types/container/list.h"
#include "types/container/list_predeclare.h"
#include "types/container/allocation_buffer.h"
#include "types/math/vec4.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr PipelineRef;

//Special features that are only important for implementation, but we do want to be cached.

typedef enum EDxCommandQueue {

	EDxCommandQueue_Copy,                     //Queue for dedicated host -> device copies
	EDxCommandQueue_Compute,
	EDxCommandQueue_Graphics,

	//EDxCommandQueue_VideoDecode,            //TODO:
	//EDxCommandQueue_VideoEncode

	EDxCommandQueue_Count

} EDxCommandQueue;

#define GRAPHICS_DISPATCH_RAYS_INDIRECT_ARGS 64           //Initial ExecuteIndirect(DispatchRays) slots; grows to fit

typedef struct D3D12DispatchRaysIndirect {    //Intermediate, this one is created from a more sparse version
	D3D12_DISPATCH_RAYS_DESC desc;
	U32 padding[5];
} D3D12DispatchRaysIndirect;

typedef struct DxCommandQueue {

	ID3D12CommandQueue *queue;

	EDxCommandQueue type;
	U32 resolvedQueueId;

} DxCommandQueue;

typedef enum ECPUDescriptorHeapType {
	ECPUDescriptorHeapType_DSV,               //Depth stencils
	ECPUDescriptorHeapType_RTV,               //Render targets
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
	AllocationBuffer allocators[2];
	SpinLock locks[2];

	//A push descriptor naming a texture needs a shader visible slot, and it has to be in THIS heap: D3D12
	// binds one CBV/SRV/UAV heap at a time and swapping would drop the bindless and table state the draw
	// still needs (see DescriptorHeapInfo::maxPushDescriptors).
	//The ring sits past the declared maxima so allocators[0] never hands any of it out,
	// and each frame in flight owns a window that resets when that frame comes back around.

	//pushRingFrame is the frame in flight the offset belongs to, so the window resets exactly once when
	// that frame comes back around rather than on every bind.

	U32 pushRingBase, pushRingPerFrame, pushRingOffset;
	U32 pushRingFrame;

} DxDescriptorHeap;

TList(D3D12_DESCRIPTOR_RANGE1);
TList(D3D12_ROOT_PARAMETER1);

//DxDescriptorLayout is no real DX object, only root signature is.
//But by abstracting it like this we map more closely to Vk while also allowing splitting of root signature and desc layout.
//And reducing unnecessary conversions of ListDescriptorBinding -> DxDescriptorLayout.
//For example; we might make the same root sig multiple times but with different root constants or IA/streamout flags.
typedef struct DxDescriptorLayout {
	ListD3D12_DESCRIPTOR_RANGE1 rangesResources;
	ListD3D12_DESCRIPTOR_RANGE1 rangesSamplers;
	ListU32 bindingOffsets;
	ListU8 rootParamOffsets;
	ListD3D12_ROOT_PARAMETER1 rootParams;
	U8 padding[8];
} DxDescriptorLayout;

//Root parameters in this order: push constants, push descriptors, then the binding tables, so what changes per draw or
// dispatch lands in the first DWORDs, the ones drivers keep in fast memory. Each field is where its group starts.

typedef struct DxPipelineLayout {
	ID3D12RootSignature *rootSig;
	U32 rootParamPushDescriptors;
	U32 rootParamPushConstants;
	U32 rootParamBindings;
	U32 padding[3];
} DxPipelineLayout;

typedef struct DxDescriptorTable {
	U64 allocationLocations[2];        //Resources, samplers
	U64 allocationSizes[2];            //Resources, samplers
} DxDescriptorTable;

Bool DxDescriptorHeap_freeTable(DxDescriptorHeap *heapExt, DxDescriptorTable *table);

//Bump allocates transient shader visible slots from the heap's push ring; never binds the heap itself.

Bool DxDescriptorHeap_allocTransient(DxDescriptorHeap *heapExt, U32 fifId, U32 count, U32 *slot);
Bool DxDescriptorHeap_allocTable(DxDescriptorHeap *heapExt, DxDescriptorTable *table, const Allocator *alloc, Error *e_rr);

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

	DxCommandQueue queues[EDxCommandQueue_Count];        //Don't have to be unique queues! Indexed by EVkCommandQueue

	//3D as 1D flat List: resolvedQueueId + (backBufferId * threadCount + threadId) * resolvedQueues
	ListDxCommandAllocator commandPools;

	ID3D12Fence *commitSemaphore;

	ID3D12CommandSignature *commandSigs[EExecuteIndirectCommand_Count];

	DxDescriptorHeapSingle cpuHeaps[ECPUDescriptorHeapType_Count];

	IDXGIAdapter4 *adapter4;

	//Compacted size storage, indexed by the slot the shared allocator hands out. D3D12 has no query pool
	//for this, and EmitRaytracingAccelerationStructurePostbuildInfo cannot target a readback heap (it needs
	//UNORDERED_ACCESS), so a size is emitted into the emit pool and copied into the same slot of the CPU
	//readable one.

	#define DX_COMPACTION_QUERIES_BASE 256                      //First pool; each one after it doubles

	ListRefPtr compactionEmitPools;
	ListRefPtr compactionReadbackPools;

	//Temporary storage for submit time stuff

	ListD3D12_BUFFER_BARRIER bufferTransitions;
	ListD3D12_TEXTURE_BARRIER imageTransitions;

	//Values of commitSemaphore, one per execute, so a submit split by a flush takes several.
	//fenceId is the last value queued to signal; it only advances once the queue accepted the Signal, so a wait on it
	// always ends.
	//slotFenceValue[i] is the value whose completion frees frame in flight i, 0 while nothing ran there.

	U64 fenceId;
	U64 slotFenceValue[MAX_FRAMES_IN_FLIGHT];

	//Timing (EGraphicsFeatures2_Timestamps): one query heap and one readback buffer per frame in flight, read a
	// frame later once its fence has signalled. timestampPeriod is nanoseconds per tick, timestampCursor the
	// transient next slot while an op walk records.

	ID3D12QueryHeap *timestampHeap[MAX_FRAMES_IN_FLIGHT];
	DeviceBufferRef *timestampReadback[MAX_FRAMES_IN_FLIGHT];   //Readback DeviceBuffer the query results resolve into
	DeviceBufferRef *dispatchRaysIndirect[MAX_FRAMES_IN_FLIGHT];   //ExecuteIndirect args, an Indirect DeviceBuffer
	DeviceBufferRef *dispatchRaysIndirectStaging[MAX_FRAMES_IN_FLIGHT];   //SBT upload staging, only when WBI is absent
	U32 timestampCapacity[MAX_FRAMES_IN_FLIGHT];
	F32 timestampPeriod;
	U32 timestampCursor;
	U32 dispatchRaysIndirectCursor;                //Argument slots used this frame; reset each submit

	U64 padding;

	//AMD's driver extension, the only route to a compiled shader on D3D12; zeroed on any other driver.

	DxAmdShaderAnalyzer amdAnalyzer;

	//Breadcrumbs (GraphicsDevice::breadcrumbs): memory the process allocated (VirtualAlloc), opened as a heap with
	// OpenExistingHeapFromAddress so it stays readable after the device is gone, and a buffer over it the scopes write.

	void *breadcrumbMemory;
	ID3D12Heap *breadcrumbHeap;
	ID3D12Resource *breadcrumbBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS breadcrumbAddress;

	//Pipeline cache: the library every PSO is stored in under its key, and the blob it was opened from, which it reads
	// for as long as it lives. NULL where the driver has no pipeline libraries.

	ID3D12PipelineLibrary *pipelineLibrary;
	U64 pipelineLibraryStored;                      //PSOs stored this run; a loaded blob would discard them
	Buffer pipelineLibraryData;

} DxGraphicsDevice;

typedef struct GraphicsInstance GraphicsInstance;

//Drains the info queue's stored validation messages into the log, so a failed HRESULT names the real complaint.
//No op with infoQueue1 present (Win11), since the message callback already printed (and counted) everything live.
//The drained messages count into instance's validation counters when instance is non NULL.
void DxGraphicsDevice_logDebugMessages(
	const DxGraphicsDevice *deviceExt, GraphicsInstance *instance, const Allocator *alloc
);

typedef struct DxCommandBufferState {

	DxCommandBuffer *buffer;

	RefPtr *tempPipelines[EPipelineType_Count];       //Pipelines that were set via command, but not bound yet
	RefPtr *pipeline;

	//Bindful: heap and table state set by the bind commands, emitted lazily at the work ops.
	//The heap bind is EXPLICIT because switching heaps can stall the GPU on some hardware; lastBoundHeap
	// tracks what the command buffer really has so the switch only ever goes out when it changed.
	//defaultDescriptorsBound starts false: the default (bindless) heap, root signature and table only bind at
	// the first work op that runs a default layout pipeline, so a purely bindful frame never pays for them.

	RefPtr *boundDescriptorTable;
	RefPtr *boundDescriptorHeap;
	RefPtr *lastBoundHeap;
	RefPtr *lastBoundTable[2];                        //Per bind point: [0] = graphics, [1] = compute
	ID3D12RootSignature *lastRootSig[2];

	//Push constants and push descriptors are root arguments, so a root signature switch drops them and each
	// bind point needs its own re-emit; that is what the Emitted pairs track.

	U8 pushConstantData[128];

	U8 pushConstantSize;
	U8 pushConstantsEmitted[2];                       //Per bind point: [0] = graphics, [1] = compute
	U8 pushDescriptorCount;
	U8 pushDescriptorsEmitted[2];                     //Per bind point: [0] = graphics, [1] = compute
	Bool defaultDescriptorsBound;
	U8 padding0;

	Descriptor pushDescriptors[OXC3_MAX_PUSH_DESCRIPTORS];

	//Scopes

	U32 breadcrumbSlot;                               //Of the open scope, U32_MAX without one
	U16 scopeCounter;
	U8 curScopeFlags;                                 //ECommandScopeInternalFlags of the open scope
	U8 padding1;

	//Render state

	ImageAndRange boundTargets[9];                    //All 8 RTVs and DSV
	ImageAndRange resolveTargets[9];                  //Dst MSAA targets
	U8 resolveModes[9];                               //EMSAAResolveMode each of the above resolves with
	U8 anyResolve;
	U8 inRender;
	U8 colorCount;                                    //If inRender, how many colors are bound (upper mask = has depth)
	U8 stencilRef, tempStencilRef;
	U8 boundPrimitiveTopology;
	U8 padding2;

	F32x4 blendConstants, tempBlendConstants;

	I32x2 size;                                       //If inRender,    defines current size
	I32x2 offset;                                     //^               defines offset

	SetPrimitiveBuffersCmd boundBuffers, tempBoundBuffers;

	D3D12_VIEWPORT boundViewport, tempViewport;
	D3D12_RECT boundScissor, tempScissor;

} DxCommandBufferState;

Bool DxGraphicsDevice_flush(GraphicsDeviceRef *deviceRef, DxCommandBufferState *commandBuffer, Error *e_rr);

//One fence value rather than everything submitted, so a caller needing only its own frame back does not stall
//the rest. A value already passed, 0 included, returns without waiting.

Bool DxGraphicsDevice_waitFence(GraphicsDeviceRef *deviceRef, U64 fenceId, Error *e_rr);

//Until the submit with this id (GraphicsDevice::submitId as it was during that submit) is done; 0 returns at once.
//It may also wait for a later submit that reused the same frame in flight.

Bool DxGraphicsDevice_waitSubmit(GraphicsDeviceRef *deviceRef, U64 submitId, Error *e_rr);

//Bytes in use in the local or the non local segment, and the OS budget for it (U64_MAX when either is unknown).
U64 DxGraphicsDevice_getMemoryUsage(GraphicsDevice *device, Bool isDeviceLocal, U64 *budget);

//Counts a PSO's code in the memory stats: the driver's cached blob, else the bytecode it was created from as an estimate
typedef struct Pipeline Pipeline;
void DxPipeline_trackMemory(Pipeline *pipeline, ID3D12PipelineState *pso, U64 bytecodeLength);

//Builds a compute or graphics PSO through the device's pipeline library: loaded under key when the library has it,
// else created and stored there. Counts its code in the memory stats (DxPipeline_trackMemory).

Bool DxGraphicsDevice_buildPipeline(
	GraphicsDevice *device,
	Pipeline *pipeline,
	const void *desc,                    //D3D12_COMPUTE or D3D12_GRAPHICS_PIPELINE_STATE_DESC, by pipeline->type
	U64 key,
	U64 irBytes,
	ID3D12PipelineState **result,
	Error *e_rr
);

//Residency priority of a heap or committed resource: CPU side memory (staging) is paged out first, high last.
void DxGraphicsDevice_setResidencyPriority(DxGraphicsDevice *deviceExt, ID3D12Pageable *object, Bool cpuSide, Bool high);

//A breadcrumb (GraphicsDevice::breadcrumbs): a slot set to value once the GPU began (MARKER_IN) or finished (MARKER_OUT)
// what it covers. Nothing for U32_MAX.

void DxGraphicsDevice_breadcrumb(
	const GraphicsDevice *device,
	const DxGraphicsDevice *deviceExt,
	DxCommandBuffer *buffer,
	U32 slot,
	U32 value,
	D3D12_WRITEBUFFERIMMEDIATE_MODE mode
);

//Builds a texture's SRV or UAV at dst.
//Shared so a descriptor table and a texture push descriptor's single entry table cannot end up disagreeing
// about a dimension, a mip range or a depth plane's format.

void DxDescriptorTable_writeTexture(
	DxGraphicsDevice *deviceExt,
	const Descriptor *d,
	EGfxRegisterType registerType,
	D3D12_CPU_DESCRIPTOR_HANDLE dst
);

D3D12_SHADER_VISIBILITY DxDescriptorLayout_convertVisibility(U32 visibility);

//A sampler is described the same way whether it lands in a sampler heap or is baked into a root signature.

D3D12_FILTER DxSampler_toFilter(SamplerInfo sinfo);
D3D12_STATIC_SAMPLER_DESC DxSampler_toStaticDesc(SamplerInfo sinfo, GfxBinding binding, U32 visibility);

Bool DxGraphicsDevice_createDescriptorHeapSingle(
	DxGraphicsDevice *deviceExt,
	D3D12_DESCRIPTOR_HEAP_DESC desc,
	CharString *name,
	DxDescriptorHeapSingle *heap,
	Bool reqGpuHandle,
	const Allocator *alloc,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
