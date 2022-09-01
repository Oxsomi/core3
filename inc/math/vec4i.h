#pragma once

inline f32x4 f32x4_bitsI32x4(i32x4 a) { return *(const f32x4*) &a; }          //Convert raw bits to data type
inline i32x4 i32x4_bitsF32x4(f32x4 a) { return *(const i32x4*) &a; }          //Convert raw bits to data type

//Arithmetic

inline i32x4 i32x4_zero();
inline i32x4 i32x4_one();
inline i32x4 i32x4_two();
inline i32x4 i32x4_negTwo();
inline i32x4 i32x4_negOne();
inline i32x4 i32x4_mask2();
inline i32x4 i32x4_mask3();
inline i32x4 i32x4_xxxx4(i32 x);

inline i32x4 i32x4_add(i32x4 a, i32x4 b);
inline i32x4 i32x4_sub(i32x4 a, i32x4 b);
inline i32x4 i32x4_mul(i32x4 a, i32x4 b);
inline i32x4 i32x4_div(i32x4 a, i32x4 b);

inline i32x4 i32x4_complement(i32x4 a) { return i32x4_sub(i32x4_one(), a); }
inline i32x4 i32x4_negate(i32x4 a) { return i32x4_sub(i32x4_zero(), a); }
inline i32x4 i32x4_inverse(i32x4 a) { return i32x4_div(i32x4_one(), a); }

inline i32x4 i32x4_pow2(i32x4 a) { return i32x4_mul(a, a); }

inline i32x4 i32x4_sign(i32x4 v);     //Zero counts as signed too
inline i32x4 i32x4_abs(i32x4 v);
inline i32x4 i32x4_mod(i32x4 v, i32x4 d) { return i32x4_sub(v, i32x4_mul(i32x4_div(v, d), d)); }

inline i32 i32x4_reduce(i32x4 a);     //ax+ay+az+aw

inline i32x4 i32x4_min(i32x4 a, i32x4 b);
inline i32x4 i32x4_max(i32x4 a, i32x4 b);
inline i32x4 i32x4_clamp(i32x4 a, i32x4 mi, i32x4 ma) { return i32x4_max(mi, i32x4_min(ma, a)); }

//Boolean

inline bool i32x4_all(i32x4 a);
inline bool i32x4_any(i32x4 b);

inline i32x4 i32x4_eq(i32x4 a, i32x4 b);
inline i32x4 i32x4_neq(i32x4 a, i32x4 b);
inline i32x4 i32x4_geq(i32x4 a, i32x4 b);
inline i32x4 i32x4_gt(i32x4 a, i32x4 b);
inline i32x4 i32x4_leq(i32x4 a, i32x4 b);
inline i32x4 i32x4_lt(i32x4 a, i32x4 b);

inline i32x4 i32x4_or(i32x4 a, i32x4 b);
inline i32x4 i32x4_and(i32x4 a, i32x4 b);
inline i32x4 i32x4_xor(i32x4 a, i32x4 b);

inline bool i32x4_eq4(i32x4 a, i32x4 b) { return i32x4_all(i32x4_eq(a, b)); }
inline bool i32x4_neq4(i32x4 a, i32x4 b) { return !i32x4_eq4(a, b); }

//Swizzles and shizzle

inline i32 i32x4_x(i32x4 a);
inline i32 i32x4_y(i32x4 a);
inline i32 i32x4_z(i32x4 a);
inline i32 i32x4_w(i32x4 a);
inline i32 i32x4_get(i32x4 a, u8 i);

inline void i32x4_setX(i32x4 *a, i32 v);
inline void i32x4_setY(i32x4 *a, i32 v);
inline void i32x4_setZ(i32x4 *a, i32 v);
inline void i32x4_setW(i32x4 *a, i32 v);
inline void i32x4_set(i32x4 *a, u8 i, i32 v);

//Construction

inline i32x4 i32x4_init1(i32 x);
inline i32x4 i32x4_init2(i32 x, i32 y);
inline i32x4 i32x4_init3(i32 x, i32 y, i32 z);
inline i32x4 i32x4_init4(i32 x, i32 y, i32 z, i32 w);

inline i32x4 i32x4_load1(const i32 *arr);
inline i32x4 i32x4_load2(const i32 *arr);
inline i32x4 i32x4_load3(const i32 *arr);
inline i32x4 i32x4_load4(const i32 *arr) { return *(const i32x4*) arr; }

//4D swizzles

#define _shufflei1(a, x) _shufflei(a, x, x, x, x)

inline i32x4 i32x4_xxxx(i32x4 a) { return _shufflei1(a, 0); }
inline i32x4 i32x4_xxxy(i32x4 a) { return _shufflei(a, 0, 0, 0, 1); }
inline i32x4 i32x4_xxxz(i32x4 a) { return _shufflei(a, 0, 0, 0, 2); }
inline i32x4 i32x4_xxxw(i32x4 a) { return _shufflei(a, 0, 0, 0, 3); }
inline i32x4 i32x4_xxyx(i32x4 a) { return _shufflei(a, 0, 0, 1, 0); }
inline i32x4 i32x4_xxyy(i32x4 a) { return _shufflei(a, 0, 0, 1, 1); }
inline i32x4 i32x4_xxyz(i32x4 a) { return _shufflei(a, 0, 0, 1, 2); }
inline i32x4 i32x4_xxyw(i32x4 a) { return _shufflei(a, 0, 0, 1, 3); }
inline i32x4 i32x4_xxzx(i32x4 a) { return _shufflei(a, 0, 0, 2, 0); }
inline i32x4 i32x4_xxzy(i32x4 a) { return _shufflei(a, 0, 0, 2, 1); }
inline i32x4 i32x4_xxzz(i32x4 a) { return _shufflei(a, 0, 0, 2, 2); }
inline i32x4 i32x4_xxzw(i32x4 a) { return _shufflei(a, 0, 0, 2, 3); }
inline i32x4 i32x4_xxwx(i32x4 a) { return _shufflei(a, 0, 0, 3, 0); }
inline i32x4 i32x4_xxwy(i32x4 a) { return _shufflei(a, 0, 0, 3, 1); }
inline i32x4 i32x4_xxwz(i32x4 a) { return _shufflei(a, 0, 0, 3, 2); }
inline i32x4 i32x4_xxww(i32x4 a) { return _shufflei(a, 0, 0, 3, 3); }

inline i32x4 i32x4_xyxx(i32x4 a) { return _shufflei(a, 0, 1, 0, 0); }
inline i32x4 i32x4_xyxy(i32x4 a) { return _shufflei(a, 0, 1, 0, 1); }
inline i32x4 i32x4_xyxz(i32x4 a) { return _shufflei(a, 0, 1, 0, 2); }
inline i32x4 i32x4_xyxw(i32x4 a) { return _shufflei(a, 0, 1, 0, 3); }
inline i32x4 i32x4_xyyx(i32x4 a) { return _shufflei(a, 0, 1, 1, 0); }
inline i32x4 i32x4_xyyy(i32x4 a) { return _shufflei(a, 0, 1, 1, 1); }
inline i32x4 i32x4_xyyz(i32x4 a) { return _shufflei(a, 0, 1, 1, 2); }
inline i32x4 i32x4_xyyw(i32x4 a) { return _shufflei(a, 0, 1, 1, 3); }
inline i32x4 i32x4_xyzx(i32x4 a) { return _shufflei(a, 0, 1, 2, 0); }
inline i32x4 i32x4_xyzy(i32x4 a) { return _shufflei(a, 0, 1, 2, 1); }
inline i32x4 i32x4_xyzz(i32x4 a) { return _shufflei(a, 0, 1, 2, 2); }
inline i32x4 i32x4_xyzw(i32x4 a) { return _shufflei(a, 0, 1, 2, 3); }
inline i32x4 i32x4_xywx(i32x4 a) { return _shufflei(a, 0, 1, 3, 0); }
inline i32x4 i32x4_xywy(i32x4 a) { return _shufflei(a, 0, 1, 3, 1); }
inline i32x4 i32x4_xywz(i32x4 a) { return _shufflei(a, 0, 1, 3, 2); }
inline i32x4 i32x4_xyww(i32x4 a) { return _shufflei(a, 0, 1, 3, 3); }

inline i32x4 i32x4_xzxx(i32x4 a) { return _shufflei(a, 0, 2, 0, 0); }
inline i32x4 i32x4_xzxy(i32x4 a) { return _shufflei(a, 0, 2, 0, 1); }
inline i32x4 i32x4_xzxz(i32x4 a) { return _shufflei(a, 0, 2, 0, 2); }
inline i32x4 i32x4_xzxw(i32x4 a) { return _shufflei(a, 0, 2, 0, 3); }
inline i32x4 i32x4_xzyx(i32x4 a) { return _shufflei(a, 0, 2, 1, 0); }
inline i32x4 i32x4_xzyy(i32x4 a) { return _shufflei(a, 0, 2, 1, 1); }
inline i32x4 i32x4_xzyz(i32x4 a) { return _shufflei(a, 0, 2, 1, 2); }
inline i32x4 i32x4_xzyw(i32x4 a) { return _shufflei(a, 0, 2, 1, 3); }
inline i32x4 i32x4_xzzx(i32x4 a) { return _shufflei(a, 0, 2, 2, 0); }
inline i32x4 i32x4_xzzy(i32x4 a) { return _shufflei(a, 0, 2, 2, 1); }
inline i32x4 i32x4_xzzz(i32x4 a) { return _shufflei(a, 0, 2, 2, 2); }
inline i32x4 i32x4_xzzw(i32x4 a) { return _shufflei(a, 0, 2, 2, 3); }
inline i32x4 i32x4_xzwx(i32x4 a) { return _shufflei(a, 0, 2, 3, 0); }
inline i32x4 i32x4_xzwy(i32x4 a) { return _shufflei(a, 0, 2, 3, 1); }
inline i32x4 i32x4_xzwz(i32x4 a) { return _shufflei(a, 0, 2, 3, 2); }
inline i32x4 i32x4_xzww(i32x4 a) { return _shufflei(a, 0, 2, 3, 3); }

inline i32x4 i32x4_xwxx(i32x4 a) { return _shufflei(a, 0, 3, 0, 0); }
inline i32x4 i32x4_xwxy(i32x4 a) { return _shufflei(a, 0, 3, 0, 1); }
inline i32x4 i32x4_xwxz(i32x4 a) { return _shufflei(a, 0, 3, 0, 2); }
inline i32x4 i32x4_xwxw(i32x4 a) { return _shufflei(a, 0, 3, 0, 3); }
inline i32x4 i32x4_xwyx(i32x4 a) { return _shufflei(a, 0, 3, 1, 0); }
inline i32x4 i32x4_xwyy(i32x4 a) { return _shufflei(a, 0, 3, 1, 1); }
inline i32x4 i32x4_xwyz(i32x4 a) { return _shufflei(a, 0, 3, 1, 2); }
inline i32x4 i32x4_xwyw(i32x4 a) { return _shufflei(a, 0, 3, 1, 3); }
inline i32x4 i32x4_xwzx(i32x4 a) { return _shufflei(a, 0, 3, 2, 0); }
inline i32x4 i32x4_xwzy(i32x4 a) { return _shufflei(a, 0, 3, 2, 1); }
inline i32x4 i32x4_xwzz(i32x4 a) { return _shufflei(a, 0, 3, 2, 2); }
inline i32x4 i32x4_xwzw(i32x4 a) { return _shufflei(a, 0, 3, 2, 3); }
inline i32x4 i32x4_xwwx(i32x4 a) { return _shufflei(a, 0, 3, 3, 0); }
inline i32x4 i32x4_xwwy(i32x4 a) { return _shufflei(a, 0, 3, 3, 1); }
inline i32x4 i32x4_xwwz(i32x4 a) { return _shufflei(a, 0, 3, 3, 2); }
inline i32x4 i32x4_xwww(i32x4 a) { return _shufflei(a, 0, 3, 3, 3); }

inline i32x4 i32x4_yxxx(i32x4 a) { return _shufflei(a, 1, 0, 0, 0); }
inline i32x4 i32x4_yxxy(i32x4 a) { return _shufflei(a, 1, 0, 0, 1); }
inline i32x4 i32x4_yxxz(i32x4 a) { return _shufflei(a, 1, 0, 0, 2); }
inline i32x4 i32x4_yxxw(i32x4 a) { return _shufflei(a, 1, 0, 0, 3); }
inline i32x4 i32x4_yxyx(i32x4 a) { return _shufflei(a, 1, 0, 1, 0); }
inline i32x4 i32x4_yxyy(i32x4 a) { return _shufflei(a, 1, 0, 1, 1); }
inline i32x4 i32x4_yxyz(i32x4 a) { return _shufflei(a, 1, 0, 1, 2); }
inline i32x4 i32x4_yxyw(i32x4 a) { return _shufflei(a, 1, 0, 1, 3); }
inline i32x4 i32x4_yxzx(i32x4 a) { return _shufflei(a, 1, 0, 2, 0); }
inline i32x4 i32x4_yxzy(i32x4 a) { return _shufflei(a, 1, 0, 2, 1); }
inline i32x4 i32x4_yxzz(i32x4 a) { return _shufflei(a, 1, 0, 2, 2); }
inline i32x4 i32x4_yxzw(i32x4 a) { return _shufflei(a, 1, 0, 2, 3); }
inline i32x4 i32x4_yxwx(i32x4 a) { return _shufflei(a, 1, 0, 3, 0); }
inline i32x4 i32x4_yxwy(i32x4 a) { return _shufflei(a, 1, 0, 3, 1); }
inline i32x4 i32x4_yxwz(i32x4 a) { return _shufflei(a, 1, 0, 3, 2); }
inline i32x4 i32x4_yxww(i32x4 a) { return _shufflei(a, 1, 0, 3, 3); }

inline i32x4 i32x4_yyxx(i32x4 a) { return _shufflei(a, 1, 1, 0, 0); }
inline i32x4 i32x4_yyxy(i32x4 a) { return _shufflei(a, 1, 1, 0, 1); }
inline i32x4 i32x4_yyxz(i32x4 a) { return _shufflei(a, 1, 1, 0, 2); }
inline i32x4 i32x4_yyxw(i32x4 a) { return _shufflei(a, 1, 1, 0, 3); }
inline i32x4 i32x4_yyyx(i32x4 a) { return _shufflei(a, 1, 1, 1, 0); }
inline i32x4 i32x4_yyyy(i32x4 a) { return _shufflei1(a, 1); } 
inline i32x4 i32x4_yyyz(i32x4 a) { return _shufflei(a, 1, 1, 1, 2); }
inline i32x4 i32x4_yyyw(i32x4 a) { return _shufflei(a, 1, 1, 1, 3); }
inline i32x4 i32x4_yyzx(i32x4 a) { return _shufflei(a, 1, 1, 2, 0); }
inline i32x4 i32x4_yyzy(i32x4 a) { return _shufflei(a, 1, 1, 2, 1); }
inline i32x4 i32x4_yyzz(i32x4 a) { return _shufflei(a, 1, 1, 2, 2); }
inline i32x4 i32x4_yyzw(i32x4 a) { return _shufflei(a, 1, 1, 2, 3); }
inline i32x4 i32x4_yywx(i32x4 a) { return _shufflei(a, 1, 1, 3, 0); }
inline i32x4 i32x4_yywy(i32x4 a) { return _shufflei(a, 1, 1, 3, 1); }
inline i32x4 i32x4_yywz(i32x4 a) { return _shufflei(a, 1, 1, 3, 2); }
inline i32x4 i32x4_yyww(i32x4 a) { return _shufflei(a, 1, 1, 3, 3); }

inline i32x4 i32x4_yzxx(i32x4 a) { return _shufflei(a, 1, 2, 0, 0); }
inline i32x4 i32x4_yzxy(i32x4 a) { return _shufflei(a, 1, 2, 0, 1); }
inline i32x4 i32x4_yzxz(i32x4 a) { return _shufflei(a, 1, 2, 0, 2); }
inline i32x4 i32x4_yzxw(i32x4 a) { return _shufflei(a, 1, 2, 0, 3); }
inline i32x4 i32x4_yzyx(i32x4 a) { return _shufflei(a, 1, 2, 1, 0); }
inline i32x4 i32x4_yzyy(i32x4 a) { return _shufflei(a, 1, 2, 1, 1); }
inline i32x4 i32x4_yzyz(i32x4 a) { return _shufflei(a, 1, 2, 1, 2); }
inline i32x4 i32x4_yzyw(i32x4 a) { return _shufflei(a, 1, 2, 1, 3); }
inline i32x4 i32x4_yzzx(i32x4 a) { return _shufflei(a, 1, 2, 2, 0); }
inline i32x4 i32x4_yzzy(i32x4 a) { return _shufflei(a, 1, 2, 2, 1); }
inline i32x4 i32x4_yzzz(i32x4 a) { return _shufflei(a, 1, 2, 2, 2); }
inline i32x4 i32x4_yzzw(i32x4 a) { return _shufflei(a, 1, 2, 2, 3); }
inline i32x4 i32x4_yzwx(i32x4 a) { return _shufflei(a, 1, 2, 3, 0); }
inline i32x4 i32x4_yzwy(i32x4 a) { return _shufflei(a, 1, 2, 3, 1); }
inline i32x4 i32x4_yzwz(i32x4 a) { return _shufflei(a, 1, 2, 3, 2); }
inline i32x4 i32x4_yzww(i32x4 a) { return _shufflei(a, 1, 2, 3, 3); }

inline i32x4 i32x4_ywxx(i32x4 a) { return _shufflei(a, 1, 3, 0, 0); }
inline i32x4 i32x4_ywxy(i32x4 a) { return _shufflei(a, 1, 3, 0, 1); }
inline i32x4 i32x4_ywxz(i32x4 a) { return _shufflei(a, 1, 3, 0, 2); }
inline i32x4 i32x4_ywxw(i32x4 a) { return _shufflei(a, 1, 3, 0, 3); }
inline i32x4 i32x4_ywyx(i32x4 a) { return _shufflei(a, 1, 3, 1, 0); }
inline i32x4 i32x4_ywyy(i32x4 a) { return _shufflei(a, 1, 3, 1, 1); }
inline i32x4 i32x4_ywyz(i32x4 a) { return _shufflei(a, 1, 3, 1, 2); }
inline i32x4 i32x4_ywyw(i32x4 a) { return _shufflei(a, 1, 3, 1, 3); }
inline i32x4 i32x4_ywzx(i32x4 a) { return _shufflei(a, 1, 3, 2, 0); }
inline i32x4 i32x4_ywzy(i32x4 a) { return _shufflei(a, 1, 3, 2, 1); }
inline i32x4 i32x4_ywzz(i32x4 a) { return _shufflei(a, 1, 3, 2, 2); }
inline i32x4 i32x4_ywzw(i32x4 a) { return _shufflei(a, 1, 3, 2, 3); }
inline i32x4 i32x4_ywwx(i32x4 a) { return _shufflei(a, 1, 3, 3, 0); }
inline i32x4 i32x4_ywwy(i32x4 a) { return _shufflei(a, 1, 3, 3, 1); }
inline i32x4 i32x4_ywwz(i32x4 a) { return _shufflei(a, 1, 3, 3, 2); }
inline i32x4 i32x4_ywww(i32x4 a) { return _shufflei(a, 1, 3, 3, 3); }

inline i32x4 i32x4_zxxx(i32x4 a) { return _shufflei(a, 2, 0, 0, 0); }
inline i32x4 i32x4_zxxy(i32x4 a) { return _shufflei(a, 2, 0, 0, 1); }
inline i32x4 i32x4_zxxz(i32x4 a) { return _shufflei(a, 2, 0, 0, 2); }
inline i32x4 i32x4_zxxw(i32x4 a) { return _shufflei(a, 2, 0, 0, 3); }
inline i32x4 i32x4_zxyx(i32x4 a) { return _shufflei(a, 2, 0, 1, 0); }
inline i32x4 i32x4_zxyy(i32x4 a) { return _shufflei(a, 2, 0, 1, 1); }
inline i32x4 i32x4_zxyz(i32x4 a) { return _shufflei(a, 2, 0, 1, 2); }
inline i32x4 i32x4_zxyw(i32x4 a) { return _shufflei(a, 2, 0, 1, 3); }
inline i32x4 i32x4_zxzx(i32x4 a) { return _shufflei(a, 2, 0, 2, 0); }
inline i32x4 i32x4_zxzy(i32x4 a) { return _shufflei(a, 2, 0, 2, 1); }
inline i32x4 i32x4_zxzz(i32x4 a) { return _shufflei(a, 2, 0, 2, 2); }
inline i32x4 i32x4_zxzw(i32x4 a) { return _shufflei(a, 2, 0, 2, 3); }
inline i32x4 i32x4_zxwx(i32x4 a) { return _shufflei(a, 2, 0, 3, 0); }
inline i32x4 i32x4_zxwy(i32x4 a) { return _shufflei(a, 2, 0, 3, 1); }
inline i32x4 i32x4_zxwz(i32x4 a) { return _shufflei(a, 2, 0, 3, 2); }
inline i32x4 i32x4_zxww(i32x4 a) { return _shufflei(a, 2, 0, 3, 3); }

inline i32x4 i32x4_zyxx(i32x4 a) { return _shufflei(a, 2, 1, 0, 0); }
inline i32x4 i32x4_zyxy(i32x4 a) { return _shufflei(a, 2, 1, 0, 1); }
inline i32x4 i32x4_zyxz(i32x4 a) { return _shufflei(a, 2, 1, 0, 2); }
inline i32x4 i32x4_zyxw(i32x4 a) { return _shufflei(a, 2, 1, 0, 3); }
inline i32x4 i32x4_zyyx(i32x4 a) { return _shufflei(a, 2, 1, 1, 0); }
inline i32x4 i32x4_zyyy(i32x4 a) { return _shufflei(a, 2, 1, 1, 1); }
inline i32x4 i32x4_zyyz(i32x4 a) { return _shufflei(a, 2, 1, 1, 2); }
inline i32x4 i32x4_zyyw(i32x4 a) { return _shufflei(a, 2, 1, 1, 3); }
inline i32x4 i32x4_zyzx(i32x4 a) { return _shufflei(a, 2, 1, 2, 0); }
inline i32x4 i32x4_zyzy(i32x4 a) { return _shufflei(a, 2, 1, 2, 1); }
inline i32x4 i32x4_zyzz(i32x4 a) { return _shufflei(a, 2, 1, 2, 2); }
inline i32x4 i32x4_zyzw(i32x4 a) { return _shufflei(a, 2, 1, 2, 3); }
inline i32x4 i32x4_zywx(i32x4 a) { return _shufflei(a, 2, 1, 3, 0); }
inline i32x4 i32x4_zywy(i32x4 a) { return _shufflei(a, 2, 1, 3, 1); }
inline i32x4 i32x4_zywz(i32x4 a) { return _shufflei(a, 2, 1, 3, 2); }
inline i32x4 i32x4_zyww(i32x4 a) { return _shufflei(a, 2, 1, 3, 3); }

inline i32x4 i32x4_zzxx(i32x4 a) { return _shufflei(a, 2, 2, 0, 0); }
inline i32x4 i32x4_zzxy(i32x4 a) { return _shufflei(a, 2, 2, 0, 1); }
inline i32x4 i32x4_zzxz(i32x4 a) { return _shufflei(a, 2, 2, 0, 2); }
inline i32x4 i32x4_zzxw(i32x4 a) { return _shufflei(a, 2, 2, 0, 3); }
inline i32x4 i32x4_zzyx(i32x4 a) { return _shufflei(a, 2, 2, 1, 0); }
inline i32x4 i32x4_zzyy(i32x4 a) { return _shufflei(a, 2, 2, 1, 1); }
inline i32x4 i32x4_zzyz(i32x4 a) { return _shufflei(a, 2, 2, 1, 2); }
inline i32x4 i32x4_zzyw(i32x4 a) { return _shufflei(a, 2, 2, 1, 3); }
inline i32x4 i32x4_zzzx(i32x4 a) { return _shufflei(a, 2, 2, 2, 0); }
inline i32x4 i32x4_zzzy(i32x4 a) { return _shufflei(a, 2, 2, 2, 1); }
inline i32x4 i32x4_zzzz(i32x4 a) { return _shufflei1(a, 2); }
inline i32x4 i32x4_zzzw(i32x4 a) { return _shufflei(a, 2, 2, 2, 3); }
inline i32x4 i32x4_zzwx(i32x4 a) { return _shufflei(a, 2, 2, 3, 0); }
inline i32x4 i32x4_zzwy(i32x4 a) { return _shufflei(a, 2, 2, 3, 1); }
inline i32x4 i32x4_zzwz(i32x4 a) { return _shufflei(a, 2, 2, 3, 2); }
inline i32x4 i32x4_zzww(i32x4 a) { return _shufflei(a, 2, 2, 3, 3); }

inline i32x4 i32x4_zwxx(i32x4 a) { return _shufflei(a, 2, 3, 0, 0); }
inline i32x4 i32x4_zwxy(i32x4 a) { return _shufflei(a, 2, 3, 0, 1); }
inline i32x4 i32x4_zwxz(i32x4 a) { return _shufflei(a, 2, 3, 0, 2); }
inline i32x4 i32x4_zwxw(i32x4 a) { return _shufflei(a, 2, 3, 0, 3); }
inline i32x4 i32x4_zwyx(i32x4 a) { return _shufflei(a, 2, 3, 1, 0); }
inline i32x4 i32x4_zwyy(i32x4 a) { return _shufflei(a, 2, 3, 1, 1); }
inline i32x4 i32x4_zwyz(i32x4 a) { return _shufflei(a, 2, 3, 1, 2); }
inline i32x4 i32x4_zwyw(i32x4 a) { return _shufflei(a, 2, 3, 1, 3); }
inline i32x4 i32x4_zwzx(i32x4 a) { return _shufflei(a, 2, 3, 2, 0); }
inline i32x4 i32x4_zwzy(i32x4 a) { return _shufflei(a, 2, 3, 2, 1); }
inline i32x4 i32x4_zwzz(i32x4 a) { return _shufflei(a, 2, 3, 2, 2); }
inline i32x4 i32x4_zwzw(i32x4 a) { return _shufflei(a, 2, 3, 2, 3); }
inline i32x4 i32x4_zwwx(i32x4 a) { return _shufflei(a, 2, 3, 3, 0); }
inline i32x4 i32x4_zwwy(i32x4 a) { return _shufflei(a, 2, 3, 3, 1); }
inline i32x4 i32x4_zwwz(i32x4 a) { return _shufflei(a, 2, 3, 3, 2); }
inline i32x4 i32x4_zwww(i32x4 a) { return _shufflei(a, 2, 3, 3, 3); }

inline i32x4 i32x4_wxxx(i32x4 a) { return _shufflei(a, 3, 0, 0, 0); }
inline i32x4 i32x4_wxxy(i32x4 a) { return _shufflei(a, 3, 0, 0, 1); }
inline i32x4 i32x4_wxxz(i32x4 a) { return _shufflei(a, 3, 0, 0, 2); }
inline i32x4 i32x4_wxxw(i32x4 a) { return _shufflei(a, 3, 0, 0, 3); }
inline i32x4 i32x4_wxyx(i32x4 a) { return _shufflei(a, 3, 0, 1, 0); }
inline i32x4 i32x4_wxyy(i32x4 a) { return _shufflei(a, 3, 0, 1, 1); }
inline i32x4 i32x4_wxyz(i32x4 a) { return _shufflei(a, 3, 0, 1, 2); }
inline i32x4 i32x4_wxyw(i32x4 a) { return _shufflei(a, 3, 0, 1, 3); }
inline i32x4 i32x4_wxzx(i32x4 a) { return _shufflei(a, 3, 0, 2, 0); }
inline i32x4 i32x4_wxzy(i32x4 a) { return _shufflei(a, 3, 0, 2, 1); }
inline i32x4 i32x4_wxzz(i32x4 a) { return _shufflei(a, 3, 0, 2, 2); }
inline i32x4 i32x4_wxzw(i32x4 a) { return _shufflei(a, 3, 0, 2, 3); }
inline i32x4 i32x4_wxwx(i32x4 a) { return _shufflei(a, 3, 0, 3, 0); }
inline i32x4 i32x4_wxwy(i32x4 a) { return _shufflei(a, 3, 0, 3, 1); }
inline i32x4 i32x4_wxwz(i32x4 a) { return _shufflei(a, 3, 0, 3, 2); }
inline i32x4 i32x4_wxww(i32x4 a) { return _shufflei(a, 3, 0, 3, 3); }

inline i32x4 i32x4_wyxx(i32x4 a) { return _shufflei(a, 3, 1, 0, 0); }
inline i32x4 i32x4_wyxy(i32x4 a) { return _shufflei(a, 3, 1, 0, 1); }
inline i32x4 i32x4_wyxz(i32x4 a) { return _shufflei(a, 3, 1, 0, 2); }
inline i32x4 i32x4_wyxw(i32x4 a) { return _shufflei(a, 3, 1, 0, 3); }
inline i32x4 i32x4_wyyx(i32x4 a) { return _shufflei(a, 3, 1, 1, 0); }
inline i32x4 i32x4_wyyy(i32x4 a) { return _shufflei(a, 3, 1, 1, 1); }
inline i32x4 i32x4_wyyz(i32x4 a) { return _shufflei(a, 3, 1, 1, 2); }
inline i32x4 i32x4_wyyw(i32x4 a) { return _shufflei(a, 3, 1, 1, 3); }
inline i32x4 i32x4_wyzx(i32x4 a) { return _shufflei(a, 3, 1, 2, 0); }
inline i32x4 i32x4_wyzy(i32x4 a) { return _shufflei(a, 3, 1, 2, 1); }
inline i32x4 i32x4_wyzz(i32x4 a) { return _shufflei(a, 3, 1, 2, 2); }
inline i32x4 i32x4_wyzw(i32x4 a) { return _shufflei(a, 3, 1, 2, 3); }
inline i32x4 i32x4_wywx(i32x4 a) { return _shufflei(a, 3, 1, 3, 0); }
inline i32x4 i32x4_wywy(i32x4 a) { return _shufflei(a, 3, 1, 3, 1); }
inline i32x4 i32x4_wywz(i32x4 a) { return _shufflei(a, 3, 1, 3, 2); }
inline i32x4 i32x4_wyww(i32x4 a) { return _shufflei(a, 3, 1, 3, 3); }

inline i32x4 i32x4_wzxx(i32x4 a) { return _shufflei(a, 3, 2, 0, 0); }
inline i32x4 i32x4_wzxy(i32x4 a) { return _shufflei(a, 3, 2, 0, 1); }
inline i32x4 i32x4_wzxz(i32x4 a) { return _shufflei(a, 3, 2, 0, 2); }
inline i32x4 i32x4_wzxw(i32x4 a) { return _shufflei(a, 3, 2, 0, 3); }
inline i32x4 i32x4_wzyx(i32x4 a) { return _shufflei(a, 3, 2, 1, 0); }
inline i32x4 i32x4_wzyy(i32x4 a) { return _shufflei(a, 3, 2, 1, 1); }
inline i32x4 i32x4_wzyz(i32x4 a) { return _shufflei(a, 3, 2, 1, 2); }
inline i32x4 i32x4_wzyw(i32x4 a) { return _shufflei(a, 3, 2, 1, 3); }
inline i32x4 i32x4_wzzx(i32x4 a) { return _shufflei(a, 3, 2, 2, 0); }
inline i32x4 i32x4_wzzy(i32x4 a) { return _shufflei(a, 3, 2, 2, 1); }
inline i32x4 i32x4_wzzz(i32x4 a) { return _shufflei(a, 3, 2, 2, 2); }
inline i32x4 i32x4_wzzw(i32x4 a) { return _shufflei(a, 3, 2, 2, 3); }
inline i32x4 i32x4_wzwx(i32x4 a) { return _shufflei(a, 3, 2, 3, 0); }
inline i32x4 i32x4_wzwy(i32x4 a) { return _shufflei(a, 3, 2, 3, 1); }
inline i32x4 i32x4_wzwz(i32x4 a) { return _shufflei(a, 3, 2, 3, 2); }
inline i32x4 i32x4_wzww(i32x4 a) { return _shufflei(a, 3, 2, 3, 3); }

inline i32x4 i32x4_wwxx(i32x4 a) { return _shufflei(a, 3, 3, 0, 0); }
inline i32x4 i32x4_wwxy(i32x4 a) { return _shufflei(a, 3, 3, 0, 1); }
inline i32x4 i32x4_wwxz(i32x4 a) { return _shufflei(a, 3, 3, 0, 2); }
inline i32x4 i32x4_wwxw(i32x4 a) { return _shufflei(a, 3, 3, 0, 3); }
inline i32x4 i32x4_wwyx(i32x4 a) { return _shufflei(a, 3, 3, 1, 0); }
inline i32x4 i32x4_wwyy(i32x4 a) { return _shufflei(a, 3, 3, 1, 1); }
inline i32x4 i32x4_wwyz(i32x4 a) { return _shufflei(a, 3, 3, 1, 2); }
inline i32x4 i32x4_wwyw(i32x4 a) { return _shufflei(a, 3, 3, 1, 3); }
inline i32x4 i32x4_wwzx(i32x4 a) { return _shufflei(a, 3, 3, 2, 0); }
inline i32x4 i32x4_wwzy(i32x4 a) { return _shufflei(a, 3, 3, 2, 1); }
inline i32x4 i32x4_wwzz(i32x4 a) { return _shufflei(a, 3, 3, 2, 2); }
inline i32x4 i32x4_wwzw(i32x4 a) { return _shufflei(a, 3, 3, 2, 3); }
inline i32x4 i32x4_wwwx(i32x4 a) { return _shufflei(a, 3, 3, 3, 0); }
inline i32x4 i32x4_wwwy(i32x4 a) { return _shufflei(a, 3, 3, 3, 1); }
inline i32x4 i32x4_wwwz(i32x4 a) { return _shufflei(a, 3, 3, 3, 2); }
inline i32x4 i32x4_wwww(i32x4 a) { return _shufflei1(a, 3); }

inline i32x4 i32x4_trunc2(i32x4 a);
inline i32x4 i32x4_trunc3(i32x4 a);

//2D swizzles

inline i32x4 i32x4_xx(i32x4 a) { return i32x4_trunc2(i32x4_xxxx(a)); }
inline i32x4 i32x4_xy(i32x4 a) { return i32x4_trunc2(i32x4_xyxx(a)); }
inline i32x4 i32x4_xz(i32x4 a) { return i32x4_trunc2(i32x4_xzxx(a)); }
inline i32x4 i32x4_xw(i32x4 a) { return i32x4_trunc2(i32x4_xwxx(a)); }

inline i32x4 i32x4_yx(i32x4 a) { return i32x4_trunc2(i32x4_yxxx(a)); }
inline i32x4 i32x4_yy(i32x4 a) { return i32x4_trunc2(i32x4_yyxx(a)); }
inline i32x4 i32x4_yz(i32x4 a) { return i32x4_trunc2(i32x4_yzxx(a)); }
inline i32x4 i32x4_yw(i32x4 a) { return i32x4_trunc2(i32x4_ywxx(a)); }

inline i32x4 i32x4_zx(i32x4 a) { return i32x4_trunc2(i32x4_zxxx(a)); }
inline i32x4 i32x4_zy(i32x4 a) { return i32x4_trunc2(i32x4_zyxx(a)); }
inline i32x4 i32x4_zz(i32x4 a) { return i32x4_trunc2(i32x4_zzxx(a)); }
inline i32x4 i32x4_zw(i32x4 a) { return i32x4_trunc2(i32x4_zwxx(a)); }

inline i32x4 i32x4_wx(i32x4 a) { return i32x4_trunc2(i32x4_wxxx(a)); }
inline i32x4 i32x4_wy(i32x4 a) { return i32x4_trunc2(i32x4_wyxx(a)); }
inline i32x4 i32x4_wz(i32x4 a) { return i32x4_trunc2(i32x4_wzxx(a)); }
inline i32x4 i32x4_ww(i32x4 a) { return i32x4_trunc2(i32x4_wwxx(a)); }

//3D swizzles

inline i32x4 i32x4_xxx(i32x4 a) { return i32x4_trunc3(i32x4_xxxx(a)); }
inline i32x4 i32x4_xxy(i32x4 a) { return i32x4_trunc3(i32x4_xxyx(a)); }
inline i32x4 i32x4_xxz(i32x4 a) { return i32x4_trunc3(i32x4_xxzx(a)); }
inline i32x4 i32x4_xyx(i32x4 a) { return i32x4_trunc3(i32x4_xyxx(a)); }
inline i32x4 i32x4_xyy(i32x4 a) { return i32x4_trunc3(i32x4_xyyx(a)); }
inline i32x4 i32x4_xyz(i32x4 a) { return i32x4_trunc3(i32x4_xyzx(a)); }
inline i32x4 i32x4_xzx(i32x4 a) { return i32x4_trunc3(i32x4_xzxx(a)); }
inline i32x4 i32x4_xzy(i32x4 a) { return i32x4_trunc3(i32x4_xzyx(a)); }
inline i32x4 i32x4_xzz(i32x4 a) { return i32x4_trunc3(i32x4_xzzx(a)); }

inline i32x4 i32x4_yxx(i32x4 a) { return i32x4_trunc3(i32x4_yxxx(a)); }
inline i32x4 i32x4_yxy(i32x4 a) { return i32x4_trunc3(i32x4_yxyx(a)); }
inline i32x4 i32x4_yxz(i32x4 a) { return i32x4_trunc3(i32x4_yxzx(a)); }
inline i32x4 i32x4_yyx(i32x4 a) { return i32x4_trunc3(i32x4_yyxx(a)); }
inline i32x4 i32x4_yyy(i32x4 a) { return i32x4_trunc3(i32x4_yyyx(a)); }
inline i32x4 i32x4_yyz(i32x4 a) { return i32x4_trunc3(i32x4_yyzx(a)); }
inline i32x4 i32x4_yzx(i32x4 a) { return i32x4_trunc3(i32x4_yzxx(a)); }
inline i32x4 i32x4_yzy(i32x4 a) { return i32x4_trunc3(i32x4_yzyx(a)); }
inline i32x4 i32x4_yzz(i32x4 a) { return i32x4_trunc3(i32x4_yzzx(a)); }

inline i32x4 i32x4_zxx(i32x4 a) { return i32x4_trunc3(i32x4_zxxx(a)); }
inline i32x4 i32x4_zxy(i32x4 a) { return i32x4_trunc3(i32x4_zxyx(a)); }
inline i32x4 i32x4_zxz(i32x4 a) { return i32x4_trunc3(i32x4_zxzx(a)); }
inline i32x4 i32x4_zyx(i32x4 a) { return i32x4_trunc3(i32x4_zyxx(a)); }
inline i32x4 i32x4_zyy(i32x4 a) { return i32x4_trunc3(i32x4_zyyx(a)); }
inline i32x4 i32x4_zyz(i32x4 a) { return i32x4_trunc3(i32x4_zyzx(a)); }
inline i32x4 i32x4_zzx(i32x4 a) { return i32x4_trunc3(i32x4_zzxx(a)); }
inline i32x4 i32x4_zzy(i32x4 a) { return i32x4_trunc3(i32x4_zzyx(a)); }
inline i32x4 i32x4_zzz(i32x4 a) { return i32x4_trunc3(i32x4_zzzx(a)); }

inline i32 i32x4_x(i32x4 a);
inline i32 i32x4_y(i32x4 a);
inline i32 i32x4_z(i32x4 a);
inline i32 i32x4_w(i32x4 a);

inline i32x4 i32x4_init1(i32 x);
inline i32x4 i32x4_init2(i32 x, i32 y);
inline i32x4 i32x4_init3(i32 x, i32 y, i32 z);
inline i32x4 i32x4_init4(i32 x, i32 y, i32 z, i32 w);

inline i32x4 i32x4_xxxx4(i32 x);

inline i32x4 i32x4_load4(const i32 *arr);

inline i32x4 i32x4_zero();

//Needed since cmp returns -1 as int, so we negate

inline i32x4 i32x4_eq(i32x4 a, i32x4 b);
inline i32x4 i32x4_neq(i32x4 a, i32x4 b);
inline i32x4 i32x4_geq(i32x4 a, i32x4 b);
inline i32x4 i32x4_gt(i32x4 a, i32x4 b);
inline i32x4 i32x4_leq(i32x4 a, i32x4 b);
inline i32x4 i32x4_lt(i32x4 a, i32x4 b);

inline i32x4 i32x4_or(i32x4 a, i32x4 b);
inline i32x4 i32x4_and(i32x4 a, i32x4 b);
inline i32x4 i32x4_xor(i32x4 a, i32x4 b);

inline i32x4 i32x4_min(i32x4 a, i32x4 b);
inline i32x4 i32x4_max(i32x4 a, i32x4 b);

//Obtain sign (-1 if <0, otherwise 1)
//(a < 0) * 2 + 1;
//-1 * 2 + 1: -2 + 1: -1 for < 0
//0 * 2 + 1; 0 + 1; 1 for >=0

inline i32x4 i32x4_sign(i32x4 v) {
    return i32x4_add(
        i32x4_mul(i32x4_lt(v, i32x4_zero()), i32x4_two()), 
        i32x4_one()
    ); 
}

inline i32x4 i32x4_abs(i32x4 v){ return i32x4_mul(i32x4_sign(v), v); }

inline i32 i32x4_reduce(i32x4 a);

//Simple definitions

inline i32x4 i32x4_one() { return i32x4_xxxx4(1); }
inline i32x4 i32x4_two() { return i32x4_xxxx4(2); }
inline i32x4 i32x4_negOne() { return i32x4_xxxx4(-1); }
inline i32x4 i32x4_negTwo() { return i32x4_xxxx4(-2); }
inline i32x4 i32x4_mask2() { return i32x4_init4(1, 1, 0, 0); }
inline i32x4 i32x4_mask3() { return i32x4_init4(1, 1, 1, 0); }

inline bool i32x4_all(i32x4 b) { return i32x4_reduce(i32x4_neq(b, i32x4_zero())) == 4; }
inline bool i32x4_any(i32x4 b) { return i32x4_reduce(i32x4_neq(b, i32x4_zero())); }

inline i32x4 i32x4_load1(const i32 *arr) { return i32x4_init1(*arr); }
inline i32x4 i32x4_load2(const i32 *arr) { return i32x4_init2(*arr, arr[1]); }
inline i32x4 i32x4_load3(const i32 *arr) { return i32x4_init3(*arr, arr[1], arr[2]); }

inline void i32x4_setX(i32x4 *a, i32 v) { *a = i32x4_init4(v,             i32x4_y(*a),    i32x4_z(*a),    i32x4_w(*a)); }
inline void i32x4_setY(i32x4 *a, i32 v) { *a = i32x4_init4(i32x4_x(*a),   v,              i32x4_z(*a),    i32x4_w(*a)); }
inline void i32x4_setZ(i32x4 *a, i32 v) { *a = i32x4_init4(i32x4_x(*a),   i32x4_y(*a),    v,              i32x4_w(*a)); }
inline void i32x4_setW(i32x4 *a, i32 v) { *a = i32x4_init4(i32x4_x(*a),   i32x4_y(*a),    i32x4_z(*a),    v); }

inline void i32x4_set(i32x4 *a, u8 i, i32 v) {
    switch (i & 3) {
        case 0:     i32x4_setX(a, v);     break;
        case 1:     i32x4_setY(a, v);     break;
        case 2:     i32x4_setZ(a, v);     break;
        default:    i32x4_setW(a, v);
    }
}

inline i32 i32x4_get(i32x4 a, u8 i) {
    switch (i & 3) {
        case 0:     return i32x4_x(a);
        case 1:     return i32x4_y(a);
        case 2:     return i32x4_z(a);
        default:    return i32x4_w(a);
    }
}