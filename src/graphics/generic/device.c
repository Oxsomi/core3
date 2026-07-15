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

//graphics/generic/device.c

#include "types/container/list_impl.h"
#include "types/base/platform_types.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/device.h"
#include "types/container/memory_stream.h"
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
#include "types/container/buffer.h"
#include "platforms/logx.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/container/ref_ptr.h"
#include "formats/oiSH/sh_file.h"
#include "types/base/time.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/base/constants.h"

#include <stddef.h>

TListNamedImpl(ListSpinLockPtr);
TListNamedImpl(ListCommandListRef);
TListNamedImpl(ListSwapchainRef);

void GraphicsDevice_free(GraphicsDevice *device, const Allocator *alloc) {

	(void)alloc;

	if(!device)
		return;

	for(U64 i = 0; i < 2; ++i)
		RefPtr_dec(&device->copyShaders[i]);

	RefPtr_dec(&device->copyPipelineLayout);        //Even though it's 'ref counted' it's internal so destruction order matters
	RefPtr_dec(&device->copyDescLayout);
	RefPtr_dec(&device->copyDescPushDesc);
	RefPtr_dec(&device->defaultDescriptorTable);
	RefPtr_dec(&device->defaultPipelineLayout);
	RefPtr_dec(&device->defaultDescLayout);
	RefPtr_dec(&device->defaultCBufferLayout);
	RefPtr_dec(&device->defaultDescriptorHeaps);

	for(U64 i = 0; i < device->framesInFlight; ++i)
		RefPtr_dec(&device->frameData[i]);

	RefPtr_dec(&device->staging);

	SpinLock_lock(&device->lock, U64_MAX);
	SpinLock_lock(&device->allocator.lock, U64_MAX);
	ListWeakRefPtr_free(&device->pendingResources, alloc);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
		AllocationBuffer_free(&device->stagingAllocations[i], alloc);

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
			Log_printCapturedStackTraceCustom(
				alloc, (const void**) block.stackTrace, sizeof(block.stackTrace) / sizeof(void*),
				ELogLevel_Warn, ELogOptions_NewLine
			);
	}

	ListDeviceMemoryBlock_free(&device->allocator.blocks, alloc);
	ListSpinLockPtr_free(&device->currentLocks, alloc);

	for(U64 i = 0; i < device->framesInFlight; ++i) {

		if(device->resourcesInFlight[i].length)
			Log_warnLnx(
			"GraphicsDevice_free detected resourcesInFlight still left on free. "
			"This shouldn't be possible, as all resources should already be released!"
		);

		ListRefPtr_free(&device->resourcesInFlight[i], alloc);
	}

	RefPtr_dec(&device->instance);
}

Bool GraphicsDeviceRef_createPrebuiltShaders(GraphicsDeviceRef *deviceRef, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	Buffer tempBuffer = Buffer_createNull();
	MemoryStreamRef *tempStream = NULL;
	SHFile tmpBinary = (SHFile) { 0 };
	DescriptorLayoutInfo pushDescriptors = (DescriptorLayoutInfo) { 0 };
	DescriptorLayoutInfo info = (DescriptorLayoutInfo) { 0 };

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	//Load prebuilt shaders

	const CharString virtualSection = CharString_createRefCStrConst("//OxC3_graphics");
	const RefPtrType memStreamType = MemoryStream_makeType(alloc);
	const RefPtrType fileHandleType = FileHandle_makeType(alloc);

	gotoIfError3(clean, File_loadVirtual(&virtualSection, &memStreamType, NULL, NULL, alloc, e_rr));

	//image_copy shaders, only the 2nd can rotate images

	const CharString path = CharString_createRefCStrConst("//OxC3_graphics/shaders/image_copy.oiSH");
	gotoIfError3(clean, File_read(&path, U64_MAX, 0, 0, &fileHandleType, &tempBuffer, e_rr));

	gotoIfError3(clean, MemoryStream_createFromBufferRegion(
		tempBuffer, 0, 0, EMemoryStreamFlags_None, &memStreamType, &tempStream, e_rr
	));

	U64 streamOffset = 0;
	gotoIfError3(clean, SHFile_read((StreamRef*)tempStream, &streamOffset, false, alloc, &tmpBinary, e_rr));

	CharString defines[2] = {
		CharString_createRefCStrConst("ROTATE"), CharString_createRefCStrConst("1")
	};

	for(U64 i = 0; i < 2; ++i) {

		ListCharString definesList = (ListCharString) { 0 };

		if(i)
			gotoIfError3(clean, ListCharString_createRefConst(defines, 2, &definesList, e_rr));

		CharString entry = CharString_createRefCStrConst("mainSingle");

		U32 mainSingle = GraphicsDeviceRef_getFirstShaderEntry(
			deviceRef,
			&tmpBinary,
			&entry,
			&definesList,
			ESHExtension_None,
			ESHExtension_None
		);

		if(mainSingle == U32_MAX)
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_createPrebuiltShaders() couldn't find entrypoint"));

		//Our copy shader uses push descriptors and push constants,
		//But just in case, it could also handle making a descriptor layout.

		if(!device->copyPipelineLayout) {

			DescriptorBinding pushConstants = (DescriptorBinding) { 0 };


			gotoIfError3(clean, GraphicsDeviceRef_detectLayoutFromEntry(
				deviceRef,
				&tmpBinary,
				mainSingle,
				EDescriptorLayoutFlags_InternalWeakDeviceRef,
				EDetectDescriptorLayoutFlags_AssumePushDescriptors | EDetectDescriptorLayoutFlags_AssumePushConstants,
				NULL,
				NULL,
				&pushConstants,
				&info,
				&pushDescriptors,
				e_rr
			));

			if(info.bindings.length) {
				const CharString copyDescLayoutName = CharString_createRefCStrConst("Copy image desc layout");
				gotoIfError3(clean, GraphicsDeviceRef_createDescriptorLayout(
					deviceRef, &info, &copyDescLayoutName, &device->copyDescLayout, e_rr
				));
			}

			if(pushDescriptors.bindings.length) {
				const CharString copyDescPushName = CharString_createRefCStrConst("Copy image push desc layout");
				gotoIfError3(clean, GraphicsDeviceRef_createDescriptorLayout(
					deviceRef, &pushDescriptors, &copyDescPushName, &device->copyDescPushDesc, e_rr
				));
			}

			PipelineLayoutInfo pipelineInfo = (PipelineLayoutInfo) {
				.flags = EPipelineLayoutFlags_InternalWeakDeviceRef,
				.bindings = device->copyDescLayout,
				.pushDescriptors = device->copyDescPushDesc,
				.pushConstants = pushConstants
			};

			const CharString copyPipelineLayout = CharString_createRefCStrConst("Copy image pipeline layout");

			gotoIfError3(clean, GraphicsDeviceRef_createPipelineLayout(
				deviceRef,
				&pipelineInfo,
				&copyPipelineLayout,
				&device->copyPipelineLayout,
				e_rr
			));
		}

		CharString copyImageName = CharString_createRefCStrConst("Copy image shader");

		gotoIfError3(clean, GraphicsDeviceRef_createPipelineCompute(
			deviceRef,
			&tmpBinary,
			&copyImageName,
			mainSingle,
			EPipelineFlags_InternalWeakDeviceRef,
			device->copyPipelineLayout,
			&device->copyShaders[i],
			e_rr
		));
	}

clean:
	DescriptorLayoutInfo_free(&info, alloc);
	DescriptorLayoutInfo_free(&pushDescriptors, alloc);
	SHFile_free(&tmpBinary, alloc);
	RefPtr_dec(&tempStream);
	Buffer_free(&tempBuffer, alloc);
	return s_uccess;
}

typedef enum EDescriptorTypeCount {

	EDescriptorTypeCount_Texture2D          = 131072,
	EDescriptorTypeCount_TextureCube        = 32768,
	EDescriptorTypeCount_Texture3D          = 32768,

	EDescriptorTypeCount_Buffer             = 131072,
	EDescriptorTypeCount_RWBuffer           = 131072,

	EDescriptorTypeCount_RWTexture3D        = 32768,
	EDescriptorTypeCount_RWTexture3Di       = 8192,
	EDescriptorTypeCount_RWTexture3Du       = 8192,
	EDescriptorTypeCount_RWTexture2D        = 131072,
	EDescriptorTypeCount_RWTexture2Di       = 16384,
	EDescriptorTypeCount_RWTexture2Du       = 16384,

	EDescriptorTypeCount_Sampler            = 1024,
	EDescriptorTypeCount_TLASExt            = 16,

	//DirectX bindings

	EDescriptorTypeOffset_Texture2D         = 0,
	EDescriptorTypeOffset_TextureCube       = EDescriptorTypeOffset_Texture2D + EDescriptorTypeCount_Texture2D,
	EDescriptorTypeOffset_Texture3D         = EDescriptorTypeOffset_TextureCube + EDescriptorTypeCount_TextureCube,
	EDescriptorTypeOffset_Buffer            = EDescriptorTypeOffset_Texture3D + EDescriptorTypeCount_Texture3D,
	EDescriptorTypeOffset_TLASExt           = EDescriptorTypeOffset_Buffer + EDescriptorTypeCount_Buffer,

	EDescriptorTypeOffset_RWBuffer          = 0,
	EDescriptorTypeOffset_RWTexture3D       = EDescriptorTypeOffset_RWBuffer + EDescriptorTypeCount_RWBuffer,
	EDescriptorTypeOffset_RWTexture3Di      = EDescriptorTypeOffset_RWTexture3D + EDescriptorTypeCount_RWTexture3D,
	EDescriptorTypeOffset_RWTexture3Du      = EDescriptorTypeOffset_RWTexture3Di + EDescriptorTypeCount_RWTexture3Di,
	EDescriptorTypeOffset_RWTexture2D       = EDescriptorTypeOffset_RWTexture3Du + EDescriptorTypeCount_RWTexture3Du,
	EDescriptorTypeOffset_RWTexture2Di      = EDescriptorTypeOffset_RWTexture2D + EDescriptorTypeCount_RWTexture2D,
	EDescriptorTypeOffset_RWTexture2Du      = EDescriptorTypeOffset_RWTexture2Di + EDescriptorTypeCount_RWTexture2Di,

	EDescriptorTypeOffset_Sampler           = 0,

	EDescriptorTypeCount_Buffers            =
		EDescriptorTypeCount_Buffer + EDescriptorTypeCount_RWBuffer,

	EDescriptorTypeCount_TexturesRead       =
		EDescriptorTypeCount_Texture2D + EDescriptorTypeCount_Texture3D + EDescriptorTypeCount_TextureCube,

	EDescriptorTypeCount_TexturesRW         =
		EDescriptorTypeCount_RWTexture2D + EDescriptorTypeCount_RWTexture2Du + EDescriptorTypeCount_RWTexture2Di +
		EDescriptorTypeCount_RWTexture3D + EDescriptorTypeCount_RWTexture3Du + EDescriptorTypeCount_RWTexture3Di

} EDescriptorTypeCount;

Bool GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef,
	const GraphicsDeviceInfo *info,
	EGraphicsDeviceFlags flags,
	EGraphicsBufferingMode bufferMode,
	GraphicsDeviceRef **deviceRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!instanceRef || !info || !deviceRef)
		retError(clean, Error_nullPointer(
			!instanceRef ? 0 : (!info ? 1 : 2),
			"GraphicsDeviceRef_create()::instanceRef, info and deviceRef are required"
		));

	if(instanceRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsInstance)
		retError(clean, Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_create()::instanceRef was an invalid type"
		));

	if(*deviceRef)
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_create()::*deviceRef wasn't NULL, probably indicates memleak"
		));

	//Create RefPtr

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(instanceRef);
	const Allocator *alloc = instance->alloc;

	gotoIfError3(clean, RefPtr_create(&instance->types.device, deviceRef, e_rr));
	allocated = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);

	gotoIfError3(clean, ListWeakRefPtr_reserve(&device->pendingResources, 128, alloc, e_rr));

	if (bufferMode < 2 || bufferMode > 3)
		bufferMode = _PLATFORM_TYPE == PLATFORM_ANDROID ? EGraphicsBufferingMode_Double : EGraphicsBufferingMode_Triple;

	device->info = *info;
	device->flags = flags;
	device->framesInFlight = (U8) bufferMode;

	if(device->flags & EGraphicsDeviceFlags_DisableRt)
		device->info.capabilities.features &=~ (
			EGraphicsFeatures_Raytracing         |
			EGraphicsFeatures_RayPipeline        |
			EGraphicsFeatures_RayQuery           |
			EGraphicsFeatures_RayMicromapOpacity |
			EGraphicsFeatures_RayMotionBlur      |
			EGraphicsFeatures_RayReorder         |
			EGraphicsFeatures_RayValidation
		);

	Bool isDebugInstance = instance->flags & EGraphicsInstanceFlags_IsDebug;

	if((device->flags & EGraphicsDeviceFlags_IsDebug) && !isDebugInstance)
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_create() tried to create debug device but the instance had it disabled"
		));

	#ifndef NDEBUG
		if(!(device->flags & EGraphicsDeviceFlags_DisableDebug) && isDebugInstance)
			device->flags |= EGraphicsDeviceFlags_IsDebug;
	#endif

	if(!(device->flags & EGraphicsDeviceFlags_IsDebug))
		device->info.capabilities.features &=~ EGraphicsFeatures_RayValidation;

	gotoIfError3(clean, RefPtr_inc(instanceRef));
	device->instance = instanceRef;

	//Create mem alloc, set info/instance/pending resources

	device->allocator = (DeviceMemoryAllocator) { .device = device };
	gotoIfError3(clean, ListDeviceMemoryBlock_reserve(&device->allocator.blocks, 16, alloc, e_rr));

	//Create in flight resource refs

	for(U64 i = 0; i < device->framesInFlight; ++i)
		gotoIfError3(clean, ListRefPtr_reserve(&device->resourcesInFlight[i], 64, alloc, e_rr));

	//Create extended device

	gotoIfError3(clean, GraphicsDevice_initExt(instance, info, deviceRef, e_rr));

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

		gotoIfError3(clean, GraphicsDeviceRef_createDescriptorHeap(
			*deviceRef, &heapInfo, &name, &device->defaultDescriptorHeaps, e_rr
		));

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

		gotoIfError3(clean, ListDescriptorBinding_createRefConst(bindings, descBindings, &descLayoutInfo.bindings, e_rr));
		gotoIfError3(clean, ListCharString_createRefConst(bindingNames, descBindings, &descLayoutInfo.bindingNames, e_rr));

		name = CharString_createRefCStrConst("Default descriptor layout");
		gotoIfError3(clean, GraphicsDeviceRef_createDescriptorLayout(
			*deviceRef, &descLayoutInfo, &name, &device->defaultDescLayout, e_rr
		));

		name = CharString_createRefCStrConst("Default descriptor table");
		gotoIfError3(clean, DescriptorHeapRef_createDescriptorTable(
			device->defaultDescriptorHeaps,
			device->defaultDescLayout,
			EDescriptorTableFlags_InternalWeakDeviceRef,
			name,
			&device->defaultDescriptorTable, e_rr
		));

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

		gotoIfError3(clean, ListDescriptorBinding_createRefConst(&cbv, 1, &descLayoutInfo.bindings, e_rr));
		gotoIfError3(clean, ListCharString_createRefConst(&cbvName, 1, &descLayoutInfo.bindingNames, e_rr));

		name = CharString_createRefCStrConst("Constant buffer push descriptor");
		gotoIfError3(clean, GraphicsDeviceRef_createDescriptorLayout(
			*deviceRef, &descLayoutInfo, &name, &device->defaultCBufferLayout, e_rr
		));

		//Create pipeline layout

		name = CharString_createRefCStrConst("Default pipeline layout");
		PipelineLayoutInfo pipelineLayoutInfo = (PipelineLayoutInfo) {
			.flags = EPipelineLayoutFlags_InternalWeakDeviceRef,
			.bindings = device->defaultDescLayout,
			.pushDescriptors = device->defaultCBufferLayout
		};

		gotoIfError3(clean, GraphicsDeviceRef_createPipelineLayout(
			*deviceRef, &pipelineLayoutInfo, &name, &device->defaultPipelineLayout, e_rr
		));
	}

	//Determine some flushing and block size sizes for the current GPU

	//Determine when we need to flush.
	//As a rule of thumb I decided for 20% occupied mem by just copies.
	//Or if there's distinct shared mem available too it can allocate 10% more in that memory too
	// (as long as it doesn't exceed 33%).
	//Flush threshold is kept under 4 GiB to avoid TDRs because even if the mem is available it might be slow.
	//TODO: Instead, we should base this on a quick PCIE benchmark,
	// which is tricky because there might be other things using PCIE right now.
	// Maybe we can read out this speed directly? It's more future proof than this (PCIE8 for example could be so much faster)

	const Bool isDistinct = device->info.type == EGraphicsDeviceType_Dedicated;
	U64 cpuHeapSize = device->info.capabilities.sharedMemory;
	U64 gpuHeapSize = device->info.capabilities.dedicatedMemory;

	device->flushThreshold = U64_min(
		4 * GIBI,
		isDistinct ? U64_min(gpuHeapSize / 3, cpuHeapSize / 10 + gpuHeapSize / 5) :
		cpuHeapSize / 5
	);

	device->flushThresholdPrimitives = 20 * MIBI / 3;        //20M vertices per frame limit (TODO: Base this on build time benchmark too)

	//Block sizes based on memory of each device (CPU or GPU):
	// 0 -  6GB ("4GB"):   64MB
	// 6 - 12GB ("8GB"):  128MB
	//12 - 24GB ("16GB"): 256MB
	//24GB+     ("32GB"): 512MB
	//E.g. Memory allocated CPU visible with a dGPU with 32GB available would use 512MB chunks

	if (!isDistinct) {        //Assume 50/50 split to take a conservative block size approach
		cpuHeapSize >>= 1;
		gpuHeapSize >>= 1;
	}

	device->blockSizeCpu = (64 * MIBI) << (U64) F64_clamp(F64_round(F64_log2((F64)cpuHeapSize)) - 32, 0, 3);
	device->blockSizeGpu = (64 * MIBI) << (U64) F64_clamp(F64_round(F64_log2((F64)gpuHeapSize)) - 32, 0, 3);

	//Create constant buffer and staging buffer / allocators

	//Allocate staging buffer.
	//For block size 256MiB: 64 MiB / NBuffering (2 or 3) = 32MiB or 21.333 MiB per frame.
	//For block size 64MiB: 16 MiB / NBuffering (2 or 3) = 8MiB or 5.3 MiB per frame.
	//    This block size is generally used in very memory limited systems (such as Android devices)
	//    that generally use shared mem (so won't push much over staging buffer).
	//If out of mem this will grow to be bigger.
	//But it's only used for "small" allocations (< 25% of staging buffer)
	//If a lot of these larger allocations are found it will resize the staging buffer to try to encompass it too.

	U64 stagingSize = device->blockSizeCpu / 4;
	gotoIfError3(clean, GraphicsDeviceRef_resizeStagingBuffer(*deviceRef, stagingSize, e_rr));

	//Allocate UBO

	CharString perFrameData = CharString_createRefCStrConst("Per frame data");

	for(U64 i = 0; i < device->framesInFlight; ++i)
		gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
			*deviceRef,
			EDeviceBufferUsage_Uniform,
			EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
			NULL,
			&perFrameData,
			sizeof(CBufferData), &device->frameData[i], e_rr
		));

	//Load prebuilt shaders

	gotoIfError3(clean, GraphicsDeviceRef_createPrebuiltShaders(*deviceRef, e_rr));

clean:

	if (!s_uccess && allocated)
		RefPtr_dec(deviceRef);

	return s_uccess;
}

Bool GraphicsDeviceRef_checkShaderFeatures(
	GraphicsDeviceRef *deviceRef, const SHBinaryInfo *bin, const SHEntry *entry, Error *e_rr
) {

	Bool s_uccess = true;

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_checkShaderFeatures()::deviceRef is required"));

	if(!bin || !entry)
		retError(clean, Error_nullPointer(
			!bin ? 1 : 2, "GraphicsDeviceRef_checkShaderFeatures()::bin and entry are required"
		));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ESHExtension extensions = (bin->identifier.extensions &~ bin->dormantExtensions) & ESHExtension_All;

	EGraphicsFeatures features = EGraphicsFeatures_None;
	EDxGraphicsFeatures featuresDx = EDxGraphicsFeatures_None;
	EGraphicsDataTypes dataTypes = EGraphicsDataTypes_None;

	if(extensions & ESHExtension_SubgroupOperations)        features |= EGraphicsFeatures_SubgroupOperations;
	if(extensions & ESHExtension_SubgroupArithmetic)        features |= EGraphicsFeatures_SubgroupArithmetic;
	if(extensions & ESHExtension_SubgroupShuffle)           features |= EGraphicsFeatures_SubgroupShuffle;

	if(extensions & ESHExtension_Multiview)                 features |= EGraphicsFeatures_Multiview;

	if(extensions & ESHExtension_RayQuery)                  features |= EGraphicsFeatures_RayQuery;
	if(extensions & ESHExtension_RayMicromapOpacity)        features |= EGraphicsFeatures_RayMicromapOpacity;
	if(extensions & ESHExtension_RayMotionBlur)             features |= EGraphicsFeatures_RayMotionBlur;
	if(extensions & ESHExtension_RayReorder)                features |= EGraphicsFeatures_RayReorder;

	if(extensions & ESHExtension_ComputeDeriv)              features |= EGraphicsFeatures_ComputeDeriv;
	if(extensions & ESHExtension_MeshTaskTexDeriv)          features |= EGraphicsFeatures_MeshTaskTexDeriv;

	if(extensions & ESHExtension_WriteMSTexture)            features |= EGraphicsFeatures_WriteMSTexture;

	if(extensions & ESHExtension_Bindless)                  features |= EGraphicsFeatures_Bindless;

	if(extensions & ESHExtension_F64)                       dataTypes |= EGraphicsDataTypes_F64;
	if(extensions & ESHExtension_I64)                       dataTypes |= EGraphicsDataTypes_I64;

	if(extensions & ESHExtension_16BitTypes)                dataTypes |= EGraphicsDataTypes_I16 | EGraphicsDataTypes_F16;

	if(extensions & ESHExtension_AtomicI64)                 dataTypes |= EGraphicsDataTypes_AtomicI64;
	if(extensions & ESHExtension_AtomicF32)                 dataTypes |= EGraphicsDataTypes_AtomicF32;
	if(extensions & ESHExtension_AtomicF64)                 dataTypes |= EGraphicsDataTypes_AtomicF64;

	if(extensions & ESHExtension_PAQ)                       featuresDx |= EDxGraphicsFeatures_PAQ;

	if ((bin->identifier.shaderVersion >> 8) == 6)           //Check shader model compatibility
		switch ((U8) bin->identifier.shaderVersion) {
			case 6:                                         featuresDx |= EDxGraphicsFeatures_SM6_6;    break;
			case 7:                                         featuresDx |= EDxGraphicsFeatures_SM6_7;    break;
			case 8:                                         featuresDx |= EDxGraphicsFeatures_SM6_8;    break;
			case 9:                                         featuresDx |= EDxGraphicsFeatures_SM6_9;    break;
			case 10:                                        featuresDx |= EDxGraphicsFeatures_SM6_10;   break;
			default:                                                                                    break;
		}

	if(entry->waveSize >> 4)                                featuresDx |= EDxGraphicsFeatures_WaveSizeMinMax;
	else if(entry->waveSize)                                featuresDx |= EDxGraphicsFeatures_WaveSize;

	if((device->info.capabilities.features & features) != features)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the features is missing"));

	if((device->info.capabilities.dataTypes & dataTypes) != dataTypes)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the dataTypes is missing"));

	if(bin->vendorMask != ((1 << ESHVendor_Count) - 1))
		if(!((bin->vendorMask >> device->info.vendor) & 1))
			retError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_checkShaderFeatures() binary is incompatible with vendor"
			));

	//Check for D3D12 features, shader models and DXIL

	if(GraphicsInstanceRef_ptr(device->instance)->api == EGraphicsApi_Direct3D12) {

		if((device->info.capabilities.featuresExt & (U32)featuresDx) != (U32)featuresDx)
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the featuresDx is missing"));

		if(!Buffer_length(bin->binaries[ESHBinaryType_DXIL]))
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() DXIL binary is missing"));
	}

	//Check for SPIRV

	else if(!Buffer_length(bin->binaries[ESHBinaryType_SPIRV]))
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() SPIRV binary is missing"));

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_removePending(GraphicsDeviceRef *deviceRef, RefPtr *resource) {

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		return false;

	Bool supported = false;

	const EGraphicsTypeId type = (EGraphicsTypeId) resource->refPtrType->typeId;
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
	Bool s_uccess = true;

	if (found == U64_MAX)
		goto clean;

	gotoIfError3(clean, ListWeakRefPtr_erase(pendingList, found, NULL));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess;
}

Bool GraphicsDeviceRef_handleNextFrame(GraphicsDeviceRef *deviceRef, void *commandBuffer, Error *e_rr) {

	Bool s_uccess = true;

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_handleNextFrame()::deviceRef is required"));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!SpinLock_isLockedForThread(&device->lock))
		retError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_handleNextFrame() requires device to be locked by caller"
		));

	//Release resources that were in flight.
	//This might cause resource deletions because we might be the last one releasing them.
	//For example temporary staging resources are released this way.

	ListRefPtr *inFlight = &device->resourcesInFlight[device->fifId];

	for (U64 i = 0; i < inFlight->length; ++i)
		RefPtr_dec(inFlight->ptrNonConst + i);

	gotoIfError3(clean, ListRefPtr_clear(inFlight, e_rr));

	//Release all allocations of buffer that was in flight

	AllocationBuffer_freeAll(&device->stagingAllocations[device->fifId]);

	//Update buffer data

	for(U64 i = 0; i < device->pendingResources.length; ++i) {

		RefPtr *pending = device->pendingResources.ptr[i];

		EGraphicsTypeId type = (EGraphicsTypeId) pending->refPtrType->typeId;

		switch(type) {

			case EGraphicsTypeId_DeviceBuffer:
				gotoIfError3(clean, DeviceBufferRef_flushExt(commandBuffer, deviceRef, pending, e_rr));
				break;

			case EGraphicsTypeId_DeviceTexture:
				gotoIfError3(clean, DeviceTextureRef_flushExt(commandBuffer, deviceRef, pending, e_rr));
				break;

			default:
				retError(clean, Error_unsupportedOperation(
					5, "GraphicsDeviceRef_handleNextFrame() unsupported pending graphics object"
				));
		}
	}

	gotoIfError3(clean, ListWeakRefPtr_clear(&device->pendingResources, e_rr));

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_resizeStagingBuffer(GraphicsDeviceRef *deviceRef, U64 newSize, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_resizeStagingBuffer()::deviceRef is required"));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	newSize = (((newSize + 2) / 3 + 4095) &~ 4095) * 3;            //Align to ensure we never get incompatible staging buffers

	if (device->staging) {

		//"Free" staging buffer.
		//If the staging buffer was already in flight this won't do anything until it's out of flight.

		for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
			AllocationBuffer_free(&device->stagingAllocations[i], alloc);

		RefPtr_dec(&device->staging);
	}

	CharString stagingBufferName = CharString_createRefCStrConst("Staging buffer");

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_None,
		EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
		NULL,
		&stagingBufferName,
		newSize, &device->staging, e_rr
	));

	const DeviceBuffer *staging = DeviceBufferRef_ptr(device->staging);
	const Buffer stagingBuffer = Buffer_createRef(staging->resource.mappedMemoryExt, newSize);

	for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i) {

		const AllocationBufferCreate create = (AllocationBufferCreate) {
			.size = newSize / 3,
			.alloc = alloc,
			.allocationBuffer = &device->stagingAllocations[i]
		};

		gotoIfError3(clean, AllocationBuffer_createRefFromRegion(&create, stagingBuffer, newSize / 3 * i, e_rr));
	}

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_submitCommands(
	GraphicsDeviceRef *deviceRef,
	const ListCommandListRef *commandLists,
	const ListSwapchainRef *swapchains,
	const Buffer *appData,
	F32 deltaTime,
	F32 time,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	GraphicsDevice *device = NULL;
	SpinLock *lockPtr = NULL;

	//Validation

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_submitCommands()::deviceRef is required"));

	if((!swapchains || !swapchains->length) && (!commandLists || !commandLists->length))
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_submitCommands()::swapchains or commandLists is required"
		));

	if(swapchains && swapchains->length > sizeof(((CBufferData*)NULL)->swapchains) / 8)
		retError(clean, Error_invalidParameter(
			2, 1, "GraphicsDeviceRef_submitCommands()::swapchains.length is limited to 16"
		));

	if(appData && Buffer_length(*appData) > sizeof(((CBufferData*)NULL)->appData))
		retError(clean, Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_submitCommands()::appData is limited to 368 bytes"
		));

	device = GraphicsDeviceRef_ptr(deviceRef);

	lockPtr = &device->lock;
	ELockAcquire acq = SpinLock_lock(lockPtr, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_submitCommands() couldn't acquire device lock"));

	if(acq == ELockAcquire_Acquired)
		gotoIfError3(clean, ListSpinLockPtr_pushBack(&device->currentLocks, lockPtr, alloc, e_rr));

	lockPtr = NULL;

	//Validate command lists

	for(U64 i = 0; i < (!commandLists ? 0 : commandLists->length); ++i) {

		CommandListRef *cmdRef = commandLists->ptr[i];

		if(!cmdRef || cmdRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_CommandList)
			retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_submitCommands()::commandLists[i] is required"));

		CommandList *cmd = CommandListRef_ptr(cmdRef);

		if(cmd->device != deviceRef)
			retError(clean, Error_unsupportedOperation(
				0, "GraphicsDeviceRef_submitCommands()::commandLists[i]'s device and the current device are different"
			));

		lockPtr = &cmd->lock;
		acq = SpinLock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			retError(clean, Error_invalidState(1, "GraphicsDeviceRef_submitCommands()::commandLists[i] couldn't be acquired"));
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError3(clean, ListSpinLockPtr_pushBack(&device->currentLocks, lockPtr, alloc, e_rr));

		lockPtr = NULL;

		if(cmd->state != ECommandListState_Closed)
			retError(clean, Error_invalidParameter(
				1, (U32)i, "GraphicsDeviceRef_submitCommands()::commandLists[i] wasn't closed properly"));
	}

	for (U64 i = 0; i < (!swapchains ? 0 : swapchains->length); ++i) {

		SwapchainRef *swapchainRef = swapchains->ptr[i];

		for(U64 j = 0; j < i; ++j)
			if(swapchainRef == swapchains->ptr[j])
				retError(clean, Error_invalidParameter(
					2, 2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is duplicated"
				));

		if(!swapchainRef || swapchainRef->refPtrType->typeId != (ETypeId) EGraphicsTypeId_Swapchain)
			retError(clean, Error_nullPointer(2, "GraphicsDeviceRef_submitCommands()::swapchains[i] is required"));

		Swapchain *swapchaini = SwapchainRef_ptr(swapchainRef);

		if(swapchaini->base.resource.device != deviceRef)
			retError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_submitCommands()::swapchains[i]'s device and the current device are different"
			));

		lockPtr = &swapchaini->lock;
		acq = SpinLock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {

			lockPtr = NULL;

			retError(clean, Error_invalidState(
				2, "GraphicsDeviceRef_submitCommands()::swapchains[i] couldn't acquire lock"));
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError3(clean, ListSpinLockPtr_pushBack(&device->currentLocks, lockPtr, alloc, e_rr));

		lockPtr = NULL;

		//Validate if the swapchain with a different version is bound, if yes, we have a stale cmdlist

		for (U64 j = 0; j < (!commandLists ? 0 : commandLists->length); ++j) {

			CommandListRef *cmdRef = commandLists->ptr[i];
			CommandList *cmd = CommandListRef_ptr(cmdRef);

			for(U64 k = 0; k < cmd->activeSwapchains.length; ++k) {

				DeviceResourceVersion vK = cmd->activeSwapchains.ptr[k];

				if(vK.resource == swapchainRef && vK.version != swapchaini->versionId)
					retError(clean, Error_invalidState(
						0, "GraphicsDeviceRef_submitCommands()::swapchains[i] has outdated commands in submitted command list"
					));
			}
		}
	}

	//Lock all resources linked to command lists

	for(U64 i = 0; i < device->pendingResources.length; ++i) {

		WeakRefPtr *res = device->pendingResources.ptr[i];

		EGraphicsTypeId id = (EGraphicsTypeId) res->refPtrType->typeId;

		switch (id) {

			case EGraphicsTypeId_DeviceBuffer:
				lockPtr = &DeviceBufferRef_ptr(res)->lock;
				break;

			case EGraphicsTypeId_DeviceTexture:
				lockPtr = &DeviceTextureRef_ptr(res)->lock;
				break;

			default:
				retError(clean, Error_unimplemented(
					0, "GraphicsDeviceRef_submitCommands() pendingResources contains unsupported type"
				));
		}

		if(!lockPtr)
			continue;

		acq = SpinLock_lock(lockPtr, U64_MAX);

		if(acq < ELockAcquire_Success) {
			lockPtr = NULL;
			retError(clean, Error_invalidState(2, "GraphicsDeviceRef_submitCommands() couldn't acquire resource"));
		}

		if(acq == ELockAcquire_Acquired)
			gotoIfError3(clean, ListSpinLockPtr_pushBack(&device->currentLocks, lockPtr, alloc, e_rr));

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
		.swapchainCount = (U32) (!swapchains ? 0 : swapchains->length)
	};

	if (deltaTime >= 0) {
		data.deltaTime = deltaTime;
		data.time = time;
	}

	if(appData)
		Buffer_memcpy(Buffer_createRef(data.appData, sizeof(data.appData)), *appData);

	//Submit impl should also set the swapchains and process all command lists and swapchains.
	//This is not present here because the API impl is the one in charge of how it is threaded.

	gotoIfError3(clean, GraphicsDevice_submitCommandsExt(deviceRef, commandLists, swapchains, &data, e_rr));

	//Add resources from command lists to resources in flight

	ListRefPtr *currentFlight = &device->resourcesInFlight[device->fifId];

	for (U64 j = 0; j < (!commandLists ? 0 : commandLists->length); ++j) {

		CommandListRef *cmdRef = commandLists->ptr[j];
		CommandList *cmd = CommandListRef_ptr(cmdRef);

		for(U64 i = 0; i < cmd->resources.length; ++i) {

			RefPtr *ptr = cmd->resources.ptr[i];

			if(ListRefPtr_contains(*currentFlight, ptr, 0, NULL))
				continue;

			gotoIfError3(clean, ListRefPtr_pushBack(currentFlight, ptr, alloc, e_rr));
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

	if (device) {

		for(U64 i = 0; i < device->currentLocks.length; ++i)
			SpinLock_unlock(device->currentLocks.ptrNonConst[i]);

		ListSpinLockPtr_clear(&device->currentLocks, e_rr);
	}

	return s_uccess;
}

U64 GraphicsDeviceRef_getMemoryBudget(GraphicsDeviceRef *deviceRef, Bool isDeviceLocal) {

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		return U64_MAX;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(device->info.type != EGraphicsDeviceType_Dedicated && isDeviceLocal)
		return 0;

	return GraphicsDevice_getMemoryBudgetExt(device, isDeviceLocal);
}

Bool GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!deviceRef || deviceRef->refPtrType->typeId != (ETypeId)EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_wait()::deviceRef is required"));

	device = GraphicsDeviceRef_ptr(deviceRef);

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidOperation(0, "GraphicsDeviceRef_wait() device's lock couldn't be acquired"));

	gotoIfError3(clean, GraphicsDeviceRef_waitExt(deviceRef, e_rr));

	for (U64 i = 0; i < device->framesInFlight; ++i) {

		//Release resources that were in flight.
		//This might cause resource deletions because we might be the last one releasing them.
		//For example temporary staging resources are released this way.

		ListRefPtr *inFlight = &device->resourcesInFlight[i];

		for (U64 j = 0; j < inFlight->length; ++j)
			RefPtr_dec(inFlight->ptrNonConst + j);

		gotoIfError3(clean, ListRefPtr_clear(inFlight, e_rr));

		//Release all allocations of buffer that was in flight

		AllocationBuffer_freeAll(&device->stagingAllocations[i]);
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess;
}

const Allocator *GraphicsDevice_getAlloc(const GraphicsDevice *device) {
	return device ? GraphicsInstanceRef_ptr(device->instance)->alloc : NULL;
}

const Allocator *GraphicsDeviceRef_getAlloc(GraphicsDeviceRef *device) {
	return GraphicsDevice_getAlloc(GraphicsDeviceRef_ptr(device));
}

const GraphicsObjectTypes *GraphicsDevice_getTypes(const GraphicsDevice *device) {
	return device ? &GraphicsInstanceRef_ptr(device->instance)->types : NULL;
}

const GraphicsObjectTypes *GraphicsDeviceRef_getTypes(GraphicsDeviceRef *device) {
	return GraphicsDevice_getTypes(GraphicsDeviceRef_ptr(device));
}
