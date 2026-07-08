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

//graphics/generic/tlas.c

#include "types/container/list_impl.h"
#include "graphics/generic/interface.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "platforms/logx.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/descriptor_table.h"
#include "types/container/buffer.h"
#include "formats/oiSH/sh_registers.h"
#include "types/base/constants.h"

TLASTransformSRT TLASTransformSRT_create(F32x4 scale, F32x4 pivot, F32x4 translate, QuatF32 quat, F32x4 shearing) {
	TLASTransformSRT srt = TLASTransformSRT_createSimple(scale, translate, quat);
	TLASTransformSRT_setPivot(&srt, pivot);
	TLASTransformSRT_setShearing(&srt, shearing);
	return srt;

}

TLASTransformSRT TLASTransformSRT_createSimple(F32x4 scale, F32x4 translate, QuatF32 quat) {
	TLASTransformSRT srt = (TLASTransformSRT) { 0 };
	TLASTransformSRT_setScale(&srt, scale);
	TLASTransformSRT_setTranslate(&srt, translate);
	TLASTransformSRT_setQuat(&srt, quat);
	return srt;
}

F32x4 TLASTransformSRT_getScale(TLASTransformSRT srt) {
	return F32x4_create3(srt.sx, srt.sy, srt.sz);
}

Bool TLASTransformSRT_setScale(TLASTransformSRT *srt, F32x4 value) {

	if(!srt)
		return false;

	srt->sx = F32x4_x(value);
	srt->sy = F32x4_y(value);
	srt->sz = F32x4_z(value);
	return true;
}

F32x4 TLASTransformSRT_getPivot(TLASTransformSRT srt) {
	return F32x4_create3(srt.pvx, srt.pvy, srt.pvz);
}

Bool TLASTransformSRT_setPivot(TLASTransformSRT *srt, F32x4 value) {

	if(!srt)
		return false;

	srt->pvx = F32x4_x(value);
	srt->pvy = F32x4_y(value);
	srt->pvz = F32x4_z(value);
	return true;
}

F32x4 TLASTransformSRT_getTranslate(TLASTransformSRT srt) {
	return F32x4_create3(srt.tx, srt.ty, srt.tz);
}

Bool TLASTransformSRT_setTranslate(TLASTransformSRT *srt, F32x4 value) {

	if(!srt)
		return false;

	srt->tx = F32x4_x(value);
	srt->ty = F32x4_y(value);
	srt->tz = F32x4_z(value);
	return true;
}

QuatF32 TLASTransformSRT_getQuat(TLASTransformSRT srt) {
	return QuatF32_create(srt.q0, srt.q1, srt.q2, srt.q3);
}

Bool TLASTransformSRT_setQuat(TLASTransformSRT *srt, QuatF32 value) {

	if(!srt)
		return false;

	srt->q0 = QuatF32_x(value);
	srt->q1 = QuatF32_y(value);
	srt->q2 = QuatF32_z(value);
	srt->q3 = QuatF32_w(value);
	return true;
}

F32x4 TLASTransformSRT_getShearing(TLASTransformSRT srt) {
	return F32x4_create3(srt.a, srt.b, srt.c);
}

Bool TLASTransformSRT_setShearing(TLASTransformSRT *srt, F32x4 value) {

	if(!srt)
		return false;

	srt->a = F32x4_x(value);
	srt->b = F32x4_y(value);
	srt->c = F32x4_z(value);
	return true;
}

TListImpl(TLASInstanceMotion);
TListImpl(TLASInstanceStatic);

TLASInstanceData *TLASInstanceMotion_getDataInternal(TLASInstanceMotion *mot) {
	switch (mot->type) {
		default:                            return &mot->staticInst.data;
		case ETLASInstanceType_Matrix:        return &mot->matrixInst.data;
		case ETLASInstanceType_SRT:            return &mot->srtInst.data;
	}
}

TLASInstanceData TLASInstanceMotion_getData(TLASInstanceMotion mot) {
	return *TLASInstanceMotion_getDataInternal(&mot);
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
	if(!TLAS_getInstanceDataCpuInternal(tlas, i, &data))
		return false;

	*result = *data;
	return true;
}

void TLAS_free(TLAS *tlas, const Allocator *alloc) {

	(void)alloc;

	SpinLock_lock(&tlas->base.lock, U64_MAX);

	TLAS_freeExt(tlas);
	//Log_debugLnx("Destroy: %s (%p)", tlas->base.name.ptr, tlas);
	CharString_free(&tlas->base.name, alloc);

	RefPtr_dec(&tlas->base.asBuffer);
	RefPtr_dec(&tlas->base.tempScratchBuffer);
	RefPtr_dec(&tlas->tempInstanceBuffer);

	if(tlas->base.asConstructionType == ETLASConstructionType_Serialized)
		Buffer_free(&tlas->cpuData, alloc);

	else if (tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		if(tlas->useDeviceMemory)
			RefPtr_dec(&tlas->deviceData.buffer);

		else {

			for(U64 i = 0; i < tlas->cpuInstancesStatic.length; ++i) {
				TLASInstanceData *result = NULL;
				TLAS_getInstanceDataCpuInternal(tlas, i, &result);
				RefPtr_dec(&result->blasCpu);
			}

			if(tlas->base.isMotionBlurExt)
				ListTLASInstanceMotion_free(&tlas->cpuInstancesMotion, alloc);

			else ListTLASInstanceStatic_free(&tlas->cpuInstancesStatic, alloc);
		}
	}

	if(tlas->bindlessDescriptorTable) {
		GraphicsDeviceRef_freeDescriptorBindless(tlas->base.device, tlas->bindlessDescriptorTable, tlas->handle, NULL);
		RefPtr_dec(&tlas->bindlessDescriptorTable);
	}

	RefPtr_dec(&tlas->base.device);
}

Bool GraphicsDeviceRef_createTLAS(
	GraphicsDeviceRef *dev,
	const TLAS *tlas,
	DescriptorTableRef *bindlessDescriptorTable,
	CharString name,
	TLASRef **tlasRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	//Validate

	if(!dev || dev->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createTLAS()::dev is required"));

	if(!tlas)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_createTLAS()::tlas is required"));

	if(bindlessDescriptorTable && tlas->disallowBindlessDescriptor)
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_createTLAS() bindlessDescriptorTable is set, but disallowed"));

	if(bindlessDescriptorTable && bindlessDescriptorTable->refPtrType->typeId != (ETypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(
			0,
			"GraphicsDeviceRef_createTLAS()::bindlessDescriptorTable should be valid if non NULL"
		));

	if (!tlas->disallowBindlessDescriptor && !bindlessDescriptorTable)
		bindlessDescriptorTable = GraphicsDeviceRef_ptr(dev)->defaultDescriptorTable;

	if(!tlasRef)
		retError(clean, Error_nullPointer(3, "GraphicsDeviceRef_createTLAS()::tlasRef is required"));

	if(*tlasRef)
		retError(clean, Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_createTLAS()::*tlasRef not NULL, indicates memleak"
		));

	if(tlas->base.parent && tlas->base.parent->refPtrType->typeId != (ETypeId) EGraphicsTypeId_TLASExt)
		retError(clean, Error_invalidOperation(1, "GraphicsDeviceRef_createTLAS()::parent is invalid"));

	if(tlas->base.parent && TLASRef_ptr(tlas->base.parent)->base.device != dev)
		retError(clean, Error_invalidOperation(
			1, "GraphicsDeviceRef_createTLAS()::parent and TLAS device need to share device"
		));

	if(!tlas->base.parent && (tlas->base.flags & ERTASBuildFlags_IsUpdate))
		retError(clean, Error_invalidOperation(
			7, "GraphicsDeviceRef_createTLAS()::parent is required if IsUpdate is present"
		));

	const Bool isUpdate = tlas->base.parent || (tlas->base.flags & ERTASBuildFlags_IsUpdate);

	if(!(tlas->base.flags & ERTASBuildFlags_AllowUpdate) && isUpdate)
		retError(clean, Error_invalidOperation(
			8, "GraphicsDeviceRef_createTLAS() is update is not possible if AllowUpdate is false"
		));

	EGraphicsFeatures feat = GraphicsDeviceRef_ptr(dev)->info.capabilities.features;

	if(!(feat & EGraphicsFeatures_Raytracing))
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDeviceRef_createTLAS() is unsupported without raytracing support"
		));

	if(tlas->base.isMotionBlurExt && !(feat & EGraphicsFeatures_RayMotionBlur))
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDeviceRef_createTLAS() uses motion blur, but it's unsupported"
		));

	//RTAS_validateDeviceBuffer may normalize len, so validate a local copy;
	//it's committed to the new TLAS below

	DeviceData deviceData = (DeviceData) { 0 };

	//Validate TLAS

	if(tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		if (tlas->useDeviceMemory) {

			deviceData = tlas->deviceData;
			gotoIfError3(clean, RTAS_validateDeviceBuffer(&deviceData, e_rr));

			U64 stride = tlas->base.isMotionBlurExt ? sizeof(TLASInstanceMotion) : sizeof(TLASInstanceStatic);

			if(deviceData.len < stride || deviceData.len % stride)
				retError(clean, Error_invalidOperation(9, "GraphicsDeviceRef_createTLAS() invalid AS buffer size"));
		}

		else {

			U64 length = tlas->cpuInstancesMotion.length;        //instancesMotion and instancesStatic are at the same loc

			if(!length)
				retError(clean, Error_invalidOperation(10, "GraphicsDeviceRef_createTLAS() is missing instance list"));

			for (U64 i = 0; i < length; ++i) {

				TLASInstanceData dat = (TLASInstanceData) { 0 };
				if(!TLAS_getInstanceDataCpu(tlas, i, &dat))
					retError(clean, Error_invalidOperation(
						11, "GraphicsDeviceRef_createTLAS() can't get instance data cpu"
					));

				if(dat.blasCpu) {

					if(dat.blasCpu->refPtrType->typeId != (ETypeId) EGraphicsTypeId_BLASExt)
						retError(clean, Error_invalidOperation(12, "GraphicsDeviceRef_createTLAS() invalid BLAS type"));

					if(BLASRef_ptr(dat.blasCpu)->base.device != dev)
						retError(clean, Error_invalidOperation(
							13, "GraphicsDeviceRef_createTLAS() BLAS device is incompatible"
						));
				}

				if(!(dat.instanceId24_mask8 >> 24))
					retError(clean, Error_invalidOperation(
						14,
						"GraphicsDeviceRef_createTLAS() BLAS mask is 0, this might be unintended. "
						"Set blasCpu to NULL instead to explicitly hide the instance."
					));
			}
		}
	}

	//Validate serialized

	else if(!Buffer_length(tlas->cpuData))
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createTLAS()::cpuData should be valid if serialized construction is used"
		));

	//Allocate refPtr

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->tlas, tlasRef, e_rr));

	//Fill ptr

	TLAS *tlasPtr = TLASRef_ptr(*tlasRef);

	if(tlas->base.parent)
		gotoIfError3(clean, RefPtr_inc(tlas->base.parent));

	*tlasPtr = *tlas;
	tlasPtr->base.name = CharString_createNull();

	if(isUpdate)
		tlasPtr->base.flags |= ERTASBuildFlags_IsUpdate;

	if (tlas->base.asConstructionType == ETLASConstructionType_Serialized) {
		tlasPtr->cpuData = Buffer_createNull();
		gotoIfError3(clean, Buffer_createCopy(tlas->cpuData, alloc, &tlasPtr->cpuData, e_rr));
	}
	else {

		if (tlas->useDeviceMemory) {
			tlasPtr->deviceData = (DeviceData) { 0 };
			gotoIfError3(clean, RefPtr_inc(deviceData.buffer));
			tlasPtr->deviceData = deviceData;
		}

		else {

			//Copy buffers

			if (tlas->base.isMotionBlurExt) {
				tlasPtr->cpuInstancesMotion = (ListTLASInstanceMotion) { 0 };
				gotoIfError3(clean, ListTLASInstanceMotion_createCopy(
					tlas->cpuInstancesMotion,
					alloc,
					&tlasPtr->cpuInstancesMotion,
					e_rr
				));
			}

			else {
				tlasPtr->cpuInstancesStatic = (ListTLASInstanceStatic) { 0 };
				gotoIfError3(clean, ListTLASInstanceStatic_createCopy(
					tlas->cpuInstancesStatic,
					alloc,
					&tlasPtr->cpuInstancesStatic,
					e_rr
				));
			}

			//Add refs to BLASes

			U64 length = tlas->cpuInstancesMotion.length;        //instancesMotion and instancesStatic are at the same loc

			Bool invalidData = false;

			for (U64 i = 0; i < length; ++i) {

				TLASInstanceData *dat = NULL;
				TLAS_getInstanceDataCpuInternal(tlasPtr, i, &dat);

				if(!dat->blasCpu)
					continue;

				if(!RefPtr_inc(dat->blasCpu))
					invalidData = true;

				if(invalidData) {

					for (U64 j = 0; j < length; ++j) {

						//Ensure we don't dec refs that don't belong to us yet

						if(j < i)
							RefPtr_dec(&dat->blasCpu);

						else dat->blasCpu = NULL;
					}

					break;
				}
			}

			if(invalidData)
				retError(clean, Error_invalidOperation(
					15,
					"GraphicsDeviceRef_createTLAS() One of the BLASes couldn't be found or couldn't be increased"));
		}
	}

	gotoIfError3(clean, RefPtr_inc(dev));
	tlasPtr->base.device = dev;

	if(bindlessDescriptorTable) {
		gotoIfError3(clean, RefPtr_inc(bindlessDescriptorTable));
		tlasPtr->bindlessDescriptorTable = bindlessDescriptorTable;
	}

	gotoIfError3(clean, CharString_createCopy(name, alloc, &tlasPtr->base.name, e_rr));
	//Log_debugLnx("Create: %s", tlasPtr->base.name.ptr);

	gotoIfError3(clean, TLAS_initExt(tlasPtr, e_rr));

	if(bindlessDescriptorTable && !GraphicsDeviceRef_allocateDescriptorBindless(
		dev,
		bindlessDescriptorTable,
		ESHRegisterType_AccelerationStructure,
		0,
		false,
		Descriptor_tlas(*tlasRef),
		&tlasPtr->handle,
		e_rr
	)) {
		s_uccess = false;
		goto clean;
	}

clean:

	if(!s_uccess)
		RefPtr_dec(tlasRef);

	return s_uccess;
}

Bool GraphicsDeviceRef_createTLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	TLASRef *parent,                    //If specified, indicates refit
	ListTLASInstanceStatic instances,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	CharString name,
	TLASRef **tlas,
	Error *e_rr
) {

	const TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.parent = parent
		},
		.cpuInstancesStatic = instances,
		.disallowBindlessDescriptor = disallowBindlessDescriptor
	};

	return GraphicsDeviceRef_createTLAS(dev, &tlasInfo, bindlessDescriptorTable, name, tlas, e_rr);
}

Bool GraphicsDeviceRef_createTLASMotionExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	TLASRef *parent,
	ListTLASInstanceMotion instances,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	CharString name,
	TLASRef **tlas,
	Error *e_rr
) {

	const TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.parent = parent
		},
		.cpuInstancesMotion = instances,
		.disallowBindlessDescriptor = disallowBindlessDescriptor
	};

	return GraphicsDeviceRef_createTLAS(dev, &tlasInfo, bindlessDescriptorTable, name, tlas, e_rr);
}

Bool GraphicsDeviceRef_createTLASDeviceExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	Bool isMotionBlurExt,
	TLASRef *parent,
	DeviceData instancesDevice,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	CharString name,
	TLASRef **tlas,
	Error *e_rr
) {

	const TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.isMotionBlurExt = isMotionBlurExt,
			.parent = parent
		},
		.useDeviceMemory = true,
		.disallowBindlessDescriptor = disallowBindlessDescriptor,
		.deviceData = instancesDevice
	};

	return GraphicsDeviceRef_createTLAS(dev, &tlasInfo, bindlessDescriptorTable, name, tlas, e_rr);
}

//Creating TLAS from cache

//Error GraphicsDeviceRef_createTLASFromCacheExt(
// GraphicsDeviceRef *dev, Buffer cache, Bool disallowBindlessDescriptor, CharString name, TLASRef **tlas
//) {
//
//    TLAS tlasInfo = (TLAS) {
//        .base = (RTAS) { .asConstructionType = (U8) ETLASConstructionType_Serialized, },
//        .cpuData = cache,
//        .disallowBindlessDescriptor = disallowBindlessDescriptor
//    };
//
//    return GraphicsDeviceRef_createTLAS(dev, tlasInfo, name, tlas);
//}
