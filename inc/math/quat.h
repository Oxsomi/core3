#pragma once
#include "vec.h"
#include "math/math.h"

typedef f32x4 quat;

//Simple quaternion functions

inline quat Quat_init(f32 x, f32 y, f32 z, f32 w) { return f32x4_init4(x, y, z, w); }

inline quat Quat_identity() { return Quat_init(0, 0, 0, 1); }

inline quat Quat_conj(quat q) { return Quat_init(-f32x4_x(q), -f32x4_y(q), -f32x4_z(q), f32x4_w(q)); }
inline quat Quat_normalize(quat q) { return f32x4_normalize4(q); }
inline quat Quat_inverse(quat q) { return Quat_normalize(Quat_conj(q)); }

//Shortcuts

inline bool Quat_eq(quat a, quat b) { return f32x4_eq4(a, b); }
inline bool Quat_neq(quat a, quat b) { return f32x4_neq4(a, b); }

inline f32 Quat_x(quat a) { return f32x4_x(a); }
inline f32 Quat_y(quat a) { return f32x4_y(a); }
inline f32 Quat_z(quat a) { return f32x4_z(a); }
inline f32 Quat_w(quat a) { return f32x4_w(a); }
inline f32 Quat_get(quat a, u8 i) { return f32x4_get(a, i); }

inline void Quat_setX(quat *a, f32 v) { f32x4_setX(a, v); }
inline void Quat_setY(quat *a, f32 v) { f32x4_setY(a, v); }
inline void Quat_setZ(quat *a, f32 v) { f32x4_setZ(a, v); }
inline void Quat_setW(quat *a, f32 v) { f32x4_setW(a, v); }
inline void Quat_set(quat *a, u8 i, f32 v) { f32x4_set(a, i, v); }

inline quat Quat_lerp(quat a, quat b, f32 perc) { return f32x4_lerp(a, b, perc); }

//Helper funcs

//Rotate around an axis with an angle
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Definition
//
inline quat Quat_angleAxis(f32x4 axis, f32 angle) {

	angle *= 0.5f;

	f32 sinA2 = Math_sin(angle);
	f32 cosA2 = Math_cos(angle);

	f32x4 cosB = f32x4_cos(axis);
	f32x4 q = f32x4_mul(cosB, f32x4_xxxx4(sinA2));

	f32x4_setW(&q, cosA2);

	return q;
}

//Construct quaternion from euler. Rotation around xyz (pitch, yaw, roll) in degrees
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Euler_angles_to_quaternion_conversion
//
inline quat Quat_fromEuler(f32x4 pitchYawRollDeg) {

	f32x4 pitchYawRollRad = f32x4_mul(pitchYawRollDeg, f32x4_xxxx4(Math_degToRad * .5f));

	f32x4 c = f32x4_cos(pitchYawRollRad);
	f32x4 s = f32x4_sin(pitchYawRollRad);

	f32 cp = f32x4_y(c);
	f32 cy = f32x4_y(c);
	f32 cr = f32x4_z(c);

	f32 sp = f32x4_y(s);
	f32 sy = f32x4_y(s);
	f32 sr = f32x4_z(s);

	f32 cpsr = sr * cp;
	f32 cpcr = cr * cp;
	f32 spsy = sp * sy;
	f32 spcy = sp * cy;

	return Quat_init(
		cpsr * cy - cr * spsy,
		cr * spcy + cpsr * sy,
		cpcr * sy - sr * spcy,
		cpcr * cy + sr * spsy
	);
}

//Convert back to euler, pitchYawRollDeg
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Quaternion_to_Euler_angles_conversion
//
inline f32x4 Quat_toEuler(quat q) {

	f32x4 q2	= f32x4_pow2(q);

	f32 q2_x	= f32x4_x(q2);
	f32 q2_y	= f32x4_y(q2);
	f32 q2_z	= f32x4_z(q2);

	//Calculate roll

	f32x4 qXzw	= f32x4_mul(q, f32x4_wz4(q));

	f32 qXzw_x = f32x4_x(qXzw);
	f32 qXzw_y = f32x4_y(qXzw);

	f32 cpsr = 2 * (qXzw_x + qXzw_y);
	f32 cpcr = 1 - 2 * (q2_x + q2_y);
	f32 r = Math_atan2(cpsr, cpcr);

	//Calculate pitch

	f32 sp = 2 * (qXzw_y - qXzw_x);
	f32 p;

	if (Math_absf(sp) >= 1)		//90_deg if out of range
		p = Math_pi * 0.5f * Math_signInc(sp);

	else p = Math_asin(sp);

	//Calculate yaw

	f32x4 xzXyw = f32x4_mul(f32x4_xz4(q), f32x4_yw4(q));

	f32 xzXyw_x = f32x4_x(xzXyw);
	f32 xzXyw_y = f32x4_y(xzXyw);

	f32 siny_cosp = 2 * (xzXyw_y + xzXyw_x);
	f32 cosy_cosp = 1 - 2 * (q2_y + q2_z);
	f32 y = Math_atan2(siny_cosp, cosy_cosp);

	return f32x4_mul(f32x4_init3(p, y, r), f32x4_xxxx4(Math_radToDeg));
}

//Combine two quaternions
//https://stackoverflow.com/questions/19956555/how-to-multiply-two-quaternions
//
inline quat Quat_mul(quat a, quat b) {

	f32x4 axXb = f32x4_mul(b, f32x4_xxxx(a));
	f32x4 ayXb = f32x4_mul(b, f32x4_yyyy(a));
	f32x4 azXb = f32x4_mul(b, f32x4_zzzz(a));
	f32x4 awXb = f32x4_mul(b, f32x4_wwww(a));

	f32 axXb_x = f32x4_x(axXb),	axXb_y = f32x4_y(axXb),	axXb_z = f32x4_z(axXb),	axXb_w = f32x4_w(axXb);
	f32 ayXb_x = f32x4_x(ayXb),	ayXb_y = f32x4_y(ayXb),	ayXb_z = f32x4_z(ayXb),	ayXb_w = f32x4_w(ayXb);
	f32 azXb_x = f32x4_x(azXb),	azXb_y = f32x4_y(azXb),	azXb_z = f32x4_z(azXb),	azXb_w = f32x4_w(azXb);
	f32 awXb_x = f32x4_x(awXb),	awXb_y = f32x4_y(awXb),	awXb_z = f32x4_z(awXb),	awXb_w = f32x4_w(awXb);
	
	return Quat_init(
		awXb_x + axXb_w + ayXb_z - azXb_y,
		awXb_y - axXb_z + ayXb_w + azXb_x,
		awXb_z + axXb_y - ayXb_x + azXb_w,
		awXb_w - axXb_x - ayXb_y - azXb_z
	);
}

//Get shortest arc quaternion from origin to target
//https://stackoverflow.com/questions/1171849/finding-quaternion-representing-the-rotation-from-one-vector-to-another
//
inline quat Quat_targetDirection(f32x4 origin, f32x4 target) {

	f32 leno = f32x4_len3(origin), lent = f32x4_len3(target);

	f32 w = Math_sqrtf(Math_pow2f(leno) * Math_pow2f(lent)) + f32x4_dot3(origin, target);

	f32x4 cross = f32x4_cross3(origin, target);

	return Quat_normalize(Quat_init(
		f32x4_x(cross), f32x4_y(cross), f32x4_z(cross),
		w
	));
}

//Rotate normal by quaternion
//https://math.stackexchange.com/questions/40164/how-do-you-rotate-a-vector-by-a-unit-quaternion
//
inline f32x4 Quat_applyToNormal(quat R, f32x4 P) {
	return Quat_mul(Quat_mul(R, P), Quat_conj(R));
}

//Lerp between a and b using percentage (or extrapolate if perc > 1 or < 0)
//https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm
//
inline quat Quat_slerp(quat a, quat b, f32 perc) {

	f32 cosTheta2 = f32x4_dot4(a, b);

	if (Math_absf(cosTheta2) >= 1)
		return a;

	f32 halfTheta = Math_acos(cosTheta2);
	f32 sinTheta2 = Math_sqrtf(1 - Math_pow2f(cosTheta2));

	if(Math_absf(sinTheta2) < 1e-3f)	//Theta 180deg isn't defined, so define as 50/50
		return f32x4_lerp(a, b, .5f);

	f32 invSinTheta2 = 1 / sinTheta2;

	f32 ratioA = Math_sin(halfTheta * (1 - perc)) * invSinTheta2;
	f32 ratioB = Math_sin(halfTheta * perc) * invSinTheta2; 

	a = f32x4_mul(a, f32x4_xxxx4(ratioA));
	b = f32x4_mul(b, f32x4_xxxx4(ratioB));

	return f32x4_add(a, b);
}

//inline quat Quat_fromLookRotation(f32x4 fwd, f32x4 up);

//Compressing quaternions

struct quat16 {
	i16 arr[4];
};

inline quat Quat_unpack(struct quat16 q) {
	
	f32x4 v = f32x4_init4(q.arr[0], q.arr[1], q.arr[2], q.arr[3]);

	f32x4 ma = f32x4_xxxx4(i16_MAX);

	return f32x4_div(v, ma);
}

inline struct quat16 Quat_pack(quat q) {

	q = Quat_normalize(q);

	f32x4 ma = f32x4_xxxx4(i16_MAX);

	f32x4 asI16 = f32x4_mul(q, ma);

	return (struct quat16){
		{
			(i16) f32x4_x(asI16),
			(i16) f32x4_y(asI16),
			(i16) f32x4_z(asI16),
			(i16) f32x4_w(asI16)
		}
	};
}