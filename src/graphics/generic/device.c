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
#include "types/base/string_read.h"
#include "formats/oiSH/sh_file.h"
#include "types/base/string_read_helper.h"
#include "types/base/time.h"
#include "types/base/mathi.h"
#include "types/base/mathf.h"
#include "types/base/constants.h"

#include <stddef.h>

TListNamedImpl(ListSpinLockPtr);
TListNamedImpl(ListCommandListRef);
TListNamedImpl(ListSwapchainRef);
TListImpl(DevicePendingPull);

static void GraphicsDevice_completePulls(GraphicsDevice *device, U64 fifId);

void GraphicsDevice_free(void *deviceGeneric, const Allocator *alloc) {

	GraphicsDevice *device = (GraphicsDevice*) deviceGeneric;

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
	RefPtr_dec(&device->stagingReadback);

	//Unfinished pulls can't complete anymore, so only their refs and buffers are released
	// (their callbacks never fire)

	for(U64 i = 0; i < device->pendingPulls.length; ++i) {
		RefPtr_dec(&device->pendingPulls.ptrNonConst[i].resource);
		Buffer_free(&device->pendingPulls.ptrNonConst[i].textureData, alloc);
	}

	ListDevicePendingPull_free(&device->pendingPulls, alloc);

	for(U64 j = 0; j < MAX_FRAMES_IN_FLIGHT; ++j) {

		for(U64 i = 0; i < device->pullsInFlight[j].length; ++i) {
			RefPtr_dec(&device->pullsInFlight[j].ptrNonConst[i].resource);
			RefPtr_dec(&device->pullsInFlight[j].ptrNonConst[i].stagingReadback);
			Buffer_free(&device->pullsInFlight[j].ptrNonConst[i].textureData, alloc);
		}

		ListDevicePendingPull_free(&device->pullsInFlight[j], alloc);
		AllocationBuffer_free(&device->stagingReadbackAllocations[j], alloc);
	}

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
		Buffer_createRefFromBuffer(tempBuffer, true), 0, Buffer_length(tempBuffer),
		EMemoryStreamFlags_None, &memStreamType, &tempStream, e_rr
	));

	U64 streamOffset = 0;
	gotoIfError3(clean, SHFile_read((StreamRef*)tempStream, &streamOffset, false, alloc, &tmpBinary, e_rr));

	//ROTATE is a uniform (not a define), so select the permutation by its stringified value ("false" / "true").

	CharString uniformsFalse[2] = {
		CharString_createRefCStrConst("ROTATE"), CharString_createRefCStrConst("false")
	};

	CharString uniformsTrue[2] = {
		CharString_createRefCStrConst("ROTATE"), CharString_createRefCStrConst("true")
	};

	for(U64 i = 0; i < 2; ++i) {

		ListCharString uniformsList = (ListCharString) { 0 };
		gotoIfError3(clean, ListCharString_createRefConst(i ? uniformsTrue : uniformsFalse, 2, &uniformsList, e_rr));

		CharString entry = CharString_createRefCStrConst("mainSingle");

		U32 mainSingle = GraphicsDeviceRef_getFirstShaderEntry(
			deviceRef,
			&tmpBinary,
			&entry,
			NULL,                  //defines
			&uniformsList,         //uniforms: select the ROTATE permutation
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
			&entry,        //Packaged shaders keep their original SPIRV entrypoint name ("mainSingle", not "main")
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

//Lives here rather than in descriptor_layout.c because the counts and offsets it hands out are the device's,
// EDescriptorTypeCount above is what defines them and GraphicsDeviceRef_create below is the only internal user.

//Sizes a descriptor heap so it can hold exactly the layout that is going to be allocated out of it.
//Deriving this rather than hardcoding it is what lets a caller supply a layout at all: the counts used to come
// from EDescriptorTypeCount, so any layout asking for more of a type than OxC3's own would fail at table
// creation, and any layout asking for less would quietly over-allocate the heap.
//Read and write buffers share one pool, which is why both fold into maxBuffersRW.

static DescriptorHeapInfo GraphicsDevice_heapForLayout(const DescriptorLayoutInfo *layout, EDescriptorHeapFlags flags) {

	DescriptorHeapInfo heapInfo = (DescriptorHeapInfo) { .flags = flags, .maxDescriptorTables = 1 };

	for (U64 i = 0; i < layout->bindings.length; ++i) {

		const DescriptorBinding b = layout->bindings.ptr[i];
		const ESHRegisterType type = (ESHRegisterType)(b.registerType & ESHRegisterType_TypeMask);
		const Bool isWrite = (b.registerType & ESHRegisterType_IsWrite) != 0;

		switch (type) {

			case ESHRegisterType_Sampler:
			case ESHRegisterType_SamplerComparisonState:
				heapInfo.maxSamplers = (U16) U64_min(U16_MAX, (U64) heapInfo.maxSamplers + b.count);
				break;

			case ESHRegisterType_AccelerationStructure:
				heapInfo.maxAccelerationStructures =
					(U16) U64_min(U16_MAX, (U64) heapInfo.maxAccelerationStructures + b.count);
				break;

			case ESHRegisterType_SubpassInput:
				heapInfo.maxInputAttachments = (U16) U64_min(U16_MAX, (U64) heapInfo.maxInputAttachments + b.count);
				break;

			case ESHRegisterType_ConstantBuffer:
				heapInfo.maxConstantBuffers += b.count;
				break;

			default:

				if (type >= ESHRegisterType_TextureStart && type <= ESHRegisterType_TextureEnd) {

					if (b.registerType & ESHRegisterType_IsCombinedSampler)
						heapInfo.maxCombinedSamplers =
							(U16) U64_min(U16_MAX, (U64) heapInfo.maxCombinedSamplers + b.count);

					if (isWrite)
						heapInfo.maxTexturesRW += b.count;

					else heapInfo.maxTextures += b.count;
				}

				//Every remaining buffer kind, readable or writable, comes out of the same pool

				else if (type >= ESHRegisterType_BufferStart && type < ESHRegisterType_AccelerationStructure)
					heapInfo.maxBuffersRW += b.count;

				break;
		}
	}

	return heapInfo;
}

Bool GraphicsDevice_defaultBindlessLayout(
	const GraphicsDeviceInfo *info,
	ESHBinaryType binaryType,
	DescriptorLayoutInfo *result,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	ListDescriptorBinding bindingsRef = (ListDescriptorBinding) { 0 };
	ListCharString bindingNamesRef = (ListCharString) { 0 };

	DescriptorLayoutInfo layoutInfo = (DescriptorLayoutInfo) {
		.flags = EDescriptorLayoutFlags_AllowBindlessOnArrays
	};

	if(!info || !result)
		retError(clean, Error_nullPointer(
			!info ? 0 : 2, "GraphicsDevice_defaultBindlessLayout()::info and result are required"
		));

	if(result->bindings.ptr || result->bindingNames.ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "GraphicsDevice_defaultBindlessLayout()::result wasn't empty, probably indicates memleak"
		));

	//Only the two binary types that exist today have binding numbers here, and ESHBinaryType already reserves
	// AIR and WGSL, so anything else is refused rather than quietly handed DXIL's registers.
	//That is the whole reason this takes the enum instead of a bool: a third backend would have silently
	// inherited whichever branch the bool fell through to.

	if(binaryType != ESHBinaryType_SPIRV && binaryType != ESHBinaryType_DXIL)
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDevice_defaultBindlessLayout() has no default layout for this binary type"
		));

	const Bool isSpirv = binaryType == ESHBinaryType_SPIRV;

	//These names are string literals, so the refs to them stay valid for as long as the process does.

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
				.space = isSpirv ? 0 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 0 : EDescriptorTypeOffset_Sampler
			},
			.visibility = U32_MAX
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture2D,
			.count = EDescriptorTypeCount_Texture2D,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 0 : EDescriptorTypeOffset_Texture2D
			},
			.visibility = U32_MAX
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_TextureCube,
			.count = EDescriptorTypeCount_TextureCube,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 1 : EDescriptorTypeOffset_TextureCube
			},
			.visibility = U32_MAX
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture3D,
			.count = EDescriptorTypeCount_Texture3D,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 2 : EDescriptorTypeOffset_Texture3D
			},
			.visibility = U32_MAX
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer,
			.count = EDescriptorTypeCount_Buffer,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 3 : EDescriptorTypeOffset_Buffer
			},
			.visibility = U32_MAX
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_ByteAddressBuffer | ESHRegisterType_IsWrite,
			.count = EDescriptorTypeCount_RWBuffer,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 4 : EDescriptorTypeOffset_RWBuffer
			},
			.visibility = U32_MAX
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture3D | ESHRegisterType_IsWrite,
			.count = EDescriptorTypeCount_RWTexture3D,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 5 : EDescriptorTypeOffset_RWTexture3D
			},
			.visibility = U32_MAX,
			.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_Float }
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture3D | ESHRegisterType_IsWrite,
			.count = EDescriptorTypeCount_RWTexture3Di,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 6 : EDescriptorTypeOffset_RWTexture3Di
			},
			.visibility = U32_MAX,
			.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_SInt }
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture3D | ESHRegisterType_IsWrite,
			.count = EDescriptorTypeCount_RWTexture3Du,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 7 : EDescriptorTypeOffset_RWTexture3Du
			},
			.visibility = U32_MAX,
			.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_UInt }
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture2D | ESHRegisterType_IsWrite,
			.count = EDescriptorTypeCount_RWTexture2D,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 8 : EDescriptorTypeOffset_RWTexture2D
			},
			.visibility = U32_MAX,
			.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_UNorm }
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture2D | ESHRegisterType_IsWrite,
			.count = EDescriptorTypeCount_RWTexture2Di,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 9 : EDescriptorTypeOffset_RWTexture2Di
			},
			.visibility = U32_MAX,
			.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_SInt }
		},
		(DescriptorBinding) {
			.registerType = ESHRegisterType_Texture2D | ESHRegisterType_IsWrite,
			.count = EDescriptorTypeCount_RWTexture2Du,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 10 : EDescriptorTypeOffset_RWTexture2Du
			},
			.visibility = U32_MAX,
			.textureFormat = (SHTextureFormat) { .primitive = ESHTexturePrimitive_UInt }
		}
	};

	U64 descBindings = 12;

	if(info->capabilities.features & EGraphicsFeatures_Raytracing)
		bindings[descBindings++] = (DescriptorBinding) {
			.registerType = ESHRegisterType_AccelerationStructure,
			.count = EDescriptorTypeCount_TLASExt,
			.binding = (SHBinding) {
				.space = isSpirv ? 1 : OXC3_RESERVED_SPACE,
				.binding = isSpirv ? 11 : EDescriptorTypeOffset_TLASExt
			},
			.visibility = U32_MAX
		};

	//Both stack arrays have to be copied out, since the result outlives this function.

	gotoIfError3(clean, ListDescriptorBinding_createRefConst(bindings, descBindings, &bindingsRef, e_rr));
	gotoIfError3(clean, ListCharString_createRefConst(bindingNames, descBindings, &bindingNamesRef, e_rr));

	gotoIfError3(clean, ListDescriptorBinding_createCopy(bindingsRef, alloc, &layoutInfo.bindings, e_rr));
	gotoIfError3(clean, ListCharString_createCopy(bindingNamesRef, alloc, &layoutInfo.bindingNames, e_rr));

	*result = layoutInfo;
	layoutInfo = (DescriptorLayoutInfo) { 0 };

clean:
	DescriptorLayoutInfo_free(&layoutInfo, alloc);
	return s_uccess;
}

Bool GraphicsDeviceRef_create(
	GraphicsInstanceRef *instanceRef,
	const GraphicsDeviceInfo *info,
	EGraphicsDeviceFlags flags,
	EGraphicsBufferingMode bufferMode,
	const DescriptorLayoutInfo *bindlessLayout,
	GraphicsDeviceRef **deviceRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	const Allocator *alloc = NULL;
	DescriptorLayoutInfo descLayoutInfo = (DescriptorLayoutInfo) { 0 };

	if(!instanceRef || !info || !deviceRef)
		retError(clean, Error_nullPointer(
			!instanceRef ? 0 : (!info ? 1 : 2),
			"GraphicsDeviceRef_create()::instanceRef, info and deviceRef are required"
		));

	if(instanceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsInstance)
		retError(clean, Error_invalidParameter(
			0, 0, "GraphicsDeviceRef_create()::instanceRef was an invalid type"
		));

	if(*deviceRef)
		retError(clean, Error_invalidParameter(
			1, 0, "GraphicsDeviceRef_create()::*deviceRef wasn't NULL, probably indicates memleak"
		));

	//Create RefPtr

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(instanceRef);
	alloc = instance->alloc;

	gotoIfError3(clean, RefPtr_create(&instance->types.device, deviceRef, e_rr));
	allocated = true;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(*deviceRef);

	gotoIfError3(clean, ListWeakRefPtr_reserve(&device->pendingResources, 128, alloc, e_rr));

	if (bufferMode < 2 || bufferMode > 3)
		bufferMode = _PLATFORM_TYPE == PLATFORM_ANDROID ? EGraphicsBufferingMode_Double : EGraphicsBufferingMode_Triple;

	device->info = *info;
	device->flags = flags;
	device->framesInFlight = (U8) bufferMode;

	if(device->flags & EGraphicsDeviceFlags_DisableRt) {
		device->info.capabilities.features &=~ (
			EGraphicsFeatures_Raytracing         |
			EGraphicsFeatures_RayPipeline        |
			EGraphicsFeatures_RayQuery           |
			EGraphicsFeatures_RayMicromapOpacity |
			EGraphicsFeatures_RayReorder         |
			EGraphicsFeatures_RayValidation      |
			EGraphicsFeatures_RayTriPosition
		);
		device->info.capabilities.features2 &=~ (
			EGraphicsFeatures2_RayReorderActual         |
			EGraphicsFeatures2_RayMicromapOpacityActual |
			EGraphicsFeatures2_RayMicromapOpacityU8     |
			EGraphicsFeatures2_RayClusterAS             |
			EGraphicsFeatures2_RayPartitionedTLAS       |
			EGraphicsFeatures2_RayIndirectASBuild
		);
	}

	//Clearing the feature is what actually turns bindless off, since layouts, heaps and shader checks all test it.
	//Leaving it set would let a descriptor layout or an oiSH ask for bindless that the device no longer sets up.
	//DescriptorHeap always implies Bindless, so it can't survive bindless being turned off either.

	if(device->flags & EGraphicsDeviceFlags_DisableBindless) {
		device->info.capabilities.features &=~ EGraphicsFeatures_Bindless;
		device->info.capabilities.features2 &=~ EGraphicsFeatures2_DescriptorHeap;
	}

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

	//Create the default descriptor heap, layout, table and pipeline layout.
	//All of it is skipped if the device has no bindless, which includes the caller turning it off through the flag.
	//TODO: Allow user to define the heap sizes too

	if(device->info.capabilities.features & EGraphicsFeatures_Bindless) {

		//The layout is described first, because the heap is then sized to hold exactly it.
		//The caller can hand over a layout of their own, otherwise OxC3's default is what the device runs with.
		//Either way the device copies it, since the layout has to outlive whatever the caller passed in.

		CharString name = CharString_createRefCStrConst("Default descriptor layout");
		const ESHBinaryType binaryType =
			instance->api == EGraphicsApi_Direct3D12 ? ESHBinaryType_DXIL : ESHBinaryType_SPIRV;

		const Bool isSpirv = binaryType == ESHBinaryType_SPIRV;

		if (bindlessLayout) {

			descLayoutInfo.flags = bindlessLayout->flags;

			gotoIfError3(clean, ListDescriptorBinding_createCopy(
				bindlessLayout->bindings, alloc, &descLayoutInfo.bindings, e_rr
			));

			gotoIfError3(clean, ListCharString_createCopyUnderlying(
				&bindlessLayout->bindingNames, alloc, &descLayoutInfo.bindingNames, e_rr
			));
		}

		else gotoIfError3(clean, GraphicsDevice_defaultBindlessLayout(
			&device->info, binaryType, &descLayoutInfo, alloc, e_rr
		));

		//The device holds this layout for its entire lifetime, so the layout must not keep the device alive in turn.

		descLayoutInfo.flags |= EDescriptorLayoutFlags_InternalWeakDeviceRef;

		//Now the heap, sized from the layout above rather than from EDescriptorTypeCount.
		//A supplied layout asking for more of a type than OxC3's own would otherwise fail at table creation,
		// and one asking for less would leave the heap over-allocated for descriptors nobody can address.

		DescriptorHeapInfo heapInfo = GraphicsDevice_heapForLayout(
			&descLayoutInfo, EDescriptorHeapFlags_InternalWeakDeviceRef | EDescriptorHeapFlags_AllowBindless
		);

		//One constant buffer per frame in flight on top of whatever the layout asked for.
		//These are the device's own per frame globals rather than anything the caller declared, which is why
		// the old hardcoded heap reserved three of them and deriving purely from the layout would drop them.

		if(heapInfo.maxConstantBuffers < MAX_FRAMES_IN_FLIGHT)
			heapInfo.maxConstantBuffers = MAX_FRAMES_IN_FLIGHT;

		name = CharString_createRefCStrConst("Default heap");
		gotoIfError3(clean, GraphicsDeviceRef_createDescriptorHeap(
			*deviceRef, &heapInfo, &name, &device->defaultDescriptorHeaps, e_rr
		));

		//Create descriptor set & table

		name = CharString_createRefCStrConst("Default descriptor layout");
		gotoIfError3(clean, GraphicsDeviceRef_createDescriptorLayout(
			*deviceRef, &descLayoutInfo, &name, &device->defaultDescLayout, e_rr
		));

		name = CharString_createRefCStrConst("Default descriptor table");
		gotoIfError3(clean, DescriptorHeapRef_createDescriptorTable(
			device->defaultDescriptorHeaps,
			device->defaultDescLayout,
			EDescriptorTableFlags_InternalWeakDeviceRef,
			&name,
			&device->defaultDescriptorTable, e_rr
		));

		//Create push descriptor

		descLayoutInfo = (DescriptorLayoutInfo) {
			.flags = EDescriptorLayoutFlags_InternalWeakDeviceRef | EDescriptorLayoutFlags_HasPushDescriptors
		};

		DescriptorBinding cbv = (DescriptorBinding) {
			.registerType = ESHRegisterType_ConstantBuffer,
			.count = 1,
			//Vulkan keeps it in its own set; DXIL uses the reserved space so a custom layout's b0 can't land
			// on top of it

			.binding = (SHBinding) {
				.space = isSpirv ? 2 : OXC3_RESERVED_SPACE,
				.binding = 0
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
	//Maybe we can read out this speed directly?
	//It's more future proof than this (PCIE8 for example could be so much faster)

	const Bool isDistinct = device->info.type == EGraphicsDeviceType_Dedicated;
	U64 cpuHeapSize = device->info.capabilities.sharedMemory;
	U64 gpuHeapSize = device->info.capabilities.dedicatedMemory;

	device->flushThreshold = U64_min(
		4 * GIBI,
		isDistinct ? U64_min(gpuHeapSize / 3, cpuHeapSize / 10 + gpuHeapSize / 5) :
		cpuHeapSize / 5
	);

	//20M vertices per frame limit (TODO: Base this on build time benchmark too)
	device->flushThresholdPrimitives = 20 * MIBI / 3;

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

	//Only holds anything if creating the descriptor layout it was meant for failed, otherwise it was moved into it.

	DescriptorLayoutInfo_free(&descLayoutInfo, alloc);

	if (!s_uccess && allocated)
		RefPtr_dec(deviceRef);

	return s_uccess;
}

//Only used to name a register in an error, so the base type plus the RW prefix is as much detail as is useful.

static const C8 *getRegisterTypeName(ESHRegisterType type) {

	const Bool isWrite = !!(type & ESHRegisterType_IsWrite);

	switch (type & ESHRegisterType_TypeMask) {

		case ESHRegisterType_Sampler:                   return "SamplerState";
		case ESHRegisterType_SamplerComparisonState:    return "SamplerComparisonState";
		case ESHRegisterType_ConstantBuffer:            return "ConstantBuffer";
		case ESHRegisterType_PushConstants:             return "PushConstants";
		case ESHRegisterType_AccelerationStructure:     return "RaytracingAccelerationStructure";
		case ESHRegisterType_SubpassInput:              return "SubpassInput";

		case ESHRegisterType_ByteAddressBuffer:         return isWrite ? "RWByteAddressBuffer" : "ByteAddressBuffer";
		case ESHRegisterType_StructuredBuffer:          return isWrite ? "RWStructuredBuffer" : "StructuredBuffer";
		case ESHRegisterType_StructuredBufferAtomic:    return "Append/ConsumeBuffer";
		case ESHRegisterType_StorageBuffer:             return isWrite ? "RWStorageBuffer" : "StorageBuffer";
		case ESHRegisterType_StorageBufferAtomic:       return isWrite ? "RWStorageBufferAtomic" : "StorageBufferAtomic";

		case ESHRegisterType_Texture1D:                 return isWrite ? "RWTexture1D" : "Texture1D";
		case ESHRegisterType_Texture2D:                 return isWrite ? "RWTexture2D" : "Texture2D";
		case ESHRegisterType_Texture3D:                 return isWrite ? "RWTexture3D" : "Texture3D";
		case ESHRegisterType_TextureCube:               return isWrite ? "RWTextureCube" : "TextureCube";
		case ESHRegisterType_Texture2DMS:               return isWrite ? "RWTexture2DMS" : "Texture2DMS";

		default:                                        return "Unknown";
	}
}

//A bindless register is an array the shader indexes with a descriptor handle, and the only thing it can index into
// is the layout the device was created with, so the binary and that layout have to describe the same slots.
//An oiSH built against a different layout is exactly what this catches, which is what makes recreating an older
// OxC3 layout enough to keep older oiSH files running.
//Only arrays are checked, since that's what EDescriptorLayoutFlags_AllowBindlessOnArrays exposes and what
// GraphicsDeviceRef_createDescriptorLayout turns into a bindless type id.
//Push constants, push descriptors and singular bound resources come from whatever pipeline layout the caller hands
// to the pipeline, so rejecting them here would break every pipeline that brings a layout of its own.
//An unbounded array reflects as a count of 0 and adopts whatever size the layout offers,
// so it only requires the slot to be an array at all.

static Bool GraphicsDevice_checkShaderBindless(
	const GraphicsDevice *device, const SHBinaryInfo *bin, ESHBinaryType binaryType, Error *e_rr
) {

	Bool s_uccess = true;

	const DescriptorLayout *layout =
		!device->defaultDescLayout ? NULL : DescriptorLayoutRef_ptr(device->defaultDescLayout);

	for (U64 i = 0; i < bin->registers.length; ++i) {

		const SHRegisterRuntime *reg = &bin->registers.ptr[i];
		const SHBinding *regBinding = &reg->reg.bindings.arr[binaryType];

		//U32_MAX for both space and binding means the register isn't present in this binary type at all.

		if(regBinding->space == U32_MAX && regBinding->binding == U32_MAX)
			continue;

		//Push constants are a pipeline layout concept, they never resolve through a descriptor table.

		if((reg->reg.registerType & ESHRegisterType_TypeMask) == ESHRegisterType_PushConstants)
			continue;

		U32 count = 1;

		for(U64 j = 0; j < reg->arrays.length; ++j)
			count *= reg->arrays.ptr[j];

		if(count == 1)        //Not an array, so it's bound through a pipeline layout rather than bindlessly
			continue;

		const U64 nameLen = CharString_length(reg->name);
		const C8 *typeName = getRegisterTypeName(reg->reg.registerType);
		const C8 *unbounded = count ? "" : " (unbounded)";

		//A device with the bindless feature always has a layout, so the caller of this function catches that case
		// first and with a better message; this stays as a guard in case the two ever drift apart.

		if (!layout) {

			Log_errorLnx(
				"Shader register %.*s (%s, space %"PRIu32", binding %"PRIu32", count %"PRIu32"%s) is a bindless "
				"array, but this device has no bindless layout",
				(int) nameLen, reg->name.ptr, typeName, regBinding->space, regBinding->binding, count, unbounded
			);

			retError(clean, Error_invalidState(
				0,
				"GraphicsDeviceRef_checkShaderFeatures() binary uses bindless arrays, but bindless is unsupported "
				"or was turned off with EGraphicsDeviceFlags_DisableBindless"
			));
		}

		//A slot is space, binding and register type together, since DXIL lets samplers, SRVs, UAVs and CBVs share a
		// space and a binding id while living in ranges of their own.
		//IsCombinedSampler is left out of the comparison because SHFile_combine merges registers that only differ in
		// it, so an oiSH can carry the bit for a binary type that doesn't actually combine.

		const DescriptorLayoutInfo *info = &layout->info;

		const U32 typeMask = ~(U32) ESHRegisterType_IsCombinedSampler;
		const U32 regType = (U32) reg->reg.registerType & typeMask;

		U64 match = U64_MAX;
		U64 slotMatch = U64_MAX;
		U64 nameMatch = U64_MAX;

		for (U64 j = 0; j < info->bindings.length; ++j) {

			const DescriptorBinding *b = &info->bindings.ptr[j];

			//The name is only tracked to describe a layout that moved the register somewhere else.

			if(
				nameMatch == U64_MAX && j < info->bindingNames.length &&
				CharString_equalsStringSensitive(&info->bindingNames.ptr[j], &reg->name)
			)
				nameMatch = j;

			if(b->binding.space != regBinding->space || b->binding.binding != regBinding->binding)
				continue;

			if(slotMatch == U64_MAX)
				slotMatch = j;

			if (((U32) b->registerType & typeMask) == regType) {
				match = j;
				break;
			}
		}

		//Name what the layout has instead, since the fix is to line the layout back up with the oiSH.

		if (match == U64_MAX) {

			const U64 other = slotMatch != U64_MAX ? slotMatch : nameMatch;

			if (other == U64_MAX)
				Log_errorLnx(
					"Shader register %.*s (%s, space %"PRIu32", binding %"PRIu32", count %"PRIu32"%s) has no "
					"matching binding in the device's bindless layout",
					(int) nameLen, reg->name.ptr, typeName, regBinding->space, regBinding->binding, count, unbounded
				);

			else {

				const DescriptorBinding *b = &info->bindings.ptr[other];

				const CharString otherName =
					other < info->bindingNames.length ? info->bindingNames.ptr[other] : CharString_createNull();

				Log_errorLnx(
					"Shader register %.*s (%s, space %"PRIu32", binding %"PRIu32", count %"PRIu32"%s) doesn't match "
					"the device's bindless layout, which has %.*s (%s, space %"PRIu32", binding %"PRIu32", "
					"count %"PRIu32") instead",
					(int) nameLen, reg->name.ptr, typeName, regBinding->space, regBinding->binding, count, unbounded,
					(int) CharString_length(otherName), otherName.ptr, getRegisterTypeName(b->registerType),
					b->binding.space, b->binding.binding, b->count
				);
			}

			retError(clean, Error_invalidState(
				0,
				"GraphicsDeviceRef_checkShaderFeatures() binary's bindless registers don't match the device's "
				"bindless layout"
			));
		}

		//The layout has to be an array too and it has to cover every index the shader can reach.

		const DescriptorBinding *matched = &info->bindings.ptr[match];

		if (matched->count <= 1 || matched->count < count) {

			Log_errorLnx(
				"Shader register %.*s (%s, space %"PRIu32", binding %"PRIu32") needs a bindless array of at least "
				"%"PRIu32"%s, but the device's bindless layout has %"PRIu32" there",
				(int) nameLen, reg->name.ptr, typeName, regBinding->space, regBinding->binding, count, unbounded,
				matched->count
			);

			retError(clean, Error_invalidState(
				0,
				"GraphicsDeviceRef_checkShaderFeatures() binary's bindless array doesn't fit in the device's "
				"bindless layout"
			));
		}
	}

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_checkShaderFeatures(
	GraphicsDeviceRef *deviceRef, const SHBinaryInfo *bin, const SHEntry *entry, Error *e_rr
) {

	Bool s_uccess = true;

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_checkShaderFeatures()::deviceRef is required"));

	if(!bin || !entry)
		retError(clean, Error_nullPointer(
			!bin ? 1 : 2, "GraphicsDeviceRef_checkShaderFeatures()::bin and entry are required"
		));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	ESHExtension extensions = (bin->identifier.extensions &~ bin->dormantExtensions) & ESHExtension_All;

	EGraphicsFeatures features = EGraphicsFeatures_None;
	EGraphicsFeatures2 features2 = EGraphicsFeatures2_None;
	EDxGraphicsFeatures featuresDx = EDxGraphicsFeatures_None;
	EGraphicsDataTypes dataTypes = EGraphicsDataTypes_None;

	if(extensions & ESHExtension_SubgroupOperations)        features |= EGraphicsFeatures_SubgroupOperations;
	if(extensions & ESHExtension_SubgroupArithmetic)        features |= EGraphicsFeatures_SubgroupArithmetic;
	if(extensions & ESHExtension_SubgroupShuffle)           features |= EGraphicsFeatures_SubgroupShuffle;

	if(extensions & ESHExtension_Multiview)                 features |= EGraphicsFeatures_Multiview;

	if(extensions & ESHExtension_RayQuery)                  features |= EGraphicsFeatures_RayQuery;
	if(extensions & ESHExtension_RayMicromapOpacity)        features |= EGraphicsFeatures_RayMicromapOpacity;
	if(extensions & ESHExtension_RayReorder)                features |= EGraphicsFeatures_RayReorder;
	if(extensions & ESHExtension_RayTriPosition)            features |= EGraphicsFeatures_RayTriPosition;

	if(extensions & ESHExtension_ComputeDeriv)              features |= EGraphicsFeatures_ComputeDeriv;
	if(extensions & ESHExtension_MeshTaskTexDeriv)          features |= EGraphicsFeatures_MeshTaskTexDeriv;

	if(extensions & ESHExtension_WriteMSTexture)            features |= EGraphicsFeatures_WriteMSTexture;

	if(extensions & ESHExtension_Bindless)                  features |= EGraphicsFeatures_Bindless;
	if(extensions & ESHExtension_DescriptorHeap)            features2 |= EGraphicsFeatures2_DescriptorHeap;

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

	const ESHBinaryType binaryType =
		GraphicsInstanceRef_ptr(device->instance)->api == EGraphicsApi_Direct3D12 ?
		ESHBinaryType_DXIL : ESHBinaryType_SPIRV;

	//Bindless is the one feature a caller can take away from a device that has it,
	// so it says as much instead of leaving it to the generic message below.

	if((features & EGraphicsFeatures_Bindless) && !(device->info.capabilities.features & EGraphicsFeatures_Bindless))
		retError(clean, Error_invalidState(
			0,
			"GraphicsDeviceRef_checkShaderFeatures() binary needs bindless, but the device has no bindless layout "
			"(unsupported or turned off with EGraphicsDeviceFlags_DisableBindless)"
		));

	if((device->info.capabilities.features & features) != features)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the features is missing"));

	if((device->info.capabilities.features2 & features2) != features2)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the features2 is missing"));

	if((device->info.capabilities.dataTypes & dataTypes) != dataTypes)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the dataTypes is missing"));

	//Reading an oiSH widens a was-everything mask to today's everything, so this comparison stays exact
	// however many vendors have been added since the binary was written.

	if(bin->vendorMask != ESHVendor_allMask(ESHVendor_Count))
		if(!((bin->vendorMask >> device->info.vendor) & 1))
			retError(clean, Error_invalidState(
				0, "GraphicsDeviceRef_checkShaderFeatures() binary is incompatible with vendor"
			));

	//Check for D3D12 features, shader models and DXIL

	if(binaryType == ESHBinaryType_DXIL) {

		if((device->info.capabilities.featuresExt & (U32)featuresDx) != (U32)featuresDx)
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() one of the featuresDx is missing"));

		if(!Buffer_length(bin->binaries[ESHBinaryType_DXIL]))
			retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() DXIL binary is missing"));
	}

	//Check for SPIRV

	else if(!Buffer_length(bin->binaries[ESHBinaryType_SPIRV]))
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_checkShaderFeatures() SPIRV binary is missing"));

	//The bindless registers have to line up with the layout the device is actually running,
	// otherwise the descriptor handles the shader indexes with would resolve to the wrong arrays.
	//Only binaries that the oiSH marks as needing bindless are checked,
	// since anything that still fits a bindful layout is driven by a pipeline layout of the caller's own.
	//It runs last so a binary that can't run here for a simpler reason (a missing feature, the wrong vendor or no
	// blob for this api) still reports that reason rather than a layout mismatch it was never going to reach.

	if(extensions & ESHExtension_Bindless)
		gotoIfError3(clean, GraphicsDevice_checkShaderBindless(device, bin, binaryType, e_rr));

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_removePending(GraphicsDeviceRef *deviceRef, RefPtr *resource) {

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
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

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
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

	//This frame in flight provably completed, so its readbacks can land in cpuData and report

	GraphicsDevice_completePulls(device, device->fifId);

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

	DeviceBufferRef *newStaging = NULL;
	AllocationBuffer newAllocations[MAX_FRAMES_IN_FLIGHT] = { 0 };

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_resizeStagingBuffer()::deviceRef is required"));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	newSize = (((newSize + 2) / 3 + 4095) &~ 4095) * 3;            //Align to ensure we never get incompatible staging buffers

	//The replacement is built completely before the live one is touched, so a failure (typically out of memory)
	// fails the resize and leaves the device on the staging buffer it already had.
	//That briefly holds both buffers, which is the price of the next submit surviving a failed resize.

	CharString stagingBufferName = CharString_createRefCStrConst("Staging buffer");

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_None,
		EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit,
		NULL,
		&stagingBufferName,
		newSize, &newStaging, e_rr
	));

	const DeviceBuffer *staging = DeviceBufferRef_ptr(newStaging);
	const Buffer stagingBuffer = Buffer_createRef(staging->resource.mappedMemoryExt, newSize);

	for(U64 i = 0; i < sizeof(newAllocations) / sizeof(newAllocations[0]); ++i) {

		const AllocationBufferCreate create = (AllocationBufferCreate) {
			.size = newSize / 3,
			.alloc = alloc,
			.allocationBuffer = &newAllocations[i]
		};

		gotoIfError3(clean, AllocationBuffer_createRefFromRegion(&create, stagingBuffer, newSize / 3 * i, e_rr));
	}

	//Swap only happens once nothing can fail anymore.
	//If the old staging buffer was still in flight, dec'ing it won't free it until it's out of flight.

	if (device->staging) {

		for(U64 i = 0; i < sizeof(device->stagingAllocations) / sizeof(device->stagingAllocations[0]); ++i)
			AllocationBuffer_free(&device->stagingAllocations[i], alloc);

		RefPtr_dec(&device->staging);
	}

	device->staging = newStaging;
	newStaging = NULL;

	for(U64 i = 0; i < sizeof(newAllocations) / sizeof(newAllocations[0]); ++i) {
		device->stagingAllocations[i] = newAllocations[i];
		newAllocations[i] = (AllocationBuffer) { 0 };
	}

clean:

	for(U64 i = 0; i < sizeof(newAllocations) / sizeof(newAllocations[0]); ++i)
		AllocationBuffer_free(&newAllocations[i], alloc);

	RefPtr_dec(&newStaging);

	return s_uccess;
}

//Readback twin of the staging buffer; same thirds, same create then swap, but CPU readable memory.

static Bool GraphicsDeviceRef_resizeStagingReadbackBuffer(GraphicsDeviceRef *deviceRef, U64 newSize, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	DeviceBufferRef *newStaging = NULL;
	AllocationBuffer newAllocations[MAX_FRAMES_IN_FLIGHT] = { 0 };

	newSize = (((newSize + 2) / 3 + 4095) &~ 4095) * 3;

	CharString name = CharString_createRefCStrConst("Staging readback buffer");

	gotoIfError3(clean, GraphicsDeviceRef_createBuffer(
		deviceRef,
		EDeviceBufferUsage_None,
		EGraphicsResourceFlag_InternalWeakDeviceRef | EGraphicsResourceFlag_CPUAllocatedBit |
		EGraphicsResourceFlag_CPUReadBit,
		NULL,
		&name,
		newSize, &newStaging, e_rr
	));

	const DeviceBuffer *staging = DeviceBufferRef_ptr(newStaging);
	const Buffer stagingBuffer = Buffer_createRef(staging->resource.mappedMemoryExt, newSize);

	for(U64 i = 0; i < sizeof(newAllocations) / sizeof(newAllocations[0]); ++i) {

		const AllocationBufferCreate create = (AllocationBufferCreate) {
			.size = newSize / 3,
			.alloc = alloc,
			.allocationBuffer = &newAllocations[i]
		};

		gotoIfError3(clean, AllocationBuffer_createRefFromRegion(&create, stagingBuffer, newSize / 3 * i, e_rr));
	}

	if (device->stagingReadback) {

		for(U64 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
			AllocationBuffer_free(&device->stagingReadbackAllocations[i], alloc);

		RefPtr_dec(&device->stagingReadback);
	}

	device->stagingReadback = newStaging;
	newStaging = NULL;

	for(U64 i = 0; i < sizeof(newAllocations) / sizeof(newAllocations[0]); ++i) {
		device->stagingReadbackAllocations[i] = newAllocations[i];
		newAllocations[i] = (AllocationBuffer) { 0 };
	}

clean:

	for(U64 i = 0; i < sizeof(newAllocations) / sizeof(newAllocations[0]); ++i)
		AllocationBuffer_free(&newAllocations[i], alloc);

	RefPtr_dec(&newStaging);

	return s_uccess;
}

Bool GraphicsDeviceRef_reserveReadback(GraphicsDeviceRef *deviceRef, U64 sizePerFrame, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_reserveReadback()::deviceRef is required"));

	if(sizePerFrame > TIBI)
		retError(clean, Error_outOfBounds(
			1, sizePerFrame, TIBI, "GraphicsDeviceRef_reserveReadback()::sizePerFrame is limited to 1TiB"
		));

	if(!sizePerFrame)
		sizePerFrame = 1;

	device = GraphicsDeviceRef_ptr(deviceRef);

	acq = SpinLock_lock(&device->lock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidOperation(0, "GraphicsDeviceRef_reserveReadback() couldn't acquire device lock"));

	//Only ever grow; a live ring may hold recorded pulls sized for the current buffer

	if(
		!device->stagingReadback ||
		DeviceBufferRef_ptr(device->stagingReadback)->resource.size / 3 < sizePerFrame
	)
		gotoIfError3(clean, GraphicsDeviceRef_resizeStagingReadbackBuffer(deviceRef, sizePerFrame * 3, e_rr));

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

	return s_uccess;
}

//Row measures of a pull region for any pullable texture type, in block rows for compressed formats.
//Depth formats aren't representable as an ETextureFormat, which is why this can't just call getSize directly.

static void GraphicsDevice_pullRowMeasures(
	RefPtr *resource,
	const TextureRange *range,
	U64 *rowBytes,
	U64 *rows,
	U64 *fullRowBytes,
	U64 *fullRows,
	U64 *xOffBytes,
	U64 *yRowOff
) {

	const UnifiedTexture utex = TextureRef_getUnifiedTexture(resource, NULL);

	if (utex.depthFormat) {

		//Pull bytes rather than plain bytes: the range says which plane rides this pull, and each plane has its
		// own texel size (the stencil of a combined format is 1 byte, D24's depth is a 32 bit word).

		const U8 texel = EDepthStencilFormat_getPullBytes((EDepthStencilFormat) utex.depthFormat, (U8) range->planeId);

		*rowBytes = (U64) TextureRange_width(*range) * texel;
		*rows = TextureRange_height(*range);

		if(fullRowBytes)
			*fullRowBytes = (U64) utex.width * texel;

		if(fullRows)
			*fullRows = utex.height;

		if(xOffBytes)
			*xOffBytes = (U64) range->startRange[0] * texel;

		if(yRowOff)
			*yRowOff = range->startRange[1];

		return;
	}

	const ETextureFormat format = ETextureFormatId_unpack[utex.textureFormatId];

	U8 alignY = 1;
	ETextureFormat_getAlignment(format, NULL, &alignY);

	*rowBytes = ETextureFormat_getSize(format, TextureRange_width(*range), alignY, 1);
	*rows = ((U64)TextureRange_height(*range) + alignY - 1) / alignY;

	if(fullRowBytes)
		*fullRowBytes = ETextureFormat_getSize(format, utex.width, alignY, 1);

	if(fullRows)
		*fullRows = ((U64)utex.height + alignY - 1) / alignY;

	if(xOffBytes)
		*xOffBytes = ETextureFormat_getSize(format, range->startRange[0], alignY, 1);

	if(yRowOff)
		*yRowOff = range->startRange[1] / alignY;
}

//Completes the pulls recorded for this frame in flight by copying readback memory into cpuData and firing the
// callbacks.
//This never waits itself: both callers already hold proof the frame finished, handleNextFrame through the frame
// pacing fence wait that gates reusing this slot (~framesInFlight of latency) and wait() through the full stall
// the caller explicitly asked for.

static void GraphicsDevice_completePulls(GraphicsDevice *device, U64 fifId) {

	ListDevicePendingPull *pulls = &device->pullsInFlight[fifId];

	if(!pulls->length)
		return;

	for (U64 i = 0; i < pulls->length; ++i) {

		DevicePendingPull *pull = &pulls->ptrNonConst[i];

		//The union member that's live follows from this: cpuData owners use callback, the rest textureCallback

		const TypeId typeId = pull->resource->refPtrType->typeId;

		const Bool ownsCpuData =
			typeId == (TypeId) EGraphicsTypeId_DeviceBuffer || typeId == (TypeId) EGraphicsTypeId_DeviceTexture;

		//Each pull reads the ring it was recorded into, which isn't necessarily the device's current one

		const U8 *mapped = DeviceBufferRef_ptr(pull->stagingReadback)->resource.mappedMemoryExt;
		const U8 *src = mapped + pull->stagingOffset;

		if(typeId == (TypeId) EGraphicsTypeId_DeviceBuffer) {

			DeviceBuffer *buffer = DeviceBufferRef_ptr(pull->resource);
			const U64 start = pull->range.buffer.startRange;
			const U64 len = pull->range.buffer.endRange - start;

			Buffer_memcpy(
				Buffer_createRef(buffer->cpuData.ptrNonConst + start, len),
				Buffer_createRefConst(src, len)
			);
		}

		else {

			const TextureRange range = pull->range.texture;

			U64 regionRow, rows, fullRow, fullRows, xOff, yRow;
			GraphicsDevice_pullRowMeasures(pull->resource, &range, &regionRow, &rows, &fullRow, &fullRows, &xOff, &yRow);

			const U16 z = range.startRange[2];

			//De-pitch the backend's row stride back into the tight full texture rows cpuData uses,
			// at the region's offsets; all row measures are block rows for compressed formats

			if (ownsCpuData) {

				DeviceTexture *texture = DeviceTextureRef_ptr(pull->resource);

				for(U64 k = 0; k < TextureRange_length(range); ++k)
					for(U64 j = 0; j < rows; ++j)
						Buffer_memcpy(
							Buffer_createRef(
								texture->cpuData.ptrNonConst + fullRow * (yRow + j + (z + k) * fullRows) + xOff,
								regionRow
							),
							Buffer_createRefConst(src + pull->rowPitch * (j + k * rows), regionRow)
						);
			}

			//Render targets have no cpuData, so the region is handed to the callback as an owned tight buffer

			else {

				//The destination was allocated when the pull was queued, so completion can't fail anymore

				for(U64 k = 0; k < TextureRange_length(range); ++k)
					for(U64 j = 0; j < rows; ++j)
						Buffer_memcpy(
							Buffer_createRef(pull->textureData.ptrNonConst + regionRow * (j + k * rows), regionRow),
							Buffer_createRefConst(src + pull->rowPitch * (j + k * rows), regionRow)
						);

				if(pull->textureCallback)
					pull->textureCallback(pull->resource, &pull->textureData, pull->context);

				Buffer_free(&pull->textureData, GraphicsDevice_getAlloc(device));
			}
		}

		//textureCallback shares the union and already fired inside the render target branch above

		if(ownsCpuData && pull->callback)
			pull->callback(pull->resource, pull->context);

		RefPtr_dec(&pull->resource);
		RefPtr_dec(&pull->stagingReadback);
	}

	ListDevicePendingPull_clear(pulls, NULL);
	AllocationBuffer_freeAll(&device->stagingReadbackAllocations[fifId]);
}

//Records the queued pulls into the command buffer, after the frame's own commands so they see its results.
//Called by the backends at the end of GraphicsDevice_submitCommands.

Bool GraphicsDeviceRef_flushPendingPulls(GraphicsDeviceRef *deviceRef, void *commandBuffer, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(deviceRef);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(!device->pendingPulls.length)
		return s_uccess;

	//Total readback memory needed; texture rows are conservatively padded to D3D12's 256 byte row pitch and
	// every region begins at a 512 byte aligned placed footprint offset, which Vulkan doesn't mind either

	U64 needed = 0;

	for (U64 i = 0; i < device->pendingPulls.length; ++i) {

		const DevicePendingPull *pull = &device->pendingPulls.ptr[i];
		U64 len;

		if(pull->resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer)
			len = pull->range.buffer.endRange - pull->range.buffer.startRange;

		else {

			U64 rowBytes, rows;
			GraphicsDevice_pullRowMeasures(pull->resource, &pull->range.texture, &rowBytes, &rows, NULL, NULL, NULL, NULL);

			const U64 pitch = (rowBytes + 255) &~ 255;
			len = pitch * rows * TextureRange_length(pull->range.texture);
		}

		needed += ((len + 511) &~ 511) + 512;
	}

	if(!device->stagingReadback)
		gotoIfError3(clean, GraphicsDeviceRef_resizeStagingReadbackBuffer(deviceRef, needed * 3, e_rr));

	AllocationBuffer *allocations = &device->stagingReadbackAllocations[device->fifId];
	const U8 *base = DeviceBufferRef_ptr(device->stagingReadback)->resource.mappedMemoryExt;

	for (U64 i = 0; i < device->pendingPulls.length; ++i) {

		DevicePendingPull pull = device->pendingPulls.ptr[i];
		const Bool isBuffer = pull.resource->refPtrType->typeId == (TypeId) EGraphicsTypeId_DeviceBuffer;

		U64 len;
		U64 pitch = 0;

		if(isBuffer)
			len = pull.range.buffer.endRange - pull.range.buffer.startRange;

		else {

			U64 rowBytes, rows;
			GraphicsDevice_pullRowMeasures(pull.resource, &pull.range.texture, &rowBytes, &rows, NULL, NULL, NULL, NULL);

			pitch = (rowBytes + 255) &~ 255;
			len = pitch * rows * TextureRange_length(pull.range.texture);
		}

		const AllocationBufferAllocate allocation = (AllocationBufferAllocate) {
			.allocationBuffer = allocations,
			.alignment = 512,
			.alloc = alloc
		};

		const U8 *location = NULL;

		if (!AllocationBuffer_allocateBlock(&allocation, len, &location, NULL)) {

			//Grown like the upload staging buffer, after which the allocation has to succeed

			const U64 prevSize = DeviceBufferRef_ptr(device->stagingReadback)->resource.size;
			gotoIfError3(clean, GraphicsDeviceRef_resizeStagingReadbackBuffer(deviceRef, prevSize * 2 + needed * 3, e_rr));

			allocations = &device->stagingReadbackAllocations[device->fifId];
			base = DeviceBufferRef_ptr(device->stagingReadback)->resource.mappedMemoryExt;

			const AllocationBufferAllocate retry = (AllocationBufferAllocate) {
				.allocationBuffer = allocations,
				.alignment = 512,
				.alloc = alloc
			};

			gotoIfError3(clean, AllocationBuffer_allocateBlock(&retry, len, &location, e_rr));
		}

		pull.stagingOffset = (U64)(location - base);
		pull.rowPitch = isBuffer ? len : pitch;

		if(isBuffer) {
			gotoIfError3(clean, DeviceBufferRef_pullExt(
				commandBuffer, deviceRef, pull.resource,
				pull.range.buffer.startRange, len, pull.stagingOffset, e_rr
			));
		}

		else {

			U64 backendPitch = pull.rowPitch;

			gotoIfError3(clean, DeviceTextureRef_pullExt(
				commandBuffer, deviceRef, pull.resource, &pull.range.texture, pull.stagingOffset, &backendPitch, e_rr
			));

			pull.rowPitch = backendPitch;
		}

		//The pull keeps the ring it was recorded into alive until completion, since a grow swaps the device's ring

		pull.stagingReadback = device->stagingReadback;

		gotoIfError3(clean, ListDevicePendingPull_pushBack(&device->pullsInFlight[device->fifId], pull, alloc, e_rr));
		RefPtr_inc(device->stagingReadback);        //Owned by the in flight entry just pushed

		//Ownership of the ref and the destination buffer moved to the in flight list

		device->pendingPulls.ptrNonConst[i].resource = NULL;
		device->pendingPulls.ptrNonConst[i].textureData = Buffer_createNull();
	}

clean:

	//Anything that didn't make it into the in flight list still owns its ref and destination buffer

	for(U64 i = 0; i < device->pendingPulls.length; ++i) {
		RefPtr_dec(&device->pendingPulls.ptrNonConst[i].resource);
		Buffer_free(&device->pendingPulls.ptrNonConst[i].textureData, alloc);
	}

	ListDevicePendingPull_clear(&device->pendingPulls, NULL);
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

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
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

		if(!cmdRef || cmdRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_CommandList)
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

		if(!swapchainRef || swapchainRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_Swapchain)
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

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId)EGraphicsTypeId_GraphicsDevice)
		return U64_MAX;

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(device->info.type != EGraphicsDeviceType_Dedicated && isDeviceLocal)
		return 0;

	return GraphicsDevice_getMemoryBudgetExt(device, isDeviceLocal);
}

Bool GraphicsDevice_logOnce(GraphicsDevice *device, EGraphicsDeviceMessage message) {

	if(!device)
		return false;

	//Fetch or returns the PREVIOUS bits, so the first caller sees the bit clear and everyone after sees it set

	return !(AtomicI64_or(&device->runtimeMessages, (I64) message) & (I64) message);
}

Bool GraphicsDeviceRef_wait(GraphicsDeviceRef *deviceRef, Error *e_rr) {

	Bool s_uccess = true;

	GraphicsDevice *device = NULL;
	ELockAcquire acq = ELockAcquire_Invalid;

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId)EGraphicsTypeId_GraphicsDevice)
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

		//Waiting proves every frame completed, so all recorded readbacks can land and report now

		GraphicsDevice_completePulls(device, i);
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
