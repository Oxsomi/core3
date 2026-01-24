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

#include "types/math/quat.h"
#include "types/math/vec4f_swizzle.h"
#include "types/base/error.h"
#include "types/base/mathf.h"

#define QUAT_IMPL(T, suffix)																							\
																														\
/* Rotate around an axis with an angle */																				\
/*  https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Definition */							\
																														\
Quat##T Quat##T##_angleAxis(T##x4 axis, T angle) {																		\
																														\
	axis = T##x4_normalize3(axis);																						\
																														\
	angle *= 0.5##suffix;																								\
																														\
	T sinA2 = T##_sin(angle);																							\
	T cosA2 = T##_cos(angle);																							\
																														\
	T##x4 q = T##x4_mul(axis, T##x4_xxxx4(sinA2));																		\
																														\
	return T##x4_setWCopy(q, cosA2);																					\
}																														\
																														\
Quat##T Quat##T##_fromEuler(T##x4 eulerXYZDeg) {																		\
																														\
	T##x4 halfRad = T##x4_mul(eulerXYZDeg, T##x4_xxxx4(T##_DEG_TO_RAD * .5##suffix));									\
																														\
	T##x4 c = T##x4_cos(halfRad);																						\
	T##x4 s = T##x4_sin(halfRad);																						\
																														\
	T cx = T##x4_x(c);																									\
	T cy = T##x4_y(c);																									\
	T cz = T##x4_z(c);																									\
																														\
	T sx = T##x4_x(s);																									\
	T sy = T##x4_y(s);																									\
	T sz = T##x4_z(s);																									\
																														\
	return Quat##T##_normalize(Quat##T##_create(																		\
		sx * cy * cz + cx * sy * sz,																					\
		cx * sy * cz - sx * cy * sz,																					\
		cx * cy * sz + sx * sy * cz,																					\
		cx * cy * cz - sx * sy * sz																						\
	));																													\
}																														\
																														\
T##x4 Quat##T##_toEuler(Quat##T q) {																					\
																														\
	T##x4 q2	= T##x4_pow2(q);																						\
																														\
	T qx		= T##x4_x(q);																							\
	T qy		= T##x4_y(q);																							\
	T qz		= T##x4_z(q);																							\
	T qw		= T##x4_w(q);																							\
																														\
	T qx2		= T##x4_x(q2);																							\
	T qy2		= T##x4_y(q2);																							\
	T qz2		= T##x4_z(q2);																							\
																														\
	/* Calculate X-rot */																								\
																														\
	T sinX = 2 * (qw * qx - qy * qz);																					\
	T cosX = 1 - 2 * (qx2 + qy2);																						\
	T x = T##_atan2(sinX, cosX);																						\
																														\
	/* Calculate Y-rot */																								\
																														\
	T sinY = 2 * (qw * qy + qx * qz);																					\
	T y;																												\
																														\
	if (T##_abs(sinY) >= 1)		/* 90_deg if out of range */															\
		y = T##_PI * 0.5##suffix * T##_signInc(sinY);																	\
																														\
	else y = T##_asin(sinY);																							\
																														\
	/* Calculate Z-rot */																								\
																														\
	T sinZ = 2 * (qw * qz - qx * qy);																					\
	T cosZ = 1 - 2 * (qy2 + qz2);																						\
	T z = T##_atan2(sinZ, cosZ);																						\
																														\
	return T##x4_mul(T##x4_create3(x, y, z), T##x4_xxxx4(T##_RAD_TO_DEG));												\
}																														\
																														\
/* Combine two quaternions */																							\
/*  https://stackoverflow.com/questions/19956555/how-to-multiply-two-quaternions */										\
																														\
Quat##T Quat##T##_mul(Quat##T a, Quat##T b) {																			\
																														\
	T##x4 axXb = T##x4_mul(b, T##x4_xxxx(a));																			\
	T##x4 ayXb = T##x4_mul(b, T##x4_yyyy(a));																			\
	T##x4 azXb = T##x4_mul(b, T##x4_zzzz(a));																			\
	T##x4 awXb = T##x4_mul(b, T##x4_wwww(a));																			\
																														\
	T axXb_x = T##x4_x(axXb),	axXb_y = T##x4_y(axXb),	axXb_z = T##x4_z(axXb),	axXb_w = T##x4_w(axXb);					\
	T ayXb_x = T##x4_x(ayXb),	ayXb_y = T##x4_y(ayXb),	ayXb_z = T##x4_z(ayXb),	ayXb_w = T##x4_w(ayXb);					\
	T azXb_x = T##x4_x(azXb),	azXb_y = T##x4_y(azXb),	azXb_z = T##x4_z(azXb),	azXb_w = T##x4_w(azXb);					\
	T awXb_x = T##x4_x(awXb),	awXb_y = T##x4_y(awXb),	awXb_z = T##x4_z(awXb),	awXb_w = T##x4_w(awXb);					\
																														\
	return Quat##T##_create(																							\
		awXb_x + axXb_w + ayXb_z - azXb_y,																				\
		awXb_y - axXb_z + ayXb_w + azXb_x,																				\
		awXb_z + axXb_y - ayXb_x + azXb_w,																				\
		awXb_w - axXb_x - ayXb_y - azXb_z																				\
	);																													\
}																														\
																														\
/* Get shortest arc quaternion from origin to target																	\
*  https://stackoverflow.com/questions/1171849/finding-quaternion-representing-the-rotation-from-one-vector-to-another	\
*/																														\
Quat##T Quat##T##_targetDirection(T##x4 origin, T##x4 target) {															\
																														\
	T leno = T##x4_len3(origin), lent = T##x4_len3(target);																\
																														\
	T leno2 = T##_pow2(leno), lent2 = T##_pow2(lent);																	\
																														\
	if(!T##_isValid(leno2) || !T##_isValid(lent2))																		\
		return Quat##T##_identity();																					\
																														\
	T w = T##_sqrt(leno2 * lent2) + T##x4_dot3(origin, target);															\
																														\
	T##x4 cross = T##x4_cross3(origin, target);																			\
																														\
	Quat##T newQuat = Quat##T##_create(T##x4_x(cross), T##x4_y(cross), T##x4_z(cross), w);								\
	T len = T##x4_len4(newQuat);																						\
																														\
	if (len < 1e-6)																										\
		return Quat##T##_identity();																					\
																														\
	return T##x4_mul(newQuat, T##x4_xxxx4(1.##suffix / len));															\
}																														\
																														\
/* Lerp between a and b using percentage (or extrapolate if perc > 1 or < 0) */											\
/*  https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm */						\
 																														\
Quat##T Quat##T##_slerp(Quat##T a, Quat##T b, T perc) {																	\
																														\
	T cosTheta2 = T##x4_dot4(a, b);																						\
	if (cosTheta2 < 0) {																								\
		b = T##x4_negate(b);																							\
		cosTheta2 = -cosTheta2;																							\
	}																													\
																														\
	if (cosTheta2 >= 1)																									\
		return a;																										\
																														\
	T cosTheta2Pow = T##_pow2(cosTheta2);																				\
																														\
	if(!T##_isValid(cosTheta2Pow)) return b;																			\
																														\
	T halfTheta = T##_acos(cosTheta2);																					\
	T sinTheta2 = T##_sqrt(1 - cosTheta2Pow);																			\
																														\
	if(T##_abs(sinTheta2) < 1e-3##suffix)		/* Theta 180deg isn't defined, so define as 50/50 */					\
		return Quat##T##_normalize(T##x4_lerp(a, b, perc));																\
																														\
	T invSinTheta2 = 1 / sinTheta2;																						\
																														\
	T ratioA = T##_sin(halfTheta * (1 - perc)) * invSinTheta2;															\
	T ratioB = T##_sin(halfTheta * perc) * invSinTheta2; 																\
																														\
	a = T##x4_mul(a, T##x4_xxxx4(ratioA));																				\
	b = T##x4_mul(b, T##x4_xxxx4(ratioB));																				\
																														\
	return Quat##T##_normalize(T##x4_add(a, b));																		\
}

QUAT_IMPL(F32, f);
//_QUAT_IMPL(F64, );
