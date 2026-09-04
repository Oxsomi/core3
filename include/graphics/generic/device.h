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

//graphics/generic/device.h

#pragma once
#include "graphics/generic/device_info.h"
#include "graphics/generic/device_allocator.h"
#include "graphics/generic/resource.h"
#include "types/container/ref_ptr.h"
#include "types/container/list.h"
#include "types/container/list_basic_types.h"
#include "types/container/string.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsInstanceRef;
typedef struct Allocator Allocator;
typedef struct GraphicsObjectTypes GraphicsObjectTypes;
typedef struct DescriptorLayoutInfo DescriptorLayoutInfo;
typedef RefPtr DeviceBufferRef;
typedef RefPtr PipelineRef;
typedef RefPtr DescriptorLayoutRef;
typedef RefPtr PipelineLayoutRef;
typedef RefPtr DescriptorHeapRef;
typedef RefPtr DescriptorTableRef;
typedef enum EGfxBinaryType EGfxBinaryType;

typedef struct CBufferData {        //TODO: Replace this entirely when we can.

	U32 frameId;                    //Can loop back to 0 after U32_MAX!
	F32 time;                       //Time since launch of app
	F32 deltaTime;                  //deltaTime since last frame.
	U32 swapchainCount;             //How many swapchains are present

	U32 swapchains[2 * 16];

} CBufferData;

TListNamed(SpinLock*, ListSpinLockPtr);

typedef enum EGraphicsDeviceFlags {
	EGraphicsDeviceFlags_None            = 0,
	EGraphicsDeviceFlags_IsVerbose       = 1 << 0,    //Device creation is verbose
	EGraphicsDeviceFlags_IsDebug         = 1 << 1,    //Debug features such as API/RT validation, debug marker/names
	EGraphicsDeviceFlags_DisableRt       = 1 << 2,    //Don't allow raytracing to be enabled (might reduce driver overhead)
	EGraphicsDeviceFlags_DisableDebug    = 1 << 3,    //Force disable debugging even on debug mode. NDEBUG is leading otherwise
	EGraphicsDeviceFlags_DisableBindless = 1 << 4,    //No bindless layout, even where the device supports it

	//Adds the bindless _samplers[] array to the default layout.
	//Opt in because that array owns a descriptor set to itself on Vulkan (set 0, while every resource
	// array shares set 1) and forces a sampler heap on both backends, and a shader only needs it to index
	// samplers dynamically.
	//Static samplers cover the ordinary case at no cost; see DescriptorLayoutInfo::immutableSamplers.

	EGraphicsDeviceFlags_EnableDynamicSamplers = 1 << 5

} EGraphicsDeviceFlags;

typedef enum EGraphicsBufferingMode {
	EGraphicsBufferingMode_Default,      //Defaults to device preferred; e.g. 2 on mobile, 3 on desktop
	EGraphicsBufferingMode_Default2,
	EGraphicsBufferingMode_Double,       //2 frames in flight (less latency, less memory usage)
	EGraphicsBufferingMode_Triple        //3 frames in flight (more latency, but more performant)
} EGraphicsBufferingMode;

#define MAX_FRAMES_IN_FLIGHT 3           //Don't touch
#define GRAPHICS_TIMESTAMP_QUERIES 2048          //Initial per frame-in-flight timestamp capacity; the pool grows past it
#define GRAPHICS_TIMESTAMP_QUERIES_MAX (1u << 20)  //Ceiling; a frame needing more is a runaway, so its timing is skipped

//One time runtime hints, so a message fires once per device rather than on every call that notices the
// same thing.
//A bit set in GraphicsDevice::runtimeMessages means the message was already logged;
// GraphicsDevice_logOnce is the test and set that guards the log call.

typedef enum EGraphicsDeviceMessage {

	//A real micromap object was linked into a BLAS on a device that likely emulates opacity micromaps
	// (RayMicromapOpacityActual unset), where the free special indices are usually the better tool.

	EGraphicsDeviceMessage_OmmLikelyEmulated    = 1 << 0,

	//A submit was split mid recording because pending copies or AS builds crossed the flush threshold,
	// which inserts a GPU sync point; frequent hits want a higher flushThreshold or smaller upload batches.

	EGraphicsDeviceMessage_SubmitFlushed        = 1 << 1,

	//A pipeline layout exceeded 13 root signature DWORDs, past which D3D12 drivers typically spill to memory.

	EGraphicsDeviceMessage_RootSignature13Dwords = 1 << 2,

	//An allocation preferring a dedicated block fell back to shared because the device already holds >= 2000
	// memory blocks (the cap guards the API limit of 4096 allocations).

	EGraphicsDeviceMessage_TooManyMemoryBlocks  = 1 << 3

} EGraphicsDeviceMessage;

//The GPU time of one timestamp result, keyed by both a caller id and a name so a hot path reads it back by id
// without comparing strings while a casual caller uses the name. gpuNs is a region's delta or a point's absolute
// tick time, and is 0 when the query did not resolve.

typedef struct GraphicsTiming {
	U32 id;
	U32 padding;
	U64 gpuNs;
	CharString name;
} GraphicsTiming;

TList(GraphicsTiming);

//What a submit records per timestamp so a result can be paired with its name and id once the frame completes.
// nameIndex points into that frame's owned timingNames, or U32_MAX for none; endSlot is U32_MAX for a point sample.

typedef struct TimingEntry {
	U32 id;
	U32 nameIndex;
	U32 beginSlot;
	U32 endSlot;
} TimingEntry;

TList(TimingEntry);

typedef struct GraphicsDevice {

	GraphicsInstanceRef *instance;

	GraphicsDeviceInfo info;

	U64 submitId;

	//Highest submit known to have COMPLETED on the device. Work that has to read back something a submit
	//produced, such as a compacted acceleration structure size, tests against this instead of blocking.

	U64 completedSubmitId;

	EGraphicsDeviceFlags flags;
	U16 pad0;
	U8 framesInFlight;
	U8 fifId;                //(submitId - 1) % FRAMES_IN_FLIGHT

	AtomicI64 runtimeMessages;                              //EGraphicsDeviceMessage bits that already logged

	Ns lastSubmit;

	Ns firstSubmit;                                         //Start of time

	ListWeakRefPtr pendingResources;                        //Resources pending copy from CPU to device next submit

	ListRefPtr resourcesInFlight[MAX_FRAMES_IN_FLIGHT];     //Resources in flight, TODO: HashMap

	//Compacted size slots, handed out per BLAS built with AllowCompaction and returned when the compaction
	//that reads one consumes it. Only the storage differs per backend, a query pool on Vulkan and a pair of
	//buffers on D3D12, so the bookkeeping is shared: without it the two would drift.
	//
	//Slots are recycled rather than only appended, so the high water mark tracks how many structures await
	//compaction AT ONCE and not how many a session compacts.

	ListU32 compactionFreeQueries;
	U32 compactionQueryCount;
	U32 compactionPadding;

	//Every live TLAS, WITHOUT holding a reference: a compaction walks this to find the structures that
	//resolved the address it is about to move. A TLAS adds itself on create and removes itself on free,
	//and that pairing is the whole contract, since a stale entry is a dangling read.
	//
	//Guarded by this device's lock, never nested with an RTAS lock in either direction.

	ListRefPtr liveTlases;

	SpinLock lock;                                          //Lock for submission and marking resources dirty

	DeviceMemoryAllocator allocator;

	//Staging allocations and buffers that are used to transmit/receive data from the device

	DeviceBufferRef *staging;                               //Staging buffer split by FRAMES_IN_FLIGHT
	AllocationBuffer stagingAllocations[MAX_FRAMES_IN_FLIGHT];

	//Readback (pullRegion) twin of the staging buffer, lazily created on the first pull.
	//Pulls are recorded after the frame's commands, land in readback memory and are copied to cpuData plus
	// reported through their callback once the frame provably completed on the device.

	DeviceBufferRef *stagingReadback;
	AllocationBuffer stagingReadbackAllocations[MAX_FRAMES_IN_FLIGHT];

	ListDevicePendingPull pendingPulls;                     //Requested but not yet recorded
	ListDevicePendingPull pullsInFlight[MAX_FRAMES_IN_FLIGHT];

	//Graphics constants (globals) accessible by all shaders

	DeviceBufferRef *frameData[MAX_FRAMES_IN_FLIGHT];

	//Timing (EGraphicsFeatures2_Timestamps): per frame in flight, the owned name copies and the descriptors of the
	// timestamps that frame recorded, kept from submit until that frame recycles, plus the resolved results of the
	// most recently completed frame that GraphicsDeviceRef_getTimings hands back.

	ListCharString timingNames[MAX_FRAMES_IN_FLIGHT];
	ListTimingEntry timingEntries[MAX_FRAMES_IN_FLIGHT];
	ListGraphicsTiming timings;
	ListU64 timingStack;                                    //Transient: pending entry indices for begin/end matching
	U32 timingCursor;                                       //Transient: next slot while building; total slot count after
	U32 timingSlots[MAX_FRAMES_IN_FLIGHT];                  //Query slots each frame in flight wrote, for the delayed read

	//Temporary for processing command list and to avoid allocations

	ListSpinLockPtr currentLocks;

	U64 pendingBytes;                                //For determining if it's time to flush or to resize staging buffer

	U64 flushThreshold;                              //When the pending bytes are too much and the device should flush

	U64 pendingPrimitives;                           //For determining if it's time to flush because of BLAS creation
	U64 flushThresholdPrimitives;                    //When the pending primitives are too much and the device should flush

	U64 blockSizeCpu, blockSizeGpu;                  //Block sizes for memory allocator

	//Indexed rotate | (isFloat << 1): a rotation needs its own permutation, and so does the texel type, since
	//a view's numeric type has to match what the shader declares (see image_copy.hlsl's TEXEL).

	PipelineRef *copyShaders[4];
	DescriptorLayoutRef *copyDescLayout;
	DescriptorLayoutRef *copyDescPushDesc;
	PipelineLayoutRef *copyPipelineLayout;
	DescriptorLayoutRef *defaultDescLayout;
	DescriptorLayoutRef *defaultCBufferLayout;
	PipelineLayoutRef *defaultPipelineLayout;
	DescriptorTableRef *defaultDescriptorTable;

	DescriptorHeapRef *defaultDescriptorHeaps;

} GraphicsDevice;

typedef RefPtr GraphicsDeviceRef;

typedef struct SHBinaryInfo SHBinaryInfo;
typedef struct SHEntry SHEntry;

#define GraphicsDevice_ext(ptr, T) (!ptr ? NULL : (T##GraphicsDevice*)(ptr + 1))        //impl
#define GraphicsDeviceRef_ptr(ptr) RefPtr_data(ptr, GraphicsDevice)

//Shorthands for the allocator and object RefPtrTypes the device's instance was created with.
//Both are valid for as long as the device is alive (the device holds a ref on the instance).

const Allocator *GraphicsDevice_getAlloc(const GraphicsDevice *device);
const Allocator *GraphicsDeviceRef_getAlloc(GraphicsDeviceRef *device);

const GraphicsObjectTypes *GraphicsDevice_getTypes(const GraphicsDevice *device);
const GraphicsObjectTypes *GraphicsDeviceRef_getTypes(GraphicsDeviceRef *device);

//Fills info with the layout OxC3 uses by default, so a caller can start from it rather than from nothing.
//The bindings own no memory the caller has to free beyond what DescriptorLayoutInfo_free releases.
//isSpirv selects the SPIRV spaces and bindings (Vulkan) instead of the DXIL ones (D3D12),
// so it has to match the api of the instance the device will be created on.
//The raytracing binding is only present if info advertises EGraphicsFeatures_Raytracing.

//binaryType picks which backend's binding numbers to emit, since a set/binding pair means something
// different per backend and the counts are shared.
//It refuses a type it has no numbers for, so adding AIR or WGSL to EGfxBinaryType surfaces here as an
// error rather than as silently reused DXIL registers.

//flags is the same set GraphicsDeviceRef_create takes; only EnableDynamicSamplers changes what comes back,
// and without it the layout carries no _samplers[] binding at all.

Bool GraphicsDevice_defaultBindlessLayout(
	const GraphicsDeviceInfo *info,
	EGfxBinaryType binaryType,
	EGraphicsDeviceFlags flags,
	DescriptorLayoutInfo *result,
	const Allocator *alloc,
	Error *e_rr
);

//bindlessLayout describes the device's bindless descriptor layout, table and pipeline layout.
//NULL means GraphicsDevice_defaultBindlessLayout, which is what OxC3's own shaders are compiled against.
//It is ignored if the device lacks EGraphicsFeatures_Bindless or if EGraphicsDeviceFlags_DisableBindless is set,
// in which case there is no default table or pipeline layout and every pipeline has to bring its own.
//The device copies it, so the caller keeps ownership and can free it right after.
//Its flags are taken as given, so EDescriptorLayoutFlags_AllowBindlessOnArrays has to be set to allocate bindlessly.
//reservedDescriptors is optional extra heap capacity added ON TOP of what the bindless set consumes, so
// bindful descriptor tables can be created from the device's own heap (Device defaultHeap) and live beside
// the bindless set without a second heap and the heap switch a second heap costs.
//Every field adds, maxDescriptorTables included, since the default table takes the one the heap starts with.

typedef struct DescriptorHeapInfo DescriptorHeapInfo;

Bool GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef,
	const GraphicsDeviceInfo *info,
	EGraphicsDeviceFlags flags,
	EGraphicsBufferingMode bufferingMode,
	const DescriptorLayoutInfo *bindlessLayout,        //NULL for OxC3's default layout
	const DescriptorHeapInfo *reservedDescriptors,     //NULL for no extra capacity
	GraphicsDeviceRef **device,
	Error *e_rr
);

//Checks whether a shader binary can run on this device at all.
//Besides features, data types, shader model and vendor, this also refuses a binary whose bindless registers don't
// match the device's bindless layout, since the descriptor handles it indexes with would resolve to the wrong arrays.
//Only binaries the oiSH marks as needing bindless are held to that, so a bindful shader with a pipeline layout of
// its own is unaffected; the mismatch itself is logged with the register and what the layout has instead.

Bool GraphicsDeviceRef_checkShaderFeatures(
	GraphicsDeviceRef *device, const SHBinaryInfo *info, const SHEntry *entry, Error *e_rr
);

//Ensure there are no pending changes from non-existent resources.
Bool GraphicsDeviceRef_removePending(GraphicsDeviceRef *deviceRef, RefPtr *resource);

typedef RefPtr CommandListRef;
typedef RefPtr SwapchainRef;

TListNamed(CommandListRef*, ListCommandListRef);
TListNamed(SwapchainRef*, ListSwapchainRef);

//Returns memory in use in bytes.
//For dGPU, isDeviceLocal is VRAM while !isDeviceLocal is "shared" mem
//For iGPU/CPU it will return the same 0 for device local.
//It returns U64_MAX on error (e.g. if nullptr)
U64 GraphicsDeviceRef_getMemoryBudget(GraphicsDeviceRef *deviceRef, Bool isDeviceLocal);

//Submit commands to device.
//Per dispatch data a shader needs travels as a push constant, which the shader declares and the pipeline
//layout validates, rather than through a block of untyped bytes that every shader shared.
Bool GraphicsDeviceRef_submitCommands(

	GraphicsDeviceRef *deviceRef,
	const ListCommandListRef *commandLists,
	const ListSwapchainRef *swapchains,

	//Set deltaTime < 0 to indicate it has to auto calculate time and deltaTime.
	//But this is not recommended when the deltaTime is constant for example.

	F32 deltaTime,
	F32 time,
	Error *e_rr
);

//Wait on previously submitted commands
Bool GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef, Error *e_rr);

//Copies the GPU timings of the most recently completed frame into a caller-owned list, one per manual region,
// insert or timed scope, keyed by id and name.
//Latent by framesInFlight submits, since a timestamp is only readable once its frame's fence signals.
//The caller owns the result, names and all, and frees it with ListGraphicsTiming_freeUnderlying;
// a list a previous call filled may be passed back, its old contents are released first.

Bool GraphicsDeviceRef_getTimings(GraphicsDeviceRef *deviceRef, ListGraphicsTiming *timings, Error *e_rr);

//Frees a list filled by GraphicsDeviceRef_getTimings: the owned name copies, then the array.

void ListGraphicsTiming_freeUnderlying(ListGraphicsTiming *timings, const Allocator *alloc);

//Timing internals used by the backends. buildTimings assigns query slots and builds the frame's timing entries
// from the submitted lists, returning the slot count it used (0 when timing is off, over capacity, or on failure).
// resolveTimings pairs resolved raw ticks (already masked to the queue's valid bits) with those entries into
// device->timings, converting by nsPerTick.

U32 GraphicsDevice_buildTimings(GraphicsDevice *device, U8 fifId, const ListCommandListRef *lists, const Allocator *alloc);
Bool GraphicsDevice_resolveTimings(
	GraphicsDevice *device, U8 fifId, const U64 *ticks, U32 tickCount, F32 nsPerTick, const Allocator *alloc, Error *e_rr
);

//True exactly once per device per message, so the caller logs on true and stays silent forever after.

Bool GraphicsDevice_logOnce(GraphicsDevice *device, EGraphicsDeviceMessage message);

//Create the pullRegion readback buffers ahead of time, sized for sizePerFrame bytes of pulls per frame.
//The readback memory is otherwise created at the first pull, which on D3D12 can bring in a whole new memory
// block mid frame; reserving during load time moves that hitch to a predictable place.
//Zero reserves the minimum, the buffers only ever grow and underestimating just re-grows at the next pull.
Bool GraphicsDeviceRef_reserveReadback(GraphicsDeviceRef *deviceRef, U64 sizePerFrame, Error *e_rr);

//Private

Bool GraphicsDeviceRef_handleNextFrame(GraphicsDeviceRef *deviceRef, void *commandBuffer, Error *e_rr);

//Records the queued pullRegion reads into the command buffer; called by backends after the frame's commands.
Bool GraphicsDeviceRef_flushPendingPulls(GraphicsDeviceRef *deviceRef, void *commandBuffer, Error *e_rr);
Bool GraphicsDeviceRef_resizeStagingBuffer(GraphicsDeviceRef *deviceRef, U64 newSize, Error *e_rr);

//Shader targets this device's driver can compile for besides the device itself; empty on a backend or driver
// that offers none, which is every one but AMD's D3D12 today.
//The names are the driver's own and can be passed back as an ISA target.

Bool GraphicsDeviceRef_listShaderTargets(
	GraphicsDeviceRef *deviceRef, const Allocator *alloc, ListCharString *result, Error *e_rr
);

#ifdef __cplusplus
	}
#endif
