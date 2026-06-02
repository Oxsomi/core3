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

//formats/oiSB/sb_variable.h

#pragma once
#include "types/container/list.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Check docs/oiSB.md for the file spec

typedef enum ESBPrimitive {
	ESBPrimitive_Invalid,
	ESBPrimitive_Float,        //Float, unorm, snorm
	ESBPrimitive_Int,
	ESBPrimitive_UInt
} ESBPrimitive;

typedef enum ESBVector {
	ESBVector_N1,
	ESBVector_N2,
	ESBVector_N3,
	ESBVector_N4
} ESBVector;

typedef enum ESBMatrix {       //How many vectors are stored. So float4x3 in HLSL = F32x3x4 in OxC3
	ESBMatrix_N1,
	ESBMatrix_N2,
	ESBMatrix_N3,
	ESBMatrix_N4
} ESBMatrix;

typedef enum ESBStride {
	ESBStride_X8,              //This one is currently unused, but might be available in the future
	ESBStride_X16,
	ESBStride_X32,
	ESBStride_X64
} ESBStride;

#define ESBType_create(stride, prim, vec, mat) (((mat) << 6) | ((stride) << 4) | ((prim) << 2) | (vec))
#define ESBType_createN(stride, prim, name, H, suffix, suffix1)                                              \
	ESBType_##name##suffix1##suffix  = ESBType_create(stride, prim, ESBVector_N1, H),                        \
	ESBType_##name##x2##suffix       = ESBType_create(stride, prim, ESBVector_N2, H),                        \
	ESBType_##name##x3##suffix       = ESBType_create(stride, prim, ESBVector_N3, H),                        \
	ESBType_##name##x4##suffix       = ESBType_create(stride, prim, ESBVector_N4, H)

#define ESBType_createVec(dim, suffix0, suffix1)                                                             \
	ESBType_createN(ESBStride_X16, ESBPrimitive_Float, F16, dim, suffix0, suffix1),                          \
	ESBType_createN(ESBStride_X16, ESBPrimitive_Int, I16, dim, suffix0, suffix1),                            \
	ESBType_createN(ESBStride_X16, ESBPrimitive_UInt, U16, dim, suffix0, suffix1),                           \
																											 \
	ESBType_createN(ESBStride_X32, ESBPrimitive_Float, F32, dim, suffix0, suffix1),                          \
	ESBType_createN(ESBStride_X32, ESBPrimitive_Int, I32, dim, suffix0, suffix1),                            \
	ESBType_createN(ESBStride_X32, ESBPrimitive_UInt, U32, dim, suffix0, suffix1),                           \
																											 \
	ESBType_createN(ESBStride_X64, ESBPrimitive_Float, F64, dim, suffix0, suffix1),                          \
	ESBType_createN(ESBStride_X64, ESBPrimitive_Int, I64, dim, suffix0, suffix1),                            \
	ESBType_createN(ESBStride_X64, ESBPrimitive_UInt, U64, dim, suffix0, suffix1)

typedef enum ESBType {
	ESBType_createVec(ESBMatrix_N1,   ,   ),
	ESBType_createVec(ESBMatrix_N2, x2, x1),
	ESBType_createVec(ESBMatrix_N3, x3, x1),
	ESBType_createVec(ESBMatrix_N4, x4, x1)
} ESBType;

static inline ESBVector ESBType_getVector(ESBType type) {
	return type & 3;
}

static inline ESBMatrix ESBType_getMatrix(ESBType type) {
	return (type >> 6) & 3;
}

static inline ESBPrimitive ESBType_getPrimitive(ESBType type) {
	return (type >> 2) & 3;
}

static inline ESBStride ESBType_getStride(ESBType type) {
	return (type >> 4) & 3;
}

static inline U8 ESBType_getSize(ESBType type, Bool isPacked) {

	U8 primitiveSize = 1 << ESBType_getStride(type);
	U8 w = (U8)ESBType_getVector(type) + 1;
	U8 h = (U8)ESBType_getMatrix(type) + 1;

	if (isPacked)
		return primitiveSize * w * h;

	U8 realStride = w * primitiveSize;
	U8 stride = (realStride + 15) & ~15;
	return stride * (h - 1) + realStride;
}

const C8 *ESBType_name(ESBType type);

typedef struct SBStruct {
	U32 stride;
} SBStruct;

typedef enum ESBVarFlag {
	ESBVarFlag_None                = 0,
	ESBVarFlag_IsUsedVarSPIRV      = 1 << 0,        //Variable is used by shader (SPIRV)
	ESBVarFlag_IsUsedVarDXIL       = 1 << 1,        //Variable is used by shader (DXIL)
	ESBVarFlag_Invalid             = 0xFFFFFFFF << 2
} ESBVarFlag;

typedef struct SBVar {        //Is seen as a U32[3] for hashing

	U16 structId;             //If not U16_MAX, ESBType needs to be 0 and this should be a valid struct
	U16 arrayDimOrArrayId;    //<= 32767: array dimension, otherwise arrayId (& 0x7FFF)

	U32 offset;

	U8 type;                  //ESBType if structId == U16_MAX
	U8 flags;                 //ESBStructVarFlag
	U16 parentId;             //root = U16_MAX

} SBVar;

TList(SBStruct);
TList(SBVar);

#ifdef __cplusplus
	}
#endif
