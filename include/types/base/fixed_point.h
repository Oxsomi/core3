/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Fixed point

#define FixedPoint(frac, integ)																\
																							\
typedef U64 FP##integ##f##frac;																\
																							\
FP##integ##f##frac FP##integ##f##frac##_add(FP##integ##f##frac a, FP##integ##f##frac b);	\
FP##integ##f##frac FP##integ##f##frac##_sub(FP##integ##f##frac a, FP##integ##f##frac b);	\
																							\
FP##integ##f##frac FP##integ##f##frac##_fromDouble(F64 v);									\
F64 FP##integ##f##frac##_toDouble(FP##integ##f##frac value);

//Fixed point 42 (FP37f4): 4 fract, 37 integer, 1 sign.
//+-1.4M km precision 1/16th cm
//3x F42 < 128 bit (2 bit remainder)
//Can pack 3 in uint4.

FixedPoint(4, 37)

//Fixed point for bigger scale, 53 (FP46f6): 6 fract, 46 integer, 1 sign.
//~700M km (about 4.5au) with precision 1/64th cm.
//Can pack 3 in uint4 + uint.

FixedPoint(6, 46)

#ifdef __cplusplus
	}
#endif
