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

#include "types/base/fixed_point.h"

#undef FixedPoint

#define FixedPoint(frac, integ)																\
																							\
typedef U64 FP##integ##f##frac;																\
																							\
FP##integ##f##frac FP##integ##f##frac##_add(FP##integ##f##frac a, FP##integ##f##frac b) {	\
	return a + b;																			\
}																							\
																							\
FP##integ##f##frac FP##integ##f##frac##_sub(FP##integ##f##frac a, FP##integ##f##frac b) {	\
	return a - b;																			\
}																							\
																							\
FP##integ##f##frac FP##integ##f##frac##_fromDouble(F64 v) {									\
																							\
	Bool sign = v < 0;																		\
																							\
	if(sign)																				\
		v = -v;																				\
																							\
	v *= (1 << frac);																		\
	FP##integ##f##frac res = (FP##integ##f##frac)v;											\
																							\
	if(sign) {																				\
		--res;																				\
		res ^= ((FP##integ##f##frac)1 << ((frac + integ) + 1)) - 1;							\
	}																						\
																							\
	return res;																				\
}																							\
																							\
F64 FP##integ##f##frac##_toDouble(FP##integ##f##frac value) {								\
																							\
	if (value >> (frac + integ)) {															\
		value ^= ((FP##integ##f##frac)1 << ((frac + integ) + 1)) - 1;						\
		++value;																			\
		return value * (-1.0 / (1 << frac));												\
	} 																						\
																							\
	return value * (1.0 / (1 << frac)); 													\
}

FixedPoint(4, 37)
FixedPoint(6, 46)
