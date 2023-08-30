/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

//In line with vulkan standard.
#define GraphicsApplicationInfo_Version(major, minor, patch)	\
((major) << 22) | ((minor) << 12) | (patch)

typedef struct GraphicsApplicationInfo {

	CharString name;

	U32 version, padding;

} GraphicsApplicationInfo;

typedef enum EGraphicsApi {
	EGraphicsApi_Vulkan,
	EGraphicsApi_DirectX12,
	EGraphicsApi_Metal,
	EGraphicsApi_WebGPU
} EGraphicsApi;

typedef struct GraphicsInstance {

	GraphicsApplicationInfo application;

	EGraphicsApi api;
	U32 apiVersion;

	void *ext;

} GraphicsInstance;

typedef struct GraphicsDeviceCapabilities GraphicsDeviceCapabilities;
typedef struct GraphicsDeviceInfo GraphicsDeviceInfo;

impl Error GraphicsInstance_create(GraphicsApplicationInfo info, Bool isVerbose, GraphicsInstance *inst);
impl Bool GraphicsInstance_free(GraphicsInstance *inst);

impl Error GraphicsInstance_getDeviceInfos(const GraphicsInstance *inst, Bool isVerbose, List *infos);	//<GraphicsDeviceInfo>

extern U64 GraphicsInstance_vendorMaskAll;
extern U64 GraphicsInstance_deviceTypeAll;

Error GraphicsInstance_getPreferredGpu(
	const GraphicsInstance *inst, 
	GraphicsDeviceCapabilities requiredCapabilities, 
	U64 vendorMask,
	U64 deviceTypeMask,
	Bool verbose,
	GraphicsDeviceInfo *deviceInfo
);
