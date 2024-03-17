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

#include "platforms/ext/listx_impl.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "types/buffer.h"

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

TListImpl(TLASInstanceMotion);
TListImpl(TLASInstanceStatic);

TLASInstanceData *TLASInstanceMotion_getDataInternal(TLASInstanceMotion *mot) {
	switch (mot->type) {
		default:							return &mot->staticInst.data;
		case ETLASInstanceType_Matrix:		return &mot->matrixInst.data;
		case ETLASInstanceType_SRT:			return &mot->srtInst.data;
	}
}

TLASInstanceData TLASInstanceMotion_getData(TLASInstanceMotion mot) {
	return *TLASInstanceMotion_getDataInternal(&mot);
}

Error TLASRef_dec(TLASRef **tlas) {
	return !RefPtr_dec(tlas) ? Error_invalidOperation(0, "TLASRef_dec()::tlas is required") : Error_none();
}

Error TLASRef_inc(TLASRef *tlas) {
	return !RefPtr_inc(tlas) ? Error_invalidOperation(0, "TLASRef_inc()::tlas is required") : Error_none();
}

Bool TLAS_getInstanceDataCpuInternal(const TLAS *tlas, U64 i, TLASInstanceData **result) {

	if(
		!result ||
		tlas->useDeviceMemory || 
		tlas->base.asConstructionType != ETLASConstructionType_Instances || 
		i >= tlas->cpuInstancesMotion.length
	)
		return false;

	if(tlas->base.isMotionBlurExt) {

		TLASInstanceMotion *motion = &tlas->cpuInstancesMotion.ptrNonConst[i];

		if(motion->type >= ETLASInstanceType_Count)
			return false;

		*result = TLASInstanceMotion_getDataInternal(motion);
	}

	else *result = &tlas->cpuInstancesStatic.ptrNonConst[i].data;

	return true;
}

Bool TLAS_getInstanceDataCpu(const TLAS *tlas, U64 i, TLASInstanceData *result) {

	if(!result)
		return false;

	TLASInstanceData *data = NULL;
	Bool b = TLAS_getInstanceDataCpuInternal(tlas, i, &data);

	if(!b)
		return false;

	*result = *data;
	return true;
}

impl extern const U64 TLASExt_size;
impl Bool TLAS_freeExt(TLAS *tlas);

Bool TLAS_free(TLAS *tlas, Allocator allocator) {

	(void)allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)tlas - sizeof(RefPtr));

	Lock_free(&tlas->base.lock);

	Bool success = GraphicsDeviceRef_removePending(tlas->base.device, refPtr);
	success &= TLAS_freeExt(tlas);
	success &= CharString_freex(&tlas->base.name);

	success &= !DeviceBufferRef_dec(&tlas->base.asBuffer).genericError;

	if(tlas->base.asConstructionType == ETLASConstructionType_Serialized)
		success &= Buffer_freex(&tlas->cpuData);

	else if (tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		if(tlas->useDeviceMemory)
			success &= !DeviceBufferRef_dec(&tlas->deviceData.buffer).genericError;

		else {

			for(U64 i = 0; i < tlas->cpuInstancesStatic.length; ++i) {
				TLASInstanceData *result = NULL;
				TLAS_getInstanceDataCpuInternal(tlas, i, &result);
				BLASRef_dec(&result->blasCpu);
			}

			if(tlas->base.isMotionBlurExt)
				success &= ListTLASInstanceMotion_freex(&tlas->cpuInstancesMotion);

			else success &= ListTLASInstanceStatic_freex(&tlas->cpuInstancesStatic);
		}
	}

	GraphicsDevice *device = GraphicsDeviceRef_ptr(tlas->base.device);
	ELockAcquire acq = Lock_lock(&device->descriptorLock, U64_MAX);

	if (acq >= ELockAcquire_Success) {

		ListU32 allocations = (ListU32) { 0 };
		ListU32_createRefConst(&tlas->handle, 1, &allocations);
		GraphicsDeviceRef_freeDescriptors(tlas->base.device, &allocations);

		if(acq == ELockAcquire_Acquired)
			Lock_unlock(&device->descriptorLock);
	}

	success &= !GraphicsDeviceRef_dec(&tlas->base.device).genericError;
	return success;
}

Error GraphicsDeviceRef_createTLAS(GraphicsDeviceRef *dev, TLAS tlas, CharString name, TLASRef **tlasRef) {

	//Validate

	if(!dev || dev->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createTLAS()::dev is required");

	if(!tlasRef)
		return Error_nullPointer(3, "GraphicsDeviceRef_createTLAS()::tlasRef is required");

	if(*tlasRef)
		return Error_invalidParameter(3, 0, "GraphicsDeviceRef_createTLAS()::*tlasRef not NULL, indicates memleak");

	if(tlas.base.parent && tlas.base.parent->typeId != (ETypeId) EGraphicsTypeId_TLASExt)
		return Error_invalidOperation(1, "GraphicsDeviceRef_createTLAS()::parent is invalid");

	if(tlas.base.parent && TLASRef_ptr(tlas.base.parent)->base.device != dev)
		return Error_invalidOperation(1, "GraphicsDeviceRef_createTLAS()::parent and TLAS device need to share device");

	if(tlas.base.parent)
		tlas.base.flags |= ERTASBuildFlags_IsUpdate;

	else if(tlas.base.flags & ERTASBuildFlags_IsUpdate)
		return Error_invalidOperation(7, "GraphicsDeviceRef_createTLAS()::parent is required if IsUpdate is present");

	if(!(tlas.base.flags & ERTASBuildFlags_AllowUpdate) && (tlas.base.flags & ERTASBuildFlags_IsUpdate))
		return Error_invalidOperation(8, "GraphicsDeviceRef_createTLAS() is update is not possible if AllowUpdate is false");

	EGraphicsFeatures feat = GraphicsDeviceRef_ptr(dev)->info.capabilities.features;

	if(!(feat & EGraphicsFeatures_Raytracing))
		return Error_unsupportedOperation(0, "GraphicsDeviceRef_createTLAS() is unsupported without raytracing support");

	if(tlas.base.isMotionBlurExt && !(feat & EGraphicsFeatures_RayMotionBlur))
		return Error_unsupportedOperation(0, "GraphicsDeviceRef_createTLAS() uses motion blur, but it's unsupported");

	//Validate TLAS

	if(tlas.base.asConstructionType == ETLASConstructionType_Instances) {

		if (tlas.useDeviceMemory) {

			Error err = RTAS_validateDeviceBuffer(&tlas.deviceData);

			if(err.genericError)
				return err;

			U64 stride = tlas.base.isMotionBlurExt ? sizeof(TLASInstanceMotion) : sizeof(TLASInstanceStatic);

			if(tlas.deviceData.len < stride || tlas.deviceData.len % stride)
				return Error_invalidOperation(9, "GraphicsDeviceRef_createTLAS() invalid AS buffer size");
		}

		else {

			U64 length = tlas.cpuInstancesMotion.length;		//instancesMotion and instancesStatic are at the same loc

			if(!length)
				return Error_invalidOperation(10, "GraphicsDeviceRef_createTLAS() is missing instance list");

			for (U64 i = 0; i < length; ++i) {

				TLASInstanceData dat = (TLASInstanceData) { 0 };
				if(!TLAS_getInstanceDataCpu(&tlas, i, &dat))
					return Error_invalidOperation(11, "GraphicsDeviceRef_createTLAS() can't get instance data cpu");

				if(dat.blasCpu) {

					if(dat.blasCpu->typeId != (ETypeId) EGraphicsTypeId_BLASExt)
						return Error_invalidOperation(12, "GraphicsDeviceRef_createTLAS() invalid BLAS type");

					if(BLASRef_ptr(dat.blasCpu)->base.device != dev)
						return Error_invalidOperation(13, "GraphicsDeviceRef_createTLAS() BLAS device is incompatible");
				}

				if(!(dat.instanceId24_mask8 >> 24))
					return Error_invalidOperation(
						14, 
						"GraphicsDeviceRef_createTLAS() BLAS mask is 0, this might be unintended. "
						"Set blasCpu to NULL instead to explicitly hide the instance."
					);
			}
		}
	}

	//Validate serialized

	else if(!Buffer_length(tlas.cpuData))
		return Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createTLAS()::cpuData should be valid if serialized construction is used"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	Error err = Error_none();
	ELockAcquire acq0 = ELockAcquire_Invalid;
	ELockAcquire acq1 = ELockAcquire_Invalid;

	//Allocate refPtr

	gotoIfError(clean, RefPtr_createx(
		(U32) (sizeof(TLAS) + TLASExt_size),
		(ObjectFreeFunc) TLAS_free,
		(ETypeId) EGraphicsTypeId_TLASExt,
		tlasRef
	));

	//Fill ptr

	TLAS *tlasPtr = TLASRef_ptr(*tlasRef);

	if(tlas.base.parent)
		gotoIfError(clean, TLASRef_inc(tlas.base.parent));

	*tlasPtr = tlas;
	tlasPtr->base.lock = Lock_create();
	tlasPtr->base.name = CharString_createNull();

	if (tlas.base.asConstructionType == ETLASConstructionType_Serialized) {
		tlasPtr->cpuData = Buffer_createNull();
		gotoIfError(clean, Buffer_createCopyx(tlas.cpuData, &tlasPtr->cpuData));
	}
	else {

		if (tlas.useDeviceMemory) {
			tlasPtr->deviceData = (DeviceData) { 0 };
			gotoIfError(clean, DeviceBufferRef_inc(tlas.deviceData.buffer));
			tlasPtr->deviceData = tlas.deviceData;
		}

		else {

			//Copy buffers

			if (tlas.base.isMotionBlurExt) {
				tlasPtr->cpuInstancesMotion = (ListTLASInstanceMotion) { 0 };
				gotoIfError(clean, ListTLASInstanceMotion_createCopyx(tlas.cpuInstancesMotion, &tlasPtr->cpuInstancesMotion));
			}

			else {
				tlasPtr->cpuInstancesStatic = (ListTLASInstanceStatic) { 0 };
				gotoIfError(clean, ListTLASInstanceStatic_createCopyx(tlas.cpuInstancesStatic, &tlasPtr->cpuInstancesStatic));
			}

			//Add refs to BLASes

			U64 length = tlas.cpuInstancesMotion.length;		//instancesMotion and instancesStatic are at the same loc

			Bool invalidData = false;

			for (U64 i = 0; i < length; ++i) {

				TLASInstanceData *dat = NULL;
				TLAS_getInstanceDataCpuInternal(&tlas, i, &dat);

				if(!dat->blasCpu)
					continue;

				if(BLASRef_inc(dat->blasCpu).genericError)
					invalidData = true;

				if(invalidData) {

					for (U64 j = 0; j < length; ++j) {

						//Ensure we don't dec refs that don't belong to us yet

						if(j < i)
							BLASRef_dec(&dat->blasCpu);

						else dat->blasCpu = NULL;
					}

					break;
				}
			}

			if(invalidData)
				return Error_invalidOperation(
					15, "GraphicsDeviceRef_createTLAS() One of the BLASes couldn't be found or couldn't be increased"
				);
		}
	}

	gotoIfError(clean, GraphicsDeviceRef_inc(dev));
	tlasPtr->base.device = dev;

	gotoIfError(clean, CharString_createCopyx(name, &tlasPtr->base.name));

	//Push for the graphics impl to process next submit,
	//If it's GPU generated, the user is expected to manually call buildTLASExt to ensure it's enqueued at the right time

	if (!(tlas.base.flags & ERTASBuildFlags_DisableAutomaticUpdate)) {

		acq0 = Lock_lock(&device->lock, U64_MAX);

		if(acq0 < ELockAcquire_Success)
			gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_createTLAS()::dev couldn't be locked"));

		gotoIfError(clean, ListWeakRefPtr_pushBackx(&device->pendingTlases, *tlasRef));

		if(acq0 == ELockAcquire_Acquired) {
			Lock_unlock(&device->lock);
			acq0 = ELockAcquire_Invalid;
		}
	}

	//Reserve TLAS in array

	acq1 = Lock_lock(&device->descriptorLock, U64_MAX);

	if(acq1 < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(
			0, "GraphicsDeviceRef_createTLAS() couldn't acquire descriptor lock"
		));

	//Create images

	tlasPtr->handle = GraphicsDeviceRef_allocateDescriptor(dev, EDescriptorType_TLASExt);

	if(tlasPtr->handle == U32_MAX)
		gotoIfError(clean, Error_outOfMemory(0, "GraphicsDeviceRef_createTLAS() couldn't allocate AS descriptor"));

clean:

	if(acq1 == ELockAcquire_Acquired)
		Lock_unlock(&device->descriptorLock);

	if(acq0 == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	if(err.genericError)
		TLASRef_dec(tlasRef);

	return err;
}

Error GraphicsDeviceRef_createTLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	TLASRef *parent,					//If specified, indicates refit
	ListTLASInstanceStatic instances,
	CharString name,
	TLASRef **tlas
) {

	TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.parent = parent
		},
		.cpuInstancesStatic = instances
	};

	return GraphicsDeviceRef_createTLAS(dev, tlasInfo, name, tlas);
}

Error GraphicsDeviceRef_createTLASMotionExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	TLASRef *parent,					//If specified, indicates refit
	ListTLASInstanceMotion instances,
	CharString name,
	TLASRef **tlas
) {

	TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.parent = parent
		},
		.cpuInstancesMotion = instances
	};

	return GraphicsDeviceRef_createTLAS(dev, tlasInfo, name, tlas);
}

Error GraphicsDeviceRef_createTLASDeviceExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	Bool isMotionBlurExt,				//Requires extension
	TLASRef *parent,					//If specified, indicates refit
	DeviceData instancesDevice,			//Instances on the GPU, should be sized correctly
	CharString name,
	TLASRef **tlas
) {

	TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.isMotionBlurExt = isMotionBlurExt,
			.parent = parent
		},
		.useDeviceMemory = true,
		.deviceData = instancesDevice
	};

	return GraphicsDeviceRef_createTLAS(dev, tlasInfo, name, tlas);
}

//Creating TLAS from cache

//Error GraphicsDeviceRef_createTLASFromCacheExt(GraphicsDeviceRef *dev, Buffer cache, CharString name, TLASRef **tlas) {
//
//	TLAS tlasInfo = (TLAS) {
//		.base = (RTAS) { .asConstructionType = (U8) ETLASConstructionType_Serialized, },
//		.cpuData = cache
//	};
//
//	return GraphicsDeviceRef_createTLAS(dev, tlasInfo, name, tlas);
//}
