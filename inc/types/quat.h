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

#pragma once
#include "vec.h"

#define QUAT_FUNC(T)											\
																\
typedef T##x4 Quat##T;											\
																\
/* Simple quaternion functions */								\
																\
Quat##T Quat##T##_create(T x, T y, T z, T w);					\
																\
Quat##T Quat##T##_identity();									\
																\
Quat##T Quat##T##_conj(Quat##T q);								\
Quat##T Quat##T##_normalize(Quat##T q);							\
Quat##T Quat##T##_inverse(Quat##T q);							\
																\
/* Shortcuts */													\
																\
Bool Quat##T##_eq(Quat##T a, Quat##T b);						\
Bool Quat##T##_neq(Quat##T a, Quat##T b);						\
																\
T Quat##T##_x(Quat##T a);										\
T Quat##T##_y(Quat##T a);										\
T Quat##T##_z(Quat##T a);										\
T Quat##T##_w(Quat##T a);										\
T Quat##T##_get(Quat##T a, U8 i);								\
																\
void Quat##T##_setX(Quat##T *a, T v);							\
void Quat##T##_setY(Quat##T *a, T v);							\
void Quat##T##_setZ(Quat##T *a, T v);							\
void Quat##T##_setW(Quat##T *a, T v);							\
void Quat##T##_set(Quat##T *a, U8 i, T v);						\
																\
Quat##T Quat##T##_lerp(Quat##T a, Quat##T b, T perc);			\
																\
/* Helper funcs */												\
																\
Quat##T Quat##T##_angleAxis(T##x4 axis, T angle);				\
Quat##T Quat##T##_fromEuler(T##x4 pitchYawRollDeg);				\
T##x4 Quat##T##_toEuler(Quat##T q);								\
Quat##T Quat##T##_mul(Quat##T a, Quat##T b);					\
Quat##T Quat##T##_targetDirection(T##x4 origin, T##x4 target);	\
T##x4 Quat##T##_applyToNormal(Quat##T R, T##x4 P);				\
																\
Quat##T Quat##T##_slerp(Quat##T a, Quat##T b, T perc)			\
																\
/* Quat##T Quat##T##_fromLookRotation(T##x4 fwd, T##x4 up); */

QUAT_FUNC(F32);
//QUAT_FUNC(F64);		TODO:
