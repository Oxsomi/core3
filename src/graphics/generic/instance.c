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
#include "graphics/generic/opacity_micromap.h"
#include "graphics/generic/descriptor_layout.h"
#include "graphics/generic/descriptor_table.h"
#include "graphics/generic/descriptor_heap.h"
#include "graphics/generic/pipeline_layout.h"
#include "graphics/generic/command_list.h"
#include "types/container/ref_ptr.h"
#include "types/base/error.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/container/string_helper.h"

TListImpl(GraphicsDeviceInfo);

//Internal capability derivation and dependency normalization, shared with device creation in device.c.

void GraphicsDeviceInfo_deriveCapabilities(GraphicsDeviceInfo *deviceInfo);
void GraphicsDeviceInfo_fixCapabilities(GraphicsDeviceInfo *info, EGraphicsApi api);

//A comma separated list of capability names to clear from every device queried, so a fallback can be run on
// hardware that has the feature behind it.
//Names come from the tables in device_info.c.
//A name that resolves to nothing is logged rather than dropped: a typo would otherwise leave the capability in
// place and silently test the path it was meant to disable.

#define OXC3_DISABLE_CAPABILITIES_ENV "OXC3_DISABLE_CAPABILITIES"

//Spaces around a name are trimmed, so "a, b" names the same pair as "a,b"

static CharString GraphicsDevice_trimCapabilityName(CharString str) {

	const C8 *ptr = str.ptr;
	U64 len = CharString_length(str);

	while(len && (*ptr == ' ' || *ptr == '\t')) {
		++ptr;
		--len;
	}

	while(len && (ptr[len - 1] == ' ' || ptr[len - 1] == '\t'))
		--len;

	return CharString_createRefSizedConst(ptr, len, false);
}

static Bool GraphicsDevice_disableCapabilitiesFromEnv(
	GraphicsDeviceInfo *deviceInfo, EGraphicsApi api, const Allocator *alloc, Error *e_rr
) {

	Bool s_uccess = true;

	//The splits are refs into env, so the list has to be freed before it

	CharString env = CharString_createNull();
	ListCharString names = (ListCharString) { 0 };

	gotoIfError3(clean, Platform_getEnv(
		CharString_createRefCStrConst(OXC3_DISABLE_CAPABILITIES_ENV), alloc, &env, e_rr
	));

	if(!CharString_length(env))
		goto clean;

	gotoIfError3(clean, CharString_splitSensitive(
		&(CharStringSplit) { .s = &env, .allocator = alloc, .result = &names }, ',', e_rr
	));

	for (U64 i = 0; i < names.length; ++i) {

		const CharString name = GraphicsDevice_trimCapabilityName(names.ptr[i]);

		if(!CharString_length(name))
			continue;

		if(GraphicsDeviceInfo_disableCapability(deviceInfo, api, name))
			Log_performanceLnx(
				OXC3_DISABLE_CAPABILITIES_ENV " cleared %.*s from the device",
				(I32) CharString_length(name), name.ptr
			);

		else Log_warnLnx(
			OXC3_DISABLE_CAPABILITIES_ENV " names \"%.*s\", which is not a capability of this api, so it does nothing",
			(I32) CharString_length(name), name.ptr
		);
	}

clean:

	ListCharString_free(&names, alloc);
	CharString_free(&env, alloc);
	return s_uccess;
}

//The only way device infos are allowed to leave this file, so nothing can hand out an adapter whose derived
// bits were never filled in and a search sees the same capabilities a created device will report.

static Bool GraphicsInstance_getDeviceInfosDerived(
	const GraphicsInstance *inst,
	ListGraphicsDeviceInfo *infos,
	const GraphicsDeviceCapFlags *disabledCaps,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool queried = false;

	gotoIfError3(clean, GraphicsInstance_getDeviceInfosExt(inst, infos, e_rr));
	queried = true;

	//Derive from hardware reports before masking so explicitly disabled derived bits stay disabled.

	for(U64 i = 0; i < infos->length; ++i) {

		GraphicsDeviceInfo *info = &infos->ptrNonConst[i];

		GraphicsDeviceInfo_deriveCapabilities(info);

		if(disabledCaps) {
			info->capabilities.flags.features &= ~disabledCaps->features;
			info->capabilities.flags.features2 &= ~disabledCaps->features2;
			info->capabilities.flags.dataTypes &= ~disabledCaps->dataTypes;
			info->capabilities.flags.featuresExt &= ~disabledCaps->featuresExt;
		}

		gotoIfError3(clean, GraphicsDevice_disableCapabilitiesFromEnv(info, inst->api, inst->alloc, e_rr));
		GraphicsDeviceInfo_fixCapabilities(info, inst->api);
	}

clean:
	if(!s_uccess && queried)
		ListGraphicsDeviceInfo_free(infos, inst->alloc);

	return s_uccess;
}

//Every one of these objects has a backend extension struct sitting directly behind it, so the RefPtr has to
//account for that struct's alignment as well as the owner's; the owner being 64 byte aligned says nothing
//about where the block behind it lands, and it's the block behind it that holds a SpinLock.
//The stride rather than the plain size, so the block itself starts aligned too.

#define OXC3_APPENDED_LEN(T, extSize) RefPtrType_pack(                  \
	sizeof(T) + GraphicsObjectSize_stride(extSize),                     \
	U64_max(alignof(T), GraphicsObjectSize_alignment(extSize))          \
)

const C8 *EGraphicsApi_name[EGraphicsApi_Count] = {
	"Vulkan", "D3D12"
};

Bool GraphicsInstance_getPreferredDevice(
	const GraphicsInstance *inst,
	const GraphicsDeviceCapabilities *requiredCapabilities,
	const GraphicsDeviceCapFlags *disabledCaps,
	U64 vendorMask,
	U64 deviceTypeMask,
	GraphicsDeviceInfo *deviceInfo,
	Error *e_rr
) {

	Bool s_uccess = true;

	ListGraphicsDeviceInfo tmp = (ListGraphicsDeviceInfo) { 0 };

	if(!inst || !deviceInfo)
		retError(clean, Error_nullPointer(
			!inst ? 0 : 5,
			"GraphicsInstance_getPreferredDevice()::inst and deviceInfo are required"
		));

	if(deviceInfo->name[0])
		retError(clean, Error_invalidParameter(
			5, 0, "GraphicsInstance_getPreferredDevice()::*deviceInfo must be empty"
		));

	gotoIfError3(clean, GraphicsInstance_getDeviceInfosDerived(inst, &tmp, disabledCaps, e_rr));

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

			const GraphicsDeviceCapFlags required = requiredCapabilities->flags;
			const GraphicsDeviceCapFlags available = info.capabilities.flags;

			if((available.dataTypes & required.dataTypes) != required.dataTypes)
				continue;

			if((available.features & required.features) != required.features)
				continue;

			if((available.features2 & required.features2) != required.features2)
				continue;

			if((available.featuresExt & required.featuresExt) != required.featuresExt)
				continue;

			if (
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

//The other object frees are each a void* shim in their own file.
//The GraphicsInstance RefPtr free is the backend GraphicsInstance_freeExt itself, which is typed
// GraphicsInstance* because it is also invoked through the interface table.
//This tiny shim adapts it to ObjectFreeFunc, so registering it below is a real type match under
// -fsanitize=function rather than a cast across mismatched function types.
static void GraphicsInstance_freeRefPtr(void *instGeneric, const Allocator *alloc) {
	GraphicsInstance_freeExt((GraphicsInstance*) instGeneric, alloc);
}

RefPtrType GraphicsInstance_makeType(EGraphicsApi api, const Allocator *alloc) {

	api = EGraphicsApi_resolve(api);

	//With GRAPHICS_API_DYNAMIC the backend is a separate module that may not have registered.
	//In that case there's no instance size to add and nothing valid to hand out.
	//A zeroed type is rejected by GraphicsInstance_create() below, so this surfaces as an ordinary error, not a crash.

	const GraphicsObjectSizes *sizes = GraphicsInterface_getObjectSizes(api);

	if(!sizes)
		return (RefPtrType) { 0 };

	return (RefPtrType) {
		.typeId = (TypeId) EGraphicsTypeId_GraphicsInstance,
		.lengthAndAlignment = OXC3_APPENDED_LEN(GraphicsInstance, sizes->instance),
		.alloc = alloc,
		.free = GraphicsInstance_freeRefPtr
	};
}

//Free functions of the graphics objects; only used as the ObjectFreeFunc of their RefPtrType.
//These are intentionally not exposed in the public headers, use RefPtr_dec instead.

//RefPtr destructors: all take void* and cast internally, so the RefPtrType.free slot they
//are registered in is a real type match under -fsanitize=function.
void GraphicsDevice_free(void *device, const Allocator *alloc);
void DeviceBuffer_free(void *buffer, const Allocator *alloc);
void DeviceTexture_free(void *texture, const Allocator *alloc);
void GraphicsDevice_freeRenderTexture(void *renderTexture, const Allocator *alloc);
void GraphicsDevice_freeDepthStencil(void *depthStencil, const Allocator *alloc);
void Swapchain_free(void *swapchain, const Allocator *alloc);
void Pipeline_free(void *pipeline, const Allocator *alloc);
void Sampler_free(void *sampler, const Allocator *alloc);
void BLAS_free(void *blas, const Allocator *alloc);
void TLAS_free(void *tlas, const Allocator *alloc);
void OpacityMicromap_free(void *micromap, const Allocator *alloc);
void DescriptorLayout_free(void *layout, const Allocator *alloc);
void DescriptorTable_free(void *table, const Allocator *alloc);
void DescriptorHeap_free(void *heap, const Allocator *alloc);
void PipelineLayout_free(void *layout, const Allocator *alloc);
void CommandList_free(void *cmd, const Allocator *alloc);

//Fills the RefPtrTypes of all objects that can be created through this instance (or its devices).
//They live in the GraphicsInstance so they're guaranteed to outlive the objects created with them,
// since every graphics object holds a reference to the device and the device to the instance.

static GraphicsObjectTypes GraphicsInstance_makeObjectTypes(EGraphicsApi api, const Allocator *alloc) {

	const GraphicsObjectSizes *sizes = GraphicsInterface_getObjectSizes(api);

	//These three reserve one image, so the appended size is asked for with that count; the accessors in
	// texture.c derive their offsets from the same helper, which is the point of it existing.
	//The alignment has to cover the backend struct too, not just the owner: the owner being 64 byte aligned
	// says nothing about where the appended blocks land, and it's the appended block that holds the SpinLock.

	const U64 imageSize = UnifiedTexture_appendedSize(sizes->image, 1);
	const U64 imageAlignment = GraphicsObjectSize_alignment(sizes->image);

	return (GraphicsObjectTypes) {

		.device = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_GraphicsDevice,
			.lengthAndAlignment = OXC3_APPENDED_LEN(GraphicsDevice, sizes->device),
			.alloc = alloc,
			.free = GraphicsDevice_free
		},

		.buffer = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DeviceBuffer,
			.lengthAndAlignment = OXC3_APPENDED_LEN(DeviceBuffer, sizes->buffer),
			.alloc = alloc,
			.free = DeviceBuffer_free
		},

		.deviceTexture = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DeviceTexture,
			.lengthAndAlignment = RefPtrType_pack(
				sizeof(DeviceTexture) + imageSize, U64_max(alignof(DeviceTexture), imageAlignment)
			),
			.alloc = alloc,
			.free = DeviceTexture_free
		},

		.renderTexture = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_RenderTexture,
			.lengthAndAlignment = RefPtrType_pack(
				sizeof(RenderTexture) + imageSize, U64_max(alignof(RenderTexture), imageAlignment)
			),
			.alloc = alloc,
			.free = GraphicsDevice_freeRenderTexture
		},

		.depthStencil = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DepthStencil,
			.lengthAndAlignment = RefPtrType_pack(
				sizeof(DepthStencil) + imageSize, U64_max(alignof(DepthStencil), imageAlignment)
			),
			.alloc = alloc,
			.free = GraphicsDevice_freeDepthStencil
		},

		.swapchain = (RefPtrType) {

			//On some platforms the images we get back != the images we request.
			//Even though we request 3, we might get 5 even.
			//https://github.com/googlesamples/vulkan-basic-samples/issues/24#issuecomment-442626040
			//Unfortunately we still have to allocate up to (48 + 16) * 2 = 128 bytes extra, not too bad though.

			.typeId = (TypeId) EGraphicsTypeId_Swapchain,
			//Reserves the full image count up front so its ext blocks never move when the swapchain resizes.
			//Asked for as one reservation rather than imageSize * MAX, which would have counted the padding in
			// front of the ext blocks once per image.

			.lengthAndAlignment = RefPtrType_pack(
				sizeof(Swapchain) +
				UnifiedTexture_appendedSize(sizes->image, SWAPCHAIN_MAX_IMAGES) +
				GraphicsObjectSize_stride(sizes->swapchain),
				U64_max(
					U64_max(alignof(Swapchain), imageAlignment),
					GraphicsObjectSize_alignment(sizes->swapchain)
				)
			),
			.alloc = alloc,
			.free = Swapchain_free
		},

		//The three pipeline kinds share a typeId but not a length: graphics and raytracing pipelines store
		// their info block behind the backend data (Pipeline_infoOffset), compute has no info block at all.
		//Sizing them apart keeps compute pipelines from carrying the biggest info as dead padding.

		.pipelineCompute = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_Pipeline,
			.lengthAndAlignment = OXC3_APPENDED_LEN(Pipeline, sizes->pipeline),
			.alloc = alloc,
			.free = Pipeline_free
		},

		.pipelineGraphics = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_Pipeline,
			.lengthAndAlignment = RefPtrType_pack(
				sizeof(Pipeline) + GraphicsObjectSize_stride(sizes->pipeline) + sizeof(PipelineGraphicsInfo),
				U64_max(
					U64_max(alignof(Pipeline), alignof(PipelineGraphicsInfo)),
					GraphicsObjectSize_alignment(sizes->pipeline)
				)
			),
			.alloc = alloc,
			.free = Pipeline_free
		},

		.pipelineRaytracing = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_Pipeline,
			.lengthAndAlignment = RefPtrType_pack(
				sizeof(Pipeline) + GraphicsObjectSize_stride(sizes->pipeline) + sizeof(PipelineRaytracingInfo),
				U64_max(
					U64_max(alignof(Pipeline), alignof(PipelineRaytracingInfo)),
					GraphicsObjectSize_alignment(sizes->pipeline)
				)
			),
			.alloc = alloc,
			.free = Pipeline_free
		},

		.sampler = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_Sampler,
			.lengthAndAlignment = OXC3_APPENDED_LEN(Sampler, sizes->sampler),
			.alloc = alloc,
			.free = Sampler_free
		},

		.blas = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_BLASExt,
			.lengthAndAlignment = OXC3_APPENDED_LEN(BLAS, sizes->blas),
			.alloc = alloc,
			.free = BLAS_free
		},

		.tlas = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_TLASExt,
			.lengthAndAlignment = OXC3_APPENDED_LEN(TLAS, sizes->tlas),
			.alloc = alloc,
			.free = TLAS_free
		},

		.opacityMicromap = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_OpacityMicromapExt,
			.lengthAndAlignment = OXC3_APPENDED_LEN(OpacityMicromap, sizes->opacityMicromap),
			.alloc = alloc,
			.free = OpacityMicromap_free
		},

		.descriptorLayout = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DescriptorLayout,
			.lengthAndAlignment = OXC3_APPENDED_LEN(DescriptorLayout, sizes->descriptorLayout),
			.alloc = alloc,
			.free = DescriptorLayout_free
		},

		.descriptorTable = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DescriptorTable,
			.lengthAndAlignment = OXC3_APPENDED_LEN(DescriptorTable, sizes->descriptorTable),
			.alloc = alloc,
			.free = DescriptorTable_free
		},

		.descriptorHeap = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_DescriptorHeap,
			.lengthAndAlignment = OXC3_APPENDED_LEN(DescriptorHeap, sizes->descriptorHeap),
			.alloc = alloc,
			.free = DescriptorHeap_free
		},

		.pipelineLayout = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_PipelineLayout,
			.lengthAndAlignment = OXC3_APPENDED_LEN(PipelineLayout, sizes->pipelineLayout),
			.alloc = alloc,
			.free = PipelineLayout_free
		},

		.commandList = (RefPtrType) {
			.typeId = (TypeId) EGraphicsTypeId_CommandList,
			.lengthAndAlignment = RefPtrType_pack(sizeof(CommandList), alignof(CommandList)),
			.alloc = alloc,
			.free = CommandList_free
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

	//GraphicsInstance_createExt dispatches straight through tables[api].instanceCreate without a null check.
	//On non-Windows, EGraphicsApi_resolve() falls back to Vulkan without consulting the registry.
	//An api whose backend never registered would therefore call a null pointer.
	//Report it as unsupported instead.

	if(!GraphicsInterface_supportsApi(api))
		retError(clean, Error_unsupportedOperation(0, "GraphicsInstance_create()::api has no backend available"));

	const GraphicsObjectSizes *sizes = GraphicsInterface_getObjectSizes(api);

	//type->free deliberately isn't matched against &GraphicsInstance_freeExt, for the reason spelled out in Stream_create().
	//With DynamicLinkingGraphics the backend is a shared lib that links OxC3_graphics PUBLIC,
	// so the app links that static lib as well.
	//Both modules then end up with their own GraphicsInstance_freeExt, which Mach-O keeps separate.
	//typeId, the exact length for this api and alloc still pin the type down to a GraphicsInstance_makeType() result.

	if (
		!type ||
		!sizes ||
		type->typeId != (TypeId) EGraphicsTypeId_GraphicsInstance ||
		RefPtrType_length(type) != (U32) (sizeof(GraphicsInstance) + GraphicsObjectSize_stride(sizes->instance)) ||
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

U64 GraphicsInstance_getValidationErrors(GraphicsInstance *inst) {
	return !inst ? 0 : (U64) AtomicI64_load(&inst->validationErrors);
}

U64 GraphicsInstance_getValidationWarnings(GraphicsInstance *inst) {
	return !inst ? 0 : (U64) AtomicI64_load(&inst->validationWarnings);
}

Bool GraphicsInstance_getDeviceInfos(
	const GraphicsInstance *inst,
	ListGraphicsDeviceInfo *infos,
	const GraphicsDeviceCapFlags *disabledCaps,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!inst || !infos)
		retError(clean, Error_nullPointer(
			!inst ? 0 : 1, "GraphicsInstance_getDeviceInfos()::inst and infos are required"
		));

	gotoIfError3(clean, GraphicsInstance_getDeviceInfosDerived(inst, infos, disabledCaps, e_rr));

clean:
	return s_uccess;
}
