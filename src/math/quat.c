/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "math/quat.h"
#include "types/error.h"

Quat Quat_create(F32 x, F32 y, F32 z, F32 w) { return F32x4_create4(x, y, z, w); }

Quat Quat_identity() { return Quat_create(0, 0, 0, 1); }

Quat Quat_conj(Quat q) { return Quat_create(-F32x4_x(q), -F32x4_y(q), -F32x4_z(q), F32x4_w(q)); }
Quat Quat_normalize(Quat q) { return F32x4_normalize4(q); }
Quat Quat_inverse(Quat q) { return Quat_normalize(Quat_conj(q)); }

Bool Quat_eq(Quat a, Quat b) { return F32x4_eq4(a, b); }
Bool Quat_neq(Quat a, Quat b) { return F32x4_neq4(a, b); }

F32 Quat_x(Quat a) { return F32x4_x(a); }
F32 Quat_y(Quat a) { return F32x4_y(a); }
F32 Quat_z(Quat a) { return F32x4_z(a); }
F32 Quat_w(Quat a) { return F32x4_w(a); }
F32 Quat_get(Quat a, U8 i) { return F32x4_get(a, i); }

void Quat_setX(Quat *a, F32 v) { F32x4_setX(a, v); }
void Quat_setY(Quat *a, F32 v) { F32x4_setY(a, v); }
void Quat_setZ(Quat *a, F32 v) { F32x4_setZ(a, v); }
void Quat_setW(Quat *a, F32 v) { F32x4_setW(a, v); }
void Quat_set(Quat *a, U8 i, F32 v) { F32x4_set(a, i, v); }

Quat Quat_lerp(Quat a, Quat b, F32 perc) { return F32x4_lerp(a, b, perc); }

//Rotate normal by quaternion
//https://math.stackexchange.com/questions/40164/how-do-you-rotate-a-vector-by-a-unit-quaternion
//
F32x4 Quat_applyToNormal(Quat R, F32x4 P) {
	return Quat_mul(Quat_mul(R, P), Quat_conj(R));
}

//Rotate around an axis with an angle
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Definition
//
Quat Quat_angleAxis(F32x4 axis, F32 angle) {

	angle *= 0.5f;

	F32 sinA2 = F32_sin(angle);
	F32 cosA2 = F32_cos(angle);

	F32x4 cosB = F32x4_cos(axis);
	F32x4 q = F32x4_mul(cosB, F32x4_xxxx4(sinA2));

	F32x4_setW(&q, cosA2);
	return q;
}

//Conquaternion from euler. Rotation around xyz (pitch, yaw, roll) in degrees
//https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Euler_angles_to_quaternion_conversion
//
Quat Quat_fromEuler(F32x4 pitchYawRollDeg) {

	F32x4 pitchYawRollRad = F32x4_mul(pitchYawRollDeg, F32x4_xxxx4(F32_DEG_TO_RAD * .5f));

	F32x4 c = F32x4_cos(pitchYawRollRad);
	F32x4 s = F32x4_sin(pitchYawRollRad);

	F32 cp = F32x4_x(c);
	F32 cy = F32x4_y(c);
	F32 cr = F32x4_z(c);

	F32 sp = F32x4_x(s);
	F32 sy = F32x4_y(s);
	F32 sr = F32x4_z(s);

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
F32x4 Quat_toEuler(Quat q) {

	F32x4 q2	= F32x4_pow2(q);

	F32 q2_x	= F32x4_x(q2);
	F32 q2_y	= F32x4_y(q2);
	F32 q2_z	= F32x4_z(q2);

	//Calculate roll

	F32x4 qXzw	= F32x4_mul(q, F32x4_wz4(q));

	F32 qXzw_x = F32x4_x(qXzw);
	F32 qXzw_y = F32x4_y(qXzw);

	F32 cpsr = 2 * (qXzw_x + qXzw_y);
	F32 cpcr = 1 - 2 * (q2_x + q2_y);
	F32 r = F32_atan2(cpsr, cpcr);

	//Calculate pitch

	F32 sp = 2 * (qXzw_y - qXzw_x);
	F32 p;

	if (F32_abs(sp) >= 1)		//90_deg if out of range
		p = F32_PI * 0.5f * F32_signInc(sp);

	else p = F32_asin(sp);

	//Calculate yaw

	F32x4 xzXyw = F32x4_mul(F32x4_xz4(q), F32x4_yw4(q));

	F32 xzXyw_x = F32x4_x(xzXyw);
	F32 xzXyw_y = F32x4_y(xzXyw);

	F32 siny_cosp = 2 * (xzXyw_y + xzXyw_x);
	F32 cosy_cosp = 1 - 2 * (q2_y + q2_z);
	F32 y = F32_atan2(siny_cosp, cosy_cosp);

	return F32x4_mul(F32x4_create3(p, y, r), F32x4_xxxx4(F32_RAD_TO_DEG));
}

//Combine two quaternions
//https://stackoverflow.com/questions/19956555/how-to-multiply-two-quaternions
//
Quat Quat_mul(Quat a, Quat b) {

	F32x4 axXb = F32x4_mul(b, F32x4_xxxx(a));
	F32x4 ayXb = F32x4_mul(b, F32x4_yyyy(a));
	F32x4 azXb = F32x4_mul(b, F32x4_zzzz(a));
	F32x4 awXb = F32x4_mul(b, F32x4_wwww(a));

	F32 axXb_x = F32x4_x(axXb),	axXb_y = F32x4_y(axXb),	axXb_z = F32x4_z(axXb),	axXb_w = F32x4_w(axXb);
	F32 ayXb_x = F32x4_x(ayXb),	ayXb_y = F32x4_y(ayXb),	ayXb_z = F32x4_z(ayXb),	ayXb_w = F32x4_w(ayXb);
	F32 azXb_x = F32x4_x(azXb),	azXb_y = F32x4_y(azXb),	azXb_z = F32x4_z(azXb),	azXb_w = F32x4_w(azXb);
	F32 awXb_x = F32x4_x(awXb),	awXb_y = F32x4_y(awXb),	awXb_z = F32x4_z(awXb),	awXb_w = F32x4_w(awXb);

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
Quat Quat_targetDirection(F32x4 origin, F32x4 target) {

	F32 leno = F32x4_len3(origin), lent = F32x4_len3(target);

	F32 leno2 = 0, lent2 = 0;
	Error err;

	if((err = F32_pow2(leno, &leno2)).genericError || (err = F32_pow2(lent, &lent2)).genericError)
		return Quat_identity();

	F32 w = F32_sqrt(leno2 * lent2) + F32x4_dot3(origin, target);

	F32x4 cross = F32x4_cross3(origin, target);

	return Quat_normalize(Quat_create(
		F32x4_x(cross), F32x4_y(cross), F32x4_z(cross),
		w
	));
}

//Lerp between a and b using percentage (or extrapolate if perc > 1 or < 0)
//https://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/index.htm
//
Quat Quat_slerp(Quat a, Quat b, F32 perc) {

	F32 cosTheta2 = F32x4_dot4(a, b);

	if (F32_abs(cosTheta2) >= 1)
		return a;

	F32 cosTheta2Pow = 0;
	
	Error err = F32_pow2(cosTheta2, &cosTheta2Pow);

	if(err.genericError)
		return b;

	F32 halfTheta = F32_acos(cosTheta2);
	F32 sinTheta2 = F32_sqrt(1 - cosTheta2Pow);

	if(F32_abs(sinTheta2) < 1e-3f)		//Theta 180deg isn't defined, so define as 50/50
		return F32x4_lerp(a, b, .5f);

	F32 invSinTheta2 = 1 / sinTheta2;

	F32 ratioA = F32_sin(halfTheta * (1 - perc)) * invSinTheta2;
	F32 ratioB = F32_sin(halfTheta * perc) * invSinTheta2; 

	a = F32x4_mul(a, F32x4_xxxx4(ratioA));
	b = F32x4_mul(b, F32x4_xxxx4(ratioB));

	return F32x4_add(a, b);
}
