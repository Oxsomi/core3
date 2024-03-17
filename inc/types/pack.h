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
#include "types/quat.h"

//U64 packing

U64 U64_pack21x3(U32 x, U32 y, U32 z);
U32 U64_unpack21x3(U64 packed, U8 off);
Bool U64_setPacked21x3(U64 *packed, U8 off, U32 v);

Bool U64_pack20x3u4(U64 *dst, U32 x, U32 y, U32 z, U8 u4);
U32 U64_unpack20x3(U64 packed, U8 off);
Bool U64_setPacked20x3(U64 *packed, U8 off, U32 v);

#define GET_BIT_OP(T)								\
Bool T##_getBit(T packed, U8 off);					\
Bool T##_setBit(T *packed, U8 off, Bool b)

GET_BIT_OP(U64);
GET_BIT_OP(U32);
GET_BIT_OP(U16);
GET_BIT_OP(U8);

//Compressing quaternions

typedef struct QuatS16 {
	I16 arr[4];
} QuatS16;

QuatF32 QuatF32_unpack(QuatS16 q);
QuatS16 QuatF32_pack(QuatF32 q);
