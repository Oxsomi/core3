/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/error.h"
#include "types/math/math.h"

#define QUAT_IMPL(T, suffix)																							\
																														\
Quat##T Quat##T##_create(T x, T y, T z, T w) { return T##x4_create4(x, y, z, w); }										\
																														\
Quat##T Quat##T##_identity() { return Quat##T##_create(0, 0, 0, 1); }													\
																														\
Quat##T Quat##T##_conj(Quat##T q) { return Quat##T##_create(-T##x4_x(q), -T##x4_y(q), -T##x4_z(q), T##x4_w(q)); }		\
Quat##T Quat##T##_normalize(Quat##T q) { return T##x4_normalize4(q); }													\
Quat##T Quat##T##_inverse(Quat##T q) { return Quat##T##_normalize(Quat##T##_conj(q)); }									\
																														\
Bool Quat##T##_eq(Quat##T a, Quat##T b) { return T##x4_eq4(a, b); }														\
Bool Quat##T##_neq(Quat##T a, Quat##T b) { return T##x4_neq4(a, b); }													\
																														\
T Quat##T##_x(Quat##T a) { return T##x4_x(a); }																			\
T Quat##T##_y(Quat##T a) { return T##x4_y(a); }																			\
T Quat##T##_z(Quat##T a) { return T##x4_z(a); }																			\
T Quat##T##_w(Quat##T a) { return T##x4_w(a); }																			\
T Quat##T##_get(Quat##T a, U8 i) { return T##x4_get(a, i); }															\
																														\
void Quat##T##_setX(Quat##T *a, T v) { T##x4_setX(a, v); }																\
void Quat##T##_setY(Quat##T *a, T v) { T##x4_setY(a, v); }																\
void Quat##T##_setZ(Quat##T *a, T v) { T##x4_setZ(a, v); }																\
void Quat##T##_setW(Quat##T *a, T v) { T##x4_setW(a, v); }																\
void Quat##T##_set(Quat##T *a, U8 i, T v) { T##x4_set(a, i, v); }														\
																														\
Quat##T Quat##T##_lerp(Quat##T a, Quat##T b, T perc) { return T##x4_lerp(a, b, perc); }									\
																														\
/* Rotate normal by quaternion */																						\
/*  https://math.stackexchange.com/questions/40164/how-do-you-rotate-a-vector-by-a-unit-quaternion */					\
																														\
T##x4 Quat##T##_applyToNormal(Quat##T R, T##x4 P) {																		\
	return Quat##T##_mul(Quat##T##_mul(R, P), Quat##T##_conj(R));														\
}																														\
																														\
/* Rotate around an axis with an angle */																				\
/*  https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Definition */							\
																														\
Quat##T Quat##T##_angleAxis(T##x4 axis, T angle) {																		\
																														\
	angle *= 0.5##suffix;																								\
																														\
	T sinA2 = T##_sin(angle);																							\
	T cosA2 = T##_cos(angle);																							\
																														\
	T##x4 cosB = T##x4_cos(axis);																						\
	T##x4 q = T##x4_mul(cosB, T##x4_xxxx4(sinA2));																		\
																														\
	T##x4_setW(&q, cosA2);																								\
	return q;																											\
}																														\
																														\
Quat##T Quat##T##_fromEuler(T##x4 pitchYawRollDeg) {																	\
																														\
	T##x4 pitchYawRollRad = T##x4_mul(pitchYawRollDeg, T##x4_xxxx4(T##_DEG_TO_RAD * .5##suffix));						\
																														\
	T##x4 c = T##x4_cos(pitchYawRollRad);																				\
	T##x4 s = T##x4_sin(pitchYawRollRad);																				\
																														\
	T cp = T##x4_x(c);																									\
	T cy = T##x4_y(c);																									\
	T cr = T##x4_z(c);																									\
																														\
	T sp = T##x4_x(s);																									\
	T sy = T##x4_y(s);																									\
	T sr = T##x4_z(s);																									\
																														\
	T cpsr = sr * cp;																									\
	T cpcr = cr * cp;																									\
	T spsy = sp * sy;																									\
	T spcy = sp * cy;																									\
																														\
	return Quat##T##_create(																							\
		cpsr * cy - cr * spsy,																							\
		cr * spcy + cpsr * sy,																							\
		cpcr * sy - sr * spcy,																							\
		cpcr * cy + sr * spsy																							\
	);																													\
}																														\
																														\
T##x4 Quat##T##_toEuler(Quat##T q) {																					\
																														\
	T##x4 q2	= T##x4_pow2(q);																						\
																														\
	T q2_x		= T##x4_x(q2);																							\
	T q2_y		= T##x4_y(q2);																							\
	T q2_z		= T##x4_z(q2);																							\
																														\
	/* Calculate roll */																								\
																														\
	T##x4 qXzw	= T##x4_mul(q, T##x4_wz4(q));																			\
																														\
	T qXzw_x = T##x4_x(qXzw);																							\
	T qXzw_y = T##x4_y(qXzw);																							\
																														\
	T cpsr = 2 * (qXzw_x + qXzw_y);																						\
	T cpcr = 1 - 2 * (q2_x + q2_y);																						\
	T r = T##_atan2(cpsr, cpcr);																						\
																														\
	/* Calculate pitch */																								\
																														\
	T sp = 2 * (qXzw_y - qXzw_x);																						\
	T p;																												\
																														\
	if (T##_abs(sp) >= 1)		/* 90_deg if out of range */															\
		p = T##_PI * 0.5##suffix * T##_signInc(sp);																		\
																														\
	else p = T##_asin(sp);																								\
																														\
	/* Calculate yaw */																									\
																														\
	T##x4 xzXyw = T##x4_mul(T##x4_xz4(q), T##x4_yw4(q));																\
																														\
	T xzXyw_x = T##x4_x(xzXyw);																							\
	T xzXyw_y = T##x4_y(xzXyw);																							\
																														\
	T siny_cosp = 2 * (xzXyw_y + xzXyw_x);																				\
	T cosy_cosp = 1 - 2 * (q2_y + q2_z);																				\
	T y = T##_atan2(siny_cosp, cosy_cosp);																				\
																														\
	return T##x4_mul(T##x4_create3(p, y, r), T##x4_xxxx4(T##_RAD_TO_DEG));												\
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
	T leno2 = 0, lent2 = 0;																								\
	Error err;																											\
																														\
	if((err = T##_pow2(leno, &leno2)).genericError || (err = T##_pow2(lent, &lent2)).genericError)						\
		return Quat##T##_identity();																					\
																														\
	T w = T##_sqrt(leno2 * lent2) + T##x4_dot3(origin, target);															\
																														\
	T##x4 cross = T##x4_cross3(origin, target);																			\
																														\
	return Quat##T##_normalize(Quat##T##_create(																		\
		T##x4_x(cross), T##x4_y(cross), T##x4_z(cross),																	\
		w																												\
	));																													\
}																														\
																														\
/* Lerp between a and b using percentage (or extrapolate if perc > 1 or < 0) */											\
/*  https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm */						\
 																														\
Quat##T Quat##T##_slerp(Quat##T a, Quat##T b, T perc) {																	\
																														\
	T cosTheta2 = T##x4_dot4(a, b);																						\
																														\
	if (T##_abs(cosTheta2) >= 1)																						\
		return a;																										\
																														\
	T cosTheta2Pow = 0;																									\
																														\
	Error err = T##_pow2(cosTheta2, &cosTheta2Pow);																		\
																														\
	if(err.genericError)																								\
		return b;																										\
																														\
	T halfTheta = T##_acos(cosTheta2);																					\
	T sinTheta2 = T##_sqrt(1 - cosTheta2Pow);																			\
																														\
	if(T##_abs(sinTheta2) < 1e-3##suffix)		/* Theta 180deg isn't defined, so define as 50/50 */					\
		return T##x4_lerp(a, b, .5##suffix);																			\
																														\
	T invSinTheta2 = 1 / sinTheta2;																						\
																														\
	T ratioA = T##_sin(halfTheta * (1 - perc)) * invSinTheta2;															\
	T ratioB = T##_sin(halfTheta * perc) * invSinTheta2; 																\
																														\
	a = T##x4_mul(a, T##x4_xxxx4(ratioA));																				\
	b = T##x4_mul(b, T##x4_xxxx4(ratioB));																				\
																														\
	return T##x4_add(a, b);																								\
}

QUAT_IMPL(F32, f);
//_QUAT_IMPL(F64, );
