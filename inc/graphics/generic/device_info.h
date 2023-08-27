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
#include "types/types.h"

typedef enum EGraphicsDeviceType {

	EGraphicsDeviceType_Dedicated,
	EGraphicsDeviceType_Integrated,
	EGraphicsDeviceType_Simulated,
	EGraphicsDeviceType_CPU,
	EGraphicsDeviceType_Other

} EGraphicsDeviceType;

typedef enum EGraphicsVendor {

	EGraphicsVendor_NV,
	EGraphicsVendor_AMD,
	EGraphicsVendor_ARM,
	EGraphicsVendor_QCOM,
	EGraphicsVendor_INTC,
	EGraphicsVendor_IMGT,
	EGraphicsVendor_Unknown

} EGraphicsVendor;

typedef enum EGraphicsFeatures {

	//When this is turned on, the GPU benefits from tiled rendering.
	//This is true for mobile devices only or some chips such as QCOM on windows.

	EGraphicsFeatures_TiledRendering			= 1 << 0,

	//EGraphicsFeatures_VariableRateShading		= 1 << 1,		TODO:

	EGraphicsFeatures_MultiDrawIndirectCount	= 1 << 2,

	EGraphicsFeatures_MeshShader				= 1 << 3,		//Mesh and task shaders
	EGraphicsFeatures_GeometryShader			= 1 << 4,
	EGraphicsFeatures_TessellationShader		= 1 << 5,

	EGraphicsFeatures_SubgroupArithmetic		= 1 << 6,
	EGraphicsFeatures_SubgroupShuffle			= 1 << 7,

	EGraphicsFeatures_Swapchain			= 1 << 8,

	//Raytracing extensions

	EGraphicsFeatures_Raytracing				= 1 << 9,		//Requires RayPipeline or RayQuery
	EGraphicsFeatures_RayPipeline				= 1 << 10,
	EGraphicsFeatures_RayIndirect				= 1 << 11,
	EGraphicsFeatures_RayQuery					= 1 << 12,
	EGraphicsFeatures_RayMicromapOpacity		= 1 << 13,
	EGraphicsFeatures_RayMicromapDisplacement	= 1 << 14,
	EGraphicsFeatures_RayMotionBlur				= 1 << 15,
	EGraphicsFeatures_RayReorder				= 1 << 16,

	//LUID for sharing devices

	EGraphicsFeatures_SupportsLUID				= 1 << 17

} EGraphicsFeatures;

typedef enum EGraphicsDataTypes {

	//What operations are available on native data types

	EGraphicsDataTypes_I64					= 1 << 0,
	EGraphicsDataTypes_F16					= 1 << 1,
	EGraphicsDataTypes_F64					= 1 << 2,

	EGraphicsDataTypes_AtomicI64			= 1 << 3,

	//What texture formats are available
	//These can be both supported.

	EGraphicsDataTypes_ASTC					= 1 << 4,			//If false, BCn has to be supported
	EGraphicsDataTypes_BCn					= 1 << 5,			//If false, ASTC has to be supported

	//What formats are available for the swapchain

	EGraphicsDataTypes_HDR10A2				= 1 << 6,
	EGraphicsDataTypes_RGBA16f				= 1 << 7,

	//If render targets can have MSAA8x, 2x or 16x.

	EGraphicsDataTypes_MSAA2x				= 1 << 8,
	EGraphicsDataTypes_MSAA8x				= 1 << 9,
	EGraphicsDataTypes_MSAA16x				= 1 << 10

} EGraphicsDataTypes;

//Up to 200k of each type (except dynamic) are supported.
//This is only turned off for Apple devices and older phones.
//Minimum OxC3 spec Android phones, NV/AMD/Intel laptops/desktops all support this.
//Only apple doesn't...

typedef enum EGraphicsBindingTier {

	EGraphicsBindingTier_Low,
	EGraphicsBindingTier_High

} EGraphicsBindingTier;

//This struct represents the abilities a graphics device has.

typedef struct GraphicsDeviceCapabilities {

	EGraphicsFeatures features;

	EGraphicsDataTypes dataTypes;
	EGraphicsBindingTier bindingTier;

	U32 featuresExt;				//Extended device features, API dependent

} GraphicsDeviceCapabilities;

//The device info struct represents a physical device.

typedef struct GraphicsDeviceInfo {

	C8 name[256];
	C8 driverName[256];
	C8 driverInfo[256];
	
	EGraphicsDeviceType type;
	EGraphicsVendor vendor;

	U32 id;
	GraphicsDeviceCapabilities capabilities;

	U64 luid;			//Check SupportsLUID

	U64 uuid[2];

	void *ext;

} GraphicsDeviceInfo;
