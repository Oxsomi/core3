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
#include "graphics/generic/tlas.h"
#include "graphics/generic/opacity_micromap.h"
#include "platforms/logx.h"
#include "graphics/generic/device_buffer.h"
#include "graphics/generic/device.h"
#include "types/container/string.h"
#include "types/container/ref_ptr.h"
#include "types/base/constants.h"
#include "types/container/list_impl.h"

TListImpl(BLASGeometry);

void BLAS_free(void *blasGeneric, const Allocator *alloc) {

	BLAS *blas = (BLAS*) blasGeneric;

	(void)alloc;

	SpinLock_lock(&blas->base.lock, U64_MAX);

	BLAS_freeExt(blas);
	CharString_free(&blas->base.name, alloc);

	//The build claims a slot and prepareCompactBLAS hands it back, so a structure destroyed between the two,
	// or one whose compaction is never prepared at all, is the one path left that can return it.
	//Without this the slot stays claimed for the rest of the session and the pools grow by one every time.

	if(blas->base.compactionQuery != U32_MAX) {

		GraphicsDevice *device = GraphicsDeviceRef_ptr(blas->base.device);

		GraphicsDevice_releaseCompactionQuery(device, blas->base.compactionQuery, alloc);
		blas->base.compactionQuery = U32_MAX;
	}

	RefPtr_dec(&blas->base.asBuffer);
	RefPtr_dec(&blas->base.tempScratchBuffer);

	//The destination of a compaction that was prepared but never recorded; BLAS_freeExt above has already
	// destroyed whatever the backend put in it.

	RefPtr_dec(&blas->base.pendingCompactBuffer);

	if(blas->base.asConstructionType == EBLASConstructionType_Serialized)
		Buffer_free(&blas->cpuData, alloc);

	else if (blas->base.asConstructionType == EBLASConstructionType_Procedural)
		RefPtr_dec(&blas->aabbBuffer.buffer);

	else {

		for(U64 i = 0; i < blas->geometries.length; ++i) {

			BLASGeometry *geometry = &blas->geometries.ptrNonConst[i];

			RefPtr_dec(&geometry->indexBuffer.buffer);
			RefPtr_dec(&geometry->positionBuffer.buffer);
			RefPtr_dec(&geometry->ommIndexBuffer.buffer);
			RefPtr_dec(&geometry->ommMicromap);
		}

		ListBLASGeometry_free(&blas->geometries, alloc);
	}

	RefPtr_dec(&blas->base.device);
}

//Defined with the other EOMMIndex helpers below; the create validation above them needs the width too.

static U8 EOMMIndex_stride(ETextureFormatId ommIndexFormat);

Bool GraphicsDeviceRef_createBLAS(
	GraphicsDeviceRef *dev, const BLAS *blas, const CharString *name, BLASRef **blasRef, Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = GraphicsDeviceRef_getAlloc(dev);

	//Owned here until it is handed to the BLAS below, so a failure anywhere in between frees it.
	//Declared before the first jump to clean, which frees it.

	ListBLASGeometry geometries = (ListBLASGeometry) { 0 };

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

	//RTAS_validateDeviceBuffer may normalize len, so validate a local copy of every geometry;
	//they're committed to the new BLAS below

	DeviceData aabbBuffer = (DeviceData) { 0 };

	//Validate geometry BLAS

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry) {

		if(!blas->geometries.length)
			retError(clean, Error_invalidParameter(
				1, 0, "GraphicsDeviceRef_createBLAS()::blas->geometries needs at least one geometry"
			));

		gotoIfError3(clean, ListBLASGeometry_resize(&geometries, blas->geometries.length, alloc, e_rr));

		for(U64 i = 0; i < blas->geometries.length; ++i) {

			BLASGeometry geometry = blas->geometries.ptr[i];

			//An unsupported OMM index buffer would either be ignored or fail deep inside the build, rather
			// than at the create call that asked for it.

			if(geometry.ommIndexFormatId && !(feat & EGraphicsFeatures_RayMicromapOpacity))
				retError(clean, Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS() uses an OMM index buffer, but opacity micromaps are unsupported"
				));

			U16 stride = geometry.positionBufferStride;
			ETextureFormatId positionFormat = geometry.positionFormatId;
			U16 positionOffset = geometry.positionOffset;
			ETextureFormatId indexFormat = geometry.indexFormatId;

			if(stride > 2048 || !stride)
				retError(clean, Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS()::stride must be >0 and <=2048"
				));

			U8 reqMultiple = 2;

			if(
				positionFormat == ETextureFormatId_RGBA32f || positionFormat == ETextureFormatId_RG32f ||
				positionFormat == ETextureFormatId_RGB32f
			)
				reqMultiple = 4;

			if(stride & (reqMultiple - 1))
				retError(clean, Error_unsupportedOperation(
					1, "GraphicsDeviceRef_createBLAS()::stride must be multiple of channel byte size (2 or 4)"
				));

			gotoIfError3(clean, RTAS_validateDeviceBuffer(&geometry.positionBuffer, e_rr));

			if(indexFormat != ETextureFormatId_Undefined)
				gotoIfError3(clean, RTAS_validateDeviceBuffer(&geometry.indexBuffer, e_rr));

			switch (positionFormat) {

				//RGB32f is the format both APIs actually take for an acceleration structure, and the only one
				//they mandate. The four component twin is accepted beside it and translated to this one by
				//both backends, which is what lets a caller whose stride is 16 pass either; a caller whose
				//stride is 12 has to name this one, since nothing may read the w that is not there.

				case ETextureFormatId_RGB32f:

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

					if(geometry.positionBuffer.len < stride || (geometry.positionBuffer.len % stride))
						retError(clean, Error_unsupportedOperation(
							1, "GraphicsDeviceRef_createBLAS()::positionBuffer should be multiple of stride"
						));

					if((geometry.positionBuffer.len / stride) >> 32)
						retError(clean, Error_outOfBounds(
							0, geometry.positionBuffer.len / stride, U32_MAX,
							"GraphicsDeviceRef_createBLAS() vertices out of bounds"
						));

					break;

				default:
					retError(clean, Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::positionFormat must be RGB32f, RGBA(16f/32f/16s) or RG(16f/16s/32f)"
					));
			}

			switch (indexFormat) {

				case ETextureFormatId_Undefined:

					if(geometry.indexBuffer.buffer)
						retError(clean, Error_unsupportedOperation(
							1, "GraphicsDeviceRef_createBLAS()::indexBuffer should be NULL if indexFormat is Undefined"
						));

					break;

				case ETextureFormatId_R16u:
				case ETextureFormatId_R32u: {

					U8 indexStride = indexFormat == ETextureFormatId_R32u ? 4 : 2;

					if(!geometry.indexBuffer.buffer || (geometry.indexBuffer.len & (indexStride - 1)))
						retError(clean, Error_unsupportedOperation(
							1, "GraphicsDeviceRef_createBLAS()::indexBuffer should be multiple of indexFormat and not NULL"
						));

					break;
				}

				default:
					retError(clean, Error_unsupportedOperation(
						2, "GraphicsDeviceRef_createBLAS()::indexFormat must be R32u or R16u"
					));
			}

			//Validate opacity micromaps (stage 1, special indices only)

			const ETextureFormatId ommIndexFormat = geometry.ommIndexFormatId;

			if (ommIndexFormat == ETextureFormatId_Undefined) {

				if(geometry.ommMicromap)
					retError(clean, Error_invalidOperation(
						1, "GraphicsDeviceRef_createBLAS()::ommMicromap requires an OMM index buffer to link through"
					));

				if(geometry.ommIndexBuffer.buffer)
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

				if(
					ommIndexFormat != ETextureFormatId_R16u && ommIndexFormat != ETextureFormatId_R32u &&
					ommIndexFormat != ETextureFormatId_R8u
				)
					retError(clean, Error_unsupportedOperation(
						2, "GraphicsDeviceRef_createBLAS()::ommIndexFormat must be R32u, R16u or R8u"
					));

				//D3D12 always takes 8-bit OMM indices, Vulkan only under the KHR extension, which is what the
				// qualifier bit reports; rejected here so the mistake surfaces at create rather than in the driver.

				if(
					ommIndexFormat == ETextureFormatId_R8u &&
					!(GraphicsDeviceRef_ptr(dev)->info.capabilities.features2 & EGraphicsFeatures2_RayMicromapOpacityU8)
				)
					retError(clean, Error_unsupportedOperation(
						2, "GraphicsDeviceRef_createBLAS()::ommIndexFormat R8u needs RayMicromapOpacityU8"
					));

				//Validated before the length checks below, because RTAS_validateDeviceBuffer normalizes a len of 0
				// to "rest of the buffer"; checking lengths first would reject that spelling instead of resolving it.

				gotoIfError3(clean, RTAS_validateDeviceBuffer(&geometry.ommIndexBuffer, e_rr));

				const U8 ommIndexStride = EOMMIndex_stride(ommIndexFormat);

				if(!geometry.ommIndexBuffer.buffer || (geometry.ommIndexBuffer.len & (ommIndexStride - 1)))
					retError(clean, Error_unsupportedOperation(
						1,
						"GraphicsDeviceRef_createBLAS()::ommIndexBuffer should be multiple of ommIndexFormat and not NULL"
					));

				//A linked micromap has to be a real one from the same device; NULL is the special index only form.

				if (geometry.ommMicromap) {

					if(geometry.ommMicromap->refPtrType->typeId != (TypeId) EGraphicsTypeId_OpacityMicromapExt)
						retError(clean, Error_invalidOperation(
							1, "GraphicsDeviceRef_createBLAS()::ommMicromap is invalid"
						));

					if(OpacityMicromapRef_ptr(geometry.ommMicromap)->base.device != dev)
						retError(clean, Error_invalidOperation(
							1, "GraphicsDeviceRef_createBLAS()::ommMicromap needs to share the BLAS's device"
						));

					//Worth saying once rather than never or per create: a real micromap costs build time and
					// memory, and on a device that likely emulates micromaps traversal won't pay that back the
					// way the free special indices would (see EGraphicsFeatures2_RayMicromapOpacityActual).

					if(
						!(GraphicsDeviceRef_ptr(dev)->info.capabilities.features2 &
						EGraphicsFeatures2_RayMicromapOpacityActual) &&
						GraphicsDevice_logOnce(GraphicsDeviceRef_ptr(dev), EGraphicsDeviceMessage_OmmLikelyEmulated)
					)
						Log_performanceLnx(
							"GraphicsDeviceRef_createBLAS() linked a real opacity micromap, but this device likely "
							"emulates micromaps (RayMicromapOpacityActual is unset); special index OMM is free, a "
							"micromap object may not pay off here"
						);
				}

				//One OMM index per triangle, and a triangle is three vertex indices.

				const U8 indexStride = indexFormat == ETextureFormatId_R32u ? 4 : 2;
				const U64 triangles = geometry.indexBuffer.len / indexStride / 3;

				if(geometry.ommIndexBuffer.len / ommIndexStride != triangles)
					retError(clean, Error_unsupportedOperation(
						1, "GraphicsDeviceRef_createBLAS()::ommIndexBuffer needs exactly one index per triangle"
					));
			}

			geometries.ptrNonConst[i] = geometry;
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

	*blasPtr = *blas;
	blasPtr->base.name = CharString_createNull();

	//The copy above carries the CALLER's geometries, which this BLAS has not referenced yet.
	//Cleared before anything can fail, so a free on the way out never decs a reference it never took.

	if(blas->base.asConstructionType == EBLASConstructionType_Geometry)
		blasPtr->geometries = (ListBLASGeometry) { 0 };

	//U32_MAX means no compacted size slot is claimed, which is what the free path reads to decide whether it
	// has one to hand back.
	//Set from the copied struct here rather than in a backend's init, so the sentinel holds for both backends
	// and for a creation that fails before the backend ever ran.

	blasPtr->base.compactionQuery = U32_MAX;

	//Set as soon as the object exists rather than once it is fully built.
	//A failure below frees the half built BLAS, and BLAS_freeExt reaches the backend through base.device,
	// so leaving it for the end turned any late failure into a crash.

	gotoIfError3(clean, RefPtr_inc(dev));
	blasPtr->base.device = dev;

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

		//Handed over zeroed, so the geometries the loop below has not reached yet hold nothing to release.
		//Committing the validated copies rather than the caller's is what carries the normalized lengths
		// (a len of 0 means "rest of the buffer") into the object.

		blasPtr->geometries = geometries;
		geometries = (ListBLASGeometry) { 0 };

		for(U64 i = 0; i < blasPtr->geometries.length; ++i) {

			const BLASGeometry validated = blasPtr->geometries.ptr[i];
			BLASGeometry *geometry = &blasPtr->geometries.ptrNonConst[i];

			*geometry = (BLASGeometry) { 0 };

			//The index buffer is optional (indexFormat Undefined), and inc'ing a NULL one reports failure without an
			// error, so an index free BLAS has to be recognized rather than inc'd blindly.
			//The same holds for the OMM index buffer and the micromap.
			//Each reference is stored the moment it is taken, so a failure further down still releases it.

			if(validated.indexBuffer.buffer)
				RefPtr_inc(validated.indexBuffer.buffer);

			geometry->indexBuffer = validated.indexBuffer;

			if(validated.ommIndexBuffer.buffer)
				RefPtr_inc(validated.ommIndexBuffer.buffer);

			geometry->ommIndexBuffer = validated.ommIndexBuffer;

			if(validated.ommMicromap)
				gotoIfError3(clean, RefPtr_inc(validated.ommMicromap));

			geometry->ommMicromap = validated.ommMicromap;

			gotoIfError3(clean, RefPtr_inc(validated.positionBuffer.buffer));
			*geometry = validated;
		}
	}

	if(name)
		gotoIfError3(clean, CharString_createCopy(*name, alloc, &blasPtr->base.name, e_rr));

	gotoIfError3(clean, BLAS_initExt(blasPtr, e_rr));

clean:

	ListBLASGeometry_free(&geometries, alloc);

	if(!s_uccess)
		RefPtr_dec(blasRef);

	return s_uccess;
}

//The element width of an OMM index format, 0 for a format that carries no OMM.
//Kept local because the legal formats are validated at create time; anything else has no width to report.

static U8 EOMMIndex_stride(ETextureFormatId ommIndexFormat) {

	switch (ommIndexFormat) {
		case ETextureFormatId_R8u:     return 1;
		case ETextureFormatId_R16u:    return 2;
		case ETextureFormatId_R32u:    return 4;
		default:                       return 0;
	}
}

//All ones at the element's width, so the helpers below stay arithmetic rather than a ladder of ternaries.

static U32 EOMMIndex_mask(U8 stride) {
	return stride == 4 ? U32_MAX : ((U32)1 << (stride * 8)) - 1;
}

U32 EOMMSpecialIndex_pack(EOMMSpecialIndex specialIndex, ETextureFormatId ommIndexFormat) {

	//Both APIs define the special indices as negative signed values but compare them against the UNSIGNED
	// element, so the stored value is the two's complement truncated to that element's width.
	//Masking rather than casting through I16/I32 keeps the truncation explicit and width driven.

	const U8 stride = EOMMIndex_stride(ommIndexFormat);

	if(!stride)
		return 0;

	const U32 raw = (U32)(I32) specialIndex;
	return raw & EOMMIndex_mask(stride);
}

U32 EOMMIndex_max(ETextureFormatId ommIndexFormat) {

	//Four special values sit at the top of the range, so the last usable real index is four below the maximum.

	const U8 stride = EOMMIndex_stride(ommIndexFormat);

	if(!stride)
		return 0;

	return EOMMIndex_mask(stride) - 4;
}

Bool EOMMIndex_isSpecial(U32 raw, ETextureFormatId ommIndexFormat) {

	const U8 stride = EOMMIndex_stride(ommIndexFormat);

	if(!stride)
		return false;

	return raw > EOMMIndex_max(ommIndexFormat) && raw <= EOMMIndex_mask(stride);
}

BLASGeometry BLASGeometry_indexed(
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	ETextureFormatId indexFormat,
	DeviceData indexBuffer
) {
	return (BLASGeometry) {
		.positionFormatId = (U8) positionFormat,
		.indexFormatId = (U8) indexFormat,
		.positionOffset = positionOffset,
		.positionBufferStride = positionBufferStride,
		.positionBuffer = positionBuffer,
		.indexBuffer = indexBuffer
	};
}

//Unindexed is the same geometry with no index buffer, which the format enum already expresses as Undefined.
//It stays a helper rather than an entry point so there is only one create call to validate and to extend.

BLASGeometry BLASGeometry_unindexed(
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer
) {
	return BLASGeometry_indexed(
		positionFormat,
		positionOffset,
		positionBufferStride,
		positionBuffer,
		ETextureFormatId_Undefined,
		(DeviceData) { 0 }
	);
}

BLASGeometry BLASGeometry_indexedWithOmmIndicesExt(
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	ETextureFormatId indexFormat,
	DeviceData indexBuffer,
	ETextureFormatId ommIndexFormat,
	DeviceData ommIndexBuffer
) {

	BLASGeometry geometry = BLASGeometry_indexed(
		positionFormat,
		positionOffset,
		positionBufferStride,
		positionBuffer,
		indexFormat,
		indexBuffer
	);

	geometry.ommIndexFormatId = (U8) ommIndexFormat;
	geometry.ommIndexBuffer = ommIndexBuffer;
	return geometry;
}

BLASGeometry BLASGeometry_indexedWithOmmExt(
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	ETextureFormatId indexFormat,
	DeviceData indexBuffer,
	ETextureFormatId ommIndexFormat,
	DeviceData ommIndexBuffer,
	OpacityMicromapRef *ommMicromap
) {

	BLASGeometry geometry = BLASGeometry_indexedWithOmmIndicesExt(
		positionFormat, positionOffset, positionBufferStride, positionBuffer,
		indexFormat, indexBuffer, ommIndexFormat, ommIndexBuffer
	);

	geometry.ommMicromap = ommMicromap;
	return geometry;
}

BLASCreateInfo BLASCreateInfo_geometries(ERTASBuildFlags buildFlags, ListBLASGeometry geometries) {
	return (BLASCreateInfo) {
		.buildFlags = buildFlags,
		.geometries = geometries
	};
}

BLASCreateInfo BLASCreateInfo_single(ERTASBuildFlags buildFlags, const BLASGeometry *geometry) {

	ListBLASGeometry geometries = (ListBLASGeometry) { 0 };

	//A ref over one element of the caller's memory cannot overflow or allocate, so the only way this fails is
	// a NULL geometry, which leaves an empty list that create rejects by name.

	ListBLASGeometry_createRefConst(geometry, 1, &geometries, NULL);

	return BLASCreateInfo_geometries(buildFlags, geometries);
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
			.flags = (U8) info->buildFlags
		},
		.geometries = info->geometries
	};

	gotoIfError3(clean, GraphicsDeviceRef_createBLAS(dev, &blasInfo, name, blas, e_rr));

clean:
	return s_uccess;
}

//Creating BLAS from AABBs

Bool GraphicsDeviceRef_createBLASProceduralExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASGeometryFlag geometryFlags,
	U32 aabbStride,
	U32 aabbOffset,
	DeviceData buffer,
	const CharString *name,
	BLASRef **blas,
	Error *e_rr
) {
	const BLAS blasInfo = (BLAS) {
		.base = (RTAS) {
			.asConstructionType = (U8) EBLASConstructionType_Procedural,
			.flags = (U8) buildFlags
		},
		.aabbBuffer = buffer,
		.aabbStride = aabbStride,
		.aabbOffset = aabbOffset,
		.aabbFlags = (U8) geometryFlags
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

//Slot bookkeeping for the compacted size. The backend owns the storage this indexes into; WHICH slot is
//shared, so the two backends cannot drift apart on recycling or on when the storage has to grow.

//Pool k holds base << k slots, so the storage doubles: tens of thousands of pending structures need a
//handful of pools, and a scene with a few never allocates past the first.

Bool GraphicsDevice_claimCompactionQuery(
	GraphicsDevice *device, U32 base, U32 *query, Bool *needsPool, Error *e_rr
) {

	Bool s_uccess = true;

	*needsPool = false;

	//A returned slot before a new one, so a scene that compacts as it builds never grows the storage at all.

	if(device->compactionFreeQueries.length) {
		*query = device->compactionFreeQueries.ptr[device->compactionFreeQueries.length - 1];
		gotoIfError3(clean, ListU32_popBack(&device->compactionFreeQueries, NULL, e_rr));
		goto clean;
	}

	//The pools together hold base * (2^n - 1) slots, so the count landing exactly on that boundary is what
	// says the next slot has no storage behind it yet.

	U32 covered = 0, size = base;

	while(covered + size <= device->compactionQueryCount) {
		covered += size;
		size <<= 1;
	}

	*needsPool = covered == device->compactionQueryCount;
	*query = device->compactionQueryCount++;

clean:
	return s_uccess;
}

//The size of the pool a new slot needs, which is the one the caller is about to create.

U32 GraphicsDevice_compactionPoolSize(const GraphicsDevice *device, U32 base) {

	U32 covered = 0, size = base;

	while(covered + size <= device->compactionQueryCount) {
		covered += size;
		size <<= 1;
	}

	return size;
}

void GraphicsDevice_compactionQueryPool(U32 query, U32 base, U32 *pool, U32 *index) {

	U32 covered = 0, size = base, i = 0;

	while(covered + size <= query) {
		covered += size;
		size <<= 1;
		++i;
	}

	*pool = i;
	*index = query - covered;
}

//Failing to recycle would leak one slot, which is not worth failing the compaction that just succeeded.

void GraphicsDevice_releaseCompactionQuery(GraphicsDevice *device, U32 query, const Allocator *alloc) {
	ListU32_pushBack(&device->compactionFreeQueries, query, alloc, NULL);
}

//Everything about whether a compaction may happen at all, which is all that can be decided without
//touching an API. The backend reads the size and allocates the destination.
//
//Runs while the copy is being RECORDED, so a rejection reaches the caller there rather than from inside a
//submit. See CommandListRef_compactBLASExt for what the rules mean to a caller.

Bool GraphicsDeviceRef_markTlasesStaleForBLAS(GraphicsDeviceRef *deviceRef, BLASRef *blasRef, Bool duringSubmit, Error *e_rr) {

	Bool s_uccess = true;
	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	//liveTlases is device state a create or a free on another thread can be mutating.
	//Reentrant by the lock's own convention: only an Acquired result unlocks, so a caller already holding
	// the device lock (submit processing) passes straight through.

	const ELockAcquire liveAcq = SpinLock_lock(&device->lock, U64_MAX);

	if(liveAcq < ELockAcquire_Success)
		retError(clean, Error_invalidState(
			2, "GraphicsDeviceRef_markTlasesStaleForBLAS() couldn't acquire device lock"
		));

	for (U64 i = 0; i < device->liveTlases.length; ++i) {

		TLAS *tlas = TLASRef_ptr((TLASRef*) device->liveTlases.ptr[i]);

		//A device memory TLAS writes its own instance addresses, so whether it references this structure
		// cannot be read from here and its addresses could not be re-resolved anyway. Skipped, and the
		// caller owns it; see CommandListRef_compactBLASExt.

		if(TLAS_hasFlag(tlas, ETLASFlag_UseDeviceMemory))
			continue;

		for (U64 j = 0; j < tlas->cpuInstances.length; ++j) {

			TLASInstanceData dat = (TLASInstanceData) { 0 };
			TLAS_getInstanceDataCpu(tlas, j, &dat);

			if (dat.blasCpu == blasRef) {

				//InstancesDirty is what makes the next build actually re-resolve: an update on its own
				// refits without refilling, so the TLAS would otherwise rebuild around the old address.

				tlas->base.flagsExt |= (U8) (ETLASFlag_AddressesStale | ETLASFlag_InstancesDirty);

				//staleAtSubmitId names the LAST submit the old address is valid for, which the refusal
				// grace compares against (staleAtSubmitId >= submitId passes).
				//At record time that is the upcoming submit (submitId); during submit processing the
				// counter already advanced past the submit being processed, so it is submitId - 1: stamping
				// the raw counter there would gift the NEXT submit the grace and let a stale trace through.

				tlas->staleAtSubmitId = device->submitId - (duringSubmit ? 1 : 0);
				break;
			}
		}
	}

	if(liveAcq == ELockAcquire_Acquired)
		SpinLock_unlock(&device->lock);

clean:
	return s_uccess;
}

Bool GraphicsDeviceRef_prepareCompactBLAS(GraphicsDeviceRef *deviceRef, BLASRef *blasRef, Bool *recorded, Error *e_rr) {

	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;
	BLAS *blas = NULL;

	if(!deviceRef || deviceRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_GraphicsDevice)
		retError(clean, Error_nullPointer(0, "GraphicsDeviceRef_prepareCompactBLAS()::deviceRef is required"));

	if(!blasRef || blasRef->refPtrType->typeId != (TypeId) EGraphicsTypeId_BLASExt)
		retError(clean, Error_nullPointer(1, "GraphicsDeviceRef_prepareCompactBLAS()::blasRef is required"));

	if(!recorded)
		retError(clean, Error_nullPointer(2, "GraphicsDeviceRef_prepareCompactBLAS()::recorded is required"));

	*recorded = false;
	blas = BLASRef_ptr(blasRef);

	//Nothing to do rather than an error, so a caller can sweep everything it owns without first sorting out
	// what was built compactable, and recording twice is harmless.

	if(blas->base.isCompacted || !(blas->base.flags & ERTASBuildFlags_AllowCompaction))
		goto clean;

	if(!blas->base.isCompleted)
		retError(clean, Error_invalidOperation(
			0, "GraphicsDeviceRef_prepareCompactBLAS() the structure has to be built first"
		));

	GraphicsDevice *device = GraphicsDeviceRef_ptr(deviceRef);

	if(blas->base.compactionQuery == U32_MAX)
		retError(clean, Error_invalidState(
			2,
			"GraphicsDeviceRef_prepareCompactBLAS() no compacted size was recorded for this structure, "
			"its build predates the compaction path or the backend doesn't support it"
		));

	//The size is produced BY the build, so it does not exist until that build's submit has run. A submit is
	// done if the device was waited since, or if framesInFlight submits have been queued behind it, which
	// is what forced its fence to be waited.

	const Bool completed =
		device->completedSubmitId >= blas->base.compactionSubmitId ||
		device->submitId >= blas->base.compactionSubmitId + device->framesInFlight;

	if(!completed)
		retError(clean, Error_invalidState(
			3,
			"GraphicsDeviceRef_prepareCompactBLAS() the build's submit hasn't completed, so the compacted "
			"size isn't readable yet, record the compaction in a later submit than the build"
		));

	if((acq = SpinLock_lock(&blas->base.lock, U64_MAX)) < ELockAcquire_Success)
		retError(clean, Error_invalidOperation(
			4, "GraphicsDeviceRef_prepareCompactBLAS() couldn't acquire the RTAS lock"
		));

	gotoIfError3(clean, BLASRef_prepareCompactExt(deviceRef, blasRef, recorded, e_rr));

clean:

	if(blas && acq == ELockAcquire_Acquired)
		SpinLock_unlock(&blas->base.lock);

	return s_uccess;
}
