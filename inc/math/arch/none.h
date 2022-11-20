#pragma once

//Cast

inline F32x4 F32x4_fromI32x4(I32x4 a) { 
	return (F32x4) { .v = { 
		(F32) I32x4_x(a), (F32) I32x4_y(a), (F32) I32x4_z(a), (F32) I32x4_w(a) 
	} }; 
}

inline I32x4 I32x4_fromF32x4(F32x4 a) { 
	return (I32x4) { .v = { 
		(I32) F32x4_x(a), (I32) F32x4_y(a), (I32) F32x4_z(a), (I32) F32x4_w(a) 
	} }; 
}

inline F32x2 F32x2_fromI32x2(I32x2 a) { return (F32x2) { .v = { (F32) I32x2_x(a), (F32) I32x2_y(a) } }; }
inline I32x2 I32x2_fromF32x2(F32x2 a) { return (I32x2) { .v = { (I32) F32x2_x(a), (I32) F32x2_y(a) } }; }

//Arithmetic

inline I32x4 I32x4_add(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] + b.v[i])
inline F32x4 F32x4_add(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] + b.v[i])
inline I32x2 I32x2_add(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] + b.v[i])
inline F32x2 F32x2_add(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] + b.v[i])

inline I32x4 I32x4_sub(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] - b.v[i])
inline F32x4 F32x4_sub(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] - b.v[i])
inline I32x2 I32x2_sub(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] - b.v[i])
inline F32x2 F32x2_sub(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] - b.v[i])

inline I32x4 I32x4_mul(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] * b.v[i])
inline F32x4 F32x4_mul(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] * b.v[i])
inline I32x2 I32x2_mul(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] * b.v[i])
inline F32x2 F32x2_mul(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] * b.v[i])

inline I32x4 I32x4_div(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] / b.v[i])
inline F32x4 F32x4_div(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] / b.v[i])
inline I32x2 I32x2_div(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] / b.v[i])
inline F32x2 F32x2_div(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] / b.v[i])

//Swizzle

inline I32x4 I32x4_trunc2(I32x4 a) { return I32x4_create2(I32x4_x(a), I32x4_y(a)); }
inline F32x4 F32x4_trunc2(F32x4 a) { return F32x4_create2(F32x4_x(a), F32x4_y(a)); }

inline I32x4 I32x4_trunc3(I32x4 a) { return I32x4_create3(I32x4_x(a), I32x4_y(a), I32x4_z(a)); }
inline F32x4 F32x4_trunc3(F32x4 a) { return F32x4_create3(F32x4_x(a), F32x4_y(a), F32x4_z(a)); }

inline I32 I32x4_x(I32x4 a) { return a.v[0]; }
inline F32 F32x4_x(F32x4 a) { return a.v[0]; }
inline I32 I32x2_x(I32x2 a) { return a.v[0]; }
inline F32 F32x2_x(F32x2 a) { return a.v[0]; }

inline I32 I32x4_y(I32x4 a) { return a.v[1]; }
inline F32 F32x4_y(F32x4 a) { return a.v[1]; }
inline I32 I32x2_y(I32x2 a) { return a.v[1]; }
inline F32 F32x2_y(F32x2 a) { return a.v[1]; }

inline I32 I32x4_z(I32x4 a) { return a.v[2]; }
inline F32 F32x4_z(F32x4 a) { return a.v[2]; }

inline I32 I32x4_w(I32x4 a) { return a.v[3]; }
inline F32 F32x4_w(F32x4 a) { return a.v[3]; }

inline I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { return (I32x4) { .v = { x, y, z, w } }; }
inline F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { return (F32x4) { .v = { x, y, z, w } }; }

inline I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return I32x4_create4(x, y, z, 0); }
inline F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return F32x4_create4(x, y, z, 0); }

inline I32x4 I32x4_create2(I32 x, I32 y) { return I32x4_create4(x, y, 0, 0); }
inline F32x4 F32x4_create2(F32 x, F32 y) { return F32x4_create4(x, y, 0, 0); }
inline I32x2 I32x2_create2(I32 x, I32 y) { return (I32x2) { .v = { x, y } }; }
inline F32x2 F32x2_create2(F32 x, F32 y) { return (F32x2) { .v = { x, y } }; }

inline I32x4 I32x4_create1(I32 x) { return I32x4_create4(x, 0, 0, 0); }
inline F32x4 F32x4_create1(F32 x) { return F32x4_create4(x, 0, 0, 0); }
inline I32x2 I32x2_create1(I32 x) { return I32x2_create2(x, 0); }
inline F32x2 F32x2_create1(F32 x) { return F32x2_create2(x, 0); }

inline I32x4 I32x4_xxxx4(I32 x) { return I32x4_create4(x, x, x, x); }
inline F32x4 F32x4_xxxx4(F32 x) { return F32x4_create4(x, x, x, x); }
inline I32x2 I32x2_xx2(I32 x) { return I32x2_create2(x, x); }
inline F32x2 F32x2_xx2(F32 x) { return F32x2_create2(x, x); }

inline I32x4 I32x4_zero() { return I32x4_xxxx4(0); }
inline I32x2 I32x2_zero() { return I32x2_xx2(0); }
inline F32x4 F32x4_zero() { return F32x4_xxxx4(0); }
inline F32x2 F32x2_zero() { return F32x2_xx2(0); }

//Comparison

inline I32x4 I32x4_eq(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] == b.v[i])
inline I32x2 I32x2_eq(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] == b.v[i])
inline F32x4 F32x4_eq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] == b.v[i]))
inline F32x2 F32x2_eq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] == b.v[i]))

inline I32x4 I32x4_neq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] != b.v[i])
inline I32x2 I32x2_neq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] != b.v[i])
inline F32x4 F32x4_neq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] != b.v[i]))
inline F32x2 F32x2_neq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] != b.v[i]))

inline I32x4 I32x4_geq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] >= b.v[i])
inline I32x2 I32x2_geq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] >= b.v[i])
inline F32x4 F32x4_geq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] >= b.v[i]))
inline F32x2 F32x2_geq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] >= b.v[i]))

inline I32x4 I32x4_gt(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] > b.v[i])
inline I32x2 I32x2_gt(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] > b.v[i])
inline F32x4 F32x4_gt(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] > b.v[i]))
inline F32x2 F32x2_gt(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] > b.v[i]))

inline I32x4 I32x4_leq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] <= b.v[i])
inline I32x2 I32x2_leq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] <= b.v[i])
inline F32x4 F32x4_leq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] <= b.v[i]))
inline F32x2 F32x2_leq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] <= b.v[i]))

inline I32x4 I32x4_lt(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] < b.v[i])
inline I32x2 I32x2_lt(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] < b.v[i])
inline F32x4 F32x4_lt(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] < b.v[i]))
inline F32x2 F32x2_lt(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] < b.v[i]))

//Bitwise

inline I32x4 I32x4_or(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] | b.v[i])
inline I32x2 I32x2_or(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] | b.v[i])

inline I32x4 I32x4_and(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] & b.v[i])
inline I32x2 I32x2_and(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] & b.v[i])

inline I32x4 I32x4_xor(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] ^ b.v[i])
inline I32x2 I32x2_xor(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] ^ b.v[i])

//Min/max

inline I32x4 I32x4_min(I32x4 a, I32x4 b) _NONE_OP4I((I32) I64_min(a.v[i], b.v[i]))
inline I32x2 I32x2_min(I32x2 a, I32x2 b) _NONE_OP2I((I32) I64_min(a.v[i], b.v[i]))
inline F32x4 F32x4_min(F32x4 a, F32x4 b) _NONE_OP4F(F32_min(a.v[i], b.v[i]))
inline F32x2 F32x2_min(F32x2 a, F32x2 b) _NONE_OP2F(F32_min(a.v[i], b.v[i]))

inline I32x4 I32x4_max(I32x4 a, I32x4 b) _NONE_OP4I((I32) I64_max(a.v[i], b.v[i]))
inline I32x2 I32x2_max(I32x2 a, I32x2 b) _NONE_OP2I((I32) I64_max(a.v[i], b.v[i]))
inline F32x4 F32x4_max(F32x4 a, F32x4 b) _NONE_OP4F(F32_max(a.v[i], b.v[i]))
inline F32x2 F32x2_max(F32x2 a, F32x2 b) _NONE_OP2F(F32_max(a.v[i], b.v[i]))

//Reduce

inline I32 I32x4_reduce(I32x4 a) { return I32x4_x(a) + I32x4_y(a) + I32x4_z(a) + I32x4_w(a); }
inline F32 F32x4_reduce(F32x4 a) { return F32x4_x(a) + F32x4_y(a) + F32x4_z(a) + F32x4_w(a); }

inline I32 I32x2_reduce(I32x2 a) { return I32x2_x(a) + I32x2_y(a); }
inline F32 F32x2_reduce(F32x2 a) { return F32x2_x(a) + F32x2_y(a); }

//Simple swizzle

inline I32x2 I32x2_xx(I32x2 a) { return I32x2_xx2(I32x2_x(a)); }
inline F32x2 F32x2_xx(F32x2 a) { return F32x2_xx2(F32x2_x(a)); }

inline I32x2 I32x2_yx(I32x2 a) { return I32x2_create2(I32x2_y(a), I32x2_x(a)); }
inline F32x2 F32x2_yx(F32x2 a) { return F32x2_create2(F32x2_y(a), F32x2_x(a)); }

inline F32x2 F32x2_yy(F32x2 a) { return F32x2_xx2(F32x2_y(a)); }
inline I32x2 I32x2_yy(I32x2 a) { return I32x2_xx2(I32x2_y(a)); }

//Float arithmetic

inline F32x4 F32x4_ceil(F32x4 a) _NONE_OP4F(F32_ceil(a.v[i]))
inline F32x2 F32x2_ceil(F32x2 a) _NONE_OP2F(F32_ceil(a.v[i]))

inline F32x4 F32x4_floor(F32x4 a) _NONE_OP4F(F32_floor(a.v[i]))
inline F32x2 F32x2_floor(F32x2 a) _NONE_OP2F(F32_floor(a.v[i]))

inline F32x4 F32x4_round(F32x4 a) _NONE_OP4F(F32_round(a.v[i]))
inline F32x2 F32x2_round(F32x2 a) _NONE_OP2F(F32_round(a.v[i]))

inline F32x4 F32x4_sqrt(F32x4 a) _NONE_OP4F(F32_sqrt(a.v[i]))
inline F32x2 F32x2_sqrt(F32x2 a) _NONE_OP2F(F32_sqrt(a.v[i]))

inline F32x4 F32x4_rsqrt(F32x4 a) _NONE_OP4F(1 / F32_sqrt(a.v[i]))
inline F32x2 F32x2_rsqrt(F32x2 a) _NONE_OP2F(1 / F32_sqrt(a.v[i]))

inline F32x4 F32x4_acos(F32x4 a) _NONE_OP4F(F32_acos(a.v[i]))
inline F32x2 F32x2_acos(F32x2 a) _NONE_OP2F(F32_acos(a.v[i]))

inline F32x4 F32x4_cos(F32x4 a) _NONE_OP4F(F32_cos(a.v[i]))
inline F32x2 F32x2_cos(F32x2 a) _NONE_OP2F(F32_cos(a.v[i]))

inline F32x4 F32x4_asin(F32x4 a) _NONE_OP4F(F32_asin(a.v[i]))
inline F32x2 F32x2_asin(F32x2 a) _NONE_OP2F(F32_asin(a.v[i]))

inline F32x4 F32x4_sin(F32x4 a) _NONE_OP4F(F32_sin(a.v[i]))
inline F32x2 F32x2_sin(F32x2 a) _NONE_OP2F(F32_sin(a.v[i]))

inline F32x4 F32x4_atan(F32x4 a) _NONE_OP4F(F32_atan(a.v[i]))
inline F32x2 F32x2_atan(F32x2 a) _NONE_OP2F(F32_atan(a.v[i]))

inline F32x4 F32x4_atan2(F32x4 a, F32x4 x) _NONE_OP4F(F32_atan2(a.v[i], x.v[i]))
inline F32x2 F32x2_atan2(F32x2 a, F32x2 x) _NONE_OP2F(F32_atan2(a.v[i], x.v[i]))

inline F32x4 F32x4_tan(F32x4 a)  _NONE_OP4F(F32_tan(a.v[i]))
inline F32x2 F32x2_tan(F32x2 a)  _NONE_OP2F(F32_tan(a.v[i]))

inline F32x4 F32x4_loge(F32x4 a)  _NONE_OP4F(F32_loge(a.v[i]))
inline F32x2 F32x2_loge(F32x2 a)  _NONE_OP2F(F32_loge(a.v[i]))

inline F32x4 F32x4_log10(F32x4 a)  _NONE_OP4F(F32_log10(a.v[i]))
inline F32x2 F32x2_log10(F32x2 a)  _NONE_OP2F(F32_log10(a.v[i]))

inline F32x4 F32x4_log2(F32x4 a)  _NONE_OP4F(F32_log2(a.v[i]))
inline F32x2 F32x2_log2(F32x2 a)  _NONE_OP2F(F32_log2(a.v[i]))

//These return 0 if an invalid value was returned. TODO: Make this conform better!

inline F32 _F32_expe(F32 v) { F32 v0 = 0; F32_expe(v, &v0); return v0; }
inline F32 _F32_exp10(F32 v) { F32 v0 = 0; F32_exp10(v, &v0); return v0; }
inline F32 _F32_exp2(F32 v) { F32 v0 = 0; F32_exp2(v, &v0); return v0; }

//

inline F32x4 F32x4_exp(F32x4 a) _NONE_OP4F(_F32_expe(a.v[i]))
inline F32x2 F32x2_exp(F32x2 a) _NONE_OP2F(_F32_expe(a.v[i]))

inline F32x4 F32x4_exp10(F32x4 a) _NONE_OP4F(_F32_exp10(a.v[i]))
inline F32x2 F32x2_exp10(F32x2 a) _NONE_OP2F(_F32_exp10(a.v[i]))

inline F32x2 F32x2_exp2(F32x2 a) _NONE_OP2F(_F32_exp2(a.v[i]))
inline F32x4 F32x4_exp2(F32x4 a) _NONE_OP4F(_F32_exp2(a.v[i]))

//Dot products

inline F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(a) * F32x4_x(b) + F32x4_y(a) * F32x4_y(b); }
inline F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_dot2(a, b) + F32x4_z(a) * F32x4_z(b); }
inline F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_dot3(a, b) + F32x4_w(a) * F32x4_w(b); }

inline F32 F32x2_dot(F32x2 a, F32x2 b) { return F32x4_dot2(F32x4_fromF32x2(a), F32x4_fromF32x2(b)); }
