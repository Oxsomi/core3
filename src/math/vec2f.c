/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "math/vec.h"

F32x2 F32x2_one() { return F32x2_xx2(1); }
F32x2 F32x2_two() { return F32x2_xx2(2); }
F32x2 F32x2_negOne() { return F32x2_xx2(-1); }
F32x2 F32x2_negTwo() { return F32x2_xx2(-2); }

F32x2 F32x2_complement(F32x2 a) { return F32x2_sub(F32x2_one(), a); }
F32x2 F32x2_negate(F32x2 a) { return F32x2_sub(F32x2_zero(), a); }
F32x2 F32x2_inverse(F32x2 a) { return F32x2_div(F32x2_one(), a); }

F32x2 F32x2_pow2(F32x2 a) { return F32x2_mul(a, a); }

F32x2 F32x2_clamp(F32x2 a, F32x2 mi, F32x2 ma) { return F32x2_max(mi, F32x2_min(ma, a)); }
F32x2 F32x2_saturate(F32x2 a) { return F32x2_clamp(a, F32x2_zero(), F32x2_one()); }

F32x2 F32x2_fract(F32x2 v) { return F32x2_sub(v, F32x2_floor(v)); }
F32x2 F32x2_mod(F32x2 v, F32x2 d) { return F32x2_sub(v, F32x2_mul(F32x2_fract(F32x2_div(v, d)), d)); }

F32 F32x2_satDot(F32x2 a, F32x2 b) { return F32_saturate(F32x2_dot(a, b)); }

F32 F32x2_sqLen(F32x2 v) { return F32x2_dot(v, v); }
F32 F32x2_len(F32x2 v) { return F32_sqrt(F32x2_sqLen(v)); }
F32x2 F32x2_normalize(F32x2 v) { return F32x2_mul(v, F32x2_xx2(1 / F32x2_len(v))); }

//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml

F32x2 F32x2_reflect(F32x2 I, F32x2 N) {
	return F32x2_sub(I, F32x2_mul(N, F32x2_xx2(2 * F32x2_dot(N, I))));
}

Bool F32x2_eq2(F32x2 a, F32x2 b) { return F32x2_all(F32x2_eq(a, b)); }
Bool F32x2_neq2(F32x2 a, F32x2 b) { return !F32x2_eq2(a, b); }

F32x2 F32x2_xy(F32x2 a) { return a; }

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

F32x2 F32x2_sign(F32x2 v) {
	return F32x2_add(
		F32x2_mul(F32x2_lt(v, F32x2_zero()), F32x2_two()), 
		F32x2_one()
	); 
}

F32x2 F32x2_abs(F32x2 v) { return F32x2_mul(F32x2_sign(v), v); }

Bool F32x2_all(F32x2 b) { return F32x2_reduce(F32x2_neq(b, F32x2_zero())) == 4; }
Bool F32x2_any(F32x2 b) { return F32x2_reduce(F32x2_neq(b, F32x2_zero())); }

F32x2 F32x2_load1(const F32 *arr) { return arr ? F32x2_create1(*arr) : F32x2_zero(); }
F32x2 F32x2_load2(const F32 *arr) { return arr ? F32x2_create2(*arr, arr[1]) : F32x2_zero(); }

void F32x2_setX(F32x2 *a, F32 v) { if(a) *a = F32x2_create2(v, F32x2_y(*a)); }
void F32x2_setY(F32x2 *a, F32 v) { if(a) *a = F32x2_create2(F32x2_x(*a), v); }

void F32x2_set(F32x2 *a, U8 i, F32 v) {
	switch (i & 1) {
		case 0:		F32x2_setX(a, v);	break;
		default:	F32x2_setY(a, v);
	}
}

F32 F32x2_get(F32x2 a, U8 i) {
	switch (i & 1) {
		case 0:		return F32x2_x(a);
		default:	return F32x2_y(a);
	}
}

F32x2 F32x2_mul2x2(F32x2 v2, F32x2 v2x2[2]) {
	return F32x2_add(
		F32x2_mul(v2x2[0], F32x2_xx(v2)),
		F32x2_mul(v2x2[1], F32x2_yy(v2))
	);
}

F32x2 F32x2_mul2x3(F32x2 v2, F32x2 v2x3[3]) {
	return F32x2_add(F32x2_add(
		F32x2_mul(v2x3[0], F32x2_xx(v2)),
		F32x2_mul(v2x3[1], F32x2_yy(v2))
	), v2x3[2]);
}

//Casts from vec4f

F32x2 F32x2_fromF32x4(F32x4 a) { return F32x2_load2((const F32*) &a); }
F32x4 F32x4_fromF32x2(F32x2 a) { return F32x4_load2((const F32*) &a); }

//Cast from vec2f to vec4

F32x4 F32x2_create2_4(F32x2 a, F32x2 b) { return F32x4_create4(F32x2_x(a), F32x2_y(a), F32x2_x(b), F32x2_y(b)); }

F32x4 F32x4_create2_1_1(F32x2 a, F32 b, F32 c) { return F32x4_create4(F32x2_x(a), F32x2_y(a), b, c); }
F32x4 F32x4_create1_2_1(F32 a, F32x2 b, F32 c) { return F32x4_create4(a, F32x2_x(b), F32x2_y(b), c); }
F32x4 F32x4_create1_1_2(F32 a, F32 b, F32x2 c) { return F32x4_create4(a, b, F32x2_x(c), F32x2_y(c)); }

F32x4 F32x4_create2_1(F32x2 a, F32 b) { return F32x4_create3(F32x2_x(a), F32x2_y(a), b); }
F32x4 F32x4_create1_2(F32 a, F32x2 b) { return F32x4_create3(a, F32x2_x(b), F32x2_y(b)); }
