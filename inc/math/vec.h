#pragma once

#include <immintrin.h>
#include <xmmintrin.h>
#include <smmintrin.h>

#include "math/math.h"

//Very lightweight layer around SIMD vectors
//Int vectors are ignored

typedef __m128 F32x4;

//Arithmetic

inline F32x4 Vec_zero();
inline F32x4 Vec_one();
inline F32x4 Vec_two();
inline F32x4 Vec_mask2();
inline F32x4 Vec_mask3();
inline F32x4 Vec_xxxx4(F32 x);

inline F32x4 Vec_add(F32x4 a, F32x4 b);
inline F32x4 Vec_sub(F32x4 a, F32x4 b);
inline F32x4 Vec_mul(F32x4 a, F32x4 b);
inline F32x4 Vec_div(F32x4 a, F32x4 b);

inline F32x4 Vec_srgb8Unpack(U32 v);
inline U32 Vec_srgb8Pack(F32x4 v);
inline F32x4 Vec_rgb8Unpack(U32 v);
inline U32 Vec_rgb8Pack(F32x4 v);

inline F32x4 Vec_complement(F32x4 a) { return Vec_sub(Vec_one(), a); }
inline F32x4 Vec_negate(F32x4 a) { return Vec_sub(Vec_zero(), a); }
inline F32x4 Vec_inverse(F32x4 a) { return Vec_div(Vec_one(), a); }

inline F32x4 Vec_pow2(F32x4 a) { return Vec_mul(a, a); }

inline F32x4 Vec_sign(F32x4 v);     //Zero counts as signed too
inline F32x4 Vec_abs(F32x4 v);
inline F32x4 Vec_ceil(F32x4 v);
inline F32x4 Vec_floor(F32x4 v);
inline F32x4 Vec_round(F32x4 v);
inline F32x4 Vec_fract(F32x4 v) { return Vec_sub(v, Vec_floor(v)); }
inline F32x4 Vec_mod(F32x4 v, F32x4 d) { return Vec_mul(Vec_fract(Vec_div(v, d)), d); }

inline F32 Vec_reduce(F32x4 a);     //ax+ay+az+aw

inline F32 Vec_dot2(F32x4 a, F32x4 b);
inline F32 Vec_dot3(F32x4 a, F32x4 b);
inline F32 Vec_dot4(F32x4 a, F32x4 b);

inline F32 Vec_sqLen2(F32x4 v);
inline F32 Vec_sqLen3(F32x4 v);
inline F32 Vec_sqLen4(F32x4 v);

inline F32 Vec_len2(F32x4 v) { return Math_sqrtf(Vec_sqLen2(v)); }
inline F32 Vec_len3(F32x4 v) { return Math_sqrtf(Vec_sqLen3(v)); }
inline F32 Vec_len4(F32x4 v) { return Math_sqrtf(Vec_sqLen4(v)); }

inline F32x4 Vec_normalize2(F32x4 v);
inline F32x4 Vec_normalize3(F32x4 v);
inline F32x4 Vec_normalize4(F32x4 v);

inline F32x4 Vec_acos(F32x4 v);
inline F32x4 Vec_cos(F32x4 v);
inline F32x4 Vec_asin(F32x4 v);
inline F32x4 Vec_sin(F32x4 v);
inline F32x4 Vec_atan(F32x4 v);
inline F32x4 Vec_atan2(F32x4 y, F32x4 x);
inline F32x4 Vec_tan(F32x4 v);
inline F32x4 Vec_sqrt(F32x4 a);
inline F32x4 Vec_rsqrt(F32x4 a);

inline F32x4 Vec_loge(F32x4 v);
inline F32x4 Vec_log10(F32x4 v);
inline F32x4 Vec_log2(F32x4 v);

inline F32x4 Vec_exp(F32x4 v);
inline F32x4 Vec_exp10(F32x4 v);
inline F32x4 Vec_exp2(F32x4 v);

inline F32x4 Vec_cross3(F32x4 a, F32x4 b);

inline F32x4 Vec_lerp(F32x4 a, F32x4 b, F32 perc) { 
    b = Vec_sub(b, a);
    return Vec_add(a, Vec_mul(b, Vec_xxxx4(perc)));
}

inline F32x4 Vec_min(F32x4 a, F32x4 b);
inline F32x4 Vec_max(F32x4 a, F32x4 b);
inline F32x4 Vec_clamp(F32x4 a, F32x4 mi, F32x4 ma) { return Vec_max(mi, Vec_min(ma, a)); }
inline F32x4 Vec_saturate(F32x4 a) { return Vec_clamp(a, Vec_zero(), Vec_one()); }

//Boolean

inline Bool Vec_all(F32x4 a);
inline Bool Vec_any(F32x4 b);

inline F32x4 Vec_eq(F32x4 a, F32x4 b);
inline F32x4 Vec_neq(F32x4 a, F32x4 b);
inline F32x4 Vec_geq(F32x4 a, F32x4 b);
inline F32x4 Vec_gt(F32x4 a, F32x4 b);
inline F32x4 Vec_leq(F32x4 a, F32x4 b);
inline F32x4 Vec_lt(F32x4 a, F32x4 b);

inline F32x4 Vec_or(F32x4 a, F32x4 b);
inline F32x4 Vec_and(F32x4 a, F32x4 b);
inline F32x4 Vec_xor(F32x4 a, F32x4 b);

inline Bool Vec_eq4(F32x4 a, F32x4 b) { return Vec_all(Vec_eq(a, b)); }
inline Bool Vec_neq4(F32x4 a, F32x4 b) { return !Vec_eq4(a, b); }

//Swizzles and shizzle

inline F32 Vec_x(F32x4 a);
inline F32 Vec_y(F32x4 a);
inline F32 Vec_z(F32x4 a);
inline F32 Vec_w(F32x4 a);
inline F32 Vec_get(F32x4 a, U8 i);

inline F32x4 Vec_xxxx(F32x4 a);
inline F32x4 Vec_yyyy(F32x4 a); 
inline F32x4 Vec_zzzz(F32x4 a);
inline F32x4 Vec_wwww(F32x4 a);

inline void Vec_setX(F32x4 *a, F32 v);
inline void Vec_setY(F32x4 *a, F32 v);
inline void Vec_setZ(F32x4 *a, F32 v);
inline void Vec_setW(F32x4 *a, F32 v);
inline void Vec_set(F32x4 *a, U8 i, F32 v);

//Construction

inline F32x4 Vec_create1(F32 x);
inline F32x4 Vec_create2(F32 x, F32 y);
inline F32x4 Vec_create3(F32 x, F32 y, F32 z);
inline F32x4 Vec_create4(F32 x, F32 y, F32 z, F32 w);

inline F32x4 Vec_load1(const F32 *arr);
inline F32x4 Vec_load2(const F32 *arr);
inline F32x4 Vec_load3(const F32 *arr);
inline F32x4 Vec_load4(const F32 *arr);

inline F32x4 Vec_combine2(F32x4 xy, F32x4 zw) { 
	return Vec_create4(Vec_x(xy), Vec_y(xy), Vec_x(zw), Vec_y(zw)); 
}

//Arch dependent source; arithmetic

inline F32x4 Vec_add(F32x4 a, F32x4 b) { return _mm_add_ps(a, b); }
inline F32x4 Vec_sub(F32x4 a, F32x4 b) { return _mm_sub_ps(a, b); }
inline F32x4 Vec_mul(F32x4 a, F32x4 b) { return _mm_mul_ps(a, b); }
inline F32x4 Vec_div(F32x4 a, F32x4 b) { return _mm_div_ps(a, b); }

inline F32x4 Vec_ceil(F32x4 a) { return _mm_ceil_ps(a); }
inline F32x4 Vec_floor(F32x4 a) { return _mm_floor_ps(a); }
inline F32x4 Vec_round(F32x4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }

inline F32x4 Vec_sqrt(F32x4 a) { return _mm_sqrt_ps(a); }
inline F32x4 Vec_rsqrt(F32x4 a) { return _mm_rsqrt_ps(a); }

inline F32x4 Vec_acos(F32x4 v) { return _mm_acos_ps(v); }
inline F32x4 Vec_cos(F32x4 v) { return _mm_cos_ps(v); }
inline F32x4 Vec_asin(F32x4 v) { return _mm_asin_ps(v); }
inline F32x4 Vec_sin(F32x4 v) { return _mm_sin_ps(v); }
inline F32x4 Vec_atan(F32x4 v) { return _mm_atan_ps(v); }
inline F32x4 Vec_atan2(F32x4 y, F32x4 x) { return _mm_atan2_ps(y, x); }
inline F32x4 Vec_tan(F32x4 v) { return _mm_tan_ps(v); }

inline F32x4 Vec_loge(F32x4 v) { return _mm_log_ps(v); }
inline F32x4 Vec_log10(F32x4 v) { return _mm_log10_ps(v); }
inline F32x4 Vec_log2(F32x4 v) { return _mm_log2_ps(v); }

inline F32x4 Vec_exp(F32x4 v) { return _mm_exp_ps(v); }
inline F32x4 Vec_exp10(F32x4 v) { return _mm_exp10_ps(v); }
inline F32x4 Vec_exp2(F32x4 v)  { return _mm_exp2_ps(v); }

//Shuffle and extracting values

#define _shuffle(a, x, y, z, w) _mm_shuffle_ps(a, a, _MM_SHUFFLE(w, z, y, x))
#define _shuffle1(a, x) _shuffle(a, x, x, x, x)

//4D swizzles

inline F32x4 Vec_xxxx(F32x4 a) { return _shuffle1(a, 0); }
inline F32x4 Vec_xxxy(F32x4 a) { return _shuffle(a, 0, 0, 0, 1); }
inline F32x4 Vec_xxxz(F32x4 a) { return _shuffle(a, 0, 0, 0, 2); }
inline F32x4 Vec_xxxw(F32x4 a) { return _shuffle(a, 0, 0, 0, 3); }
inline F32x4 Vec_xxyx(F32x4 a) { return _shuffle(a, 0, 0, 1, 0); }
inline F32x4 Vec_xxyy(F32x4 a) { return _shuffle(a, 0, 0, 1, 1); }
inline F32x4 Vec_xxyz(F32x4 a) { return _shuffle(a, 0, 0, 1, 2); }
inline F32x4 Vec_xxyw(F32x4 a) { return _shuffle(a, 0, 0, 1, 3); }
inline F32x4 Vec_xxzx(F32x4 a) { return _shuffle(a, 0, 0, 2, 0); }
inline F32x4 Vec_xxzy(F32x4 a) { return _shuffle(a, 0, 0, 2, 1); }
inline F32x4 Vec_xxzz(F32x4 a) { return _shuffle(a, 0, 0, 2, 2); }
inline F32x4 Vec_xxzw(F32x4 a) { return _shuffle(a, 0, 0, 2, 3); }
inline F32x4 Vec_xxwx(F32x4 a) { return _shuffle(a, 0, 0, 3, 0); }
inline F32x4 Vec_xxwy(F32x4 a) { return _shuffle(a, 0, 0, 3, 1); }
inline F32x4 Vec_xxwz(F32x4 a) { return _shuffle(a, 0, 0, 3, 2); }
inline F32x4 Vec_xxww(F32x4 a) { return _shuffle(a, 0, 0, 3, 3); }

inline F32x4 Vec_xyxx(F32x4 a) { return _shuffle(a, 0, 1, 0, 0); }
inline F32x4 Vec_xyxy(F32x4 a) { return _shuffle(a, 0, 1, 0, 1); }
inline F32x4 Vec_xyxz(F32x4 a) { return _shuffle(a, 0, 1, 0, 2); }
inline F32x4 Vec_xyxw(F32x4 a) { return _shuffle(a, 0, 1, 0, 3); }
inline F32x4 Vec_xyyx(F32x4 a) { return _shuffle(a, 0, 1, 1, 0); }
inline F32x4 Vec_xyyy(F32x4 a) { return _shuffle(a, 0, 1, 1, 1); }
inline F32x4 Vec_xyyz(F32x4 a) { return _shuffle(a, 0, 1, 1, 2); }
inline F32x4 Vec_xyyw(F32x4 a) { return _shuffle(a, 0, 1, 1, 3); }
inline F32x4 Vec_xyzx(F32x4 a) { return _shuffle(a, 0, 1, 2, 0); }
inline F32x4 Vec_xyzy(F32x4 a) { return _shuffle(a, 0, 1, 2, 1); }
inline F32x4 Vec_xyzz(F32x4 a) { return _shuffle(a, 0, 1, 2, 2); }
inline F32x4 Vec_xyzw(F32x4 a) { return _shuffle(a, 0, 1, 2, 3); }
inline F32x4 Vec_xywx(F32x4 a) { return _shuffle(a, 0, 1, 3, 0); }
inline F32x4 Vec_xywy(F32x4 a) { return _shuffle(a, 0, 1, 3, 1); }
inline F32x4 Vec_xywz(F32x4 a) { return _shuffle(a, 0, 1, 3, 2); }
inline F32x4 Vec_xyww(F32x4 a) { return _shuffle(a, 0, 1, 3, 3); }

inline F32x4 Vec_xzxx(F32x4 a) { return _shuffle(a, 0, 2, 0, 0); }
inline F32x4 Vec_xzxy(F32x4 a) { return _shuffle(a, 0, 2, 0, 1); }
inline F32x4 Vec_xzxz(F32x4 a) { return _shuffle(a, 0, 2, 0, 2); }
inline F32x4 Vec_xzxw(F32x4 a) { return _shuffle(a, 0, 2, 0, 3); }
inline F32x4 Vec_xzyx(F32x4 a) { return _shuffle(a, 0, 2, 1, 0); }
inline F32x4 Vec_xzyy(F32x4 a) { return _shuffle(a, 0, 2, 1, 1); }
inline F32x4 Vec_xzyz(F32x4 a) { return _shuffle(a, 0, 2, 1, 2); }
inline F32x4 Vec_xzyw(F32x4 a) { return _shuffle(a, 0, 2, 1, 3); }
inline F32x4 Vec_xzzx(F32x4 a) { return _shuffle(a, 0, 2, 2, 0); }
inline F32x4 Vec_xzzy(F32x4 a) { return _shuffle(a, 0, 2, 2, 1); }
inline F32x4 Vec_xzzz(F32x4 a) { return _shuffle(a, 0, 2, 2, 2); }
inline F32x4 Vec_xzzw(F32x4 a) { return _shuffle(a, 0, 2, 2, 3); }
inline F32x4 Vec_xzwx(F32x4 a) { return _shuffle(a, 0, 2, 3, 0); }
inline F32x4 Vec_xzwy(F32x4 a) { return _shuffle(a, 0, 2, 3, 1); }
inline F32x4 Vec_xzwz(F32x4 a) { return _shuffle(a, 0, 2, 3, 2); }
inline F32x4 Vec_xzww(F32x4 a) { return _shuffle(a, 0, 2, 3, 3); }

inline F32x4 Vec_xwxx(F32x4 a) { return _shuffle(a, 0, 3, 0, 0); }
inline F32x4 Vec_xwxy(F32x4 a) { return _shuffle(a, 0, 3, 0, 1); }
inline F32x4 Vec_xwxz(F32x4 a) { return _shuffle(a, 0, 3, 0, 2); }
inline F32x4 Vec_xwxw(F32x4 a) { return _shuffle(a, 0, 3, 0, 3); }
inline F32x4 Vec_xwyx(F32x4 a) { return _shuffle(a, 0, 3, 1, 0); }
inline F32x4 Vec_xwyy(F32x4 a) { return _shuffle(a, 0, 3, 1, 1); }
inline F32x4 Vec_xwyz(F32x4 a) { return _shuffle(a, 0, 3, 1, 2); }
inline F32x4 Vec_xwyw(F32x4 a) { return _shuffle(a, 0, 3, 1, 3); }
inline F32x4 Vec_xwzx(F32x4 a) { return _shuffle(a, 0, 3, 2, 0); }
inline F32x4 Vec_xwzy(F32x4 a) { return _shuffle(a, 0, 3, 2, 1); }
inline F32x4 Vec_xwzz(F32x4 a) { return _shuffle(a, 0, 3, 2, 2); }
inline F32x4 Vec_xwzw(F32x4 a) { return _shuffle(a, 0, 3, 2, 3); }
inline F32x4 Vec_xwwx(F32x4 a) { return _shuffle(a, 0, 3, 3, 0); }
inline F32x4 Vec_xwwy(F32x4 a) { return _shuffle(a, 0, 3, 3, 1); }
inline F32x4 Vec_xwwz(F32x4 a) { return _shuffle(a, 0, 3, 3, 2); }
inline F32x4 Vec_xwww(F32x4 a) { return _shuffle(a, 0, 3, 3, 3); }

inline F32x4 Vec_yxxx(F32x4 a) { return _shuffle(a, 1, 0, 0, 0); }
inline F32x4 Vec_yxxy(F32x4 a) { return _shuffle(a, 1, 0, 0, 1); }
inline F32x4 Vec_yxxz(F32x4 a) { return _shuffle(a, 1, 0, 0, 2); }
inline F32x4 Vec_yxxw(F32x4 a) { return _shuffle(a, 1, 0, 0, 3); }
inline F32x4 Vec_yxyx(F32x4 a) { return _shuffle(a, 1, 0, 1, 0); }
inline F32x4 Vec_yxyy(F32x4 a) { return _shuffle(a, 1, 0, 1, 1); }
inline F32x4 Vec_yxyz(F32x4 a) { return _shuffle(a, 1, 0, 1, 2); }
inline F32x4 Vec_yxyw(F32x4 a) { return _shuffle(a, 1, 0, 1, 3); }
inline F32x4 Vec_yxzx(F32x4 a) { return _shuffle(a, 1, 0, 2, 0); }
inline F32x4 Vec_yxzy(F32x4 a) { return _shuffle(a, 1, 0, 2, 1); }
inline F32x4 Vec_yxzz(F32x4 a) { return _shuffle(a, 1, 0, 2, 2); }
inline F32x4 Vec_yxzw(F32x4 a) { return _shuffle(a, 1, 0, 2, 3); }
inline F32x4 Vec_yxwx(F32x4 a) { return _shuffle(a, 1, 0, 3, 0); }
inline F32x4 Vec_yxwy(F32x4 a) { return _shuffle(a, 1, 0, 3, 1); }
inline F32x4 Vec_yxwz(F32x4 a) { return _shuffle(a, 1, 0, 3, 2); }
inline F32x4 Vec_yxww(F32x4 a) { return _shuffle(a, 1, 0, 3, 3); }

inline F32x4 Vec_yyxx(F32x4 a) { return _shuffle(a, 1, 1, 0, 0); }
inline F32x4 Vec_yyxy(F32x4 a) { return _shuffle(a, 1, 1, 0, 1); }
inline F32x4 Vec_yyxz(F32x4 a) { return _shuffle(a, 1, 1, 0, 2); }
inline F32x4 Vec_yyxw(F32x4 a) { return _shuffle(a, 1, 1, 0, 3); }
inline F32x4 Vec_yyyx(F32x4 a) { return _shuffle(a, 1, 1, 1, 0); }
inline F32x4 Vec_yyyy(F32x4 a) { return _shuffle1(a, 1); } 
inline F32x4 Vec_yyyz(F32x4 a) { return _shuffle(a, 1, 1, 1, 2); }
inline F32x4 Vec_yyyw(F32x4 a) { return _shuffle(a, 1, 1, 1, 3); }
inline F32x4 Vec_yyzx(F32x4 a) { return _shuffle(a, 1, 1, 2, 0); }
inline F32x4 Vec_yyzy(F32x4 a) { return _shuffle(a, 1, 1, 2, 1); }
inline F32x4 Vec_yyzz(F32x4 a) { return _shuffle(a, 1, 1, 2, 2); }
inline F32x4 Vec_yyzw(F32x4 a) { return _shuffle(a, 1, 1, 2, 3); }
inline F32x4 Vec_yywx(F32x4 a) { return _shuffle(a, 1, 1, 3, 0); }
inline F32x4 Vec_yywy(F32x4 a) { return _shuffle(a, 1, 1, 3, 1); }
inline F32x4 Vec_yywz(F32x4 a) { return _shuffle(a, 1, 1, 3, 2); }
inline F32x4 Vec_yyww(F32x4 a) { return _shuffle(a, 1, 1, 3, 3); }

inline F32x4 Vec_yzxx(F32x4 a) { return _shuffle(a, 1, 2, 0, 0); }
inline F32x4 Vec_yzxy(F32x4 a) { return _shuffle(a, 1, 2, 0, 1); }
inline F32x4 Vec_yzxz(F32x4 a) { return _shuffle(a, 1, 2, 0, 2); }
inline F32x4 Vec_yzxw(F32x4 a) { return _shuffle(a, 1, 2, 0, 3); }
inline F32x4 Vec_yzyx(F32x4 a) { return _shuffle(a, 1, 2, 1, 0); }
inline F32x4 Vec_yzyy(F32x4 a) { return _shuffle(a, 1, 2, 1, 1); }
inline F32x4 Vec_yzyz(F32x4 a) { return _shuffle(a, 1, 2, 1, 2); }
inline F32x4 Vec_yzyw(F32x4 a) { return _shuffle(a, 1, 2, 1, 3); }
inline F32x4 Vec_yzzx(F32x4 a) { return _shuffle(a, 1, 2, 2, 0); }
inline F32x4 Vec_yzzy(F32x4 a) { return _shuffle(a, 1, 2, 2, 1); }
inline F32x4 Vec_yzzz(F32x4 a) { return _shuffle(a, 1, 2, 2, 2); }
inline F32x4 Vec_yzzw(F32x4 a) { return _shuffle(a, 1, 2, 2, 3); }
inline F32x4 Vec_yzwx(F32x4 a) { return _shuffle(a, 1, 2, 3, 0); }
inline F32x4 Vec_yzwy(F32x4 a) { return _shuffle(a, 1, 2, 3, 1); }
inline F32x4 Vec_yzwz(F32x4 a) { return _shuffle(a, 1, 2, 3, 2); }
inline F32x4 Vec_yzww(F32x4 a) { return _shuffle(a, 1, 2, 3, 3); }

inline F32x4 Vec_ywxx(F32x4 a) { return _shuffle(a, 1, 3, 0, 0); }
inline F32x4 Vec_ywxy(F32x4 a) { return _shuffle(a, 1, 3, 0, 1); }
inline F32x4 Vec_ywxz(F32x4 a) { return _shuffle(a, 1, 3, 0, 2); }
inline F32x4 Vec_ywxw(F32x4 a) { return _shuffle(a, 1, 3, 0, 3); }
inline F32x4 Vec_ywyx(F32x4 a) { return _shuffle(a, 1, 3, 1, 0); }
inline F32x4 Vec_ywyy(F32x4 a) { return _shuffle(a, 1, 3, 1, 1); }
inline F32x4 Vec_ywyz(F32x4 a) { return _shuffle(a, 1, 3, 1, 2); }
inline F32x4 Vec_ywyw(F32x4 a) { return _shuffle(a, 1, 3, 1, 3); }
inline F32x4 Vec_ywzx(F32x4 a) { return _shuffle(a, 1, 3, 2, 0); }
inline F32x4 Vec_ywzy(F32x4 a) { return _shuffle(a, 1, 3, 2, 1); }
inline F32x4 Vec_ywzz(F32x4 a) { return _shuffle(a, 1, 3, 2, 2); }
inline F32x4 Vec_ywzw(F32x4 a) { return _shuffle(a, 1, 3, 2, 3); }
inline F32x4 Vec_ywwx(F32x4 a) { return _shuffle(a, 1, 3, 3, 0); }
inline F32x4 Vec_ywwy(F32x4 a) { return _shuffle(a, 1, 3, 3, 1); }
inline F32x4 Vec_ywwz(F32x4 a) { return _shuffle(a, 1, 3, 3, 2); }
inline F32x4 Vec_ywww(F32x4 a) { return _shuffle(a, 1, 3, 3, 3); }

inline F32x4 Vec_zxxx(F32x4 a) { return _shuffle(a, 2, 0, 0, 0); }
inline F32x4 Vec_zxxy(F32x4 a) { return _shuffle(a, 2, 0, 0, 1); }
inline F32x4 Vec_zxxz(F32x4 a) { return _shuffle(a, 2, 0, 0, 2); }
inline F32x4 Vec_zxxw(F32x4 a) { return _shuffle(a, 2, 0, 0, 3); }
inline F32x4 Vec_zxyx(F32x4 a) { return _shuffle(a, 2, 0, 1, 0); }
inline F32x4 Vec_zxyy(F32x4 a) { return _shuffle(a, 2, 0, 1, 1); }
inline F32x4 Vec_zxyz(F32x4 a) { return _shuffle(a, 2, 0, 1, 2); }
inline F32x4 Vec_zxyw(F32x4 a) { return _shuffle(a, 2, 0, 1, 3); }
inline F32x4 Vec_zxzx(F32x4 a) { return _shuffle(a, 2, 0, 2, 0); }
inline F32x4 Vec_zxzy(F32x4 a) { return _shuffle(a, 2, 0, 2, 1); }
inline F32x4 Vec_zxzz(F32x4 a) { return _shuffle(a, 2, 0, 2, 2); }
inline F32x4 Vec_zxzw(F32x4 a) { return _shuffle(a, 2, 0, 2, 3); }
inline F32x4 Vec_zxwx(F32x4 a) { return _shuffle(a, 2, 0, 3, 0); }
inline F32x4 Vec_zxwy(F32x4 a) { return _shuffle(a, 2, 0, 3, 1); }
inline F32x4 Vec_zxwz(F32x4 a) { return _shuffle(a, 2, 0, 3, 2); }
inline F32x4 Vec_zxww(F32x4 a) { return _shuffle(a, 2, 0, 3, 3); }

inline F32x4 Vec_zyxx(F32x4 a) { return _shuffle(a, 2, 1, 0, 0); }
inline F32x4 Vec_zyxy(F32x4 a) { return _shuffle(a, 2, 1, 0, 1); }
inline F32x4 Vec_zyxz(F32x4 a) { return _shuffle(a, 2, 1, 0, 2); }
inline F32x4 Vec_zyxw(F32x4 a) { return _shuffle(a, 2, 1, 0, 3); }
inline F32x4 Vec_zyyx(F32x4 a) { return _shuffle(a, 2, 1, 1, 0); }
inline F32x4 Vec_zyyy(F32x4 a) { return _shuffle(a, 2, 1, 1, 1); }
inline F32x4 Vec_zyyz(F32x4 a) { return _shuffle(a, 2, 1, 1, 2); }
inline F32x4 Vec_zyyw(F32x4 a) { return _shuffle(a, 2, 1, 1, 3); }
inline F32x4 Vec_zyzx(F32x4 a) { return _shuffle(a, 2, 1, 2, 0); }
inline F32x4 Vec_zyzy(F32x4 a) { return _shuffle(a, 2, 1, 2, 1); }
inline F32x4 Vec_zyzz(F32x4 a) { return _shuffle(a, 2, 1, 2, 2); }
inline F32x4 Vec_zyzw(F32x4 a) { return _shuffle(a, 2, 1, 2, 3); }
inline F32x4 Vec_zywx(F32x4 a) { return _shuffle(a, 2, 1, 3, 0); }
inline F32x4 Vec_zywy(F32x4 a) { return _shuffle(a, 2, 1, 3, 1); }
inline F32x4 Vec_zywz(F32x4 a) { return _shuffle(a, 2, 1, 3, 2); }
inline F32x4 Vec_zyww(F32x4 a) { return _shuffle(a, 2, 1, 3, 3); }

inline F32x4 Vec_zzxx(F32x4 a) { return _shuffle(a, 2, 2, 0, 0); }
inline F32x4 Vec_zzxy(F32x4 a) { return _shuffle(a, 2, 2, 0, 1); }
inline F32x4 Vec_zzxz(F32x4 a) { return _shuffle(a, 2, 2, 0, 2); }
inline F32x4 Vec_zzxw(F32x4 a) { return _shuffle(a, 2, 2, 0, 3); }
inline F32x4 Vec_zzyx(F32x4 a) { return _shuffle(a, 2, 2, 1, 0); }
inline F32x4 Vec_zzyy(F32x4 a) { return _shuffle(a, 2, 2, 1, 1); }
inline F32x4 Vec_zzyz(F32x4 a) { return _shuffle(a, 2, 2, 1, 2); }
inline F32x4 Vec_zzyw(F32x4 a) { return _shuffle(a, 2, 2, 1, 3); }
inline F32x4 Vec_zzzx(F32x4 a) { return _shuffle(a, 2, 2, 2, 0); }
inline F32x4 Vec_zzzy(F32x4 a) { return _shuffle(a, 2, 2, 2, 1); }
inline F32x4 Vec_zzzz(F32x4 a) { return _shuffle1(a, 2); }
inline F32x4 Vec_zzzw(F32x4 a) { return _shuffle(a, 2, 2, 2, 3); }
inline F32x4 Vec_zzwx(F32x4 a) { return _shuffle(a, 2, 2, 3, 0); }
inline F32x4 Vec_zzwy(F32x4 a) { return _shuffle(a, 2, 2, 3, 1); }
inline F32x4 Vec_zzwz(F32x4 a) { return _shuffle(a, 2, 2, 3, 2); }
inline F32x4 Vec_zzww(F32x4 a) { return _shuffle(a, 2, 2, 3, 3); }

inline F32x4 Vec_zwxx(F32x4 a) { return _shuffle(a, 2, 3, 0, 0); }
inline F32x4 Vec_zwxy(F32x4 a) { return _shuffle(a, 2, 3, 0, 1); }
inline F32x4 Vec_zwxz(F32x4 a) { return _shuffle(a, 2, 3, 0, 2); }
inline F32x4 Vec_zwxw(F32x4 a) { return _shuffle(a, 2, 3, 0, 3); }
inline F32x4 Vec_zwyx(F32x4 a) { return _shuffle(a, 2, 3, 1, 0); }
inline F32x4 Vec_zwyy(F32x4 a) { return _shuffle(a, 2, 3, 1, 1); }
inline F32x4 Vec_zwyz(F32x4 a) { return _shuffle(a, 2, 3, 1, 2); }
inline F32x4 Vec_zwyw(F32x4 a) { return _shuffle(a, 2, 3, 1, 3); }
inline F32x4 Vec_zwzx(F32x4 a) { return _shuffle(a, 2, 3, 2, 0); }
inline F32x4 Vec_zwzy(F32x4 a) { return _shuffle(a, 2, 3, 2, 1); }
inline F32x4 Vec_zwzz(F32x4 a) { return _shuffle(a, 2, 3, 2, 2); }
inline F32x4 Vec_zwzw(F32x4 a) { return _shuffle(a, 2, 3, 2, 3); }
inline F32x4 Vec_zwwx(F32x4 a) { return _shuffle(a, 2, 3, 3, 0); }
inline F32x4 Vec_zwwy(F32x4 a) { return _shuffle(a, 2, 3, 3, 1); }
inline F32x4 Vec_zwwz(F32x4 a) { return _shuffle(a, 2, 3, 3, 2); }
inline F32x4 Vec_zwww(F32x4 a) { return _shuffle(a, 2, 3, 3, 3); }

inline F32x4 Vec_wxxx(F32x4 a) { return _shuffle(a, 3, 0, 0, 0); }
inline F32x4 Vec_wxxy(F32x4 a) { return _shuffle(a, 3, 0, 0, 1); }
inline F32x4 Vec_wxxz(F32x4 a) { return _shuffle(a, 3, 0, 0, 2); }
inline F32x4 Vec_wxxw(F32x4 a) { return _shuffle(a, 3, 0, 0, 3); }
inline F32x4 Vec_wxyx(F32x4 a) { return _shuffle(a, 3, 0, 1, 0); }
inline F32x4 Vec_wxyy(F32x4 a) { return _shuffle(a, 3, 0, 1, 1); }
inline F32x4 Vec_wxyz(F32x4 a) { return _shuffle(a, 3, 0, 1, 2); }
inline F32x4 Vec_wxyw(F32x4 a) { return _shuffle(a, 3, 0, 1, 3); }
inline F32x4 Vec_wxzx(F32x4 a) { return _shuffle(a, 3, 0, 2, 0); }
inline F32x4 Vec_wxzy(F32x4 a) { return _shuffle(a, 3, 0, 2, 1); }
inline F32x4 Vec_wxzz(F32x4 a) { return _shuffle(a, 3, 0, 2, 2); }
inline F32x4 Vec_wxzw(F32x4 a) { return _shuffle(a, 3, 0, 2, 3); }
inline F32x4 Vec_wxwx(F32x4 a) { return _shuffle(a, 3, 0, 3, 0); }
inline F32x4 Vec_wxwy(F32x4 a) { return _shuffle(a, 3, 0, 3, 1); }
inline F32x4 Vec_wxwz(F32x4 a) { return _shuffle(a, 3, 0, 3, 2); }
inline F32x4 Vec_wxww(F32x4 a) { return _shuffle(a, 3, 0, 3, 3); }

inline F32x4 Vec_wyxx(F32x4 a) { return _shuffle(a, 3, 1, 0, 0); }
inline F32x4 Vec_wyxy(F32x4 a) { return _shuffle(a, 3, 1, 0, 1); }
inline F32x4 Vec_wyxz(F32x4 a) { return _shuffle(a, 3, 1, 0, 2); }
inline F32x4 Vec_wyxw(F32x4 a) { return _shuffle(a, 3, 1, 0, 3); }
inline F32x4 Vec_wyyx(F32x4 a) { return _shuffle(a, 3, 1, 1, 0); }
inline F32x4 Vec_wyyy(F32x4 a) { return _shuffle(a, 3, 1, 1, 1); }
inline F32x4 Vec_wyyz(F32x4 a) { return _shuffle(a, 3, 1, 1, 2); }
inline F32x4 Vec_wyyw(F32x4 a) { return _shuffle(a, 3, 1, 1, 3); }
inline F32x4 Vec_wyzx(F32x4 a) { return _shuffle(a, 3, 1, 2, 0); }
inline F32x4 Vec_wyzy(F32x4 a) { return _shuffle(a, 3, 1, 2, 1); }
inline F32x4 Vec_wyzz(F32x4 a) { return _shuffle(a, 3, 1, 2, 2); }
inline F32x4 Vec_wyzw(F32x4 a) { return _shuffle(a, 3, 1, 2, 3); }
inline F32x4 Vec_wywx(F32x4 a) { return _shuffle(a, 3, 1, 3, 0); }
inline F32x4 Vec_wywy(F32x4 a) { return _shuffle(a, 3, 1, 3, 1); }
inline F32x4 Vec_wywz(F32x4 a) { return _shuffle(a, 3, 1, 3, 2); }
inline F32x4 Vec_wyww(F32x4 a) { return _shuffle(a, 3, 1, 3, 3); }

inline F32x4 Vec_wzxx(F32x4 a) { return _shuffle(a, 3, 2, 0, 0); }
inline F32x4 Vec_wzxy(F32x4 a) { return _shuffle(a, 3, 2, 0, 1); }
inline F32x4 Vec_wzxz(F32x4 a) { return _shuffle(a, 3, 2, 0, 2); }
inline F32x4 Vec_wzxw(F32x4 a) { return _shuffle(a, 3, 2, 0, 3); }
inline F32x4 Vec_wzyx(F32x4 a) { return _shuffle(a, 3, 2, 1, 0); }
inline F32x4 Vec_wzyy(F32x4 a) { return _shuffle(a, 3, 2, 1, 1); }
inline F32x4 Vec_wzyz(F32x4 a) { return _shuffle(a, 3, 2, 1, 2); }
inline F32x4 Vec_wzyw(F32x4 a) { return _shuffle(a, 3, 2, 1, 3); }
inline F32x4 Vec_wzzx(F32x4 a) { return _shuffle(a, 3, 2, 2, 0); }
inline F32x4 Vec_wzzy(F32x4 a) { return _shuffle(a, 3, 2, 2, 1); }
inline F32x4 Vec_wzzz(F32x4 a) { return _shuffle(a, 3, 2, 2, 2); }
inline F32x4 Vec_wzzw(F32x4 a) { return _shuffle(a, 3, 2, 2, 3); }
inline F32x4 Vec_wzwx(F32x4 a) { return _shuffle(a, 3, 2, 3, 0); }
inline F32x4 Vec_wzwy(F32x4 a) { return _shuffle(a, 3, 2, 3, 1); }
inline F32x4 Vec_wzwz(F32x4 a) { return _shuffle(a, 3, 2, 3, 2); }
inline F32x4 Vec_wzww(F32x4 a) { return _shuffle(a, 3, 2, 3, 3); }

inline F32x4 Vec_wwxx(F32x4 a) { return _shuffle(a, 3, 3, 0, 0); }
inline F32x4 Vec_wwxy(F32x4 a) { return _shuffle(a, 3, 3, 0, 1); }
inline F32x4 Vec_wwxz(F32x4 a) { return _shuffle(a, 3, 3, 0, 2); }
inline F32x4 Vec_wwxw(F32x4 a) { return _shuffle(a, 3, 3, 0, 3); }
inline F32x4 Vec_wwyx(F32x4 a) { return _shuffle(a, 3, 3, 1, 0); }
inline F32x4 Vec_wwyy(F32x4 a) { return _shuffle(a, 3, 3, 1, 1); }
inline F32x4 Vec_wwyz(F32x4 a) { return _shuffle(a, 3, 3, 1, 2); }
inline F32x4 Vec_wwyw(F32x4 a) { return _shuffle(a, 3, 3, 1, 3); }
inline F32x4 Vec_wwzx(F32x4 a) { return _shuffle(a, 3, 3, 2, 0); }
inline F32x4 Vec_wwzy(F32x4 a) { return _shuffle(a, 3, 3, 2, 1); }
inline F32x4 Vec_wwzz(F32x4 a) { return _shuffle(a, 3, 3, 2, 2); }
inline F32x4 Vec_wwzw(F32x4 a) { return _shuffle(a, 3, 3, 2, 3); }
inline F32x4 Vec_wwwx(F32x4 a) { return _shuffle(a, 3, 3, 3, 0); }
inline F32x4 Vec_wwwy(F32x4 a) { return _shuffle(a, 3, 3, 3, 1); }
inline F32x4 Vec_wwwz(F32x4 a) { return _shuffle(a, 3, 3, 3, 2); }
inline F32x4 Vec_wwww(F32x4 a) { return _shuffle1(a, 3); }

inline F32x4 Vec_trunc2(F32x4 a) { return _mm_movelh_ps(a, Vec_zero()); }
inline F32x4 Vec_trunc3(F32x4 a) { 
    F32x4 z0 = Vec_xzzz(_mm_movelh_ps(Vec_zzzz(a), Vec_zero()));
    return _mm_movelh_ps(a, z0); 
}

//2D swizzles

inline F32x4 Vec_xx(F32x4 a) { return Vec_trunc2(Vec_xxxx(a)); }
inline F32x4 Vec_xy(F32x4 a) { return Vec_trunc2(Vec_xyxx(a)); }
inline F32x4 Vec_xz(F32x4 a) { return Vec_trunc2(Vec_xzxx(a)); }
inline F32x4 Vec_xw(F32x4 a) { return Vec_trunc2(Vec_xwxx(a)); }

inline F32x4 Vec_yx(F32x4 a) { return Vec_trunc2(Vec_yxxx(a)); }
inline F32x4 Vec_yy(F32x4 a) { return Vec_trunc2(Vec_yyxx(a)); }
inline F32x4 Vec_yz(F32x4 a) { return Vec_trunc2(Vec_yzxx(a)); }
inline F32x4 Vec_yw(F32x4 a) { return Vec_trunc2(Vec_ywxx(a)); }

inline F32x4 Vec_zx(F32x4 a) { return Vec_trunc2(Vec_zxxx(a)); }
inline F32x4 Vec_zy(F32x4 a) { return Vec_trunc2(Vec_zyxx(a)); }
inline F32x4 Vec_zz(F32x4 a) { return Vec_trunc2(Vec_zzxx(a)); }
inline F32x4 Vec_zw(F32x4 a) { return Vec_trunc2(Vec_zwxx(a)); }

inline F32x4 Vec_wx(F32x4 a) { return Vec_trunc2(Vec_wxxx(a)); }
inline F32x4 Vec_wy(F32x4 a) { return Vec_trunc2(Vec_wyxx(a)); }
inline F32x4 Vec_wz(F32x4 a) { return Vec_trunc2(Vec_wzxx(a)); }
inline F32x4 Vec_ww(F32x4 a) { return Vec_trunc2(Vec_wwxx(a)); }

//3D swizzles

inline F32x4 Vec_xxx(F32x4 a) { return Vec_trunc3(Vec_xxxx(a)); }
inline F32x4 Vec_xxy(F32x4 a) { return Vec_trunc3(Vec_xxyx(a)); }
inline F32x4 Vec_xxz(F32x4 a) { return Vec_trunc3(Vec_xxzx(a)); }
inline F32x4 Vec_xyx(F32x4 a) { return Vec_trunc3(Vec_xyxx(a)); }
inline F32x4 Vec_xyy(F32x4 a) { return Vec_trunc3(Vec_xyyx(a)); }
inline F32x4 Vec_xyz(F32x4 a) { return Vec_trunc3(Vec_xyzx(a)); }
inline F32x4 Vec_xzx(F32x4 a) { return Vec_trunc3(Vec_xzxx(a)); }
inline F32x4 Vec_xzy(F32x4 a) { return Vec_trunc3(Vec_xzyx(a)); }
inline F32x4 Vec_xzz(F32x4 a) { return Vec_trunc3(Vec_xzzx(a)); }

inline F32x4 Vec_yxx(F32x4 a) { return Vec_trunc3(Vec_yxxx(a)); }
inline F32x4 Vec_yxy(F32x4 a) { return Vec_trunc3(Vec_yxyx(a)); }
inline F32x4 Vec_yxz(F32x4 a) { return Vec_trunc3(Vec_yxzx(a)); }
inline F32x4 Vec_yyx(F32x4 a) { return Vec_trunc3(Vec_yyxx(a)); }
inline F32x4 Vec_yyy(F32x4 a) { return Vec_trunc3(Vec_yyyx(a)); }
inline F32x4 Vec_yyz(F32x4 a) { return Vec_trunc3(Vec_yyzx(a)); }
inline F32x4 Vec_yzx(F32x4 a) { return Vec_trunc3(Vec_yzxx(a)); }
inline F32x4 Vec_yzy(F32x4 a) { return Vec_trunc3(Vec_yzyx(a)); }
inline F32x4 Vec_yzz(F32x4 a) { return Vec_trunc3(Vec_yzzx(a)); }

inline F32x4 Vec_zxx(F32x4 a) { return Vec_trunc3(Vec_zxxx(a)); }
inline F32x4 Vec_zxy(F32x4 a) { return Vec_trunc3(Vec_zxyx(a)); }
inline F32x4 Vec_zxz(F32x4 a) { return Vec_trunc3(Vec_zxzx(a)); }
inline F32x4 Vec_zyx(F32x4 a) { return Vec_trunc3(Vec_zyxx(a)); }
inline F32x4 Vec_zyy(F32x4 a) { return Vec_trunc3(Vec_zyyx(a)); }
inline F32x4 Vec_zyz(F32x4 a) { return Vec_trunc3(Vec_zyzx(a)); }
inline F32x4 Vec_zzx(F32x4 a) { return Vec_trunc3(Vec_zzxx(a)); }
inline F32x4 Vec_zzy(F32x4 a) { return Vec_trunc3(Vec_zzyx(a)); }
inline F32x4 Vec_zzz(F32x4 a) { return Vec_trunc3(Vec_zzzx(a)); }

inline F32 Vec_x(F32x4 a) { return _mm_cvtss_f32(a); }
inline F32 Vec_y(F32x4 a) { return Vec_x(Vec_yyyy(a)); }
inline F32 Vec_z(F32x4 a) { return Vec_x(Vec_zzzz(a)); }
inline F32 Vec_w(F32x4 a) { return Vec_x(Vec_wwww(a)); }

inline F32x4 Vec_create1(F32 x) { return _mm_set_ps(0, 0, 0, x); }
inline F32x4 Vec_create2(F32 x, F32 y) { return _mm_set_ps(0, 0, y, x); }
inline F32x4 Vec_create3(F32 x, F32 y, F32 z) { return _mm_set_ps(0, z, y, x); }
inline F32x4 Vec_create4(F32 x, F32 y, F32 z, F32 w) { return _mm_set_ps(w, z, y, x); }

inline F32x4 Vec_xxxx4(F32 x) { return _mm_set1_ps(x); }

inline F32x4 Vec_load4(const F32 *arr) { return _mm_load_ps(arr);}

inline F32x4 Vec_zero() { return _mm_setzero_ps(); }

//Needed since cmp returns -1 as int, so we convert to -1.f and then negate

inline F32x4 _cvtitof(F32x4 a) { return _mm_cvtepI32_ps(*(__m128i*) &a); }
inline F32x4 _Vec_negatei(F32x4 a) { return Vec_negate(_cvtitof(a)); }

inline F32x4 Vec_eq(F32x4 a, F32x4 b)  { return _Vec_negatei(_mm_cmpeq_ps(a, b)); }
inline F32x4 Vec_neq(F32x4 a, F32x4 b) { return _Vec_negatei(_mm_cmpneq_ps(a, b)); }
inline F32x4 Vec_geq(F32x4 a, F32x4 b) { return _Vec_negatei(_mm_cmpge_ps(a, b)); }
inline F32x4 Vec_gt(F32x4 a, F32x4 b)  { return _Vec_negatei(_mm_cmpgt_ps(a, b)); }
inline F32x4 Vec_leq(F32x4 a, F32x4 b) { return _Vec_negatei(_mm_cmple_ps(a, b)); }
inline F32x4 Vec_lt(F32x4 a, F32x4 b)  { return _Vec_negatei(_mm_cmplt_ps(a, b)); }

inline F32x4 Vec_or(F32x4 a, F32x4 b)  { return _cvtitof(_mm_or_ps(a, b)); }
inline F32x4 Vec_and(F32x4 a, F32x4 b) { return _cvtitof(_mm_and_ps(a, b)); }
inline F32x4 Vec_xor(F32x4 a, F32x4 b) { return _cvtitof(_mm_xor_ps(a, b)); }

inline F32x4 Vec_min(F32x4 a, F32x4 b) { return _mm_min_ps(a, b); }
inline F32x4 Vec_max(F32x4 a, F32x4 b) { return _mm_max_ps(a, b); }

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0
//
inline F32x4 Vec_sign(F32x4 v) {
    return Vec_add(
        Vec_mul(_mm_cmplt_ps(v, Vec_zero()), Vec_two()), 
        Vec_one()
    ); 
}

inline F32x4 Vec_cross3(F32x4 a, F32x4 b) {
    return Vec_normalize3(
        Vec_sub(
            Vec_mul(Vec_yzx(a), Vec_zxy(b)),
            Vec_mul(Vec_zxy(a), Vec_yzx(b))
        )
    );
}

inline F32x4 Vec_abs(F32x4 v){ return Vec_mul(Vec_sign(v), v); }

inline F32 Vec_reduce(F32x4 a) {
    return Vec_x(_mm_hadd_ps(_mm_hadd_ps(a, Vec_zero()), Vec_zero()));
}

inline F32 Vec_dot4(F32x4 a, F32x4 b) { return Vec_x(_mm_dp_ps(a, b, 0xFF)); }
inline F32 Vec_dot2(F32x4 a, F32x4 b) { return Vec_x(_mm_dp_ps(Vec_xy(a), Vec_xy(b), 0xFF)); }
inline F32 Vec_dot3(F32x4 a, F32x4 b) { return Vec_x(_mm_dp_ps(Vec_xyz(a), Vec_xyz(b), 0xFF)); }

inline F32 Vec_sqLen2(F32x4 v) { v = Vec_xy(v);  return Vec_x(_mm_dp_ps(v, v, 0xFF)); }
inline F32 Vec_sqLen3(F32x4 v) { v = Vec_xyz(v); return Vec_x(_mm_dp_ps(v, v, 0xFF)); }
inline F32 Vec_sqLen4(F32x4 v) { return Vec_x(_mm_dp_ps(v, v, 0xFF)); }

//Arch independent

//Shifts are hard, so we just shift by division and floor
//
inline F32x4 Vec_rgb8Unpack(U32 v) {
    F32x4 rgb8 = Vec_floor(Vec_div(Vec_xxxx4((F32)v), Vec_create3(0x10000, 0x100, 0x1)));
    return Vec_div(Vec_mod(rgb8, Vec_xxxx4(0x100)), Vec_xxxx4(0xFF));
}

inline U32 Vec_rgb8Pack(F32x4 v) {
    F32x4 v8 = Vec_floor(Vec_mul(v, Vec_xxxx4(0xFF)));
    F32x4 preShift = Vec_trunc3(Vec_mul(v8, Vec_create3(0x10000, 0x100, 0x1)));
    return (U32) Vec_reduce(preShift);
}

inline F32x4 Vec_srgba8Unpack(U32 v) {
    return Vec_pow2(Vec_rgb8Unpack(v << 8 >> 8));
}

inline U32 Vec_srgba8Pack(F32x4 v) {
    return Vec_rgb8Pack(Vec_sqrt(v)) | 0xFF000000;
}

inline F32x4 Vec_one() { return Vec_xxxx4(1); }
inline F32x4 Vec_two() { return Vec_xxxx4(2); }
inline F32x4 Vec_mask2() { return Vec_create4(1, 1, 0, 0); }
inline F32x4 Vec_mask3() { return Vec_create4(1, 1, 1, 0); }

inline Bool Vec_all(F32x4 b) { return Vec_reduce(Vec_neq(b, Vec_zero())) == 4; }
inline Bool Vec_any(F32x4 b) { return Vec_reduce(Vec_neq(b, Vec_zero())); }

inline F32x4 Vec_normalize2(F32x4 v) { v = Vec_xy(v);  return Vec_mul(v, Vec_rsqrt(_mm_dp_ps(v, v, 0xFF))); }
inline F32x4 Vec_normalize3(F32x4 v) { v = Vec_xyz(v); return Vec_mul(v, Vec_rsqrt(_mm_dp_ps(v, v, 0xFF))); }
inline F32x4 Vec_normalize4(F32x4 v) { return Vec_mul(v, Vec_rsqrt(_mm_dp_ps(v, v, 0xFF))); }

inline F32x4 Vec_load1(const F32 *arr) { return Vec_create1(*arr); }
inline F32x4 Vec_load2(const F32 *arr) { return Vec_create2(*arr, arr[1]); }
inline F32x4 Vec_load3(const F32 *arr) { return Vec_create3(*arr, arr[1], arr[2]); }

inline void Vec_setX(F32x4 *a, F32 v) { *a = Vec_add(Vec_mul(*a, Vec_create4(0, 1, 1, 1)), Vec_create4(v, 0, 0, 0)); }
inline void Vec_setY(F32x4 *a, F32 v) { *a = Vec_add(Vec_mul(*a, Vec_create4(1, 0, 1, 1)), Vec_create4(0, v, 0, 0)); }
inline void Vec_setZ(F32x4 *a, F32 v) { *a = Vec_add(Vec_mul(*a, Vec_create4(1, 1, 0, 1)), Vec_create4(0, 0, v, 0)); }
inline void Vec_setW(F32x4 *a, F32 v) { *a = Vec_add(Vec_mul(*a, Vec_create4(1, 1, 1, 0)), Vec_create4(0, 0, 0, v)); }

inline void Vec_set(F32x4 *a, U8 i, F32 v) {
    switch (i & 3) {
		case 0:     Vec_setX(a, v);     break;
		case 1:     Vec_setY(a, v);     break;
		case 2:     Vec_setZ(a, v);     break;
		default:    Vec_setW(a, v);
    }
}

inline F32 Vec_get(F32x4 a, U8 i) {
    switch (i & 3) {
		case 0:     return Vec_x(a);
		case 1:     return Vec_y(a);
		case 2:     return Vec_z(a);
		default:    return Vec_w(a);
    }
}