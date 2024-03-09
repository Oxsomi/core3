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

#include "platforms/ext/listx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/ref_ptrx.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "types/buffer.h"

Error BLASRef_dec(BLASRef **blas) {
	return !RefPtr_dec(blas) ? Error_invalidOperation(0, "BLASRef_dec()::blas is required") : Error_none();
}

Error BLASRef_inc(BLASRef *blas) {
	return !RefPtr_inc(blas) ? Error_invalidOperation(0, "BLASRef_inc()::blas is required") : Error_none();
}

impl extern const U64 BLASExt_size;
impl Bool BLAS_freeExt(BLAS *blas);

Bool BLAS_free(BLAS *blas, Allocator allocator) {

	(void)allocator;

	RefPtr *refPtr = (RefPtr*)((const U8*)blas - sizeof(RefPtr));

	Lock_free(&blas->base.lock);

	Bool success = GraphicsDeviceRef_removePending(blas->base.device, refPtr);
	success &= BLAS_freeExt(blas);
	success &= CharString_freex(&blas->base.name);

	success &= !DeviceBufferRef_dec(&blas->base.asBuffer).genericError;

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		success &= Buffer_freex(&blas->cpuData);

	else if (blas->base.asConstructionType == EBLASConstructionType_Procedural)
		success &= !DeviceBufferRef_dec(&blas->aabbBuffer.buffer).genericError;

	else {
		success &= !DeviceBufferRef_dec(&blas->indexBuffer.buffer).genericError;
		success &= !DeviceBufferRef_dec(&blas->positionBuffer.buffer).genericError;
	}

	success &= !GraphicsDeviceRef_dec(&blas->base.device).genericError;
	return success;
}

Error GraphicsDeviceRef_createBLAS(GraphicsDeviceRef *dev, BLAS blas, CharString name, BLASRef **blasRef) {

	//Validate

	if(!dev || dev->typeId != EGraphicsTypeId_GraphicsDevice)
		return Error_nullPointer(0, "GraphicsDeviceRef_createBLAS()::dev is required");

	if(!blasRef)
		return Error_nullPointer(3, "GraphicsDeviceRef_createBLAS()::blasRef is required");

	if(*blasRef)
		return Error_invalidParameter(3, 0, "GraphicsDeviceRef_createBLAS()::*blasRef not NULL, indicates memleak");

	if(blas.base.parent && blas.base.parent->typeId != (ETypeId) EGraphicsTypeId_BLASExt)
		return Error_invalidOperation(1, "GraphicsDeviceRef_createBLAS()::parent is invalid");

	if(blas.base.parent && BLASRef_ptr(blas.base.parent)->base.device != dev)
		return Error_invalidOperation(1, "GraphicsDeviceRef_createBLAS()::parent and BLAS device need to share device");

	if(blas.base.parent)
		blas.base.flags |= ERTASBuildFlags_IsUpdate;

	else if(blas.base.flags & ERTASBuildFlags_IsUpdate)
		return Error_invalidOperation(7, "GraphicsDeviceRef_createBLAS()::parent is required if IsUpdate is present");

	if(!(blas.base.flags & ERTASBuildFlags_AllowUpdate) && (blas.base.flags & ERTASBuildFlags_IsUpdate))
		return Error_invalidOperation(7, "GraphicsDeviceRef_createBLAS() is update is not possible if AllowUpdate is false");

	EGraphicsFeatures feat = GraphicsDeviceRef_ptr(dev)->info.capabilities.features;

	if(!(feat & EGraphicsFeatures_Raytracing))
		return Error_unsupportedOperation(0, "GraphicsDeviceRef_createBLAS() is unsupported without raytracing support");

	if(blas.base.isMotionBlurExt && !(feat & EGraphicsFeatures_RayMotionBlur))
		return Error_unsupportedOperation(1, "GraphicsDeviceRef_createBLAS() uses motion blur, but it's unsupported");

	//Validate geometry BLAS

	if(blas.base.asConstructionType == EBLASConstructionType_Geometry) {

		U16 stride = blas.positionBufferStride;
		ETextureFormatId positionFormat = blas.positionFormatId;
		U16 positionOffset = blas.positionOffset;
		ETextureFormatId indexFormat = blas.indexFormatId;

		if(stride > 2048 || !stride)
			return Error_unsupportedOperation(1, "GraphicsDeviceRef_createBLAS()::stride must be >0 and <=2048");

		U8 reqMultiple = 2;

		if(positionFormat == ETextureFormatId_RGBA32f || positionFormat == ETextureFormatId_RG32f)
			reqMultiple = 4;

		if(stride & (reqMultiple - 1))
			return Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createBLAS()::stride must be multiple of channel byte size (2 or 4)"
			);

		Error err = Error_none();

		if((err = RTAS_validateDeviceBuffer(&blas.positionBuffer)).genericError)
			return err;

		if(indexFormat != ETextureFormatId_Undefined && (err = RTAS_validateDeviceBuffer(&blas.indexBuffer)).genericError)
			return err;

		switch (positionFormat) {

			case ETextureFormatId_RGBA16f:
			case ETextureFormatId_RGBA16s:
			case ETextureFormatId_RGBA32f:
			case ETextureFormatId_RG16f:
			case ETextureFormatId_RG16s:
			case ETextureFormatId_RG32f:

				if(positionOffset + ETextureFormat_getSize(ETextureFormatId_unpack[positionFormat], 1, 1, 1) > stride)
					return Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::positionOffset and/or positionFormat out of bounds (> stride)"
					);

				if(blas.positionBuffer.len < stride || (blas.positionBuffer.len % stride))
					return Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::positionBuffer should be multiple of stride"
					);

				if((blas.positionBuffer.len / stride) >> 32)
					return Error_outOfBounds(
						0, blas.positionBuffer.len / stride, U32_MAX,
						"GraphicsDeviceRef_createBLAS() vertices out of bounds"
					);

				break;

			default:
				return Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS()::positionFormat must be RGBA(16f/32f/16)"
				);
		}

		switch (indexFormat) {

			case ETextureFormatId_Undefined:

				if(blas.indexBuffer.buffer)
					return Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::indexBuffer should be NULL if indexFormat is Undefined"
					);

				break;

			case ETextureFormatId_R16u:
			case ETextureFormatId_R32u: {

				U8 indexStride = indexFormat == ETextureFormatId_R32u ? 4 : 2;

				if(!blas.indexBuffer.buffer || (blas.indexBuffer.len & (indexStride - 1)))
					return Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::indexBuffer should be multiple of indexFormat and not NULL"
					);

				break;
			}

			default:
				return Error_unsupportedOperation(
					2, "GraphicsDeviceRef_createBLAS()::positionFormat must be RGBA(16f/32f/16)"
				);
		}
	}

	//Validate AABBs

	else if (blas.base.asConstructionType == EBLASConstructionType_Procedural) {

		U64 stride = blas.aabbStride;

		if(!stride || stride & 7)
			return Error_unsupportedOperation(1, "GraphicsDeviceRef_createBLAS()::stride must be >0 and divisible by 8");

		if((U64)blas.aabbOffset + 24 > stride)
			return Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createBLAS()::aabbOffset out of bounds (> stride)"
			);

		Error err = Error_none();

		if((err = RTAS_validateDeviceBuffer(&blas.aabbBuffer)).genericError)
			return err;

		if(blas.aabbBuffer.len < stride || (blas.aabbBuffer.len % stride))
			return Error_unsupportedOperation(
				1, "GraphicsDeviceRef_createBLAS()::aabbBuffer should be multiple of stride"
			);
	}

	//Validate serialized

	else if(!Buffer_length(blas.cpuData))
		return Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createBLAS()::cpuData should be valid if serialized construction is used"
		);

	GraphicsDevice *device = GraphicsDeviceRef_ptr(dev);
	Error err = Error_none();
	ELockAcquire acq = ELockAcquire_Invalid;

	//Allocate refPtr

	_gotoIfError(clean, RefPtr_createx(
		(U32) (sizeof(BLAS) + BLASExt_size),
		(ObjectFreeFunc) BLAS_free,
		(ETypeId) EGraphicsTypeId_BLASExt,
		blasRef
	));

	//Fill ptr

	BLAS *blasPtr = BLASRef_ptr(*blasRef);

	if(blas.base.parent)
		_gotoIfError(clean, BLASRef_inc(blas.base.parent));

	*blasPtr = blas;
	blasPtr->base.lock = Lock_create();
	blasPtr->base.name = CharString_createNull();

	if (blas.base.asConstructionType == EBLASConstructionType_Serialized) {
		blasPtr->cpuData = Buffer_createNull();
		_gotoIfError(clean, Buffer_createCopyx(blas.cpuData, &blasPtr->cpuData));
	}

	else if (blas.base.asConstructionType == EBLASConstructionType_Procedural) {

		blasPtr->aabbBuffer = (DeviceData) { 0 };
		_gotoIfError(clean, DeviceBufferRef_inc(blas.aabbBuffer.buffer));
		blasPtr->aabbBuffer = blas.aabbBuffer;
	}

	else {

		blasPtr->indexBuffer = (DeviceData) { 0 };
		blasPtr->positionBuffer = (DeviceData) { 0 };

		_gotoIfError(clean, DeviceBufferRef_inc(blas.indexBuffer.buffer));
		blasPtr->indexBuffer = blas.indexBuffer;

		_gotoIfError(clean, DeviceBufferRef_inc(blas.positionBuffer.buffer));
		blasPtr->positionBuffer = blas.positionBuffer;
	}

	_gotoIfError(clean, GraphicsDeviceRef_inc(dev));
	blasPtr->base.device = dev;

	_gotoIfError(clean, CharString_createCopyx(name, &blasPtr->base.name));

	//Push for the graphics impl to process next submit,
	//If it's GPU generated, the user is expected to manually call buildBLASExt to ensure it's enqueued at the right time

	if (!(blas.base.flags & ERTASBuildFlags_DisableAutomaticUpdate)) {

		acq = Lock_lock(&device->lock, U64_MAX);

		if(acq < ELockAcquire_Success)
			_gotoIfError(clean, Error_invalidState(0, "GraphicsDeviceRef_createBLAS()::dev couldn't be locked"));

		_gotoIfError(clean, ListWeakRefPtr_pushBackx(&device->pendingBlases, *blasRef));
	}

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&device->lock);

	if(err.genericError)
		BLASRef_dec(blasRef);

	return err;
}

Error GraphicsDeviceRef_createBLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,	//RGBA16f, RGBA32f, RGBA16s
	U16 positionOffset,					//Offset into first position for first vertex
	ETextureFormatId indexFormat,		//R16u, R32u, Undefined
	U16 positionBufferStride,			//<=2048 and multiple of 2 (if not 32f) or 4 (RGBA32f)
	DeviceData positionBuffer,			//Required
	DeviceData indexBuffer,				//Optional if indexFormat == Undefined
	BLASRef *parent,					//If specified, indicates refit
	CharString name,
	BLASRef **blas
) {
	BLAS blasInfo = (BLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) EBLASConstructionType_Geometry,
			.flags = (U8) buildFlags,
			.flagsExt = (U8) blasFlags,
			.parent = parent
		},
		.positionFormatId =	(U8) positionFormat,
		.indexFormatId = (U8) indexFormat,
		.positionBufferStride = positionBufferStride,
		.positionOffset = positionOffset,
		.indexBuffer = indexBuffer,
		.positionBuffer = positionBuffer
	};

	return GraphicsDeviceRef_createBLAS(dev, blasInfo, name, blas);
}

Error GraphicsDeviceRef_createBLASUnindexedExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,	//RGBA16f, RGBA32f, RGBA16s
	U16 positionOffset,					//Offset into first position for first vertex
	U16 positionBufferStride,			//<=2048 and multiple of 2 (if not 32f) or 4 (RGBA32f)
	DeviceData positionBuffer,			//Required
	BLASRef *parent,					//If specified, indicates refit
	CharString name,
	BLASRef **blas
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
	);
}

//Creating BLAS from AABBs

Error GraphicsDeviceRef_createBLASProceduralExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	U32 aabbStride,
	U32 aabbOffset,
	DeviceData buffer,
	BLASRef *parent,
	CharString name,
	BLASRef **blas
) {

	BLAS blasInfo = (BLAS) {
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

	return GraphicsDeviceRef_createBLAS(dev, blasInfo, name, blas);
}

//Creating BLAS from cache

//Error GraphicsDeviceRef_createBLASFromCacheExt(GraphicsDeviceRef *dev, Buffer cache, CharString name, BLASRef **blas) {
//
//	BLAS blasInfo = (BLAS) {
//		.base = (RTAS) { .asConstructionType = (U8) EBLASConstructionType_Serialized, },
//		.cpuData = cache
//	};
//
//	return GraphicsDeviceRef_createBLAS(dev, blasInfo, name, blas);
//}
