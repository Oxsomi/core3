#pragma once

inline f32x2 f32x2_bitsI32x2(i32x2 a) { return *(const f32x2*) &a; }          //Convert raw bits to data type
inline i32x2 i32x2_bitsF32x2(f32x2 a) { return *(const i32x2*) &a; }          //Convert raw bits to data type

//Arithmetic

inline i32x2 i32x2_zero();
inline i32x2 i32x2_one();
inline i32x2 i32x2_two();
inline i32x2 i32x2_negTwo();
inline i32x2 i32x2_negOne();
inline i32x2 i32x2_xx2(i32 x);

inline i32x2 i32x2_add(i32x2 a, i32x2 b);
inline i32x2 i32x2_sub(i32x2 a, i32x2 b);
inline i32x2 i32x2_mul(i32x2 a, i32x2 b);
inline i32x2 i32x2_div(i32x2 a, i32x2 b);

inline i32x2 i32x2_complement(i32x2 a) { return i32x2_sub(i32x2_one(), a); }
inline i32x2 i32x2_negate(i32x2 a) { return i32x2_sub(i32x2_zero(), a); }
inline i32x2 i32x2_inverse(i32x2 a) { return i32x2_div(i32x2_one(), a); }

inline i32x2 i32x2_pow2(i32x2 a) { return i32x2_mul(a, a); }

inline i32x2 i32x2_sign(i32x2 v);     //Zero counts as signed too
inline i32x2 i32x2_abs(i32x2 v);
inline i32x2 i32x2_mod(i32x2 v, i32x2 d) { return i32x2_sub(v, i32x2_mul(i32x2_div(v, d), d)); }

inline i32 i32x2_reduce(i32x2 a);     //ax+ay

inline i32x2 i32x2_min(i32x2 a, i32x2 b);
inline i32x2 i32x2_max(i32x2 a, i32x2 b);
inline i32x2 i32x2_clamp(i32x2 a, i32x2 mi, i32x2 ma) { return i32x2_max(mi, i32x2_min(ma, a)); }

//Boolean / bitwise

inline bool i32x2_all(i32x2 a);
inline bool i32x2_any(i32x2 b);

inline i32x2 i32x2_eq(i32x2 a, i32x2 b);
inline i32x2 i32x2_neq(i32x2 a, i32x2 b);
inline i32x2 i32x2_geq(i32x2 a, i32x2 b);
inline i32x2 i32x2_gt(i32x2 a, i32x2 b);
inline i32x2 i32x2_leq(i32x2 a, i32x2 b);
inline i32x2 i32x2_lt(i32x2 a, i32x2 b);

inline i32x2 i32x2_or(i32x2 a, i32x2 b);
inline i32x2 i32x2_and(i32x2 a, i32x2 b);
inline i32x2 i32x2_xor(i32x2 a, i32x2 b);

inline bool i32x2_eq4(i32x2 a, i32x2 b) { return i32x2_all(i32x2_eq(a, b)); }
inline bool i32x2_neq4(i32x2 a, i32x2 b) { return !i32x2_eq4(a, b); }

//Swizzles and shizzle

inline i32 i32x2_x(i32x2 a);
inline i32 i32x2_y(i32x2 a);
inline i32 i32x2_get(i32x2 a, u8 i);

inline void i32x2_setX(i32x2 *a, i32 v);
inline void i32x2_setY(i32x2 *a, i32 v);
inline void i32x2_set(i32x2 *a, u8 i, i32 v);

//Construction

inline i32x2 i32x2_init1(i32 x);
inline i32x2 i32x2_init2(i32 x, i32 y);

inline i32x2 i32x2_load1(const i32 *arr);
inline i32x2 i32x2_load2(const i32 *arr);

inline i32x2 i32x2_xx(i32x2 a);
inline i32x2 i32x2_yx(i32x2 a);
inline i32x2 i32x2_yy(i32x2 a);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline i32x2 i32x2_sign(i32x2 v) {
    return i32x2_add(
        i32x2_mul(i32x2_lt(v, i32x2_zero()), i32x2_two()), 
        i32x2_one()
    ); 
}

inline i32x2 i32x2_abs(i32x2 v){ return i32x2_mul(i32x2_sign(v), v); }

//Simple definitions

inline i32x2 i32x2_one() { return i32x2_xx2(1); }
inline i32x2 i32x2_two() { return i32x2_xx2(2); }
inline i32x2 i32x2_negOne() { return i32x2_xx2(-1); }
inline i32x2 i32x2_negTwo() { return i32x2_xx2(-2); }

inline bool i32x2_all(i32x2 b) { return i32x2_reduce(i32x2_neq(b, i32x2_zero())) == 4; }
inline bool i32x2_any(i32x2 b) { return i32x2_reduce(i32x2_neq(b, i32x2_zero())); }

inline i32x2 i32x2_load1(const i32 *arr) { return i32x2_init1(*arr); }
inline i32x2 i32x2_load2(const i32 *arr) { return i32x2_init2(*arr, arr[1]); }

inline void i32x2_setX(i32x2 *a, i32 v) { *a = i32x2_init2(v,             i32x2_y(*a)); }
inline void i32x2_setY(i32x2 *a, i32 v) { *a = i32x2_init2(i32x2_x(*a),   v); }

inline void i32x2_set(i32x2 *a, u8 i, i32 v) {
    switch (i & 1) {
        case 0:      i32x2_setX(a, v);     break;
        default:     i32x2_setY(a, v);     break;
    }
}

inline i32 i32x2_get(i32x2 a, u8 i) {
    switch (i & 1) {
        case 0:      return i32x2_x(a);
        default:     return i32x2_y(a);
    }
}