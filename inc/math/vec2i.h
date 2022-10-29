#pragma once

//TODO: Error checking

inline F32x2 F32x2_bitsI32x2(I32x2 a) { return *(const F32x2*) &a; }          //Convert raw bits to data type
inline I32x2 I32x2_bitsF32x2(F32x2 a) { return *(const I32x2*) &a; }          //Convert raw bits to data type

//Arithmetic

inline I32x2 I32x2_zero();
inline I32x2 I32x2_one();
inline I32x2 I32x2_two();
inline I32x2 I32x2_negTwo();
inline I32x2 I32x2_negOne();
inline I32x2 I32x2_xx2(I32 x);

inline I32x2 I32x2_add(I32x2 a, I32x2 b);
inline I32x2 I32x2_sub(I32x2 a, I32x2 b);
inline I32x2 I32x2_mul(I32x2 a, I32x2 b);
inline I32x2 I32x2_div(I32x2 a, I32x2 b);

inline I32x2 I32x2_complement(I32x2 a) { return I32x2_sub(I32x2_one(), a); }
inline I32x2 I32x2_negate(I32x2 a) { return I32x2_sub(I32x2_zero(), a); }

inline I32x2 I32x2_pow2(I32x2 a) { return I32x2_mul(a, a); }

inline I32x2 I32x2_sign(I32x2 v);     //Zero counts as signed too
inline I32x2 I32x2_abs(I32x2 v);
inline I32x2 I32x2_mod(I32x2 v, I32x2 d) { return I32x2_sub(v, I32x2_mul(I32x2_div(v, d), d)); }

inline I32 I32x2_reduce(I32x2 a);     //ax+ay

inline I32x2 I32x2_min(I32x2 a, I32x2 b);
inline I32x2 I32x2_max(I32x2 a, I32x2 b);
inline I32x2 I32x2_clamp(I32x2 a, I32x2 mi, I32x2 ma) { return I32x2_max(mi, I32x2_min(ma, a)); }

//Boolean / bitwise

inline Bool I32x2_all(I32x2 a);
inline Bool I32x2_any(I32x2 b);

inline I32x2 I32x2_eq(I32x2 a, I32x2 b);
inline I32x2 I32x2_neq(I32x2 a, I32x2 b);
inline I32x2 I32x2_geq(I32x2 a, I32x2 b);
inline I32x2 I32x2_gt(I32x2 a, I32x2 b);
inline I32x2 I32x2_leq(I32x2 a, I32x2 b);
inline I32x2 I32x2_lt(I32x2 a, I32x2 b);

inline I32x2 I32x2_or(I32x2 a, I32x2 b);
inline I32x2 I32x2_and(I32x2 a, I32x2 b);
inline I32x2 I32x2_xor(I32x2 a, I32x2 b);

inline Bool I32x2_eq2(I32x2 a, I32x2 b) { return I32x2_all(I32x2_eq(a, b)); }
inline Bool I32x2_neq2(I32x2 a, I32x2 b) { return !I32x2_eq2(a, b); }

//Swizzles and shizzle

inline I32 I32x2_x(I32x2 a);
inline I32 I32x2_y(I32x2 a);
inline I32 I32x2_get(I32x2 a, U8 i);

inline void I32x2_setX(I32x2 *a, I32 v);
inline void I32x2_setY(I32x2 *a, I32 v);
inline void I32x2_set(I32x2 *a, U8 i, I32 v);

//Construction

inline I32x2 I32x2_create1(I32 x);
inline I32x2 I32x2_create2(I32 x, I32 y);

inline I32x2 I32x2_load1(const I32 *arr);
inline I32x2 I32x2_load2(const I32 *arr);

inline I32x2 I32x2_xx(I32x2 a);
inline I32x2 I32x2_xy(I32x2 a) { return a; }
inline I32x2 I32x2_yx(I32x2 a);
inline I32x2 I32x2_yy(I32x2 a);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline I32x2 I32x2_sign(I32x2 v) {
    return I32x2_add(
        I32x2_mul(I32x2_lt(v, I32x2_zero()), I32x2_two()), 
        I32x2_one()
    );
}

inline I32x2 I32x2_abs(I32x2 v){ return I32x2_mul(I32x2_sign(v), v); }

//Simple defcreateions

inline I32x2 I32x2_one() { return I32x2_xx2(1); }
inline I32x2 I32x2_two() { return I32x2_xx2(2); }
inline I32x2 I32x2_negOne() { return I32x2_xx2(-1); }
inline I32x2 I32x2_negTwo() { return I32x2_xx2(-2); }

inline Bool I32x2_all(I32x2 b) { return I32x2_reduce(I32x2_neq(b, I32x2_zero())) == 4; }
inline Bool I32x2_any(I32x2 b) { return I32x2_reduce(I32x2_neq(b, I32x2_zero())); }

inline I32x2 I32x2_load1(const I32 *arr) { return I32x2_create1(*arr); }
inline I32x2 I32x2_load2(const I32 *arr) { return I32x2_create2(*arr, arr[1]); }

inline void I32x2_setX(I32x2 *a, I32 v) { *a = I32x2_create2(v,             I32x2_y(*a)); }
inline void I32x2_setY(I32x2 *a, I32 v) { *a = I32x2_create2(I32x2_x(*a),   v); }

inline void I32x2_set(I32x2 *a, U8 i, I32 v) {
    switch (i & 1) {
        case 0:      I32x2_setX(a, v);     break;
        default:     I32x2_setY(a, v);     break;
    }
}

inline I32 I32x2_get(I32x2 a, U8 i) {
    switch (i & 1) {
        case 0:      return I32x2_x(a);
        default:     return I32x2_y(a);
    }
}

//Casts from vec4i

inline I32x2 I32x2_fromI32x4(I32x4 a) { return I32x2_load2((const I32*) &a); }
inline I32x4 I32x4_fromI32x2(I32x2 a) { return I32x4_load2((const I32*) &a); }

inline I32x2 I32x4_xx(I32x4 a) { return I32x2_fromI32x4(I32x4_xxxx(a)); }
inline I32x2 I32x4_xy(I32x4 a) { return I32x2_fromI32x4(a); }
inline I32x2 I32x4_xz(I32x4 a) { return I32x2_fromI32x4(I32x4_xzxx(a)); }
inline I32x2 I32x4_xw(I32x4 a) { return I32x2_fromI32x4(I32x4_xwxx(a)); }

inline I32x2 I32x4_yx(I32x4 a) { return I32x2_fromI32x4(I32x4_yxxx(a)); }
inline I32x2 I32x4_yy(I32x4 a) { return I32x2_fromI32x4(I32x4_yyxx(a)); }
inline I32x2 I32x4_yz(I32x4 a) { return I32x2_fromI32x4(I32x4_yzxx(a)); }
inline I32x2 I32x4_yw(I32x4 a) { return I32x2_fromI32x4(I32x4_ywxx(a)); }

inline I32x2 I32x4_zx(I32x4 a) { return I32x2_fromI32x4(I32x4_zxxx(a)); }
inline I32x2 I32x4_zy(I32x4 a) { return I32x2_fromI32x4(I32x4_zyxx(a)); }
inline I32x2 I32x4_zz(I32x4 a) { return I32x2_fromI32x4(I32x4_zzxx(a)); }
inline I32x2 I32x4_zw(I32x4 a) { return I32x2_fromI32x4(I32x4_zwxx(a)); }

inline I32x2 I32x4_wx(I32x4 a) { return I32x2_fromI32x4(I32x4_wxxx(a)); }
inline I32x2 I32x4_wy(I32x4 a) { return I32x2_fromI32x4(I32x4_wyxx(a)); }
inline I32x2 I32x4_wz(I32x4 a) { return I32x2_fromI32x4(I32x4_wzxx(a)); }
inline I32x2 I32x4_ww(I32x4 a) { return I32x2_fromI32x4(I32x4_wwxx(a)); }

//Cast from vec2f to vec4

inline I32x4 I32x4_create2_2(I32x2 a, I32x2 b) { return I32x4_create4(I32x2_x(a), I32x2_y(a), I32x2_x(b), I32x2_y(b)); }

inline I32x4 I32x4_create2_1_1(I32x2 a, I32 b, I32 c) { return I32x4_create4(I32x2_x(a), I32x2_y(a), b, c); }
inline I32x4 I32x4_create1_2_1(I32 a, I32x2 b, I32 c) { return I32x4_create4(a, I32x2_x(b), I32x2_y(b), c); }
inline I32x4 I32x4_create1_1_2(I32 a, I32 b, I32x2 c) { return I32x4_create4(a, b, I32x2_x(c), I32x2_y(c)); }


inline I32x4 I32x4_create2_1(I32x2 a, I32 b) { return I32x4_create3(I32x2_x(a), I32x2_y(a), b); }
inline I32x4 I32x4_create1_2(I32 a, I32x2 b) { return I32x4_create3(a, I32x2_x(b), I32x2_y(b)); }
