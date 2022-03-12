#pragma once

#include <immintrin.h>
#include <xmmintrin.h>
#include <smmintrin.h>

#include "math_helper.h"

//Very lightweight layer around SIMD vectors
//Int vectors are ignored

typedef __m128 f32x4;

//Arithmetic

inline f32x4 Vec_zero();
inline f32x4 Vec_one();
inline f32x4 Vec_two();
inline f32x4 Vec_xxxx4(f32 x);

inline f32x4 Vec_add(f32x4 a, f32x4 b);
inline f32x4 Vec_sub(f32x4 a, f32x4 b);
inline f32x4 Vec_mul(f32x4 a, f32x4 b);
inline f32x4 Vec_div(f32x4 a, f32x4 b);

inline f32x4 Vec_complement(f32x4 a) { return Vec_sub(Vec_one(), a); }
inline f32x4 Vec_negate(f32x4 a) { return Vec_sub(Vec_zero(), a); }
inline f32x4 Vec_inverse(f32x4 a) { return Vec_div(Vec_one(), a); }

inline f32x4 Vec_pow2(f32x4 a) { return Vec_mul(a, a); }

inline f32x4 Vec_sign(f32x4 v);     //Zero counts as signed too
inline f32x4 Vec_abs(f32x4 v);
inline f32x4 Vec_ceil(f32x4 v);
inline f32x4 Vec_floor(f32x4 v);
inline f32x4 Vec_round(f32x4 v);
inline f32x4 Vec_fract(f32x4 v) { return Vec_sub(v, Vec_floor(v)); }
inline f32x4 Vec_mod(f32x4 v, f32x4 d) { return Vec_mul(Vec_fract(Vec_div(v, d)), d); }

inline f32 Vec_reduce(f32x4 a);     //ax+ay+az+aw

inline f32 Vec_dot2(f32x4 a, f32x4 b);
inline f32 Vec_dot3(f32x4 a, f32x4 b);
inline f32 Vec_dot4(f32x4 a, f32x4 b);

inline f32 Vec_sqLen2(f32x4 v) { return Vec_dot2(v, v); }
inline f32 Vec_sqLen3(f32x4 v) { return Vec_dot3(v, v); }
inline f32 Vec_sqLen4(f32x4 v) { return Vec_dot4(v, v); }

inline f32 Vec_len2(f32x4 v) { return Math_sqrtf(Vec_sqLen2(v)); }
inline f32 Vec_len3(f32x4 v) { return Math_sqrtf(Vec_sqLen3(v)); }
inline f32 Vec_len4(f32x4 v) { return Math_sqrtf(Vec_sqLen4(v)); }

inline f32x4 Vec_normalize2(f32x4 v);
inline f32x4 Vec_normalize3(f32x4 v);
inline f32x4 Vec_normalize4(f32x4 v);

inline f32x4 Vec_acos(f32x4 v);
inline f32x4 Vec_cos(f32x4 v);
inline f32x4 Vec_asin(f32x4 v);
inline f32x4 Vec_sin(f32x4 v);
inline f32x4 Vec_atan(f32x4 v);
inline f32x4 Vec_atan2(f32x4 y, f32x4 x);
inline f32x4 Vec_tan(f32x4 v);
inline f32x4 Vec_sqrt(f32x4 a);

inline f32x4 Vec_loge(f32x4 v);
inline f32x4 Vec_log10(f32x4 v);
inline f32x4 Vec_log2(f32x4 v);

inline f32x4 Vec_exp(f32x4 v);
inline f32x4 Vec_exp10(f32x4 v);
inline f32x4 Vec_exp2(f32x4 v);

inline f32x4 Vec_cross3(f32x4 a, f32x4 b);

inline f32x4 Vec_lerp(f32x4 a, f32x4 b, f32 perc) { 
    b = Vec_sub(b, a);
    return Vec_add(a, Vec_mul(b, Vec_xxxx4(perc)));
}

inline f32x4 Vec_min(f32x4 a, f32x4 b);
inline f32x4 Vec_max(f32x4 a, f32x4 b);
inline f32x4 Vec_clamp(f32x4 a, f32x4 mi, f32x4 ma) { return Vec_max(mi, Vec_min(ma, a)); }
inline f32x4 Vec_saturate(f32x4 a) { return Vec_clamp(a, Vec_zero(), Vec_one()); }

//Boolean

inline bool Vec_all(f32x4 a);
inline bool Vec_any(f32x4 b);

inline f32x4 Vec_eq(f32x4 a, f32x4 b);
inline f32x4 Vec_neq(f32x4 a, f32x4 b);
inline f32x4 Vec_geq(f32x4 a, f32x4 b);
inline f32x4 Vec_gt(f32x4 a, f32x4 b);
inline f32x4 Vec_leq(f32x4 a, f32x4 b);
inline f32x4 Vec_lt(f32x4 a, f32x4 b);

inline f32x4 Vec_or(f32x4 a, f32x4 b);
inline f32x4 Vec_and(f32x4 a, f32x4 b);
inline f32x4 Vec_xor(f32x4 a, f32x4 b);

inline bool Vec_eq4(f32x4 a, f32x4 b) { return Vec_all(Vec_eq(a, b)); }
inline bool Vec_neq4(f32x4 a, f32x4 b) { return !Vec_eq4(a, b); }

//Swizzles and shizzle

inline f32 Vec_x(f32x4 a);
inline f32 Vec_y(f32x4 a);
inline f32 Vec_z(f32x4 a);
inline f32 Vec_w(f32x4 a);
inline f32 Vec_get(f32x4 a, u8 i);

inline void Vec_setX(f32x4 *a, f32 v);
inline void Vec_setY(f32x4 *a, f32 v);
inline void Vec_setZ(f32x4 *a, f32 v);
inline void Vec_setW(f32x4 *a, f32 v);
inline void Vec_set(f32x4 *a, u8 i, f32 v);

//Construction

inline f32x4 Vec_init1(f32 x);
inline f32x4 Vec_init2(f32 x, f32 y);
inline f32x4 Vec_init3(f32 x, f32 y, f32 z);
inline f32x4 Vec_init4(f32 x, f32 y, f32 z, f32 w);

inline f32x4 Vec_load1(const f32 *arr);
inline f32x4 Vec_load2(const f32 *arr);
inline f32x4 Vec_load3(const f32 *arr);
inline f32x4 Vec_load4(const f32 *arr);

//Arch independent

//2D swizzles

inline f32x4 Vec_xx(f32x4 a) { return Vec_init2(Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_xy(f32x4 a) { return Vec_init2(Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_xz(f32x4 a) { return Vec_init2(Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_xw(f32x4 a) { return Vec_init2(Vec_x(a), Vec_w(a)); }

inline f32x4 Vec_yx(f32x4 a) { return Vec_init2(Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_yy(f32x4 a) { return Vec_init2(Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_yz(f32x4 a) { return Vec_init2(Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_yw(f32x4 a) { return Vec_init2(Vec_y(a), Vec_w(a)); }

inline f32x4 Vec_zx(f32x4 a) { return Vec_init2(Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_zy(f32x4 a) { return Vec_init2(Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_zz(f32x4 a) { return Vec_init2(Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_zw(f32x4 a) { return Vec_init2(Vec_z(a), Vec_w(a)); }

inline f32x4 Vec_wx(f32x4 a) { return Vec_init2(Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_wy(f32x4 a) { return Vec_init2(Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_wz(f32x4 a) { return Vec_init2(Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_ww(f32x4 a) { return Vec_init2(Vec_w(a), Vec_w(a)); }

//3D swizzles

inline f32x4 Vec_xxx(f32x4 a) { return Vec_init3(Vec_x(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_xxy(f32x4 a) { return Vec_init3(Vec_x(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_xxz(f32x4 a) { return Vec_init3(Vec_x(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_xyx(f32x4 a) { return Vec_init3(Vec_x(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_xyy(f32x4 a) { return Vec_init3(Vec_x(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_xyz(f32x4 a) { return Vec_init3(Vec_x(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_xzx(f32x4 a) { return Vec_init3(Vec_x(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_xzy(f32x4 a) { return Vec_init3(Vec_x(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_xzz(f32x4 a) { return Vec_init3(Vec_x(a), Vec_z(a), Vec_z(a)); }

inline f32x4 Vec_yxx(f32x4 a) { return Vec_init3(Vec_y(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_yxy(f32x4 a) { return Vec_init3(Vec_y(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_yxz(f32x4 a) { return Vec_init3(Vec_y(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_yyx(f32x4 a) { return Vec_init3(Vec_y(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_yyy(f32x4 a) { return Vec_init3(Vec_y(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_yyz(f32x4 a) { return Vec_init3(Vec_y(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_yzx(f32x4 a) { return Vec_init3(Vec_y(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_yzy(f32x4 a) { return Vec_init3(Vec_y(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_yzz(f32x4 a) { return Vec_init3(Vec_y(a), Vec_z(a), Vec_z(a)); }

inline f32x4 Vec_zxx(f32x4 a) { return Vec_init3(Vec_z(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_zxy(f32x4 a) { return Vec_init3(Vec_z(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_zxz(f32x4 a) { return Vec_init3(Vec_z(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_zyx(f32x4 a) { return Vec_init3(Vec_z(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_zyy(f32x4 a) { return Vec_init3(Vec_z(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_zyz(f32x4 a) { return Vec_init3(Vec_z(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_zzx(f32x4 a) { return Vec_init3(Vec_z(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_zzy(f32x4 a) { return Vec_init3(Vec_z(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_zzz(f32x4 a) { return Vec_init3(Vec_z(a), Vec_z(a), Vec_z(a)); }

//4D swizzles

inline f32x4 Vec_xxxx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_xxxy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_xxxz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_xxxw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_xxyx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_xxyy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_xxyz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_xxyw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_xxzx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_xxzy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_xxzz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_xxzw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_xxwx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_xxwy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_xxwz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_xxww(f32x4 a) { return Vec_init4(Vec_x(a), Vec_x(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_xyxx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_xyxy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_xyxz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_xyxw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_xyyx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_xyyy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_xyyz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_xyyw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_xyzx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_xyzy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_xyzz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_xyzw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_xywx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_xywy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_xywz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_xyww(f32x4 a) { return Vec_init4(Vec_x(a), Vec_y(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_xzxx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_xzxy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_xzxz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_xzxw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_xzyx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_xzyy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_xzyz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_xzyw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_xzzx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_xzzy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_xzzz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_xzzw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_xzwx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_xzwy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_xzwz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_xzww(f32x4 a) { return Vec_init4(Vec_x(a), Vec_z(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_xwxx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_xwxy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_xwxz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_xwxw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_xwyx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_xwyy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_xwyz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_xwyw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_xwzx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_xwzy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_xwzz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_xwzw(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_xwwx(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_xwwy(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_xwwz(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_xwww(f32x4 a) { return Vec_init4(Vec_x(a), Vec_w(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_yxxx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_yxxy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_yxxz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_yxxw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_yxyx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_yxyy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_yxyz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_yxyw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_yxzx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_yxzy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_yxzz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_yxzw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_yxwx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_yxwy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_yxwz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_yxww(f32x4 a) { return Vec_init4(Vec_y(a), Vec_x(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_yyxx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_yyxy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_yyxz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_yyxw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_yyyx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_yyyy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_yyyz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_yyyw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_yyzx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_yyzy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_yyzz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_yyzw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_yywx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_yywy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_yywz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_yyww(f32x4 a) { return Vec_init4(Vec_y(a), Vec_y(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_yzxx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_yzxy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_yzxz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_yzxw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_yzyx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_yzyy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_yzyz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_yzyw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_yzzx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_yzzy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_yzzz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_yzzw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_yzwx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_yzwy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_yzwz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_yzww(f32x4 a) { return Vec_init4(Vec_y(a), Vec_z(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_ywxx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_ywxy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_ywxz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_ywxw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_ywyx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_ywyy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_ywyz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_ywyw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_ywzx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_ywzy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_ywzz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_ywzw(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_ywwx(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_ywwy(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_ywwz(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_ywww(f32x4 a) { return Vec_init4(Vec_y(a), Vec_w(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_zxxx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_zxxy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_zxxz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_zxxw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_zxyx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_zxyy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_zxyz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_zxyw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_zxzx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_zxzy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_zxzz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_zxzw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_zxwx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_zxwy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_zxwz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_zxww(f32x4 a) { return Vec_init4(Vec_z(a), Vec_x(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_zyxx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_zyxy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_zyxz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_zyxw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_zyyx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_zyyy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_zyyz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_zyyw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_zyzx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_zyzy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_zyzz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_zyzw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_zywx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_zywy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_zywz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_zyww(f32x4 a) { return Vec_init4(Vec_z(a), Vec_y(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_zzxx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_zzxy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_zzxz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_zzxw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_zzyx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_zzyy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_zzyz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_zzyw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_zzzx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_zzzy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_zzzz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_zzzw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_zzwx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_zzwy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_zzwz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_zzww(f32x4 a) { return Vec_init4(Vec_z(a), Vec_z(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_zwxx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_zwxy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_zwxz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_zwxw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_zwyx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_zwyy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_zwyz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_zwyw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_zwzx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_zwzy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_zwzz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_zwzw(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_zwwx(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_zwwy(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_zwwz(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_zwww(f32x4 a) { return Vec_init4(Vec_z(a), Vec_w(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_wxxx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_wxxy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_wxxz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_wxxw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_wxyx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_wxyy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_wxyz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_wxyw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_wxzx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_wxzy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_wxzz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_wxzw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_wxwx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_wxwy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_wxwz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_wxww(f32x4 a) { return Vec_init4(Vec_w(a), Vec_x(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_wyxx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_wyxy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_wyxz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_wyxw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_wyyx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_wyyy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_wyyz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_wyyw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_wyzx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_wyzy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_wyzz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_wyzw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_wywx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_wywy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_wywz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_wyww(f32x4 a) { return Vec_init4(Vec_w(a), Vec_y(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_wzxx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_wzxy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_wzxz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_wzxw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_wzyx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_wzyy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_wzyz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_wzyw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_wzzx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_wzzy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_wzzz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_wzzw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_wzwx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_wzwy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_wzwz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_wzww(f32x4 a) { return Vec_init4(Vec_w(a), Vec_z(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_wwxx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_x(a), Vec_x(a)); }
inline f32x4 Vec_wwxy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_x(a), Vec_y(a)); }
inline f32x4 Vec_wwxz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_x(a), Vec_z(a)); }
inline f32x4 Vec_wwxw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_x(a), Vec_w(a)); }
inline f32x4 Vec_wwyx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_y(a), Vec_x(a)); }
inline f32x4 Vec_wwyy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_y(a), Vec_y(a)); }
inline f32x4 Vec_wwyz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_y(a), Vec_z(a)); }
inline f32x4 Vec_wwyw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_y(a), Vec_w(a)); }
inline f32x4 Vec_wwzx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_z(a), Vec_x(a)); }
inline f32x4 Vec_wwzy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_z(a), Vec_y(a)); }
inline f32x4 Vec_wwzz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_z(a), Vec_z(a)); }
inline f32x4 Vec_wwzw(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_z(a), Vec_w(a)); }
inline f32x4 Vec_wwwx(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_w(a), Vec_x(a)); }
inline f32x4 Vec_wwwy(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_w(a), Vec_y(a)); }
inline f32x4 Vec_wwwz(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_w(a), Vec_z(a)); }
inline f32x4 Vec_wwww(f32x4 a) { return Vec_init4(Vec_w(a), Vec_w(a), Vec_w(a), Vec_w(a)); }

inline f32x4 Vec_one() { return Vec_xxxx4(1); }
inline f32x4 Vec_two() { return Vec_xxxx4(2); }

inline bool Vec_all(f32x4 b) { return Vec_reduce(Vec_neq(b, Vec_zero())) == 4; }
inline bool Vec_any(f32x4 b) { return Vec_reduce(Vec_neq(b, Vec_zero())); }

inline f32x4 Vec_normalize2(f32x4 v) { return Vec_mul(v, Vec_xxxx4(1 / Vec_len2(v))); }
inline f32x4 Vec_normalize3(f32x4 v) { return Vec_mul(v, Vec_xxxx4(1 / Vec_len3(v))); }
inline f32x4 Vec_normalize4(f32x4 v) { return Vec_mul(v, Vec_xxxx4(1 / Vec_len4(v))); }

inline f32x4 Vec_load1(const f32 *arr) { return Vec_init1(*arr); }
inline f32x4 Vec_load2(const f32 *arr) { return Vec_init2(*arr, arr[1]); }
inline f32x4 Vec_load3(const f32 *arr) { return Vec_init3(*arr, arr[1], arr[2]); }

inline void Vec_setX(f32x4 *a, f32 v) { *a = Vec_init4(v, Vec_y(*a), Vec_z(*a), Vec_w(*a)); }
inline void Vec_setY(f32x4 *a, f32 v) { *a = Vec_init4(Vec_x(*a), v, Vec_z(*a), Vec_w(*a)); }
inline void Vec_setZ(f32x4 *a, f32 v) { *a = Vec_init4(Vec_x(*a), Vec_y(*a), v, Vec_w(*a)); }
inline void Vec_setW(f32x4 *a, f32 v) { *a = Vec_init4(Vec_x(*a), Vec_y(*a), Vec_z(*a), v); }

inline void Vec_set(f32x4 *a, u8 i, f32 v) {
    switch (i & 3) {
        case 0:     Vec_setX(a, v);     break;
        case 1:     Vec_setY(a, v);     break;
        case 2:     Vec_setZ(a, v);     break;
        default:    Vec_setW(a, v);
    }
}

inline f32 Vec_get(f32x4 a, u8 i) {
    switch (i & 3) {
        case 0:     return Vec_x(a);
        case 1:     return Vec_y(a);
        case 2:     return Vec_z(a);
        default:    return Vec_w(a);
    }
}

inline f32 Vec_dot4(f32x4 a, f32x4 b) { return Vec_reduce(Vec_mul(a, b)); }
inline f32 Vec_dot2(f32x4 a, f32x4 b) { return Vec_dot4(Vec_xy(a), Vec_xy(b)); }
inline f32 Vec_dot3(f32x4 a, f32x4 b) { return Vec_dot4(Vec_xyz(a), Vec_xyz(b)); }

//Arch dependent source; arithmetic

inline f32x4 Vec_add(f32x4 a, f32x4 b) { return _mm_add_ps(a, b); }
inline f32x4 Vec_sub(f32x4 a, f32x4 b) { return _mm_sub_ps(a, b); }
inline f32x4 Vec_mul(f32x4 a, f32x4 b) { return _mm_mul_ps(a, b); }
inline f32x4 Vec_div(f32x4 a, f32x4 b) { return _mm_div_ps(a, b); }

inline f32x4 Vec_ceil(f32x4 a) { return _mm_ceil_ps(a); }
inline f32x4 Vec_floor(f32x4 a) { return _mm_floor_ps(a); }
inline f32x4 Vec_round(f32x4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }

inline f32x4 Vec_sqrt(f32x4 a) { return _mm_sqrt_ps(a); }

inline f32x4 Vec_acos(f32x4 v) { return _mm_acos_ps(v); }
inline f32x4 Vec_cos(f32x4 v) { return _mm_cos_ps(v); }
inline f32x4 Vec_asin(f32x4 v) { return _mm_asin_ps(v); }
inline f32x4 Vec_sin(f32x4 v) { return _mm_sin_ps(v); }
inline f32x4 Vec_atan(f32x4 v) { return _mm_atan_ps(v); }
inline f32x4 Vec_atan2(f32x4 y, f32x4 x) { return _mm_atan2_ps(y, x); }
inline f32x4 Vec_tan(f32x4 v) { return _mm_tan_ps(v); }

inline f32x4 Vec_loge(f32x4 v) { return _mm_log_ps(v); }
inline f32x4 Vec_log10(f32x4 v) { return _mm_log10_ps(v); }
inline f32x4 Vec_log2(f32x4 v) { return _mm_log2_ps(v); }

inline f32x4 Vec_exp(f32x4 v) { return _mm_exp_ps(v); }
inline f32x4 Vec_exp10(f32x4 v) { return _mm_exp10_ps(v); }
inline f32x4 Vec_exp2(f32x4 v)  { return _mm_exp2_ps(v); }

//Shuffle and extracting values

#define _shuffle(a, x, y, z, w) _mm_shuffle_ps(a, a, _MM_SHUFFLE(x, y, z, w))
#define _shuffle1(a, x) _shuffle(a, x, x, x, x)

inline f32 Vec_x(f32x4 a) { return _mm_cvtss_f32(a); }
inline f32 Vec_y(f32x4 a) { return Vec_x(Vec_yyyy(a)); }
inline f32 Vec_z(f32x4 a) { return Vec_x(Vec_zzzz(a)); }
inline f32 Vec_w(f32x4 a) { return Vec_x(Vec_wwww(a)); }

inline f32x4 Vec_init1(f32 x) { return _mm_set_ps(x, 0, 0, 0); }
inline f32x4 Vec_init2(f32 x, f32 y) { return _mm_set_ps(x, y, 0, 0); }
inline f32x4 Vec_init3(f32 x, f32 y, f32 z) { return _mm_set_ps(x, y, z, 0); }
inline f32x4 Vec_init4(f32 x, f32 y, f32 z, f32 w) { return _mm_set_ps(x, y, z, w); }
inline f32x4 Vec_xxxx4(f32 x) { return _mm_set1_ps(x); }

inline f32x4 Vec_load4(const f32 *arr) { return _mm_load_ps(arr);}

inline f32x4 Vec_zero() { return _mm_setzero_ps(); }

//Needed since cmp returns -1 as int, so we convert to -1.f and then negate

inline f32x4 _cvtitof(f32x4 a) { return _mm_cvtepi32_ps(*(__m128i*) &a); }
inline f32x4 _Vec_negatei(f32x4 a) { return Vec_negate(_cvtitof(a)); }

inline f32x4 Vec_eq(f32x4 a, f32x4 b)  { return _Vec_negatei(_mm_cmpeq_ps(a, b)); }
inline f32x4 Vec_neq(f32x4 a, f32x4 b) { return _Vec_negatei(_mm_cmpneq_ps(a, b)); }
inline f32x4 Vec_geq(f32x4 a, f32x4 b) { return _Vec_negatei(_mm_cmpge_ps(a, b)); }
inline f32x4 Vec_gt(f32x4 a, f32x4 b)  { return _Vec_negatei(_mm_cmpgt_ps(a, b)); }
inline f32x4 Vec_leq(f32x4 a, f32x4 b) { return _Vec_negatei(_mm_cmple_ps(a, b)); }
inline f32x4 Vec_lt(f32x4 a, f32x4 b)  { return _Vec_negatei(_mm_cmplt_ps(a, b)); }

inline f32x4 Vec_or(f32x4 a, f32x4 b)  { return _cvtitof(_mm_or_ps(a, b)); }
inline f32x4 Vec_and(f32x4 a, f32x4 b) { return _cvtitof(_mm_and_ps(a, b)); }
inline f32x4 Vec_xor(f32x4 a, f32x4 b) { return _cvtitof(_mm_xor_ps(a, b)); }

inline f32x4 Vec_min(f32x4 a, f32x4 b) { return _mm_min_ps(a, b); }
inline f32x4 Vec_max(f32x4 a, f32x4 b) { return _mm_max_ps(a, b); }

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0
//
inline f32x4 Vec_sign(f32x4 v) {
    return Vec_add(
        Vec_mul(_mm_cmplt_ps(v, Vec_zero()), Vec_two()), 
        Vec_one()
    ); 
}

inline f32x4 Vec_cross3(f32x4 a, f32x4 b) {
    return Vec_normalize3(
        Vec_sub(
            Vec_mul(Vec_yzx(a), Vec_zxy(b)),
            Vec_mul(Vec_zxy(a), Vec_yzx(b))
        )
    );
}

inline f32x4 Vec_abs(f32x4 v){ return Vec_mul(Vec_sign(v), v); }

inline f32 Vec_reduce(f32x4 a) {
    return Vec_x(_mm_hadd_ps(_mm_hadd_ps(a, Vec_zero()), Vec_zero()));
}