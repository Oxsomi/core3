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

//graphics/generic/acceleration_structure.h

#pragma once
#include "types/container/string.h"
#include "types/base/lock.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;
typedef RefPtr GraphicsDeviceRef;
typedef RefPtr DeviceBufferRef;

typedef struct DeviceData DeviceData;

typedef RefPtr RTASRef;        //BLASRef or TLASRef

typedef enum ERTASBuildFlags {

	ERTASBuildFlags_None                       = 0,

	//Allow updates using "refitting" (skeletal animations, etc.).
	//A refit is IN PLACE: the first build of an AS is a full build, and every build recorded after that
	// one refits the same acceleration structure from itself, so the object, its device address and its
	// bindless handle all stay exactly what they were.
	//Change the inputs and record the update again to refit; a BLAS reads its own position buffer back, a
	// TLAS takes new instances through TLASRef_setInstancesExt.
	//Repeated refits degrade traversal quality on both APIs, so an AS that has drifted far from the
	// geometry it was built for wants a fresh one rather than another refit.

	ERTASBuildFlags_AllowUpdate                = 1 << 0,

	ERTASBuildFlags_AllowCompaction            = 1 << 1,        //Allow to reduce memory used after compact command
	ERTASBuildFlags_FastTrace                  = 1 << 2,        //Prefer optimizing longer / longer builds for better RT perf
	ERTASBuildFlags_FastBuild                  = 1 << 3,        //Prefer fast builds over longer builds (might be worse RT perf)
	ERTASBuildFlags_MinimizeMemory             = 1 << 4,        //Ensure both scratch and output mem is reduced (slower builds)

	//Reserved, free to reuse.
	//Used to mean "this build is a refit", which is no longer something the caller asks for: whether a
	// build refits is decided by whether the AS has been built before.

	ERTASBuildFlags_Reserved5                  = 1 << 5,

	//Keep triangle vertex positions readable from a hit (RayTriPosition / position fetch).
	//Both APIs make this a build time opt in, and the AS is larger with it on, so it stays off by default.

	ERTASBuildFlags_AllowDataAccessExt         = 1 << 6,

	ERTASBuildFlags_Count                      = 7,

	ERTASBuildFlags_DefaultTLAS                = ERTASBuildFlags_FastBuild,
	ERTASBuildFlags_DefaultBLAS                = ERTASBuildFlags_FastTrace | ERTASBuildFlags_AllowCompaction,

	//Every flag an opacity micromap build accepts.
	//Micromaps have no update mode at all (VkBuildMicromapModeEXT only has BUILD), so AllowUpdate is an error
	// rather than a no-op, and there is no micromap counterpart to MinimizeMemory.

	ERTASBuildFlags_SupportedOpacityMicromapExt =
		ERTASBuildFlags_FastTrace | ERTASBuildFlags_FastBuild | ERTASBuildFlags_AllowCompaction

} ERTASBuildFlags;

typedef struct RTAS {

	GraphicsDeviceRef *device;

	U8 padding0[2];
	Bool padding2;
	Bool isCompleted;                          //If this is active, we know the RTAS is already done

	U8 padding1;
	U8 flags;                                  //ERTASBuildFlags
	U8 flagsExt;                               //For BLAS; EBLASFlag
	U8 asConstructionType;                     //ETLASConstructionType or EBlasConstructionType

	//Compaction bookkeeping; see CommandListRef_compactBLASExt for what any of it means to a caller.
	//
	//compactionQuery is the slot holding this structure's compacted size, U32_MAX when none was claimed.
	//compactionSubmitId is the submit that RECORDED it, since the size does not exist until that submit
	//has completed.

	U64 compactionSubmitId;

	//The smaller structure a recorded compaction will copy INTO, allocated when the copy is recorded and
	//swapped in for asBuffer as the op is encoded into the command buffer.

	DeviceBufferRef *pendingCompactBuffer;

	U32 compactionQuery;
	Bool isCompacted;
	U8 padding3[3];

	DeviceBufferRef *asBuffer;                 //The acceleration structure as a buffer
	DeviceBufferRef *tempScratchBuffer;        //Not required, but might include scratch buffer for temp build memory

	CharString name;                           //Debug name

	SpinLock lock;                             //Before reading on CPU; for example for refitting

} RTAS;

Bool RTAS_validateDeviceBuffer(DeviceData *bufPtr, Error *e_rr);        //Check if buffer is accessible by RTAS (BLAS/TLAS)

#ifdef __cplusplus
	}
#endif
