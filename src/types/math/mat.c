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

//types/math/mat.c

#include "types/math/mat.h"
#include "types/math/vec4f_swizzle.h"
#include "types/base/mathf.h"

#include <stdio.h>

#define MAT_IMPL(T, suffix)                                                                                             \
																														\
/* Rotations. Laid out to match F32x4x4_rotateX/Y/Z in shader_compiler/shaders/types.hlsli. */                          \
																														\
T##x4x4 T##x4x4_rotateX(T rad) {                                                                                        \
	const T c = T##_cos(rad), s = T##_sin(rad);                                                                         \
	T##x4x4 m = T##x4x4_identity();                                                                                     \
	m.v[1] = T##x4_create4(0,  c, s, 0);                                                                                \
	m.v[2] = T##x4_create4(0, -s, c, 0);                                                                                \
	return m;                                                                                                           \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_rotateY(T rad) {                                                                                        \
	const T c = T##_cos(rad), s = T##_sin(rad);                                                                         \
	T##x4x4 m = T##x4x4_identity();                                                                                     \
	m.v[0] = T##x4_create4(c, 0, -s, 0);                                                                                \
	m.v[2] = T##x4_create4(s, 0,  c, 0);                                                                                \
	return m;                                                                                                           \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_rotateZ(T rad) {                                                                                        \
	const T c = T##_cos(rad), s = T##_sin(rad);                                                                         \
	T##x4x4 m = T##x4x4_identity();                                                                                     \
	m.v[0] = T##x4_create4( c, s, 0, 0);                                                                                \
	m.v[1] = T##x4_create4(-s, c, 0, 0);                                                                                \
	return m;                                                                                                           \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_rotate(T##x4 eulerRad) {                                                                                \
	return T##x4x4_mul(                                                                                                 \
		T##x4x4_rotateX(T##x4_x(eulerRad)),                                                                             \
		T##x4x4_mul(T##x4x4_rotateY(T##x4_y(eulerRad)), T##x4x4_rotateZ(T##x4_z(eulerRad)))                             \
	);                                                                                                                  \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_rotateQuat(Quat##T q) {                                                                                 \
																														\
	T##x4x4 m = T##x4x4_identity();                                                                                     \
																														\
	/* Each basis vector is the quaternion applied to the corresponding axis */                                         \
	m.v[0] = T##x4_setWCopy(Quat##T##_applyToNormal(q, T##x4_create4(1, 0, 0, 0)), 0);                                  \
	m.v[1] = T##x4_setWCopy(Quat##T##_applyToNormal(q, T##x4_create4(0, 1, 0, 0)), 0);                                  \
	m.v[2] = T##x4_setWCopy(Quat##T##_applyToNormal(q, T##x4_create4(0, 0, 1, 0)), 0);                                  \
	return m;                                                                                                           \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_transformSRT(T##x4 scale, T##x4 eulerRad, T##x4 translate) {                                            \
	return T##x4x4_mul(                                                                                                 \
		T##x4x4_scale(scale),                                                                                           \
		T##x4x4_mul(T##x4x4_rotate(eulerRad), T##x4x4_translate(translate))                                             \
	);                                                                                                                  \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_view(T##x4 position, T##x4 eulerRad) {                                                                  \
	return T##x4x4_mul(T##x4x4_translate(T##x4_negate(position)), T##x4x4_rotate(eulerRad));                            \
}                                                                                                                       \
																														\
/* Projections */                                                                                                       \
																														\
T##x4x4 T##x4x4_perspective(T fovYRad, T aspect, T nearPlane, T farPlane) {                                             \
																														\
	const T scale = 1 / T##_tan(fovYRad / 2);                                                                           \
	const T range = farPlane - nearPlane;                                                                               \
																														\
	/* Reverse Z: near lands on depth 1, far on 0, matching the engine's Gt compare and clear-to-0 */                   \
																														\
	T##x4x4 m = T##x4x4_zero();                                                                                         \
	m.v[0] = T##x4_create4(scale / aspect, 0, 0, 0);                                                                    \
	m.v[1] = T##x4_create4(0, scale, 0, 0);                                                                             \
	m.v[2] = T##x4_create4(0, 0, -nearPlane / range, 1);                                                                \
	m.v[3] = T##x4_create4(0, 0, nearPlane * farPlane / range, 0);                                                      \
	return m;                                                                                                           \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_ortho(T left, T right, T bottom, T top, T nearPlane, T farPlane) {                                      \
																														\
	const T width = right - left;                                                                                       \
	const T height = top - bottom;                                                                                      \
	const T range = farPlane - nearPlane;                                                                               \
																														\
	/* Reverse Z, same convention as perspective: near lands on depth 1, far on 0 */                                    \
																														\
	T##x4x4 m = T##x4x4_zero();                                                                                         \
	m.v[0] = T##x4_create4(2 / width, 0, 0, 0);                                                                         \
	m.v[1] = T##x4_create4(0, 2 / height, 0, 0);                                                                        \
	m.v[2] = T##x4_create4(0, 0, -1 / range, 0);                                                                        \
	m.v[3] = T##x4_create4(                                                                                             \
		-(right + left) / width, -(top + bottom) / height, farPlane / range, 1                                          \
	);                                                                                                                  \
	return m;                                                                                                           \
}                                                                                                                       \
																														\
/* The last row is the eye projected onto the basis, which is what makes it the inverse of the */                       \
/* camera's own transform. */                                                                                           \
																														\
T##x4x4 T##x4x4_construct(T##x4 x, T##x4 y, T##x4 z, T##x4 eye) {                                                       \
	T##x4x4 m;                                                                                                          \
	m.v[0] = T##x4_create4(T##x4_x(x), T##x4_x(y), T##x4_x(z), 0);                                                      \
	m.v[1] = T##x4_create4(T##x4_y(x), T##x4_y(y), T##x4_y(z), 0);                                                      \
	m.v[2] = T##x4_create4(T##x4_z(x), T##x4_z(y), T##x4_z(z), 0);                                                      \
	m.v[3] = T##x4_create4(-T##x4_dot3(x, eye), -T##x4_dot3(y, eye), -T##x4_dot3(z, eye), 1);                           \
	return m;                                                                                                           \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_lookDir(T##x4 eye, T##x4 direction, T##x4 up) {                                                         \
	const T##x4 z = T##x4_normalize3(direction);                                                                        \
	const T##x4 x = T##x4_normalize3(T##x4_cross3(T##x4_normalize3(up), z));                                            \
	const T##x4 y = T##x4_cross3(z, x);                                                                                 \
	return T##x4x4_construct(x, y, z, eye);                                                                             \
}                                                                                                                       \
																														\
T##x4x4 T##x4x4_lookAt(T##x4 eye, T##x4 center, T##x4 up) {                                                             \
	return T##x4x4_lookDir(eye, T##x4_sub(center, eye), up);                                                            \
}                                                                                                                       \
																														\
/* Determinant and inverse. Cofactor expansion, sharing the six 2x2 minors between them; same structure */              \
/* as the HLSL inverseSlow, but reading the elements out once up front instead of per term. */                          \
																														\
T T##x4x4_determinant(T##x4x4 m) {                                                                                      \
																														\
	const T a00 = T##x4_x(m.v[0]), a01 = T##x4_y(m.v[0]), a02 = T##x4_z(m.v[0]), a03 = T##x4_w(m.v[0]);                 \
	const T a10 = T##x4_x(m.v[1]), a11 = T##x4_y(m.v[1]), a12 = T##x4_z(m.v[1]), a13 = T##x4_w(m.v[1]);                 \
	const T a20 = T##x4_x(m.v[2]), a21 = T##x4_y(m.v[2]), a22 = T##x4_z(m.v[2]), a23 = T##x4_w(m.v[2]);                 \
	const T a30 = T##x4_x(m.v[3]), a31 = T##x4_y(m.v[3]), a32 = T##x4_z(m.v[3]), a33 = T##x4_w(m.v[3]);                 \
																														\
	const T b00 = a20 * a31 - a21 * a30;                                                                                \
	const T b01 = a20 * a32 - a22 * a30;                                                                                \
	const T b02 = a20 * a33 - a23 * a30;                                                                                \
	const T b03 = a21 * a32 - a22 * a31;                                                                                \
	const T b04 = a21 * a33 - a23 * a31;                                                                                \
	const T b05 = a22 * a33 - a23 * a32;                                                                                \
																														\
	return                                                                                                              \
		(a00 * a11 - a01 * a10) * b05 - (a00 * a12 - a02 * a10) * b04 + (a00 * a13 - a03 * a10) * b03 +                 \
		(a01 * a12 - a02 * a11) * b02 - (a01 * a13 - a03 * a11) * b01 + (a02 * a13 - a03 * a12) * b00;                  \
}                                                                                                                       \
																														\
Bool T##x4x4_inverse(T##x4x4 m, T##x4x4 *result) {                                                                      \
																														\
	if(!result)                                                                                                         \
		return false;                                                                                                   \
																														\
	const T a00 = T##x4_x(m.v[0]), a01 = T##x4_y(m.v[0]), a02 = T##x4_z(m.v[0]), a03 = T##x4_w(m.v[0]);                 \
	const T a10 = T##x4_x(m.v[1]), a11 = T##x4_y(m.v[1]), a12 = T##x4_z(m.v[1]), a13 = T##x4_w(m.v[1]);                 \
	const T a20 = T##x4_x(m.v[2]), a21 = T##x4_y(m.v[2]), a22 = T##x4_z(m.v[2]), a23 = T##x4_w(m.v[2]);                 \
	const T a30 = T##x4_x(m.v[3]), a31 = T##x4_y(m.v[3]), a32 = T##x4_z(m.v[3]), a33 = T##x4_w(m.v[3]);                 \
																														\
	const T s0 = a00 * a11 - a01 * a10;                                                                                 \
	const T s1 = a00 * a12 - a02 * a10;                                                                                 \
	const T s2 = a00 * a13 - a03 * a10;                                                                                 \
	const T s3 = a01 * a12 - a02 * a11;                                                                                 \
	const T s4 = a01 * a13 - a03 * a11;                                                                                 \
	const T s5 = a02 * a13 - a03 * a12;                                                                                 \
																														\
	const T c5 = a22 * a33 - a23 * a32;                                                                                 \
	const T c4 = a21 * a33 - a23 * a31;                                                                                 \
	const T c3 = a21 * a32 - a22 * a31;                                                                                 \
	const T c2 = a20 * a33 - a23 * a30;                                                                                 \
	const T c1 = a20 * a32 - a22 * a30;                                                                                 \
	const T c0 = a20 * a31 - a21 * a30;                                                                                 \
																														\
	const T det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;                                            \
																														\
	if(!T##_isValid(det) || det == 0)                                                                                   \
		return false;                                                                                                   \
																														\
	const T id = 1 / det;                                                                                               \
	T##x4x4 r;                                                                                                          \
																														\
	r.v[0] = T##x4_create4(                                                                                             \
		( a11 * c5 - a12 * c4 + a13 * c3) * id, (-a01 * c5 + a02 * c4 - a03 * c3) * id,                                 \
		( a31 * s5 - a32 * s4 + a33 * s3) * id, (-a21 * s5 + a22 * s4 - a23 * s3) * id                                  \
	);                                                                                                                  \
																														\
	r.v[1] = T##x4_create4(                                                                                             \
		(-a10 * c5 + a12 * c2 - a13 * c1) * id, ( a00 * c5 - a02 * c2 + a03 * c1) * id,                                 \
		(-a30 * s5 + a32 * s2 - a33 * s1) * id, ( a20 * s5 - a22 * s2 + a23 * s1) * id                                  \
	);                                                                                                                  \
																														\
	r.v[2] = T##x4_create4(                                                                                             \
		( a10 * c4 - a11 * c2 + a13 * c0) * id, (-a00 * c4 + a01 * c2 - a03 * c0) * id,                                 \
		( a30 * s4 - a31 * s2 + a33 * s0) * id, (-a20 * s4 + a21 * s2 - a23 * s0) * id                                  \
	);                                                                                                                  \
																														\
	r.v[3] = T##x4_create4(                                                                                             \
		(-a10 * c3 + a11 * c1 - a12 * c0) * id, ( a00 * c3 - a01 * c1 + a02 * c0) * id,                                 \
		(-a30 * s3 + a31 * s1 - a32 * s0) * id, ( a20 * s3 - a21 * s1 + a22 * s0) * id                                  \
	);                                                                                                                  \
																														\
	*result = r;                                                                                                        \
	return true;                                                                                                        \
}

MAT_IMPL(F32, f);
//MAT_IMPL(F64, );        TODO: see MAT_FUNC(F64) in mat.h

//Written out rather than macro generated:
// the conversion specifier and the (double) promotion snprintf needs are tied to the float type,
// so there's nothing generic left to share.

U64 F32x4x4_format(F32x4x4 m, C8 *buffer, U64 bufferSize) {

	if(!buffer || !bufferSize)
		return 0;

	buffer[0] = '\0';

	U64 written = 0;

	for (U8 i = 0; i < 4; ++i) {

		const int n = snprintf(
			buffer + written, (size_t)(bufferSize - written),
			"[ %10.4f %10.4f %10.4f %10.4f ]\n",
			(double) F32x4_x(m.v[i]), (double) F32x4_y(m.v[i]),
			(double) F32x4_z(m.v[i]), (double) F32x4_w(m.v[i])
		);

		//snprintf returns what it *would* have written, so >= the space left means it got truncated
		if(n < 0 || (U64) n >= bufferSize - written) {
			buffer[written] = '\0';
			return 0;
		}

		written += (U64) n;
	}

	return written;
}
