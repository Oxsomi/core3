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

//types/math/vec4f_swizzle.h

#pragma once
#include "types/math/vec4f.h"
#include "types/math/vec_cvt.h"

#ifdef __cplusplus
	extern "C" {
#endif

//4D Swizzles

#define F32x4_expand4(xv, x0, yv, y0, zv, z0, wv, w0)                                            \
static inline F32x4 F32x4_##xv##yv##zv##wv(F32x4 a) { return vecShufflef(a, x0, y0, z0, w0); }

#define F32x4_expand3(...)                                                                        \
F32x4_expand4(__VA_ARGS__, x, 0); F32x4_expand4(__VA_ARGS__, y, 1);                                \
F32x4_expand4(__VA_ARGS__, z, 2); F32x4_expand4(__VA_ARGS__, w, 3);

#define F32x4_expand2(...)                                                                        \
F32x4_expand3(__VA_ARGS__, x, 0); F32x4_expand3(__VA_ARGS__, y, 1);                                \
F32x4_expand3(__VA_ARGS__, z, 2); F32x4_expand3(__VA_ARGS__, w, 3);

#define F32x4_expand(...)                                                                        \
F32x4_expand2(__VA_ARGS__, x, 0); F32x4_expand2(__VA_ARGS__, y, 1);                                \
F32x4_expand2(__VA_ARGS__, z, 2); F32x4_expand2(__VA_ARGS__, w, 3);

F32x4_expand(x, 0);
F32x4_expand(y, 1);
F32x4_expand(z, 2);
F32x4_expand(w, 3);

//3D swizzles

#define F32x3_expand3(xv, yv, zv)                                                                \
static inline F32x4 F32x4_##xv##yv##zv(F32x4 a) { return F32x4_trunc3(F32x4_##xv##yv##zv##x(a)); }

#define F32x3_expand2(...)                                                                        \
F32x3_expand3(__VA_ARGS__, x); F32x3_expand3(__VA_ARGS__, y);                                     \
F32x3_expand3(__VA_ARGS__, z); F32x3_expand3(__VA_ARGS__, w);

#define F32x3_expand(...)                                                                        \
F32x3_expand2(__VA_ARGS__, x); F32x3_expand2(__VA_ARGS__, y);                                    \
F32x3_expand2(__VA_ARGS__, z); F32x3_expand2(__VA_ARGS__, w);

F32x3_expand(x);
F32x3_expand(y);
F32x3_expand(z);
F32x3_expand(w);

//2D swizzles

#define F32x2_expand2(xv, yv)                                                                    \
static inline F32x4 F32x4_##xv##yv##4(F32x4 a) { return F32x4_trunc2(F32x4_##xv##yv##xx(a)); }    \
static inline F32x2 F32x4_##xv##yv(F32x4 a) { return F32x2_fromF32x4(F32x4_##xv##yv##xx(a)); }

#define F32x2_expand(...)                                                                        \
F32x2_expand2(__VA_ARGS__, x); F32x2_expand2(__VA_ARGS__, y);                                    \
F32x2_expand2(__VA_ARGS__, z); F32x2_expand2(__VA_ARGS__, w);

F32x2_expand(x);
F32x2_expand(y);
F32x2_expand(z);
F32x2_expand(w);

//Functions that require swizzles (cross and matrix ops)

static inline F32x4 F32x4_mul3x3(F32x4 v3, F32x4 v3x3[3]) {
	return F32x4_fma(v3x3[0], F32x4_xxx(v3),
		F32x4_fma(v3x3[1], F32x4_yyy(v3),
			F32x4_mul(v3x3[2], F32x4_zzz(v3))
		)
	);
}

static inline F32x4 F32x4_mul4x4(F32x4 v4, F32x4 v4x4[4]) {
	return F32x4_fma(v4x4[0], F32x4_xxxx(v4),
		F32x4_fma(v4x4[1], F32x4_yyyy(v4),
			F32x4_fma(v4x4[2], F32x4_zzzz(v4),
				F32x4_mul(v4x4[3], F32x4_wwww(v4))
			)
		)
	);
}

static inline F32x4 F32x4_mul3x4(F32x4 v4, F32x4 v3x4[4]) {
	return F32x4_fma(v3x4[0], F32x4_xxx(v4),
		F32x4_fma(v3x4[1], F32x4_yyy(v4),
			F32x4_fma(v3x4[2], F32x4_zzz(v4),
				F32x4_mul(v3x4[3], F32x4_www(v4))
			)
		)
	);
}

static inline F32x4 F32x4_cross3(F32x4 a, F32x4 b) {
	return F32x4_sub(
		F32x4_mul(F32x4_yzx(a), F32x4_zxy(b)),
		F32x4_mul(F32x4_zxy(a), F32x4_yzx(b))
	);
}

#ifdef __cplusplus
	}
#endif
