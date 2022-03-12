#pragma once
#include "vec.h"
#include "math_helper.h"

typedef f32x4 quat;

//Simple quaternion functions

inline quat Quat_init(f32 x, f32 y, f32 z, f32 w) { return Vec_init4(x, y, z, w); }

inline quat Quat_identity() { return Quat_init(0, 0, 0, 1); }

inline quat Quat_conj(quat q) { return Quat_init(-Vec_x(q), -Vec_y(q), -Vec_z(q), Vec_w(q)); }
inline quat Quat_normalize(quat q) { return Vec_normalize4(q); }
inline quat Quat_inverse(quat q) { return Quat_normalize(Quat_conj(q)); }

//Shortcuts

inline bool Quat_eq(quat a, quat b) { return Vec_eq(a, b); }
inline bool Quat_neq(quat a, quat b) { return Vec_neq(a, b); }

inline f32 Quat_x(quat a) { return Vec_x(a); }
inline f32 Quat_y(quat a) { return Vec_y(a); }
inline f32 Quat_z(quat a) { return Vec_z(a); }
inline f32 Quat_w(quat a) { return Vec_w(a); }
inline f32 Quat_get(quat a, u8 i) { return Vec_get(a, i); }

inline void Quat_setX(quat *a, f32 v) { Vec_setX(a, v); }
inline void Quat_setY(quat *a, f32 v) { Vec_setY(a, v); }
inline void Quat_setZ(quat *a, f32 v) { Vec_setZ(a, v); }
inline void Quat_setW(quat *a, f32 v) { Vec_setW(a, v); }
inline void Quat_set(quat *a, u8 i, f32 v) { Vec_set(a, i, v); }

inline quat Quat_lerp(quat a, quat b, f32 perc) { return Vec_lerp(a, b, perc); }

//Helper funcs

//Rotate around an axis with an angle
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Definition
//
inline quat Quat_angleAxis(f32x4 axis, f32 angle) {

	angle *= 0.5f;

	f32 sinA2 = Math_sin(angle);
	f32 cosA2 = Math_cos(angle);

	f32x4 cosB = Vec_cos(axis);
	f32x4 q = Vec_mul(cosB, Vec_xxxx4(sinA2));

	Vec_setW(&q, cosA2);

	return q;
}

//Construct quaternion from euler. Rotation around xyz (pitch, yaw, roll) in radians
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Euler_angles_to_quaternion_conversion
//
inline quat Quat_fromEuler(f32x4 pitchYawRollRad) {

	pitchYawRollRad = Vec_mul(pitchYawRollRad, Vec_xxxx4(.5f));

	f32x4 c = Vec_cos(pitchYawRollRad);
	f32x4 s = Vec_sin(pitchYawRollRad);

	f32 cp = Vec_y(c);
	f32 cy = Vec_y(c);
	f32 cr = Vec_z(c);

	f32 sp = Vec_y(s);
	f32 sy = Vec_y(s);
	f32 sr = Vec_z(s);

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

//Convert back to euler, pitchYawRollRad
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Quaternion_to_Euler_angles_conversion
//
inline f32x4 Quat_toEuler(quat q) {

	f32x4 q2	= Vec_pow2(q);

	f32 q2_x	= Vec_x(q2);
	f32 q2_y	= Vec_y(q2);
	f32 q2_z	= Vec_z(q2);

	//Calculate roll

	f32x4 qXzw	= Vec_mul(q, Vec_wz(q));

	f32 qXzw_x = Vec_x(qXzw);
	f32 qXzw_y = Vec_y(qXzw);

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

	f32x4 xzXyw = Vec_mul(Vec_xz(q), Vec_yw(q));

	f32 xzXyw_x = Vec_x(xzXyw);
	f32 xzXyw_y = Vec_y(xzXyw);

	f32 siny_cosp = 2 * (xzXyw_y + xzXyw_x);
	f32 cosy_cosp = 1 - 2 * (q2_y + q2_z);
	f32 y = Math_atan2(siny_cosp, cosy_cosp);

	return Vec_init3(p, y, r);
}

//Combine two quaternions
//https://stackoverflow.com/questions/19956555/how-to-multiply-two-quaternions
//
inline quat Quat_mul(quat a, quat b) {

	f32x4 axXb = Vec_mul(b, Vec_xxxx(a));
	f32x4 ayXb = Vec_mul(b, Vec_yyyy(a));
	f32x4 azXb = Vec_mul(b, Vec_zzzz(a));
	f32x4 awXb = Vec_mul(b, Vec_wwww(a));

	f32 axXb_x = Vec_x(axXb),	axXb_y = Vec_y(axXb),	axXb_z = Vec_z(axXb),	axXb_w = Vec_w(axXb);
	f32 ayXb_x = Vec_x(ayXb),	ayXb_y = Vec_y(ayXb),	ayXb_z = Vec_z(ayXb),	ayXb_w = Vec_w(ayXb);
	f32 azXb_x = Vec_x(azXb),	azXb_y = Vec_y(azXb),	azXb_z = Vec_z(azXb),	azXb_w = Vec_w(azXb);
	f32 awXb_x = Vec_x(awXb),	awXb_y = Vec_y(awXb),	awXb_z = Vec_z(awXb),	awXb_w = Vec_w(awXb);
	
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

	f32 leno = Vec_len3(origin), lent = Vec_len3(target);

	f32 w = Math_sqrtf(Math_pow2f(leno) * Math_pow2f(lent)) + Vec_dot3(origin, target);

	f32x4 cross = Vec_cross3(origin, target);

	return Quat_normalize(Quat_init(
		Vec_x(cross), Vec_y(cross), Vec_z(cross),
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

	f32 cosTheta2 = Vec_dot4(a, b);

	if (Math_absf(cosTheta2) >= 1)
		return a;

	f32 halfTheta = Math_acos(cosTheta2);
	f32 sinTheta2 = Math_sqrtf(1 - Math_pow2f(cosTheta2));

	if(Math_absf(sinTheta2) < 1e-3f)	//Theta 180deg isn't defined, so define as 50/50
		return Vec_lerp(a, b, .5f);

	f32 invSinTheta2 = 1 / sinTheta2;

	f32 ratioA = Math_sin(halfTheta * (1 - perc)) * invSinTheta2;
	f32 ratioB = Math_sin(halfTheta * perc) * invSinTheta2; 

	a = Vec_mul(a, Vec_xxxx4(ratioA));
	b = Vec_mul(b, Vec_xxxx4(ratioB));

	return Vec_add(a, b);
}

//inline quat Quat_fromLookRotation(f32x4 fwd, f32x4 up);

//Compressing quaternions

struct quat16 {
	i16 arr[4];
};

inline quat Quat_unpack(struct quat16 q) {
	
	f32x4 v = Vec_init4(q.arr[0], q.arr[1], q.arr[2], q.arr[3]);

	f32x4 ma = Vec_xxxx4(i16_MAX);

	return Vec_div(v, ma);
}

inline struct quat16 Quat_pack(quat q) {

	q = Quat_normalize(q);

	f32x4 ma = Vec_xxxx4(i16_MAX);

	f32x4 asI16 = Vec_mul(q, ma);

	return (struct quat16){
		{
			(i16) Vec_x(asI16),
			(i16) Vec_y(asI16),
			(i16) Vec_z(asI16),
			(i16) Vec_w(asI16)
		}
	};
}