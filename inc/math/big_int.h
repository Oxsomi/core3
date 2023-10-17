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
#include "math/vec.h"
#include "types/platform_types.h"

#if _PLATFORM_TYPE == EPlatform_Linux
	typedef __int128 U128;
#else
	typedef I32x4 U128;
#endif

U128 U128_create(const U8 data[16]);
U128 U128_mul64(U64 a, U64 b);			//Multiply two 64-bit numbers to generate a 128-bit number
U128 U128_add64(U64 a, U64 b);			//Add two 64-bit numbers but keep the overflow bit

U128 U128_zero();
U128 U128_one();

U128 U128_xor(U128 a, U128 b);
U128 U128_or(U128 a, U128 b);
U128 U128_and(U128 a, U128 b);

//U128 U128_not(U128 a);

Bool U128_eq(U128 a, U128 b);
Bool U128_neq(U128 a, U128 b);
Bool U128_lt(U128 a, U128 b);
Bool U128_leq(U128 a, U128 b);
Bool U128_gt(U128 a, U128 b);
Bool U128_geq(U128 a, U128 b);

//U128 U128_div(U128 a, U128 b);
//U128 U128_mod(U128 a, U128 b);
U128 U128_mul(U128 a, U128 b);
U128 U128_add(U128 a, U128 b);
U128 U128_sub(U128 a, U128 b);
U128 U128_negate(U128 a);
U128 U128_complement(U128 a);

U128 U128_min(U128 a, U128 b);
U128 U128_max(U128 a, U128 b);
U128 U128_clamp(U128 a, U128 mi, U128 ma);
