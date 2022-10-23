#pragma once

//TODO: Error checking

//Arithmetic

inline F32x4 F32x4_zero();
inline F32x4 F32x4_one();
inline F32x4 F32x4_two();
inline F32x4 F32x4_negTwo();
inline F32x4 F32x4_negOne();
inline F32x4 F32x4_xxxx4(F32 x);

inline F32x4 F32x4_add(F32x4 a, F32x4 b);
inline F32x4 F32x4_sub(F32x4 a, F32x4 b);
inline F32x4 F32x4_mul(F32x4 a, F32x4 b);
inline F32x4 F32x4_div(F32x4 a, F32x4 b);

inline F32x4 F32x4_srgba8Unpack(U32 v);
inline U32   F32x4_srgba8Pack(F32x4 v);
inline F32x4 F32x4_rgb8Unpack(U32 v);
inline U32   F32x4_rgb8Pack(F32x4 v);

inline F32x4 F32x4_complement(F32x4 a) { return F32x4_sub(F32x4_one(), a); }
inline F32x4 F32x4_negate(F32x4 a) { return F32x4_sub(F32x4_zero(), a); }
inline F32x4 F32x4_inverse(F32x4 a) { return F32x4_div(F32x4_one(), a); }

inline F32x4 F32x4_pow2(F32x4 a) { return F32x4_mul(a, a); }

inline F32x4 F32x4_sign(F32x4 v);     //Zero counts as signed too
inline F32x4 F32x4_abs(F32x4 v);
inline F32x4 F32x4_ceil(F32x4 v);
inline F32x4 F32x4_floor(F32x4 v);
inline F32x4 F32x4_round(F32x4 v);
inline F32x4 F32x4_pow(F32x4 v, F32x4 e);
inline F32x4 F32x4_fract(F32x4 v) { return F32x4_sub(v, F32x4_floor(v)); }
inline F32x4 F32x4_mod(F32x4 v, F32x4 d) { return F32x4_mul(F32x4_fract(F32x4_div(v, d)), d); }

inline F32 F32x4_reduce(F32x4 a);     //ax+ay+az+aw

inline F32 F32x4_dot2(F32x4 a, F32x4 b);
inline F32 F32x4_dot3(F32x4 a, F32x4 b);
inline F32 F32x4_dot4(F32x4 a, F32x4 b);

inline F32 F32x4_satDot2(F32x4 X, F32x4 Y) { return F32_saturate(F32x4_dot2(X, Y)); }
inline F32 F32x4_satDot3(F32x4 X, F32x4 Y) { return F32_saturate(F32x4_dot3(X, Y)); }
inline F32 F32x4_satDot4(F32x4 X, F32x4 Y) { return F32_saturate(F32x4_dot4(X, Y)); }

inline F32x4 F32x4_reflect2(F32x4 I, F32x4 N);
inline F32x4 F32x4_reflect3(F32x4 I, F32x4 N);

inline F32 F32x4_sqLen2(F32x4 v) { return F32x4_dot2(v, v); }
inline F32 F32x4_sqLen3(F32x4 v) { return F32x4_dot3(v, v); }
inline F32 F32x4_sqLen4(F32x4 v) { return F32x4_dot4(v, v); }

inline F32 F32x4_len2(F32x4 v) { return F32_sqrt(F32x4_sqLen2(v)); }
inline F32 F32x4_len3(F32x4 v) { return F32_sqrt(F32x4_sqLen3(v)); }
inline F32 F32x4_len4(F32x4 v) { return F32_sqrt(F32x4_sqLen4(v)); }

inline F32x4 F32x4_acos(F32x4 v);
inline F32x4 F32x4_cos(F32x4 v);
inline F32x4 F32x4_asin(F32x4 v);
inline F32x4 F32x4_sin(F32x4 v);
inline F32x4 F32x4_atan(F32x4 v);
inline F32x4 F32x4_atan2(F32x4 y, F32x4 x);
inline F32x4 F32x4_tan(F32x4 v);
inline F32x4 F32x4_sqrt(F32x4 a);
inline F32x4 F32x4_rsqrt(F32x4 a);

inline F32x4 F32x4_normalize2(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen2(v)))); }
inline F32x4 F32x4_normalize3(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen3(v)))); }
inline F32x4 F32x4_normalize4(F32x4 v) { return F32x4_mul(v, F32x4_rsqrt(F32x4_xxxx4(F32x4_sqLen4(v)))); }

inline F32x4 F32x4_loge(F32x4 v);
inline F32x4 F32x4_log10(F32x4 v);
inline F32x4 F32x4_log2(F32x4 v);

inline F32x4 F32x4_exp(F32x4 v);
inline F32x4 F32x4_exp10(F32x4 v);
inline F32x4 F32x4_exp2(F32x4 v);

inline F32x4 F32x4_cross3(F32x4 a, F32x4 b);

inline F32x4 F32x4_lerp(F32x4 a, F32x4 b, F32 perc) { 
    b = F32x4_sub(b, a);
    return F32x4_add(a, F32x4_mul(b, F32x4_xxxx4(perc)));
}

inline F32x4 F32x4_min(F32x4 a, F32x4 b);
inline F32x4 F32x4_max(F32x4 a, F32x4 b);
inline F32x4 F32x4_clamp(F32x4 a, F32x4 mi, F32x4 ma) { return F32x4_max(mi, F32x4_min(ma, a)); }
inline F32x4 F32x4_saturate(F32x4 a) { return F32x4_clamp(a, F32x4_zero(), F32x4_one()); }

//Boolean

inline Bool F32x4_all(F32x4 a);
inline Bool F32x4_any(F32x4 b);

inline F32x4 F32x4_eq(F32x4 a, F32x4 b);
inline F32x4 F32x4_neq(F32x4 a, F32x4 b);
inline F32x4 F32x4_geq(F32x4 a, F32x4 b);
inline F32x4 F32x4_gt(F32x4 a, F32x4 b);
inline F32x4 F32x4_leq(F32x4 a, F32x4 b);
inline F32x4 F32x4_lt(F32x4 a, F32x4 b);

inline Bool F32x4_eq4(F32x4 a, F32x4 b) { return F32x4_all(F32x4_eq(a, b)); }
inline Bool F32x4_neq4(F32x4 a, F32x4 b) { return !F32x4_eq4(a, b); }

//Swizzles and shizzle

inline F32 F32x4_x(F32x4 a);
inline F32 F32x4_y(F32x4 a);
inline F32 F32x4_z(F32x4 a);
inline F32 F32x4_w(F32x4 a);
inline F32 F32x4_get(F32x4 a, U8 i);

inline void F32x4_setX(F32x4 *a, F32 v);
inline void F32x4_setY(F32x4 *a, F32 v);
inline void F32x4_setZ(F32x4 *a, F32 v);
inline void F32x4_setW(F32x4 *a, F32 v);
inline void F32x4_set(F32x4 *a, U8 i, F32 v);

//Construction

inline F32x4 F32x4_create1(F32 x);
inline F32x4 F32x4_create2(F32 x, F32 y);
inline F32x4 F32x4_create3(F32 x, F32 y, F32 z);
inline F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w);

inline F32x4 F32x4_load1(const F32 *arr);
inline F32x4 F32x4_load2(const F32 *arr);
inline F32x4 F32x4_load3(const F32 *arr);
inline F32x4 F32x4_load4(const F32 *arr) { return *(const F32x4*) arr; }

//Shuffle and extracting values

#define _shufflef1(a, x) _shufflef(a, x, x, x, x)

//4D swizzles

inline F32x4 F32x4_xxxx(F32x4 a) { return _shufflef1(a, 0); }
inline F32x4 F32x4_xxxy(F32x4 a) { return _shufflef(a, 0, 0, 0, 1); }
inline F32x4 F32x4_xxxz(F32x4 a) { return _shufflef(a, 0, 0, 0, 2); }
inline F32x4 F32x4_xxxw(F32x4 a) { return _shufflef(a, 0, 0, 0, 3); }
inline F32x4 F32x4_xxyx(F32x4 a) { return _shufflef(a, 0, 0, 1, 0); }
inline F32x4 F32x4_xxyy(F32x4 a) { return _shufflef(a, 0, 0, 1, 1); }
inline F32x4 F32x4_xxyz(F32x4 a) { return _shufflef(a, 0, 0, 1, 2); }
inline F32x4 F32x4_xxyw(F32x4 a) { return _shufflef(a, 0, 0, 1, 3); }
inline F32x4 F32x4_xxzx(F32x4 a) { return _shufflef(a, 0, 0, 2, 0); }
inline F32x4 F32x4_xxzy(F32x4 a) { return _shufflef(a, 0, 0, 2, 1); }
inline F32x4 F32x4_xxzz(F32x4 a) { return _shufflef(a, 0, 0, 2, 2); }
inline F32x4 F32x4_xxzw(F32x4 a) { return _shufflef(a, 0, 0, 2, 3); }
inline F32x4 F32x4_xxwx(F32x4 a) { return _shufflef(a, 0, 0, 3, 0); }
inline F32x4 F32x4_xxwy(F32x4 a) { return _shufflef(a, 0, 0, 3, 1); }
inline F32x4 F32x4_xxwz(F32x4 a) { return _shufflef(a, 0, 0, 3, 2); }
inline F32x4 F32x4_xxww(F32x4 a) { return _shufflef(a, 0, 0, 3, 3); }

inline F32x4 F32x4_xyxx(F32x4 a) { return _shufflef(a, 0, 1, 0, 0); }
inline F32x4 F32x4_xyxy(F32x4 a) { return _shufflef(a, 0, 1, 0, 1); }
inline F32x4 F32x4_xyxz(F32x4 a) { return _shufflef(a, 0, 1, 0, 2); }
inline F32x4 F32x4_xyxw(F32x4 a) { return _shufflef(a, 0, 1, 0, 3); }
inline F32x4 F32x4_xyyx(F32x4 a) { return _shufflef(a, 0, 1, 1, 0); }
inline F32x4 F32x4_xyyy(F32x4 a) { return _shufflef(a, 0, 1, 1, 1); }
inline F32x4 F32x4_xyyz(F32x4 a) { return _shufflef(a, 0, 1, 1, 2); }
inline F32x4 F32x4_xyyw(F32x4 a) { return _shufflef(a, 0, 1, 1, 3); }
inline F32x4 F32x4_xyzx(F32x4 a) { return _shufflef(a, 0, 1, 2, 0); }
inline F32x4 F32x4_xyzy(F32x4 a) { return _shufflef(a, 0, 1, 2, 1); }
inline F32x4 F32x4_xyzz(F32x4 a) { return _shufflef(a, 0, 1, 2, 2); }
inline F32x4 F32x4_xyzw(F32x4 a) { return a; }
inline F32x4 F32x4_xywx(F32x4 a) { return _shufflef(a, 0, 1, 3, 0); }
inline F32x4 F32x4_xywy(F32x4 a) { return _shufflef(a, 0, 1, 3, 1); }
inline F32x4 F32x4_xywz(F32x4 a) { return _shufflef(a, 0, 1, 3, 2); }
inline F32x4 F32x4_xyww(F32x4 a) { return _shufflef(a, 0, 1, 3, 3); }

inline F32x4 F32x4_xzxx(F32x4 a) { return _shufflef(a, 0, 2, 0, 0); }
inline F32x4 F32x4_xzxy(F32x4 a) { return _shufflef(a, 0, 2, 0, 1); }
inline F32x4 F32x4_xzxz(F32x4 a) { return _shufflef(a, 0, 2, 0, 2); }
inline F32x4 F32x4_xzxw(F32x4 a) { return _shufflef(a, 0, 2, 0, 3); }
inline F32x4 F32x4_xzyx(F32x4 a) { return _shufflef(a, 0, 2, 1, 0); }
inline F32x4 F32x4_xzyy(F32x4 a) { return _shufflef(a, 0, 2, 1, 1); }
inline F32x4 F32x4_xzyz(F32x4 a) { return _shufflef(a, 0, 2, 1, 2); }
inline F32x4 F32x4_xzyw(F32x4 a) { return _shufflef(a, 0, 2, 1, 3); }
inline F32x4 F32x4_xzzx(F32x4 a) { return _shufflef(a, 0, 2, 2, 0); }
inline F32x4 F32x4_xzzy(F32x4 a) { return _shufflef(a, 0, 2, 2, 1); }
inline F32x4 F32x4_xzzz(F32x4 a) { return _shufflef(a, 0, 2, 2, 2); }
inline F32x4 F32x4_xzzw(F32x4 a) { return _shufflef(a, 0, 2, 2, 3); }
inline F32x4 F32x4_xzwx(F32x4 a) { return _shufflef(a, 0, 2, 3, 0); }
inline F32x4 F32x4_xzwy(F32x4 a) { return _shufflef(a, 0, 2, 3, 1); }
inline F32x4 F32x4_xzwz(F32x4 a) { return _shufflef(a, 0, 2, 3, 2); }
inline F32x4 F32x4_xzww(F32x4 a) { return _shufflef(a, 0, 2, 3, 3); }

inline F32x4 F32x4_xwxx(F32x4 a) { return _shufflef(a, 0, 3, 0, 0); }
inline F32x4 F32x4_xwxy(F32x4 a) { return _shufflef(a, 0, 3, 0, 1); }
inline F32x4 F32x4_xwxz(F32x4 a) { return _shufflef(a, 0, 3, 0, 2); }
inline F32x4 F32x4_xwxw(F32x4 a) { return _shufflef(a, 0, 3, 0, 3); }
inline F32x4 F32x4_xwyx(F32x4 a) { return _shufflef(a, 0, 3, 1, 0); }
inline F32x4 F32x4_xwyy(F32x4 a) { return _shufflef(a, 0, 3, 1, 1); }
inline F32x4 F32x4_xwyz(F32x4 a) { return _shufflef(a, 0, 3, 1, 2); }
inline F32x4 F32x4_xwyw(F32x4 a) { return _shufflef(a, 0, 3, 1, 3); }
inline F32x4 F32x4_xwzx(F32x4 a) { return _shufflef(a, 0, 3, 2, 0); }
inline F32x4 F32x4_xwzy(F32x4 a) { return _shufflef(a, 0, 3, 2, 1); }
inline F32x4 F32x4_xwzz(F32x4 a) { return _shufflef(a, 0, 3, 2, 2); }
inline F32x4 F32x4_xwzw(F32x4 a) { return _shufflef(a, 0, 3, 2, 3); }
inline F32x4 F32x4_xwwx(F32x4 a) { return _shufflef(a, 0, 3, 3, 0); }
inline F32x4 F32x4_xwwy(F32x4 a) { return _shufflef(a, 0, 3, 3, 1); }
inline F32x4 F32x4_xwwz(F32x4 a) { return _shufflef(a, 0, 3, 3, 2); }
inline F32x4 F32x4_xwww(F32x4 a) { return _shufflef(a, 0, 3, 3, 3); }

inline F32x4 F32x4_yxxx(F32x4 a) { return _shufflef(a, 1, 0, 0, 0); }
inline F32x4 F32x4_yxxy(F32x4 a) { return _shufflef(a, 1, 0, 0, 1); }
inline F32x4 F32x4_yxxz(F32x4 a) { return _shufflef(a, 1, 0, 0, 2); }
inline F32x4 F32x4_yxxw(F32x4 a) { return _shufflef(a, 1, 0, 0, 3); }
inline F32x4 F32x4_yxyx(F32x4 a) { return _shufflef(a, 1, 0, 1, 0); }
inline F32x4 F32x4_yxyy(F32x4 a) { return _shufflef(a, 1, 0, 1, 1); }
inline F32x4 F32x4_yxyz(F32x4 a) { return _shufflef(a, 1, 0, 1, 2); }
inline F32x4 F32x4_yxyw(F32x4 a) { return _shufflef(a, 1, 0, 1, 3); }
inline F32x4 F32x4_yxzx(F32x4 a) { return _shufflef(a, 1, 0, 2, 0); }
inline F32x4 F32x4_yxzy(F32x4 a) { return _shufflef(a, 1, 0, 2, 1); }
inline F32x4 F32x4_yxzz(F32x4 a) { return _shufflef(a, 1, 0, 2, 2); }
inline F32x4 F32x4_yxzw(F32x4 a) { return _shufflef(a, 1, 0, 2, 3); }
inline F32x4 F32x4_yxwx(F32x4 a) { return _shufflef(a, 1, 0, 3, 0); }
inline F32x4 F32x4_yxwy(F32x4 a) { return _shufflef(a, 1, 0, 3, 1); }
inline F32x4 F32x4_yxwz(F32x4 a) { return _shufflef(a, 1, 0, 3, 2); }
inline F32x4 F32x4_yxww(F32x4 a) { return _shufflef(a, 1, 0, 3, 3); }

inline F32x4 F32x4_yyxx(F32x4 a) { return _shufflef(a, 1, 1, 0, 0); }
inline F32x4 F32x4_yyxy(F32x4 a) { return _shufflef(a, 1, 1, 0, 1); }
inline F32x4 F32x4_yyxz(F32x4 a) { return _shufflef(a, 1, 1, 0, 2); }
inline F32x4 F32x4_yyxw(F32x4 a) { return _shufflef(a, 1, 1, 0, 3); }
inline F32x4 F32x4_yyyx(F32x4 a) { return _shufflef(a, 1, 1, 1, 0); }
inline F32x4 F32x4_yyyy(F32x4 a) { return _shufflef1(a, 1); } 
inline F32x4 F32x4_yyyz(F32x4 a) { return _shufflef(a, 1, 1, 1, 2); }
inline F32x4 F32x4_yyyw(F32x4 a) { return _shufflef(a, 1, 1, 1, 3); }
inline F32x4 F32x4_yyzx(F32x4 a) { return _shufflef(a, 1, 1, 2, 0); }
inline F32x4 F32x4_yyzy(F32x4 a) { return _shufflef(a, 1, 1, 2, 1); }
inline F32x4 F32x4_yyzz(F32x4 a) { return _shufflef(a, 1, 1, 2, 2); }
inline F32x4 F32x4_yyzw(F32x4 a) { return _shufflef(a, 1, 1, 2, 3); }
inline F32x4 F32x4_yywx(F32x4 a) { return _shufflef(a, 1, 1, 3, 0); }
inline F32x4 F32x4_yywy(F32x4 a) { return _shufflef(a, 1, 1, 3, 1); }
inline F32x4 F32x4_yywz(F32x4 a) { return _shufflef(a, 1, 1, 3, 2); }
inline F32x4 F32x4_yyww(F32x4 a) { return _shufflef(a, 1, 1, 3, 3); }

inline F32x4 F32x4_yzxx(F32x4 a) { return _shufflef(a, 1, 2, 0, 0); }
inline F32x4 F32x4_yzxy(F32x4 a) { return _shufflef(a, 1, 2, 0, 1); }
inline F32x4 F32x4_yzxz(F32x4 a) { return _shufflef(a, 1, 2, 0, 2); }
inline F32x4 F32x4_yzxw(F32x4 a) { return _shufflef(a, 1, 2, 0, 3); }
inline F32x4 F32x4_yzyx(F32x4 a) { return _shufflef(a, 1, 2, 1, 0); }
inline F32x4 F32x4_yzyy(F32x4 a) { return _shufflef(a, 1, 2, 1, 1); }
inline F32x4 F32x4_yzyz(F32x4 a) { return _shufflef(a, 1, 2, 1, 2); }
inline F32x4 F32x4_yzyw(F32x4 a) { return _shufflef(a, 1, 2, 1, 3); }
inline F32x4 F32x4_yzzx(F32x4 a) { return _shufflef(a, 1, 2, 2, 0); }
inline F32x4 F32x4_yzzy(F32x4 a) { return _shufflef(a, 1, 2, 2, 1); }
inline F32x4 F32x4_yzzz(F32x4 a) { return _shufflef(a, 1, 2, 2, 2); }
inline F32x4 F32x4_yzzw(F32x4 a) { return _shufflef(a, 1, 2, 2, 3); }
inline F32x4 F32x4_yzwx(F32x4 a) { return _shufflef(a, 1, 2, 3, 0); }
inline F32x4 F32x4_yzwy(F32x4 a) { return _shufflef(a, 1, 2, 3, 1); }
inline F32x4 F32x4_yzwz(F32x4 a) { return _shufflef(a, 1, 2, 3, 2); }
inline F32x4 F32x4_yzww(F32x4 a) { return _shufflef(a, 1, 2, 3, 3); }

inline F32x4 F32x4_ywxx(F32x4 a) { return _shufflef(a, 1, 3, 0, 0); }
inline F32x4 F32x4_ywxy(F32x4 a) { return _shufflef(a, 1, 3, 0, 1); }
inline F32x4 F32x4_ywxz(F32x4 a) { return _shufflef(a, 1, 3, 0, 2); }
inline F32x4 F32x4_ywxw(F32x4 a) { return _shufflef(a, 1, 3, 0, 3); }
inline F32x4 F32x4_ywyx(F32x4 a) { return _shufflef(a, 1, 3, 1, 0); }
inline F32x4 F32x4_ywyy(F32x4 a) { return _shufflef(a, 1, 3, 1, 1); }
inline F32x4 F32x4_ywyz(F32x4 a) { return _shufflef(a, 1, 3, 1, 2); }
inline F32x4 F32x4_ywyw(F32x4 a) { return _shufflef(a, 1, 3, 1, 3); }
inline F32x4 F32x4_ywzx(F32x4 a) { return _shufflef(a, 1, 3, 2, 0); }
inline F32x4 F32x4_ywzy(F32x4 a) { return _shufflef(a, 1, 3, 2, 1); }
inline F32x4 F32x4_ywzz(F32x4 a) { return _shufflef(a, 1, 3, 2, 2); }
inline F32x4 F32x4_ywzw(F32x4 a) { return _shufflef(a, 1, 3, 2, 3); }
inline F32x4 F32x4_ywwx(F32x4 a) { return _shufflef(a, 1, 3, 3, 0); }
inline F32x4 F32x4_ywwy(F32x4 a) { return _shufflef(a, 1, 3, 3, 1); }
inline F32x4 F32x4_ywwz(F32x4 a) { return _shufflef(a, 1, 3, 3, 2); }
inline F32x4 F32x4_ywww(F32x4 a) { return _shufflef(a, 1, 3, 3, 3); }

inline F32x4 F32x4_zxxx(F32x4 a) { return _shufflef(a, 2, 0, 0, 0); }
inline F32x4 F32x4_zxxy(F32x4 a) { return _shufflef(a, 2, 0, 0, 1); }
inline F32x4 F32x4_zxxz(F32x4 a) { return _shufflef(a, 2, 0, 0, 2); }
inline F32x4 F32x4_zxxw(F32x4 a) { return _shufflef(a, 2, 0, 0, 3); }
inline F32x4 F32x4_zxyx(F32x4 a) { return _shufflef(a, 2, 0, 1, 0); }
inline F32x4 F32x4_zxyy(F32x4 a) { return _shufflef(a, 2, 0, 1, 1); }
inline F32x4 F32x4_zxyz(F32x4 a) { return _shufflef(a, 2, 0, 1, 2); }
inline F32x4 F32x4_zxyw(F32x4 a) { return _shufflef(a, 2, 0, 1, 3); }
inline F32x4 F32x4_zxzx(F32x4 a) { return _shufflef(a, 2, 0, 2, 0); }
inline F32x4 F32x4_zxzy(F32x4 a) { return _shufflef(a, 2, 0, 2, 1); }
inline F32x4 F32x4_zxzz(F32x4 a) { return _shufflef(a, 2, 0, 2, 2); }
inline F32x4 F32x4_zxzw(F32x4 a) { return _shufflef(a, 2, 0, 2, 3); }
inline F32x4 F32x4_zxwx(F32x4 a) { return _shufflef(a, 2, 0, 3, 0); }
inline F32x4 F32x4_zxwy(F32x4 a) { return _shufflef(a, 2, 0, 3, 1); }
inline F32x4 F32x4_zxwz(F32x4 a) { return _shufflef(a, 2, 0, 3, 2); }
inline F32x4 F32x4_zxww(F32x4 a) { return _shufflef(a, 2, 0, 3, 3); }

inline F32x4 F32x4_zyxx(F32x4 a) { return _shufflef(a, 2, 1, 0, 0); }
inline F32x4 F32x4_zyxy(F32x4 a) { return _shufflef(a, 2, 1, 0, 1); }
inline F32x4 F32x4_zyxz(F32x4 a) { return _shufflef(a, 2, 1, 0, 2); }
inline F32x4 F32x4_zyxw(F32x4 a) { return _shufflef(a, 2, 1, 0, 3); }
inline F32x4 F32x4_zyyx(F32x4 a) { return _shufflef(a, 2, 1, 1, 0); }
inline F32x4 F32x4_zyyy(F32x4 a) { return _shufflef(a, 2, 1, 1, 1); }
inline F32x4 F32x4_zyyz(F32x4 a) { return _shufflef(a, 2, 1, 1, 2); }
inline F32x4 F32x4_zyyw(F32x4 a) { return _shufflef(a, 2, 1, 1, 3); }
inline F32x4 F32x4_zyzx(F32x4 a) { return _shufflef(a, 2, 1, 2, 0); }
inline F32x4 F32x4_zyzy(F32x4 a) { return _shufflef(a, 2, 1, 2, 1); }
inline F32x4 F32x4_zyzz(F32x4 a) { return _shufflef(a, 2, 1, 2, 2); }
inline F32x4 F32x4_zyzw(F32x4 a) { return _shufflef(a, 2, 1, 2, 3); }
inline F32x4 F32x4_zywx(F32x4 a) { return _shufflef(a, 2, 1, 3, 0); }
inline F32x4 F32x4_zywy(F32x4 a) { return _shufflef(a, 2, 1, 3, 1); }
inline F32x4 F32x4_zywz(F32x4 a) { return _shufflef(a, 2, 1, 3, 2); }
inline F32x4 F32x4_zyww(F32x4 a) { return _shufflef(a, 2, 1, 3, 3); }

inline F32x4 F32x4_zzxx(F32x4 a) { return _shufflef(a, 2, 2, 0, 0); }
inline F32x4 F32x4_zzxy(F32x4 a) { return _shufflef(a, 2, 2, 0, 1); }
inline F32x4 F32x4_zzxz(F32x4 a) { return _shufflef(a, 2, 2, 0, 2); }
inline F32x4 F32x4_zzxw(F32x4 a) { return _shufflef(a, 2, 2, 0, 3); }
inline F32x4 F32x4_zzyx(F32x4 a) { return _shufflef(a, 2, 2, 1, 0); }
inline F32x4 F32x4_zzyy(F32x4 a) { return _shufflef(a, 2, 2, 1, 1); }
inline F32x4 F32x4_zzyz(F32x4 a) { return _shufflef(a, 2, 2, 1, 2); }
inline F32x4 F32x4_zzyw(F32x4 a) { return _shufflef(a, 2, 2, 1, 3); }
inline F32x4 F32x4_zzzx(F32x4 a) { return _shufflef(a, 2, 2, 2, 0); }
inline F32x4 F32x4_zzzy(F32x4 a) { return _shufflef(a, 2, 2, 2, 1); }
inline F32x4 F32x4_zzzz(F32x4 a) { return _shufflef1(a, 2); }
inline F32x4 F32x4_zzzw(F32x4 a) { return _shufflef(a, 2, 2, 2, 3); }
inline F32x4 F32x4_zzwx(F32x4 a) { return _shufflef(a, 2, 2, 3, 0); }
inline F32x4 F32x4_zzwy(F32x4 a) { return _shufflef(a, 2, 2, 3, 1); }
inline F32x4 F32x4_zzwz(F32x4 a) { return _shufflef(a, 2, 2, 3, 2); }
inline F32x4 F32x4_zzww(F32x4 a) { return _shufflef(a, 2, 2, 3, 3); }

inline F32x4 F32x4_zwxx(F32x4 a) { return _shufflef(a, 2, 3, 0, 0); }
inline F32x4 F32x4_zwxy(F32x4 a) { return _shufflef(a, 2, 3, 0, 1); }
inline F32x4 F32x4_zwxz(F32x4 a) { return _shufflef(a, 2, 3, 0, 2); }
inline F32x4 F32x4_zwxw(F32x4 a) { return _shufflef(a, 2, 3, 0, 3); }
inline F32x4 F32x4_zwyx(F32x4 a) { return _shufflef(a, 2, 3, 1, 0); }
inline F32x4 F32x4_zwyy(F32x4 a) { return _shufflef(a, 2, 3, 1, 1); }
inline F32x4 F32x4_zwyz(F32x4 a) { return _shufflef(a, 2, 3, 1, 2); }
inline F32x4 F32x4_zwyw(F32x4 a) { return _shufflef(a, 2, 3, 1, 3); }
inline F32x4 F32x4_zwzx(F32x4 a) { return _shufflef(a, 2, 3, 2, 0); }
inline F32x4 F32x4_zwzy(F32x4 a) { return _shufflef(a, 2, 3, 2, 1); }
inline F32x4 F32x4_zwzz(F32x4 a) { return _shufflef(a, 2, 3, 2, 2); }
inline F32x4 F32x4_zwzw(F32x4 a) { return _shufflef(a, 2, 3, 2, 3); }
inline F32x4 F32x4_zwwx(F32x4 a) { return _shufflef(a, 2, 3, 3, 0); }
inline F32x4 F32x4_zwwy(F32x4 a) { return _shufflef(a, 2, 3, 3, 1); }
inline F32x4 F32x4_zwwz(F32x4 a) { return _shufflef(a, 2, 3, 3, 2); }
inline F32x4 F32x4_zwww(F32x4 a) { return _shufflef(a, 2, 3, 3, 3); }

inline F32x4 F32x4_wxxx(F32x4 a) { return _shufflef(a, 3, 0, 0, 0); }
inline F32x4 F32x4_wxxy(F32x4 a) { return _shufflef(a, 3, 0, 0, 1); }
inline F32x4 F32x4_wxxz(F32x4 a) { return _shufflef(a, 3, 0, 0, 2); }
inline F32x4 F32x4_wxxw(F32x4 a) { return _shufflef(a, 3, 0, 0, 3); }
inline F32x4 F32x4_wxyx(F32x4 a) { return _shufflef(a, 3, 0, 1, 0); }
inline F32x4 F32x4_wxyy(F32x4 a) { return _shufflef(a, 3, 0, 1, 1); }
inline F32x4 F32x4_wxyz(F32x4 a) { return _shufflef(a, 3, 0, 1, 2); }
inline F32x4 F32x4_wxyw(F32x4 a) { return _shufflef(a, 3, 0, 1, 3); }
inline F32x4 F32x4_wxzx(F32x4 a) { return _shufflef(a, 3, 0, 2, 0); }
inline F32x4 F32x4_wxzy(F32x4 a) { return _shufflef(a, 3, 0, 2, 1); }
inline F32x4 F32x4_wxzz(F32x4 a) { return _shufflef(a, 3, 0, 2, 2); }
inline F32x4 F32x4_wxzw(F32x4 a) { return _shufflef(a, 3, 0, 2, 3); }
inline F32x4 F32x4_wxwx(F32x4 a) { return _shufflef(a, 3, 0, 3, 0); }
inline F32x4 F32x4_wxwy(F32x4 a) { return _shufflef(a, 3, 0, 3, 1); }
inline F32x4 F32x4_wxwz(F32x4 a) { return _shufflef(a, 3, 0, 3, 2); }
inline F32x4 F32x4_wxww(F32x4 a) { return _shufflef(a, 3, 0, 3, 3); }

inline F32x4 F32x4_wyxx(F32x4 a) { return _shufflef(a, 3, 1, 0, 0); }
inline F32x4 F32x4_wyxy(F32x4 a) { return _shufflef(a, 3, 1, 0, 1); }
inline F32x4 F32x4_wyxz(F32x4 a) { return _shufflef(a, 3, 1, 0, 2); }
inline F32x4 F32x4_wyxw(F32x4 a) { return _shufflef(a, 3, 1, 0, 3); }
inline F32x4 F32x4_wyyx(F32x4 a) { return _shufflef(a, 3, 1, 1, 0); }
inline F32x4 F32x4_wyyy(F32x4 a) { return _shufflef(a, 3, 1, 1, 1); }
inline F32x4 F32x4_wyyz(F32x4 a) { return _shufflef(a, 3, 1, 1, 2); }
inline F32x4 F32x4_wyyw(F32x4 a) { return _shufflef(a, 3, 1, 1, 3); }
inline F32x4 F32x4_wyzx(F32x4 a) { return _shufflef(a, 3, 1, 2, 0); }
inline F32x4 F32x4_wyzy(F32x4 a) { return _shufflef(a, 3, 1, 2, 1); }
inline F32x4 F32x4_wyzz(F32x4 a) { return _shufflef(a, 3, 1, 2, 2); }
inline F32x4 F32x4_wyzw(F32x4 a) { return _shufflef(a, 3, 1, 2, 3); }
inline F32x4 F32x4_wywx(F32x4 a) { return _shufflef(a, 3, 1, 3, 0); }
inline F32x4 F32x4_wywy(F32x4 a) { return _shufflef(a, 3, 1, 3, 1); }
inline F32x4 F32x4_wywz(F32x4 a) { return _shufflef(a, 3, 1, 3, 2); }
inline F32x4 F32x4_wyww(F32x4 a) { return _shufflef(a, 3, 1, 3, 3); }

inline F32x4 F32x4_wzxx(F32x4 a) { return _shufflef(a, 3, 2, 0, 0); }
inline F32x4 F32x4_wzxy(F32x4 a) { return _shufflef(a, 3, 2, 0, 1); }
inline F32x4 F32x4_wzxz(F32x4 a) { return _shufflef(a, 3, 2, 0, 2); }
inline F32x4 F32x4_wzxw(F32x4 a) { return _shufflef(a, 3, 2, 0, 3); }
inline F32x4 F32x4_wzyx(F32x4 a) { return _shufflef(a, 3, 2, 1, 0); }
inline F32x4 F32x4_wzyy(F32x4 a) { return _shufflef(a, 3, 2, 1, 1); }
inline F32x4 F32x4_wzyz(F32x4 a) { return _shufflef(a, 3, 2, 1, 2); }
inline F32x4 F32x4_wzyw(F32x4 a) { return _shufflef(a, 3, 2, 1, 3); }
inline F32x4 F32x4_wzzx(F32x4 a) { return _shufflef(a, 3, 2, 2, 0); }
inline F32x4 F32x4_wzzy(F32x4 a) { return _shufflef(a, 3, 2, 2, 1); }
inline F32x4 F32x4_wzzz(F32x4 a) { return _shufflef(a, 3, 2, 2, 2); }
inline F32x4 F32x4_wzzw(F32x4 a) { return _shufflef(a, 3, 2, 2, 3); }
inline F32x4 F32x4_wzwx(F32x4 a) { return _shufflef(a, 3, 2, 3, 0); }
inline F32x4 F32x4_wzwy(F32x4 a) { return _shufflef(a, 3, 2, 3, 1); }
inline F32x4 F32x4_wzwz(F32x4 a) { return _shufflef(a, 3, 2, 3, 2); }
inline F32x4 F32x4_wzww(F32x4 a) { return _shufflef(a, 3, 2, 3, 3); }

inline F32x4 F32x4_wwxx(F32x4 a) { return _shufflef(a, 3, 3, 0, 0); }
inline F32x4 F32x4_wwxy(F32x4 a) { return _shufflef(a, 3, 3, 0, 1); }
inline F32x4 F32x4_wwxz(F32x4 a) { return _shufflef(a, 3, 3, 0, 2); }
inline F32x4 F32x4_wwxw(F32x4 a) { return _shufflef(a, 3, 3, 0, 3); }
inline F32x4 F32x4_wwyx(F32x4 a) { return _shufflef(a, 3, 3, 1, 0); }
inline F32x4 F32x4_wwyy(F32x4 a) { return _shufflef(a, 3, 3, 1, 1); }
inline F32x4 F32x4_wwyz(F32x4 a) { return _shufflef(a, 3, 3, 1, 2); }
inline F32x4 F32x4_wwyw(F32x4 a) { return _shufflef(a, 3, 3, 1, 3); }
inline F32x4 F32x4_wwzx(F32x4 a) { return _shufflef(a, 3, 3, 2, 0); }
inline F32x4 F32x4_wwzy(F32x4 a) { return _shufflef(a, 3, 3, 2, 1); }
inline F32x4 F32x4_wwzz(F32x4 a) { return _shufflef(a, 3, 3, 2, 2); }
inline F32x4 F32x4_wwzw(F32x4 a) { return _shufflef(a, 3, 3, 2, 3); }
inline F32x4 F32x4_wwwx(F32x4 a) { return _shufflef(a, 3, 3, 3, 0); }
inline F32x4 F32x4_wwwy(F32x4 a) { return _shufflef(a, 3, 3, 3, 1); }
inline F32x4 F32x4_wwwz(F32x4 a) { return _shufflef(a, 3, 3, 3, 2); }
inline F32x4 F32x4_wwww(F32x4 a) { return _shufflef1(a, 3); }

inline F32x4 F32x4_trunc2(F32x4 a);
inline F32x4 F32x4_trunc3(F32x4 a);

//2D swizzles

inline F32x4 F32x4_xx4(F32x4 a) { return F32x4_trunc2(F32x4_xxxx(a)); }
inline F32x4 F32x4_xy4(F32x4 a) { return F32x4_trunc2(a); }
inline F32x4 F32x4_xz4(F32x4 a) { return F32x4_trunc2(F32x4_xzxx(a)); }
inline F32x4 F32x4_xw4(F32x4 a) { return F32x4_trunc2(F32x4_xwxx(a)); }

inline F32x4 F32x4_yx4(F32x4 a) { return F32x4_trunc2(F32x4_yxxx(a)); }
inline F32x4 F32x4_yy4(F32x4 a) { return F32x4_trunc2(F32x4_yyxx(a)); }
inline F32x4 F32x4_yz4(F32x4 a) { return F32x4_trunc2(F32x4_yzxx(a)); }
inline F32x4 F32x4_yw4(F32x4 a) { return F32x4_trunc2(F32x4_ywxx(a)); }

inline F32x4 F32x4_zx4(F32x4 a) { return F32x4_trunc2(F32x4_zxxx(a)); }
inline F32x4 F32x4_zy4(F32x4 a) { return F32x4_trunc2(F32x4_zyxx(a)); }
inline F32x4 F32x4_zz4(F32x4 a) { return F32x4_trunc2(F32x4_zzxx(a)); }
inline F32x4 F32x4_zw4(F32x4 a) { return F32x4_trunc2(F32x4_zwxx(a)); }

inline F32x4 F32x4_wx4(F32x4 a) { return F32x4_trunc2(F32x4_wxxx(a)); }
inline F32x4 F32x4_wy4(F32x4 a) { return F32x4_trunc2(F32x4_wyxx(a)); }
inline F32x4 F32x4_wz4(F32x4 a) { return F32x4_trunc2(F32x4_wzxx(a)); }
inline F32x4 F32x4_ww4(F32x4 a) { return F32x4_trunc2(F32x4_wwxx(a)); }

//3D swizzles

inline F32x4 F32x4_xxx(F32x4 a) { return F32x4_trunc3(F32x4_xxxx(a)); }
inline F32x4 F32x4_xxy(F32x4 a) { return F32x4_trunc3(F32x4_xxyx(a)); }
inline F32x4 F32x4_xxz(F32x4 a) { return F32x4_trunc3(F32x4_xxzx(a)); }
inline F32x4 F32x4_xyx(F32x4 a) { return F32x4_trunc3(F32x4_xyxx(a)); }
inline F32x4 F32x4_xyy(F32x4 a) { return F32x4_trunc3(F32x4_xyyx(a)); }
inline F32x4 F32x4_xyz(F32x4 a) { return F32x4_trunc3(a); }
inline F32x4 F32x4_xzx(F32x4 a) { return F32x4_trunc3(F32x4_xzxx(a)); }
inline F32x4 F32x4_xzy(F32x4 a) { return F32x4_trunc3(F32x4_xzyx(a)); }
inline F32x4 F32x4_xzz(F32x4 a) { return F32x4_trunc3(F32x4_xzzx(a)); }

inline F32x4 F32x4_yxx(F32x4 a) { return F32x4_trunc3(F32x4_yxxx(a)); }
inline F32x4 F32x4_yxy(F32x4 a) { return F32x4_trunc3(F32x4_yxyx(a)); }
inline F32x4 F32x4_yxz(F32x4 a) { return F32x4_trunc3(F32x4_yxzx(a)); }
inline F32x4 F32x4_yyx(F32x4 a) { return F32x4_trunc3(F32x4_yyxx(a)); }
inline F32x4 F32x4_yyy(F32x4 a) { return F32x4_trunc3(F32x4_yyyx(a)); }
inline F32x4 F32x4_yyz(F32x4 a) { return F32x4_trunc3(F32x4_yyzx(a)); }
inline F32x4 F32x4_yzx(F32x4 a) { return F32x4_trunc3(F32x4_yzxx(a)); }
inline F32x4 F32x4_yzy(F32x4 a) { return F32x4_trunc3(F32x4_yzyx(a)); }
inline F32x4 F32x4_yzz(F32x4 a) { return F32x4_trunc3(F32x4_yzzx(a)); }

inline F32x4 F32x4_zxx(F32x4 a) { return F32x4_trunc3(F32x4_zxxx(a)); }
inline F32x4 F32x4_zxy(F32x4 a) { return F32x4_trunc3(F32x4_zxyx(a)); }
inline F32x4 F32x4_zxz(F32x4 a) { return F32x4_trunc3(F32x4_zxzx(a)); }
inline F32x4 F32x4_zyx(F32x4 a) { return F32x4_trunc3(F32x4_zyxx(a)); }
inline F32x4 F32x4_zyy(F32x4 a) { return F32x4_trunc3(F32x4_zyyx(a)); }
inline F32x4 F32x4_zyz(F32x4 a) { return F32x4_trunc3(F32x4_zyzx(a)); }
inline F32x4 F32x4_zzx(F32x4 a) { return F32x4_trunc3(F32x4_zzxx(a)); }
inline F32x4 F32x4_zzy(F32x4 a) { return F32x4_trunc3(F32x4_zzyx(a)); }
inline F32x4 F32x4_zzz(F32x4 a) { return F32x4_trunc3(F32x4_zzzx(a)); }

inline F32x4 F32x4_fromI32x4(I32x4 a);
inline I32x4 I32x4_fromF32x4(F32x4 a);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline F32x4 F32x4_sign(F32x4 v) {
    return F32x4_add(
        F32x4_mul(F32x4_lt(v, F32x4_zero()), F32x4_two()), 
        F32x4_one()
    ); 
}

inline F32x4 F32x4_cross3(F32x4 a, F32x4 b) {
    return F32x4_normalize3(
        F32x4_sub(
            F32x4_mul(F32x4_yzx(a), F32x4_zxy(b)),
            F32x4_mul(F32x4_zxy(a), F32x4_yzx(b))
        )
    );
}

inline F32x4 F32x4_abs(F32x4 v) { return F32x4_mul(F32x4_sign(v), v); }

//Shifts are hard, so we just shift by division and floor

inline F32x4 F32x4_rgb8Unpack(U32 v) {
    F32x4 rgb8 = F32x4_floor(F32x4_div(F32x4_xxxx4((F32)v), F32x4_create3(0x10000, 0x100, 0x1)));
    return F32x4_div(F32x4_mod(rgb8, F32x4_xxxx4(0x100)), F32x4_xxxx4(0xFF));
}

inline U32 F32x4_rgb8Pack(F32x4 v) {
    F32x4 v8 = F32x4_floor(F32x4_mul(v, F32x4_xxxx4(0xFF)));
    F32x4 preShift = F32x4_trunc3(F32x4_mul(v8, F32x4_create3(0x10000, 0x100, 0x1)));
    return (U32) F32x4_reduce(preShift);
}

inline F32x4 F32x4_srgba8Unpack(U32 v) {
    return F32x4_pow2(F32x4_rgb8Unpack(v << 8 >> 8));
}

inline U32 F32x4_srgba8Pack(F32x4 v) {
    return F32x4_rgb8Pack(F32x4_sqrt(v)) | 0xFF000000;
}

inline F32x4 F32x4_one() { return F32x4_xxxx4(1); }
inline F32x4 F32x4_two() { return F32x4_xxxx4(2); }
inline F32x4 F32x4_negOne() { return F32x4_xxxx4(-1); }
inline F32x4 F32x4_negTwo() { return F32x4_xxxx4(-2); }

inline Bool F32x4_all(F32x4 b) { return F32x4_reduce(F32x4_neq(b, F32x4_zero())) == 4; }
inline Bool F32x4_any(F32x4 b) { return F32x4_reduce(F32x4_neq(b, F32x4_zero())); }

inline F32x4 F32x4_load1(const F32 *arr) { return F32x4_create1(*arr); }
inline F32x4 F32x4_load2(const F32 *arr) { return F32x4_create2(*arr, arr[1]); }
inline F32x4 F32x4_load3(const F32 *arr) { return F32x4_create3(*arr, arr[1], arr[2]); }

inline void F32x4_setX(F32x4 *a, F32 v) { *a = F32x4_create4(v,           F32x4_y(*a),    F32x4_z(*a),    F32x4_w(*a)); }
inline void F32x4_setY(F32x4 *a, F32 v) { *a = F32x4_create4(F32x4_x(*a),   v,            F32x4_z(*a),    F32x4_w(*a)); }
inline void F32x4_setZ(F32x4 *a, F32 v) { *a = F32x4_create4(F32x4_x(*a),   F32x4_y(*a),    v,            F32x4_w(*a)); }
inline void F32x4_setW(F32x4 *a, F32 v) { *a = F32x4_create4(F32x4_x(*a),   F32x4_y(*a),    F32x4_z(*a),    v); }

inline void F32x4_set(F32x4 *a, U8 i, F32 v) {
    switch (i & 3) {
        case 0:     F32x4_setX(a, v);     break;
        case 1:     F32x4_setY(a, v);     break;
        case 2:     F32x4_setZ(a, v);     break;
        default:    F32x4_setW(a, v);
    }
}

inline F32 F32x4_get(F32x4 a, U8 i) {
    switch (i & 3) {
        case 0:     return F32x4_x(a);
        case 1:     return F32x4_y(a);
        case 2:     return F32x4_z(a);
        default:    return F32x4_w(a);
    }
}

inline F32x4 F32x4_mul3x3(F32x4 v3, F32x4 v3x3[3]) {
    return F32x4_add(
        F32x4_add(
            F32x4_mul(v3x3[0], F32x4_xxx(v3)),
            F32x4_mul(v3x3[1], F32x4_yyy(v3))
        ),
        F32x4_mul(v3x3[2], F32x4_zzz(v3))
    );
}

//https://registry.khronos.org/OpenGL-Refpages/gl4/html/reflect.xhtml

inline F32x4 F32x4_reflect2(F32x4 I, F32x4 N) {
    return F32x4_sub(I, F32x4_mul(N, F32x4_xxxx4(2 * F32x4_dot2(N, I))));
}

inline F32x4 F32x4_reflect3(F32x4 I, F32x4 N) {
    return F32x4_sub(I, F32x4_mul(N, F32x4_xxxx4(2 * F32x4_dot3(N, I))));
}
