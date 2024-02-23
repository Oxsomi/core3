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
#include "types/lock.h"
#include "device.h"
#include "resource.h"

typedef RefPtr GraphicsDeviceRef;
typedef struct CharString CharString;

typedef RefPtr RTASRef;		//BLASRef or TLASRef

typedef enum ERTASBuildFlags {

	ERTASBuildFlags_None					= 0,

	ERTASBuildFlags_AllowUpdate				= 1 << 0,		//Allow updates using "refitting" (skeletal animations, etc.)
	ERTASBuildFlags_AllowCompaction			= 1 << 1,		//Allow to reduce memory used after compact command
	ERTASBuildFlags_FastTrace				= 1 << 2,		//Prefer optimizing longer / longer builds for better RT perf
	ERTASBuildFlags_FastBuild				= 1 << 3,		//Prefer fast builds over longer builds (might be worse RT perf)
	ERTASBuildFlags_MinimizeMemory			= 1 << 4,		//Ensure both scratch and output mem is reduced (slower builds)

	ERTASBuildFlags_UseDeviceMemory			= 1 << 5,		//Switches between using cpuData or deviceData for all geometries
	ERTASBuildFlags_IsUpdate				= 1 << 6,		//If the current update is a refit (requires parent AS to be set)

	ERTASBuildFlags_Count					= 7,

	ERTASBuildFlags_DefaultTLAS				= ERTASBuildFlags_FastBuild,
	ERTASBuildFlags_DefaultBLAS				= ERTASBuildFlags_FastTrace | ERTASBuildFlags_AllowCompaction

} ETLASBuildFlags;

typedef struct DeviceData {
	DeviceBufferRef *buffer;
	U64 offset, len;
} DeviceData;

typedef struct RTAS {

	GraphicsResource resource;

	U64 lastBuild;							//Used to check if compaction is allowed yet and if completed

	U8 padding[2];
	Bool isMotionBlurExt;					//If active, this will make a motion blur AS
	Bool isCompleted;						//If this is active, we know the TLAS is already done

	Bool isPending, isCompactionPending;	//Compaction happens the frame after the real build; so never if animating
	U8 flags;								//ERTASBuildFlags
	U8 asConstructionType;					//ETLASConstructionType or EBlasConstructionType

	RTASRef *parent;						//Only if Updated / this is a refit

	DeviceBufferRef *scratchBuffer;			//Allocated until the build is complete, unless EBLASConstructionType_Update

	Lock lock;								//Before reading on CPU; for example for refitting

	U64 gpuAddress;							//Changes when compaction or a full rebuild happens

} RTAS;

