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

//graphics/generic/instance.c

#include "types/container/list_impl.h"
#include "types/base/platform_types.h"
#include "graphics/generic/interface.h"
#include "graphics/generic/instance.h"
#include "graphics/generic/device.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device_texture.h"
#include "graphics/generic/render_texture.h"
#include "graphics/generic/depth_stencil.h"
#include "graphics/generic/swapchain.h"
#include "graphics/generic/pipeline.h"
#include "graphics/generic/sampler.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/command_list.h"
#include "types/container/ref_ptr.h"
#include "types/base/error.h"

TListImpl(GraphicsDeviceInfo);

const C8 *EGraphicsApi_name[EGraphicsApi_Count] = {
	"Vulkan", "D3D12"
};

Bool GraphicsInstance_getPreferredDevice(
	const GraphicsInstance *inst,
	const GraphicsDeviceCapabilities *requiredCapabilities,
	U64 vendorMask,
	U64 deviceTypeMask,
	GraphicsDeviceInfo *deviceInfo,
	Error *e_rr
) {

	Bool s_uccess = true;

	ListGraphicsDeviceInfo tmp = (ListGraphicsDeviceInfo) { 0 };

	if(!inst || !deviceInfo)
		retError(clean, Error_nullPointer(
			!inst ? 0 : 4,
			"GraphicsInstance_getPreferredDevice()::inst, requiredCapabilities and deviceInfo are required"
		));

	if(deviceInfo->name[0])
		retError(clean, Error_invalidParameter(
			4, 0, "GraphicsInstance_getPreferredDevice()::*deviceInfo must be empty"
		));

	gotoIfError3(clean, GraphicsInstance_getDeviceInfosExt(inst, &tmp, e_rr));

	U64 preferredDedicated = 0;
	U64 preferredNonDedicated = 0;
	U64 preferredIntegrated = 0;
	Bool hasDedicated = false;
	Bool hasIntegrated = false;
	Bool hasAny = false;

	for (U64 i = 0; i < tmp.length; ++i) {

		const GraphicsDeviceInfo info = tmp.ptr[i];

		//Check if vendor and device type are supported

		if(!((vendorMask >> info.vendor) & 1) || !((deviceTypeMask >> info.type) & 1))
			continue;

		//Check capabilities

		if(requiredCapabilities) {

			if((info.capabilities.dataTypes & requiredCapabilities->dataTypes) != requiredCapabilities->dataTypes)
				continue;

			if((info.capabilities.features & requiredCapabilities->features) != requiredCapabilities->features)
				continue;

			if((info.capabilities.features2 & requiredCapabilities->features2) != requiredCapabilities->features2)
				continue;

			if((info.capabilities.featuresExt & requiredCapabilities->featuresExt) != requiredCapabilities->featuresExt)
				continue;

			if(
				info.capabilities.sharedMemory < requiredCapabilities->sharedMemory ||
				info.capabilities.dedicatedMemory < requiredCapabilities->dedicatedMemory ||
				info.capabilities.maxBufferSize < requiredCapabilities->maxBufferSize ||
				info.capabilities.maxAllocationSize < requiredCapabilities->maxAllocationSize
			)
				continue;
		}

		if(info.type == EGraphicsDeviceType_Dedicated) {
			preferredDedicated = i;
			hasDedicated = hasAny = true;
			break;
		}

		else {

			if (info.type == EGraphicsDeviceType_Integrated) {
				preferredIntegrated = i;
				hasIntegrated = true;
			}

			else preferredNonDedicated = i;
		}

		hasAny = true;
	}

	if(!hasAny)
		retError(clean, Error_notFound(0, 0, "GraphicsInstance_getPreferredDevice() no supported queried devices"));

	const U64 picked = hasDedicated ? preferredDedicated : (hasIntegrated ? preferredIntegrated : preferredNonDedicated);
	*deviceInfo = tmp.ptr[picked];

clean:
	if(inst)
		ListGraphicsDeviceInfo_free(&tmp, inst->alloc);
	return s_uccess;
}

Bool GraphicsInterface_create(Error *e_rr) {
	return GraphicsInterface_init(e_rr);
}

Bool GraphicsInterface_supportsApi(EGraphicsApi api) {
	return GraphicsInterface_supports(api);
}

EGraphicsApi EGraphicsApi_resolve(EGraphicsApi api) {

	if (api >= EGraphicsApi_Count) {
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			api = GraphicsInterface_supports(EGraphicsApi_Direct3D12) ? EGraphicsApi_Direct3D12 : EGraphicsApi_Vulkan;
		#else
			api = EGraphicsApi_Vulkan;
		#endif
	}

	return api;
}

RefPtrType GraphicsInstance_makeType(EGraphicsApi api, const Allocator *alloc) {

	api = EGraphicsApi_resolve(api);

	return (RefPtrType) {
		.typeId = (TypeId) EGraphicsTypeId_GraphicsInstance,
		.length = (U32)(sizeof(GraphicsInstance) + GraphicsInterface_getObjectSizes(api)->instance),
		.alloc = alloc,
		.free = (ObjectFreeFunc) GraphicsInstance_freeExt
	};
}

//Free functions of the graphics objects; only used as the ObjectFreeFunc of their RefPtrType.
//These are intentionally not exposed in the public headers, use RefPtr_dec instead.

void GraphicsDevice_free(GraphicsDevice *device, const Allocator *alloc);
void DeviceBuffer_free(DeviceBuffer *buffer, const Allocator *alloc);
void DeviceTexture_free(DeviceTexture *texture, const Allocator *alloc);
void GraphicsDevice_freeRenderTexture(RenderTexture *renderTexture, const Allocator *alloc);
void GraphicsDevice_freeDepthStencil(DepthStencil *depthStencil, const Allocator *alloc);
void Swapchain_free(Swapchain *swapchain, const Allocator *alloc);
void Pipeline_free(Pipeline *pipeline, const Allocator *alloc);
void Sampler_free(Sampler *sampler, const Allocator *alloc);
void BLAS_free(BLAS *blas, const Allocator *alloc);
void TLAS_free(TLAS *tlas, const Allocator *alloc);
void DescriptorLayout_free(DescriptorLayout *layout, const Allocator *alloc);
void DescriptorTable_free(DescriptorTable *table, const Allocator *alloc);
void DescriptorHeap_free(DescriptorHeap *heap, const Allocator *alloc);
void PipelineLayout_free(PipelineLayout *layout, const Allocator *alloc);
void CommandList_free(CommandList *cmd, const Allocator *alloc);

//Fills the RefPtrTypes of all objects that can be created through this instance (or its devices).
//They live in the GraphicsInstance so they're guaranteed to outlive the objects created with them,
//since every graphics object holds a reference to the device and the device to the instance.

static GraphicsObjectTypes GraphicsInstance_makeObjectTypes(EGraphicsApi api, const Allocator *alloc) {

	const GraphicsObjectSizes *sizes = GraphicsInterface_getObjectSizes(api);
	const U64 imageSize = sizes->image + sizeof(UnifiedTextureImage);

	return (GraphicsObjectTypes) {

		.device = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_GraphicsDevice,
			.length = (U32)(sizeof(GraphicsDevice) + sizes->device),
			.alloc = alloc,
			.free = (ObjectFreeFunc) GraphicsDevice_free
		},

		.buffer = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DeviceBuffer,
			.length = (U32)(sizeof(DeviceBuffer) + sizes->buffer),
			.alloc = alloc,
			.free = (ObjectFreeFunc) DeviceBuffer_free
		},

		.deviceTexture = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DeviceTexture,
			.length = (U32)(sizeof(DeviceTexture) + imageSize),
			.alloc = alloc,
			.free = (ObjectFreeFunc) DeviceTexture_free
		},

		.renderTexture = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_RenderTexture,
			.length = (U32)(sizeof(RenderTexture) + imageSize),
			.alloc = alloc,
			.free = (ObjectFreeFunc) GraphicsDevice_freeRenderTexture
		},

		.depthStencil = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DepthStencil,
			.length = (U32)(sizeof(DepthStencil) + imageSize),
			.alloc = alloc,
			.free = (ObjectFreeFunc) GraphicsDevice_freeDepthStencil
		},

		.swapchain = (RefPtrType) {

			//On some platforms the images we get back != the images we request.
			//Even though we request 3, we might get 5 even.
			//https://github.com/googlesamples/vulkan-basic-samples/issues/24#issuecomment-442626040
			//Unfortunately we still have to allocate up to (48 + 16) * 2 = 128 bytes extra, not too bad though.

			.typeId = (TypeId) EGraphicsTypeId_Swapchain,
			.length = (U32)(sizeof(Swapchain) + imageSize * SWAPCHAIN_MAX_IMAGES + sizes->swapchain),
			.alloc = alloc,
			.free = (ObjectFreeFunc) Swapchain_free
		},

		.pipeline = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_Pipeline,
			.length = (U32)(sizeof(Pipeline) + sizes->pipeline),
			.alloc = alloc,
			.free = (ObjectFreeFunc) Pipeline_free
		},

		.sampler = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_Sampler,
			.length = (U32)(sizeof(Sampler) + sizes->sampler),
			.alloc = alloc,
			.free = (ObjectFreeFunc) Sampler_free
		},

		.blas = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_BLASExt,
			.length = (U32)(sizeof(BLAS) + sizes->blas),
			.alloc = alloc,
			.free = (ObjectFreeFunc) BLAS_free
		},

		.tlas = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_TLASExt,
			.length = (U32)(sizeof(TLAS) + sizes->tlas),
			.alloc = alloc,
			.free = (ObjectFreeFunc) TLAS_free
		},

		.descriptorLayout = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DescriptorLayout,
			.length = (U32)(sizeof(DescriptorLayout) + sizes->descriptorLayout),
			.alloc = alloc,
			.free = (ObjectFreeFunc) DescriptorLayout_free
		},

		.descriptorTable = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DescriptorTable,
			.length = (U32)(sizeof(DescriptorTable) + sizes->descriptorTable),
			.alloc = alloc,
			.free = (ObjectFreeFunc) DescriptorTable_free
		},

		.descriptorHeap = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DescriptorHeap,
			.length = (U32)(sizeof(DescriptorHeap) + sizes->descriptorHeap),
			.alloc = alloc,
			.free = (ObjectFreeFunc) DescriptorHeap_free
		},

		.pipelineLayout = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_PipelineLayout,
			.length = (U32)(sizeof(PipelineLayout) + sizes->pipelineLayout),
			.alloc = alloc,
			.free = (ObjectFreeFunc) PipelineLayout_free
		},

		.commandList = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_CommandList,
			.length = (U32) sizeof(CommandList),
			.alloc = alloc,
			.free = (ObjectFreeFunc) CommandList_free
		}
	};
}

Bool GraphicsInstance_create(
	const GraphicsApplicationInfo *info,
	EGraphicsApi api,
	EGraphicsInstanceFlags flags,
	const Allocator *alloc,
	const RefPtrType *type,
	GraphicsInstanceRef **instanceRef,
	Error *e_rr
) {

	Bool s_uccess = true;

	Bool initRefPtr = false;

	api = EGraphicsApi_resolve(api);

	//type->free deliberately isn't matched against &GraphicsInstance_freeExt, for the reason spelled out in Stream_create().
	//With DynamicLinkingGraphics the backend is a shared lib that links OxC3_graphics PUBLIC, so the app links that static
	// lib as well and both modules end up with their own GraphicsInstance_freeExt, which Mach-O keeps separate.
	//typeId, the exact length for this api and alloc still pin the type down to a GraphicsInstance_makeType() result.

	if(
		!type ||
		type->typeId != (TypeId) EGraphicsTypeId_GraphicsInstance ||
		type->length != (U32)(sizeof(GraphicsInstance) + GraphicsInterface_getObjectSizes(api)->instance) ||
		!type->free ||
		type->alloc != alloc
	)
		retError(clean, Error_invalidParameter(
			4, 0, "GraphicsInstance_create()::type is invalid, use GraphicsInstance_makeType with a matching api and alloc"
		));

	if(!info)
		retError(clean, Error_nullPointer(0, "GraphicsInstance_create()::info is required"));

	gotoIfError3(clean, RefPtr_create(type, instanceRef, e_rr));
	initRefPtr = true;

	GraphicsInstance *instance = GraphicsInstanceRef_ptr(*instanceRef);

	*instance = (GraphicsInstance) {
		.application = *info,
		.api = api,
		.flags = flags,
		.alloc = alloc,
		.types = GraphicsInstance_makeObjectTypes(api, alloc)
	};

	#ifndef NDEBUG
		if(!(flags & EGraphicsInstanceFlags_DisableDebug))
			instance->flags |= EGraphicsInstanceFlags_IsDebug;
	#endif

	gotoIfError3(clean, GraphicsInstance_createExt(info, instanceRef, e_rr));

clean:

	if(!s_uccess && initRefPtr)
		RefPtr_dec(instanceRef);

	return s_uccess;
}

Bool GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos, Error *e_rr) {

	Bool s_uccess = true;

	if(!inst || !infos)
		retError(clean, Error_nullPointer(
			!inst ? 0 : 1, "GraphicsInstance_getDeviceInfos()::inst and infos are required"
		));

	gotoIfError3(clean, GraphicsInstance_getDeviceInfosExt(inst, infos, e_rr));

clean:
	return s_uccess;
}
