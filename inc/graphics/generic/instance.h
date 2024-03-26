/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/string.h"
#include "types/ref_ptr.h"
#include "device_info.h"

//In line with vulkan standard.
#define GraphicsApplicationInfo_Version(major, minor, patch)	((major) << 22) | ((minor) << 12) | (patch)

typedef struct GraphicsApplicationInfo {
	CharString name;
	U32 version, padding;
} GraphicsApplicationInfo;

typedef enum EGraphicsApi {
	EGraphicsApi_Vulkan,
	EGraphicsApi_DirectX12,
	//EGraphicsApi_Metal,
	//EGraphicsApi_WebGPU,
	EGraphicsApi_Count
} EGraphicsApi;

typedef struct GraphicsInstance {

	GraphicsApplicationInfo application;

	EGraphicsApi api;
	U32 apiVersion;

} GraphicsInstance;

typedef RefPtr GraphicsInstanceRef;

#define GraphicsInstance_ext(ptr, T) (!ptr ? NULL : (T##GraphicsInstance*)(ptr + 1))		//impl
#define GraphicsInstanceRef_ptr(ptr) RefPtr_data(ptr, GraphicsInstance)

Error GraphicsInstanceRef_dec(GraphicsInstanceRef **inst);
Error GraphicsInstanceRef_inc(GraphicsInstanceRef *inst);

Error GraphicsInstance_create(GraphicsApplicationInfo info, Bool isVerbose, GraphicsInstanceRef **inst);

TList(GraphicsDeviceInfo);

impl Error GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, Bool isVerbose, ListGraphicsDeviceInfo *infos);

static const U64 GraphicsInstance_vendorMaskAll = 0xFFFFFFFFFFFFFFFF;
static const U64 GraphicsInstance_deviceTypeAll = 0xFFFFFFFFFFFFFFFF;

Error GraphicsInstance_getPreferredDevice(
	const GraphicsInstance *inst,
	GraphicsDeviceCapabilities requiredCapabilities,
	U64 vendorMask,
	U64 deviceTypeMask,
	Bool verbose,
	GraphicsDeviceInfo *deviceInfo
);
