#pragma once

//Int

//Arithmetic

inline i32x4 i32x4_add(i32x4 a, i32x4 b) { return _mm_add_epi32(a, b); }
inline i32x4 i32x4_sub(i32x4 a, i32x4 b) { return _mm_sub_epi32(a, b); }
inline i32x4 i32x4_mul(i32x4 a, i32x4 b) { return _mm_mul_epi32(a, b); }
inline i32x4 i32x4_div(i32x4 a, i32x4 b) { return _mm_div_epi32(a, b); }

//Swizzle

inline i32x4 i32x4_trunc2(i32x4 a) { return _mm_movelh_epi32(a, i32x4_zero()); }
inline i32x4 i32x4_trunc3(i32x4 a) { 
    i32x4 z0 = i32x4_xzzz(_mm_movelh_epi32(i32x4_zzzz(a), i32x4_zero()));
    return _mm_movelh_epi32(a, z0); 
}

//Swizzle

inline i32 i32x4_x(i32x4 a) { return _mm_cvtss_i32(*(f32x4*) &a); }
inline i32 i32x4_y(i32x4 a) { return i32x4_x(i32x4_yyyy(a)); }
inline i32 i32x4_z(i32x4 a) { return i32x4_x(i32x4_zzzz(a)); }
inline i32 i32x4_w(i32x4 a) { return i32x4_x(i32x4_wwww(a)); }

inline i32x4 i32x4_init1(i32 x) { return _mm_set_epi32(0, 0, 0, x); }
inline i32x4 i32x4_init2(i32 x, i32 y) { return _mm_set_epi32(0, 0, y, x); }
inline i32x4 i32x4_init3(i32 x, i32 y, i32 z) { return _mm_set_epi32(0, z, y, x); }
inline i32x4 i32x4_init4(i32 x, i32 y, i32 z, i32 w) { return _mm_set_epi32(w, z, y, x); }

inline i32x4 i32x4_xxxx4(i32 x) { return _mm_set1_epi32(x); }

inline i32x4 i32x4_load4(const i32 *arr) { return _mm_load_epi32(arr);}

inline i32x4 i32x4_zero() { return _mm_setzero_epi32(); }

//Comparison

inline i32x4 i32x4_eq(i32x4 a, i32x4 b)  { return i32x4_negate(_mm_cmpeq_epi32(a, b)); }
inline i32x4 i32x4_neq(i32x4 a, i32x4 b) { return i32x4_negate(_mm_cmpneq_epi32(a, b)); }
inline i32x4 i32x4_geq(i32x4 a, i32x4 b) { return i32x4_negate(_mm_cmpge_epi32(a, b)); }
inline i32x4 i32x4_gt(i32x4 a, i32x4 b)  { return i32x4_negate(_mm_cmpgt_epi32(a, b)); }
inline i32x4 i32x4_leq(i32x4 a, i32x4 b) { return i32x4_negate(_mm_cmple_epi32(a, b)); }
inline i32x4 i32x4_lt(i32x4 a, i32x4 b)  { return i32x4_negate(_mm_cmplt_epi32(a, b)); }

//Bitwise

inline i32x4 i32x4_or(i32x4 a, i32x4 b)  { return _mm_or_epi32(a, b); }
inline i32x4 i32x4_and(i32x4 a, i32x4 b) { return _mm_and_epi32(a, b); }
inline i32x4 i32x4_xor(i32x4 a, i32x4 b) { return _mm_xor_epi32(a, b); }

inline i32x4 i32x4_min(i32x4 a, i32x4 b) { return _mm_min_epi32(a, b); }
inline i32x4 i32x4_max(i32x4 a, i32x4 b) { return _mm_max_epi32(a, b); }

//Reduce

inline i32 i32x4_reduce(i32x4 a) {
    return i32x4_x(_mm_hadd_epi32(_mm_hadd_epi32(a, i32x4_zero()), i32x4_zero()));
}

//Float

//Arithmetic

inline f32x4 f32x4_add(f32x4 a, f32x4 b) { return _mm_add_ps(a, b); }
inline f32x4 f32x4_sub(f32x4 a, f32x4 b) { return _mm_sub_ps(a, b); }
inline f32x4 f32x4_mul(f32x4 a, f32x4 b) { return _mm_mul_ps(a, b); }
inline f32x4 f32x4_div(f32x4 a, f32x4 b) { return _mm_div_ps(a, b); }

inline f32x4 f32x4_ceil(f32x4 a) { return _mm_ceil_ps(a); }
inline f32x4 f32x4_floor(f32x4 a) { return _mm_floor_ps(a); }
inline f32x4 f32x4_round(f32x4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }

inline f32x4 f32x4_sqrt(f32x4 a) { return _mm_sqrt_ps(a); }
inline f32x4 f32x4_rsqrt(f32x4 a) { return _mm_rsqrt_ps(a); }

inline f32x4 f32x4_acos(f32x4 v) { return _mm_acos_ps(v); }
inline f32x4 f32x4_cos(f32x4 v) { return _mm_cos_ps(v); }
inline f32x4 f32x4_asin(f32x4 v) { return _mm_asin_ps(v); }
inline f32x4 f32x4_sin(f32x4 v) { return _mm_sin_ps(v); }
inline f32x4 f32x4_atan(f32x4 v) { return _mm_atan_ps(v); }
inline f32x4 f32x4_atan2(f32x4 y, f32x4 x) { return _mm_atan2_ps(y, x); }
inline f32x4 f32x4_tan(f32x4 v) { return _mm_tan_ps(v); }

inline f32x4 f32x4_loge(f32x4 v) { return _mm_log_ps(v); }
inline f32x4 f32x4_log10(f32x4 v) { return _mm_log10_ps(v); }
inline f32x4 f32x4_log2(f32x4 v) { return _mm_log2_ps(v); }

inline f32x4 f32x4_exp(f32x4 v) { return _mm_exp_ps(v); }
inline f32x4 f32x4_exp10(f32x4 v) { return _mm_exp10_ps(v); }
inline f32x4 f32x4_exp2(f32x4 v)  { return _mm_exp2_ps(v); }

//Swizzle

inline f32x4 f32x4_trunc2(f32x4 a) { return _mm_movelh_ps(a, f32x4_zero()); }
inline f32x4 f32x4_trunc3(f32x4 a) { 
    f32x4 z0 = f32x4_xzzz(_mm_movelh_ps(f32x4_zzzz(a), f32x4_zero()));
    return _mm_movelh_ps(a, z0); 
}

inline f32 f32x4_x(f32x4 a) { return _mm_cvtss_f32(a); }
inline f32 f32x4_y(f32x4 a) { return f32x4_x(f32x4_yyyy(a)); }
inline f32 f32x4_z(f32x4 a) { return f32x4_x(f32x4_zzzz(a)); }
inline f32 f32x4_w(f32x4 a) { return f32x4_x(f32x4_wwww(a)); }

inline f32x4 f32x4_init1(f32 x) { return _mm_set_ps(0, 0, 0, x); }
inline f32x4 f32x4_init2(f32 x, f32 y) { return _mm_set_ps(0, 0, y, x); }
inline f32x4 f32x4_init3(f32 x, f32 y, f32 z) { return _mm_set_ps(0, z, y, x); }
inline f32x4 f32x4_init4(f32 x, f32 y, f32 z, f32 w) { return _mm_set_ps(w, z, y, x); }

inline f32x4 f32x4_xxxx4(f32 x) { return _mm_set1_ps(x); }

inline f32x4 f32x4_load4(const f32 *arr) { return _mm_load_ps(arr);}

inline f32x4 f32x4_zero() { return _mm_setzero_ps(); }

//Cast

inline f32x4 f32x4_fromI32x4(i32x4 a) { return _mm_cvtepi32_ps(a); }
inline i32x4 i32x4_fromF32x4(f32x4 a) { return _mm_cvtps_epi32(a); }

//Compare

inline f32x4 f32x4_negate(f32x4 a);

inline f32x4 _f32x4_recastI32x4(f32x4 a) { return f32x4_fromI32x4(*(__m128i*) &a); }
inline f32x4 _f32x4_negateRecasti(f32x4 a) { return f32x4_negate(_f32x4_recastI32x4(a)); }

inline f32x4 f32x4_eq(f32x4 a, f32x4 b)  { return _f32x4_negateRecasti(_mm_cmpeq_ps(a, b)); }
inline f32x4 f32x4_neq(f32x4 a, f32x4 b) { return _f32x4_negateRecasti(_mm_cmpneq_ps(a, b)); }
inline f32x4 f32x4_geq(f32x4 a, f32x4 b) { return _f32x4_negateRecasti(_mm_cmpge_ps(a, b)); }
inline f32x4 f32x4_gt(f32x4 a, f32x4 b)  { return _f32x4_negateRecasti(_mm_cmpgt_ps(a, b)); }
inline f32x4 f32x4_leq(f32x4 a, f32x4 b) { return _f32x4_negateRecasti(_mm_cmple_ps(a, b)); }
inline f32x4 f32x4_lt(f32x4 a, f32x4 b)  { return _f32x4_negateRecasti(_mm_cmplt_ps(a, b)); }

inline f32x4 f32x4_or(f32x4 a, f32x4 b)  { return _f32x4_recastI32x4(_mm_or_ps(a, b)); }
inline f32x4 f32x4_and(f32x4 a, f32x4 b) { return _f32x4_recastI32x4(_mm_and_ps(a, b)); }
inline f32x4 f32x4_xor(f32x4 a, f32x4 b) { return _f32x4_recastI32x4(_mm_xor_ps(a, b)); }

inline f32x4 f32x4_min(f32x4 a, f32x4 b) { return _mm_min_ps(a, b); }
inline f32x4 f32x4_max(f32x4 a, f32x4 b) { return _mm_max_ps(a, b); }

//Some math operations

inline f32 f32x4_reduce(f32x4 a) {
    return f32x4_x(_mm_hadd_ps(_mm_hadd_ps(a, f32x4_zero()), f32x4_zero()));
}

inline f32 f32x4_dot4(f32x4 a, f32x4 b) { return f32x4_x(_mm_dp_ps(a, b, 0xFF)); }
inline f32 f32x4_dot2(f32x4 a, f32x4 b) { return f32x4_x(_mm_dp_ps(f32x4_xy(a), f32x4_xy(b), 0xFF)); }
inline f32 f32x4_dot3(f32x4 a, f32x4 b) { return f32x4_x(_mm_dp_ps(f32x4_xyz(a), f32x4_xyz(b), 0xFF)); }
