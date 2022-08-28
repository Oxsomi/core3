#pragma once

//Int

//Arithmetic

#define _NONE_OP_T(suffix, prefix, N, T) {  \
                                            \
    T res = a;                              \
                                            \
    for (u8 i = 0; i < N; ++i)              \
        res.v[i] op b.v[i];                 \
                                            \
    return res;                             \
}

#define _NONE_OPI(N, ...) _NONE_OP_T(suffix, prefix, N, i32x4)
#define _NONE_OPF(N, ...) _NONE_OP_T(suffix, prefix, N, f32x4)

#define _NONE_OP_SELF_T(T, N, ...) {        \
                                            \
    T res = a;                              \
                                            \
    for (u8 i = 0; i < N; ++i)              \
        res.v[i] __VA_ARGS;                 \
                                            \
    return res;                             \
}

#define _NONE_OP_SELFI(N, ...) _NONE_OP_SELF_T(i32x4, N, __VA_ARGS__)
#define _NONE_OP_SELFF(N, ...) _NONE_OP_SELF_T(f32x4, N, __VA_ARGS__)

#define _NONE_OP4I(op) _NONE_OPI(op, , 4)
#define _NONE_OP4_SELFI(...) _NONE_OP_SELFI(4, __VA_ARGS__)

#define _NONE_OP4F(op) _NONE_OPF(op, , 4)
#define _NONE_OP4_SELFF(...) _NONE_OP_SELFF(4, __VA_ARGS__)

inline i32x4 i32x4_add(i32x4 a, i32x4 b) _NONE_OP4I(+=)
inline i32x4 i32x4_sub(i32x4 a, i32x4 b) _NONE_OP4I(-=)
inline i32x4 i32x4_mul(i32x4 a, i32x4 b) _NONE_OP4I(*=)
inline i32x4 i32x4_div(i32x4 a, i32x4 b) _NONE_OP4I(/=)

//Swizzle

inline i32 i32x4_x(i32x4 a) { return a.v[0]; }
inline i32 i32x4_y(i32x4 a) { return a.v[1] }
inline i32 i32x4_z(i32x4 a) { return a.v[2] }
inline i32 i32x4_w(i32x4 a) { return a.v[3] }

inline i32x4 i32x4_trunc2(i32x4 a) { return (i32x4){ { i32x4_x(a), i32x4_y(a), 0, 0 } }; }
inline i32x4 i32x4_trunc3(i32x4 a) { return (i32x4){ { i32x4_x(a), i32x4_y(a), i32x4_z(a), 0 } }; }

inline i32x4 i32x4_init1(i32 x) { return (i32x4) { { x } }; }
inline i32x4 i32x4_init2(i32 x, i32 y) { return (i32x4) { { x, y } }; }
inline i32x4 i32x4_init3(i32 x, i32 y, i32 z) { return (i32x4) { { x, y, z } }; }
inline i32x4 i32x4_init4(i32 x, i32 y, i32 z, i32 w) { return (i32x4) { { x, y, z, w } }; }

inline i32x4 i32x4_xxxx4(i32 x) { return (i32x4) { { x, x, x, x } }; }

inline i32x4 i32x4_load4(const i32 *arr) { return *(const i32x4*)&arr; }

inline i32x4 i32x4_zero() { return (i32x4){ 0 } }

//Comparison

inline i32x4 i32x4_eq(i32x4 a, i32x4 b)  _NONE_OPI(= (res.v[i] ==, ), 4)
inline i32x4 i32x4_neq(i32x4 a, i32x4 b) _NONE_OPI(= (res.v[i] !=, ), 4)
inline i32x4 i32x4_geq(i32x4 a, i32x4 b) _NONE_OPI(= (res.v[i] >=, ), 4)
inline i32x4 i32x4_gt(i32x4 a, i32x4 b)  _NONE_OPI(= (res.v[i] > , ), 4)
inline i32x4 i32x4_leq(i32x4 a, i32x4 b) _NONE_OPI(= (res.v[i] <=, ), 4)
inline i32x4 i32x4_lt(i32x4 a, i32x4 b)  _NONE_OPI(= (res.v[i] < , ), 4)

//Bitwise

inline i32x4 i32x4_or(i32x4 a, i32x4 b)  _NONE_OP4I(|=)
inline i32x4 i32x4_and(i32x4 a, i32x4 b) _NONE_OP4I(&=)
inline i32x4 i32x4_xor(i32x4 a, i32x4 b) _NONE_OP4I(^=)

inline i32x4 i32x4_min(i32x4 a, i32x4 b) _NONE_OP4_SELFI(= Math_mini(res.v[i], b.v[i]))
inline i32x4 i32x4_max(i32x4 a, i32x4 b) _NONE_OP4_SELFI(= Math_maxi(res.v[i], b.v[i]))

//Reduce

inline i32 i32x4_reduce(i32x4 a) { return i32x4_x(a) + i32x4_y(a) + i32x4_z(a) + i32x4_w(a); }

//Float

//Arithmetic

inline f32x4 f32x4_add(f32x4 a, f32x4 b) _NONE_OP4F(+=)
inline f32x4 f32x4_sub(f32x4 a, f32x4 b) _NONE_OP4F(-=)
inline f32x4 f32x4_mul(f32x4 a, f32x4 b) _NONE_OP4F(*=)
inline f32x4 f32x4_div(f32x4 a, f32x4 b) _NONE_OP4F(/=)

inline f32x4 f32x4_ceil(f32x4 a) _NONE_OP4_SELFF(= Math_ceil(res.v[i]))
inline f32x4 f32x4_floor(f32x4 a) _NONE_OP4_SELFF(= Math_floor(res.v[i]))
inline f32x4 f32x4_round(f32x4 a) _NONE_OP4_SELFF(= Math_round(res.v[i]))

inline f32x4 f32x4_sqrt(f32x4 a) _NONE_OP4_SELFF(= Math_sqrtf(res.v[i]))
inline f32x4 f32x4_rsqrt(f32x4 a) _NONE_OP4_SELFF(= 1 / Math_sqrtf(res.v[i]))

inline f32x4 f32x4_acos(f32x4 v) _NONE_OP4_SELFF(= Math_acos(res.v[i]))
inline f32x4 f32x4_cos(f32x4 v) _NONE_OP4_SELFF(= Math_cos(res.v[i]))
inline f32x4 f32x4_asin(f32x4 v) _NONE_OP4_SELFF(= Math_asin(res.v[i]))
inline f32x4 f32x4_sin(f32x4 v) _NONE_OP4_SELFF(= Math_sin(res.v[i]))
inline f32x4 f32x4_atan(f32x4 v) _NONE_OP4_SELFF(= Math_atan(res.v[i]))
inline f32x4 f32x4_atan2(f32x4 y, f32x4 x) _NONE_OP4_SELFF(= Math_atan2(res.v[i], x.v[i]))
inline f32x4 f32x4_tan(f32x4 v)  _NONE_OP4_SELFF(= Math_tan(res.v[i]))

inline f32x4 f32x4_loge(f32x4 v)  _NONE_OP4_SELFF(= Math_logef(res.v[i]))
inline f32x4 f32x4_log10(f32x4 v)  _NONE_OP4_SELFF(= Math_log10f(res.v[i]))
inline f32x4 f32x4_log2(f32x4 v)  _NONE_OP4_SELFF(= Math_log2f(res.v[i]))

inline f32x4 f32x4_exp(f32x4 v) _NONE_OP4_SELFF(= Math_expf(res.v[i]))
inline f32x4 f32x4_exp10(f32x4 v) _NONE_OP4_SELFF(= Math_exp10f(res.v[i]))
inline f32x4 f32x4_exp2(f32x4 v) _NONE_OP4_SELFF(= Math_exp2f(res.v[i]))

//Swizzle

inline f32 f32x4_x(f32x4 a) { return a.v[0]; }
inline f32 f32x4_y(f32x4 a) { return a.v[1] }
inline f32 f32x4_z(f32x4 a) { return a.v[2] }
inline f32 f32x4_w(f32x4 a) { return a.v[3] }

inline f32x4 f32x4_trunc2(f32x4 a) { return (f32x4){ { f32x4_x(a), f32x4_y(a), 0, 0 } }; }
inline f32x4 f32x4_trunc3(f32x4 a) { return (f32x4){ { f32x4_x(a), f32x4_y(a), f32x4_z(a), 0 } }; }

inline f32x4 f32x4_init1(f32 x) { return (f32x4) { { x } }; }
inline f32x4 f32x4_init2(f32 x, f32 y) { return (f32x4) { { x, y } }; }
inline f32x4 f32x4_init3(f32 x, f32 y, f32 z) { return (f32x4) { { x, y, z } }; }
inline f32x4 f32x4_init4(f32 x, f32 y, f32 z, f32 w) { return (f32x4) { { x, y, z, w } }; }

inline f32x4 f32x4_xxxx4(f32 x) { return (f32x4) { { x, x, x, x } }; }

inline f32x4 f32x4_load4(const f32 *arr) { return *(const f32x4*)&arr; }

inline f32x4 f32x4_zero() { return (f32x4){ 0 } }

//Cast

inline f32x4 f32x4_fromI32x4(i32x4 a) _NONE_OP4_SELFF(= f32(a.v[i]))
inline i32x4 i32x4_fromF32x4(f32x4 a) _NONE_OP4_SELFF(= i32(a.v[i]))

//Assign

inline f32x4 f32x4_eq(f32x4 a, f32x4 b)  _NONE_OP4_SELFF(= (a.v[i] == b.v[i]))
inline f32x4 f32x4_neq(f32x4 a, f32x4 b) _NONE_OP4_SELFF(= (a.v[i] != b.v[i]))
inline f32x4 f32x4_geq(f32x4 a, f32x4 b) _NONE_OP4_SELFF(= (a.v[i] >= b.v[i]))
inline f32x4 f32x4_gt(f32x4 a, f32x4 b)  _NONE_OP4_SELFF(= (a.v[i] >  b.v[i]))
inline f32x4 f32x4_leq(f32x4 a, f32x4 b) _NONE_OP4_SELFF(= (a.v[i] <= b.v[i]))
inline f32x4 f32x4_lt(f32x4 a, f32x4 b)  _NONE_OP4_SELFF(= (a.v[i] <  b.v[i]))

inline f32x4 f32x4_or(f32x4 a, f32x4 b)  _NONE_OP4F(^=)
inline f32x4 f32x4_and(f32x4 a, f32x4 b) _NONE_OP4F(&=)
inline f32x4 f32x4_xor(f32x4 a, f32x4 b) _NONE_OP4F(^=)

inline f32x4 f32x4_min(f32x4 a, f32x4 b) _NONE_OP4_SELFF(= Math_minf(a.v[i], b.v[i]))
inline f32x4 f32x4_max(f32x4 a, f32x4 b) _NONE_OP4_SELFF(= Math_maxf(a.v[i], b.v[i]))

//Some math operations

inline f32 f32x4_reduce(f32x4 a) { return f32x4_x(a) + f32x4_y(a) + f32x4_z(a) + f32x4_w(a); }
