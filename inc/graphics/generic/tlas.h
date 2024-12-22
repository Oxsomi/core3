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
#include "acceleration_structure.h"
#include "device_buffer.h"
#include "types/math/quat.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef RefPtr BLASRef;

typedef enum ETLASInstanceFlag {
	ETLASInstanceFlag_None						= 0,
	ETLASInstanceFlag_DisableCulling			= 1 << 0,		//Culling is force disabled for the BLAS
	ETLASInstanceFlag_CCW						= 1 << 1,		//Reverse winding order for the BLAS
	ETLASInstanceFlag_ForceDisableAnyHit		= 1 << 2,		//Force anyHit off for the BLAS
	ETLASInstanceFlag_ForceEnableAnyHit			= 1 << 3,		//Force anyHit on for the BLAS
	ETLASInstanceFlag_Count						= 4,
	ETLASInstanceFlag_Default					= ETLASInstanceFlag_DisableCulling | ETLASInstanceFlag_ForceDisableAnyHit
} ETLASInstanceFlag;

typedef enum ETLASConstructionType {
	ETLASConstructionType_Instances,	//deviceData, cpuInstancesMotion or cpuInstancesStatic contains valid data
	ETLASConstructionType_Serialized,	//cpuData contains serialized data from a previously created AS
	ETLASConstructionType_Count
} ETLASConstructionType;

typedef F32 TLASTransform[3][4];

typedef struct TLASInstanceData {

	U32 instanceId24_mask8;				//InstanceID(): shader-specific instance id AND 8-bit mask to allow disabling per ray
	U32 sbtOffset24_flags8;				//Shader binding table offset AND 8-bit ETLASInstanceFlag

	union {								//Set any of these two to NULL to hide the instance
		BLASRef *blasCpu;				//Only if created from the CPU
		U64 blasDeviceAddress;			//Otherwise on the device, it should set this to the BLAS's address
	};

} TLASInstanceData;

typedef enum ETLASInstanceType {
	ETLASInstanceType_Static,			//TLASInstanceStatic
	ETLASInstanceType_Matrix,			//TLASInstanceMatrix
	ETLASInstanceType_SRT,				//TLASInstanceSRT
	ETLASInstanceType_Count
} ETLASInstanceType;

//These are the different type of TLASInstances;
//TLASInstance itself is CPU-only & wraps two types; so the TLAS could be used to support HW motion blur with a simple flag.
//TLASInstanceDevice is the default, while TLASInstanceMotionDevice is the specific one for the HW RT motion blur extension.

//https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkSRTDataNV.html
typedef struct TLASTransformSRT {

	//sx, sy and sz are scale
	//pvx, pvy, pvz are pivots
	//tx, ty, tz are translate
	//q0, q1, q2, q3 is a QuatF32 for the orientation
	//a, b, c is shearing

	F32 sx, a, b;
	F32 pvx, sy, c;
	F32 pvy, sz, pvz;
	F32 q0, q1, q2, q3;		//QuatF32 (but doesn't properly align)
	F32 tx, ty, tz;

} TLASTransformSRT;

F32x4 TLASTransformSRT_create(F32x4 scale, F32x4 pivot, F32x4 translate, QuatF32 quat, F32x4 shearing);
F32x4 TLASTransformSRT_createSimple(F32x4 scale, F32x4 translate, QuatF32 quat);

F32x4 TLASTransformSRT_getScale(TLASTransformSRT srt);
Bool TLASTransformSRT_setScale(TLASTransformSRT *srt, F32x4 value);

F32x4 TLASTransformSRT_getPivot(TLASTransformSRT srt);
Bool TLASTransformSRT_setPivot(TLASTransformSRT *srt, F32x4 value);

F32x4 TLASTransformSRT_getTranslate(TLASTransformSRT srt);
Bool TLASTransformSRT_setTranslate(TLASTransformSRT *srt, F32x4 value);

QuatF32 TLASTransformSRT_getQuat(TLASTransformSRT srt);
Bool TLASTransformSRT_setQuat(TLASTransformSRT *srt, QuatF32 value);

F32x4 TLASTransformSRT_getShearing(TLASTransformSRT srt);
Bool TLASTransformSRT_setShearing(TLASTransformSRT *srt, F32x4 value);

typedef struct TLASInstanceStatic {
	TLASTransform transform;
	TLASInstanceData data;
} TLASInstanceStatic;

typedef struct TLASInstanceSRT {
	TLASTransformSRT prev, next;
	TLASInstanceData data;
} TLASInstanceSRT;

typedef struct TLASInstanceMatrixMotion {
	TLASTransform prev, next;
	TLASInstanceData data;
} TLASInstanceMatrixMotion;

typedef struct TLASInstanceMotion {

	ETLASInstanceType type;
	U32 padding;

	union {
		TLASInstanceSRT srtInst;					//ETLASInstanceMotionType_SRT
		TLASInstanceMatrixMotion matrixInst;		//ETLASInstanceMotionType_Matrix
		TLASInstanceStatic staticInst;				//ETLASInstanceMotionType_Static
	};

} TLASInstanceMotion;

TLASInstanceData TLASInstanceMotion_getData(TLASInstanceMotion mot);

TList(TLASInstanceMotion);
TList(TLASInstanceStatic);

//A TLAS is a ListTLASInstance or a DeviceBuffer that contains either
// TLASInstance[] (isMotionBlurExt) or TLASInstanceStatic[] (!isMotionBlurExt)

typedef struct TLAS {

	RTAS base;

	Bool useDeviceMemory;
	U8 padding[3];

	U32 handle;

	DeviceBufferRef *tempInstanceBuffer;		//If cpuInstanceMotion or cpuInstancesStatic, temp upload heap

	union {

		//If useDeviceMemory
		//TLASInstanceMotion[] (isMotionBlurExt) or TLASInstanceStatic[] (!isMotionBlurExt)
		DeviceData deviceData;

		//If !useDeviceMemory && isMotionBlurExt
		ListTLASInstanceMotion cpuInstancesMotion;

		//If !useDeviceMemory && !isMotionBlurExt
		ListTLASInstanceStatic cpuInstancesStatic;

		//If ETLASConstructionType_Serialized
		Buffer cpuData;
	};

} TLAS;

typedef RefPtr TLASRef;

#define TLAS_ext(ptr, T) (!ptr ? NULL : (T##TLAS*)(ptr + 1))		//impl
#define TLASRef_ptr(ptr) RefPtr_data(ptr, TLAS)

Error TLASRef_dec(TLASRef **tlas);
Error TLASRef_inc(TLASRef *tlas);

Bool TLAS_getInstanceDataCpu(const TLAS *tlas, U64 i, TLASInstanceData *result);

//Creating TLASes;
//The changes are queued until the graphics device submits the next commands.
//If the TLAS is deleted before submitting any commands then it won't be filled with anything.

Error GraphicsDeviceRef_createTLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	TLASRef *parent,					//If specified, indicates refit
	ListTLASInstanceStatic instances,
	CharString name,
	TLASRef **tlas
);

Error GraphicsDeviceRef_createTLASMotionExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	TLASRef *parent,					//If specified, indicates refit
	ListTLASInstanceMotion instances,
	CharString name,
	TLASRef **tlas
);

Error GraphicsDeviceRef_createTLASDeviceExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	Bool isMotionBlurExt,				//Requires extension
	TLASRef *parent,					//If specified, indicates refit
	DeviceData instancesDevice,			//Instances on the GPU, should be sized correctly
	CharString name,
	TLASRef **tlas
);

//Error GraphicsDeviceRef_createTLASFromCacheExt(GraphicsDeviceRef *dev, Buffer cache, CharString name, TLASRef **tlas);

#ifdef __cplusplus
	}
#endif
