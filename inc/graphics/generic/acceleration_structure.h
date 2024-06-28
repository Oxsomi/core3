/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "device_buffer.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr GraphicsDeviceRef;

typedef RefPtr RTASRef;		//BLASRef or TLASRef

typedef enum ERTASBuildFlags {

	ERTASBuildFlags_None					= 0,

	ERTASBuildFlags_AllowUpdate				= 1 << 0,		//Allow updates using "refitting" (skeletal animations, etc.)
	ERTASBuildFlags_AllowCompaction			= 1 << 1,		//Allow to reduce memory used after compact command
	ERTASBuildFlags_FastTrace				= 1 << 2,		//Prefer optimizing longer / longer builds for better RT perf
	ERTASBuildFlags_FastBuild				= 1 << 3,		//Prefer fast builds over longer builds (might be worse RT perf)
	ERTASBuildFlags_MinimizeMemory			= 1 << 4,		//Ensure both scratch and output mem is reduced (slower builds)

	ERTASBuildFlags_IsUpdate				= 1 << 5,		//If the current update is a refit (requires parent AS to be set)

	ERTASBuildFlags_Count					= 6,

	ERTASBuildFlags_DefaultTLAS				= ERTASBuildFlags_FastBuild,
	ERTASBuildFlags_DefaultBLAS				= ERTASBuildFlags_FastTrace | ERTASBuildFlags_AllowCompaction

} ERTASBuildFlags;

typedef struct RTAS {

	GraphicsDeviceRef *device;

	U8 padding0[2];
	Bool isMotionBlurExt;					//If active, this will make a motion blur AS
	Bool isCompleted;						//If this is active, we know the RTAS is already done

	U8 padding1;
	U8 flags;								//ERTASBuildFlags
	U8 flagsExt;							//For BLAS; EBLASFlag
	U8 asConstructionType;					//ETLASConstructionType or EBlasConstructionType

	RTASRef *parent;						//Only if Updated / this is a refit

	DeviceBufferRef *asBuffer;				//The acceleration structure as a buffer
	DeviceBufferRef *tempScratchBuffer;		//Not required, but might include scratch buffer for temp build memory

	CharString name;						//Debug name

	SpinLock lock;								//Before reading on CPU; for example for refitting

} RTAS;

Error RTAS_validateDeviceBuffer(DeviceData *bufPtr);		//Check if buffer is accessible by RTAS (BLAS/TLAS)

#ifdef __cplusplus
	}
#endif
