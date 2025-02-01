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
#include "graphics/generic/device_info.h"
#include "graphics/generic/allocator.h"
#include "graphics/generic/descriptor.h"
#include "types/container/ref_ptr.h"
#include "types/container/list.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsInstanceRef;
typedef RefPtr DeviceBufferRef;

typedef struct CBufferData {

	U32 frameId;					//Can loop back to 0 after U32_MAX!
	F32 time;						//Time since launch of app
	F32 deltaTime;					//deltaTime since last frame.
	U32 swapchainCount;				//How many swapchains are present (will insert ids into appData)

	U32 swapchains[2 * 16];

	U32 appData[(512 - 16 - 2 * 16 * 4) / 4];

} CBufferData;

typedef struct DescriptorStackTrace {

	U32 resourceId, padding;

	#ifndef NDEBUG
		void *stackTrace[8];
	#else
		void *stackTrace[4];
	#endif

} DescriptorStackTrace;

TListNamed(SpinLock*, ListSpinLockPtr);
TList(DescriptorStackTrace);

typedef enum EGraphicsDeviceFlags {
	EGraphicsDeviceFlags_None			= 0,
	EGraphicsDeviceFlags_IsVerbose		= 1 << 0,	//Device creation is verbose
	EGraphicsDeviceFlags_IsDebug		= 1 << 1,	//Debug features such as API/RT validation, debug marker/names
	EGraphicsDeviceFlags_DisableRt		= 1 << 2,	//Don't allow raytracing to be enabled (might reduce driver overhead)
	EGraphicsDeviceFlags_DisableDebug	= 1 << 3	//Force disable debugging even on debug mode. NDEBUG is leading otherwise
} EGraphicsDeviceFlags;

typedef enum EGraphicsBufferingMode {
	EGraphicsBufferingMode_Default,		//Defaults to device preferred; e.g. 2 on mobile, 3 on desktop
	EGraphicsBufferingMode_Default2,
	EGraphicsBufferingMode_Double,		//2 frames in flight (less latency, less memory usage)
	EGraphicsBufferingMode_Triple		//3 frames in flight (more latency, but more performant)
} EGraphicsBufferingMode;

#define MAX_FRAMES_IN_FLIGHT 3			//Don't touch

typedef struct GraphicsDevice {

	GraphicsInstanceRef *instance;

	GraphicsDeviceInfo info;

	U64 submitId;

	EGraphicsDeviceFlags flags;
	U16 pad0;
	U8 framesInFlight;
	U8 fifId;				//(submitId - 1) % FRAMES_IN_FLIGHT

	Ns lastSubmit;

	Ns firstSubmit;											//Start of time

	ListWeakRefPtr pendingResources;						//Resources pending copy from CPU to device next submit

	ListRefPtr resourcesInFlight[MAX_FRAMES_IN_FLIGHT];		//Resources in flight, TODO: HashMap

	SpinLock lock;											//Lock for submission and marking resources dirty

	DeviceMemoryAllocator allocator;

	//Staging allocations and buffers that are used to transmit/receive data from the device

	DeviceBufferRef *staging;								//Staging buffer split by FRAMES_IN_FLIGHT
	AllocationBuffer stagingAllocations[MAX_FRAMES_IN_FLIGHT];

	//Graphics constants (globals) accessible by all shaders

	DeviceBufferRef *frameData[MAX_FRAMES_IN_FLIGHT];

	//Temporary for processing command list and to avoid allocations

	ListSpinLockPtr currentLocks;

	U64 pendingBytes;							//For determining if it's time to flush or to resize staging buffer

	U64 flushThreshold;							//When the pending bytes are too much and the device should flush

	U64 pendingPrimitives;						//For determining if it's time to flush because of BLAS creation
	U64 flushThresholdPrimitives;				//When the pending primitives are too much and the device should flush

	//Used for allocating descriptors

	SpinLock descriptorLock;
	Buffer freeList[EDescriptorType_ResourceCount];
	ListDescriptorStackTrace descriptorStackTraces;

	U64 blockSizeCpu, blockSizeGpu;				//Block sizes for memory allocator

	U64 pad1;									//Pad to 16-byte aligned to allow impl to use for example vectors

} GraphicsDevice;

typedef RefPtr GraphicsDeviceRef;

typedef struct SHBinaryInfo SHBinaryInfo;
typedef struct SHEntry SHEntry;

#define GraphicsDevice_ext(ptr, T) (!ptr ? NULL : (T##GraphicsDevice*)(ptr + 1))		//impl
#define GraphicsDeviceRef_ptr(ptr) RefPtr_data(ptr, GraphicsDevice)

Error GraphicsDeviceRef_dec(GraphicsDeviceRef **device);
Error GraphicsDeviceRef_inc(GraphicsDeviceRef *device);

Error GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef,
	const GraphicsDeviceInfo *info,
	EGraphicsDeviceFlags flags,
	EGraphicsBufferingMode bufferingMode,
	GraphicsDeviceRef **device
);

Bool GraphicsDeviceRef_checkShaderFeatures(GraphicsDeviceRef *device, SHBinaryInfo info, SHEntry entry, Error *e_rr);

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

//Submit commands to device
//appData is up to a 368 byte per frame array used for transmitting render critical info.
Error GraphicsDeviceRef_submitCommands(

	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains,
	Buffer appData,

	//Set deltaTime < 0 to indicate it has to auto calculate time and deltaTime.
	//But this is not recommended when the deltaTime is constant for example.

	F32 deltaTime,
	F32 time
);

//Wait on previously submitted commands
Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef);

//Private

Error GraphicsDeviceRef_handleNextFrame(GraphicsDeviceRef *deviceRef, void *commandBuffer);
Error GraphicsDeviceRef_resizeStagingBuffer(GraphicsDeviceRef *deviceRef, U64 newSize);

#ifdef __cplusplus
	}
#endif
