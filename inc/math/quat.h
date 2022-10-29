#pragma once
#include "vec.h"
#include "math/math.h"

typedef F32x4 Quat;

//Simple quaternion functions

inline Quat Quat_create(F32 x, F32 y, F32 z, F32 w) { return F32x4_create4(x, y, z, w); }

inline Quat Quat_identity() { return Quat_create(0, 0, 0, 1); }

inline Quat Quat_conj(Quat q) { return Quat_create(-F32x4_x(q), -F32x4_y(q), -F32x4_z(q), F32x4_w(q)); }
inline Quat Quat_normalize(Quat q) { return F32x4_normalize4(q); }
inline Quat Quat_inverse(Quat q) { return Quat_normalize(Quat_conj(q)); }

//Shortcuts

inline Bool Quat_eq(Quat a, Quat b) { return F32x4_eq4(a, b); }
inline Bool Quat_neq(Quat a, Quat b) { return F32x4_neq4(a, b); }

inline F32 Quat_x(Quat a) { return F32x4_x(a); }
inline F32 Quat_y(Quat a) { return F32x4_y(a); }
inline F32 Quat_z(Quat a) { return F32x4_z(a); }
inline F32 Quat_w(Quat a) { return F32x4_w(a); }
inline F32 Quat_get(Quat a, U8 i) { return F32x4_get(a, i); }

inline void Quat_setX(Quat *a, F32 v) { F32x4_setX(a, v); }
inline void Quat_setY(Quat *a, F32 v) { F32x4_setY(a, v); }
inline void Quat_setZ(Quat *a, F32 v) { F32x4_setZ(a, v); }
inline void Quat_setW(Quat *a, F32 v) { F32x4_setW(a, v); }
inline void Quat_set(Quat *a, U8 i, F32 v) { F32x4_set(a, i, v); }

inline Quat Quat_lerp(Quat a, Quat b, F32 perc) { return F32x4_lerp(a, b, perc); }

//Helper funcs

Quat Quat_angleAxis(F32x4 axis, F32 angle);
Quat Quat_fromEuler(F32x4 pitchYawRollDeg);
F32x4 Quat_toEuler(Quat q);
Quat Quat_mul(Quat a, Quat b);
Quat Quat_targetDirection(F32x4 origin, F32x4 target);

//Rotate normal by quaternion
//https://math.stackexchange.com/questions/40164/how-do-you-rotate-a-vector-by-a-unit-quaternion
//
inline F32x4 Quat_applyToNormal(Quat R, F32x4 P) {
	return Quat_mul(Quat_mul(R, P), Quat_conj(R));
}

Quat Quat_slerp(Quat a, Quat b, F32 perc);

//inline Quat Quat_fromLookRotation(F32x4 fwd, F32x4 up);
