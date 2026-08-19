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

//graphics/generic/blas.h

#pragma once
#include "graphics/generic/acceleration_structure.h"
#include "graphics/generic/device_buffer.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EBLASFlag {
	EBLASFlag_None                     = 0,
	EBLASFlag_AvoidDuplicateAnyHit     = 1 << 0,        //Don't run the same anyHit twice on the same triangle/AABB
	EBLASFlag_DisableAnyHit            = 1 << 1,        //Force anyHit off for the geometry's triangles/AABBs
	EBLASFlag_Count                    = 2
} EBLASFlag;

typedef enum EBLASConstructionType {
	EBLASConstructionType_Geometry,            //Triangles
	EBLASConstructionType_Procedural,          //AABBs
	EBLASConstructionType_Serialized,
	EBLASConstructionType_Count
} EBLASConstructionType;

typedef struct BLAS {

	RTAS base;

	union {

		//If EBLASConstructionType_Geometry
		struct {

			U8 positionFormatId;                        //ETextureFormatId: RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
			U8 indexFormatId;                           //ETextureFormatId: R16u, R32u or Undefined
			U16 positionBufferStride;                   //<= 2048

			U16 positionOffset;

			//Element type only; the values it holds are EOMMSpecialIndex, see above.

			U8 ommIndexFormatId;                        //ETextureFormatId: R16u, R32u or Undefined for no OMM
			U8 padding;

			DeviceData positionBuffer;

			DeviceData indexBuffer;                     //Only if indexFormatId

			//Stage 1 opacity micromaps: special indices only, so this is the per triangle index buffer and
			// there is no micromap object to attach yet.

			DeviceData ommIndexBuffer;                  //Only if ommIndexFormatId
		};

		//If EBLASConstructionType_Procedural
		struct {
			U32 aabbStride, aabbOffset;
			DeviceData aabbBuffer;
		};

		//If EBLASConstructionType_Serialized
		Buffer cpuData;
	};

} BLAS;

typedef RefPtr BLASRef;

#define BLAS_ext(ptr, T) (!ptr ? NULL : (T##BLAS*)(ptr + 1))        //impl
#define BLASRef_ptr(ptr) RefPtr_data(ptr, BLAS)

//Creating BLASes;
//Every geometry buffer is a DeviceData, so it always references a DeviceBuffer rather than CPU memory.
//    Feeding one from CPU memory is a two step nicety: GraphicsDeviceRef_createBufferData uploads the bytes
//    into a DeviceBuffer (which needs EDeviceBufferUsage_ASReadExt), and the DeviceData then points at it.
//    Whether that buffer's allocation is device local or CPU backed does not matter here and is not checked.
//    NOTE this is unlike TLAS, which really does stage CPU instances internally via tempInstanceBuffer.
//BLAS recording has to be done manually during command list recording.
//    This is done through the buildBLASExt command which allows both generating BLAS from compute as well.
//If the BLAS is deleted before submitting any commands then it won't exist.
//    Submitting an empty/unfinished BLAS to a TLAS will hide the instance.
//    It has to re-create a TLAS if the BLAS is finished to ensure the BLAS is shown.

//Opacity micromap special indices: the values an OMM index buffer holds when no micromap object is attached.
//Both APIs define these as NEGATIVE signed constants but read the buffer as UNSIGNED, so the element stores
// the two's complement truncated to its own width: with R16u, FullyTransparent is 0xFFFF, with R32u it is
// 0xFFFFFFFF. The helpers below do that truncation so a caller never has to.
//A hit resolves as: FullyTransparent is ignored entirely, FullyOpaque skips anyHit, and both Unknown values
// are treated as non opaque so anyHit still runs.
//Values match VkOpacityMicromapSpecialIndexEXT and D3D12_RAYTRACING_OPACITY_MICROMAP_SPECIAL_INDEX.

typedef enum EOMMSpecialIndex {
	EOMMSpecialIndex_FullyTransparent        = -1,
	EOMMSpecialIndex_FullyOpaque             = -2,
	EOMMSpecialIndex_FullyUnknownTransparent = -3,
	EOMMSpecialIndex_FullyUnknownOpaque      = -4
} EOMMSpecialIndex;

//Pack a special index into the element width the format uses, which is the value to store in the buffer.
//Returns 0 for a format that carries no OMM, since there is no element to write in that case.

U32 EOMMSpecialIndex_pack(EOMMSpecialIndex specialIndex, ETextureFormatId ommIndexFormat);

//Highest index that still means "entry N of a micromap" rather than a special value.
//The special values occupy the TOP of each element's range, so a generator counting upwards has to stop here;
// R16u gives 0xFFFB and R32u gives 0xFFFFFFFB.
//The base offset (added to non special indices by both APIs) is applied AFTER the special check, so it cannot
// rescue an index that already collided with the reserved range.

U32 EOMMIndex_max(ETextureFormatId ommIndexFormat);

//Whether a raw element value read back from the buffer falls in that reserved range.

Bool EOMMIndex_isSpecial(U32 raw, ETextureFormatId ommIndexFormat);

//Creating BLAS from triangle geometry.
//The parameters travel as a struct rather than as a dozen arguments so optional geometry features can be
// added as fields instead of as another entry point.
//Build one with the helpers below rather than by hand: they take the REQUIRED parameters positionally, so a
// forgotten one is still a compile error, and leave the optional fields zeroed.

typedef struct BLASCreateInfo {

	ERTASBuildFlags buildFlags;
	EBLASFlag blasFlags;

	ETextureFormatId positionFormat;    //RGBA16f, RGBA32f, RGBA16s, RG16f, RG32f, RG16s
	ETextureFormatId indexFormat;       //R16u, R32u, Undefined for unindexed

	U16 positionOffset;                 //Offset into first position for first vertex
	U16 positionBufferStride;           //<=2048 and multiple of 2 (if not 32f) or 4 (RGBA32f)

	U32 padding;

	DeviceData positionBuffer;          //Required
	DeviceData indexBuffer;             //Only if indexFormat

	//Stage 1 opacity micromaps (Ext): special index only, no micromap object is attached.
	//Requires indices, so it is only reachable through BLASCreateInfo_indexedWithOmmIndicesExt.

	//R16u or R32u only. R8u is legal on D3D12 and on VK_KHR_opacity_micromap but forbidden by the EXT
	// extension we target, so it is deliberately not offered.

	ETextureFormatId ommIndexFormat;    //R16u, R32u, Undefined for no OMM
	U32 padding1;

	//ONE element PER TRIANGLE, so exactly indexBuffer triangles worth, and every element must be an
	// EOMMSpecialIndex while no micromap object exists to index into.

	DeviceData ommIndexBuffer;          //Only if ommIndexFormat

} BLASCreateInfo;

BLASCreateInfo BLASCreateInfo_indexed(
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer,
	ETextureFormatId indexFormat,
	DeviceData indexBuffer
);

BLASCreateInfo BLASCreateInfo_unindexed(
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	ETextureFormatId positionFormat,
	U16 positionOffset,
	U16 positionBufferStride,
	DeviceData positionBuffer
);

//Opacity micromaps: the SPECIAL INDEX form, which needs no micromap object at all.
//Named for what it takes rather than for OMM in general, because it only fills in the per triangle index
// buffer; it creates nothing and returns nothing but a filled in BLASCreateInfo.
//Linking a built OpacityMicromapRef is a different helper (an OMM ARRAY), not this one.
//The index buffer holds one special index per triangle, so it needs the triangle indices too, which is why it
// extends the indexed form rather than standing on its own.

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
);

Bool GraphicsDeviceRef_createBLASExt(
	GraphicsDeviceRef *dev,
	const BLASCreateInfo *info,
	const CharString *name,
	BLASRef **blas,
	Error *e_rr
);

//Creating BLAS from AABBs

Bool GraphicsDeviceRef_createBLASProceduralExt(
	GraphicsDeviceRef *dev,
	ERTASBuildFlags buildFlags,
	EBLASFlag blasFlags,
	U32 aabbStride,                     //Alignment: 8
	U32 aabbOffset,                     //Offset into the aabb array
	DeviceData buffer,                  //Required
	const CharString *name,
	BLASRef **blas,
	Error *e_rr
);

//Creating BLAS from cache TODO:
//Error GraphicsDeviceRef_createBLASFromCacheExt(GraphicsDeviceRef *dev, Buffer cache, CharString name, BLASRef **blas);

#ifdef __cplusplus
	}
#endif
