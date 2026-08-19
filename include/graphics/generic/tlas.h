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

//graphics/generic/tlas.h

#pragma once
#include "graphics/generic/acceleration_structure.h"
#include "graphics/generic/device_buffer.h"
#include "types/math/quat.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr BLASRef;

typedef enum ETLASInstanceFlag {
	ETLASInstanceFlag_None                      = 0,
	ETLASInstanceFlag_DisableCulling            = 1 << 0,        //Culling is force disabled for the BLAS
	ETLASInstanceFlag_CCW                       = 1 << 1,        //Reverse winding order for the BLAS
	ETLASInstanceFlag_ForceDisableAnyHit        = 1 << 2,        //Force anyHit off for the BLAS
	ETLASInstanceFlag_ForceEnableAnyHit         = 1 << 3,        //Force anyHit on for the BLAS
	ETLASInstanceFlag_Count                     = 4,
	ETLASInstanceFlag_Default                   = ETLASInstanceFlag_DisableCulling | ETLASInstanceFlag_ForceDisableAnyHit
} ETLASInstanceFlag;

//TLAS specific state, which lives in RTAS::flagsExt the way EBLASFlag does for a BLAS.
//A byte of flags rather than a run of Bools because four Bools packed with the bindless handle into exactly 8
// bytes, and a fifth would have cost another 8 to carry one bit.
//UseDeviceMemory and DisallowBindlessDescriptor come from the caller through the create functions, the rest is
// state OxC3 keeps itself.

typedef enum ETLASFlag {

	ETLASFlag_None                       = 0,

	//Instances live in a device buffer (deviceData) rather than in cpuInstances.

	ETLASFlag_UseDeviceMemory            = 1 << 0,

	//Won't allocate a bindless descriptor, so the TLAS can't be reached from a shader.

	ETLASFlag_DisallowBindlessDescriptor = 1 << 1,

	//Cached at create for validation: whether every visible instance's BLAS was built with
	// ERTASBuildFlags_AllowDataAccessExt, so ray triangle position fetch is legal against this TLAS.
	//Only knowable when the instances are CPU side; device built and serialized TLASes leave Known unset.

	ETLASFlag_BlasDataAccessKnown        = 1 << 2,
	ETLASFlag_BlasDataAccessAll          = 1 << 3,

	//Set by TLASRef_setInstancesExt and cleared by the build that consumed it.
	//The instance array lives in a mapped buffer that is filled once at create, so without this a refit would
	// rebuild the structure over the instances it already had.

	ETLASFlag_InstancesDirty             = 1 << 4,

	ETLASFlag_Count                      = 5

} ETLASFlag;

typedef enum ETLASConstructionType {
	ETLASConstructionType_Instances,     //deviceData or cpuInstances contains valid data
	ETLASConstructionType_Serialized,    //cpuData contains serialized data from a previously created AS
	ETLASConstructionType_Count
} ETLASConstructionType;

typedef F32 TLASTransform[3][4];

typedef struct TLASInstanceData {

	U32 instanceId24_mask8;              //InstanceID(): shader-specific instance id AND 8-bit mask to allow disabling per ray
	U32 sbtOffset24_flags8;              //Shader binding table offset AND 8-bit ETLASInstanceFlag

	union {                              //Set any of these two to NULL to hide the instance
		BLASRef *blasCpu;                //Only if created from the CPU
		U64 blasDeviceAddress;           //Otherwise on the device, it should set this to the BLAS's address
	};

} TLASInstanceData;

typedef struct TLASInstance {
	TLASTransform transform;
	TLASInstanceData data;
} TLASInstance;

TList(TLASInstance);

//A TLAS is a ListTLASInstance or a DeviceBuffer that contains TLASInstance[]

typedef U32 BindlessDescriptor;

typedef struct TLAS {

	RTAS base;

	DescriptorTableRef *bindlessDescriptorTable;

	//TLAS specific flags are ETLASFlag in base.flagsExt; read them through TLAS_hasFlag rather than directly,
	// since base.flags right next to it is the BUILD flags and the two are easy to mix up at a glance.

	BindlessDescriptor handle;

	U32 padding0;

	DeviceBufferRef *tempInstanceBuffer;        //If cpuInstances, temp upload heap

	union {

		//If useDeviceMemory; TLASInstance[]
		DeviceData deviceData;

		//If !useDeviceMemory
		ListTLASInstance cpuInstances;

		//If ETLASConstructionType_Serialized
		Buffer cpuData;
	};

} TLAS;

typedef RefPtr TLASRef;

#define TLAS_ext(ptr, T) (!ptr ? NULL : (T##TLAS*)(ptr + 1))        //impl
#define TLASRef_ptr(ptr) RefPtr_data(ptr, TLAS)

static inline Bool TLAS_hasFlag(const TLAS *tlas, ETLASFlag flag) {
	return !!(tlas->base.flagsExt & (U8) flag);
}

Bool TLAS_getInstanceDataCpu(const TLAS *tlas, U64 i, TLASInstanceData *result);

//Replaces the instances a CPU built TLAS holds, so the next recorded update refits it in place.
//The count has to match what the TLAS was built with: both APIs update an existing structure rather than
// resize one, so a scene that gained or lost instances needs a new TLAS instead of a refit.
//Only valid on a TLAS built from CPU instances, and only on one that allows updates.

Bool TLASRef_setInstancesExt(TLASRef *tlas, const ListTLASInstance *instances, Error *e_rr);

//Creating TLASes;
//The changes are queued until the graphics device submits the next commands.
//If the TLAS is deleted before submitting any commands then it won't be filled with anything.

Bool GraphicsDeviceRef_createTLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	const ListTLASInstance *instances,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	TLASRef **tlas,
	Error *e_rr
);

Bool GraphicsDeviceRef_createTLASDeviceExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	const DeviceData *instancesDevice,  //Instances on the GPU, should be sized correctly
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	TLASRef **tlas,
	Error *e_rr
);

//Bool GraphicsDeviceRef_createTLASFromCacheExt(
//    GraphicsDeviceRef *dev, Buffer cache, Bool disallowBindlessDescriptor, CharString name, TLASRef **tlas, Error *e_rr
//);

#ifdef __cplusplus
	}
#endif
