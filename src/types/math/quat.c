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
Quat##T Quat##T##_targetDirection(T##x4 origin, T##x4 target, T##x4 up) {												\
																														\
	/* fwd */																											\
	T##x4 forward = T##x4_sub(target, origin);																			\
	T forwardLen = T##x4_len3(forward);																					\
	if (forwardLen < 1e-6##suffix)																						\
		return Quat##T##_identity();																					\
																														\
	T invForward = 1.0##suffix / forwardLen;																			\
	forward = T##x4_mul(forward, T##x4_xxxx4(invForward));																\
																														\
	/* right */																											\
	T##x4 right = T##x4_cross3(up, forward);																			\
	T rightLen = T##x4_len3(right);																						\
	if (rightLen < 1e-6##suffix) {																						\
		T##x4 tempUp = T##x4_create3(0, 1, 0);																			\
		right = T##x4_cross3(tempUp, forward);																			\
		rightLen = T##x4_len3(right);																					\
		if (rightLen < 1e-6##suffix)																					\
			right = T##x4_create3(1, 0, 0);																				\
	}																													\
	T invRight = 1.0##suffix / rightLen;																				\
	right = T##x4_mul(right, T##x4_xxxx4(invRight));																	\
																														\
	/* up */																											\
	T##x4 trueUp = T##x4_cross3(forward, right);																		\
	return Quat##T##_fromOrientation(right, trueUp, forward);															\
}																														\
																														\
Quat##T Quat##T##_fromOrientation(T##x4 right, T##x4 trueUp, T##x4 forward) {											\
																														\
	/* Compute the trace of the rotation matrix */																		\
	T trace = T##x4_x(right) + T##x4_y(trueUp) + T##x4_z(forward);														\
																														\
	T x, y, z, w;																										\
																														\
	/* Case 1: trace positive */																						\
	if (trace > 0) {																									\
		T s = T##_sqrt(trace + 1.0##suffix) * 2.0##suffix;																\
																														\
		/* Off-diagonal differences for column-major matrix */															\
		x = T##x4_y(forward) - T##x4_z(trueUp);  /* m21 - m12 */														\
		y = T##x4_z(right) - T##x4_x(forward); /* m02 - m20 */															\
		z = T##x4_x(trueUp) - T##x4_y(right);   /* m10 - m01 */															\
		x /= s; y /= s; z /= s;																							\
		w = 0.25##suffix * s;																							\
	}																													\
	/* Case 2: largest diagonal is m00 (right.x) */																		\
	else if (T##x4_x(right) > T##x4_y(trueUp) && T##x4_x(right) > T##x4_z(forward)) {									\
		T s = T##_sqrt(1.0##suffix + T##x4_x(right) - T##x4_y(trueUp) - T##x4_z(forward)) * 2.0##suffix;				\
		T invS = 1.0##suffix / s;																						\
																														\
		x = 0.25##suffix * s;																							\
		y = (T##x4_x(trueUp) + T##x4_y(right)) * invS;																	\
		z = (T##x4_x(forward) + T##x4_z(right)) * invS;																	\
		w = (T##x4_y(forward) - T##x4_z(trueUp)) * invS;																\
	}																													\
	/* Case 3: largest diagonal is m11 (trueUp.y) */																	\
	else if (T##x4_y(trueUp) > T##x4_z(forward)) {																		\
		T s = T##_sqrt(1.0##suffix + T##x4_y(trueUp) - T##x4_x(right) - T##x4_z(forward)) * 2.0##suffix;				\
		T invS = 1.0##suffix / s;																						\
																														\
		x = (T##x4_y(right) + T##x4_x(trueUp)) * invS;																	\
		y = 0.25##suffix * s;																							\
		z = (T##x4_z(forward) + T##x4_y(trueUp)) * invS;																\
		w = (T##x4_z(right) - T##x4_x(forward)) * invS;																	\
	}																													\
	/* Case 4: largest diagonal is m22 (forward.z) */																	\
	else {																												\
		T s = T##_sqrt(1.0##suffix + T##x4_z(forward) - T##x4_x(right) - T##x4_y(trueUp)) * 2.0##suffix;				\
		T invS = 1.0##suffix / s;																						\
																														\
		x = (T##x4_x(forward) + T##x4_z(right)) * invS;																	\
		y = (T##x4_y(forward) + T##x4_z(trueUp)) * invS;																\
		z = 0.25##suffix * s;																							\
		w = (T##x4_y(right) - T##x4_x(trueUp)) * invS;																	\
	}																													\
																														\
	/* Normalize the quaternion and return */																			\
	return Quat##T##_normalize(Quat##T##_create(x, y, z, w));															\
}																														\
																														\
/* Lerp between a and b using percentage (or extrapolate if perc > 1 or < 0) */											\
/*  https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm */						\
 																														\
Quat##T Quat##T##_slerp(Quat##T a, Quat##T b, T perc) {																	\
																														\
	a = Quat##T##_normalize(a);																							\
	b = Quat##T##_normalize(b);																							\
																														\
	T cosTheta = T##x4_dot4(a, b);																						\
	if (cosTheta < 0) {																									\
		b = T##x4_negate(b);																							\
		cosTheta = -cosTheta;																							\
	}																													\
																														\
	if (cosTheta >= 1)																									\
		return a;																										\
																														\
	T theta = T##_acos(cosTheta);																						\
	T sinTheta2 = T##_sqrt(1 - cosTheta * cosTheta);																	\
																														\
	if(T##_abs(sinTheta2) > 1 - 5e-4)																					\
		return Quat##T##_normalize(T##x4_lerp(a, b, perc));																\
																														\
	T invSinTheta2 = 1 / sinTheta2;																						\
																														\
	T ratioA = T##_sin(theta * (1 - perc)) * invSinTheta2;																\
	T ratioB = T##_sin(theta * perc) * invSinTheta2; 																	\
																														\
	a = T##x4_mul(a, T##x4_xxxx4(ratioA));																				\
	b = T##x4_mul(b, T##x4_xxxx4(ratioB));																				\
																														\
	return Quat##T##_normalize(T##x4_add(a, b));																		\
}

QUAT_IMPL(F32, f);

//_QUAT_IMPL(F64, );
