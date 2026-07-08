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

//graphics/generic/blas.c

#include "types/container/list.h"
#include "graphics/generic/interface.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "platforms/logx.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "types/container/buffer.h"
#include "types/base/constants.h"

void BLAS_free(BLAS *blas, const Allocator *alloc) {

	(void)alloc;

	SpinLock_lock(&blas->base.lock, U64_MAX);

	BLAS_freeExt(blas);
	//Log_debugLnx("Destroy: %s (%p)", blas->base.name.ptr, blas);
	CharString_free(&blas->base.name, alloc);

	RefPtr_dec(&blas->base.asBuffer);
	RefPtr_dec(&blas->base.tempScratchBuffer);

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		Buffer_free(&blas->cpuData, alloc);

	else if (blas->base.asConstructionType == EBLASConstructionType_Procedural)
		RefPtr_dec(&blas->aabbBuffer.buffer);

	else {
		RefPtr_dec(&blas->indexBuffer.buffer);
		RefPtr_dec(&blas->positionBuffer.buffer);
	}

	RefPtr_dec(&blas->base.device);
}

Bool GraphicsDeviceRef_createBLAS(GraphicsDeviceRef *dev, const BLAS *blas, CharString name, BLASRef **blasRef, Error *e_rr) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	//Validate

	if(!dev || dev->refPtrType->typeId != (ETypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createBLAS()::dev is required"));

	if(!blas)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_createBLAS()::blas is required"));

	if(!blasRef)
		retError(clean, Error_nullPointer(3, "GraphicsDeviceRef_createBLAS()::blasRef is required"));

	if(*blasRef)
		retError(clean, Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_createBLAS()::*blasRef not NULL, indicates memleak"
		));

	if(blas->base.parent && blas->base.parent->refPtrType->typeId != (ETypeId) EGraphicsTypeId_BLASExt)
		retError(clean, Error_invalidOperation(1, "GraphicsDeviceRef_createBLAS()::parent is invalid"));

	if(blas->base.parent && BLASRef_ptr(blas->base.parent)->base.device != dev)
		retError(clean, Error_invalidOperation(
			1, "GraphicsDeviceRef_createBLAS()::parent and BLAS device need to share device"
		));

	if(!blas->base.parent && (blas->base.flags & ERTASBuildFlags_IsUpdate))
		retError(clean, Error_invalidOperation(
			7, "GraphicsDeviceRef_createBLAS()::parent is required if IsUpdate is present"
		));

	const Bool isUpdate = blas->base.parent || (blas->base.flags & ERTASBuildFlags_IsUpdate);

	if(!(blas->base.flags & ERTASBuildFlags_AllowUpdate) && isUpdate)
		retError(clean, Error_invalidOperation(
			7, "GraphicsDeviceRef_createBLAS() is update is not possible if AllowUpdate is false"
		));

	EGraphicsFeatures feat = GraphicsDeviceRef_ptr(dev)->info.capabilities.features;

	if(!(feat & EGraphicsFeatures_Raytracing))
		retError(clean, Error_unsupportedOperation(
			0, "GraphicsDeviceRef_createBLAS() is unsupported without raytracing support"
		));

	if(blas->base.isMotionBlurExt && !(feat & EGraphicsFeatures_RayMotionBlur))
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createBLAS() uses motion blur, but it's unsupported"
		));

	//RTAS_validateDeviceBuffer may normalize len, so validate local copies;
	//they're committed to the new BLAS below

	DeviceData positionBuffer = (DeviceData) { 0 };
	DeviceData indexBuffer = (DeviceData) { 0 };
	DeviceData aabbBuffer = (DeviceData) { 0 };

	//Validate geometry BLAS

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry) {

		positionBuffer = blas->positionBuffer;
		indexBuffer = blas->indexBuffer;

		U16 stride = blas->positionBufferStride;
		ETextureFormatId positionFormat = blas->positionFormatId;
		U16 positionOffset = blas->positionOffset;
		ETextureFormatId indexFormat = blas->indexFormatId;

		if(stride > 2048 || !stride)
			retError(clean, Error_unsupportedOperation(1, "GraphicsDeviceRef_createBLAS()::stride must be >0 and <=2048"));

		U8 reqMultiple = 2;

		if(positionFormat == ETextureFormatId_RGBA32f || positionFormat == ETextureFormatId_RG32f)
			reqMultiple = 4;

		if(stride & (reqMultiple - 1))
			retError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createBLAS()::stride must be multiple of channel byte size (2 or 4)"
			));

		gotoIfError3(clean, RTAS_validateDeviceBuffer(&positionBuffer, e_rr));

		if(indexFormat != ETextureFormatId_Undefined)
			gotoIfError3(clean, RTAS_validateDeviceBuffer(&indexBuffer, e_rr));

		switch (positionFormat) {

			case ETextureFormatId_RGBA16f:
			case ETextureFormatId_RGBA16s:
			case ETextureFormatId_RGBA32f:
			case ETextureFormatId_RG16f:
			case ETextureFormatId_RG16s:
			case ETextureFormatId_RG32f:

				if(positionOffset + ETextureFormat_getSize(ETextureFormatId_unpack[positionFormat], 1, 1, 1) > stride)
					retError(clean, Error_unsupportedOperation(
						1,
						"GraphicsDeviceRef_createBLAS()::positionOffset and/or positionFormat out of bounds (> stride)"
					));

				if(positionBuffer.len < stride || (positionBuffer.len % stride))
					retError(clean, Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::positionBuffer should be multiple of stride"
					));

				if((positionBuffer.len / stride) >> 32)
					retError(clean, Error_outOfBounds(
						0, positionBuffer.len / stride, U32_MAX,
						"GraphicsDeviceRef_createBLAS() vertices out of bounds"
					));

				break;

			default:
				retError(clean, Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS()::positionFormat must be RGBA(16f/32f/16)"
				));
		}

		switch (indexFormat) {

			case ETextureFormatId_Undefined:

				if(indexBuffer.buffer)
					retError(clean, Error_unsupportedOperation(
						1,
						"GraphicsDeviceRef_createBLAS()::indexBuffer should be NULL if indexFormat is Undefined"
					));

				break;

			case ETextureFormatId_R16u:
			case ETextureFormatId_R32u: {

				U8 indexStride = indexFormat == ETextureFormatId_R32u ? 4 : 2;

				if(!indexBuffer.buffer || (indexBuffer.len & (indexStride - 1)))
					retError(clean, Error_unsupportedOperation(
						1,
						"GraphicsDeviceRef_createBLAS()::indexBuffer should be multiple of indexFormat and not NULL"
					));

				break;
			}

			default:
				retError(clean, Error_unsupportedOperation(
					2,
					"GraphicsDeviceRef_createBLAS()::indexFormat must be R32u or R16u"
				));
		}
	}

	//Validate AABBs

	else if (blas->base.asConstructionType == EBLASConstructionType_Procedural) {

		aabbBuffer = blas->aabbBuffer;

		U64 stride = blas->aabbStride;

		if(!stride || stride & 7)
			retError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createBLAS()::stride must be >0 and divisible by 8"
			));

		if((U64)blas->aabbOffset + 24 > stride)
			retError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createBLAS()::aabbOffset out of bounds (> stride)"
			));

		gotoIfError3(clean, RTAS_validateDeviceBuffer(&aabbBuffer, e_rr));

		if(aabbBuffer.len < stride || (aabbBuffer.len % stride))
			retError(clean, Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createBLAS()::aabbBuffer should be multiple of stride"
			));
	}

	//Validate serialized

	else if(!Buffer_length(blas->cpuData))
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createBLAS()::cpuData should be valid if serialized construction is used"
		));

	//Allocate refPtr

	gotoIfError3(clean, RefPtr_create(&GraphicsDeviceRef_getTypes(dev)->blas, blasRef, e_rr));

	//Fill ptr

	BLAS *blasPtr = BLASRef_ptr(*blasRef);

	if(blas->base.parent)
		gotoIfError3(clean, RefPtr_inc(blas->base.parent));

	*blasPtr = *blas;
	blasPtr->base.name = CharString_createNull();

	if(isUpdate)
		blasPtr->base.flags |= ERTASBuildFlags_IsUpdate;

	if (blas->base.asConstructionType == EBLASConstructionType_Serialized) {
		blasPtr->cpuData = Buffer_createNull();
		gotoIfError3(clean, Buffer_createCopy(blas->cpuData, alloc, &blasPtr->cpuData, e_rr));
	}

	else if (blas->base.asConstructionType == EBLASConstructionType_Procedural) {
		blasPtr->aabbBuffer = (DeviceData) { 0 };
		gotoIfError3(clean, RefPtr_inc(aabbBuffer.buffer));
		blasPtr->aabbBuffer = aabbBuffer;
	}

	else {

		blasPtr->indexBuffer = (DeviceData) { 0 };
		blasPtr->positionBuffer = (DeviceData) { 0 };

		gotoIfError3(clean, RefPtr_inc(indexBuffer.buffer));
		blasPtr->indexBuffer = indexBuffer;

		gotoIfError3(clean, RefPtr_inc(positionBuffer.buffer));
		blasPtr->positionBuffer = positionBuffer;
	}

	gotoIfError3(clean, RefPtr_inc(dev));
	blasPtr->base.device = dev;

	gotoIfError3(clean, CharString_createCopy(name, alloc, &blasPtr->base.name, e_rr));
	//Log_debugLnx("Create: %s (%p)", blasPtr->base.name.ptr, blasPtr);
	gotoIfError3(clean, BLAS_initExt(blasPtr, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(blasRef);

	return s_uccess;
}

Bool GraphicsDeviceRef_createBLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,
	U16 positionOffset,
	ETextureFormatId indexFormat,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	DeviceData indexBuffer,
	BLASRef *parent,
	CharString name,
	BLASRef **blas,
	Error *e_rr
) {
	const BLAS blasInfo = (BLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) EBLASConstructionType_Geometry,
			.flags = (U8) buildFlags,
			.flagsExt = (U8) blasFlags,
			.parent = parent
		},
		.positionFormatId =    (U8) positionFormat,
		.indexFormatId = (U8) indexFormat,
		.positionBufferStride = positionBufferStride,
		.positionOffset = positionOffset,
		.indexBuffer = indexBuffer,
		.positionBuffer = positionBuffer
	};

	return GraphicsDeviceRef_createBLAS(dev, &blasInfo, name, blas, e_rr);
}

Bool GraphicsDeviceRef_createBLASUnindexedExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	BLASRef *parent,
	CharString name,
	BLASRef **blas,
	Error *e_rr
) {
	return GraphicsDeviceRef_createBLASExt(
		dev,
		buildFlags,
		blasFlags,
		positionFormat,
		positionOffset,
		ETextureFormatId_Undefined,
		positionBufferStride,
		positionBuffer,
		(DeviceData) { 0 },
		parent,
		name,
		blas
	, e_rr);
}

//Creating BLAS from AABBs

Bool GraphicsDeviceRef_createBLASProceduralExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	U32 aabbStride,
	U32 aabbOffset,
	DeviceData buffer,
	BLASRef *parent,
	CharString name,
	BLASRef **blas,
	Error *e_rr
) {
	const BLAS blasInfo = (BLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) EBLASConstructionType_Procedural,
			.flags = (U8) buildFlags,
			.flagsExt = (U8) blasFlags,
			.parent = parent
		},
		.aabbBuffer = buffer,
		.aabbStride = aabbStride,
		.aabbOffset = aabbOffset
	};

	return GraphicsDeviceRef_createBLAS(dev, &blasInfo, name, blas, e_rr);
}

//Creating BLAS from cache

//Error GraphicsDeviceRef_createBLASFromCacheExt(GraphicsDeviceRef *dev, Buffer cache, CharString name, BLASRef **blas) {
//
//    BLAS blasInfo = (BLAS) {
//        .base = (RTAS) { .asConstructionType = (U8) EBLASConstructionType_Serialized, },
//        .cpuData = cache
//    };
//
//    return GraphicsDeviceRef_createBLAS(dev, blasInfo, name, blas);
//}
