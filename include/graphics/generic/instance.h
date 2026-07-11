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

//graphics/generic/instance.h

#pragma once
#include "types/container/string.h"
#include "graphics/generic/device_info.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef struct RefPtrType RefPtrType;
typedef struct Allocator Allocator;

typedef struct GraphicsApplicationInfo {
	CharString name;
	U32 version, padding;
} GraphicsApplicationInfo;

#define GRAPHICS_API_VULKAN 0
#define GRAPHICS_API_D3D12 1

typedef enum EGraphicsApi {
	EGraphicsApi_Vulkan            = GRAPHICS_API_VULKAN,
	EGraphicsApi_Direct3D12        = GRAPHICS_API_D3D12,
	//EGraphicsApi_Metal,
	//EGraphicsApi_WebGPU,
	EGraphicsApi_Count
} EGraphicsApi;

extern const C8 *EGraphicsApi_name[EGraphicsApi_Count];

typedef enum EGraphicsInstanceFlags {
	EGraphicsInstanceFlags_None              = 0,
	EGraphicsInstanceFlags_IsDebug           = 1 << 0,
	EGraphicsInstanceFlags_IsVerbose         = 1 << 1,
	EGraphicsInstanceFlags_DisableDebug      = 1 << 2,
	EGraphicsInstanceFlags_DisableGPUBV      = 1 << 3    //Disable GPU based validation (but keep other debugging)
} EGraphicsInstanceFlags;

//RefPtrTypes of every object kind the instance (and its devices) can create.
//These are stored here because the RefPtrType must stay alive for as long as any RefPtr created with it.
//The lengths depend on the api's ext sizes, the allocator is the one the instance was created with.

typedef struct GraphicsObjectTypes {
	RefPtrType device, buffer, deviceTexture, renderTexture, depthStencil, swapchain;
	RefPtrType pipeline, sampler, blas, tlas;
	RefPtrType descriptorLayout, descriptorTable, descriptorHeap, pipelineLayout;
	RefPtrType commandList;
} GraphicsObjectTypes;

typedef struct GraphicsInstance {

	GraphicsApplicationInfo application;

	EGraphicsApi api;
	U32 apiVersion;

	EGraphicsInstanceFlags flags;
	U32 padding;

	const Allocator *alloc;             //Allocator all graphics objects of this instance are created with

	GraphicsObjectTypes types;

} GraphicsInstance;

typedef RefPtr GraphicsInstanceRef;
typedef struct ListGraphicsDeviceInfo ListGraphicsDeviceInfo;

#define GraphicsInstance_ext(ptr, T) (!ptr ? NULL : (T##GraphicsInstance*)(ptr + 1))        //impl
#define GraphicsInstanceRef_ptr(ptr) RefPtr_data(ptr, GraphicsInstance)

Bool GraphicsInterface_create(Error *e_rr);              //Prepare interface to query info about supported APIs
Bool GraphicsInterface_supportsApi(EGraphicsApi api);

//Resolves EGraphicsApi_Count into the default api of the current platform
EGraphicsApi EGraphicsApi_resolve(EGraphicsApi api);

//Make the RefPtrType used to allocate the GraphicsInstance itself.
//The result must stay alive for as long as the instance does (see RefPtr_create).
//GraphicsInterface_create() must have been called before for a valid result (object sizes).
RefPtrType GraphicsInstance_makeType(EGraphicsApi api, const Allocator *alloc);

Bool GraphicsInstance_create(
	const GraphicsApplicationInfo *info,
	EGraphicsApi api,                                    //EGraphicsApi_Count = Default
	EGraphicsInstanceFlags flags,
	const Allocator *alloc,                              //Must match type->alloc
	const RefPtrType *type,                              //GraphicsInstance_makeType (must outlive instance)
	GraphicsInstanceRef **inst,
	Error *e_rr
);

Bool GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, ListGraphicsDeviceInfo *infos, Error *e_rr);

TList(GraphicsDeviceInfo);

static const U64 GraphicsInstance_vendorMaskAll = 0xFFFFFFFFFFFFFFFF;
static const U64 GraphicsInstance_deviceTypeAll = 0xFFFFFFFFFFFFFFFF;

Bool GraphicsInstance_getPreferredDevice(
	const GraphicsInstance *inst,
	const GraphicsDeviceCapabilities *requiredCapabilities,
	U64 vendorMask,
	U64 deviceTypeMask,
	GraphicsDeviceInfo *deviceInfo,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
