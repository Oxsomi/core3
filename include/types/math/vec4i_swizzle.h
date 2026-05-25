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

//types/math/vec4i_swizzle.h

#pragma once
#include "types/math/vec4i.h"
#include "types/math/vec_cvt.h"

#ifdef __cplusplus
	extern "C" {
#endif
		
//4D Swizzles

#define I32x4_expand4(xv, x0, yv, y0, zv, z0, wv, w0)											\
static inline I32x4 I32x4_##xv##yv##zv##wv(I32x4 a) { return vecShufflei(a, x0, y0, z0, w0); }

#define I32x4_expand3(...)																		\
I32x4_expand4(__VA_ARGS__, x, 0); I32x4_expand4(__VA_ARGS__, y, 1);								\
I32x4_expand4(__VA_ARGS__, z, 2); I32x4_expand4(__VA_ARGS__, w, 3);

#define I32x4_expand2(...)																		\
I32x4_expand3(__VA_ARGS__, x, 0); I32x4_expand3(__VA_ARGS__, y, 1);								\
I32x4_expand3(__VA_ARGS__, z, 2); I32x4_expand3(__VA_ARGS__, w, 3);

#define I32x4_expand(...)																		\
I32x4_expand2(__VA_ARGS__, x, 0); I32x4_expand2(__VA_ARGS__, y, 1);								\
I32x4_expand2(__VA_ARGS__, z, 2); I32x4_expand2(__VA_ARGS__, w, 3);

I32x4_expand(x, 0);
I32x4_expand(y, 1);
I32x4_expand(z, 2);
I32x4_expand(w, 3);

//3D swizzles

#define I32x3_expand3(xv, yv, zv)																\
static inline I32x4 I32x4_##xv##yv##zv(I32x4 a) { return I32x4_trunc3(I32x4_##xv##yv##zv##x(a)); }

#define I32x3_expand2(...)																		\
I32x3_expand3(__VA_ARGS__, x); I32x3_expand3(__VA_ARGS__, y);									\
I32x3_expand3(__VA_ARGS__, z); I32x3_expand3(__VA_ARGS__, w);

#define I32x3_expand(...)																		\
I32x3_expand2(__VA_ARGS__, x); I32x3_expand2(__VA_ARGS__, y);									\
I32x3_expand2(__VA_ARGS__, z); I32x3_expand2(__VA_ARGS__, w);

I32x3_expand(x);
I32x3_expand(y);
I32x3_expand(z);
I32x3_expand(w);

//2D swizzles

#define I32x2_expand2(xv, yv)																	\
static inline I32x4 I32x4_##xv##yv##4(I32x4 a) { return I32x4_trunc2(I32x4_##xv##yv##xx(a)); }	\
static inline I32x2 I32x4_##xv##yv(I32x4 a) { return I32x2_fromI32x4(I32x4_##xv##yv##xx(a)); }

#define I32x2_expand(...)																		\
I32x2_expand2(__VA_ARGS__, x); I32x2_expand2(__VA_ARGS__, y);									\
I32x2_expand2(__VA_ARGS__, z); I32x2_expand2(__VA_ARGS__, w);

I32x2_expand(x);
I32x2_expand(y);
I32x2_expand(z);
I32x2_expand(w);

#ifdef __cplusplus
	}
#endif
