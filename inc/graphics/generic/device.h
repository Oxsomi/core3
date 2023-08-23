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

typedef enum EDeviceType {

	EDeviceType_Dedicated,
	EDeviceType_Integrated,
	EDeviceType_CPU,
	EDeviceType_Other

} EDeviceType;

typedef enum EDeviceVendor {

	EDeviceVendor_NV,
	EDeviceVendor_AMD,
	EDeviceVendor_ARM,
	EDeviceVendor_QCOM,
	EDeviceVendor_INTC,
	EDeviceVendor_IMGT,
	EDeviceVendor_Unknown

} EDeviceVendor;

typedef enum EDeviceFeatures {

	//When this is turned on, the GPU benefits from tiled rendering.
	//This is true for mobile devices only or some chips such as QCOM on windows.

	EDeviceFeature_TiledRendering			= 1 << 0,

	//Raytracing extensions

	EDeviceFeature_Raytracing				= 1 << 1,
	EDeviceFeature_RayQuery					= 1 << 2,
	EDeviceFeature_RayMicromapOpacity		= 1 << 3,
	EDeviceFeature_RayMicromapDisplacement	= 1 << 4,
	EDeviceFeature_RayMotionBlur			= 1 << 5,
	EDeviceFeature_RayReorder				= 1 << 6,

	//Other important extensions

	EDeviceFeature_MeshShaders				= 1 << 7,
	EDeviceFeature_VariableRateShading		= 1 << 8,

	EDeviceFeature_MultiDrawIndirectCount	= 1 << 9,

	EDeviceFeature_GeometryShader			= 1 << 10,
	EDeviceFeature_TesellationShader		= 1 << 11,

	EDeviceFeature_SubgroupArithmetic		= 1 << 12,
	EDeviceFeature_SubgroupShuffle			= 1 << 13,

	//Up to 200k of each type (except dynamic) are supported.
	//This is only turned off for Apple devices and older phones.
	//Minimum OxC3 spec Android phones, NV/AMD/Intel laptops/desktops all support this.
	//Only apple doesn't...

	EDeviceFeature_ExtendedDescriptorSize	= 1 << 14

} EDeviceFeatures;

typedef enum EDeviceDataTypes {

	//What operations are available on native data types

	EDeviceDataTypes_I64					= 1 << 0,
	EDeviceDataTypes_F16					= 1 << 1,
	EDeviceDataTypes_F64					= 1 << 2,

	EDeviceDataTypes_AtomicI64				= 1 << 3,
	EDeviceDataTypes_AtomicF32				= 1 << 4,

	//What texture formats are available
	//These can be both supported.

	EDeviceDataTypes_ASTC					= 1 << 5,			//If false, BCn has to be supported
	EDeviceDataTypes_BCn					= 1 << 6,			//If false, ASTC has to be supported

	//What formats are available for the swapchain

	EDeviceDataTypes_HDR10A2				= 1 << 7,
	EDeviceDataTypes_RGBA16f				= 1 << 8

} EDeviceDataTypes;

typedef struct DeviceInfo {

	C8 name[256];
	
	EDeviceType type;
	EDeviceVendor vendor;

	U32 id;
	EDeviceFeatures features;

	EDeviceDataTypes dataTypes;
	U32 pad;

} DeviceInfo;
