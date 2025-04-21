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
#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/command_list.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/pipeline.h"
#include "platforms/ext/bufferx.h"
#include "platforms/log.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/ext/ref_ptrx.h"
#include "formats/oiSH/sh_file.h"
#include "types/base/time.h"
#include "types/math/math.h"

#include <stddef.h>

TListNamedImpl(ListSpinLockPtr);
TListNamedImpl(ListCommandListRef);
TListNamedImpl(ListSwapchainRef);

Error GraphicsDeviceRef_dec(GraphicsDeviceRef **device) {
	return !RefPtr_dec(device) ?
		Error_invalidOperation(0, "GraphicsDeviceRef_dec()::device is invalid") : Error_none();
}

Error GraphicsDeviceRef_inc(GraphicsDeviceRef *device) {
	return !RefPtr_inc(device) ?
		Error_invalidOperation(0, "GraphicsDeviceRef_inc()::device is invalid") : Error_none();
}

Bool GraphicsDevice_free(GraphicsDevice *device, Allocator alloc) {

	(void)alloc;

	if(!device)
		return true;

	for(U64 i = 0; i < device->framesInFlight; ++i) {

		for(U64 j = 0; j < device->resourcesInFlight[i].length; ++j)
			RefPtr_dec(device->resourcesInFlight[i].ptrNonConst + j);

		ListRefPtr_freex(&device->resourcesInFlight[i]);
	}

	for(U64 i = 0; i < 2; ++i)
		RefPtr_dec(&device->copyShaders[i]);

	RefPtr_dec(&device->copyPipelineLayout);		//Even though it's 'ref counted' it's internal so destruction order matters
	RefPtr_dec(&device->copyDescLayout);
	RefPtr_dec(&device->copyDescPushDesc);
	RefPtr_dec(&device->defaultDescriptorTable);
	RefPtr_dec(&device->defaultPipelineLayout);
	RefPtr_dec(&device->defaultDescLayout);
	RefPtr_dec(&device->defaultCBufferLayout);
	RefPtr_dec(&device->defaultDescriptorHeaps);

	for(U64 i = 0; i < device->framesInFlight; ++i)
		DeviceBufferRef_dec(&device->frameData[i]);

	DeviceBufferRef_dec(&device->staging);

	SpinLock_lock(&device->lock, U64_MAX);
	SpinLock_lock(&device->allocator.lock, U64_MAX);
	ListWeakRefPtr_freex(&device->pendingResources);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		AllocationBuffer_freex(&device->stagingAllocations[i]);

	GraphicsDevice_freeExt(GraphicsInstanceRef_ptr(device->instance), (void*) GraphicsInstance_ext(device, ));

	U64 leakedBlocks = 0;

	for (U64 i = 0; i < device->allocator.blocks.length; ++i) {
		const DeviceMemoryBlock block = device->allocator.blocks.ptr[i];
		leakedBlocks += (Bool) Buffer_length(block.allocations.buffer);
	}

	if(leakedBlocks)
		Log_warnLnx("Leaked graphics device memory (showing up to 16/"PRIu64" entries):", leakedBlocks);

	for (U64 i = 0; i < leakedBlocks && i < 16; ++i) {

		const DeviceMemoryBlock block = device->allocator.blocks.ptr[i];
		const U64 leaked = Buffer_length(block.allocations.buffer);

		if(!leaked)
			continue;

		Log_warnLnx("%"PRIu64": %"PRIu64" bytes", i, leaked);

		if(device->flags & EGraphicsDeviceFlags_IsDebug)
			Log_printCapturedStackTraceCustomx(
				(const void**) block.stackTrace, sizeof(block.stackTrace) / sizeof(void*),
				ELogLevel_Warn, ELogOptions_NewLine
			);
	}

	ListDeviceMemoryBlock_freex(&device->allocator.blocks);
	ListSpinLockPtr_freex(&device->currentLocks);

	GraphicsInstanceRef_dec(&device->instance);

	return true;
}

Bool GraphicsDeviceRef_createPrebuiltShaders(GraphicsDeviceRef *deviceRef, Error *e_rr) {
	
	Bool s_uccess = true;
	Buffer tempBuffer = Buffer_createNull();
	SHFile tmpBinary = (SHFile) { 0 };
	DescriptorLayoutInfo pushDescriptors = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo info = (DescriptorLayoutInfo) { 0 };

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	//Load prebuilt shaders
	
	if(!File_loadVirtual(CharString_createRefCStrConst("//OxC3_graphics"), NULL, e_rr))
		goto clean;

	//image_copy shaders, only the 2nd can rotate images

	CharString path = CharString_createRefCStrConst("//OxC3_graphics/shaders/image_copy.oiSH");
	gotoIfError3(clean, File_readx(path, U64_MAX, 0, 0, &tempBuffer, e_rr))
	gotoIfError3(clean, SHFile_readx(tempBuffer, false, &tmpBinary, e_rr))

	CharString uniforms[2] = {
		CharString_createRefCStrConst("ROTATE"),
		CharString_createRefCStrConst("1")
	};

	for(U64 i = 0; i < 2; ++i) {

		ListCharString uniformsList = (ListCharString) { 0 };

		if(i)
			gotoIfError2(clean, ListCharString_createRefConst(uniforms, 2, &uniformsList))
		
		U32 mainSingle = GraphicsDeviceRef_getFirstShaderEntry(
			deviceRef,
			tmpBinary,
			CharString_createRefCStrConst("mainSingle"),
			uniformsList,
			ESHExtension_None,
			ESHExtension_None
		);

		if(mainSingle == U32_MAX)
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_createPrebuiltShaders() couldn't find entrypoint"))

		//Our copy shader uses push descriptors and push constants,
		//But just in case, it could also handle making a descriptor layout.

		if(!device->copyPipelineLayout) {

			DescriptorBinding pushConstants = (DescriptorBinding) { 0 };

			gotoIfError3(clean, GraphicsDeviceRef_detectLayoutFromEntry(
				deviceRef,
				tmpBinary,
				mainSingle,
				EDescriptorLayoutFlags_InternalWeakDeviceRef,
				EDetectDescriptorLayoutFlags_AssumePushDescriptors | EDetectDescriptorLayoutFlags_AssumePushConstants,
				(ListCharString) { 0 },
				(CharString) { 0 },
				&pushConstants,
				&info,
				&pushDescriptors,
				e_rr
			))

			if(info.bindings.length)
				gotoIfError2(clean, GraphicsDeviceRef_createDescriptorLayout(
					deviceRef, &info, CharString_createRefCStrConst("Copy image desc layout"), &device->copyDescLayout
				))

			if(pushDescriptors.bindings.length)
				gotoIfError2(clean, GraphicsDeviceRef_createDescriptorLayout(
					deviceRef, &pushDescriptors, CharString_createRefCStrConst("Copy image push desc layout"), &device->copyDescPushDesc
				))

			PipelineLayoutInfo pipelineInfo = (PipelineLayoutInfo) {
				.flags = EPipelineLayoutFlags_InternalWeakDeviceRef,
				.bindings = device->copyDescLayout,
				.pushDescriptors = device->copyDescPushDesc,
				.pushConstants = pushConstants
			};

			gotoIfError2(clean, GraphicsDeviceRef_createPipelineLayout(
				deviceRef,
				pipelineInfo,
				CharString_createRefCStrConst("Copy image pipeline layout"),
				&device->copyPipelineLayout
			))
		}

		gotoIfError3(clean, GraphicsDeviceRef_createPipelineCompute(
			deviceRef,
			tmpBinary,
			CharString_createRefCStrConst("Copy image shader"),
			mainSingle,
			EPipelineFlags_InternalWeakDeviceRef,
			device->copyPipelineLayout,
			&device->copyShaders[i],
			e_rr
		))
	}

clean:
	DescriptorLayoutInfo_free(&info, Platform_instance->alloc);
	DescriptorLayoutInfo_free(&pushDescriptors, Platform_instance->alloc);
	SHFile_freex(&tmpBinary);
	Buffer_freex(&tempBuffer);
	return s_uccess;
}

typedef enum EDescriptorTypeCount {

	EDescriptorTypeCount_Texture2D			= 131072,
	EDescriptorTypeCount_TextureCube		= 32768,
	EDescriptorTypeCount_Texture3D			= 32768,

	EDescriptorTypeCount_Buffer				= 131072,
	EDescriptorTypeCount_RWBuffer			= 131072,

	EDescriptorTypeCount_RWTexture3D		= 32768,
	EDescriptorTypeCount_RWTexture3Di		= 8192,
	EDescriptorTypeCount_RWTexture3Du		= 8192,
	EDescriptorTypeCount_RWTexture2D		= 131072,
	EDescriptorTypeCount_RWTexture2Di		= 16384,
	EDescriptorTypeCount_RWTexture2Du		= 16384,

	EDescriptorTypeCount_Sampler			= 1024,
	EDescriptorTypeCount_TLASExt			= 16,

	//DirectX bindings

	EDescriptorTypeOffset_Texture2D			= 0,
	EDescriptorTypeOffset_TextureCube		= EDescriptorTypeOffset_Texture2D + EDescriptorTypeCount_Texture2D,
	EDescriptorTypeOffset_Texture3D			= EDescriptorTypeOffset_TextureCube + EDescriptorTypeCount_TextureCube,
	EDescriptorTypeOffset_Buffer			= EDescriptorTypeOffset_Texture3D + EDescriptorTypeCount_Texture3D,
	EDescriptorTypeOffset_TLASExt			= EDescriptorTypeOffset_Buffer + EDescriptorTypeCount_Buffer,

	EDescriptorTypeOffset_RWBuffer			= 0,
	EDescriptorTypeOffset_RWTexture3D		= EDescriptorTypeOffset_RWBuffer + EDescriptorTypeCount_RWBuffer,
	EDescriptorTypeOffset_RWTexture3Di		= EDescriptorTypeOffset_RWTexture3D + EDescriptorTypeCount_RWTexture3D,
	EDescriptorTypeOffset_RWTexture3Du		= EDescriptorTypeOffset_RWTexture3Di + EDescriptorTypeCount_RWTexture3Di,
	EDescriptorTypeOffset_RWTexture2D		= EDescriptorTypeOffset_RWTexture3Du + EDescriptorTypeCount_RWTexture3Du,
	EDescriptorTypeOffset_RWTexture2Di		= EDescriptorTypeOffset_RWTexture2D + EDescriptorTypeCount_RWTexture2D,
	EDescriptorTypeOffset_RWTexture2Du		= EDescriptorTypeOffset_RWTexture2Di + EDescriptorTypeCount_RWTexture2Di,

	EDescriptorTypeOffset_Sampler			= 0,

	EDescriptorTypeCount_Buffers =
		EDescriptorTypeCount_Buffer + EDescriptorTypeCount_RWBuffer,

	EDescriptorTypeCount_TexturesRead =
		EDescriptorTypeCount_Texture2D + EDescriptorTypeCount_Texture3D + EDescriptorTypeCount_TextureCube,

	EDescriptorTypeCount_TexturesRW =
		EDescriptorTypeCount_RWTexture2D + EDescriptorTypeCount_RWTexture2Du + EDescriptorTypeCount_RWTexture2Di +
		EDescriptorTypeCount_RWTexture3D + EDescriptorTypeCount_RWTexture3Du + EDescriptorTypeCount_RWTexture3Di

} EDescriptorTypeCount;

Error GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef,
	const GraphicsDeviceInfo *info,
	EGraphicsDeviceFlags flags,
	EGraphicsBufferingMode bufferMode,
	GraphicsDeviceRef **deviceRef
) {

	if(!instanceRef || !info || !deviceRef)
		return Error_nullPointer(
			!instanceRef ? 0 : (!info ? 1 : 2),
			"GraphicsDeviceRef_create()::instanceRef, info and deviceRef are required"
		);

	if(instanceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsInstance)
		return Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_create()::instanceRef was an invalid type"
		);

	if(*deviceRef)
		return Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_create()::*deviceRef wasn't NULL, probably indicates memleak"
		);

	//Create RefPtr

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(instanceRef);

	Error err = Error_none();
	gotoIfError(clean, RefPtr_createx(
		(U32)(sizeof(GraphicsDevice) + GraphicsInterface_getObjectSizes(instance->api)->device),
		(ObjectFreeFunc) GraphicsDevice_free,
		(ETypeId) EGraphicsTypeId_GraphicsDevice,
		deviceRef
	))

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);

	gotoIfError(clean, ListWeakRefPtr_reservex(&device->pendingResources, 128))

	if (bufferMode < 2 || bufferMode > 3)
		bufferMode = _PLATFORM_TYPE == PLATFORM_ANDROID ? EGraphicsBufferingMode_Double : EGraphicsBufferingMode_Triple;

	device->info = *info;
	device->flags = flags;
	device->framesInFlight = (U8) bufferMode;

	if(device->flags & EGraphicsDeviceFlags_DisableRt)
		device->info.capabilities.features &=~ (
			EGraphicsFeatures_Raytracing				|
			EGraphicsFeatures_RayPipeline				|
			EGraphicsFeatures_RayQuery					|
			EGraphicsFeatures_RayMicromapOpacity		|
			EGraphicsFeatures_RayMicromapDisplacement	|
			EGraphicsFeatures_RayMotionBlur				|
			EGraphicsFeatures_RayReorder				|
			EGraphicsFeatures_RayValidation
		);

	Bool isDebugInstance = instance->flags & EGraphicsInstanceFlags_IsDebug;

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && !isDebugInstance)
		gotoIfError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_create() tried to create debug device but the instance had it disabled"
		))

	#ifndef NDEBUG
		if(!(device->flags & EGraphicsDeviceFlags_DisableDebug) && isDebugInstance)
			device->flags |= EGraphicsDeviceFlags_IsDebug;
	#endif

	if(!(device->flags & EGraphicsDeviceFlags_IsDebug))
		device->info.capabilities.features &=~ EGraphicsFeatures_RayValidation;

	gotoIfError(clean, GraphicsInstanceRef_inc(instanceRef))
	device->instance = instanceRef;
	//Create mem alloc, set info/instance/pending resources

	device->allocator = (DeviceMemoryAllocator) { .device = device };
	gotoIfError(clean, ListDeviceMemoryBlock_reservex(&device->allocator.blocks, 16))

	//Create in flight resource refs

	for(U64 i = 0; i < device->framesInFlight; ++i)
		gotoIfError(clean, ListRefPtr_reservex(&device->resourcesInFlight[i], 64))

	//Create extended device

	gotoIfError(clean, GraphicsDevice_initExt(instance, info, deviceRef))

	//Create default descriptor heaps
	//TODO: Allow user to define these

	if(device->info.capabilities.features & EGraphicsFeatures_Bindless) {
	
		CharString name = CharString_createRefCStrConst("Default heap");

		DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) {

			.flags = EDescriptorHeapFlags_InternalWeakDeviceRef | EDescriptorHeapFlags_AllowBindless,

			.maxAccelerationStructures =
				device->info.capabilities.features & EGraphicsFeatures_Raytracing ? EDescriptorTypeCount_TLASExt : 0,

			.maxSamplers = EDescriptorTypeCount_Sampler,
			.maxTextures = EDescriptorTypeCount_TexturesRead,
			.maxTexturesRW = EDescriptorTypeCount_TexturesRW,
			.maxBuffersRW = EDescriptorTypeCount_Buffers,
			.maxConstantBuffers = 3,
			.maxDescriptorTables = 1
		};

		gotoIfError(clean, GraphicsDeviceRef_createDescriptorHeap(*deviceRef, heapInfo, name, &device->defaultDescriptorHeaps))

		//Create default descriptor layout
		//TODO: Make this configurable and have a way to create the default one

		Bool isSpirv = instance->api == EGraphicsApi_Vulkan;

		CharString bindingNames[13] = {
			CharString_createRefCStrConst("_samplers"),
			CharString_createRefCStrConst("_textures2D"),
			CharString_createRefCStrConst("_textureCubes"),
			CharString_createRefCStrConst("_textures3D"),
			CharString_createRefCStrConst("_buffer"),
			CharString_createRefCStrConst("_rwBuffer"),
			CharString_createRefCStrConst("_rwTextures3D"),
			CharString_createRefCStrConst("_rwTextures3Di"),
			CharString_createRefCStrConst("_rwTextures3Du"),
			CharString_createRefCStrConst("_rwTextures2D"),
			CharString_createRefCStrConst("_rwTextures2Di"),
			CharString_createRefCStrConst("_rwTextures2Du"),
			CharString_createRefCStrConst("_tlasExt")
		};

		DescriptorBinding bindings[13] = {
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Sampler,
				.count = EDescriptorTypeCount_Sampler,
				.binding = (SHBinding) {
					.space = 0,
					.binding = isSpirv ? 0 : EDescriptorTypeOffset_Sampler
				},
				.visibility = U32_MAX
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture2D,
				.count = EDescriptorTypeCount_Texture2D,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 0 : EDescriptorTypeOffset_Texture2D
				},
				.visibility = U32_MAX
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_TextureCube,
				.count = EDescriptorTypeCount_TextureCube,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 1 : EDescriptorTypeOffset_TextureCube
				},
				.visibility = U32_MAX
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture3D,
				.count = EDescriptorTypeCount_Texture3D,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 2 : EDescriptorTypeOffset_Texture3D
				},
				.visibility = U32_MAX
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_ByteAddressBuffer,
				.count = EDescriptorTypeCount_Buffer,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 3 : EDescriptorTypeOffset_Buffer
				},
				.visibility = U32_MAX
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_ByteAddressBuffer | ESHRegisterType_IsWrite,
				.count = EDescriptorTypeCount_RWBuffer,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 4 : EDescriptorTypeOffset_RWBuffer
				},
				.visibility = U32_MAX
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture3D | ESHRegisterType_IsWrite,
				.count = EDescriptorTypeCount_RWTexture3D,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 5 : EDescriptorTypeOffset_RWTexture3D
				},
				.visibility = U32_MAX,
				.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_Float }
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture3D | ESHRegisterType_IsWrite,
				.count = EDescriptorTypeCount_RWTexture3Di,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 6 : EDescriptorTypeOffset_RWTexture3Di
				},
				.visibility = U32_MAX,
				.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_SInt }
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture3D | ESHRegisterType_IsWrite,
				.count = EDescriptorTypeCount_RWTexture3Du,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 7 : EDescriptorTypeOffset_RWTexture3Du
				},
				.visibility = U32_MAX,
				.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_UInt }
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture2D | ESHRegisterType_IsWrite,
				.count = EDescriptorTypeCount_RWTexture2D,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 8 : EDescriptorTypeOffset_RWTexture2D
				},
				.visibility = U32_MAX,
				.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_UNorm }
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture2D | ESHRegisterType_IsWrite,
				.count = EDescriptorTypeCount_RWTexture2Di,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 9 : EDescriptorTypeOffset_RWTexture2Di
				},
				.visibility = U32_MAX,
				.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_SInt }
			},
			(DescriptorBinding) {
				.registerType = ESHRegisterType_Texture2D | ESHRegisterType_IsWrite,
				.count = EDescriptorTypeCount_RWTexture2Du,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 10 : EDescriptorTypeOffset_RWTexture2Du
				},
				.visibility = U32_MAX,
				.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_UInt }
			}
		};

		U64 descBindings = 12;

		if(device->info.capabilities.features & EGraphicsFeatures_Raytracing)
			bindings[descBindings++] = (DescriptorBinding) {
				.registerType = ESHRegisterType_AccelerationStructure,
				.count = EDescriptorTypeCount_TLASExt,
				.binding = (SHBinding) {
					.space = isSpirv ? 1 : 0,
					.binding = isSpirv ? 11 : EDescriptorTypeOffset_TLASExt
				},
				.visibility = U32_MAX
			};

		DescriptorLayoutInfo descLayoutInfo = (DescriptorLayoutInfo) {
			.flags = EDescriptorLayoutFlags_InternalWeakDeviceRef | EDescriptorLayoutFlags_AllowBindlessOnArrays
		};

		//Create descriptor set & table

		gotoIfError(clean, ListDescriptorBinding_createRefConst(bindings, descBindings, &descLayoutInfo.bindings))
		gotoIfError(clean, ListCharString_createRefConst(bindingNames, descBindings, &descLayoutInfo.bindingNames))

		name = CharString_createRefCStrConst("Default descriptor layout");
		gotoIfError(clean, GraphicsDeviceRef_createDescriptorLayout(*deviceRef, &descLayoutInfo, name, &device->defaultDescLayout))

		name = CharString_createRefCStrConst("Default descriptor table");
		gotoIfError(clean, DescriptorHeapRef_createDescriptorTable(
			device->defaultDescriptorHeaps,
			device->defaultDescLayout,
			EDescriptorTableFlags_InternalWeakDeviceRef,
			name,
			&device->defaultDescriptorTable
		))

		//Create push descriptor

		descLayoutInfo = (DescriptorLayoutInfo) {
			.flags = EDescriptorLayoutFlags_InternalWeakDeviceRef | EDescriptorLayoutFlags_HasPushDescriptors
		};

		DescriptorBinding cbv = (DescriptorBinding) {
			.registerType = ESHRegisterType_ConstantBuffer,
			.count = 1,
			.binding = (SHBinding) {
				.space = isSpirv ? 2 : 0,
				.binding = isSpirv ? 0 : 0
			},
			.visibility = U32_MAX,
			.constantBufferSize = (U32) sizeof(CBufferData)
		};
		
		CharString cbvName = CharString_createRefCStrConst("globals");

		gotoIfError(clean, ListDescriptorBinding_createRefConst(&cbv, 1, &descLayoutInfo.bindings))
		gotoIfError(clean, ListCharString_createRefConst(&cbvName, 1, &descLayoutInfo.bindingNames))

		name = CharString_createRefCStrConst("Constant buffer push descriptor");
		gotoIfError(clean, GraphicsDeviceRef_createDescriptorLayout(*deviceRef, &descLayoutInfo, name, &device->defaultCBufferLayout))

		//Create pipeline layout

		name = CharString_createRefCStrConst("Default pipeline layout");
		PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) {
			.flags = EPipelineLayoutFlags_InternalWeakDeviceRef,
			.bindings = device->defaultDescLayout,
			.pushDescriptors = device->defaultCBufferLayout
		};

		gotoIfError(clean, GraphicsDeviceRef_createPipelineLayout(
			*deviceRef, pipelineLayoutInfo, name, &device->defaultPipelineLayout
		))
	}

	//Determine some flushing and block size sizes for the current GPU
	
	//Determine when we need to flush.
	//As a rule of thumb I decided for 20% occupied mem by just copies.
	//Or if there's distinct shared mem available too it can allocate 10% more in that memory too
	// (as long as it doesn't exceed 33%).
	//Flush threshold is kept under 4 GiB to avoid TDRs because even if the mem is available it might be slow.

	const Bool isDistinct = device->info.type == EGraphicsDeviceType_Dedicated;
	U64 cpuHeapSize = device->info.capabilities.sharedMemory;
	U64 gpuHeapSize = device->info.capabilities.dedicatedMemory;

	device->flushThreshold = U64_min(
		4 * GIBI,
		isDistinct ? U64_min(gpuHeapSize / 3, cpuHeapSize / 10 + gpuHeapSize / 5) :
		cpuHeapSize / 5
	);

	device->flushThresholdPrimitives = 20 * MIBI / 3;		//20M vertices per frame limit

	//Block sizes based on memory of each device (CPU or GPU):
	// 0 -  6GB ("4GB"):   64MB
	// 6 - 12GB ("8GB"):  128MB
	//12 - 24GB ("16GB"): 256MB
	//24GB+ 	("32GB"): 512MB
	//E.g. Memory allocated CPU visible with a dGPU with 32GB available would 

	if (!isDistinct) {		//Assume 50/50 split to take a conservative block size approach
		cpuHeapSize >>= 1;
		gpuHeapSize >>= 1;
	}

	device->blockSizeCpu = (64 * MIBI) << (U64) F64_clamp(F64_round(F64_log2((F64)cpuHeapSize)) - 32, 0, 3);
	device->blockSizeGpu = (64 * MIBI) << (U64) F64_clamp(F64_round(F64_log2((F64)gpuHeapSize)) - 32, 0, 3);

	//Create constant buffer and staging buffer / allocators

	//Allocate staging buffer.
	//For block size 256MiB: 64 MiB / NBuffering (2 or 3) = 32MiB or 21.333 MiB per frame.
	//For block size 64MiB: 16 MiB / NBuffering (2 or 3) = 8MiB or 5.3 MiB per frame.
	//	This block size is generally used in very memory limited systems (such as Android devices)
	//	that generally use shared mem (so won't push much over staging buffer).
	//If out of mem this will grow to be bigger.
	//But it's only used for "small" allocations (< 25% of staging buffer)
	//If a lot of these larger allocations are found it will resize the staging buffer to try to encompass it too.

	U64 stagingSize = device->blockSizeCpu / 4;
	gotoIfError(clean, GraphicsDeviceRef_resizeStagingBuffer(*deviceRef, stagingSize))

	//Allocate UBO

	for(U64 i = 0; i < device->framesInFlight; ++i)
		gotoIfError(clean, GraphicsDeviceRef_createBuffer(
			*deviceRef,
			EDeviceBufferUsage_Uniform,
			EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
			NULL,
			CharString_createRefCStrConst("Per frame data"),
			sizeof(CBufferData), &device->frameData[i]
		))

	//Load prebuilt shaders
	
	if(!GraphicsDeviceRef_createPrebuiltShaders(*deviceRef, &err))
		goto clean;

clean:

	if (err.genericError)
		GraphicsDeviceRef_dec(deviceRef);

	return err;
}

Bool GraphicsDeviceRef_checkShaderFeatures(GraphicsDeviceRef *deviceRef, SHBinaryInfo bin, SHEntry entry, Error *e_rr) {

	Bool s_uccess = true;

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_checkShaderFeatures()::deviceRef is required"))

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ESHExtension extensions = (bin.identifier.extensions &~ bin.dormantExtensions) & ESHExtension_All;

	EGraphicsFeatures features = EGraphicsFeatures_None;
	EDxGraphicsFeatures featuresDx = EDxGraphicsFeatures_None;
	EGraphicsDataTypes dataTypes = EGraphicsDataTypes_None;

	if(extensions & ESHExtension_SubgroupOperations)		features |= EGraphicsFeatures_SubgroupOperations;
	if(extensions & ESHExtension_SubgroupArithmetic)		features |= EGraphicsFeatures_SubgroupArithmetic;
	if(extensions & ESHExtension_SubgroupShuffle)			features |= EGraphicsFeatures_SubgroupShuffle;

	if(extensions & ESHExtension_Multiview)					features |= EGraphicsFeatures_Multiview;

	if(extensions & ESHExtension_RayQuery)					features |= EGraphicsFeatures_RayQuery;
	if(extensions & ESHExtension_RayMicromapOpacity)		features |= EGraphicsFeatures_RayMicromapOpacity;
	if(extensions & ESHExtension_RayMicromapDisplacement)	features |= EGraphicsFeatures_RayMicromapDisplacement;
	if(extensions & ESHExtension_RayMotionBlur)				features |= EGraphicsFeatures_RayMotionBlur;
	if(extensions & ESHExtension_RayReorder)				features |= EGraphicsFeatures_RayReorder;

	if(extensions & ESHExtension_ComputeDeriv)				features |= EGraphicsFeatures_ComputeDeriv;
	if(extensions & ESHExtension_MeshTaskTexDeriv)			features |= EGraphicsFeatures_MeshTaskTexDeriv;

	if(extensions & ESHExtension_WriteMSTexture)			features |= EGraphicsFeatures_WriteMSTexture;

	if(extensions & ESHExtension_Bindless)					features |= EGraphicsFeatures_Bindless;

	if(extensions & ESHExtension_F64)						dataTypes |= EGraphicsDataTypes_F64;
	if(extensions & ESHExtension_I64)						dataTypes |= EGraphicsDataTypes_I64;

	if(extensions & ESHExtension_16BitTypes)				dataTypes |= EGraphicsDataTypes_I16 | EGraphicsDataTypes_F16;

	if(extensions & ESHExtension_AtomicI64)					dataTypes |= EGraphicsDataTypes_AtomicI64;
	if(extensions & ESHExtension_AtomicF32)					dataTypes |= EGraphicsDataTypes_AtomicF32;
	if(extensions & ESHExtension_AtomicF64)					dataTypes |= EGraphicsDataTypes_AtomicF64;

	if(extensions & ESHExtension_PAQ)						featuresDx |= EDxGraphicsFeatures_PAQ;

	if ((bin.identifier.shaderVersion >> 8) == 6)			//Check shader model compatibility
		switch ((U8) bin.identifier.shaderVersion) {
			case 6:											featuresDx |= EDxGraphicsFeatures_SM6_6;	break;
			case 7:											featuresDx |= EDxGraphicsFeatures_SM6_7;	break;
			case 8:											featuresDx |= EDxGraphicsFeatures_SM6_8;	break;
			case 9:											featuresDx |= EDxGraphicsFeatures_SM6_9;	break;
			default:																					break;
		}

	if(entry.waveSize >> 4)									featuresDx |= EDxGraphicsFeatures_WaveSizeMinMax;
	else if(entry.waveSize)									featuresDx |= EDxGraphicsFeatures_WaveSize;

	if((device->info.capabilities.features & features) != features)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the features is missing"))

	if((device->info.capabilities.dataTypes & dataTypes) != dataTypes)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the dataTypes is missing"))

	if(bin.vendorMask != ((1 << ESHVendor_Count) - 1))
		if(!((bin.vendorMask >> device->info.vendor) & 1))
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() binary is incompatible with vendor"))

	//Check for D3D12 features, shader models and DXIL

	if(GraphicsInstanceRef_ptr(device->instance)->api == EGraphicsApi_Direct3D12) {

		if((device->info.capabilities.featuresExt & (U32)featuresDx) != (U32)featuresDx)
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the featuresDx is missing"))

		if(!Buffer_length(bin.binaries[ESHBinaryType_DXIL]))
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() DXIL binary is missing"))
	}

	//Check for SPIRV

	else if(!Buffer_length(bin.binaries[ESHBinaryType_SPIRV]))
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() SPIRV binary is missing"))

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_removePending(GraphicsDeviceRef *deviceRef, RefPtr *resource) {

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return false;

	Bool supported = false;

	const EGraphicsTypeId type = (EGraphicsTypeId) resource->typeId;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ListWeakRefPtr *pendingList = NULL;

	switch (type) {

		case EGraphicsTypeId_DeviceBuffer:
			supported = DeviceBufferRef_ptr(resource)->resource.device == deviceRef;
			pendingList = &device->pendingResources;
			break;

		case EGraphicsTypeId_DeviceTexture:
			supported = DeviceTextureRef_ptr(resource)->base.resource.device == deviceRef;
			pendingList = &device->pendingResources;
			break;

		default:
			return false;
	}

	if(!supported || !pendingList)
		return false;

	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return false;

	const U64 found = ListWeakRefPtr_findFirst(*pendingList, resource, 0, NULL);
	Bool success = false;

	if (found == U64_MAX) {
		success = true;
		goto clean;
	}

	Error err = Error_none();
	gotoIfError(clean, ListWeakRefPtr_erase(pendingList, found))

	success = true;

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return success;
}

Error GraphicsDeviceRef_handleNextFrame(GraphicsDeviceRef *deviceRef, void *commandBuffer) {

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_handleNextFrame()::deviceRef is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!SpinLock_isLockedForThread(&device->lock))
		return Error_invalidState(
			0, "GraphicsDeviceRef_handleNextFrame() requires device to be locked by caller"
		);

	//Release resources that were in flight.
	//This might cause resource deletions because we might be the last one releasing them.
	//For example temporary staging resources are released this way.

	ListRefPtr *inFlight = &device->resourcesInFlight[device->fifId];

	for (U64 i = 0; i < inFlight->length; ++i)
		RefPtr_dec(inFlight->ptrNonConst + i);

	Error err = Error_none();
	gotoIfError(clean, ListRefPtr_clear(inFlight))

	//Release all allocations of buffer that was in flight

	if(!AllocationBuffer_freeAll(&device->stagingAllocations[device->fifId]))
		gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_handleNextFrame() AllocationBuffer_freeAll failed"))

	//Update buffer data

	for(U64 i = 0; i < device->pendingResources.length; ++i) {

		RefPtr *pending = device->pendingResources.ptr[i];

		EGraphicsTypeId type = (EGraphicsTypeId) pending->typeId;

		switch(type) {

			case EGraphicsTypeId_DeviceBuffer:
				gotoIfError(clean, DeviceBufferRef_flushExt(commandBuffer, deviceRef, pending))
				break;

			case EGraphicsTypeId_DeviceTexture:
				gotoIfError(clean, DeviceTextureRef_flushExt(commandBuffer, deviceRef, pending))
				break;

			default:
				gotoIfError(clean, Error_unsupportedOperation(
					5, "GraphicsDeviceRef_handleNextFrame() unsupported pending graphics object"
				))
		}
	}

	gotoIfError(clean, ListWeakRefPtr_clear(&device->pendingResources))

clean:
	return err;
}

Error GraphicsDeviceRef_resizeStagingBuffer(GraphicsDeviceRef *deviceRef, U64 newSize) {

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_resizeStagingBuffer()::deviceRef is required");

	Error err;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	newSize = (((newSize + 2) / 3 + 4095) &~ 4095) * 3;			//Align to ensure we never get incompatible staging buffers

	if (device->staging) {

		//"Free" staging buffer.
		//If the staging buffer was already in flight this won't do anything until it's out of flight.

		for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
			AllocationBuffer_freex(&device->stagingAllocations[i]);

		DeviceBufferRef_dec(&device->staging);
	}

	gotoIfError(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_None,
		EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
		NULL,
		CharString_createRefCStrConst("Staging buffer"),
		newSize, &device->staging
	))

	const DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
	const Buffer stagingBuffer = Buffer_createRef(staging->resource.mappedMemoryExt, newSize);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		gotoIfError(clean, AllocationBuffer_createRefFromRegionx(
			stagingBuffer, newSize / 3 * i, newSize / 3, 0, &device->stagingAllocations[i]
		))

clean:
	return err;
}

Error GraphicsDeviceRef_submitCommands(
	GraphicsDeviceRef *deviceRef,
	ListCommandListRef commandLists,
	ListSwapchainRef swapchains,
	Buffer appData,
	F32 deltaTime,
	F32 time
) {

	//Validation

	if(!deviceRef || deviceRef->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_submitCommands()::deviceRef is required");

	if(!swapchains.length && !commandLists.length)
		return Error_invalidOperation(
			0, "GraphicsDeviceRef_submitCommands()::swapchains or commandLists is required"
		);

	if(swapchains.length > sizeof(((CBufferData*)NULL)->swapchains) / 8)
		return Error_invalidParameter(
			2, 1, "GraphicsDeviceRef_submitCommands()::swapchains.length is limited to 16"
		);

	if(Buffer_length(appData) > sizeof(((CBufferData*)NULL)->appData))
		return Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_submitCommands()::appData is limited to 368 bytes"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Error err = Error_none();

	SpinLock *lockPtr = &device->lock;
	ELockAcquire acq = SpinLock_lock(lockPtr, U64_MAX);

	if(acq < ELockAcquire_Success)
		return Error_invalidState(0, "GraphicsDeviceRef_submitCommands() couldn't acquire device lock");

	if(acq == ELockAcquire_Acquired)
		gotoIfError(clean, ListSpinLockPtr_pushBackx(&device->currentLocks, lockPtr))

	lockPtr = NULL;

	//Validate command lists

	for(U64 i = 0; i < commandLists.length; ++i) {

		CommandListRef *cmdRef = commandLists.ptr[i];

		if(!cmdRef || cmdRef->typeId != (ETypeId) EGraphicsTypeId_CommandList)
			gotoIfError(clean, Error_nullPointer(1, "GraphicsDeviceRef_submitCommands()::commandLists[i] is required"))

		CommandList *cmd = CommandListRef_ptr(cmdRef);

		if(cmd->device != deviceRef)
			gotoIfError(clean, Error_unsupportedOperation(
				0, "GraphicsDeviceRef_submitCommands()::commandLists[i]'s device and the current device are different"
			))

		lockPtr = &cmd->lock;
		acq = SpinLock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			gotoIfError(clean, Error_invalidState(
				1, "GraphicsDeviceRef_submitCommands()::commandLists[i] couldn't be acquired"
			))
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError(clean, ListSpinLockPtr_pushBackx(&device->currentLocks, lockPtr))

		lockPtr = NULL;

		if(cmd->state != ECommandListState_Closed)
			gotoIfError(clean, Error_invalidParameter(
				1, (U32)i, "GraphicsDeviceRef_submitCommands()::commandLists[i] wasn't closed properly"
			))
	}

	for (U64 i = 0; i < swapchains.length; ++i) {

		SwapchainRef *swapchainRef = swapchains.ptr[i];

		for(U64 j = 0; j < i; ++j)
			if(swapchainRef == swapchains.ptr[j])
				gotoIfError(clean, Error_invalidParameter(
					2, 2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is duplicated"
				))

		if(!swapchainRef || swapchainRef->typeId != (ETypeId) EGraphicsTypeId_Swapchain)
			gotoIfError(clean, Error_nullPointer(2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is required"))

		Swapchain *swapchaini = SwapchainRef_ptr(swapchainRef);

		if(swapchaini->base.resource.device != deviceRef)
			gotoIfError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_submitCommands()::swapchains[i]'s device and the current device are different"
			))

		lockPtr = &swapchaini->lock;
		acq = SpinLock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {

			lockPtr = NULL;

			gotoIfError(clean, Error_invalidState(
				2, "GraphicsDeviceRef_submitCommands()::swapchains[i] couldn't acquire lock"
			))
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError(clean, ListSpinLockPtr_pushBackx(&device->currentLocks, lockPtr))

		lockPtr = NULL;

		//Validate if the swapchain with a different version is bound, if yes, we have a stale cmdlist

		for (U64 j = 0; j < commandLists.length; ++j) {

			CommandListRef *cmdRef = commandLists.ptr[i];
			CommandList *cmd = CommandListRef_ptr(cmdRef);

			for(U64 k = 0; k < cmd->activeSwapchains.length; ++k) {

				DeviceResourceVersion vK = cmd->activeSwapchains.ptr[k];

				if(vK.resource == swapchainRef && vK.version != swapchaini->versionId)
					gotoIfError(clean, Error_invalidState(
						0, "GraphicsDeviceRef_submitCommands()::swapchains[i] has outdated commands in submitted command list"
					))
			}
		}
	}

	//Lock all resources linked to command lists

	for(U64 i = 0; i < device->pendingResources.length; ++i) {

		WeakRefPtr *res = device->pendingResources.ptr[i];

		EGraphicsTypeId id = (EGraphicsTypeId) res->typeId;

		switch (id) {

			case EGraphicsTypeId_DeviceBuffer:
				lockPtr = &DeviceBufferRef_ptr(res)->lock;
				break;

			case EGraphicsTypeId_DeviceTexture:
				lockPtr = &DeviceTextureRef_ptr(res)->lock;
				break;

			default:
				gotoIfError(clean, Error_unimplemented(
					0, "GraphicsDeviceRef_submitCommands() pendingResources contains unsupported type"
				))
		}

		if(!lockPtr)
			continue;

		acq = SpinLock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			gotoIfError(clean, Error_invalidState(2, "GraphicsDeviceRef_submitCommands() couldn't acquire resource"))
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError(clean, ListSpinLockPtr_pushBackx(&device->currentLocks, lockPtr))

		lockPtr = NULL;
	}

	//We start counting from 1, since implementation might set fence to 0 as init.
	//We don't want a possible deadlock there.

	device->fifId = device->submitId % device->framesInFlight;
	++device->submitId;

	//Set app data

	Ns now = Time_now();

	CBufferData data = (CBufferData) {
		.frameId = (U32) device->submitId,
		.time = device->firstSubmit ? (F32)((F64)(now - device->firstSubmit) / SECOND) : 0,
		.deltaTime = device->firstSubmit ? (F32)((F64)(now - device->lastSubmit) / SECOND) : 0,
		.swapchainCount = (U32) swapchains.length
	};

	if (deltaTime >= 0) {
		data.deltaTime = deltaTime;
		data.time = time;
	}

	Buffer_memcpy(Buffer_createRef(data.appData, sizeof(data.appData)), appData);

	//Submit impl should also set the swapchains and process all command lists and swapchains.
	//This is not present here because the API impl is the one in charge of how it is threaded.

	gotoIfError(clean, GraphicsDevice_submitCommandsExt(deviceRef, commandLists, swapchains, data))

	//Add resources from command lists to resources in flight

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	for (U64 j = 0; j < commandLists.length; ++j) {

		CommandListRef *cmdRef = commandLists.ptr[j];
		CommandList *cmd = CommandListRef_ptr(cmdRef);

		for(U64 i = 0; i < cmd->resources.length; ++i) {

			RefPtr *ptr = cmd->resources.ptr[i];

			if(ListRefPtr_contains(*currentFlight, ptr, 0, NULL))
				continue;

			gotoIfError(clean, ListRefPtr_pushBackx(currentFlight, ptr))
			RefPtr_inc(ptr);
		}
	}

	//Ensure our next fence value is used

	device->lastSubmit = Time_now();

	if(!device->firstSubmit)
		device->firstSubmit = device->lastSubmit;

	device->pendingBytes = 0;

clean:

	if(lockPtr)
		SpinLock_unlock(lockPtr);

	for(U64 i = 0; i < device->currentLocks.length; ++i)
		SpinLock_unlock(device->currentLocks.ptrNonConst[i]);

	ListSpinLockPtr_clear(&device->currentLocks);
	return err;
}

U64 GraphicsDeviceRef_getMemoryBudget(GraphicsDeviceRef *deviceRef, Bool isDeviceLocal) {

	if(!deviceRef || deviceRef->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return U64_MAX;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(device->info.type != EGraphicsDeviceType_Dedicated && isDeviceLocal)
		return 0;

	return GraphicsDevice_getMemoryBudgetExt(device, isDeviceLocal);
}

Error GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef) {

	if(!deviceRef || deviceRef->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_wait()::deviceRef is required");

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	Error err;
	const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		return Error_invalidOperation(0, "GraphicsDeviceRef_wait() device's lock couldn't be acquired");

	gotoIfError(clean, GraphicsDeviceRef_waitExt(deviceRef))

	for (U64 i = 0; i < device->framesInFlight; ++i) {

		//Release resources that were in flight.
		//This might cause resource deletions because we might be the last one releasing them.
		//For example temporary staging resources are released this way.

		ListRefPtr *inFlight = &device->resourcesInFlight[i];

		for (U64 j = 0; j < inFlight->length; ++j)
			RefPtr_dec(inFlight->ptrNonConst + j);

		gotoIfError(clean, ListRefPtr_clear(inFlight))

		//Release all allocations of buffer that was in flight

		if(!AllocationBuffer_freeAll(&device->stagingAllocations[i]))
			gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_wait() couldn't AllocationBuffer_freeAll"))
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return err;
}
