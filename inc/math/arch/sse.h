#pragma once

//Cast

inline I32x4 I32x4_fromF32x4(F32x4 a) { return _mm_cvtps_epi32(a); }
inline F32x4 F32x4_fromI32x4(I32x4 a) { return _mm_cvtepi32_ps(a); }
inline I32x2 I32x2_fromF32x2(F32x2 a) { return (I32x2) { .v = { (I32) F32x2_y(a), (I32) F32x2_y(a) } }; }
inline F32x2 F32x2_fromI32x2(I32x2 a) { return (F32x2) { .v = { (F32) I32x2_y(a), (F32) I32x2_y(a) } }; }

//Arithmetic

inline I32x4 I32x4_add(I32x4 a, I32x4 b) { return _mm_add_epi32(a, b); }
inline F32x4 F32x4_add(F32x4 a, F32x4 b) { return _mm_add_ps(a, b); }
inline I32x2 I32x2_add(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] + b.v[i])
inline F32x2 F32x2_add(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] + b.v[i])

inline I32x4 I32x4_sub(I32x4 a, I32x4 b) { return _mm_sub_epi32(a, b); }
inline F32x4 F32x4_sub(F32x4 a, F32x4 b) { return _mm_sub_ps(a, b); }
inline I32x2 I32x2_sub(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] - b.v[i])
inline F32x2 F32x2_sub(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] - b.v[i])

inline I32x4 I32x4_mul(I32x4 a, I32x4 b) { return _mm_mul_epi32(a, b); }
inline F32x4 F32x4_mul(F32x4 a, F32x4 b) { return _mm_mul_ps(a, b); }
inline I32x2 I32x2_mul(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] * b.v[i])
inline F32x2 F32x2_mul(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] * b.v[i])

inline I32x4 I32x4_div(I32x4 a, I32x4 b) { return _mm_div_epi32(a, b); }
inline F32x4 F32x4_div(F32x4 a, F32x4 b) { return _mm_div_ps(a, b); }
inline I32x2 I32x2_div(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] / b.v[i])
inline F32x2 F32x2_div(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] / b.v[i])

//Swizzle

inline I32x4 _movelh_epi32(I32x4 a, I32x4 b) { 
	return I32x4_bitsF32x4(_mm_movelh_ps(
		F32x4_bitsI32x4(a), F32x4_bitsI32x4(b)
	)); 
}

inline I32x4 I32x4_trunc2(I32x4 a) { return _movelh_epi32(a, I32x4_zero()); }
inline F32x4 F32x4_trunc2(F32x4 a) { return _mm_movelh_ps(a, F32x4_zero()); }

inline I32x4 I32x4_trunc3(I32x4 a) { 
	I32x4 z0 = I32x4_xzzz(_movelh_epi32(I32x4_zzzz(a), I32x4_zero()));
	return _movelh_epi32(a, z0); 
}

inline F32x4 F32x4_trunc3(F32x4 a) { 
	F32x4 z0 = F32x4_xzzz(_mm_movelh_ps(F32x4_zzzz(a), F32x4_zero()));
	return _mm_movelh_ps(a, z0); 
}

//Swizzle

inline I32 I32x4_x(I32x4 a) { return _mm_cvtss_i32(*(F32x4*) &a); }
inline F32 F32x4_x(F32x4 a) { return _mm_cvtss_f32(a); }
inline I32 I32x2_x(I32x2 a) { return a.v[0]; }
inline F32 F32x2_x(F32x2 a) { return a.v[0]; }

inline I32 I32x4_y(I32x4 a) { return I32x4_x(I32x4_yyyy(a)); }
inline F32 F32x4_y(F32x4 a) { return F32x4_x(F32x4_yyyy(a)); }
inline I32 I32x2_y(I32x2 a) { return a.v[1]; }
inline F32 F32x2_y(F32x2 a) { return a.v[1]; }

inline I32 I32x4_z(I32x4 a) { return I32x4_x(I32x4_zzzz(a)); }
inline F32 F32x4_z(F32x4 a) { return F32x4_x(F32x4_zzzz(a)); }

inline I32 I32x4_w(I32x4 a) { return I32x4_x(I32x4_wwww(a)); }
inline F32 F32x4_w(F32x4 a) { return F32x4_x(F32x4_wwww(a)); }

inline I32x4 I32x4_create2(I32 x, I32 y) { return _mm_set_epi32(0, 0, y, x); }
inline F32x4 F32x4_create2(F32 x, F32 y) { return _mm_set_ps(0, 0, y, x); }
inline I32x2 I32x2_create2(I32 x, I32 y) { return (I32x2){ .v = { x, y } }; }
inline F32x2 F32x2_create2(F32 x, F32 y) { return (F32x2){ .v = { x, y } }; }

inline I32x4 I32x4_create1(I32 x) { return _mm_set_epi32(0, 0, 0, x); }
inline F32x4 F32x4_create1(F32 x) { return _mm_set_ps(0, 0, 0, x); }
inline I32x2 I32x2_create1(I32 x) { return I32x2_create2(x, 0); }
inline F32x2 F32x2_create1(F32 x) { return F32x2_create2(x, 0); }

inline F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return _mm_set_ps(0, z, y, x); }
inline I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return _mm_set_epi32(0, z, y, x); }

inline F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { return _mm_set_ps(w, z, y, x); }
inline I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { return _mm_set_epi32(w, z, y, x); }

inline F32x4 F32x4_xxxx4(F32 x) { return _mm_set1_ps(x); }
inline I32x4 I32x4_xxxx4(I32 x) { return _mm_set1_epi32(x); }
inline F32x2 F32x2_xx2(F32 x) { return F32x2_create2(x, x); }
inline I32x2 I32x2_xx2(I32 x) { return I32x2_create2(x, x); }

inline I32x4 I32x4_zero() { return I32x4_xxxx4(0); }
inline F32x4 F32x4_zero() { return _mm_setzero_ps(); }
inline I32x2 I32x2_zero() { return (I32x2){ 0 }; }
inline F32x2 F32x2_zero() { return (F32x2){ 0 }; }

//Comparison

inline F32x4 F32x4_recastI32x4Internal(F32x4 a) { return F32x4_fromI32x4(*(const I32x4*) &a); }
inline F32x4 F32x4_negateRecastiInternal(F32x4 a) { return F32x4_negate(F32x4_recastI32x4Internal(a)); }

inline I32x4 I32x4_eq(I32x4 a, I32x4 b)  { return I32x4_negate(_mm_cmpeq_epi32(a, b)); }
inline F32x4 F32x4_eq(F32x4 a, F32x4 b)  { return F32x4_negateRecastiInternal(_mm_cmpeq_ps(a, b)); }
inline I32x2 I32x2_eq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] == b.v[i])
inline F32x2 F32x2_eq(F32x2 a, F32x2 b) _NONE_OP2F((F32)(a.v[i] == b.v[i]))

inline I32x4 I32x4_neq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmpeq_epi32(a, b)); }
inline F32x4 F32x4_neq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpneq_ps(a, b)); }
inline I32x2 I32x2_neq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] != b.v[i])
inline F32x2 F32x2_neq(F32x2 a, F32x2 b) _NONE_OP2F((F32)(a.v[i] != b.v[i]))

inline I32x4 I32x4_geq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmplt_epi32(a, b)); }
inline F32x4 F32x4_geq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmpge_ps(a, b)); }
inline I32x2 I32x2_geq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] >= b.v[i])
inline F32x2 F32x2_geq(F32x2 a, F32x2 b) _NONE_OP2F((F32)(a.v[i] >= b.v[i]))

inline I32x4 I32x4_gt(I32x4 a, I32x4 b)  { return I32x4_negate(_mm_cmpgt_epi32(a, b)); }
inline F32x4 F32x4_gt(F32x4 a, F32x4 b)  { return F32x4_negateRecastiInternal(_mm_cmpgt_ps(a, b)); }
inline I32x2 I32x2_gt(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] > b.v[i])
inline F32x2 F32x2_gt(F32x2 a, F32x2 b) _NONE_OP2F((F32)(a.v[i] > b.v[i]))

inline I32x4 I32x4_leq(I32x4 a, I32x4 b) { return I32x4_add(I32x4_one(), _mm_cmpgt_epi32(a, b)); }
inline F32x4 F32x4_leq(F32x4 a, F32x4 b) { return F32x4_negateRecastiInternal(_mm_cmple_ps(a, b)); }
inline I32x2 I32x2_leq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] <= b.v[i])
inline F32x2 F32x2_leq(F32x2 a, F32x2 b) _NONE_OP2F((F32)(a.v[i] <= b.v[i]))

inline I32x4 I32x4_lt(I32x4 a, I32x4 b)  { return I32x4_negate(_mm_cmplt_epi32(a, b)); }
inline F32x4 F32x4_lt(F32x4 a, F32x4 b)  { return F32x4_negateRecastiInternal(_mm_cmplt_ps(a, b)); }
inline I32x2 I32x2_lt(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] < b.v[i])
inline F32x2 F32x2_lt(F32x2 a, F32x2 b) _NONE_OP2F((F32)(a.v[i] < b.v[i]))

//Bitwise

inline I32x4 I32x4_or(I32x4 a, I32x4 b)  _NONE_OP4I(I32x4_get(a, i) | I32x4_get(b, i))		//_mm_or_epi32 is AVX512 :(
inline I32x2 I32x2_or(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] | b.v[i])

inline I32x4 I32x4_and(I32x4 a, I32x4 b) _NONE_OP4I(I32x4_get(a, i) & I32x4_get(b, i))		//_mm_and_epi32 is AVX512 :(
inline I32x2 I32x2_and(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] & b.v[i])

inline I32x4 I32x4_xor(I32x4 a, I32x4 b) _NONE_OP4I(I32x4_get(a, i) ^ I32x4_get(b, i))		//_mm_xor_epi32 is AVX512 :(
inline I32x2 I32x2_xor(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] ^ b.v[i])

//Min/max

inline I32x4 I32x4_min(I32x4 a, I32x4 b) { return _mm_min_epi32(a, b); }
inline F32x4 F32x4_min(F32x4 a, F32x4 b) { return _mm_min_ps(a, b); }
inline I32x2 I32x2_min(I32x2 a, I32x2 b) { return I32x2_fromI32x4(I32x4_min(I32x4_fromI32x2(a), I32x4_fromI32x2(b))); }
inline F32x2 F32x2_min(F32x2 a, F32x2 b) { return F32x2_fromF32x4(F32x4_min(F32x4_fromF32x2(a), F32x4_fromF32x2(b))); }

inline I32x4 I32x4_max(I32x4 a, I32x4 b) { return _mm_max_epi32(a, b); }
inline F32x4 F32x4_max(F32x4 a, F32x4 b) { return _mm_max_ps(a, b); }
inline I32x2 I32x2_max(I32x2 a, I32x2 b) { return I32x2_fromI32x4(I32x4_max(I32x4_fromI32x2(a), I32x4_fromI32x2(b))); }
inline F32x2 F32x2_max(F32x2 a, F32x2 b) { return F32x2_fromF32x4(F32x4_max(F32x4_fromF32x2(a), F32x4_fromF32x2(b))); }

//Reduce

inline I32 I32x4_reduce(I32x4 a) {
	return I32x4_x(_mm_hadd_epi32(_mm_hadd_epi32(a, I32x4_zero()), I32x4_zero()));
}

inline F32 F32x4_reduce(F32x4 a) {
	return F32x4_x(_mm_hadd_ps(_mm_hadd_ps(a, F32x4_zero()), F32x4_zero()));
}

inline I32 I32x2_reduce(I32x2 a) { return I32x2_x(a) + I32x2_y(a); }
inline F32 F32x2_reduce(F32x2 a) { return F32x2_x(a) + F32x2_y(a); }

//Swizzle I32x2

inline I32x2 I32x2_xx(I32x2 a) { return I32x2_xx2(I32x2_x(a)); }
inline F32x2 F32x2_xx(F32x2 a) { return F32x2_xx2(F32x2_x(a)); }

inline I32x2 I32x2_yy(I32x2 a) { return I32x2_xx2(I32x2_y(a)); }
inline F32x2 F32x2_yy(F32x2 a) { return F32x2_xx2(F32x2_y(a)); }

inline I32x2 I32x2_yx(I32x2 a) { return I32x2_create2(I32x2_y(a), I32x2_x(a)); }
inline F32x2 F32x2_yx(F32x2 a) { return F32x2_create2(F32x2_y(a), F32x2_x(a)); }

//Float specific

//Rounding

inline F32x4 F32x4_ceil(F32x4 a) { return _mm_ceil_ps(a); }
inline F32x4 F32x4_floor(F32x4 a) { return _mm_floor_ps(a); }
inline F32x4 F32x4_round(F32x4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }

//Expensive math

inline F32x4 F32x4_pow(F32x4 v, F32x4 e) { return _mm_pow_ps(v, e); }
inline F32x4 F32x4_sqrt(F32x4 a) { return _mm_sqrt_ps(a); }
inline F32x4 F32x4_rsqrt(F32x4 a) { return _mm_rsqrt_ps(a); }

inline F32x4 F32x4_loge(F32x4 v) { return _mm_log_ps(v); }
inline F32x4 F32x4_log10(F32x4 v) { return _mm_log10_ps(v); }
inline F32x4 F32x4_log2(F32x4 v) { return _mm_log2_ps(v); }

inline F32x4 F32x4_exp(F32x4 v) { return _mm_exp_ps(v); }
inline F32x4 F32x4_exp10(F32x4 v) { return _mm_exp10_ps(v); }
inline F32x4 F32x4_exp2(F32x4 v)  { return _mm_exp2_ps(v); }

//Trig

inline F32x4 F32x4_acos(F32x4 v) { return _mm_acos_ps(v); }
inline F32x4 F32x4_cos(F32x4 v) { return _mm_cos_ps(v); }
inline F32x4 F32x4_asin(F32x4 v) { return _mm_asin_ps(v); }
inline F32x4 F32x4_sin(F32x4 v) { return _mm_sin_ps(v); }
inline F32x4 F32x4_atan(F32x4 v) { return _mm_atan_ps(v); }
inline F32x4 F32x4_atan2(F32x4 y, F32x4 x) { return _mm_atan2_ps(y, x); }
inline F32x4 F32x4_tan(F32x4 v) { return _mm_tan_ps(v); }

//Version for vec2

#define _F32x2_wrap1(x) { return F32x2_fromF32x4(x(F32x4_fromF32x2(a))); }
#define _F32x2_wrap2(x) { return F32x2_fromF32x4(x(F32x4_fromF32x2(a), F32x4_fromF32x2(b))); }

inline F32x2 F32x2_ceil(F32x2 a)  _F32x2_wrap1(F32x4_ceil)
inline F32x2 F32x2_floor(F32x2 a) _F32x2_wrap1(F32x4_floor)
inline F32x2 F32x2_round(F32x2 a) _F32x2_wrap1(F32x4_round)

inline F32x2 F32x2_pow(F32x2 a, F32x2 b) _F32x2_wrap2(F32x4_pow)
inline F32x2 F32x2_sqrt(F32x2 a)  _F32x2_wrap1(F32x4_sqrt)
inline F32x2 F32x2_rsqrt(F32x2 a) _F32x2_wrap1(F32x4_rsqrt)

inline F32x2 F32x2_loge(F32x2 a)  _F32x2_wrap1(F32x4_loge)
inline F32x2 F32x2_log10(F32x2 a) _F32x2_wrap1(F32x4_log10)
inline F32x2 F32x2_log2(F32x2 a)  _F32x2_wrap1(F32x4_log2)

inline F32x2 F32x2_exp(F32x2 a)   _F32x2_wrap1(F32x4_exp)
inline F32x2 F32x2_exp10(F32x2 a) _F32x2_wrap1(F32x4_exp10)
inline F32x2 F32x2_exp2(F32x2 a)  _F32x2_wrap1(F32x4_exp2)

inline F32x2 F32x2_acos(F32x2 a) _F32x2_wrap1(F32x4_acos)
inline F32x2 F32x2_cos(F32x2 a)  _F32x2_wrap1(F32x4_cos)
inline F32x2 F32x2_asin(F32x2 a) _F32x2_wrap1(F32x4_asin)
inline F32x2 F32x2_sin(F32x2 a)  _F32x2_wrap1(F32x4_sin)
inline F32x2 F32x2_atan(F32x2 a) _F32x2_wrap1(F32x4_atan)
inline F32x2 F32x2_tan(F32x2 a)  _F32x2_wrap1(F32x4_tan)
inline F32x2 F32x2_atan2(F32x2 a, F32x2 b) _F32x2_wrap2(F32x4_atan2)

//Dot products

inline F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(a, b, 0xFF)); }
inline F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(F32x4_xy4(a), F32x4_xy4(b), 0xFF)); }
inline F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_x(_mm_dp_ps(F32x4_xyz(a), F32x4_xyz(b), 0xFF)); }

inline F32 F32x2_dot(F32x2 a, F32x2 b) { return F32x4_dot2(F32x4_fromF32x2(a), F32x4_fromF32x2(b)); }
