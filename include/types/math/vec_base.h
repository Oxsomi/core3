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

#define NONE_OP_SELF_T(T, N, ...)			\
											\
	T res = { 0 };							\
											\
	for (U8 i = 0; i < N; ++i)				\
		T##_setRef(&res, i, (__VA_ARGS__));	\
											\
	return res

//Helper function to insert a simple non SIMD operation
//Useful if there's no SIMD function that's faster than native (or if fallback is used)

#define NONE_OP2I(...) NONE_OP_SELF_T(I32x2, 2, __VA_ARGS__)
#define NONE_OP2F(...) NONE_OP_SELF_T(F32x2, 2, __VA_ARGS__)
#define NONE_OP4I(...) NONE_OP_SELF_T(I32x4, 4, __VA_ARGS__)
#define NONE_OP4F(...) NONE_OP_SELF_T(F32x4, 4, __VA_ARGS__)
