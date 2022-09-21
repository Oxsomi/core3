#pragma once
#include "vec.h"
#include "math/math.h"

typedef F32x4 Quat;

//Simple quaternion functions

inline Quat Quat_create(F32 x, F32 y, F32 z, F32 w) { return Vec_create4(x, y, z, w); }

inline Quat Quat_identity() { return Quat_create(0, 0, 0, 1); }

inline Quat Quat_conj(Quat q) { return Quat_create(-Vec_x(q), -Vec_y(q), -Vec_z(q), Vec_w(q)); }
inline Quat Quat_normalize(Quat q) { return Vec_normalize4(q); }
inline Quat Quat_inverse(Quat q) { return Quat_normalize(Quat_conj(q)); }

//Shortcuts

inline Bool Quat_eq(Quat a, Quat b) { return Vec_eq4(a, b); }
inline Bool Quat_neq(Quat a, Quat b) { return Vec_neq4(a, b); }

inline F32 Quat_x(Quat a) { return Vec_x(a); }
inline F32 Quat_y(Quat a) { return Vec_y(a); }
inline F32 Quat_z(Quat a) { return Vec_z(a); }
inline F32 Quat_w(Quat a) { return Vec_w(a); }
inline F32 Quat_get(Quat a, U8 i) { return Vec_get(a, i); }

inline void Quat_setX(Quat *a, F32 v) { Vec_setX(a, v); }
inline void Quat_setY(Quat *a, F32 v) { Vec_setY(a, v); }
inline void Quat_setZ(Quat *a, F32 v) { Vec_setZ(a, v); }
inline void Quat_setW(Quat *a, F32 v) { Vec_setW(a, v); }
inline void Quat_set(Quat *a, U8 i, F32 v) { Vec_set(a, i, v); }

inline Quat Quat_lerp(Quat a, Quat b, F32 perc) { return Vec_lerp(a, b, perc); }

//Helper funcs

//Rotate around an axis with an angle
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Definition
//
inline Quat Quat_angleAxis(F32x4 axis, F32 angle) {

	angle *= 0.5f;

	F32 sinA2 = Math_sin(angle);
	F32 cosA2 = Math_cos(angle);

	F32x4 cosB = Vec_cos(axis);
	F32x4 q = Vec_mul(cosB, Vec_xxxx4(sinA2));

	Vec_setW(&q, cosA2);

	return q;
}

//Construct quaternion from euler. Rotation around xyz (pitch, yaw, roll) in degrees
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Euler_angles_to_quaternion_conversion
//
inline Quat Quat_fromEuler(F32x4 pitchYawRollDeg) {

	F32x4 pitchYawRollRad = Vec_mul(pitchYawRollDeg, Vec_xxxx4(Math_degToRad * .5f));

	F32x4 c = Vec_cos(pitchYawRollRad);
	F32x4 s = Vec_sin(pitchYawRollRad);

	F32 cp = Vec_y(c);
	F32 cy = Vec_y(c);
	F32 cr = Vec_z(c);

	F32 sp = Vec_y(s);
	F32 sy = Vec_y(s);
	F32 sr = Vec_z(s);

	F32 cpsr = sr * cp;
	F32 cpcr = cr * cp;
	F32 spsy = sp * sy;
	F32 spcy = sp * cy;

	return Quat_create(
		cpsr * cy - cr * spsy,
		cr * spcy + cpsr * sy,
		cpcr * sy - sr * spcy,
		cpcr * cy + sr * spsy
	);
}

//Convert back to euler, pitchYawRollDeg
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Quaternion_to_Euler_angles_conversion
//
inline F32x4 Quat_toEuler(Quat q) {

	F32x4 q2	= Vec_pow2(q);

	F32 q2_x	= Vec_x(q2);
	F32 q2_y	= Vec_y(q2);
	F32 q2_z	= Vec_z(q2);

	//Calculate roll

	F32x4 qXzw	= Vec_mul(q, Vec_wz(q));

	F32 qXzw_x = Vec_x(qXzw);
	F32 qXzw_y = Vec_y(qXzw);

	F32 cpsr = 2 * (qXzw_x + qXzw_y);
	F32 cpcr = 1 - 2 * (q2_x + q2_y);
	F32 r = Math_atan2(cpsr, cpcr);

	//Calculate pitch

	F32 sp = 2 * (qXzw_y - qXzw_x);
	F32 p;

	if (Math_absf(sp) >= 1)		//90_deg if out of range
		p = Math_pi * 0.5f * Math_signInc(sp);

	else p = Math_asin(sp);

	//Calculate yaw

	F32x4 xzXyw = Vec_mul(Vec_xz(q), Vec_yw(q));

	F32 xzXyw_x = Vec_x(xzXyw);
	F32 xzXyw_y = Vec_y(xzXyw);

	F32 siny_cosp = 2 * (xzXyw_y + xzXyw_x);
	F32 cosy_cosp = 1 - 2 * (q2_y + q2_z);
	F32 y = Math_atan2(siny_cosp, cosy_cosp);

	return Vec_mul(Vec_create3(p, y, r), Vec_xxxx4(Math_radToDeg));
}

//Combine two quaternions
//https://stackoverflow.com/questions/19956555/how-to-multiply-two-quaternions
//
inline Quat Quat_mul(Quat a, Quat b) {

	F32x4 axXb = Vec_mul(b, Vec_xxxx(a));
	F32x4 ayXb = Vec_mul(b, Vec_yyyy(a));
	F32x4 azXb = Vec_mul(b, Vec_zzzz(a));
	F32x4 awXb = Vec_mul(b, Vec_wwww(a));

	F32 axXb_x = Vec_x(axXb),	axXb_y = Vec_y(axXb),	axXb_z = Vec_z(axXb),	axXb_w = Vec_w(axXb);
	F32 ayXb_x = Vec_x(ayXb),	ayXb_y = Vec_y(ayXb),	ayXb_z = Vec_z(ayXb),	ayXb_w = Vec_w(ayXb);
	F32 azXb_x = Vec_x(azXb),	azXb_y = Vec_y(azXb),	azXb_z = Vec_z(azXb),	azXb_w = Vec_w(azXb);
	F32 awXb_x = Vec_x(awXb),	awXb_y = Vec_y(awXb),	awXb_z = Vec_z(awXb),	awXb_w = Vec_w(awXb);
	
	return Quat_create(
		awXb_x + axXb_w + ayXb_z - azXb_y,
		awXb_y - axXb_z + ayXb_w + azXb_x,
		awXb_z + axXb_y - ayXb_x + azXb_w,
		awXb_w - axXb_x - ayXb_y - azXb_z
	);
}

//Get shortest arc quaternion from origin to target
//https://stackoverflow.com/questions/1171849/finding-quaternion-representing-the-rotation-from-one-vector-to-another
//
inline Quat Quat_targetDirection(F32x4 origin, F32x4 target) {

	F32 leno = Vec_len3(origin), lent = Vec_len3(target);

	F32 w = Math_sqrtf(Math_pow2f(leno) * Math_pow2f(lent)) + Vec_dot3(origin, target);

	F32x4 cross = Vec_cross3(origin, target);

	return Quat_normalize(Quat_create(
		Vec_x(cross), Vec_y(cross), Vec_z(cross),
		w
	));
}

//Rotate normal by quaternion
//https://math.stackexchange.com/questions/40164/how-do-you-rotate-a-vector-by-a-unit-quaternion
//
inline F32x4 Quat_applyToNormal(Quat R, F32x4 P) {
	return Quat_mul(Quat_mul(R, P), Quat_conj(R));
}

//Lerp between a and b using percentage (or extrapolate if perc > 1 or < 0)
//https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm
//
inline Quat Quat_slerp(Quat a, Quat b, F32 perc) {

	F32 cosTheta2 = Vec_dot4(a, b);

	if (Math_absf(cosTheta2) >= 1)
		return a;

	F32 halfTheta = Math_acos(cosTheta2);
	F32 sinTheta2 = Math_sqrtf(1 - Math_pow2f(cosTheta2));

	if(Math_absf(sinTheta2) < 1e-3f)	//Theta 180deg isn't defined, so define as 50/50
		return Vec_lerp(a, b, .5f);

	F32 invSinTheta2 = 1 / sinTheta2;

	F32 ratioA = Math_sin(halfTheta * (1 - perc)) * invSinTheta2;
	F32 ratioB = Math_sin(halfTheta * perc) * invSinTheta2; 

	a = Vec_mul(a, Vec_xxxx4(ratioA));
	b = Vec_mul(b, Vec_xxxx4(ratioB));

	return Vec_add(a, b);
}

//inline Quat Quat_fromLookRotation(F32x4 fwd, F32x4 up);

//Compressing quaternions

struct Quat16 {
	I16 arr[4];
};

inline Quat Quat_unpack(struct Quat16 q) {
	
	F32x4 v = Vec_create4(q.arr[0], q.arr[1], q.arr[2], q.arr[3]);

	F32x4 ma = Vec_xxxx4(I16_MAX);

	return Vec_div(v, ma);
}

inline struct Quat16 Quat_pack(Quat q) {

	q = Quat_normalize(q);

	F32x4 ma = Vec_xxxx4(I16_MAX);

	F32x4 asI16 = Vec_mul(q, ma);

	return (struct Quat16){
		{
			(I16) Vec_x(asI16),
			(I16) Vec_y(asI16),
			(I16) Vec_z(asI16),
			(I16) Vec_w(asI16)
		}
	};
}