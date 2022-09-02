#pragma once

//Cast

inline i32x4 i32x4_fromF32x4(f32x4 a) { return _mm_cvtps_epi32(a); }
inline f32x4 f32x4_fromI32x4(i32x4 a) { return _mm_cvtepi32_ps(a); }
inline i32x2 i32x2_fromF32x2(f32x2 a) { return (i32x2) { .v = { (i32) f32x2_y(a), (i32) f32x2_y(a) } }; }
inline f32x2 f32x2_fromI32x2(i32x2 a) { return (f32x2) { .v = { (f32) i32x2_y(a), (f32) i32x2_y(a) } }; }

//Arithmetic

inline i32x4 i32x4_add(i32x4 a, i32x4 b) { return _mm_add_epi32(a, b); }
inline f32x4 f32x4_add(f32x4 a, f32x4 b) { return _mm_add_ps(a, b); }
inline i32x2 i32x2_add(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] + b.v[i])
inline f32x2 f32x2_add(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] + b.v[i])

inline i32x4 i32x4_sub(i32x4 a, i32x4 b) { return _mm_sub_epi32(a, b); }
inline f32x4 f32x4_sub(f32x4 a, f32x4 b) { return _mm_sub_ps(a, b); }
inline i32x2 i32x2_sub(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] - b.v[i])
inline f32x2 f32x2_sub(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] - b.v[i])

inline i32x4 i32x4_mul(i32x4 a, i32x4 b) { return _mm_mul_epi32(a, b); }
inline f32x4 f32x4_mul(f32x4 a, f32x4 b) { return _mm_mul_ps(a, b); }
inline i32x2 i32x2_mul(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] * b.v[i])
inline f32x2 f32x2_mul(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] * b.v[i])

inline i32x4 i32x4_div(i32x4 a, i32x4 b) { return _mm_div_epi32(a, b); }
inline f32x4 f32x4_div(f32x4 a, f32x4 b) { return _mm_div_ps(a, b); }
inline i32x2 i32x2_div(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] / b.v[i])
inline f32x2 f32x2_div(f32x2 a, f32x2 b) _NONE_OP2F(a.v[i] / b.v[i])

//Swizzle

inline i32x4 _movelh_epi32(i32x4 a, i32x4 b) { 
    return i32x4_bitsF32x4(_mm_movelh_ps(
        f32x4_bitsI32x4(a), f32x4_bitsI32x4(b)
    )); 
}

inline i32x4 i32x4_trunc2(i32x4 a) { return _movelh_epi32(a, i32x4_zero()); }
inline f32x4 f32x4_trunc2(f32x4 a) { return _mm_movelh_ps(a, f32x4_zero()); }

inline i32x4 i32x4_trunc3(i32x4 a) { 
    i32x4 z0 = i32x4_xzzz(_movelh_epi32(i32x4_zzzz(a), i32x4_zero()));
    return _movelh_epi32(a, z0); 
}

inline f32x4 f32x4_trunc3(f32x4 a) { 
    f32x4 z0 = f32x4_xzzz(_mm_movelh_ps(f32x4_zzzz(a), f32x4_zero()));
    return _mm_movelh_ps(a, z0); 
}

//Swizzle

inline i32 i32x4_x(i32x4 a) { return _mm_cvtss_i32(*(f32x4*) &a); }
inline f32 f32x4_x(f32x4 a) { return _mm_cvtss_f32(a); }
inline i32 i32x2_x(i32x2 a) { return a.v[0]; }
inline f32 f32x2_x(f32x2 a) { return a.v[0]; }

inline i32 i32x4_y(i32x4 a) { return i32x4_x(i32x4_yyyy(a)); }
inline f32 f32x4_y(f32x4 a) { return f32x4_x(f32x4_yyyy(a)); }
inline i32 i32x2_y(i32x2 a) { return a.v[1]; }
inline f32 f32x2_y(f32x2 a) { return a.v[1]; }

inline i32 i32x4_z(i32x4 a) { return i32x4_x(i32x4_zzzz(a)); }
inline f32 f32x4_z(f32x4 a) { return f32x4_x(f32x4_zzzz(a)); }

inline i32 i32x4_w(i32x4 a) { return i32x4_x(i32x4_wwww(a)); }
inline f32 f32x4_w(f32x4 a) { return f32x4_x(f32x4_wwww(a)); }

inline i32x4 i32x4_init2(i32 x, i32 y) { return _mm_set_epi32(0, 0, y, x); }
inline f32x4 f32x4_init2(f32 x, f32 y) { return _mm_set_ps(0, 0, y, x); }
inline i32x2 i32x2_init2(i32 x, i32 y) { return (i32x2){ .v = { x, y } }; }
inline f32x2 f32x2_init2(f32 x, f32 y) { return (f32x2){ .v = { x, y } }; }

inline i32x4 i32x4_init1(i32 x) { return _mm_set_epi32(0, 0, 0, x); }
inline f32x4 f32x4_init1(f32 x) { return _mm_set_ps(0, 0, 0, x); }
inline i32x2 i32x2_init1(i32 x) { return i32x2_init2(x, 0); }
inline f32x2 f32x2_init1(f32 x) { return f32x2_init2(x, 0); }

inline f32x4 f32x4_init3(f32 x, f32 y, f32 z) { return _mm_set_ps(0, z, y, x); }
inline i32x4 i32x4_init3(i32 x, i32 y, i32 z) { return _mm_set_epi32(0, z, y, x); }

inline f32x4 f32x4_init4(f32 x, f32 y, f32 z, f32 w) { return _mm_set_ps(w, z, y, x); }
inline i32x4 i32x4_init4(i32 x, i32 y, i32 z, i32 w) { return _mm_set_epi32(w, z, y, x); }

inline f32x4 f32x4_xxxx4(f32 x) { return _mm_set1_ps(x); }
inline i32x4 i32x4_xxxx4(i32 x) { return _mm_set1_epi32(x); }
inline f32x2 f32x2_xx2(f32 x) { return f32x2_init2(x, x); }
inline i32x2 i32x2_xx2(i32 x) { return i32x2_init2(x, x); }

inline i32x4 i32x4_zero() { return i32x4_xxxx4(0); }
inline f32x4 f32x4_zero() { return _mm_setzero_ps(); }
inline i32x2 i32x2_zero() { return (i32x2){ 0 }; }
inline f32x2 f32x2_zero() { return (f32x2){ 0 }; }

//Comparison

inline f32x4 _f32x4_recastI32x4(f32x4 a) { return f32x4_fromI32x4(*(const i32x4*) &a); }
inline f32x4 _f32x4_negateRecasti(f32x4 a) { return f32x4_negate(_f32x4_recastI32x4(a)); }

inline i32x4 i32x4_eq(i32x4 a, i32x4 b)  { return i32x4_negate(_mm_cmpeq_epi32(a, b)); }
inline f32x4 f32x4_eq(f32x4 a, f32x4 b)  { return _f32x4_negateRecasti(_mm_cmpeq_ps(a, b)); }
inline i32x2 i32x2_eq(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] == b.v[i])
inline f32x2 f32x2_eq(f32x2 a, f32x2 b) _NONE_OP2F((f32)(a.v[i] == b.v[i]))

inline i32x4 i32x4_neq(i32x4 a, i32x4 b) { return i32x4_add(i32x4_one(), _mm_cmpeq_epi32(a, b)); }
inline f32x4 f32x4_neq(f32x4 a, f32x4 b) { return _f32x4_negateRecasti(_mm_cmpneq_ps(a, b)); }
inline i32x2 i32x2_neq(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] != b.v[i])
inline f32x2 f32x2_neq(f32x2 a, f32x2 b) _NONE_OP2F((f32)(a.v[i] != b.v[i]))

inline i32x4 i32x4_geq(i32x4 a, i32x4 b) { return i32x4_add(i32x4_one(), _mm_cmplt_epi32(a, b)); }
inline f32x4 f32x4_geq(f32x4 a, f32x4 b) { return _f32x4_negateRecasti(_mm_cmpge_ps(a, b)); }
inline i32x2 i32x2_geq(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] >= b.v[i])
inline f32x2 f32x2_geq(f32x2 a, f32x2 b) _NONE_OP2F((f32)(a.v[i] >= b.v[i]))

inline i32x4 i32x4_gt(i32x4 a, i32x4 b)  { return i32x4_negate(_mm_cmpgt_epi32(a, b)); }
inline f32x4 f32x4_gt(f32x4 a, f32x4 b)  { return _f32x4_negateRecasti(_mm_cmpgt_ps(a, b)); }
inline i32x2 i32x2_gt(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] > b.v[i])
inline f32x2 f32x2_gt(f32x2 a, f32x2 b) _NONE_OP2F((f32)(a.v[i] > b.v[i]))

inline i32x4 i32x4_leq(i32x4 a, i32x4 b) { return i32x4_add(i32x4_one(), _mm_cmpgt_epi32(a, b)); }
inline f32x4 f32x4_leq(f32x4 a, f32x4 b) { return _f32x4_negateRecasti(_mm_cmple_ps(a, b)); }
inline i32x2 i32x2_leq(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] <= b.v[i])
inline f32x2 f32x2_leq(f32x2 a, f32x2 b) _NONE_OP2F((f32)(a.v[i] <= b.v[i]))

inline i32x4 i32x4_lt(i32x4 a, i32x4 b)  { return i32x4_negate(_mm_cmplt_epi32(a, b)); }
inline f32x4 f32x4_lt(f32x4 a, f32x4 b)  { return _f32x4_negateRecasti(_mm_cmplt_ps(a, b)); }
inline i32x2 i32x2_lt(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] < b.v[i])
inline f32x2 f32x2_lt(f32x2 a, f32x2 b) _NONE_OP2F((f32)(a.v[i] < b.v[i]))

//Bitwise

inline i32x4 i32x4_or(i32x4 a, i32x4 b)  { return _mm_or_epi32(a, b); }
inline i32x2 i32x2_or(i32x2 a, i32x2 b)  _NONE_OP2I(a.v[i] | b.v[i])

inline i32x4 i32x4_and(i32x4 a, i32x4 b) { return _mm_and_epi32(a, b); }
inline i32x2 i32x2_and(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] & b.v[i])

inline i32x4 i32x4_xor(i32x4 a, i32x4 b) { return _mm_xor_epi32(a, b); }
inline i32x2 i32x2_xor(i32x2 a, i32x2 b) _NONE_OP2I(a.v[i] ^ b.v[i])

//Min/max

inline i32x4 i32x4_min(i32x4 a, i32x4 b) { return _mm_min_epi32(a, b); }
inline f32x4 f32x4_min(f32x4 a, f32x4 b) { return _mm_min_ps(a, b); }
inline i32x2 i32x2_min(i32x2 a, i32x2 b) { return i32x2_fromI32x4(i32x4_min(i32x4_fromI32x2(a), i32x4_fromI32x2(b))); }
inline f32x2 f32x2_min(f32x2 a, f32x2 b) { return f32x2_fromF32x4(f32x4_min(f32x4_fromF32x2(a), f32x4_fromF32x2(b))); }

inline i32x4 i32x4_max(i32x4 a, i32x4 b) { return _mm_max_epi32(a, b); }
inline f32x4 f32x4_max(f32x4 a, f32x4 b) { return _mm_max_ps(a, b); }
inline i32x2 i32x2_max(i32x2 a, i32x2 b) { return i32x2_fromI32x4(i32x4_max(i32x4_fromI32x2(a), i32x4_fromI32x2(b))); }
inline f32x2 f32x2_max(f32x2 a, f32x2 b) { return f32x2_fromF32x4(f32x4_max(f32x4_fromF32x2(a), f32x4_fromF32x2(b))); }

//Reduce

inline i32 i32x4_reduce(i32x4 a) {
    return i32x4_x(_mm_hadd_epi32(_mm_hadd_epi32(a, i32x4_zero()), i32x4_zero()));
}

inline f32 f32x4_reduce(f32x4 a) {
    return f32x4_x(_mm_hadd_ps(_mm_hadd_ps(a, f32x4_zero()), f32x4_zero()));
}

inline i32 i32x2_reduce(i32x2 a) { return i32x2_x(a) + i32x2_y(a); }
inline f32 f32x2_reduce(f32x2 a) { return f32x2_x(a) + f32x2_y(a); }

//Swizzle i32x2

inline i32x2 i32x2_xx(i32x2 a) { return i32x2_xx2(i32x2_x(a)); }
inline f32x2 f32x2_xx(f32x2 a) { return f32x2_xx2(f32x2_x(a)); }

inline i32x2 i32x2_yy(i32x2 a) { return i32x2_xx2(i32x2_x(a)); }
inline f32x2 f32x2_yy(f32x2 a) { return f32x2_xx2(f32x2_x(a)); }

inline i32x2 i32x2_yx(i32x2 a) { return i32x2_init2(i32x2_y(a), i32x2_x(a)); }
inline f32x2 f32x2_yx(f32x2 a) { return f32x2_init2(f32x2_y(a), f32x2_x(a)); }

//Float specific

//Rounding

inline f32x4 f32x4_ceil(f32x4 a) { return _mm_ceil_ps(a); }
inline f32x4 f32x4_floor(f32x4 a) { return _mm_floor_ps(a); }
inline f32x4 f32x4_round(f32x4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }

//Expensive math

inline f32x4 f32x4_pow(f32x4 v, f32x4 e) { return _mm_pow_ps(v, e); }
inline f32x4 f32x4_sqrt(f32x4 a) { return _mm_sqrt_ps(a); }
inline f32x4 f32x4_rsqrt(f32x4 a) { return _mm_rsqrt_ps(a); }

inline f32x4 f32x4_loge(f32x4 v) { return _mm_log_ps(v); }
inline f32x4 f32x4_log10(f32x4 v) { return _mm_log10_ps(v); }
inline f32x4 f32x4_log2(f32x4 v) { return _mm_log2_ps(v); }

inline f32x4 f32x4_exp(f32x4 v) { return _mm_exp_ps(v); }
inline f32x4 f32x4_exp10(f32x4 v) { return _mm_exp10_ps(v); }
inline f32x4 f32x4_exp2(f32x4 v)  { return _mm_exp2_ps(v); }

//Trig

inline f32x4 f32x4_acos(f32x4 v) { return _mm_acos_ps(v); }
inline f32x4 f32x4_cos(f32x4 v) { return _mm_cos_ps(v); }
inline f32x4 f32x4_asin(f32x4 v) { return _mm_asin_ps(v); }
inline f32x4 f32x4_sin(f32x4 v) { return _mm_sin_ps(v); }
inline f32x4 f32x4_atan(f32x4 v) { return _mm_atan_ps(v); }
inline f32x4 f32x4_atan2(f32x4 y, f32x4 x) { return _mm_atan2_ps(y, x); }
inline f32x4 f32x4_tan(f32x4 v) { return _mm_tan_ps(v); }

//Version for vec2

#define _f32x2_wrap1(x) { return f32x2_fromF32x4(x(f32x4_fromF32x2(a))); }
#define _f32x2_wrap2(x) { return f32x2_fromF32x4(x(f32x4_fromF32x2(a), f32x4_fromF32x2(b))); }

inline f32x2 f32x2_ceil(f32x2 a)  _f32x2_wrap1(f32x4_ceil)
inline f32x2 f32x2_floor(f32x2 a) _f32x2_wrap1(f32x4_floor)
inline f32x2 f32x2_round(f32x2 a) _f32x2_wrap1(f32x4_round)

inline f32x2 f32x2_pow(f32x2 a, f32x2 b) _f32x2_wrap2(f32x4_pow)
inline f32x2 f32x2_sqrt(f32x2 a)  _f32x2_wrap1(f32x4_sqrt)
inline f32x2 f32x2_rsqrt(f32x2 a) _f32x2_wrap1(f32x4_rsqrt)

inline f32x2 f32x2_loge(f32x2 a)  _f32x2_wrap1(f32x4_loge)
inline f32x2 f32x2_log10(f32x2 a) _f32x2_wrap1(f32x4_log10)
inline f32x2 f32x2_log2(f32x2 a)  _f32x2_wrap1(f32x4_log2)

inline f32x2 f32x2_exp(f32x2 a)   _f32x2_wrap1(f32x4_exp)
inline f32x2 f32x2_exp10(f32x2 a) _f32x2_wrap1(f32x4_exp10)
inline f32x2 f32x2_exp2(f32x2 a)  _f32x2_wrap1(f32x4_exp2)

inline f32x2 f32x2_acos(f32x2 a) _f32x2_wrap1(f32x4_acos)
inline f32x2 f32x2_cos(f32x2 a)  _f32x2_wrap1 (f32x4_cos)
inline f32x2 f32x2_asin(f32x2 a) _f32x2_wrap1(f32x4_asin)
inline f32x2 f32x2_sin(f32x2 a)  _f32x2_wrap1(f32x4_sin)
inline f32x2 f32x2_atan(f32x2 a) _f32x2_wrap1(f32x4_atan)
inline f32x2 f32x2_tan(f32x2 a) _f32x2_wrap1(f32x4_tan)
inline f32x2 f32x2_atan2(f32x2 a, f32x2 b) _f32x2_wrap2(f32x4_atan2)

//Dot products

inline f32 f32x4_dot4(f32x4 a, f32x4 b) { return f32x4_x(_mm_dp_ps(a, b, 0xFF)); }
inline f32 f32x4_dot2(f32x4 a, f32x4 b) { return f32x4_x(_mm_dp_ps(f32x4_xy4(a), f32x4_xy4(b), 0xFF)); }
inline f32 f32x4_dot3(f32x4 a, f32x4 b) { return f32x4_x(_mm_dp_ps(f32x4_xyz(a), f32x4_xyz(b), 0xFF)); }

inline f32 f32x2_dot(f32x2 a, f32x2 b) { return f32x4_dot2(f32x4_fromF32x2(a), f32x4_fromF32x2(b)); }
