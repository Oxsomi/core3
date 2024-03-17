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

#pragma once
#include "acceleration_structure.h"
#include "device_buffer.h"

typedef enum EBLASFlag {
	EBLASFlag_None					= 0,
	EBLASFlag_AvoidDuplicateAnyHit	= 1 << 0,		//Don't run the same anyHit twice on the same triangle/AABB
	EBLASFlag_DisableAnyHit			= 1 << 1,		//Force anyHit off for the geometry's triangles/AABBs
	EBLASFlag_Count					= 2
} EBLASFlag;

typedef enum EBLASConstructionType {
	EBLASConstructionType_Geometry,			//Triangles
	EBLASConstructionType_Procedural,		//AABBs
	EBLASConstructionType_Serialized,
	EBLASConstructionType_Count
} EBLASConstructionType;

typedef struct BLAS {

	RTAS base;

	union {

		//If EBLASConstructionType_Geometry
		struct {

			U8 positionFormatId;						//ETextureFormatId: RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
			U8 indexFormatId;							//ETextureFormatId: R16u, R32u or Undefined
			U16 positionBufferStride;					//<= 2048

			U16 positionOffset, padding;

			DeviceData positionBuffer;

			//TODO: For motion vectors: nextPositionBufferDevice, nextPositionBufferCpu

			DeviceData indexBuffer;						//Only if indexFormatId
		};

		//If EBLASConstructionType_AABB
		struct {
			U32 aabbStride, aabbOffset;
			DeviceData aabbBuffer;
		};

		//If EBLASConstructionType_Serialized
		Buffer cpuData;
	};

} BLAS;

typedef RefPtr BLASRef;

#define BLAS_ext(ptr, T) (!ptr ? NULL : (T##BLAS*)(ptr + 1))		//impl
#define BLASRef_ptr(ptr) RefPtr_data(ptr, BLAS)

Error BLASRef_dec(BLASRef **blas);
Error BLASRef_inc(BLASRef *blas);

//Creating BLASes;
//If cpu memory is used:
//	The changes are queued until the graphics device submits the next commands.
//BLAS recording is done before TLAS recording, but after copy commands.
//BLASes that use GPU memory and don't have AssumeDataIsPresent on -
//	have to be manually queued through the buildBLASExt command.
//	This is because generally, this memory is generated through compute or something else.
//	This would only be possible the next frame otherwise, which is terrible usage.
//If the BLAS is deleted before submitting any commands then it won't exist.
//	Submitting an empty/unfinished BLAS to a TLAS will hide the instance.
//	It has to re-create a TLAS if the BLAS is finished to ensure the BLAS is shown.

//Creating BLAS from triangle geometry

Error GraphicsDeviceRef_createBLASExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,	//RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
	U16 positionOffset,					//Offset into first position for first vertex
	ETextureFormatId indexFormat,		//R16u, R32u, Undefined
	U16 positionBufferStride,			//<=2048 and multiple of 2 (if not 32f) or 4 (RGBA32f)
	DeviceData positionBuffer,			//Required
	DeviceData indexBuffer,				//Optional if indexFormat == Undefined
	BLASRef *parent,					//If specified, indicates refit
	CharString name,
	BLASRef **blas
);

Error GraphicsDeviceRef_createBLASUnindexedExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,	//RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
	U16 positionOffset,					//Offset into first position for first vertex
	U16 positionBufferStride,			//<=2048 and multiple of 2 (if not 32f) or 4 (RGBA32f)
	DeviceData positionBuffer,			//Required
	BLASRef *parent,					//If specified, indicates refit
	CharString name,
	BLASRef **blas
);

//Creating BLAS from AABBs

Error GraphicsDeviceRef_createBLASProceduralExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	U32 aabbStride,						//Alignment: 8
	U32 aabbOffset,						//Offset into the aabb array
	DeviceData buffer,					//Required
	BLASRef *parent,					//If specified, indicates refit
	CharString name,
	BLASRef **blas
);

//Creating BLAS from cache
//TODO:
//Error GraphicsDeviceRef_createBLASFromCacheExt(GraphicsDeviceRef *dev, Buffer cache, CharString name, BLASRef **blas);
