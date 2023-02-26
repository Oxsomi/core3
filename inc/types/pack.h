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

#pragma once
#include "math/quat.h"

//U64 packing

U64 U64_pack21x3(U32 x, U32 y, U32 z);
U32 U64_unpack21x3(U64 packed, U8 off);
Bool U64_setPacked21x3(U64 *packed, U8 off, U32 v);

Bool U64_pack20x3u4(U64 *dst, U32 x, U32 y, U32 z, U8 u4);
U32 U64_unpack20x3(U64 packed, U8 off);
Bool U64_setPacked20x3(U64 *packed, U8 off, U32 v);

Bool U64_getBit(U64 packed, U8 off);
Bool U64_setBit(U64 *packed, U8 off, Bool b);

//U32 packing

Bool U32_getBit(U32 packed, U8 off);
Bool U32_setBit(U32 *packed, U8 off, Bool b);

//Compressing quaternions

typedef struct Quat16 {
	I16 arr[4];
} Quat16;

Quat Quat_unpack(Quat16 q);
Quat16 Quat_pack(Quat q);
