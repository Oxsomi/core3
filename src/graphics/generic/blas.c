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

#include "graphics/generic/interface.h"
#include "graphics/generic/blas.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "types/container/string.h"
#include "types/container/ref_ptr.h"
#include "types/base/constants.h"

void BLAS_free(void *blasGeneric, const Allocator *alloc) {

	BLAS *blas = (BLAS*) blasGeneric;

	(void)alloc;

	SpinLock_lock(&blas->base.lock, U64_MAX);

	BLAS_freeExt(blas);
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
		RefPtr_dec(&blas->ommIndexBuffer.buffer);
	}

	//A refit holds a reference to the AS it updates from, taken at create; without this the parent leaks for
	// the process lifetime, and with it goes its bindless descriptor and its device reference.

	RefPtr_dec(&blas->base.parent);

	RefPtr_dec(&blas->base.device);
}

Bool GraphicsDeviceRef_createBLAS(
	GraphicsDeviceRef *dev, const BLAS *blas, const CharString *name, BLASRef **blasRef, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	//Validate

	if(!dev || dev->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_createBLAS()::dev is required"));

	if(!blas)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_createBLAS()::blas is required"));

	if(!blasRef)
		retError(clean, Error_nullPointer(3, "GraphicsDeviceRef_createBLAS()::blasRef is required"));

	if(*blasRef)
		retError(clean, Error_invalidParameter(
			3, 0, "GraphicsDeviceRef_createBLAS()::*blasRef not NULL, indicates memleak"
		));

	if(blas->base.parent && blas->base.parent->refPtrType->typeId != (TypeId) EGraphicsTypeId_BLASExt)
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

	//Without position fetch support the flag would be silently invalid on the API side, and the failure would
	// surface at trace time rather than here where the mistake was made.

	if((blas->base.flags & ERTASBuildFlags_AllowDataAccessExt) && !(feat & EGraphicsFeatures_RayTriPosition))
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createBLAS() uses AllowDataAccess, but position fetch is unsupported"
		));

	//Same reasoning for opacity micromaps: an unsupported OMM index buffer would either be ignored or fail
	// deep inside the build, rather than at the create call that asked for it.
	//The construction type is only meaningful for geometry, so the field is read behind that check.

	if(
		blas->base.asConstructionType == EBLASConstructionType_Geometry &&
		blas->ommIndexFormatId &&
		!(feat & EGraphicsFeatures_RayMicromapOpacity)
	)
		retError(clean, Error_unsupportedOperation(
			1, "GraphicsDeviceRef_createBLAS() uses an OMM index buffer, but opacity micromaps are unsupported"
		));

	//RTAS_validateDeviceBuffer may normalize len, so validate local copies;
	//they're committed to the new BLAS below

	DeviceData positionBuffer = (DeviceData) { 0 };
	DeviceData indexBuffer = (DeviceData) { 0 };
	DeviceData aabbBuffer = (DeviceData) { 0 };
	DeviceData ommIndexBuffer = (DeviceData) { 0 };

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
						1, "GraphicsDeviceRef_createBLAS()::indexBuffer should be NULL if indexFormat is Undefined"
					));

				break;

			case ETextureFormatId_R16u:
			case ETextureFormatId_R32u: {

				U8 indexStride = indexFormat == ETextureFormatId_R32u ? 4 : 2;

				if(!indexBuffer.buffer || (indexBuffer.len & (indexStride - 1)))
					retError(clean, Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::indexBuffer should be multiple of indexFormat and not NULL"
					));

				break;
			}

			default:
				retError(clean, Error_unsupportedOperation(2, "GraphicsDeviceRef_createBLAS()::indexFormat must be R32u or R16u"));
		}

		//Validate opacity micromaps (stage 1, special indices only)

		const ETextureFormatId ommIndexFormat = blas->ommIndexFormatId;
		ommIndexBuffer = blas->ommIndexBuffer;

		if (ommIndexFormat == ETextureFormatId_Undefined) {

			if(ommIndexBuffer.buffer)
				retError(clean, Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS()::ommIndexBuffer should be NULL if ommIndexFormat is Undefined"
				));
		}

		else {

			//An OMM index is per triangle, so without triangle indices there is nothing to index against.

			if(indexFormat == ETextureFormatId_Undefined)
				retError(clean, Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS()::ommIndexFormat requires an indexed BLAS"
				));

			if(ommIndexFormat != ETextureFormatId_R16u && ommIndexFormat != ETextureFormatId_R32u)
				retError(clean, Error_unsupportedOperation(
					2, "GraphicsDeviceRef_createBLAS()::ommIndexFormat must be R32u or R16u"
				));

			//Validated before the length checks below, because RTAS_validateDeviceBuffer normalizes a len of 0
			// to "rest of the buffer"; checking lengths first would reject that spelling instead of resolving it.

			gotoIfError3(clean, RTAS_validateDeviceBuffer(&ommIndexBuffer, e_rr));

			const U8 ommIndexStride = ommIndexFormat == ETextureFormatId_R32u ? 4 : 2;

			if(!ommIndexBuffer.buffer || (ommIndexBuffer.len & (ommIndexStride - 1)))
				retError(clean, Error_unsupportedOperation(
					1,
					"GraphicsDeviceRef_createBLAS()::ommIndexBuffer should be multiple of ommIndexFormat and not NULL"
				));

			//One OMM index per triangle, and a triangle is three vertex indices.

			const U8 indexStride = indexFormat == ETextureFormatId_R32u ? 4 : 2;
			const U64 triangles = indexBuffer.len / indexStride / 3;

			if(ommIndexBuffer.len / ommIndexStride != triangles)
				retError(clean, Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS()::ommIndexBuffer needs exactly one index per triangle"
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

	//Set as soon as the object exists rather than once it is fully built.
	//A failure below frees the half built BLAS, and BLAS_freeExt reaches the backend through base.device,
	// so leaving it for the end turned any late failure into a crash.

	gotoIfError3(clean, RefPtr_inc(dev));
	blasPtr->base.device = dev;

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

		//The index buffer is optional (indexFormat Undefined), and inc'ing a NULL one reports failure without an
		// error, so an index free BLAS used to fail creation silently here.

		if(indexBuffer.buffer)
			RefPtr_inc(indexBuffer.buffer);

		blasPtr->indexBuffer = indexBuffer;

		gotoIfError3(clean, RefPtr_inc(positionBuffer.buffer));
		blasPtr->positionBuffer = positionBuffer;

		//Optional like the index buffer, so the same NULL guard applies.
		//Committing the validated local rather than leaving the struct copy in place is what carries the
		// normalized length (a len of 0 means "rest of the buffer") into the object.

		blasPtr->ommIndexBuffer = (DeviceData) { 0 };

		if(ommIndexBuffer.buffer)
			RefPtr_inc(ommIndexBuffer.buffer);

		blasPtr->ommIndexBuffer = ommIndexBuffer;
	}

	if(name)
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &blasPtr->base.name, e_rr));

	gotoIfError3(clean, BLAS_initExt(blasPtr, e_rr));

clean:

	if(!s_uccess)
		RefPtr_dec(blasRef);

	return s_uccess;
}

//The element width of an OMM index format, 0 for a format that carries no OMM.
//Kept local because the two legal formats are validated at create time; anything else has no width to report.

static U8 EOMMIndex_stride(ETextureFormatId ommIndexFormat) {

	switch (ommIndexFormat) {
		case ETextureFormatId_R16u:    return 2;
		case ETextureFormatId_R32u:    return 4;
		default:                       return 0;
	}
}

U32 EOMMSpecialIndex_pack(EOMMSpecialIndex specialIndex, ETextureFormatId ommIndexFormat) {

	//Both APIs define the special indices as negative signed values but compare them against the UNSIGNED
	// element, so the stored value is the two's complement truncated to that element's width.
	//Masking rather than casting through I16/I32 keeps the truncation explicit and width driven.

	const U8 stride = EOMMIndex_stride(ommIndexFormat);

	if(!stride)
		return 0;

	const U32 raw = (U32)(I32) specialIndex;
	return stride == 2 ? (raw & U16_MAX) : raw;
}

U32 EOMMIndex_max(ETextureFormatId ommIndexFormat) {

	//Four special values sit at the top of the range, so the last usable real index is four below the maximum.

	const U8 stride = EOMMIndex_stride(ommIndexFormat);

	if(!stride)
		return 0;

	return (stride == 2 ? U16_MAX : U32_MAX) - 4;
}

Bool EOMMIndex_isSpecial(U32 raw, ETextureFormatId ommIndexFormat) {

	const U8 stride = EOMMIndex_stride(ommIndexFormat);

	if(!stride)
		return false;

	return raw > EOMMIndex_max(ommIndexFormat) && raw <= (stride == 2 ? U16_MAX : U32_MAX);
}

BLASCreateInfo BLASCreateInfo_indexed(
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	ETextureFormatId indexFormat,
	DeviceData indexBuffer
) {
	return (BLASCreateInfo) {
		.buildFlags = buildFlags,
		.blasFlags = blasFlags,
		.positionFormat = positionFormat,
		.indexFormat = indexFormat,
		.positionOffset = positionOffset,
		.positionBufferStride = positionBufferStride,
		.positionBuffer = positionBuffer,
		.indexBuffer = indexBuffer
	};
}

//Unindexed is the same geometry with no index buffer, which the format enum already expresses as Undefined.
//It stays a helper rather than an entry point so there is only one create call to validate and to extend.

BLASCreateInfo BLASCreateInfo_unindexed(
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer
) {
	return BLASCreateInfo_indexed(
		buildFlags,
		blasFlags,
		positionFormat,
		positionOffset,
		positionBufferStride,
		positionBuffer,
		ETextureFormatId_Undefined,
		(DeviceData) { 0 }
	);
}

//Special index only OMM: the caller supplies one index per triangle and no micromap object exists yet.

BLASCreateInfo BLASCreateInfo_indexedWithOmmIndicesExt(
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	ETextureFormatId indexFormat,
	DeviceData indexBuffer,
	ETextureFormatId ommIndexFormat,
	DeviceData ommIndexBuffer
) {

	BLASCreateInfo info = BLASCreateInfo_indexed(
		buildFlags,
		blasFlags,
		positionFormat,
		positionOffset,
		positionBufferStride,
		positionBuffer,
		indexFormat,
		indexBuffer
	);

	info.ommIndexFormat = ommIndexFormat;
	info.ommIndexBuffer = ommIndexBuffer;
	return info;
}

Bool GraphicsDeviceRef_createBLASExt(
	GraphicsDeviceRef *dev,
	const BLASCreateInfo *info,
	const CharString *name,
	BLASRef **blas,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!info)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_createBLASExt()::info is required"));

	const BLAS blasInfo = (BLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) EBLASConstructionType_Geometry,
			.flags = (U8) info->buildFlags,
			.flagsExt = (U8) info->blasFlags,
			.parent = info->parent
		},
		.positionFormatId = (U8) info->positionFormat,
		.indexFormatId = (U8) info->indexFormat,
		.positionBufferStride = info->positionBufferStride,
		.positionOffset = info->positionOffset,
		.indexBuffer = info->indexBuffer,
		.positionBuffer = info->positionBuffer,
		.ommIndexFormatId = (U8) info->ommIndexFormat,
		.ommIndexBuffer = info->ommIndexBuffer
	};

	gotoIfError3(clean, GraphicsDeviceRef_createBLAS(dev, &blasInfo, name, blas, e_rr));

clean:
	return s_uccess;
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
	const CharString *name,
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

//Bool GraphicsDeviceRef_createBLASFromCacheExt(
// GraphicsDeviceRef *dev, Buffer cache, CharString name, BLASRef **blas, Error *e_rr
//) {
//
//    BLAS blasInfo = (BLAS) {
//        .base = (RTAS) { .asConstructionType = (U8) EBLASConstructionType_Serialized, },
//        .cpuData = cache
//    };
//
//    return GraphicsDeviceRef_createBLAS(dev, &blasInfo, name, blas, e_rr);
//}
