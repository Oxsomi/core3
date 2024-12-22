/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#ifndef DISALLOW_SB_OXC3_PLATFORMS
	#include "platforms/ext/listx.h"
#endif

#include "types/container/list.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Check docs/oiSB.md for the file spec

typedef enum ESBPrimitive {
	ESBPrimitive_Invalid,
	ESBPrimitive_Float,		//Float, unorm, snorm
	ESBPrimitive_Int,
	ESBPrimitive_UInt
} ESBPrimitive;

typedef enum ESBVector {
	ESBVector_N1,
	ESBVector_N2,
	ESBVector_N3,
	ESBVector_N4
} ESBVector;

typedef enum ESBMatrix {	//How many vectors are stored. So float4x3 in HLSL = F32x3x4 in OxC3
	ESBMatrix_N1,
	ESBMatrix_N2,
	ESBMatrix_N3,
	ESBMatrix_N4
} ESBMatrix;

typedef enum ESBStride {
	ESBStride_X8,			//This one is currently unused, but might be available in the future
	ESBStride_X16,
	ESBStride_X32,
	ESBStride_X64
} ESBStride;

#define ESBType_create(stride, prim, vec, mat) (((mat) << 6) | ((stride) << 4) | ((prim) << 2) | (vec))

typedef enum ESBType {

	ESBType_F16		= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N1),
	ESBType_F16x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N1),
	ESBType_F16x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N1),
	ESBType_F16x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N1),

	ESBType_I16		= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N1),
	ESBType_I16x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N1),
	ESBType_I16x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N1),
	ESBType_I16x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N1),

	ESBType_U16		= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N1),
	ESBType_U16x2	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N1),
	ESBType_U16x3	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N1),
	ESBType_U16x4	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N1),

	ESBType_F32		= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N1),
	ESBType_F32x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N1),
	ESBType_F32x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N1),
	ESBType_F32x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N1),

	ESBType_I32		= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N1),
	ESBType_I32x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N1),
	ESBType_I32x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N1),
	ESBType_I32x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N1),

	ESBType_U32		= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N1),
	ESBType_U32x2	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N1),
	ESBType_U32x3	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N1),
	ESBType_U32x4	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N1),

	ESBType_F64		= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N1),
	ESBType_F64x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N1),
	ESBType_F64x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N1),
	ESBType_F64x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N1),

	ESBType_I64		= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N1),
	ESBType_I64x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N1),
	ESBType_I64x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N1),
	ESBType_I64x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N1),

	ESBType_U64		= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N1),
	ESBType_U64x2	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N1),
	ESBType_U64x3	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N1),
	ESBType_U64x4	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N1),

	ESBType_F16x1x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N2),
	ESBType_F16x2x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N2),
	ESBType_F16x3x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N2),
	ESBType_F16x4x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N2),

	ESBType_I16x1x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N2),
	ESBType_I16x2x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N2),
	ESBType_I16x3x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N2),
	ESBType_I16x4x2	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N2),

	ESBType_U16x1x2	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N2),
	ESBType_U16x2x2	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N2),
	ESBType_U16x3x2	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N2),
	ESBType_U16x4x2	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N2),

	ESBType_F32x1x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N2),
	ESBType_F32x2x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N2),
	ESBType_F32x3x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N2),
	ESBType_F32x4x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N2),

	ESBType_I32x1x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N2),
	ESBType_I32x2x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N2),
	ESBType_I32x3x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N2),
	ESBType_I32x4x2	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N2),

	ESBType_U32x1x2	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N2),
	ESBType_U32x2x2	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N2),
	ESBType_U32x3x2	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N2),
	ESBType_U32x4x2	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N2),

	ESBType_F64x1x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N2),
	ESBType_F64x2x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N2),
	ESBType_F64x3x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N2),
	ESBType_F64x4x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N2),

	ESBType_I64x1x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N2),
	ESBType_I64x2x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N2),
	ESBType_I64x3x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N2),
	ESBType_I64x4x2	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N2),

	ESBType_U64x1x2	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N2),
	ESBType_U64x2x2	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N2),
	ESBType_U64x3x2	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N2),
	ESBType_U64x4x2	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N2),

	ESBType_F16x1x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N3),
	ESBType_F16x2x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N3),
	ESBType_F16x3x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N3),
	ESBType_F16x4x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N3),

	ESBType_I16x1x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N3),
	ESBType_I16x2x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N3),
	ESBType_I16x3x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N3),
	ESBType_I16x4x3	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N3),

	ESBType_U16x1x3	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N3),
	ESBType_U16x2x3	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N3),
	ESBType_U16x3x3	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N3),
	ESBType_U16x4x3	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N3),

	ESBType_F32x1x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N3),
	ESBType_F32x2x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N3),
	ESBType_F32x3x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N3),
	ESBType_F32x4x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N3),

	ESBType_I32x1x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N3),
	ESBType_I32x2x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N3),
	ESBType_I32x3x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N3),
	ESBType_I32x4x3	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N3),

	ESBType_U32x1x3	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N3),
	ESBType_U32x2x3	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N3),
	ESBType_U32x3x3	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N3),
	ESBType_U32x4x3	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N3),

	ESBType_F64x1x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N3),
	ESBType_F64x2x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N3),
	ESBType_F64x3x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N3),
	ESBType_F64x4x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N3),

	ESBType_I64x1x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N3),
	ESBType_I64x2x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N3),
	ESBType_I64x3x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N3),
	ESBType_I64x4x3	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N3),

	ESBType_U64x1x3	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N3),
	ESBType_U64x2x3	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N3),
	ESBType_U64x3x3	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N3),
	ESBType_U64x4x3	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N3),

	ESBType_F16x1x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N4),
	ESBType_F16x2x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N4),
	ESBType_F16x3x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N4),
	ESBType_F16x4x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N4),

	ESBType_I16x1x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N4),
	ESBType_I16x2x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N4),
	ESBType_I16x3x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N4),
	ESBType_I16x4x4	= ESBType_create(ESBStride_X16, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N4),

	ESBType_U16x1x4	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N4),
	ESBType_U16x2x4	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N4),
	ESBType_U16x3x4	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N4),
	ESBType_U16x4x4	= ESBType_create(ESBStride_X16, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N4),

	ESBType_F32x1x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N4),
	ESBType_F32x2x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N4),
	ESBType_F32x3x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N4),
	ESBType_F32x4x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N4),

	ESBType_I32x1x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N4),
	ESBType_I32x2x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N4),
	ESBType_I32x3x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N4),
	ESBType_I32x4x4	= ESBType_create(ESBStride_X32, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N4),

	ESBType_U32x1x4	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N4),
	ESBType_U32x2x4	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N4),
	ESBType_U32x3x4	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N4),
	ESBType_U32x4x4	= ESBType_create(ESBStride_X32, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N4),

	ESBType_F64x1x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N1, ESBMatrix_N4),
	ESBType_F64x2x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N2, ESBMatrix_N4),
	ESBType_F64x3x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N3, ESBMatrix_N4),
	ESBType_F64x4x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Float, ESBVector_N4, ESBMatrix_N4),

	ESBType_I64x1x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N1, ESBMatrix_N4),
	ESBType_I64x2x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N2, ESBMatrix_N4),
	ESBType_I64x3x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N3, ESBMatrix_N4),
	ESBType_I64x4x4	= ESBType_create(ESBStride_X64, ESBPrimitive_Int,   ESBVector_N4, ESBMatrix_N4),

	ESBType_U64x1x4	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N1, ESBMatrix_N4),
	ESBType_U64x2x4	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N2, ESBMatrix_N4),
	ESBType_U64x3x4	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N3, ESBMatrix_N4),
	ESBType_U64x4x4	= ESBType_create(ESBStride_X64, ESBPrimitive_UInt,  ESBVector_N4, ESBMatrix_N4),

} ESBType;

ESBStride ESBType_getStride(ESBType type);
ESBVector ESBType_getVector(ESBType type);
ESBMatrix ESBType_getMatrix(ESBType type);
ESBPrimitive ESBType_getPrimitive(ESBType type);

U8 ESBType_getSize(ESBType type, Bool isPacked);

const C8 *ESBType_name(ESBType type);

typedef struct SBStruct {
	U32 stride;
} SBStruct;

typedef enum ESBVarFlag {
	ESBVarFlag_None				= 0,
	ESBVarFlag_IsUsedVarSPIRV	= 1 << 0,		//Variable is used by shader (SPIRV)
	ESBVarFlag_IsUsedVarDXIL	= 1 << 1,		//Variable is used by shader (DXIL)
	ESBVarFlag_Invalid			= 0xFFFFFFFF << 2
} ESBVarFlag;

typedef struct SBVar {	//Is seen as a U64 and U32 for hashing

	U16 structId;		//If not U16_MAX, ESBType needs to be 0 and this should be a valid struct
	U16 arrayIndex;		//U16_MAX identifies "none"

	U32 offset;

	U8 type;			//ESBType if structId == U16_MAX
	U8 flags;			//ESBStructVarFlag
	U16 parentId;		//root = U16_MAX

} SBVar;

TList(SBStruct);
TList(SBVar);

#ifdef __cplusplus
	}
#endif
