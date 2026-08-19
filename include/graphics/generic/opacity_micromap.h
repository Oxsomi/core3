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

//graphics/generic/opacity_micromap.h

#pragma once
#include "graphics/generic/acceleration_structure.h"
#include "graphics/generic/device_buffer.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Opacity micromaps describe sub triangle opacity, so a ray can resolve transparent regions without ever
// running anyHit.
//This object is the micromap itself; BLAS references one through its OMM index buffer.
//Without one, an OMM index buffer can only hold EOMMSpecialIndex values (see blas.h), which is uniform per
// triangle rather than sub triangle.
//Requires EGraphicsFeatures_RayMicromapOpacity.

//How many opacity states one microtriangle can express.
//The values deliberately match VkOpacityMicromapFormatEXT and D3D12_RAYTRACING_OPACITY_MICROMAP_FORMAT, but
// both backends still map them through a switch so a header change breaks the build rather than the opacity.

//Starts at 1 like both APIs do, so there is no "no format" value: a zeroed format is invalid, not a default.

typedef enum EOpacityMicromapFormat {
	EOpacityMicromapFormat_Opacity2State   = 1,        //1 bit per microtriangle: transparent or opaque
	EOpacityMicromapFormat_Opacity4State   = 2         //2 bits: adds unknown transparent and unknown opaque
} EOpacityMicromapFormat;

//One (subdivisionLevel, format) pair and how many entries of this micromap use it.
//Vulkan calls this VkMicromapUsageEXT, D3D12 calls it a histogram entry; both need it at BUILD time, and
// Vulkan needs it AGAIN in every BLAS that links the micromap, which is why the object keeps its own copy for
// its whole lifetime rather than treating it as a transient build input.

typedef struct OpacityMicromapUsage {
	U32 count;                                 //Entries sharing this pair; the total over all usages is entryCount
	U16 subdivisionLevel;                      //4^level microtriangles per entry
	U16 format;                                //EOpacityMicromapFormat
} OpacityMicromapUsage;

TList(OpacityMicromapUsage);

//One record per micromap entry, laid out exactly as both APIs read it.
//Byte identical to VkMicromapTriangleEXT, and to D3D12_RAYTRACING_OPACITY_MICROMAP_DESC on a little endian
// target, so a single entry buffer feeds either backend unchanged.

typedef struct OpacityMicromapEntry {
	U32 dataOffset;                            //Byte offset into the input buffer where this entry's bits start
	U16 subdivisionLevel;
	U16 format;                                //EOpacityMicromapFormat
} OpacityMicromapEntry;

typedef struct OpacityMicromap {

	RTAS base;                                 //device, flags, isCompleted, asBuffer, tempScratchBuffer, name, lock

	//Build inputs. Both are read during the build only, but the object holds references for its lifetime so a
	// caller releasing them early cannot leave the pending build pointing at freed memory.

	DeviceData inputBuffer;                    //Packed opacity states
	DeviceData entryBuffer;                    //entryCount OpacityMicromapEntry records

	U32 entryStride;                           //Bytes between entries; >= sizeof(OpacityMicromapEntry)
	U32 entryCount;                            //Sum of every usage's count

	ListOpacityMicromapUsage usages;           //Owned copy, needed again by every BLAS that links this

} OpacityMicromap;

typedef RefPtr OpacityMicromapRef;

#define OpacityMicromap_ext(ptr, T) (!ptr ? NULL : (T##OpacityMicromap*)(ptr + 1))        //impl
#define OpacityMicromapRef_ptr(ptr) RefPtr_data(ptr, OpacityMicromap)

//base.asConstructionType and base.flagsExt are meaningless for a micromap and are rejected rather than
// ignored, so a caller cannot believe they did something.
//The legal build flags are ERTASBuildFlags_SupportedOpacityMicromapExt.

typedef struct OpacityMicromapCreateInfo {

	ERTASBuildFlags buildFlags;                //See ERTASBuildFlags_SupportedOpacityMicromapExt

	U32 entryStride;                           //Bytes between entries; >= sizeof(OpacityMicromapEntry)

	DeviceData inputBuffer;                    //Required; needs EDeviceBufferUsage_ASReadExt
	DeviceData entryBuffer;                    //Required; needs EDeviceBufferUsage_ASReadExt

	//Borrowed for the duration of the create call and then copied, so the caller keeps ownership.

	ListOpacityMicromapUsage usages;           //Required; at least one

} OpacityMicromapCreateInfo;

OpacityMicromapCreateInfo OpacityMicromapCreateInfo_create(
	ERTASBuildFlags buildFlags,
	const DeviceData *inputBuffer,
	const DeviceData *entryBuffer,
	U32 entryStride,
	const ListOpacityMicromapUsage *usages
);

//Every entry shares one subdivision level and format, which is the common case.
//The usage only has to outlive the create call.

OpacityMicromapCreateInfo OpacityMicromapCreateInfo_uniform(
	ERTASBuildFlags buildFlags,
	const DeviceData *inputBuffer,
	const DeviceData *entryBuffer,
	U32 entryStride,
	const OpacityMicromapUsage *usage
);

Bool GraphicsDeviceRef_createOpacityMicromapExt(
	GraphicsDeviceRef *dev,
	const OpacityMicromapCreateInfo *info,
	const CharString *name,
	OpacityMicromapRef **micromap,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
