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
#include "math/math.h"
#include "error.h"

//Conversions

Error F32_fromBits(U64 v, F32 *res);
Error F64_fromBits(U64 v, F64 *res);

Error I8_fromUInt(U64 v, I8 *res);
Error I8_fromInt(I64 v, I8 *res);
Error I8_fromFloat(F32 v, I8 *res);
Error I8_fromDouble(F64 v, I8 *res);

Error I16_fromUInt(U64 v, I16 *res);
Error I16_fromInt(I64 v, I16 *res);
Error I16_fromFloat(F32 v, I16 *res);
Error I16_fromDouble(F64 v, I16 *res);

Error I32_fromUInt(U64 v, I32 *res);
Error I32_fromInt(I64 v, I32 *res);
Error I32_fromFloat(F32 v, I32 *res);
Error I32_fromDouble(F64 v, I32 *res);

I16 I16_swapEndianness(I16 v);
I32 I32_swapEndianness(I32 v);
I64 I64_swapEndianness(I64 v);
U16 U16_swapEndianness(U16 v);
U32 U32_swapEndianness(U32 v);
U64 U64_swapEndianness(U64 v);

Error I64_fromUInt(U64 v, I64 *res);
Error I64_fromFloat(F32 v, I64 *res);
Error I64_fromDouble(F64 v, I64 *res);

//Cast to uints

Error U8_fromUInt(U64 v, U8 *res);
Error U8_fromInt(I64 v, U8 *res);
Error U8_fromFloat(F32 v, U8 *res);
Error U8_fromDouble(F64 v, U8 *res);

Error U16_fromUInt(U64 v, U16 *res);
Error U16_fromInt(I64 v, U16 *res);
Error U16_fromFloat(F32 v, U16 *res);
Error U16_fromDouble(F64 v, U16 *res);

Error U32_fromUInt(U64 v, U32 *res);
Error U32_fromInt(I64 v, U32 *res);
Error U32_fromFloat(F32 v, U32 *res);
Error U32_fromDouble(F64 v, U32 *res);

Error U64_fromInt(I64 v, U64 *res);
Error U64_fromFloat(F32 v, U64 *res);
Error U64_fromDouble(F64 v, U64 *res);

//Cast to floats

Error F32_fromInt(I64 v, F32 *res);
Error F32_fromUInt(U64 v, F32 *res);
Error F64_fromInt(I64 v, F64 *res);
Error F64_fromUInt(U64 v, F64 *res);
