#pragma once

//Arithmetic

inline f32x4 f32x4_zero();
inline f32x4 f32x4_one();
inline f32x4 f32x4_two();
inline f32x4 f32x4_negTwo();
inline f32x4 f32x4_negOne();
inline f32x4 f32x4_xxxx4(f32 x);

inline f32x4 f32x4_add(f32x4 a, f32x4 b);
inline f32x4 f32x4_sub(f32x4 a, f32x4 b);
inline f32x4 f32x4_mul(f32x4 a, f32x4 b);
inline f32x4 f32x4_div(f32x4 a, f32x4 b);

inline f32x4 f32x4_srgba8Unpack(u32 v);
inline u32 f32x4_srgba8Pack(f32x4 v);
inline f32x4 f32x4_rgb8Unpack(u32 v);
inline u32 f32x4_rgb8Pack(f32x4 v);

inline f32x4 f32x4_complement(f32x4 a) { return f32x4_sub(f32x4_one(), a); }
inline f32x4 f32x4_negate(f32x4 a) { return f32x4_sub(f32x4_zero(), a); }
inline f32x4 f32x4_inverse(f32x4 a) { return f32x4_div(f32x4_one(), a); }

inline f32x4 f32x4_pow2(f32x4 a) { return f32x4_mul(a, a); }

inline f32x4 f32x4_sign(f32x4 v);     //Zero counts as signed too
inline f32x4 f32x4_abs(f32x4 v);
inline f32x4 f32x4_ceil(f32x4 v);
inline f32x4 f32x4_floor(f32x4 v);
inline f32x4 f32x4_round(f32x4 v);
inline f32x4 f32x4_pow(f32x4 v, f32x4 e);
inline f32x4 f32x4_fract(f32x4 v) { return f32x4_sub(v, f32x4_floor(v)); }
inline f32x4 f32x4_mod(f32x4 v, f32x4 d) { return f32x4_mul(f32x4_fract(f32x4_div(v, d)), d); }

inline f32 f32x4_reduce(f32x4 a);     //ax+ay+az+aw

inline f32 f32x4_dot2(f32x4 a, f32x4 b);
inline f32 f32x4_dot3(f32x4 a, f32x4 b);
inline f32 f32x4_dot4(f32x4 a, f32x4 b);

inline f32 f32x4_satDot2(f32x4 X, f32x4 Y) { return Math_saturate(f32x4_dot2(X, Y)); }
inline f32 f32x4_satDot3(f32x4 X, f32x4 Y) { return Math_saturate(f32x4_dot3(X, Y)); }
inline f32 f32x4_satDot4(f32x4 X, f32x4 Y) { return Math_saturate(f32x4_dot4(X, Y)); }

inline f32x4 f32x4_reflect2(f32x4 I, f32x4 N);
inline f32x4 f32x4_reflect3(f32x4 I, f32x4 N);

inline f32 f32x4_sqLen2(f32x4 v) { return f32x4_dot2(v, v); }
inline f32 f32x4_sqLen3(f32x4 v) { return f32x4_dot3(v, v); }
inline f32 f32x4_sqLen4(f32x4 v) { return f32x4_dot4(v, v); }

inline f32 f32x4_len2(f32x4 v) { return Math_sqrtf(f32x4_sqLen2(v)); }
inline f32 f32x4_len3(f32x4 v) { return Math_sqrtf(f32x4_sqLen3(v)); }
inline f32 f32x4_len4(f32x4 v) { return Math_sqrtf(f32x4_sqLen4(v)); }

inline f32x4 f32x4_acos(f32x4 v);
inline f32x4 f32x4_cos(f32x4 v);
inline f32x4 f32x4_asin(f32x4 v);
inline f32x4 f32x4_sin(f32x4 v);
inline f32x4 f32x4_atan(f32x4 v);
inline f32x4 f32x4_atan2(f32x4 y, f32x4 x);
inline f32x4 f32x4_tan(f32x4 v);
inline f32x4 f32x4_sqrt(f32x4 a);
inline f32x4 f32x4_rsqrt(f32x4 a);

inline f32x4 f32x4_normalize2(f32x4 v) { return f32x4_mul(v, f32x4_rsqrt(f32x4_xxxx4(f32x4_sqLen2(v)))); }
inline f32x4 f32x4_normalize3(f32x4 v) { return f32x4_mul(v, f32x4_rsqrt(f32x4_xxxx4(f32x4_sqLen3(v)))); }
inline f32x4 f32x4_normalize4(f32x4 v) { return f32x4_mul(v, f32x4_rsqrt(f32x4_xxxx4(f32x4_sqLen4(v)))); }

inline f32x4 f32x4_loge(f32x4 v);
inline f32x4 f32x4_log10(f32x4 v);
inline f32x4 f32x4_log2(f32x4 v);

inline f32x4 f32x4_exp(f32x4 v);
inline f32x4 f32x4_exp10(f32x4 v);
inline f32x4 f32x4_exp2(f32x4 v);

inline f32x4 f32x4_cross3(f32x4 a, f32x4 b);

inline f32x4 f32x4_lerp(f32x4 a, f32x4 b, f32 perc) { 
    b = f32x4_sub(b, a);
    return f32x4_add(a, f32x4_mul(b, f32x4_xxxx4(perc)));
}

inline f32x4 f32x4_min(f32x4 a, f32x4 b);
inline f32x4 f32x4_max(f32x4 a, f32x4 b);
inline f32x4 f32x4_clamp(f32x4 a, f32x4 mi, f32x4 ma) { return f32x4_max(mi, f32x4_min(ma, a)); }
inline f32x4 f32x4_saturate(f32x4 a) { return f32x4_clamp(a, f32x4_zero(), f32x4_one()); }

//Boolean

inline bool f32x4_all(f32x4 a);
inline bool f32x4_any(f32x4 b);

inline f32x4 f32x4_eq(f32x4 a, f32x4 b);
inline f32x4 f32x4_neq(f32x4 a, f32x4 b);
inline f32x4 f32x4_geq(f32x4 a, f32x4 b);
inline f32x4 f32x4_gt(f32x4 a, f32x4 b);
inline f32x4 f32x4_leq(f32x4 a, f32x4 b);
inline f32x4 f32x4_lt(f32x4 a, f32x4 b);

inline bool f32x4_eq4(f32x4 a, f32x4 b) { return f32x4_all(f32x4_eq(a, b)); }
inline bool f32x4_neq4(f32x4 a, f32x4 b) { return !f32x4_eq4(a, b); }

//Swizzles and shizzle

inline f32 f32x4_x(f32x4 a);
inline f32 f32x4_y(f32x4 a);
inline f32 f32x4_z(f32x4 a);
inline f32 f32x4_w(f32x4 a);
inline f32 f32x4_get(f32x4 a, u8 i);

inline void f32x4_setX(f32x4 *a, f32 v);
inline void f32x4_setY(f32x4 *a, f32 v);
inline void f32x4_setZ(f32x4 *a, f32 v);
inline void f32x4_setW(f32x4 *a, f32 v);
inline void f32x4_set(f32x4 *a, u8 i, f32 v);

//Construction

inline f32x4 f32x4_init1(f32 x);
inline f32x4 f32x4_init2(f32 x, f32 y);
inline f32x4 f32x4_init3(f32 x, f32 y, f32 z);
inline f32x4 f32x4_init4(f32 x, f32 y, f32 z, f32 w);

inline f32x4 f32x4_load1(const f32 *arr);
inline f32x4 f32x4_load2(const f32 *arr);
inline f32x4 f32x4_load3(const f32 *arr);
inline f32x4 f32x4_load4(const f32 *arr) { return *(const f32x4*) arr; }

//Shuffle and extracting values

#define _shufflef1(a, x) _shufflef(a, x, x, x, x)

//4D swizzles

inline f32x4 f32x4_xxxx(f32x4 a) { return _shufflef1(a, 0); }
inline f32x4 f32x4_xxxy(f32x4 a) { return _shufflef(a, 0, 0, 0, 1); }
inline f32x4 f32x4_xxxz(f32x4 a) { return _shufflef(a, 0, 0, 0, 2); }
inline f32x4 f32x4_xxxw(f32x4 a) { return _shufflef(a, 0, 0, 0, 3); }
inline f32x4 f32x4_xxyx(f32x4 a) { return _shufflef(a, 0, 0, 1, 0); }
inline f32x4 f32x4_xxyy(f32x4 a) { return _shufflef(a, 0, 0, 1, 1); }
inline f32x4 f32x4_xxyz(f32x4 a) { return _shufflef(a, 0, 0, 1, 2); }
inline f32x4 f32x4_xxyw(f32x4 a) { return _shufflef(a, 0, 0, 1, 3); }
inline f32x4 f32x4_xxzx(f32x4 a) { return _shufflef(a, 0, 0, 2, 0); }
inline f32x4 f32x4_xxzy(f32x4 a) { return _shufflef(a, 0, 0, 2, 1); }
inline f32x4 f32x4_xxzz(f32x4 a) { return _shufflef(a, 0, 0, 2, 2); }
inline f32x4 f32x4_xxzw(f32x4 a) { return _shufflef(a, 0, 0, 2, 3); }
inline f32x4 f32x4_xxwx(f32x4 a) { return _shufflef(a, 0, 0, 3, 0); }
inline f32x4 f32x4_xxwy(f32x4 a) { return _shufflef(a, 0, 0, 3, 1); }
inline f32x4 f32x4_xxwz(f32x4 a) { return _shufflef(a, 0, 0, 3, 2); }
inline f32x4 f32x4_xxww(f32x4 a) { return _shufflef(a, 0, 0, 3, 3); }

inline f32x4 f32x4_xyxx(f32x4 a) { return _shufflef(a, 0, 1, 0, 0); }
inline f32x4 f32x4_xyxy(f32x4 a) { return _shufflef(a, 0, 1, 0, 1); }
inline f32x4 f32x4_xyxz(f32x4 a) { return _shufflef(a, 0, 1, 0, 2); }
inline f32x4 f32x4_xyxw(f32x4 a) { return _shufflef(a, 0, 1, 0, 3); }
inline f32x4 f32x4_xyyx(f32x4 a) { return _shufflef(a, 0, 1, 1, 0); }
inline f32x4 f32x4_xyyy(f32x4 a) { return _shufflef(a, 0, 1, 1, 1); }
inline f32x4 f32x4_xyyz(f32x4 a) { return _shufflef(a, 0, 1, 1, 2); }
inline f32x4 f32x4_xyyw(f32x4 a) { return _shufflef(a, 0, 1, 1, 3); }
inline f32x4 f32x4_xyzx(f32x4 a) { return _shufflef(a, 0, 1, 2, 0); }
inline f32x4 f32x4_xyzy(f32x4 a) { return _shufflef(a, 0, 1, 2, 1); }
inline f32x4 f32x4_xyzz(f32x4 a) { return _shufflef(a, 0, 1, 2, 2); }
inline f32x4 f32x4_xyzw(f32x4 a) { return a; }
inline f32x4 f32x4_xywx(f32x4 a) { return _shufflef(a, 0, 1, 3, 0); }
inline f32x4 f32x4_xywy(f32x4 a) { return _shufflef(a, 0, 1, 3, 1); }
inline f32x4 f32x4_xywz(f32x4 a) { return _shufflef(a, 0, 1, 3, 2); }
inline f32x4 f32x4_xyww(f32x4 a) { return _shufflef(a, 0, 1, 3, 3); }

inline f32x4 f32x4_xzxx(f32x4 a) { return _shufflef(a, 0, 2, 0, 0); }
inline f32x4 f32x4_xzxy(f32x4 a) { return _shufflef(a, 0, 2, 0, 1); }
inline f32x4 f32x4_xzxz(f32x4 a) { return _shufflef(a, 0, 2, 0, 2); }
inline f32x4 f32x4_xzxw(f32x4 a) { return _shufflef(a, 0, 2, 0, 3); }
inline f32x4 f32x4_xzyx(f32x4 a) { return _shufflef(a, 0, 2, 1, 0); }
inline f32x4 f32x4_xzyy(f32x4 a) { return _shufflef(a, 0, 2, 1, 1); }
inline f32x4 f32x4_xzyz(f32x4 a) { return _shufflef(a, 0, 2, 1, 2); }
inline f32x4 f32x4_xzyw(f32x4 a) { return _shufflef(a, 0, 2, 1, 3); }
inline f32x4 f32x4_xzzx(f32x4 a) { return _shufflef(a, 0, 2, 2, 0); }
inline f32x4 f32x4_xzzy(f32x4 a) { return _shufflef(a, 0, 2, 2, 1); }
inline f32x4 f32x4_xzzz(f32x4 a) { return _shufflef(a, 0, 2, 2, 2); }
inline f32x4 f32x4_xzzw(f32x4 a) { return _shufflef(a, 0, 2, 2, 3); }
inline f32x4 f32x4_xzwx(f32x4 a) { return _shufflef(a, 0, 2, 3, 0); }
inline f32x4 f32x4_xzwy(f32x4 a) { return _shufflef(a, 0, 2, 3, 1); }
inline f32x4 f32x4_xzwz(f32x4 a) { return _shufflef(a, 0, 2, 3, 2); }
inline f32x4 f32x4_xzww(f32x4 a) { return _shufflef(a, 0, 2, 3, 3); }

inline f32x4 f32x4_xwxx(f32x4 a) { return _shufflef(a, 0, 3, 0, 0); }
inline f32x4 f32x4_xwxy(f32x4 a) { return _shufflef(a, 0, 3, 0, 1); }
inline f32x4 f32x4_xwxz(f32x4 a) { return _shufflef(a, 0, 3, 0, 2); }
inline f32x4 f32x4_xwxw(f32x4 a) { return _shufflef(a, 0, 3, 0, 3); }
inline f32x4 f32x4_xwyx(f32x4 a) { return _shufflef(a, 0, 3, 1, 0); }
inline f32x4 f32x4_xwyy(f32x4 a) { return _shufflef(a, 0, 3, 1, 1); }
inline f32x4 f32x4_xwyz(f32x4 a) { return _shufflef(a, 0, 3, 1, 2); }
inline f32x4 f32x4_xwyw(f32x4 a) { return _shufflef(a, 0, 3, 1, 3); }
inline f32x4 f32x4_xwzx(f32x4 a) { return _shufflef(a, 0, 3, 2, 0); }
inline f32x4 f32x4_xwzy(f32x4 a) { return _shufflef(a, 0, 3, 2, 1); }
inline f32x4 f32x4_xwzz(f32x4 a) { return _shufflef(a, 0, 3, 2, 2); }
inline f32x4 f32x4_xwzw(f32x4 a) { return _shufflef(a, 0, 3, 2, 3); }
inline f32x4 f32x4_xwwx(f32x4 a) { return _shufflef(a, 0, 3, 3, 0); }
inline f32x4 f32x4_xwwy(f32x4 a) { return _shufflef(a, 0, 3, 3, 1); }
inline f32x4 f32x4_xwwz(f32x4 a) { return _shufflef(a, 0, 3, 3, 2); }
inline f32x4 f32x4_xwww(f32x4 a) { return _shufflef(a, 0, 3, 3, 3); }

inline f32x4 f32x4_yxxx(f32x4 a) { return _shufflef(a, 1, 0, 0, 0); }
inline f32x4 f32x4_yxxy(f32x4 a) { return _shufflef(a, 1, 0, 0, 1); }
inline f32x4 f32x4_yxxz(f32x4 a) { return _shufflef(a, 1, 0, 0, 2); }
inline f32x4 f32x4_yxxw(f32x4 a) { return _shufflef(a, 1, 0, 0, 3); }
inline f32x4 f32x4_yxyx(f32x4 a) { return _shufflef(a, 1, 0, 1, 0); }
inline f32x4 f32x4_yxyy(f32x4 a) { return _shufflef(a, 1, 0, 1, 1); }
inline f32x4 f32x4_yxyz(f32x4 a) { return _shufflef(a, 1, 0, 1, 2); }
inline f32x4 f32x4_yxyw(f32x4 a) { return _shufflef(a, 1, 0, 1, 3); }
inline f32x4 f32x4_yxzx(f32x4 a) { return _shufflef(a, 1, 0, 2, 0); }
inline f32x4 f32x4_yxzy(f32x4 a) { return _shufflef(a, 1, 0, 2, 1); }
inline f32x4 f32x4_yxzz(f32x4 a) { return _shufflef(a, 1, 0, 2, 2); }
inline f32x4 f32x4_yxzw(f32x4 a) { return _shufflef(a, 1, 0, 2, 3); }
inline f32x4 f32x4_yxwx(f32x4 a) { return _shufflef(a, 1, 0, 3, 0); }
inline f32x4 f32x4_yxwy(f32x4 a) { return _shufflef(a, 1, 0, 3, 1); }
inline f32x4 f32x4_yxwz(f32x4 a) { return _shufflef(a, 1, 0, 3, 2); }
inline f32x4 f32x4_yxww(f32x4 a) { return _shufflef(a, 1, 0, 3, 3); }

inline f32x4 f32x4_yyxx(f32x4 a) { return _shufflef(a, 1, 1, 0, 0); }
inline f32x4 f32x4_yyxy(f32x4 a) { return _shufflef(a, 1, 1, 0, 1); }
inline f32x4 f32x4_yyxz(f32x4 a) { return _shufflef(a, 1, 1, 0, 2); }
inline f32x4 f32x4_yyxw(f32x4 a) { return _shufflef(a, 1, 1, 0, 3); }
inline f32x4 f32x4_yyyx(f32x4 a) { return _shufflef(a, 1, 1, 1, 0); }
inline f32x4 f32x4_yyyy(f32x4 a) { return _shufflef1(a, 1); } 
inline f32x4 f32x4_yyyz(f32x4 a) { return _shufflef(a, 1, 1, 1, 2); }
inline f32x4 f32x4_yyyw(f32x4 a) { return _shufflef(a, 1, 1, 1, 3); }
inline f32x4 f32x4_yyzx(f32x4 a) { return _shufflef(a, 1, 1, 2, 0); }
inline f32x4 f32x4_yyzy(f32x4 a) { return _shufflef(a, 1, 1, 2, 1); }
inline f32x4 f32x4_yyzz(f32x4 a) { return _shufflef(a, 1, 1, 2, 2); }
inline f32x4 f32x4_yyzw(f32x4 a) { return _shufflef(a, 1, 1, 2, 3); }
inline f32x4 f32x4_yywx(f32x4 a) { return _shufflef(a, 1, 1, 3, 0); }
inline f32x4 f32x4_yywy(f32x4 a) { return _shufflef(a, 1, 1, 3, 1); }
inline f32x4 f32x4_yywz(f32x4 a) { return _shufflef(a, 1, 1, 3, 2); }
inline f32x4 f32x4_yyww(f32x4 a) { return _shufflef(a, 1, 1, 3, 3); }

inline f32x4 f32x4_yzxx(f32x4 a) { return _shufflef(a, 1, 2, 0, 0); }
inline f32x4 f32x4_yzxy(f32x4 a) { return _shufflef(a, 1, 2, 0, 1); }
inline f32x4 f32x4_yzxz(f32x4 a) { return _shufflef(a, 1, 2, 0, 2); }
inline f32x4 f32x4_yzxw(f32x4 a) { return _shufflef(a, 1, 2, 0, 3); }
inline f32x4 f32x4_yzyx(f32x4 a) { return _shufflef(a, 1, 2, 1, 0); }
inline f32x4 f32x4_yzyy(f32x4 a) { return _shufflef(a, 1, 2, 1, 1); }
inline f32x4 f32x4_yzyz(f32x4 a) { return _shufflef(a, 1, 2, 1, 2); }
inline f32x4 f32x4_yzyw(f32x4 a) { return _shufflef(a, 1, 2, 1, 3); }
inline f32x4 f32x4_yzzx(f32x4 a) { return _shufflef(a, 1, 2, 2, 0); }
inline f32x4 f32x4_yzzy(f32x4 a) { return _shufflef(a, 1, 2, 2, 1); }
inline f32x4 f32x4_yzzz(f32x4 a) { return _shufflef(a, 1, 2, 2, 2); }
inline f32x4 f32x4_yzzw(f32x4 a) { return _shufflef(a, 1, 2, 2, 3); }
inline f32x4 f32x4_yzwx(f32x4 a) { return _shufflef(a, 1, 2, 3, 0); }
inline f32x4 f32x4_yzwy(f32x4 a) { return _shufflef(a, 1, 2, 3, 1); }
inline f32x4 f32x4_yzwz(f32x4 a) { return _shufflef(a, 1, 2, 3, 2); }
inline f32x4 f32x4_yzww(f32x4 a) { return _shufflef(a, 1, 2, 3, 3); }

inline f32x4 f32x4_ywxx(f32x4 a) { return _shufflef(a, 1, 3, 0, 0); }
inline f32x4 f32x4_ywxy(f32x4 a) { return _shufflef(a, 1, 3, 0, 1); }
inline f32x4 f32x4_ywxz(f32x4 a) { return _shufflef(a, 1, 3, 0, 2); }
inline f32x4 f32x4_ywxw(f32x4 a) { return _shufflef(a, 1, 3, 0, 3); }
inline f32x4 f32x4_ywyx(f32x4 a) { return _shufflef(a, 1, 3, 1, 0); }
inline f32x4 f32x4_ywyy(f32x4 a) { return _shufflef(a, 1, 3, 1, 1); }
inline f32x4 f32x4_ywyz(f32x4 a) { return _shufflef(a, 1, 3, 1, 2); }
inline f32x4 f32x4_ywyw(f32x4 a) { return _shufflef(a, 1, 3, 1, 3); }
inline f32x4 f32x4_ywzx(f32x4 a) { return _shufflef(a, 1, 3, 2, 0); }
inline f32x4 f32x4_ywzy(f32x4 a) { return _shufflef(a, 1, 3, 2, 1); }
inline f32x4 f32x4_ywzz(f32x4 a) { return _shufflef(a, 1, 3, 2, 2); }
inline f32x4 f32x4_ywzw(f32x4 a) { return _shufflef(a, 1, 3, 2, 3); }
inline f32x4 f32x4_ywwx(f32x4 a) { return _shufflef(a, 1, 3, 3, 0); }
inline f32x4 f32x4_ywwy(f32x4 a) { return _shufflef(a, 1, 3, 3, 1); }
inline f32x4 f32x4_ywwz(f32x4 a) { return _shufflef(a, 1, 3, 3, 2); }
inline f32x4 f32x4_ywww(f32x4 a) { return _shufflef(a, 1, 3, 3, 3); }

inline f32x4 f32x4_zxxx(f32x4 a) { return _shufflef(a, 2, 0, 0, 0); }
inline f32x4 f32x4_zxxy(f32x4 a) { return _shufflef(a, 2, 0, 0, 1); }
inline f32x4 f32x4_zxxz(f32x4 a) { return _shufflef(a, 2, 0, 0, 2); }
inline f32x4 f32x4_zxxw(f32x4 a) { return _shufflef(a, 2, 0, 0, 3); }
inline f32x4 f32x4_zxyx(f32x4 a) { return _shufflef(a, 2, 0, 1, 0); }
inline f32x4 f32x4_zxyy(f32x4 a) { return _shufflef(a, 2, 0, 1, 1); }
inline f32x4 f32x4_zxyz(f32x4 a) { return _shufflef(a, 2, 0, 1, 2); }
inline f32x4 f32x4_zxyw(f32x4 a) { return _shufflef(a, 2, 0, 1, 3); }
inline f32x4 f32x4_zxzx(f32x4 a) { return _shufflef(a, 2, 0, 2, 0); }
inline f32x4 f32x4_zxzy(f32x4 a) { return _shufflef(a, 2, 0, 2, 1); }
inline f32x4 f32x4_zxzz(f32x4 a) { return _shufflef(a, 2, 0, 2, 2); }
inline f32x4 f32x4_zxzw(f32x4 a) { return _shufflef(a, 2, 0, 2, 3); }
inline f32x4 f32x4_zxwx(f32x4 a) { return _shufflef(a, 2, 0, 3, 0); }
inline f32x4 f32x4_zxwy(f32x4 a) { return _shufflef(a, 2, 0, 3, 1); }
inline f32x4 f32x4_zxwz(f32x4 a) { return _shufflef(a, 2, 0, 3, 2); }
inline f32x4 f32x4_zxww(f32x4 a) { return _shufflef(a, 2, 0, 3, 3); }

inline f32x4 f32x4_zyxx(f32x4 a) { return _shufflef(a, 2, 1, 0, 0); }
inline f32x4 f32x4_zyxy(f32x4 a) { return _shufflef(a, 2, 1, 0, 1); }
inline f32x4 f32x4_zyxz(f32x4 a) { return _shufflef(a, 2, 1, 0, 2); }
inline f32x4 f32x4_zyxw(f32x4 a) { return _shufflef(a, 2, 1, 0, 3); }
inline f32x4 f32x4_zyyx(f32x4 a) { return _shufflef(a, 2, 1, 1, 0); }
inline f32x4 f32x4_zyyy(f32x4 a) { return _shufflef(a, 2, 1, 1, 1); }
inline f32x4 f32x4_zyyz(f32x4 a) { return _shufflef(a, 2, 1, 1, 2); }
inline f32x4 f32x4_zyyw(f32x4 a) { return _shufflef(a, 2, 1, 1, 3); }
inline f32x4 f32x4_zyzx(f32x4 a) { return _shufflef(a, 2, 1, 2, 0); }
inline f32x4 f32x4_zyzy(f32x4 a) { return _shufflef(a, 2, 1, 2, 1); }
inline f32x4 f32x4_zyzz(f32x4 a) { return _shufflef(a, 2, 1, 2, 2); }
inline f32x4 f32x4_zyzw(f32x4 a) { return _shufflef(a, 2, 1, 2, 3); }
inline f32x4 f32x4_zywx(f32x4 a) { return _shufflef(a, 2, 1, 3, 0); }
inline f32x4 f32x4_zywy(f32x4 a) { return _shufflef(a, 2, 1, 3, 1); }
inline f32x4 f32x4_zywz(f32x4 a) { return _shufflef(a, 2, 1, 3, 2); }
inline f32x4 f32x4_zyww(f32x4 a) { return _shufflef(a, 2, 1, 3, 3); }

inline f32x4 f32x4_zzxx(f32x4 a) { return _shufflef(a, 2, 2, 0, 0); }
inline f32x4 f32x4_zzxy(f32x4 a) { return _shufflef(a, 2, 2, 0, 1); }
inline f32x4 f32x4_zzxz(f32x4 a) { return _shufflef(a, 2, 2, 0, 2); }
inline f32x4 f32x4_zzxw(f32x4 a) { return _shufflef(a, 2, 2, 0, 3); }
inline f32x4 f32x4_zzyx(f32x4 a) { return _shufflef(a, 2, 2, 1, 0); }
inline f32x4 f32x4_zzyy(f32x4 a) { return _shufflef(a, 2, 2, 1, 1); }
inline f32x4 f32x4_zzyz(f32x4 a) { return _shufflef(a, 2, 2, 1, 2); }
inline f32x4 f32x4_zzyw(f32x4 a) { return _shufflef(a, 2, 2, 1, 3); }
inline f32x4 f32x4_zzzx(f32x4 a) { return _shufflef(a, 2, 2, 2, 0); }
inline f32x4 f32x4_zzzy(f32x4 a) { return _shufflef(a, 2, 2, 2, 1); }
inline f32x4 f32x4_zzzz(f32x4 a) { return _shufflef1(a, 2); }
inline f32x4 f32x4_zzzw(f32x4 a) { return _shufflef(a, 2, 2, 2, 3); }
inline f32x4 f32x4_zzwx(f32x4 a) { return _shufflef(a, 2, 2, 3, 0); }
inline f32x4 f32x4_zzwy(f32x4 a) { return _shufflef(a, 2, 2, 3, 1); }
inline f32x4 f32x4_zzwz(f32x4 a) { return _shufflef(a, 2, 2, 3, 2); }
inline f32x4 f32x4_zzww(f32x4 a) { return _shufflef(a, 2, 2, 3, 3); }

inline f32x4 f32x4_zwxx(f32x4 a) { return _shufflef(a, 2, 3, 0, 0); }
inline f32x4 f32x4_zwxy(f32x4 a) { return _shufflef(a, 2, 3, 0, 1); }
inline f32x4 f32x4_zwxz(f32x4 a) { return _shufflef(a, 2, 3, 0, 2); }
inline f32x4 f32x4_zwxw(f32x4 a) { return _shufflef(a, 2, 3, 0, 3); }
inline f32x4 f32x4_zwyx(f32x4 a) { return _shufflef(a, 2, 3, 1, 0); }
inline f32x4 f32x4_zwyy(f32x4 a) { return _shufflef(a, 2, 3, 1, 1); }
inline f32x4 f32x4_zwyz(f32x4 a) { return _shufflef(a, 2, 3, 1, 2); }
inline f32x4 f32x4_zwyw(f32x4 a) { return _shufflef(a, 2, 3, 1, 3); }
inline f32x4 f32x4_zwzx(f32x4 a) { return _shufflef(a, 2, 3, 2, 0); }
inline f32x4 f32x4_zwzy(f32x4 a) { return _shufflef(a, 2, 3, 2, 1); }
inline f32x4 f32x4_zwzz(f32x4 a) { return _shufflef(a, 2, 3, 2, 2); }
inline f32x4 f32x4_zwzw(f32x4 a) { return _shufflef(a, 2, 3, 2, 3); }
inline f32x4 f32x4_zwwx(f32x4 a) { return _shufflef(a, 2, 3, 3, 0); }
inline f32x4 f32x4_zwwy(f32x4 a) { return _shufflef(a, 2, 3, 3, 1); }
inline f32x4 f32x4_zwwz(f32x4 a) { return _shufflef(a, 2, 3, 3, 2); }
inline f32x4 f32x4_zwww(f32x4 a) { return _shufflef(a, 2, 3, 3, 3); }

inline f32x4 f32x4_wxxx(f32x4 a) { return _shufflef(a, 3, 0, 0, 0); }
inline f32x4 f32x4_wxxy(f32x4 a) { return _shufflef(a, 3, 0, 0, 1); }
inline f32x4 f32x4_wxxz(f32x4 a) { return _shufflef(a, 3, 0, 0, 2); }
inline f32x4 f32x4_wxxw(f32x4 a) { return _shufflef(a, 3, 0, 0, 3); }
inline f32x4 f32x4_wxyx(f32x4 a) { return _shufflef(a, 3, 0, 1, 0); }
inline f32x4 f32x4_wxyy(f32x4 a) { return _shufflef(a, 3, 0, 1, 1); }
inline f32x4 f32x4_wxyz(f32x4 a) { return _shufflef(a, 3, 0, 1, 2); }
inline f32x4 f32x4_wxyw(f32x4 a) { return _shufflef(a, 3, 0, 1, 3); }
inline f32x4 f32x4_wxzx(f32x4 a) { return _shufflef(a, 3, 0, 2, 0); }
inline f32x4 f32x4_wxzy(f32x4 a) { return _shufflef(a, 3, 0, 2, 1); }
inline f32x4 f32x4_wxzz(f32x4 a) { return _shufflef(a, 3, 0, 2, 2); }
inline f32x4 f32x4_wxzw(f32x4 a) { return _shufflef(a, 3, 0, 2, 3); }
inline f32x4 f32x4_wxwx(f32x4 a) { return _shufflef(a, 3, 0, 3, 0); }
inline f32x4 f32x4_wxwy(f32x4 a) { return _shufflef(a, 3, 0, 3, 1); }
inline f32x4 f32x4_wxwz(f32x4 a) { return _shufflef(a, 3, 0, 3, 2); }
inline f32x4 f32x4_wxww(f32x4 a) { return _shufflef(a, 3, 0, 3, 3); }

inline f32x4 f32x4_wyxx(f32x4 a) { return _shufflef(a, 3, 1, 0, 0); }
inline f32x4 f32x4_wyxy(f32x4 a) { return _shufflef(a, 3, 1, 0, 1); }
inline f32x4 f32x4_wyxz(f32x4 a) { return _shufflef(a, 3, 1, 0, 2); }
inline f32x4 f32x4_wyxw(f32x4 a) { return _shufflef(a, 3, 1, 0, 3); }
inline f32x4 f32x4_wyyx(f32x4 a) { return _shufflef(a, 3, 1, 1, 0); }
inline f32x4 f32x4_wyyy(f32x4 a) { return _shufflef(a, 3, 1, 1, 1); }
inline f32x4 f32x4_wyyz(f32x4 a) { return _shufflef(a, 3, 1, 1, 2); }
inline f32x4 f32x4_wyyw(f32x4 a) { return _shufflef(a, 3, 1, 1, 3); }
inline f32x4 f32x4_wyzx(f32x4 a) { return _shufflef(a, 3, 1, 2, 0); }
inline f32x4 f32x4_wyzy(f32x4 a) { return _shufflef(a, 3, 1, 2, 1); }
inline f32x4 f32x4_wyzz(f32x4 a) { return _shufflef(a, 3, 1, 2, 2); }
inline f32x4 f32x4_wyzw(f32x4 a) { return _shufflef(a, 3, 1, 2, 3); }
inline f32x4 f32x4_wywx(f32x4 a) { return _shufflef(a, 3, 1, 3, 0); }
inline f32x4 f32x4_wywy(f32x4 a) { return _shufflef(a, 3, 1, 3, 1); }
inline f32x4 f32x4_wywz(f32x4 a) { return _shufflef(a, 3, 1, 3, 2); }
inline f32x4 f32x4_wyww(f32x4 a) { return _shufflef(a, 3, 1, 3, 3); }

inline f32x4 f32x4_wzxx(f32x4 a) { return _shufflef(a, 3, 2, 0, 0); }
inline f32x4 f32x4_wzxy(f32x4 a) { return _shufflef(a, 3, 2, 0, 1); }
inline f32x4 f32x4_wzxz(f32x4 a) { return _shufflef(a, 3, 2, 0, 2); }
inline f32x4 f32x4_wzxw(f32x4 a) { return _shufflef(a, 3, 2, 0, 3); }
inline f32x4 f32x4_wzyx(f32x4 a) { return _shufflef(a, 3, 2, 1, 0); }
inline f32x4 f32x4_wzyy(f32x4 a) { return _shufflef(a, 3, 2, 1, 1); }
inline f32x4 f32x4_wzyz(f32x4 a) { return _shufflef(a, 3, 2, 1, 2); }
inline f32x4 f32x4_wzyw(f32x4 a) { return _shufflef(a, 3, 2, 1, 3); }
inline f32x4 f32x4_wzzx(f32x4 a) { return _shufflef(a, 3, 2, 2, 0); }
inline f32x4 f32x4_wzzy(f32x4 a) { return _shufflef(a, 3, 2, 2, 1); }
inline f32x4 f32x4_wzzz(f32x4 a) { return _shufflef(a, 3, 2, 2, 2); }
inline f32x4 f32x4_wzzw(f32x4 a) { return _shufflef(a, 3, 2, 2, 3); }
inline f32x4 f32x4_wzwx(f32x4 a) { return _shufflef(a, 3, 2, 3, 0); }
inline f32x4 f32x4_wzwy(f32x4 a) { return _shufflef(a, 3, 2, 3, 1); }
inline f32x4 f32x4_wzwz(f32x4 a) { return _shufflef(a, 3, 2, 3, 2); }
inline f32x4 f32x4_wzww(f32x4 a) { return _shufflef(a, 3, 2, 3, 3); }

inline f32x4 f32x4_wwxx(f32x4 a) { return _shufflef(a, 3, 3, 0, 0); }
inline f32x4 f32x4_wwxy(f32x4 a) { return _shufflef(a, 3, 3, 0, 1); }
inline f32x4 f32x4_wwxz(f32x4 a) { return _shufflef(a, 3, 3, 0, 2); }
inline f32x4 f32x4_wwxw(f32x4 a) { return _shufflef(a, 3, 3, 0, 3); }
inline f32x4 f32x4_wwyx(f32x4 a) { return _shufflef(a, 3, 3, 1, 0); }
inline f32x4 f32x4_wwyy(f32x4 a) { return _shufflef(a, 3, 3, 1, 1); }
inline f32x4 f32x4_wwyz(f32x4 a) { return _shufflef(a, 3, 3, 1, 2); }
inline f32x4 f32x4_wwyw(f32x4 a) { return _shufflef(a, 3, 3, 1, 3); }
inline f32x4 f32x4_wwzx(f32x4 a) { return _shufflef(a, 3, 3, 2, 0); }
inline f32x4 f32x4_wwzy(f32x4 a) { return _shufflef(a, 3, 3, 2, 1); }
inline f32x4 f32x4_wwzz(f32x4 a) { return _shufflef(a, 3, 3, 2, 2); }
inline f32x4 f32x4_wwzw(f32x4 a) { return _shufflef(a, 3, 3, 2, 3); }
inline f32x4 f32x4_wwwx(f32x4 a) { return _shufflef(a, 3, 3, 3, 0); }
inline f32x4 f32x4_wwwy(f32x4 a) { return _shufflef(a, 3, 3, 3, 1); }
inline f32x4 f32x4_wwwz(f32x4 a) { return _shufflef(a, 3, 3, 3, 2); }
inline f32x4 f32x4_wwww(f32x4 a) { return _shufflef1(a, 3); }

inline f32x4 f32x4_trunc2(f32x4 a);
inline f32x4 f32x4_trunc3(f32x4 a);

//2D swizzles

inline f32x4 f32x4_xx(f32x4 a) { return f32x4_trunc2(f32x4_xxxx(a)); }
inline f32x4 f32x4_xy(f32x4 a) { return f32x4_trunc2(a); }
inline f32x4 f32x4_xz(f32x4 a) { return f32x4_trunc2(f32x4_xzxx(a)); }
inline f32x4 f32x4_xw(f32x4 a) { return f32x4_trunc2(f32x4_xwxx(a)); }

inline f32x4 f32x4_yx(f32x4 a) { return f32x4_trunc2(f32x4_yxxx(a)); }
inline f32x4 f32x4_yy(f32x4 a) { return f32x4_trunc2(f32x4_yyxx(a)); }
inline f32x4 f32x4_yz(f32x4 a) { return f32x4_trunc2(f32x4_yzxx(a)); }
inline f32x4 f32x4_yw(f32x4 a) { return f32x4_trunc2(f32x4_ywxx(a)); }

inline f32x4 f32x4_zx(f32x4 a) { return f32x4_trunc2(f32x4_zxxx(a)); }
inline f32x4 f32x4_zy(f32x4 a) { return f32x4_trunc2(f32x4_zyxx(a)); }
inline f32x4 f32x4_zz(f32x4 a) { return f32x4_trunc2(f32x4_zzxx(a)); }
inline f32x4 f32x4_zw(f32x4 a) { return f32x4_trunc2(f32x4_zwxx(a)); }

inline f32x4 f32x4_wx(f32x4 a) { return f32x4_trunc2(f32x4_wxxx(a)); }
inline f32x4 f32x4_wy(f32x4 a) { return f32x4_trunc2(f32x4_wyxx(a)); }
inline f32x4 f32x4_wz(f32x4 a) { return f32x4_trunc2(f32x4_wzxx(a)); }
inline f32x4 f32x4_ww(f32x4 a) { return f32x4_trunc2(f32x4_wwxx(a)); }

//3D swizzles

inline f32x4 f32x4_xxx(f32x4 a) { return f32x4_trunc3(f32x4_xxxx(a)); }
inline f32x4 f32x4_xxy(f32x4 a) { return f32x4_trunc3(f32x4_xxyx(a)); }
inline f32x4 f32x4_xxz(f32x4 a) { return f32x4_trunc3(f32x4_xxzx(a)); }
inline f32x4 f32x4_xyx(f32x4 a) { return f32x4_trunc3(f32x4_xyxx(a)); }
inline f32x4 f32x4_xyy(f32x4 a) { return f32x4_trunc3(f32x4_xyyx(a)); }
inline f32x4 f32x4_xyz(f32x4 a) { return f32x4_trunc3(a); }
inline f32x4 f32x4_xzx(f32x4 a) { return f32x4_trunc3(f32x4_xzxx(a)); }
inline f32x4 f32x4_xzy(f32x4 a) { return f32x4_trunc3(f32x4_xzyx(a)); }
inline f32x4 f32x4_xzz(f32x4 a) { return f32x4_trunc3(f32x4_xzzx(a)); }

inline f32x4 f32x4_yxx(f32x4 a) { return f32x4_trunc3(f32x4_yxxx(a)); }
inline f32x4 f32x4_yxy(f32x4 a) { return f32x4_trunc3(f32x4_yxyx(a)); }
inline f32x4 f32x4_yxz(f32x4 a) { return f32x4_trunc3(f32x4_yxzx(a)); }
inline f32x4 f32x4_yyx(f32x4 a) { return f32x4_trunc3(f32x4_yyxx(a)); }
inline f32x4 f32x4_yyy(f32x4 a) { return f32x4_trunc3(f32x4_yyyx(a)); }
inline f32x4 f32x4_yyz(f32x4 a) { return f32x4_trunc3(f32x4_yyzx(a)); }
inline f32x4 f32x4_yzx(f32x4 a) { return f32x4_trunc3(f32x4_yzxx(a)); }
inline f32x4 f32x4_yzy(f32x4 a) { return f32x4_trunc3(f32x4_yzyx(a)); }
inline f32x4 f32x4_yzz(f32x4 a) { return f32x4_trunc3(f32x4_yzzx(a)); }

inline f32x4 f32x4_zxx(f32x4 a) { return f32x4_trunc3(f32x4_zxxx(a)); }
inline f32x4 f32x4_zxy(f32x4 a) { return f32x4_trunc3(f32x4_zxyx(a)); }
inline f32x4 f32x4_zxz(f32x4 a) { return f32x4_trunc3(f32x4_zxzx(a)); }
inline f32x4 f32x4_zyx(f32x4 a) { return f32x4_trunc3(f32x4_zyxx(a)); }
inline f32x4 f32x4_zyy(f32x4 a) { return f32x4_trunc3(f32x4_zyyx(a)); }
inline f32x4 f32x4_zyz(f32x4 a) { return f32x4_trunc3(f32x4_zyzx(a)); }
inline f32x4 f32x4_zzx(f32x4 a) { return f32x4_trunc3(f32x4_zzxx(a)); }
inline f32x4 f32x4_zzy(f32x4 a) { return f32x4_trunc3(f32x4_zzyx(a)); }
inline f32x4 f32x4_zzz(f32x4 a) { return f32x4_trunc3(f32x4_zzzx(a)); }

inline f32x4 f32x4_fromI32x4(i32x4 a);
inline i32x4 i32x4_fromF32x4(f32x4 a);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline f32x4 f32x4_sign(f32x4 v) {
    return f32x4_add(
        f32x4_mul(f32x4_lt(v, f32x4_zero()), f32x4_two()), 
        f32x4_one()
    ); 
}

inline f32x4 f32x4_cross3(f32x4 a, f32x4 b) {
    return f32x4_normalize3(
        f32x4_sub(
            f32x4_mul(f32x4_yzx(a), f32x4_zxy(b)),
            f32x4_mul(f32x4_zxy(a), f32x4_yzx(b))
        )
    );
}

inline f32x4 f32x4_abs(f32x4 v){ return f32x4_mul(f32x4_sign(v), v); }

//Shifts are hard, so we just shift by division and floor

inline f32x4 f32x4_rgb8Unpack(u32 v) {
    f32x4 rgb8 = f32x4_floor(f32x4_div(f32x4_xxxx4((f32)v), f32x4_init3(0x10000, 0x100, 0x1)));
    return f32x4_div(f32x4_mod(rgb8, f32x4_xxxx4(0x100)), f32x4_xxxx4(0xFF));
}

inline u32 f32x4_rgb8Pack(f32x4 v) {
    f32x4 v8 = f32x4_floor(f32x4_mul(v, f32x4_xxxx4(0xFF)));
    f32x4 preShift = f32x4_trunc3(f32x4_mul(v8, f32x4_init3(0x10000, 0x100, 0x1)));
    return (u32) f32x4_reduce(preShift);
}

inline f32x4 f32x4_srgba8Unpack(u32 v) {
    return f32x4_pow2(f32x4_rgb8Unpack(v << 8 >> 8));
}

inline u32 f32x4_srgba8Pack(f32x4 v) {
    return f32x4_rgb8Pack(f32x4_sqrt(v)) | 0xFF000000;
}

inline f32x4 f32x4_one() { return f32x4_xxxx4(1); }
inline f32x4 f32x4_two() { return f32x4_xxxx4(2); }
inline f32x4 f32x4_negOne() { return f32x4_xxxx4(-1); }
inline f32x4 f32x4_negTwo() { return f32x4_xxxx4(-2); }

inline bool f32x4_all(f32x4 b) { return f32x4_reduce(f32x4_neq(b, f32x4_zero())) == 4; }
inline bool f32x4_any(f32x4 b) { return f32x4_reduce(f32x4_neq(b, f32x4_zero())); }

inline f32x4 f32x4_load1(const f32 *arr) { return f32x4_init1(*arr); }
inline f32x4 f32x4_load2(const f32 *arr) { return f32x4_init2(*arr, arr[1]); }
inline f32x4 f32x4_load3(const f32 *arr) { return f32x4_init3(*arr, arr[1], arr[2]); }

inline void f32x4_setX(f32x4 *a, f32 v) { *a = f32x4_init4(v,           f32x4_y(*a),    f32x4_z(*a),    f32x4_w(*a)); }
inline void f32x4_setY(f32x4 *a, f32 v) { *a = f32x4_init4(f32x4_x(*a),   v,            f32x4_z(*a),    f32x4_w(*a)); }
inline void f32x4_setZ(f32x4 *a, f32 v) { *a = f32x4_init4(f32x4_x(*a),   f32x4_y(*a),    v,            f32x4_w(*a)); }
inline void f32x4_setW(f32x4 *a, f32 v) { *a = f32x4_init4(f32x4_x(*a),   f32x4_y(*a),    f32x4_z(*a),    v); }

inline void f32x4_set(f32x4 *a, u8 i, f32 v) {
    switch (i & 3) {
        case 0:     f32x4_setX(a, v);     break;
        case 1:     f32x4_setY(a, v);     break;
        case 2:     f32x4_setZ(a, v);     break;
        default:    f32x4_setW(a, v);
    }
}

inline f32 f32x4_get(f32x4 a, u8 i) {
    switch (i & 3) {
        case 0:     return f32x4_x(a);
        case 1:     return f32x4_y(a);
        case 2:     return f32x4_z(a);
        default:    return f32x4_w(a);
    }
}

inline f32x4 f32x4_mul3x3(f32x4 v3, f32x4 v3x3[3]) {
    return f32x4_add(
        f32x4_add(
            f32x4_mul(v3x3[0], f32x4_xxx(v3)),
            f32x4_mul(v3x3[1], f32x4_yyy(v3))
        ),
        f32x4_mul(v3x3[2], f32x4_zzz(v3))
    );
}

//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml

inline f32x4 f32x4_reflect2(f32x4 I, f32x4 N) {
    return f32x4_sub(I, f32x4_mul(N, f32x4_xxxx4(2 * f32x4_dot2(N, I))));
}

inline f32x4 f32x4_reflect3(f32x4 I, f32x4 N) {
    return f32x4_sub(I, f32x4_mul(N, f32x4_xxxx4(2 * f32x4_dot3(N, I))));
}
