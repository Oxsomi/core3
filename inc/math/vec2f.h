#pragma once

//TODO: Error checking

inline F32x2 F32x2_zero();
inline F32x2 F32x2_one();
inline F32x2 F32x2_two();
inline F32x2 F32x2_negTwo();
inline F32x2 F32x2_negOne();
inline F32x2 F32x2_xx2(F32 x);

//Arithmetic

inline F32x2 F32x2_add(F32x2 a, F32x2 b);
inline F32x2 F32x2_sub(F32x2 a, F32x2 b);
inline F32x2 F32x2_mul(F32x2 a, F32x2 b);
inline F32x2 F32x2_div(F32x2 a, F32x2 b);

inline F32x2 F32x2_complement(F32x2 a) { return F32x2_sub(F32x2_one(), a); }
inline F32x2 F32x2_negate(F32x2 a) { return F32x2_sub(F32x2_zero(), a); }
inline F32x2 F32x2_inverse(F32x2 a) { return F32x2_div(F32x2_one(), a); }

inline F32x2 F32x2_pow2(F32x2 a) { return F32x2_mul(a, a); }

inline F32x2 F32x2_min(F32x2 a, F32x2 b);
inline F32x2 F32x2_max(F32x2 a, F32x2 b);
inline F32x2 F32x2_clamp(F32x2 a, F32x2 mi, F32x2 ma) { return F32x2_max(mi, F32x2_min(ma, a)); }
inline F32x2 F32x2_saturate(F32x2 a) { return F32x2_clamp(a, F32x2_zero(), F32x2_one()); }

inline F32x2 F32x2_acos(F32x2 v);
inline F32x2 F32x2_cos(F32x2 v);
inline F32x2 F32x2_asin(F32x2 v);
inline F32x2 F32x2_sin(F32x2 v);
inline F32x2 F32x2_atan(F32x2 v);
inline F32x2 F32x2_atan2(F32x2 y, F32x2 x);
inline F32x2 F32x2_tan(F32x2 v);
inline F32x2 F32x2_sqrt(F32x2 a);
inline F32x2 F32x2_rsqrt(F32x2 a);

inline F32x2 F32x2_sign(F32x2 v);     //Zero counts as signed too
inline F32x2 F32x2_abs(F32x2 v);
inline F32x2 F32x2_ceil(F32x2 v);
inline F32x2 F32x2_floor(F32x2 v);
inline F32x2 F32x2_round(F32x2 v);
inline F32x2 F32x2_fract(F32x2 v) { return F32x2_sub(v, F32x2_floor(v)); }
inline F32x2 F32x2_mod(F32x2 v, F32x2 d) { return F32x2_sub(v, F32x2_mul(F32x2_fract(F32x2_div(v, d)), d)); }

inline F32x2 F32x2_reflect(F32x2 I, F32x2 N);
inline F32 F32x2_dot(F32x2 a, F32x2 b);
inline F32 F32x2_satDot(F32x2 a, F32x2 b) { return F32_saturate(F32x2_dot(a, b)); }

inline F32 F32x2_sqLen(F32x2 v) { return F32x2_dot(v, v); }
inline F32 F32x2_len(F32x2 v) { return F32_sqrt(F32x2_sqLen(v)); }
inline F32x2 F32x2_normalize(F32x2 v) { return F32x2_mul(v, F32x2_xx2(1 / F32x2_len(v))); }

inline F32x2 F32x2_pow(F32x2 v, F32x2 e);

inline F32x2 F32x2_loge(F32x2 v);
inline F32x2 F32x2_log10(F32x2 v);
inline F32x2 F32x2_log2(F32x2 v);

inline F32x2 F32x2_exp(F32x2 v);
inline F32x2 F32x2_exp10(F32x2 v);
inline F32x2 F32x2_exp2(F32x2 v);

inline F32 F32x2_reduce(F32x2 a);     //ax+ay

//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml

inline F32x2 F32x2_reflect(F32x2 I, F32x2 N) {
    return F32x2_sub(I, F32x2_mul(N, F32x2_xx2(2 * F32x2_dot(N, I))));
}

//Boolean / bitwise

inline Bool F32x2_all(F32x2 a);
inline Bool F32x2_any(F32x2 b);

inline F32x2 F32x2_eq(F32x2 a, F32x2 b);
inline F32x2 F32x2_neq(F32x2 a, F32x2 b);
inline F32x2 F32x2_geq(F32x2 a, F32x2 b);
inline F32x2 F32x2_gt(F32x2 a, F32x2 b);
inline F32x2 F32x2_leq(F32x2 a, F32x2 b);
inline F32x2 F32x2_lt(F32x2 a, F32x2 b);

inline Bool F32x2_eq2(F32x2 a, F32x2 b) { return F32x2_all(F32x2_eq(a, b)); }
inline Bool F32x2_neq2(F32x2 a, F32x2 b) { return !F32x2_eq2(a, b); }

inline F32x2 F32x2_fromI32x2(I32x2 a);
inline I32x2 I32x2_fromF32x2(F32x2 a);

inline I32x2 I32x2_fromI32x4(I32x4 a);
inline F32x2 F32x2_fromF32x4(F32x4 a);
inline I32x4 I32x4_fromI32x2(I32x2 a);
inline F32x4 F32x4_fromF32x2(F32x2 a);

//Swizzles and shizzle

inline F32 F32x2_x(F32x2 a);
inline F32 F32x2_y(F32x2 a);
inline F32 F32x2_get(F32x2 a, U8 i);

inline void F32x2_setX(F32x2 *a, F32 v);
inline void F32x2_setY(F32x2 *a, F32 v);
inline void F32x2_set(F32x2 *a, U8 i, F32 v);

//Construction

inline F32x2 F32x2_create1(F32 x);
inline F32x2 F32x2_create2(F32 x, F32 y);

inline F32x2 F32x2_load1(const F32 *arr);
inline F32x2 F32x2_load2(const F32 *arr);

inline F32x2 F32x2_xx(F32x2 a);
inline F32x2 F32x2_xy(F32x2 a) { return a; }
inline F32x2 F32x2_yx(F32x2 a);
inline F32x2 F32x2_yy(F32x2 a);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline F32x2 F32x2_sign(F32x2 v) {
    return F32x2_add(
        F32x2_mul(F32x2_lt(v, F32x2_zero()), F32x2_two()), 
        F32x2_one()
    ); 
}

inline F32x2 F32x2_abs(F32x2 v) { return F32x2_mul(F32x2_sign(v), v); }

//Simple defcreateions

inline F32x2 F32x2_one() { return F32x2_xx2(1); }
inline F32x2 F32x2_two() { return F32x2_xx2(2); }
inline F32x2 F32x2_negOne() { return F32x2_xx2(-1); }
inline F32x2 F32x2_negTwo() { return F32x2_xx2(-2); }

inline Bool F32x2_all(F32x2 b) { return F32x2_reduce(F32x2_neq(b, F32x2_zero())) == 4; }
inline Bool F32x2_any(F32x2 b) { return F32x2_reduce(F32x2_neq(b, F32x2_zero())); }

inline F32x2 F32x2_load1(const F32 *arr) { return F32x2_create1(*arr); }
inline F32x2 F32x2_load2(const F32 *arr) { return F32x2_create2(*arr, arr[1]); }

inline void F32x2_setX(F32x2 *a, F32 v) { *a = F32x2_create2(v, F32x2_y(*a)); }
inline void F32x2_setY(F32x2 *a, F32 v) { *a = F32x2_create2(F32x2_x(*a), v); }

inline void F32x2_set(F32x2 *a, U8 i, F32 v) {
    switch (i & 1) {
        case 0:      F32x2_setX(a, v);     break;
        default:     F32x2_setY(a, v);
    }
}

inline F32 F32x2_get(F32x2 a, U8 i) {
    switch (i & 1) {
        case 0:      return F32x2_x(a);
        default:     return F32x2_y(a);
    }
}

inline F32x2 F32x2_mul2x2(F32x2 v2, F32x2 v2x2[2]) {
    return F32x2_add(
        F32x2_mul(v2x2[0], F32x2_xx(v2)),
        F32x2_mul(v2x2[1], F32x2_yy(v2))
    );
}

//Casts from vec4f
    
inline F32x2 F32x2_fromF32x4(F32x4 a) { return F32x2_load2((const F32*) &a); }
inline F32x4 F32x4_fromF32x2(F32x2 a) { return F32x4_load2((const F32*) &a); }

inline F32x2 F32x4_xx(F32x4 a) { return F32x2_fromF32x4(F32x4_xxxx(a)); }
inline F32x2 F32x4_xy(F32x4 a) { return F32x2_fromF32x4(a); }
inline F32x2 F32x4_xz(F32x4 a) { return F32x2_fromF32x4(F32x4_xzxx(a)); }
inline F32x2 F32x4_xw(F32x4 a) { return F32x2_fromF32x4(F32x4_xwxx(a)); }

inline F32x2 F32x4_yx(F32x4 a) { return F32x2_fromF32x4(F32x4_yxxx(a)); }
inline F32x2 F32x4_yy(F32x4 a) { return F32x2_fromF32x4(F32x4_yyxx(a)); }
inline F32x2 F32x4_yz(F32x4 a) { return F32x2_fromF32x4(F32x4_yzxx(a)); }
inline F32x2 F32x4_yw(F32x4 a) { return F32x2_fromF32x4(F32x4_ywxx(a)); }

inline F32x2 F32x4_zx(F32x4 a) { return F32x2_fromF32x4(F32x4_zxxx(a)); }
inline F32x2 F32x4_zy(F32x4 a) { return F32x2_fromF32x4(F32x4_zyxx(a)); }
inline F32x2 F32x4_zz(F32x4 a) { return F32x2_fromF32x4(F32x4_zzxx(a)); }
inline F32x2 F32x4_zw(F32x4 a) { return F32x2_fromF32x4(F32x4_zwxx(a)); }

inline F32x2 F32x4_wx(F32x4 a) { return F32x2_fromF32x4(F32x4_wxxx(a)); }
inline F32x2 F32x4_wy(F32x4 a) { return F32x2_fromF32x4(F32x4_wyxx(a)); }
inline F32x2 F32x4_wz(F32x4 a) { return F32x2_fromF32x4(F32x4_wzxx(a)); }
inline F32x2 F32x4_ww(F32x4 a) { return F32x2_fromF32x4(F32x4_wwxx(a)); }

//Cast from vec2f to vec4

inline F32x4 F32x2_create2_2(F32x2 a, F32x2 b) { return F32x4_create4(F32x2_x(a), F32x2_y(a), F32x2_x(b), F32x2_y(b)); }

inline F32x4 F32x2_create2_1_1(F32x2 a, F32 b, F32 c) { return F32x4_create4(F32x2_x(a), F32x2_y(a), b, c); }
inline F32x4 F32x2_create1_2_1(F32 a, F32x2 b, F32 c) { return F32x4_create4(a, F32x2_x(b), F32x2_y(b), c); }
inline F32x4 F32x2_create1_1_2(F32 a, F32 b, F32x2 c) { return F32x4_create4(a, b, F32x2_x(c), F32x2_y(c)); }

inline F32x4 F32x2_create2_1(F32x2 a, F32 b) { return F32x4_create3(F32x2_x(a), F32x2_y(a), b); }
inline F32x4 F32x2_create1_2(F32 a, F32x2 b) { return F32x4_create3(a, F32x2_x(b), F32x2_y(b)); }
