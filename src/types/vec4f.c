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

#include "types/math.h"
#include "types/vec.h"

F32x4 F32x4_complement(F32x4 a) { return F32x4_sub(F32x4_one(), a); }
F32x4 F32x4_negate(F32x4 a) { return F32x4_sub(F32x4_zero(), a); }
F32x4 F32x4_inverse(F32x4 a) { return F32x4_div(F32x4_one(), a); }

F32x4 F32x4_pow2(F32x4 a) { return F32x4_mul(a, a); }

F32x4 F32x4_normalize2(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen2(v)))); }
F32x4 F32x4_normalize3(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen3(v)))); }
F32x4 F32x4_normalize4(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen4(v)))); }

F32 F32x4_sqLen2(F32x4 v) { return F32x4_dot2(v, v); }
F32 F32x4_sqLen3(F32x4 v) { return F32x4_dot3(v, v); }
F32 F32x4_sqLen4(F32x4 v) { return F32x4_dot4(v, v); }

F32 F32x4_len2(F32x4 v) { return F32_sqrt(F32x4_sqLen2(v)); }
F32 F32x4_len3(F32x4 v) { return F32_sqrt(F32x4_sqLen3(v)); }
F32 F32x4_len4(F32x4 v) { return F32_sqrt(F32x4_sqLen4(v)); }

F32x4 F32x4_clamp(F32x4 a, F32x4 mi, F32x4 ma) { return F32x4_max(mi, F32x4_min(ma, a)); }
F32x4 F32x4_saturate(F32x4 a) { return F32x4_clamp(a, F32x4_zero(), F32x4_one()); }

F32 F32x4_satDot2(F32x4 x, F32x4 y) { return F32_saturate(F32x4_dot2(x, y)); }
F32 F32x4_satDot3(F32x4 x, F32x4 y) { return F32_saturate(F32x4_dot3(x, y)); }
F32 F32x4_satDot4(F32x4 x, F32x4 y) { return F32_saturate(F32x4_dot4(x, y)); }

F32x4 F32x4_fract(F32x4 v) { return F32x4_sub(v, F32x4_floor(v)); }
F32x4 F32x4_mod(F32x4 v, F32x4 d) { return F32x4_mul(F32x4_fract(F32x4_div(v, d)), d); }

//4D Swizzles

#undef F32x4_expand4
#undef F32x4_expand3
#undef F32x4_expand2
#undef F32x4_expand

#define F32x4_expand4(xv, x0, yv, y0, zv, z0, wv, w0)							\
F32x4 F32x4_##xv##yv##zv##wv(F32x4 a) { return vecShufflef(a, x0, y0, z0, w0); }

#define F32x4_expand3(...)												\
F32x4_expand4(__VA_ARGS__, x, 0); F32x4_expand4(__VA_ARGS__, y, 1);	\
F32x4_expand4(__VA_ARGS__, z, 2); F32x4_expand4(__VA_ARGS__, w, 3);

#define F32x4_expand2(...)												\
F32x4_expand3(__VA_ARGS__, x, 0); F32x4_expand3(__VA_ARGS__, y, 1);	\
F32x4_expand3(__VA_ARGS__, z, 2); F32x4_expand3(__VA_ARGS__, w, 3);

#define F32x4_expand(...)												\
F32x4_expand2(__VA_ARGS__, x, 0); F32x4_expand2(__VA_ARGS__, y, 1);	\
F32x4_expand2(__VA_ARGS__, z, 2); F32x4_expand2(__VA_ARGS__, w, 3);

F32x4_expand(x, 0);
F32x4_expand(y, 1);
F32x4_expand(z, 2);
F32x4_expand(w, 3);

//3D swizzles

#undef F32x3_expand3
#undef F32x3_expand2
#undef F32x3_expand

#define F32x3_expand3(xv, yv, zv) F32x4 F32x4_##xv##yv##zv(F32x4 a) { return F32x4_trunc3(F32x4_##xv##yv##zv##x(a)); }

#define F32x3_expand2(...)										\
F32x3_expand3(__VA_ARGS__, x); F32x3_expand3(__VA_ARGS__, y); \
F32x3_expand3(__VA_ARGS__, z); F32x3_expand3(__VA_ARGS__, w);

#define F32x3_expand(...)										\
F32x3_expand2(__VA_ARGS__, x); F32x3_expand2(__VA_ARGS__, y); \
F32x3_expand2(__VA_ARGS__, z); F32x3_expand2(__VA_ARGS__, w);

F32x3_expand(x);
F32x3_expand(y);
F32x3_expand(z);
F32x3_expand(w);

//2D swizzles

#undef F32x2_expand2
#undef F32x2_expand

#define F32x2_expand2(xv, yv)														\
F32x4 F32x4_##xv##yv##4(F32x4 a) { return F32x4_trunc2(F32x4_##xv##yv##xx(a)); }	\
F32x2 F32x4_##xv##yv(F32x4 a) { return F32x2_fromF32x4(F32x4_##xv##yv##xx(a)); }

#define F32x2_expand(...)										\
F32x2_expand2(__VA_ARGS__, x); F32x2_expand2(__VA_ARGS__, y); \
F32x2_expand2(__VA_ARGS__, z); F32x2_expand2(__VA_ARGS__, w);

F32x2_expand(x);
F32x2_expand(y);
F32x2_expand(z);
F32x2_expand(w);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

F32x4 F32x4_sign(F32x4 v) {
	return F32x4_add(
		F32x4_mul(F32x4_lt(v, F32x4_zero()), F32x4_two()),
		F32x4_one()
	);
}

F32x4 F32x4_cross3(F32x4 a, F32x4 b) {
	return F32x4_normalize3(
		F32x4_sub(
			F32x4_mul(F32x4_yzx(a), F32x4_zxy(b)),
			F32x4_mul(F32x4_zxy(a), F32x4_yzx(b))
		)
	);
}

F32x4 F32x4_lerp(F32x4 a, F32x4 b, F32 perc) {
	b = F32x4_sub(b, a);
	return F32x4_add(a, F32x4_mul(b, F32x4_xxxx4(perc)));
}

F32x4 F32x4_abs(F32x4 v) { return F32x4_mul(F32x4_sign(v), v); }

//Shifts are hard, so we just shift by division and floor

F32x4 F32x4_rgb8Unpack(U32 v) {
	F32x4 rgb8 = F32x4_floor(F32x4_div(F32x4_xxxx4((F32)v), F32x4_create3(0x10000, 0x100, 0x1)));
	return F32x4_div(F32x4_floor(F32x4_mod(rgb8, F32x4_xxxx4(0x100))), F32x4_xxxx4(0xFF));
}

U32 F32x4_rgb8Pack(F32x4 v) {
	F32x4 v8 = F32x4_floor(F32x4_mul(v, F32x4_xxxx4(0xFF)));
	F32x4 preShift = F32x4_mul(v8, F32x4_create3(0x10000, 0x100, 0x1));
	return (U32) F32x4_reduce(preShift);
}

F32x4 F32x4_srgba8Unpack(U32 v) {
	return F32x4_pow2(F32x4_rgb8Unpack(v << 8 >> 8));
}

U32 F32x4_srgba8Pack(F32x4 v) {
	return F32x4_rgb8Pack(F32x4_sqrt(v)) | 0xFF000000;
}

F32x4 F32x4_one() { return F32x4_xxxx4(1); }
F32x4 F32x4_two() { return F32x4_xxxx4(2); }
F32x4 F32x4_negOne() { return F32x4_xxxx4(-1); }
F32x4 F32x4_negTwo() { return F32x4_xxxx4(-2); }

Bool F32x4_all(F32x4 a) { return F32x4_reduce(F32x4_neq(a, F32x4_zero())) == 4; }
Bool F32x4_any(F32x4 a) { return F32x4_reduce(F32x4_neq(a, F32x4_zero())); }

F32x4 F32x4_load1(const F32 *arr) { return arr ? F32x4_create1(*arr) : F32x4_zero(); }
F32x4 F32x4_load2(const F32 *arr) { return arr ? F32x4_create2(*arr, arr[1]) : F32x4_zero(); }
F32x4 F32x4_load3(const F32 *arr) { return arr ? F32x4_create3(*arr, arr[1], arr[2]) : F32x4_zero(); }
F32x4 F32x4_load4(const F32 *arr) { return arr ? *(const F32x4*) arr : F32x4_zero(); }

void F32x4_setX(F32x4 *a, F32 v) { if(a) *a = F32x4_create4(v,					F32x4_y(*a),	F32x4_z(*a),	F32x4_w(*a)); }
void F32x4_setY(F32x4 *a, F32 v) { if(a) *a = F32x4_create4(F32x4_x(*a),		v,				F32x4_z(*a),	F32x4_w(*a)); }
void F32x4_setZ(F32x4 *a, F32 v) { if(a) *a = F32x4_create4(F32x4_x(*a),		F32x4_y(*a),	v,				F32x4_w(*a)); }
void F32x4_setW(F32x4 *a, F32 v) { if(a) *a = F32x4_create4(F32x4_x(*a),		F32x4_y(*a),	F32x4_z(*a),	v); }

void F32x4_set(F32x4 *a, U8 i, F32 v) {
	switch (i & 3) {
		case 0:		F32x4_setX(a, v);	break;
		case 1:		F32x4_setY(a, v);	break;
		case 2:		F32x4_setZ(a, v);	break;
		default:	F32x4_setW(a, v);
	}
}

F32 F32x4_get(F32x4 a, U8 i) {
	switch (i & 3) {
		case 0:		return F32x4_x(a);
		case 1:		return F32x4_y(a);
		case 2:		return F32x4_z(a);
		default:	return F32x4_w(a);
	}
}

F32x4 F32x4_mul3x3(F32x4 v3, F32x4 v3x3[3]) {
	return F32x4_add(
		F32x4_add(
			F32x4_mul(v3x3[0], F32x4_xxx(v3)),
			F32x4_mul(v3x3[1], F32x4_yyy(v3))
		),
		F32x4_mul(v3x3[2], F32x4_zzz(v3))
	);
}

F32x4 F32x4_mul3x4(F32x4 v3, F32x4 v3x4[4]) {
	return F32x4_add(F32x4_add(
		F32x4_add(
			F32x4_mul(v3x4[0], F32x4_xxx(v3)),
			F32x4_mul(v3x4[1], F32x4_yyy(v3))
		),
		F32x4_mul(v3x4[2], F32x4_zzz(v3))
	), v3x4[3]);
}

//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml

F32x4 F32x4_reflect2(F32x4 i, F32x4 n) {
	return F32x4_sub(i, F32x4_mul(n, F32x4_xxxx4(2 * F32x4_dot2(n, i))));
}

F32x4 F32x4_reflect3(F32x4 i, F32x4 n) {
	return F32x4_sub(i, F32x4_mul(n, F32x4_xxxx4(2 * F32x4_dot3(n, i))));
}

Bool F32x4_eq4(F32x4 a, F32x4 b) { return F32x4_all(F32x4_eq(a, b)); }
Bool F32x4_neq4(F32x4 a, F32x4 b) { return !F32x4_eq4(a, b); }
