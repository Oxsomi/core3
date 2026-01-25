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
#include "types/math/vec4f_swizzle.h"

#ifdef __cplusplus
	extern "C" {
#endif

#define QUAT_FUNC(T, relErr, absErr)																			\
																												\
typedef T##x4 Quat##T;																							\
																												\
static inline Quat##T Quat##T##_create(T x, T y, T z, T w) { return T##x4_create4(x, y, z, w); }				\
																												\
static inline Quat##T Quat##T##_identity() { return Quat##T##_create(0, 0, 0, 1); }								\
																												\
static inline Quat##T Quat##T##_conj(Quat##T q) {																\
		return Quat##T##_create(-T##x4_x(q), -T##x4_y(q), -T##x4_z(q), T##x4_w(q));								\
}																												\
static inline Quat##T Quat##T##_normalize(Quat##T q) { return T##x4_normalize4(q); }							\
static inline Quat##T Quat##T##_inverse(Quat##T q) { return Quat##T##_normalize(Quat##T##_conj(q)); }			\
																												\
static inline Bool Quat##T##_eqPrecise(Quat##T a, Quat##T b) { return T##x4_eqApprox4(a, b); }					\
static inline Bool Quat##T##_neqPrecise(Quat##T a, Quat##T b) { return T##x4_neqApprox4(a, b); }				\
static inline Bool Quat##T##_eq(Quat##T a, Quat##T b) { return T##x4_eqApproxAdv4(a, b, relErr, absErr); }		\
static inline Bool Quat##T##_neq(Quat##T a, Quat##T b) { return T##x4_neqApproxAdv4(a, b, relErr, absErr); }	\
																												\
static inline T Quat##T##_x(Quat##T a) { return T##x4_x(a); }													\
static inline T Quat##T##_y(Quat##T a) { return T##x4_y(a); }													\
static inline T Quat##T##_z(Quat##T a) { return T##x4_z(a); }													\
static inline T Quat##T##_w(Quat##T a) { return T##x4_w(a); }													\
static inline T Quat##T##_get(Quat##T a, U8 i) { return T##x4_get(a, i); }										\
																												\
static inline void Quat##T##_setX(Quat##T *a, T v) { T##x4_setXRef(a, v); }										\
static inline void Quat##T##_setY(Quat##T *a, T v) { T##x4_setYRef(a, v); }										\
static inline void Quat##T##_setZ(Quat##T *a, T v) { T##x4_setZRef(a, v); }										\
static inline void Quat##T##_setW(Quat##T *a, T v) { T##x4_setWRef(a, v); }										\
static inline void Quat##T##_set(Quat##T *a, U8 i, T v) { T##x4_setRef(a, i, v); }								\
																												\
static inline Quat##T Quat##T##_lerp(Quat##T a, Quat##T b, T perc) { return T##x4_lerp(a, b, perc); }			\
																												\
/* We use a right handed system */																				\
static inline T##x4 Quat##T##_applyToNormal(Quat##T Q, T##x4 P) {												\
	T##x4 qXyz = T##x4_trunc3(Q);																				\
	T##x4 t = T##x4_mul(T##x4_cross3(P, qXyz), T##x4_xxxx4(2));													\
	return T##x4_add(P, T##x4_add(																				\
		T##x4_mul(t, T##x4_xxxx4(T##x4_w(Q))),																	\
		T##x4_cross3(t, qXyz)																					\
	));																											\
}																												\
																												\
/* Helper funcs */																								\
/* For euler angle functions, we use XYZ rotation order */														\
																												\
Quat##T Quat##T##_angleAxis(T##x4 axis, T angle);																\
Quat##T Quat##T##_fromEuler(T##x4 eulerXYZDeg);																	\
T##x4 Quat##T##_toEuler(Quat##T q);																				\
Quat##T Quat##T##_mul(Quat##T a, Quat##T b);																	\
Quat##T Quat##T##_targetDirection(T##x4 origin, T##x4 target, T##x4 up);										\
Quat##T Quat##T##_fromOrientation(T##x4 right, T##x4 up, T##x4 fwd);											\
																												\
Quat##T Quat##T##_slerp(Quat##T a, Quat##T b, T perc);															\
																												\
static inline T##x4 Quat##T##_forward(Quat##T q) { return Quat##T##_applyToNormal(q, T##x4_create3(0, 0, 1)); }	\
static inline T##x4 Quat##T##_up(Quat##T q) { return Quat##T##_applyToNormal(q, T##x4_create3(0, 1, 0)); }		\
static inline T##x4 Quat##T##_right(Quat##T q) { return Quat##T##_applyToNormal(q, T##x4_create3(1, 0, 0)); }	\
																												\
static inline void Quat##T##_toOrientation(Quat##T q, T##x4 *fwd, T##x4 *up, T##x4 *right) {					\
	if (fwd) *fwd = Quat##T##_forward(q);																		\
	if (up) *up = Quat##T##_up(q);																				\
	if (right) *right = Quat##T##_right(q);																		\
}

/* Quat##T Quat##T##_fromLookRotation(T##x4 fwd, T##x4 up); */

QUAT_FUNC(F32, 2e-4f, 2e-2f);
//QUAT_FUNC(F64);		TODO:

#ifdef __cplusplus
	}
#endif
