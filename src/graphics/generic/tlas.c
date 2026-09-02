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
#include "graphics/generic/device.h"
#include "graphics/generic/tlas.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/bindless_descriptor.h"
#include "graphics/generic/descriptor_table.h"
#include "formats/oiSH/sh_registers.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/base/constants.h"

TListImpl(TLASInstance);

Bool TLAS_getInstanceDataCpuInternal(const TLAS *tlas, U64 i, TLASInstanceData **result) {

	if(
		!result ||
		TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory) ||
		tlas->base.asConstructionType != ETLASConstructionType_Instances ||
		i >= tlas->cpuInstances.length
	)
		return false;

	*result = &tlas->cpuInstances.ptrNonConst[i].data;
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

//Swaps in a new set of instances so the next recorded update refits rather than rebuilds.
//The instance array itself is only uploaded again at build time, which keeps a batch of transform changes to
// a single upload instead of one per call.

Bool TLASRef_setInstancesExt(TLASRef *tlasRef, const ListTLASInstance *instances, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	U64 referenced = 0;
	TLAS *tlas = NULL;

	if(!tlasRef || tlasRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_TLASExt)
		retError(clean, Error_nullPointer(0, "TLASRef_setInstancesExt()::tlas is required"));

	if(!instances)
		retError(clean, Error_nullPointer(1, "TLASRef_setInstancesExt()::instances is required"));

	tlas = TLASRef_ptr(tlasRef);

	if(!(tlas->base.flags & ERTASBuildFlags_AllowUpdate))
		retError(clean, Error_invalidOperation(
			0, "TLASRef_setInstancesExt() requires a TLAS that was built with AllowUpdate"
		));

	if(
		tlas->base.asConstructionType != ETLASConstructionType_Instances ||
		TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory)
	)
		retError(clean, Error_invalidOperation(
			0, "TLASRef_setInstancesExt() is only valid on a TLAS built from CPU instances"
		));

	//An update refits the structure that is already there instead of sizing a new one, so both APIs want the
	// instance count the TLAS was built with.

	if(instances->length != tlas->cpuInstances.length)
		retError(clean, Error_invalidParameter(
			1, 0, "TLASRef_setInstancesExt()::instances needs the length the TLAS was built with"
		));

	if((acq = SpinLock_lock(&tlas->base.lock, U64_MAX)) < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "TLASRef_setInstancesExt() couldn't acquire the TLAS"));

	//Referenced up front, so an invalid BLAS halfway down the list leaves the TLAS exactly as it was rather
	// than half swapped.

	for (; referenced < instances->length; ++referenced) {

		BLASRef *blas = instances->ptr[referenced].data.blasCpu;

		if(!blas)
			continue;

		if(blas->refPtrType->typeId != (TypeId) EGraphicsTypeId_BLASExt || !RefPtr_inc(blas))
			retError(clean, Error_invalidParameter(
				1, 0, "TLASRef_setInstancesExt()::instances[i].data.blasCpu is invalid"
			));
	}

	for (U64 i = 0; i < tlas->cpuInstances.length; ++i) {
		TLASInstanceData *old = NULL;
		TLAS_getInstanceDataCpuInternal(tlas, i, &old);
		RefPtr_dec(&old->blasCpu);
	}

	for (U64 i = 0; i < instances->length; ++i)
		tlas->cpuInstances.ptrNonConst[i] = instances->ptr[i];

	referenced = 0;        //Handed over to the TLAS
	tlas->base.flagsExt |= (U8) ETLASFlag_InstancesDirty;

clean:

	//Whatever was referenced but never handed over has to go back, or a rejected call leaks every BLAS it read.

	for (U64 i = 0; i < referenced; ++i) {
		BLASRef *blas = instances->ptr[i].data.blasCpu;
		RefPtr_dec(&blas);
	}

	if(tlas && acq == ELockAcquire_Acquired)
		SpinLock_unlock(&tlas->base.lock);

	return s_uccess;
}

void TLAS_free(void *tlasGeneric, const Allocator *alloc) {

	TLAS *tlas = (TLAS*) tlasGeneric;

	(void)alloc;

	//Out of the device's registry BEFORE anything else: an entry left behind is a dangling pointer the
	// next compaction would read. Under the device lock, and taken before this TLAS's own lock rather than
	// inside it, since the marking path holds the device lock while touching TLASes.

	if(tlas->base.device) {

		GraphicsDevice *device = GraphicsDeviceRef_ptr(tlas->base.device);
		const ELockAcquire acq = SpinLock_lock(&device->lock, U64_MAX);

		if(acq >= ELockAcquire_Success) {

			RefPtr *self = ((RefPtr*) tlas) - 1;

			for (U64 i = 0; i < device->liveTlases.length; ++i)
				if (device->liveTlases.ptr[i] == self) {
					ListRefPtr_popLocation(&device->liveTlases, i, NULL, NULL);
					break;
				}

			if(acq == ELockAcquire_Acquired)
				SpinLock_unlock(&device->lock);
		}
	}

	SpinLock_lock(&tlas->base.lock, U64_MAX);

	TLAS_freeExt(tlas);
	CharString_free(&tlas->base.name, alloc);

	RefPtr_dec(&tlas->base.asBuffer);
	RefPtr_dec(&tlas->base.tempScratchBuffer);
	RefPtr_dec(&tlas->tempInstanceBuffer);

	if(tlas->base.asConstructionType == ETLASConstructionType_Serialized)
		Buffer_free(&tlas->cpuData, alloc);

	else if (tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		if(TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory))
			RefPtr_dec(&tlas->deviceData.buffer);

		else {

			for(U64 i = 0; i < tlas->cpuInstances.length; ++i) {
				TLASInstanceData *result = NULL;
				TLAS_getInstanceDataCpuInternal(tlas, i, &result);
				RefPtr_dec(&result->blasCpu);
			}

			ListTLASInstance_free(&tlas->cpuInstances, alloc);
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
	const CharString *name,
	TLASRef **tlasRef,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);
	Bool allocated = false;

	//See the matching fields in TLAS; only the CPU instance path below can prove anything.

	ETLASFlag dataAccessFlags = ETLASFlag_None;

	//Set when an adopted structure has a compaction recorded but not yet executed.

	Bool pendingCompaction = false;

	//Validate

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createTLAS()::dev is required"));

	if(!tlas)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_createTLAS()::tlas is required"));

	if(bindlessDescriptorTable && TLAS_hasFlag(tlas, ETLASFlag_DisallowBindlessDescriptor))
		retError(clean, Error_invalidState(0, "GraphicsDeviceRef_createTLAS() bindlessDescriptorTable is set, but disallowed"));

	if(bindlessDescriptorTable && bindlessDescriptorTable->refPtrType->typeId != (TypeId) EGraphicsTypeId_DescriptorTable)
		retError(clean, Error_nullPointer(
			0,
			"GraphicsDeviceRef_createTLAS()::bindlessDescriptorTable should be valid if non NULL"
		));

	if (!TLAS_hasFlag(tlas, ETLASFlag_DisallowBindlessDescriptor) && !bindlessDescriptorTable)
		bindlessDescriptorTable = GraphicsDeviceRef_ptr(dev)->defaultDescriptorTable;

	if(!tlasRef)
		retError(clean, Error_nullPointer(3, "GraphicsDeviceRef_createTLAS()::tlasRef is required"));

	if(*tlasRef)
		retError(clean, Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_createTLAS()::*tlasRef not NULL, indicates memleak"
		));

	EGraphicsFeatures feat = GraphicsDeviceRef_ptr(dev)->info.capabilities.features;

	if(!(feat & EGraphicsFeatures_Raytracing))
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDeviceRef_createTLAS() is unsupported without raytracing support"
		));

	//RTAS_validateDeviceBuffer may normalize len, so validate a local copy; it's committed to the new TLAS below

	DeviceData deviceData = (DeviceData) { 0 };

	//Validate TLAS

	if(tlas->base.asConstructionType == ETLASConstructionType_Instances) {

		if (TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory)) {

			deviceData = tlas->deviceData;
			gotoIfError3(clean, RTAS_validateDeviceBuffer(&deviceData, e_rr));

			U64 stride = sizeof(TLASInstance);

			if(deviceData.len < stride || deviceData.len % stride)
				retError(clean, Error_invalidOperation(9, "GraphicsDeviceRef_createTLAS() invalid AS buffer size"));
		}

		else {

			U64 length = tlas->cpuInstances.length;        //instancesMotion and instancesStatic are at the same loc

			if(!length)
				retError(clean, Error_invalidOperation(10, "GraphicsDeviceRef_createTLAS() is missing instance list"));

			dataAccessFlags = ETLASFlag_BlasDataAccessKnown | ETLASFlag_BlasDataAccessAll;

			for (U64 i = 0; i < length; ++i) {

				TLASInstanceData dat = (TLASInstanceData) { 0 };
				if(!TLAS_getInstanceDataCpu(tlas, i, &dat))
					retError(clean, Error_invalidOperation(
						11, "GraphicsDeviceRef_createTLAS() can't get instance data cpu"
					));

				if(dat.blasCpu) {

					if(dat.blasCpu->refPtrType->typeId != (TypeId) EGraphicsTypeId_BLASExt)
						retError(clean, Error_invalidOperation(12, "GraphicsDeviceRef_createTLAS() invalid BLAS type"));

					if(BLASRef_ptr(dat.blasCpu)->base.device != dev)
						retError(clean, Error_invalidOperation(
							13, "GraphicsDeviceRef_createTLAS() BLAS device is incompatible"
						));

					if(!(BLASRef_ptr(dat.blasCpu)->base.flags & ERTASBuildFlags_AllowDataAccessExt))
						dataAccessFlags &=~ ETLASFlag_BlasDataAccessAll;

					//This structure has a compaction recorded but not yet executed, so the address resolved
					// here is the one that copy is about to replace. The compaction already walked the live
					// TLASes and this one did not exist yet, so it marks itself.

					if(BLASRef_ptr(dat.blasCpu)->base.pendingCompactBuffer)
						pendingCompaction = true;
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
	allocated = true;

	//Fill ptr

	TLAS *tlasPtr = TLASRef_ptr(*tlasRef);

	*tlasPtr = *tlas;
	tlasPtr->base.name = CharString_createNull();

	//The input struct is caller-constructed, so the cached validation bits are set here rather than trusted.

	tlasPtr->base.flagsExt |= (U8) dataAccessFlags;

	//Set as soon as the object exists rather than once it is fully built.
	//TLAS_freeExt reads base.device to find the backend, so a rejection between here and there used to free
	// a TLAS whose device was still NULL and segfault instead of reporting the error.

	gotoIfError3(clean, RefPtr_inc(dev));
	tlasPtr->base.device = dev;

	//Registered so a compaction can find this TLAS and tell it its addresses moved. No reference is taken;
	// TLAS_free removes the entry.

	{
		GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
		const ELockAcquire regAcq = SpinLock_lock(&device->lock, U64_MAX);

		if(regAcq < ELockAcquire_Success)
			retError(clean, Error_invalidState(
				15, "GraphicsDeviceRef_createTLAS() couldn't acquire device lock to register the TLAS"
			));

		const Bool registered = ListRefPtr_pushBack(
			&device->liveTlases, (RefPtr*) *tlasRef, GraphicsDeviceRef_getAlloc(dev), NULL
		);

		if(regAcq == ELockAcquire_Acquired)
			SpinLock_unlock(&device->lock);

		if(!registered)
			retError(clean, Error_outOfMemory(0, "GraphicsDeviceRef_createTLAS() couldn't register the TLAS"));
	}

	//The submit after that copy refuses until this TLAS is updated.

	if(pendingCompaction) {
		tlasPtr->base.flagsExt |= (U8) (ETLASFlag_AddressesStale | ETLASFlag_InstancesDirty);
		tlasPtr->staleAtSubmitId = GraphicsDeviceRef_ptr(dev)->submitId;
	}

	if (tlas->base.asConstructionType == ETLASConstructionType_Serialized) {
		tlasPtr->cpuData = Buffer_createNull();
		gotoIfError3(clean, Buffer_createCopy(tlas->cpuData, alloc, &tlasPtr->cpuData, e_rr));
	}

	else {

		if (TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory)) {
			tlasPtr->deviceData = (DeviceData) { 0 };
			gotoIfError3(clean, RefPtr_inc(deviceData.buffer));
			tlasPtr->deviceData = deviceData;
		}

		else {

			//Copy buffers

			{
				tlasPtr->cpuInstances = (ListTLASInstance) { 0 };
				gotoIfError3(clean, ListTLASInstance_createCopy(
					tlas->cpuInstances,
					alloc,
					&tlasPtr->cpuInstances,
					e_rr
				));
			}

			//Add refs to BLASes

			U64 length = tlas->cpuInstances.length;        //instancesMotion and instancesStatic are at the same loc

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
					"GraphicsDeviceRef_createTLAS() One of the BLASes couldn't be found or couldn't be increased"
				));
		}
	}

	if(bindlessDescriptorTable) {
		gotoIfError3(clean, RefPtr_inc(bindlessDescriptorTable));
		tlasPtr->bindlessDescriptorTable = bindlessDescriptorTable;
	}

	if(name)
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &tlasPtr->base.name, e_rr));

	gotoIfError3(clean, TLAS_initExt(tlasPtr, e_rr));

	Descriptor tlasDesc = Descriptor_tlas(*tlasRef);

	if(bindlessDescriptorTable && !GraphicsDeviceRef_allocateDescriptorBindless(
		dev,
		bindlessDescriptorTable,
		ESHRegisterType_AccelerationStructure,
		0,
		false,
		&tlasDesc,
		&tlasPtr->handle,
		e_rr
	)) {
		s_uccess = false;
		goto clean;
	}

clean:

	if(!s_uccess && allocated)
		RefPtr_dec(tlasRef);

	return s_uccess;
}

Bool GraphicsDeviceRef_createTLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	const ListTLASInstance *instances,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	TLASRef **tlas,
	Error *e_rr
) {

	TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.flagsExt = (U8) (disallowBindlessDescriptor ? ETLASFlag_DisallowBindlessDescriptor : ETLASFlag_None)
		}
	};

	if(instances)
		tlasInfo.cpuInstances = *instances;

	return GraphicsDeviceRef_createTLAS(dev, &tlasInfo, bindlessDescriptorTable, name, tlas, e_rr);
}

Bool GraphicsDeviceRef_createTLASDeviceExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	const DeviceData *instancesDevice,
	Bool disallowBindlessDescriptor,
	DescriptorTableRef *bindlessDescriptorTable,
	const CharString *name,
	TLASRef **tlas,
	Error *e_rr
) {

	TLAS tlasInfo = (TLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) ETLASConstructionType_Instances,
			.flags = (U8) buildFlags,
			.flagsExt = (U8) (
				ETLASFlag_UseDeviceMemory |
				(disallowBindlessDescriptor ? ETLASFlag_DisallowBindlessDescriptor : ETLASFlag_None)
			)
		}
	};

	if(instancesDevice)
		tlasInfo.deviceData = *instancesDevice;

	return GraphicsDeviceRef_createTLAS(dev, &tlasInfo, bindlessDescriptorTable, name, tlas, e_rr);
}

//Creating TLAS from cache

//Bool GraphicsDeviceRef_createTLASFromCacheExt(
// GraphicsDeviceRef *dev, Buffer cache, Bool disallowBindlessDescriptor, CharString name, TLASRef **tlas, Error *e_rr
//) {
//
//    TLAS tlasInfo = (TLAS) {
//        .base = (RTAS) { .asConstructionType = (U8) ETLASConstructionType_Serialized, },
//        .cpuData = cache,
//        .disallowBindlessDescriptor = disallowBindlessDescriptor
//    };
//
//    return GraphicsDeviceRef_createTLAS(dev, tlasInfo, name, tlas, e_rr);
//}
