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

//types/math/mat.h

#pragma once
#include "types/math/quat.h"

#ifdef __cplusplus
	extern "C" {
#endif

//4x4 matrix over T##x4, so every operation is SIMD through the existing vector dispatch
//(SSE / NEON / scalar fallback) without this file knowing which one it got.
//
//CONVENTIONS, these match include/shader_compiler/shaders/types.hlsli, which is where the same helpers live for HLSL,
// so a matrix built here can be uploaded and used as-is:
//
//  Storage      row major. v[i] is row i; v[3] of a translation is (x, y, z, 1).
//               That's the order the HLSL F32x4x4(...) constructor takes its arguments in.
//
//  Vectors      row vectors on the left: p' = p * M, i.e. HLSL's mul(float4, float4x4).
//               T##x4x4_transformPoint / _transformDirection do this.
//
//  Composition  T##x4x4_mul(a, b) applies a first, then b:
//                   p * T##x4x4_mul(a, b) == (p * a) * b
//               so a model matrix reads left to right, mul(scale, mul(rotate, translate)).
//
//  Handedness   left handed, +Z into the screen, matching F32x4x4_perspective / _lookAt in the HLSL.
//  Depth        [0, 1] (D3D style), not [-1, 1] (GL style).

#define MAT_FUNC(T, relErr, absErr)                                                                             \
																												\
typedef struct T##x4x4 {                                                                                        \
	T##x4 v[4];        /* v[i] = row i */                                                                       \
} T##x4x4;                                                                                                      \
																												\
/* Constants */                                                                                                 \
																												\
static inline T##x4x4 T##x4x4_zero() {                                                                          \
	T##x4x4 m;                                                                                                  \
	m.v[0] = m.v[1] = m.v[2] = m.v[3] = T##x4_zero();                                                           \
	return m;                                                                                                   \
}                                                                                                               \
																												\
static inline T##x4x4 T##x4x4_identity() {                                                                      \
	T##x4x4 m;                                                                                                  \
	m.v[0] = T##x4_create4(1, 0, 0, 0);                                                                         \
	m.v[1] = T##x4_create4(0, 1, 0, 0);                                                                         \
	m.v[2] = T##x4_create4(0, 0, 1, 0);                                                                         \
	m.v[3] = T##x4_create4(0, 0, 0, 1);                                                                         \
	return m;                                                                                                   \
}                                                                                                               \
																												\
/* Element access; get/set address a single element as (row, column) */                                         \
																												\
static inline T##x4 T##x4x4_row(T##x4x4 m, U8 i) { return m.v[i & 3]; }                                         \
																												\
static inline T##x4 T##x4x4_column(T##x4x4 m, U8 j) {                                                           \
	return T##x4_create4(                                                                                       \
		T##x4_get(m.v[0], j), T##x4_get(m.v[1], j), T##x4_get(m.v[2], j), T##x4_get(m.v[3], j)                  \
	);                                                                                                          \
}                                                                                                               \
																												\
static inline T T##x4x4_get(T##x4x4 m, U8 row, U8 col) { return T##x4_get(m.v[row & 3], col); }                 \
																												\
static inline void T##x4x4_setRow(T##x4x4 *m, U8 i, T##x4 v) { if(m) m->v[i & 3] = v; }                         \
																												\
static inline void T##x4x4_set(T##x4x4 *m, U8 row, U8 col, T f) {                                               \
	if(m) T##x4_setRef(&m->v[row & 3], col, f);                                                                 \
}                                                                                                               \
																												\
static inline void T##x4x4_setColumn(T##x4x4 *m, U8 j, T##x4 v) {                                               \
	if(!m) return;                                                                                              \
	for(U8 i = 0; i < 4; ++i) T##x4_setRef(&m->v[i], j, T##x4_get(v, i));                                       \
}                                                                                                               \
																												\
/* Comparison; use eqApprox for anything that went through arithmetic */                                        \
																												\
static inline Bool T##x4x4_eq(T##x4x4 a, T##x4x4 b) {                                                           \
	return                                                                                                      \
		T##x4_eqExact4(a.v[0], b.v[0]) && T##x4_eqExact4(a.v[1], b.v[1]) &&                                     \
		T##x4_eqExact4(a.v[2], b.v[2]) && T##x4_eqExact4(a.v[3], b.v[3]);                                       \
}                                                                                                               \
																												\
static inline Bool T##x4x4_neq(T##x4x4 a, T##x4x4 b) { return !T##x4x4_eq(a, b); }                              \
																												\
static inline Bool T##x4x4_eqApproxAdv(T##x4x4 a, T##x4x4 b, T relEpsilon, T absEpsilon) {                      \
	return                                                                                                      \
		T##x4_eqApproxAdv4(a.v[0], b.v[0], relEpsilon, absEpsilon) &&                                           \
		T##x4_eqApproxAdv4(a.v[1], b.v[1], relEpsilon, absEpsilon) &&                                           \
		T##x4_eqApproxAdv4(a.v[2], b.v[2], relEpsilon, absEpsilon) &&                                           \
		T##x4_eqApproxAdv4(a.v[3], b.v[3], relEpsilon, absEpsilon);                                             \
}                                                                                                               \
																												\
static inline Bool T##x4x4_eqApprox(T##x4x4 a, T##x4x4 b) {                                                     \
	return T##x4x4_eqApproxAdv(a, b, relErr, absErr);                                                           \
}                                                                                                               \
																												\
/* Component-wise arithmetic; T##x4x4_mul below is the matrix product */                                        \
																												\
static inline T##x4x4 T##x4x4_add(T##x4x4 a, T##x4x4 b) {                                                       \
	T##x4x4 m = T##x4x4_zero();                                                                                 \
	for(U8 i = 0; i < 4; ++i) m.v[i] = T##x4_add(a.v[i], b.v[i]);                                               \
	return m;                                                                                                   \
}                                                                                                               \
																												\
static inline T##x4x4 T##x4x4_sub(T##x4x4 a, T##x4x4 b) {                                                       \
	T##x4x4 m = T##x4x4_zero();                                                                                 \
	for(U8 i = 0; i < 4; ++i) m.v[i] = T##x4_sub(a.v[i], b.v[i]);                                               \
	return m;                                                                                                   \
}                                                                                                               \
																												\
static inline T##x4x4 T##x4x4_mulScalar(T##x4x4 a, T s) {                                                       \
	const T##x4 sv = T##x4_xxxx4(s);                                                                            \
	T##x4x4 m = T##x4x4_zero();                                                                                 \
	for(U8 i = 0; i < 4; ++i) m.v[i] = T##x4_mul(a.v[i], sv);                                                   \
	return m;                                                                                                   \
}                                                                                                               \
																												\
/* Transpose; the per-SIMD work lives in T##x4_transpose4 (vec4f_sse/neon/none.inc.h) */                        \
																												\
static inline T##x4x4 T##x4x4_transpose(T##x4x4 a) {                                                            \
	T##x4x4 m = T##x4x4_zero();                                                                                 \
	T##x4_transpose4(a.v, m.v);                                                                                 \
	return m;                                                                                                   \
}                                                                                                               \
																												\
/* Transforms. */                                                                                               \
/* A point carries w = 1 so it picks up the translation row, a direction carries w = 0 so it doesn't; */        \
/* that difference is the usual "why is my normal translated" bug, so they're separate calls. */                \
																												\
static inline T##x4 T##x4x4_transform(T##x4x4 m, T##x4 v) {                                                     \
	T##x4 r = T##x4_mul(T##x4_xxxx(v), m.v[0]);                                                                 \
	r = T##x4_fma(T##x4_yyyy(v), m.v[1], r);                                                                    \
	r = T##x4_fma(T##x4_zzzz(v), m.v[2], r);                                                                    \
	r = T##x4_fma(T##x4_wwww(v), m.v[3], r);                                                                    \
	return r;                                                                                                   \
}                                                                                                               \
																												\
static inline T##x4 T##x4x4_transformPoint(T##x4x4 m, T##x4 p) {                                                \
	return T##x4x4_transform(m, T##x4_setWCopy(p, 1));                                                          \
}                                                                                                               \
																												\
static inline T##x4 T##x4x4_transformDirection(T##x4x4 m, T##x4 d) {                                            \
	return T##x4x4_transform(m, T##x4_setWCopy(d, 0));                                                          \
}                                                                                                               \
																												\
/* mul(a, b) applies a first: p * mul(a, b) == (p * a) * b */                                                   \
																												\
static inline T##x4x4 T##x4x4_mul(T##x4x4 a, T##x4x4 b) {                                                       \
	T##x4x4 m = T##x4x4_zero();                                                                                 \
	for(U8 i = 0; i < 4; ++i) m.v[i] = T##x4x4_transform(b, a.v[i]);                                            \
	return m;                                                                                                   \
}                                                                                                               \
																												\
/* Trivial builders; the ones that need trig, a division tree or a basis are in mat.c */                        \
																												\
/* w scales w, matching the HLSL F32x4x4_scale; scale3 passes 1 for it, which is what you almost */             \
/* always want, a 0 there gives you a singular matrix. */                                                       \
																												\
static inline T##x4x4 T##x4x4_scale(T##x4 scale) {                                                              \
	T##x4x4 m = T##x4x4_zero();                                                                                 \
	m.v[0] = T##x4_create4(T##x4_x(scale), 0, 0, 0);                                                            \
	m.v[1] = T##x4_create4(0, T##x4_y(scale), 0, 0);                                                            \
	m.v[2] = T##x4_create4(0, 0, T##x4_z(scale), 0);                                                            \
	m.v[3] = T##x4_create4(0, 0, 0, T##x4_w(scale));                                                            \
	return m;                                                                                                   \
}                                                                                                               \
																												\
static inline T##x4x4 T##x4x4_scale3(T x, T y, T z) { return T##x4x4_scale(T##x4_create4(x, y, z, 1)); }        \
																												\
static inline T##x4x4 T##x4x4_translate(T##x4 translate) {                                                      \
	T##x4x4 m = T##x4x4_identity();                                                                             \
	m.v[3] = T##x4_create4(T##x4_x(translate), T##x4_y(translate), T##x4_z(translate), 1);                      \
	return m;                                                                                                   \
}                                                                                                               \
																												\
static inline T##x4x4 T##x4x4_translate3(T x, T y, T z) {                                                       \
	return T##x4x4_translate(T##x4_create4(x, y, z, 1));                                                        \
}                                                                                                               \
																												\
/* Helper funcs */                                                                                              \
/* Rotations take radians; positive angles rotate counter clockwise looking down the axis to the origin */      \
																												\
T##x4x4 T##x4x4_rotateX(T rad);                                                                                 \
T##x4x4 T##x4x4_rotateY(T rad);                                                                                 \
T##x4x4 T##x4x4_rotateZ(T rad);                                                                                 \
																												\
/* XYZ order, i.e. X applied first; same as the HLSL F32x4x4_rotate */                                          \
T##x4x4 T##x4x4_rotate(T##x4 eulerRad);                                                                         \
T##x4x4 T##x4x4_rotateQuat(Quat##T q);                                                                          \
																												\
/* Scale, then rotate, then translate; the order a model matrix is almost always wanted in */                   \
T##x4x4 T##x4x4_transformSRT(T##x4 scale, T##x4 eulerRad, T##x4 translate);                                     \
																												\
/* Camera: move the world so the camera sits at the origin, then orient it */                                   \
T##x4x4 T##x4x4_view(T##x4 position, T##x4 eulerRad);                                                           \
																												\
/* Projections; left handed with a [0, 1] depth range, matching the HLSL helpers */                             \
																												\
T##x4x4 T##x4x4_perspective(T fovYRad, T aspect, T nearPlane, T farPlane);                                      \
T##x4x4 T##x4x4_ortho(T left, T right, T bottom, T top, T nearPlane, T farPlane);                               \
																												\
static inline T##x4x4 T##x4x4_orthoSize(T width, T height, T nearPlane, T farPlane) {                           \
	return T##x4x4_ortho(-width / 2, width / 2, -height / 2, height / 2, nearPlane, farPlane);                  \
}                                                                                                               \
																												\
/* View matrix from an orthonormal basis plus an eye position */                                                \
T##x4x4 T##x4x4_construct(T##x4 x, T##x4 y, T##x4 z, T##x4 eye);                                                \
T##x4x4 T##x4x4_lookDir(T##x4 eye, T##x4 direction, T##x4 up);                                                  \
T##x4x4 T##x4x4_lookAt(T##x4 eye, T##x4 center, T##x4 up);                                                      \
																												\
T T##x4x4_determinant(T##x4x4 m);                                                                               \
																												\
/* False (and *result untouched) for a singular matrix, rather than filling it with inf/NaN */                  \
Bool T##x4x4_inverse(T##x4x4 m, T##x4x4 *result);                                                               \
																												\
/* Human readable dump into a caller supplied buffer, one row per line. */                                      \
/* OxC3_types_math sits below OxC3_types_container, so it can't reach the logger; format here and hand */       \
/* the result to Log_debugLn (or printf) at the layer that has one. */                                          \
/* Returns the characters written excluding the null terminator, or 0 if it didn't fit. */                      \
U64 T##x4x4_format(T##x4x4 m, C8 *buffer, U64 bufferSize);

MAT_FUNC(F32, 1e-5f, 1e-6f);
//MAT_FUNC(F64);        TODO: needs F64x4 first, same blocker as QUAT_FUNC(F64)

#ifdef __cplusplus
	}
#endif
