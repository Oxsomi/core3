#pragma once

inline f32x2 f32x2_zero();
inline f32x2 f32x2_one();
inline f32x2 f32x2_two();
inline f32x2 f32x2_negTwo();
inline f32x2 f32x2_negOne();
inline f32x2 f32x2_xx2(f32 x);

//Arithmetic

inline f32x2 f32x2_add(f32x2 a, f32x2 b);
inline f32x2 f32x2_sub(f32x2 a, f32x2 b);
inline f32x2 f32x2_mul(f32x2 a, f32x2 b);
inline f32x2 f32x2_div(f32x2 a, f32x2 b);

inline f32x2 f32x2_complement(f32x2 a) { return f32x2_sub(f32x2_one(), a); }
inline f32x2 f32x2_negate(f32x2 a) { return f32x2_sub(f32x2_zero(), a); }
inline f32x2 f32x2_inverse(f32x2 a) { return f32x2_div(f32x2_one(), a); }

inline f32x2 f32x2_pow2(f32x2 a) { return f32x2_mul(a, a); }

inline f32x2 f32x2_min(f32x2 a, f32x2 b);
inline f32x2 f32x2_max(f32x2 a, f32x2 b);
inline f32x2 f32x2_clamp(f32x2 a, f32x2 mi, f32x2 ma) { return f32x2_max(mi, f32x2_min(ma, a)); }
inline f32x2 f32x2_saturate(f32x2 a) { return f32x2_clamp(a, f32x2_zero(), f32x2_one()); }

inline f32x2 f32x2_acos(f32x2 v);
inline f32x2 f32x2_cos(f32x2 v);
inline f32x2 f32x2_asin(f32x2 v);
inline f32x2 f32x2_sin(f32x2 v);
inline f32x2 f32x2_atan(f32x2 v);
inline f32x2 f32x2_atan2(f32x2 y, f32x2 x);
inline f32x2 f32x2_tan(f32x2 v);
inline f32x2 f32x2_sqrt(f32x2 a);
inline f32x2 f32x2_rsqrt(f32x2 a);

inline f32x2 f32x2_sign(f32x2 v);     //Zero counts as signed too
inline f32x2 f32x2_abs(f32x2 v);
inline f32x2 f32x2_ceil(f32x2 v);
inline f32x2 f32x2_floor(f32x2 v);
inline f32x2 f32x2_round(f32x2 v);
inline f32x2 f32x2_fract(f32x2 v) { return f32x2_sub(v, f32x2_floor(v)); }
inline f32x2 f32x2_mod(f32x2 v, f32x2 d) { return f32x2_sub(v, f32x2_mul(f32x2_fract(f32x2_div(v, d)), d)); }

inline f32x2 f32x2_reflect(f32x2 I, f32x2 N);
inline f32 f32x2_dot(f32x2 a, f32x2 b);
inline f32 f32x2_satDot(f32x2 a, f32x2 b) { return Math_saturate(f32x2_dot(a, b)); }

inline f32 f32x2_sqLen(f32x2 v) { return f32x2_dot(v, v); }
inline f32 f32x2_len(f32x2 v) { return Math_sqrtf(f32x2_sqLen(v)); }
inline f32x2 f32x2_normalize(f32x2 v) { return f32x2_mul(v, f32x2_xx2(1 / f32x2_len(v))); }

inline f32x2 f32x2_pow(f32x2 v, f32x2 e);

inline f32x2 f32x2_loge(f32x2 v);
inline f32x2 f32x2_log10(f32x2 v);
inline f32x2 f32x2_log2(f32x2 v);

inline f32x2 f32x2_exp(f32x2 v);
inline f32x2 f32x2_exp10(f32x2 v);
inline f32x2 f32x2_exp2(f32x2 v);

inline f32 f32x2_reduce(f32x2 a);     //ax+ay

//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml

inline f32x2 f32x2_reflect(f32x2 I, f32x2 N) {
    return f32x2_sub(I, f32x2_mul(N, f32x2_xx2(2 * f32x2_dot(N, I))));
}

//Boolean / bitwise

inline bool f32x2_all(f32x2 a);
inline bool f32x2_any(f32x2 b);

inline f32x2 f32x2_eq(f32x2 a, f32x2 b);
inline f32x2 f32x2_neq(f32x2 a, f32x2 b);
inline f32x2 f32x2_geq(f32x2 a, f32x2 b);
inline f32x2 f32x2_gt(f32x2 a, f32x2 b);
inline f32x2 f32x2_leq(f32x2 a, f32x2 b);
inline f32x2 f32x2_lt(f32x2 a, f32x2 b);

inline bool f32x2_eq4(f32x2 a, f32x2 b) { return f32x2_all(f32x2_eq(a, b)); }
inline bool f32x2_neq4(f32x2 a, f32x2 b) { return !f32x2_eq4(a, b); }

inline f32x2 f32x2_fromI32x2(i32x2 a);
inline i32x2 i32x2_fromF32x2(f32x2 a);

inline i32x2 i32x2_fromI32x4(i32x4 a);
inline f32x2 f32x2_fromF32x4(f32x4 a);
inline i32x4 i32x4_fromI32x2(i32x2 a);
inline f32x4 f32x4_fromF32x2(f32x2 a);

//Swizzles and shizzle

inline f32 f32x2_x(f32x2 a);
inline f32 f32x2_y(f32x2 a);
inline f32 f32x2_get(f32x2 a, u8 i);

inline void f32x2_setX(f32x2 *a, f32 v);
inline void f32x2_setY(f32x2 *a, f32 v);
inline void f32x2_set(f32x2 *a, u8 i, f32 v);

//Construction

inline f32x2 f32x2_init1(f32 x);
inline f32x2 f32x2_init2(f32 x, f32 y);

inline f32x2 f32x2_load1(const f32 *arr);
inline f32x2 f32x2_load2(const f32 *arr);

inline f32x2 f32x2_xx(f32x2 a);
inline f32x2 f32x2_xy(f32x2 a) { return a; }
inline f32x2 f32x2_yx(f32x2 a);
inline f32x2 f32x2_yy(f32x2 a);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline f32x2 f32x2_sign(f32x2 v) {
    return f32x2_add(
        f32x2_mul(f32x2_lt(v, f32x2_zero()), f32x2_two()), 
        f32x2_one()
    ); 
}

inline f32x2 f32x2_abs(f32x2 v){ return f32x2_mul(f32x2_sign(v), v); }

//Simple definitions

inline f32x2 f32x2_one() { return f32x2_xx2(1); }
inline f32x2 f32x2_two() { return f32x2_xx2(2); }
inline f32x2 f32x2_negOne() { return f32x2_xx2(-1); }
inline f32x2 f32x2_negTwo() { return f32x2_xx2(-2); }

inline bool f32x2_all(f32x2 b) { return f32x2_reduce(f32x2_neq(b, f32x2_zero())) == 4; }
inline bool f32x2_any(f32x2 b) { return f32x2_reduce(f32x2_neq(b, f32x2_zero())); }

inline f32x2 f32x2_load1(const f32 *arr) { return f32x2_init1(*arr); }
inline f32x2 f32x2_load2(const f32 *arr) { return f32x2_init2(*arr, arr[1]); }

inline void f32x2_setX(f32x2 *a, f32 v) { *a = f32x2_init2(v, f32x2_y(*a)); }
inline void f32x2_setY(f32x2 *a, f32 v) { *a = f32x2_init2(f32x2_x(*a), v); }

inline void f32x2_set(f32x2 *a, u8 i, f32 v) {
    switch (i & 1) {
        case 0:      f32x2_setX(a, v);     break;
        default:     f32x2_setY(a, v);
    }
}

inline f32 f32x2_get(f32x2 a, u8 i) {
    switch (i & 1) {
        case 0:      return f32x2_x(a);
        default:     return f32x2_y(a);
    }
}

inline f32x2 f32x2_mul2x2(f32x2 v2, f32x2 v2x2[2]) {
    return f32x2_add(
        f32x2_mul(v2x2[0], f32x2_xx(v2)),
        f32x2_mul(v2x2[1], f32x2_yy(v2))
    );
}

//Casts from vec4f
    
inline f32x2 f32x2_fromF32x4(f32x4 a) { return f32x2_load2((const f32*) &a); }
inline f32x4 f32x4_fromF32x2(f32x2 a) { return f32x4_load2((const f32*) &a); }

inline f32x2 f32x4_xx(f32x4 a) { return f32x2_fromF32x4(f32x4_xxxx(a)); }
inline f32x2 f32x4_xy(f32x4 a) { return f32x2_fromF32x4(a); }
inline f32x2 f32x4_xz(f32x4 a) { return f32x2_fromF32x4(f32x4_xzxx(a)); }
inline f32x2 f32x4_xw(f32x4 a) { return f32x2_fromF32x4(f32x4_xwxx(a)); }

inline f32x2 f32x4_yx(f32x4 a) { return f32x2_fromF32x4(f32x4_yxxx(a)); }
inline f32x2 f32x4_yy(f32x4 a) { return f32x2_fromF32x4(f32x4_yyxx(a)); }
inline f32x2 f32x4_yz(f32x4 a) { return f32x2_fromF32x4(f32x4_yzxx(a)); }
inline f32x2 f32x4_yw(f32x4 a) { return f32x2_fromF32x4(f32x4_ywxx(a)); }

inline f32x2 f32x4_zx(f32x4 a) { return f32x2_fromF32x4(f32x4_zxxx(a)); }
inline f32x2 f32x4_zy(f32x4 a) { return f32x2_fromF32x4(f32x4_zyxx(a)); }
inline f32x2 f32x4_zz(f32x4 a) { return f32x2_fromF32x4(f32x4_zzxx(a)); }
inline f32x2 f32x4_zw(f32x4 a) { return f32x2_fromF32x4(f32x4_zwxx(a)); }

inline f32x2 f32x4_wx(f32x4 a) { return f32x2_fromF32x4(f32x4_wxxx(a)); }
inline f32x2 f32x4_wy(f32x4 a) { return f32x2_fromF32x4(f32x4_wyxx(a)); }
inline f32x2 f32x4_wz(f32x4 a) { return f32x2_fromF32x4(f32x4_wzxx(a)); }
inline f32x2 f32x4_ww(f32x4 a) { return f32x2_fromF32x4(f32x4_wwxx(a)); }

//Cast from vec2f to vec4

inline f32x4 f32x2_init2_2(f32x2 a, f32x2 b) { return f32x4_init4(f32x2_x(a), f32x2_y(a), f32x2_x(b), f32x2_y(b)); }

inline f32x4 f32x2_init2_1_1(f32x2 a, f32 b, f32 c) { return f32x4_init4(f32x2_x(a), f32x2_y(a), b, c); }
inline f32x4 f32x2_init1_2_1(f32 a, f32x2 b, f32 c) { return f32x4_init4(a, f32x2_x(b), f32x2_y(b), c); }
inline f32x4 f32x2_init1_1_2(f32 a, f32 b, f32x2 c) { return f32x4_init4(a, b, f32x2_x(c), f32x2_y(c)); }

inline f32x4 f32x2_init2_1(f32x2 a, f32 b) { return f32x4_init3(f32x2_x(a), f32x2_y(a), b); }
inline f32x4 f32x2_init1_2(f32 a, f32x2 b) { return f32x4_init3(a, f32x2_x(b), f32x2_y(b)); }
