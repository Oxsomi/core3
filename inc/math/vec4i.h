#pragma once

//TODO: Error checking

inline F32x4 F32x4_bitsI32x4(I32x4 a) { return *(const F32x4*) &a; }          //Convert raw bits to data type
inline I32x4 I32x4_bitsF32x4(F32x4 a) { return *(const I32x4*) &a; }          //Convert raw bits to data type

//Arithmetic

inline I32x4 I32x4_zero();
inline I32x4 I32x4_one();
inline I32x4 I32x4_two();
inline I32x4 I32x4_negTwo();
inline I32x4 I32x4_negOne();
inline I32x4 I32x4_xxxx4(I32 x);

inline I32x4 I32x4_add(I32x4 a, I32x4 b);
inline I32x4 I32x4_sub(I32x4 a, I32x4 b);
inline I32x4 I32x4_mul(I32x4 a, I32x4 b);
inline I32x4 I32x4_div(I32x4 a, I32x4 b);

inline I32x4 I32x4_complement(I32x4 a) { return I32x4_sub(I32x4_one(), a); }
inline I32x4 I32x4_negate(I32x4 a) { return I32x4_sub(I32x4_zero(), a); }

inline I32x4 I32x4_pow2(I32x4 a) { return I32x4_mul(a, a); }

inline I32x4 I32x4_sign(I32x4 v);     //Zero counts as signed too
inline I32x4 I32x4_abs(I32x4 v);
inline I32x4 I32x4_mod(I32x4 v, I32x4 d) { return I32x4_sub(v, I32x4_mul(I32x4_div(v, d), d)); }

inline I32 I32x4_reduce(I32x4 a);     //ax+ay+az+aw

inline I32x4 I32x4_min(I32x4 a, I32x4 b);
inline I32x4 I32x4_max(I32x4 a, I32x4 b);
inline I32x4 I32x4_clamp(I32x4 a, I32x4 mi, I32x4 ma) { return I32x4_max(mi, I32x4_min(ma, a)); }

//Boolean

inline Bool I32x4_all(I32x4 a);
inline Bool I32x4_any(I32x4 b);

inline I32x4 I32x4_eq(I32x4 a, I32x4 b);
inline I32x4 I32x4_neq(I32x4 a, I32x4 b);
inline I32x4 I32x4_geq(I32x4 a, I32x4 b);
inline I32x4 I32x4_gt(I32x4 a, I32x4 b);
inline I32x4 I32x4_leq(I32x4 a, I32x4 b);
inline I32x4 I32x4_lt(I32x4 a, I32x4 b);

inline I32x4 I32x4_or(I32x4 a, I32x4 b);
inline I32x4 I32x4_and(I32x4 a, I32x4 b);
inline I32x4 I32x4_xor(I32x4 a, I32x4 b);

inline Bool I32x4_eq4(I32x4 a, I32x4 b) { return I32x4_all(I32x4_eq(a, b)); }
inline Bool I32x4_neq4(I32x4 a, I32x4 b) { return !I32x4_eq4(a, b); }

//Swizzles and shizzle

inline I32 I32x4_x(I32x4 a);
inline I32 I32x4_y(I32x4 a);
inline I32 I32x4_z(I32x4 a);
inline I32 I32x4_w(I32x4 a);
inline I32 I32x4_get(I32x4 a, U8 i);

inline void I32x4_setX(I32x4 *a, I32 v);
inline void I32x4_setY(I32x4 *a, I32 v);
inline void I32x4_setZ(I32x4 *a, I32 v);
inline void I32x4_setW(I32x4 *a, I32 v);
inline void I32x4_set(I32x4 *a, U8 i, I32 v);

//Construction

inline I32x4 I32x4_create1(I32 x);
inline I32x4 I32x4_create2(I32 x, I32 y);
inline I32x4 I32x4_create3(I32 x, I32 y, I32 z);
inline I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w);

inline I32x4 I32x4_load1(const I32 *arr);
inline I32x4 I32x4_load2(const I32 *arr);
inline I32x4 I32x4_load3(const I32 *arr);
inline I32x4 I32x4_load4(const I32 *arr) { return *(const I32x4*) arr; }

//4D swizzles

#define _shufflei1(a, x) _shufflei(a, x, x, x, x)

inline I32x4 I32x4_xxxx(I32x4 a) { return _shufflei1(a, 0); }
inline I32x4 I32x4_xxxy(I32x4 a) { return _shufflei(a, 0, 0, 0, 1); }
inline I32x4 I32x4_xxxz(I32x4 a) { return _shufflei(a, 0, 0, 0, 2); }
inline I32x4 I32x4_xxxw(I32x4 a) { return _shufflei(a, 0, 0, 0, 3); }
inline I32x4 I32x4_xxyx(I32x4 a) { return _shufflei(a, 0, 0, 1, 0); }
inline I32x4 I32x4_xxyy(I32x4 a) { return _shufflei(a, 0, 0, 1, 1); }
inline I32x4 I32x4_xxyz(I32x4 a) { return _shufflei(a, 0, 0, 1, 2); }
inline I32x4 I32x4_xxyw(I32x4 a) { return _shufflei(a, 0, 0, 1, 3); }
inline I32x4 I32x4_xxzx(I32x4 a) { return _shufflei(a, 0, 0, 2, 0); }
inline I32x4 I32x4_xxzy(I32x4 a) { return _shufflei(a, 0, 0, 2, 1); }
inline I32x4 I32x4_xxzz(I32x4 a) { return _shufflei(a, 0, 0, 2, 2); }
inline I32x4 I32x4_xxzw(I32x4 a) { return _shufflei(a, 0, 0, 2, 3); }
inline I32x4 I32x4_xxwx(I32x4 a) { return _shufflei(a, 0, 0, 3, 0); }
inline I32x4 I32x4_xxwy(I32x4 a) { return _shufflei(a, 0, 0, 3, 1); }
inline I32x4 I32x4_xxwz(I32x4 a) { return _shufflei(a, 0, 0, 3, 2); }
inline I32x4 I32x4_xxww(I32x4 a) { return _shufflei(a, 0, 0, 3, 3); }

inline I32x4 I32x4_xyxx(I32x4 a) { return _shufflei(a, 0, 1, 0, 0); }
inline I32x4 I32x4_xyxy(I32x4 a) { return _shufflei(a, 0, 1, 0, 1); }
inline I32x4 I32x4_xyxz(I32x4 a) { return _shufflei(a, 0, 1, 0, 2); }
inline I32x4 I32x4_xyxw(I32x4 a) { return _shufflei(a, 0, 1, 0, 3); }
inline I32x4 I32x4_xyyx(I32x4 a) { return _shufflei(a, 0, 1, 1, 0); }
inline I32x4 I32x4_xyyy(I32x4 a) { return _shufflei(a, 0, 1, 1, 1); }
inline I32x4 I32x4_xyyz(I32x4 a) { return _shufflei(a, 0, 1, 1, 2); }
inline I32x4 I32x4_xyyw(I32x4 a) { return _shufflei(a, 0, 1, 1, 3); }
inline I32x4 I32x4_xyzx(I32x4 a) { return _shufflei(a, 0, 1, 2, 0); }
inline I32x4 I32x4_xyzy(I32x4 a) { return _shufflei(a, 0, 1, 2, 1); }
inline I32x4 I32x4_xyzz(I32x4 a) { return _shufflei(a, 0, 1, 2, 2); }
inline I32x4 I32x4_xyzw(I32x4 a) { return a; }
inline I32x4 I32x4_xywx(I32x4 a) { return _shufflei(a, 0, 1, 3, 0); }
inline I32x4 I32x4_xywy(I32x4 a) { return _shufflei(a, 0, 1, 3, 1); }
inline I32x4 I32x4_xywz(I32x4 a) { return _shufflei(a, 0, 1, 3, 2); }
inline I32x4 I32x4_xyww(I32x4 a) { return _shufflei(a, 0, 1, 3, 3); }

inline I32x4 I32x4_xzxx(I32x4 a) { return _shufflei(a, 0, 2, 0, 0); }
inline I32x4 I32x4_xzxy(I32x4 a) { return _shufflei(a, 0, 2, 0, 1); }
inline I32x4 I32x4_xzxz(I32x4 a) { return _shufflei(a, 0, 2, 0, 2); }
inline I32x4 I32x4_xzxw(I32x4 a) { return _shufflei(a, 0, 2, 0, 3); }
inline I32x4 I32x4_xzyx(I32x4 a) { return _shufflei(a, 0, 2, 1, 0); }
inline I32x4 I32x4_xzyy(I32x4 a) { return _shufflei(a, 0, 2, 1, 1); }
inline I32x4 I32x4_xzyz(I32x4 a) { return _shufflei(a, 0, 2, 1, 2); }
inline I32x4 I32x4_xzyw(I32x4 a) { return _shufflei(a, 0, 2, 1, 3); }
inline I32x4 I32x4_xzzx(I32x4 a) { return _shufflei(a, 0, 2, 2, 0); }
inline I32x4 I32x4_xzzy(I32x4 a) { return _shufflei(a, 0, 2, 2, 1); }
inline I32x4 I32x4_xzzz(I32x4 a) { return _shufflei(a, 0, 2, 2, 2); }
inline I32x4 I32x4_xzzw(I32x4 a) { return _shufflei(a, 0, 2, 2, 3); }
inline I32x4 I32x4_xzwx(I32x4 a) { return _shufflei(a, 0, 2, 3, 0); }
inline I32x4 I32x4_xzwy(I32x4 a) { return _shufflei(a, 0, 2, 3, 1); }
inline I32x4 I32x4_xzwz(I32x4 a) { return _shufflei(a, 0, 2, 3, 2); }
inline I32x4 I32x4_xzww(I32x4 a) { return _shufflei(a, 0, 2, 3, 3); }

inline I32x4 I32x4_xwxx(I32x4 a) { return _shufflei(a, 0, 3, 0, 0); }
inline I32x4 I32x4_xwxy(I32x4 a) { return _shufflei(a, 0, 3, 0, 1); }
inline I32x4 I32x4_xwxz(I32x4 a) { return _shufflei(a, 0, 3, 0, 2); }
inline I32x4 I32x4_xwxw(I32x4 a) { return _shufflei(a, 0, 3, 0, 3); }
inline I32x4 I32x4_xwyx(I32x4 a) { return _shufflei(a, 0, 3, 1, 0); }
inline I32x4 I32x4_xwyy(I32x4 a) { return _shufflei(a, 0, 3, 1, 1); }
inline I32x4 I32x4_xwyz(I32x4 a) { return _shufflei(a, 0, 3, 1, 2); }
inline I32x4 I32x4_xwyw(I32x4 a) { return _shufflei(a, 0, 3, 1, 3); }
inline I32x4 I32x4_xwzx(I32x4 a) { return _shufflei(a, 0, 3, 2, 0); }
inline I32x4 I32x4_xwzy(I32x4 a) { return _shufflei(a, 0, 3, 2, 1); }
inline I32x4 I32x4_xwzz(I32x4 a) { return _shufflei(a, 0, 3, 2, 2); }
inline I32x4 I32x4_xwzw(I32x4 a) { return _shufflei(a, 0, 3, 2, 3); }
inline I32x4 I32x4_xwwx(I32x4 a) { return _shufflei(a, 0, 3, 3, 0); }
inline I32x4 I32x4_xwwy(I32x4 a) { return _shufflei(a, 0, 3, 3, 1); }
inline I32x4 I32x4_xwwz(I32x4 a) { return _shufflei(a, 0, 3, 3, 2); }
inline I32x4 I32x4_xwww(I32x4 a) { return _shufflei(a, 0, 3, 3, 3); }

inline I32x4 I32x4_yxxx(I32x4 a) { return _shufflei(a, 1, 0, 0, 0); }
inline I32x4 I32x4_yxxy(I32x4 a) { return _shufflei(a, 1, 0, 0, 1); }
inline I32x4 I32x4_yxxz(I32x4 a) { return _shufflei(a, 1, 0, 0, 2); }
inline I32x4 I32x4_yxxw(I32x4 a) { return _shufflei(a, 1, 0, 0, 3); }
inline I32x4 I32x4_yxyx(I32x4 a) { return _shufflei(a, 1, 0, 1, 0); }
inline I32x4 I32x4_yxyy(I32x4 a) { return _shufflei(a, 1, 0, 1, 1); }
inline I32x4 I32x4_yxyz(I32x4 a) { return _shufflei(a, 1, 0, 1, 2); }
inline I32x4 I32x4_yxyw(I32x4 a) { return _shufflei(a, 1, 0, 1, 3); }
inline I32x4 I32x4_yxzx(I32x4 a) { return _shufflei(a, 1, 0, 2, 0); }
inline I32x4 I32x4_yxzy(I32x4 a) { return _shufflei(a, 1, 0, 2, 1); }
inline I32x4 I32x4_yxzz(I32x4 a) { return _shufflei(a, 1, 0, 2, 2); }
inline I32x4 I32x4_yxzw(I32x4 a) { return _shufflei(a, 1, 0, 2, 3); }
inline I32x4 I32x4_yxwx(I32x4 a) { return _shufflei(a, 1, 0, 3, 0); }
inline I32x4 I32x4_yxwy(I32x4 a) { return _shufflei(a, 1, 0, 3, 1); }
inline I32x4 I32x4_yxwz(I32x4 a) { return _shufflei(a, 1, 0, 3, 2); }
inline I32x4 I32x4_yxww(I32x4 a) { return _shufflei(a, 1, 0, 3, 3); }

inline I32x4 I32x4_yyxx(I32x4 a) { return _shufflei(a, 1, 1, 0, 0); }
inline I32x4 I32x4_yyxy(I32x4 a) { return _shufflei(a, 1, 1, 0, 1); }
inline I32x4 I32x4_yyxz(I32x4 a) { return _shufflei(a, 1, 1, 0, 2); }
inline I32x4 I32x4_yyxw(I32x4 a) { return _shufflei(a, 1, 1, 0, 3); }
inline I32x4 I32x4_yyyx(I32x4 a) { return _shufflei(a, 1, 1, 1, 0); }
inline I32x4 I32x4_yyyy(I32x4 a) { return _shufflei1(a, 1); } 
inline I32x4 I32x4_yyyz(I32x4 a) { return _shufflei(a, 1, 1, 1, 2); }
inline I32x4 I32x4_yyyw(I32x4 a) { return _shufflei(a, 1, 1, 1, 3); }
inline I32x4 I32x4_yyzx(I32x4 a) { return _shufflei(a, 1, 1, 2, 0); }
inline I32x4 I32x4_yyzy(I32x4 a) { return _shufflei(a, 1, 1, 2, 1); }
inline I32x4 I32x4_yyzz(I32x4 a) { return _shufflei(a, 1, 1, 2, 2); }
inline I32x4 I32x4_yyzw(I32x4 a) { return _shufflei(a, 1, 1, 2, 3); }
inline I32x4 I32x4_yywx(I32x4 a) { return _shufflei(a, 1, 1, 3, 0); }
inline I32x4 I32x4_yywy(I32x4 a) { return _shufflei(a, 1, 1, 3, 1); }
inline I32x4 I32x4_yywz(I32x4 a) { return _shufflei(a, 1, 1, 3, 2); }
inline I32x4 I32x4_yyww(I32x4 a) { return _shufflei(a, 1, 1, 3, 3); }

inline I32x4 I32x4_yzxx(I32x4 a) { return _shufflei(a, 1, 2, 0, 0); }
inline I32x4 I32x4_yzxy(I32x4 a) { return _shufflei(a, 1, 2, 0, 1); }
inline I32x4 I32x4_yzxz(I32x4 a) { return _shufflei(a, 1, 2, 0, 2); }
inline I32x4 I32x4_yzxw(I32x4 a) { return _shufflei(a, 1, 2, 0, 3); }
inline I32x4 I32x4_yzyx(I32x4 a) { return _shufflei(a, 1, 2, 1, 0); }
inline I32x4 I32x4_yzyy(I32x4 a) { return _shufflei(a, 1, 2, 1, 1); }
inline I32x4 I32x4_yzyz(I32x4 a) { return _shufflei(a, 1, 2, 1, 2); }
inline I32x4 I32x4_yzyw(I32x4 a) { return _shufflei(a, 1, 2, 1, 3); }
inline I32x4 I32x4_yzzx(I32x4 a) { return _shufflei(a, 1, 2, 2, 0); }
inline I32x4 I32x4_yzzy(I32x4 a) { return _shufflei(a, 1, 2, 2, 1); }
inline I32x4 I32x4_yzzz(I32x4 a) { return _shufflei(a, 1, 2, 2, 2); }
inline I32x4 I32x4_yzzw(I32x4 a) { return _shufflei(a, 1, 2, 2, 3); }
inline I32x4 I32x4_yzwx(I32x4 a) { return _shufflei(a, 1, 2, 3, 0); }
inline I32x4 I32x4_yzwy(I32x4 a) { return _shufflei(a, 1, 2, 3, 1); }
inline I32x4 I32x4_yzwz(I32x4 a) { return _shufflei(a, 1, 2, 3, 2); }
inline I32x4 I32x4_yzww(I32x4 a) { return _shufflei(a, 1, 2, 3, 3); }

inline I32x4 I32x4_ywxx(I32x4 a) { return _shufflei(a, 1, 3, 0, 0); }
inline I32x4 I32x4_ywxy(I32x4 a) { return _shufflei(a, 1, 3, 0, 1); }
inline I32x4 I32x4_ywxz(I32x4 a) { return _shufflei(a, 1, 3, 0, 2); }
inline I32x4 I32x4_ywxw(I32x4 a) { return _shufflei(a, 1, 3, 0, 3); }
inline I32x4 I32x4_ywyx(I32x4 a) { return _shufflei(a, 1, 3, 1, 0); }
inline I32x4 I32x4_ywyy(I32x4 a) { return _shufflei(a, 1, 3, 1, 1); }
inline I32x4 I32x4_ywyz(I32x4 a) { return _shufflei(a, 1, 3, 1, 2); }
inline I32x4 I32x4_ywyw(I32x4 a) { return _shufflei(a, 1, 3, 1, 3); }
inline I32x4 I32x4_ywzx(I32x4 a) { return _shufflei(a, 1, 3, 2, 0); }
inline I32x4 I32x4_ywzy(I32x4 a) { return _shufflei(a, 1, 3, 2, 1); }
inline I32x4 I32x4_ywzz(I32x4 a) { return _shufflei(a, 1, 3, 2, 2); }
inline I32x4 I32x4_ywzw(I32x4 a) { return _shufflei(a, 1, 3, 2, 3); }
inline I32x4 I32x4_ywwx(I32x4 a) { return _shufflei(a, 1, 3, 3, 0); }
inline I32x4 I32x4_ywwy(I32x4 a) { return _shufflei(a, 1, 3, 3, 1); }
inline I32x4 I32x4_ywwz(I32x4 a) { return _shufflei(a, 1, 3, 3, 2); }
inline I32x4 I32x4_ywww(I32x4 a) { return _shufflei(a, 1, 3, 3, 3); }

inline I32x4 I32x4_zxxx(I32x4 a) { return _shufflei(a, 2, 0, 0, 0); }
inline I32x4 I32x4_zxxy(I32x4 a) { return _shufflei(a, 2, 0, 0, 1); }
inline I32x4 I32x4_zxxz(I32x4 a) { return _shufflei(a, 2, 0, 0, 2); }
inline I32x4 I32x4_zxxw(I32x4 a) { return _shufflei(a, 2, 0, 0, 3); }
inline I32x4 I32x4_zxyx(I32x4 a) { return _shufflei(a, 2, 0, 1, 0); }
inline I32x4 I32x4_zxyy(I32x4 a) { return _shufflei(a, 2, 0, 1, 1); }
inline I32x4 I32x4_zxyz(I32x4 a) { return _shufflei(a, 2, 0, 1, 2); }
inline I32x4 I32x4_zxyw(I32x4 a) { return _shufflei(a, 2, 0, 1, 3); }
inline I32x4 I32x4_zxzx(I32x4 a) { return _shufflei(a, 2, 0, 2, 0); }
inline I32x4 I32x4_zxzy(I32x4 a) { return _shufflei(a, 2, 0, 2, 1); }
inline I32x4 I32x4_zxzz(I32x4 a) { return _shufflei(a, 2, 0, 2, 2); }
inline I32x4 I32x4_zxzw(I32x4 a) { return _shufflei(a, 2, 0, 2, 3); }
inline I32x4 I32x4_zxwx(I32x4 a) { return _shufflei(a, 2, 0, 3, 0); }
inline I32x4 I32x4_zxwy(I32x4 a) { return _shufflei(a, 2, 0, 3, 1); }
inline I32x4 I32x4_zxwz(I32x4 a) { return _shufflei(a, 2, 0, 3, 2); }
inline I32x4 I32x4_zxww(I32x4 a) { return _shufflei(a, 2, 0, 3, 3); }

inline I32x4 I32x4_zyxx(I32x4 a) { return _shufflei(a, 2, 1, 0, 0); }
inline I32x4 I32x4_zyxy(I32x4 a) { return _shufflei(a, 2, 1, 0, 1); }
inline I32x4 I32x4_zyxz(I32x4 a) { return _shufflei(a, 2, 1, 0, 2); }
inline I32x4 I32x4_zyxw(I32x4 a) { return _shufflei(a, 2, 1, 0, 3); }
inline I32x4 I32x4_zyyx(I32x4 a) { return _shufflei(a, 2, 1, 1, 0); }
inline I32x4 I32x4_zyyy(I32x4 a) { return _shufflei(a, 2, 1, 1, 1); }
inline I32x4 I32x4_zyyz(I32x4 a) { return _shufflei(a, 2, 1, 1, 2); }
inline I32x4 I32x4_zyyw(I32x4 a) { return _shufflei(a, 2, 1, 1, 3); }
inline I32x4 I32x4_zyzx(I32x4 a) { return _shufflei(a, 2, 1, 2, 0); }
inline I32x4 I32x4_zyzy(I32x4 a) { return _shufflei(a, 2, 1, 2, 1); }
inline I32x4 I32x4_zyzz(I32x4 a) { return _shufflei(a, 2, 1, 2, 2); }
inline I32x4 I32x4_zyzw(I32x4 a) { return _shufflei(a, 2, 1, 2, 3); }
inline I32x4 I32x4_zywx(I32x4 a) { return _shufflei(a, 2, 1, 3, 0); }
inline I32x4 I32x4_zywy(I32x4 a) { return _shufflei(a, 2, 1, 3, 1); }
inline I32x4 I32x4_zywz(I32x4 a) { return _shufflei(a, 2, 1, 3, 2); }
inline I32x4 I32x4_zyww(I32x4 a) { return _shufflei(a, 2, 1, 3, 3); }

inline I32x4 I32x4_zzxx(I32x4 a) { return _shufflei(a, 2, 2, 0, 0); }
inline I32x4 I32x4_zzxy(I32x4 a) { return _shufflei(a, 2, 2, 0, 1); }
inline I32x4 I32x4_zzxz(I32x4 a) { return _shufflei(a, 2, 2, 0, 2); }
inline I32x4 I32x4_zzxw(I32x4 a) { return _shufflei(a, 2, 2, 0, 3); }
inline I32x4 I32x4_zzyx(I32x4 a) { return _shufflei(a, 2, 2, 1, 0); }
inline I32x4 I32x4_zzyy(I32x4 a) { return _shufflei(a, 2, 2, 1, 1); }
inline I32x4 I32x4_zzyz(I32x4 a) { return _shufflei(a, 2, 2, 1, 2); }
inline I32x4 I32x4_zzyw(I32x4 a) { return _shufflei(a, 2, 2, 1, 3); }
inline I32x4 I32x4_zzzx(I32x4 a) { return _shufflei(a, 2, 2, 2, 0); }
inline I32x4 I32x4_zzzy(I32x4 a) { return _shufflei(a, 2, 2, 2, 1); }
inline I32x4 I32x4_zzzz(I32x4 a) { return _shufflei1(a, 2); }
inline I32x4 I32x4_zzzw(I32x4 a) { return _shufflei(a, 2, 2, 2, 3); }
inline I32x4 I32x4_zzwx(I32x4 a) { return _shufflei(a, 2, 2, 3, 0); }
inline I32x4 I32x4_zzwy(I32x4 a) { return _shufflei(a, 2, 2, 3, 1); }
inline I32x4 I32x4_zzwz(I32x4 a) { return _shufflei(a, 2, 2, 3, 2); }
inline I32x4 I32x4_zzww(I32x4 a) { return _shufflei(a, 2, 2, 3, 3); }

inline I32x4 I32x4_zwxx(I32x4 a) { return _shufflei(a, 2, 3, 0, 0); }
inline I32x4 I32x4_zwxy(I32x4 a) { return _shufflei(a, 2, 3, 0, 1); }
inline I32x4 I32x4_zwxz(I32x4 a) { return _shufflei(a, 2, 3, 0, 2); }
inline I32x4 I32x4_zwxw(I32x4 a) { return _shufflei(a, 2, 3, 0, 3); }
inline I32x4 I32x4_zwyx(I32x4 a) { return _shufflei(a, 2, 3, 1, 0); }
inline I32x4 I32x4_zwyy(I32x4 a) { return _shufflei(a, 2, 3, 1, 1); }
inline I32x4 I32x4_zwyz(I32x4 a) { return _shufflei(a, 2, 3, 1, 2); }
inline I32x4 I32x4_zwyw(I32x4 a) { return _shufflei(a, 2, 3, 1, 3); }
inline I32x4 I32x4_zwzx(I32x4 a) { return _shufflei(a, 2, 3, 2, 0); }
inline I32x4 I32x4_zwzy(I32x4 a) { return _shufflei(a, 2, 3, 2, 1); }
inline I32x4 I32x4_zwzz(I32x4 a) { return _shufflei(a, 2, 3, 2, 2); }
inline I32x4 I32x4_zwzw(I32x4 a) { return _shufflei(a, 2, 3, 2, 3); }
inline I32x4 I32x4_zwwx(I32x4 a) { return _shufflei(a, 2, 3, 3, 0); }
inline I32x4 I32x4_zwwy(I32x4 a) { return _shufflei(a, 2, 3, 3, 1); }
inline I32x4 I32x4_zwwz(I32x4 a) { return _shufflei(a, 2, 3, 3, 2); }
inline I32x4 I32x4_zwww(I32x4 a) { return _shufflei(a, 2, 3, 3, 3); }

inline I32x4 I32x4_wxxx(I32x4 a) { return _shufflei(a, 3, 0, 0, 0); }
inline I32x4 I32x4_wxxy(I32x4 a) { return _shufflei(a, 3, 0, 0, 1); }
inline I32x4 I32x4_wxxz(I32x4 a) { return _shufflei(a, 3, 0, 0, 2); }
inline I32x4 I32x4_wxxw(I32x4 a) { return _shufflei(a, 3, 0, 0, 3); }
inline I32x4 I32x4_wxyx(I32x4 a) { return _shufflei(a, 3, 0, 1, 0); }
inline I32x4 I32x4_wxyy(I32x4 a) { return _shufflei(a, 3, 0, 1, 1); }
inline I32x4 I32x4_wxyz(I32x4 a) { return _shufflei(a, 3, 0, 1, 2); }
inline I32x4 I32x4_wxyw(I32x4 a) { return _shufflei(a, 3, 0, 1, 3); }
inline I32x4 I32x4_wxzx(I32x4 a) { return _shufflei(a, 3, 0, 2, 0); }
inline I32x4 I32x4_wxzy(I32x4 a) { return _shufflei(a, 3, 0, 2, 1); }
inline I32x4 I32x4_wxzz(I32x4 a) { return _shufflei(a, 3, 0, 2, 2); }
inline I32x4 I32x4_wxzw(I32x4 a) { return _shufflei(a, 3, 0, 2, 3); }
inline I32x4 I32x4_wxwx(I32x4 a) { return _shufflei(a, 3, 0, 3, 0); }
inline I32x4 I32x4_wxwy(I32x4 a) { return _shufflei(a, 3, 0, 3, 1); }
inline I32x4 I32x4_wxwz(I32x4 a) { return _shufflei(a, 3, 0, 3, 2); }
inline I32x4 I32x4_wxww(I32x4 a) { return _shufflei(a, 3, 0, 3, 3); }

inline I32x4 I32x4_wyxx(I32x4 a) { return _shufflei(a, 3, 1, 0, 0); }
inline I32x4 I32x4_wyxy(I32x4 a) { return _shufflei(a, 3, 1, 0, 1); }
inline I32x4 I32x4_wyxz(I32x4 a) { return _shufflei(a, 3, 1, 0, 2); }
inline I32x4 I32x4_wyxw(I32x4 a) { return _shufflei(a, 3, 1, 0, 3); }
inline I32x4 I32x4_wyyx(I32x4 a) { return _shufflei(a, 3, 1, 1, 0); }
inline I32x4 I32x4_wyyy(I32x4 a) { return _shufflei(a, 3, 1, 1, 1); }
inline I32x4 I32x4_wyyz(I32x4 a) { return _shufflei(a, 3, 1, 1, 2); }
inline I32x4 I32x4_wyyw(I32x4 a) { return _shufflei(a, 3, 1, 1, 3); }
inline I32x4 I32x4_wyzx(I32x4 a) { return _shufflei(a, 3, 1, 2, 0); }
inline I32x4 I32x4_wyzy(I32x4 a) { return _shufflei(a, 3, 1, 2, 1); }
inline I32x4 I32x4_wyzz(I32x4 a) { return _shufflei(a, 3, 1, 2, 2); }
inline I32x4 I32x4_wyzw(I32x4 a) { return _shufflei(a, 3, 1, 2, 3); }
inline I32x4 I32x4_wywx(I32x4 a) { return _shufflei(a, 3, 1, 3, 0); }
inline I32x4 I32x4_wywy(I32x4 a) { return _shufflei(a, 3, 1, 3, 1); }
inline I32x4 I32x4_wywz(I32x4 a) { return _shufflei(a, 3, 1, 3, 2); }
inline I32x4 I32x4_wyww(I32x4 a) { return _shufflei(a, 3, 1, 3, 3); }

inline I32x4 I32x4_wzxx(I32x4 a) { return _shufflei(a, 3, 2, 0, 0); }
inline I32x4 I32x4_wzxy(I32x4 a) { return _shufflei(a, 3, 2, 0, 1); }
inline I32x4 I32x4_wzxz(I32x4 a) { return _shufflei(a, 3, 2, 0, 2); }
inline I32x4 I32x4_wzxw(I32x4 a) { return _shufflei(a, 3, 2, 0, 3); }
inline I32x4 I32x4_wzyx(I32x4 a) { return _shufflei(a, 3, 2, 1, 0); }
inline I32x4 I32x4_wzyy(I32x4 a) { return _shufflei(a, 3, 2, 1, 1); }
inline I32x4 I32x4_wzyz(I32x4 a) { return _shufflei(a, 3, 2, 1, 2); }
inline I32x4 I32x4_wzyw(I32x4 a) { return _shufflei(a, 3, 2, 1, 3); }
inline I32x4 I32x4_wzzx(I32x4 a) { return _shufflei(a, 3, 2, 2, 0); }
inline I32x4 I32x4_wzzy(I32x4 a) { return _shufflei(a, 3, 2, 2, 1); }
inline I32x4 I32x4_wzzz(I32x4 a) { return _shufflei(a, 3, 2, 2, 2); }
inline I32x4 I32x4_wzzw(I32x4 a) { return _shufflei(a, 3, 2, 2, 3); }
inline I32x4 I32x4_wzwx(I32x4 a) { return _shufflei(a, 3, 2, 3, 0); }
inline I32x4 I32x4_wzwy(I32x4 a) { return _shufflei(a, 3, 2, 3, 1); }
inline I32x4 I32x4_wzwz(I32x4 a) { return _shufflei(a, 3, 2, 3, 2); }
inline I32x4 I32x4_wzww(I32x4 a) { return _shufflei(a, 3, 2, 3, 3); }

inline I32x4 I32x4_wwxx(I32x4 a) { return _shufflei(a, 3, 3, 0, 0); }
inline I32x4 I32x4_wwxy(I32x4 a) { return _shufflei(a, 3, 3, 0, 1); }
inline I32x4 I32x4_wwxz(I32x4 a) { return _shufflei(a, 3, 3, 0, 2); }
inline I32x4 I32x4_wwxw(I32x4 a) { return _shufflei(a, 3, 3, 0, 3); }
inline I32x4 I32x4_wwyx(I32x4 a) { return _shufflei(a, 3, 3, 1, 0); }
inline I32x4 I32x4_wwyy(I32x4 a) { return _shufflei(a, 3, 3, 1, 1); }
inline I32x4 I32x4_wwyz(I32x4 a) { return _shufflei(a, 3, 3, 1, 2); }
inline I32x4 I32x4_wwyw(I32x4 a) { return _shufflei(a, 3, 3, 1, 3); }
inline I32x4 I32x4_wwzx(I32x4 a) { return _shufflei(a, 3, 3, 2, 0); }
inline I32x4 I32x4_wwzy(I32x4 a) { return _shufflei(a, 3, 3, 2, 1); }
inline I32x4 I32x4_wwzz(I32x4 a) { return _shufflei(a, 3, 3, 2, 2); }
inline I32x4 I32x4_wwzw(I32x4 a) { return _shufflei(a, 3, 3, 2, 3); }
inline I32x4 I32x4_wwwx(I32x4 a) { return _shufflei(a, 3, 3, 3, 0); }
inline I32x4 I32x4_wwwy(I32x4 a) { return _shufflei(a, 3, 3, 3, 1); }
inline I32x4 I32x4_wwwz(I32x4 a) { return _shufflei(a, 3, 3, 3, 2); }
inline I32x4 I32x4_wwww(I32x4 a) { return _shufflei1(a, 3); }

inline I32x4 I32x4_trunc2(I32x4 a);
inline I32x4 I32x4_trunc3(I32x4 a);

//2D swizzles

inline I32x4 I32x4_xx4(I32x4 a) { return I32x4_trunc2(I32x4_xxxx(a)); }
inline I32x4 I32x4_xy4(I32x4 a) { return I32x4_trunc2(a); }
inline I32x4 I32x4_xz4(I32x4 a) { return I32x4_trunc2(I32x4_xzxx(a)); }
inline I32x4 I32x4_xw4(I32x4 a) { return I32x4_trunc2(I32x4_xwxx(a)); }

inline I32x4 I32x4_yx4(I32x4 a) { return I32x4_trunc2(I32x4_yxxx(a)); }
inline I32x4 I32x4_yy4(I32x4 a) { return I32x4_trunc2(I32x4_yyxx(a)); }
inline I32x4 I32x4_yz4(I32x4 a) { return I32x4_trunc2(I32x4_yzxx(a)); }
inline I32x4 I32x4_yw4(I32x4 a) { return I32x4_trunc2(I32x4_ywxx(a)); }

inline I32x4 I32x4_zx4(I32x4 a) { return I32x4_trunc2(I32x4_zxxx(a)); }
inline I32x4 I32x4_zy4(I32x4 a) { return I32x4_trunc2(I32x4_zyxx(a)); }
inline I32x4 I32x4_zz4(I32x4 a) { return I32x4_trunc2(I32x4_zzxx(a)); }
inline I32x4 I32x4_zw4(I32x4 a) { return I32x4_trunc2(I32x4_zwxx(a)); }

inline I32x4 I32x4_wx4(I32x4 a) { return I32x4_trunc2(I32x4_wxxx(a)); }
inline I32x4 I32x4_wy4(I32x4 a) { return I32x4_trunc2(I32x4_wyxx(a)); }
inline I32x4 I32x4_wz4(I32x4 a) { return I32x4_trunc2(I32x4_wzxx(a)); }
inline I32x4 I32x4_ww4(I32x4 a) { return I32x4_trunc2(I32x4_wwxx(a)); }

//3D swizzles

inline I32x4 I32x4_xxx(I32x4 a) { return I32x4_trunc3(I32x4_xxxx(a)); }
inline I32x4 I32x4_xxy(I32x4 a) { return I32x4_trunc3(I32x4_xxyx(a)); }
inline I32x4 I32x4_xxz(I32x4 a) { return I32x4_trunc3(I32x4_xxzx(a)); }
inline I32x4 I32x4_xyx(I32x4 a) { return I32x4_trunc3(I32x4_xyxx(a)); }
inline I32x4 I32x4_xyy(I32x4 a) { return I32x4_trunc3(I32x4_xyyx(a)); }
inline I32x4 I32x4_xyz(I32x4 a) { return I32x4_trunc3(a); }
inline I32x4 I32x4_xzx(I32x4 a) { return I32x4_trunc3(I32x4_xzxx(a)); }
inline I32x4 I32x4_xzy(I32x4 a) { return I32x4_trunc3(I32x4_xzyx(a)); }
inline I32x4 I32x4_xzz(I32x4 a) { return I32x4_trunc3(I32x4_xzzx(a)); }

inline I32x4 I32x4_yxx(I32x4 a) { return I32x4_trunc3(I32x4_yxxx(a)); }
inline I32x4 I32x4_yxy(I32x4 a) { return I32x4_trunc3(I32x4_yxyx(a)); }
inline I32x4 I32x4_yxz(I32x4 a) { return I32x4_trunc3(I32x4_yxzx(a)); }
inline I32x4 I32x4_yyx(I32x4 a) { return I32x4_trunc3(I32x4_yyxx(a)); }
inline I32x4 I32x4_yyy(I32x4 a) { return I32x4_trunc3(I32x4_yyyx(a)); }
inline I32x4 I32x4_yyz(I32x4 a) { return I32x4_trunc3(I32x4_yyzx(a)); }
inline I32x4 I32x4_yzx(I32x4 a) { return I32x4_trunc3(I32x4_yzxx(a)); }
inline I32x4 I32x4_yzy(I32x4 a) { return I32x4_trunc3(I32x4_yzyx(a)); }
inline I32x4 I32x4_yzz(I32x4 a) { return I32x4_trunc3(I32x4_yzzx(a)); }

inline I32x4 I32x4_zxx(I32x4 a) { return I32x4_trunc3(I32x4_zxxx(a)); }
inline I32x4 I32x4_zxy(I32x4 a) { return I32x4_trunc3(I32x4_zxyx(a)); }
inline I32x4 I32x4_zxz(I32x4 a) { return I32x4_trunc3(I32x4_zxzx(a)); }
inline I32x4 I32x4_zyx(I32x4 a) { return I32x4_trunc3(I32x4_zyxx(a)); }
inline I32x4 I32x4_zyy(I32x4 a) { return I32x4_trunc3(I32x4_zyyx(a)); }
inline I32x4 I32x4_zyz(I32x4 a) { return I32x4_trunc3(I32x4_zyzx(a)); }
inline I32x4 I32x4_zzx(I32x4 a) { return I32x4_trunc3(I32x4_zzxx(a)); }
inline I32x4 I32x4_zzy(I32x4 a) { return I32x4_trunc3(I32x4_zzyx(a)); }
inline I32x4 I32x4_zzz(I32x4 a) { return I32x4_trunc3(I32x4_zzzx(a)); }

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline I32x4 I32x4_sign(I32x4 v) {
    return I32x4_add(
        I32x4_mul(I32x4_lt(v, I32x4_zero()), I32x4_two()), 
        I32x4_one()
    ); 
}

inline I32x4 I32x4_abs(I32x4 v){ return I32x4_mul(I32x4_sign(v), v); }

//Simple definitions

inline I32x4 I32x4_one() { return I32x4_xxxx4(1); }
inline I32x4 I32x4_two() { return I32x4_xxxx4(2); }
inline I32x4 I32x4_negOne() { return I32x4_xxxx4(-1); }
inline I32x4 I32x4_negTwo() { return I32x4_xxxx4(-2); }

inline Bool I32x4_all(I32x4 b) { return I32x4_reduce(I32x4_neq(b, I32x4_zero())) == 4; }
inline Bool I32x4_any(I32x4 b) { return I32x4_reduce(I32x4_neq(b, I32x4_zero())); }

inline I32x4 I32x4_load1(const I32 *arr) { return I32x4_create1(*arr); }
inline I32x4 I32x4_load2(const I32 *arr) { return I32x4_create2(*arr, arr[1]); }
inline I32x4 I32x4_load3(const I32 *arr) { return I32x4_create3(*arr, arr[1], arr[2]); }

inline void I32x4_setX(I32x4 *a, I32 v) { *a = I32x4_create4(v,             I32x4_y(*a),    I32x4_z(*a),    I32x4_w(*a)); }
inline void I32x4_setY(I32x4 *a, I32 v) { *a = I32x4_create4(I32x4_x(*a),   v,              I32x4_z(*a),    I32x4_w(*a)); }
inline void I32x4_setZ(I32x4 *a, I32 v) { *a = I32x4_create4(I32x4_x(*a),   I32x4_y(*a),    v,              I32x4_w(*a)); }
inline void I32x4_setW(I32x4 *a, I32 v) { *a = I32x4_create4(I32x4_x(*a),   I32x4_y(*a),    I32x4_z(*a),    v); }

inline void I32x4_set(I32x4 *a, U8 i, I32 v) {
    switch (i & 3) {
        case 0:     I32x4_setX(a, v);     break;
        case 1:     I32x4_setY(a, v);     break;
        case 2:     I32x4_setZ(a, v);     break;
        default:    I32x4_setW(a, v);
    }
}

inline I32 I32x4_get(I32x4 a, U8 i) {
    switch (i & 3) {
        case 0:     return I32x4_x(a);
        case 1:     return I32x4_y(a);
        case 2:     return I32x4_z(a);
        default:    return I32x4_w(a);
    }
}
