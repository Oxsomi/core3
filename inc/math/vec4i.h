#pragma once

//TODO: Error checking

F32x4 F32x4_bitsI32x4(I32x4 a);
I32x4 I32x4_bitsF32x4(F32x4 a);

//Arithmetic

impl I32x4 I32x4_zero();
I32x4 I32x4_one();
I32x4 I32x4_two();
I32x4 I32x4_negTwo();
I32x4 I32x4_negOne();
impl I32x4 I32x4_xxxx4(I32 x);

impl I32x4 I32x4_add(I32x4 a, I32x4 b);
impl I32x4 I32x4_sub(I32x4 a, I32x4 b);
impl I32x4 I32x4_mul(I32x4 a, I32x4 b);
impl I32x4 I32x4_div(I32x4 a, I32x4 b);

I32x4 I32x4_complement(I32x4 a);
I32x4 I32x4_negate(I32x4 a);

I32x4 I32x4_pow2(I32x4 a);

I32x4 I32x4_sign(I32x4 v);			//Zero counts as signed too
I32x4 I32x4_abs(I32x4 v);
I32x4 I32x4_mod(I32x4 v, I32x4 d);

impl I32 I32x4_reduce(I32x4 a);		//ax+ay+az+aw

impl I32x4 I32x4_min(I32x4 a, I32x4 b);
impl I32x4 I32x4_max(I32x4 a, I32x4 b);
I32x4 I32x4_clamp(I32x4 a, I32x4 mi, I32x4 ma);

//Boolean

Bool I32x4_all(I32x4 a);
Bool I32x4_any(I32x4 b);

impl I32x4 I32x4_eq(I32x4 a, I32x4 b);
impl I32x4 I32x4_neq(I32x4 a, I32x4 b);
impl I32x4 I32x4_geq(I32x4 a, I32x4 b);
impl I32x4 I32x4_gt(I32x4 a, I32x4 b);
impl I32x4 I32x4_leq(I32x4 a, I32x4 b);
impl I32x4 I32x4_lt(I32x4 a, I32x4 b);

impl I32x4 I32x4_or(I32x4 a, I32x4 b);
impl I32x4 I32x4_and(I32x4 a, I32x4 b);
impl I32x4 I32x4_xor(I32x4 a, I32x4 b);

Bool I32x4_eq4(I32x4 a, I32x4 b);
Bool I32x4_neq4(I32x4 a, I32x4 b);

//Swizzles and shizzle

I32 I32x4_x(I32x4 a);
I32 I32x4_y(I32x4 a);
I32 I32x4_z(I32x4 a);
I32 I32x4_w(I32x4 a);
I32 I32x4_get(I32x4 a, U8 i);

void I32x4_setX(I32x4 *a, I32 v);
void I32x4_setY(I32x4 *a, I32 v);
void I32x4_setZ(I32x4 *a, I32 v);
void I32x4_setW(I32x4 *a, I32 v);
void I32x4_set(I32x4 *a, U8 i, I32 v);

//Construction

impl I32x4 I32x4_create1(I32 x);
impl I32x4 I32x4_create2(I32 x, I32 y);
impl I32x4 I32x4_create3(I32 x, I32 y, I32 z);
impl I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w);

impl I32x4 I32x4_createFromU64x2(U64 a, U64 b);
impl I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw);				//xyzw: 4-bit selector. x as b0, w as b3. 1 means b, 0 means 0.
impl I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v);		//Appends a before b and shifts with v elements (I32) and truncates.

impl I32x4 I32x4_lsh32(I32x4 a);	//Shifting 32 bits (4 bytes)
impl I32x4 I32x4_lsh64(I32x4 a);	//Shifting 64 bits (8 bytes)
impl I32x4 I32x4_lsh96(I32x4 a);	//Shifting 96 bits (12 bytes)

impl I32x4 I32x4_aesKeyGenAssist(I32x4 a, U8 i);		//i: [0,7]. 0x0,0x1,...
impl I32x4 I32x4_aesEnc(I32x4 a, I32x4 b, Bool isLast);

I32x4 I32x4_load1(const I32 *arr);
I32x4 I32x4_load2(const I32 *arr);
I32x4 I32x4_load3(const I32 *arr);
I32x4 I32x4_load4(const I32 *arr);

I32x4 I32x4_swapEndianness(I32x4 v);

//4D swizzles

#define _I32x4_expand4(xv, yv, zv, wv) I32x4 I32x4_##xv##yv##zv##wv(I32x4 a);

#define _I32x4_expand3(...)											\
_I32x4_expand4(__VA_ARGS__, x); _I32x4_expand4(__VA_ARGS__, y);		\
_I32x4_expand4(__VA_ARGS__, z); _I32x4_expand4(__VA_ARGS__, w);

#define _I32x4_expand2(...)											\
_I32x4_expand3(__VA_ARGS__, x); _I32x4_expand3(__VA_ARGS__, y);		\
_I32x4_expand3(__VA_ARGS__, z); _I32x4_expand3(__VA_ARGS__, w);

#define _I32x4_expand(...)											\
_I32x4_expand2(__VA_ARGS__, x); _I32x4_expand2(__VA_ARGS__, y);		\
_I32x4_expand2(__VA_ARGS__, z); _I32x4_expand2(__VA_ARGS__, w);

_I32x4_expand(x);
_I32x4_expand(y);
_I32x4_expand(z);
_I32x4_expand(w);

impl I32x4 I32x4_trunc2(I32x4 a);
impl I32x4 I32x4_trunc3(I32x4 a);

//2D swizzles

#define _I32x2_expand2(xv, yv) I32x4 I32x4_##xv##yv##4(I32x4 a); I32x2 I32x4_##xv##yv(I32x4 a);

#define _I32x2_expand(...)										\
_I32x2_expand2(__VA_ARGS__, x); _I32x2_expand2(__VA_ARGS__, y); \
_I32x2_expand2(__VA_ARGS__, z); _I32x2_expand2(__VA_ARGS__, w);

_I32x2_expand(x);
_I32x2_expand(y);
_I32x2_expand(z);
_I32x2_expand(w);

//3D swizzles

#define _I32x3_expand3(xv, yv, zv) I32x4 I32x4_##xv##yv##zv(I32x4 a);

#define _I32x3_expand2(...)										\
_I32x3_expand3(__VA_ARGS__, x); _I32x3_expand3(__VA_ARGS__, y); \
_I32x3_expand3(__VA_ARGS__, z); _I32x3_expand3(__VA_ARGS__, w); 

#define _I32x3_expand(...)										\
_I32x3_expand2(__VA_ARGS__, x); _I32x3_expand2(__VA_ARGS__, y); \
_I32x3_expand2(__VA_ARGS__, z); _I32x3_expand2(__VA_ARGS__, w); 

_I32x3_expand(x);
_I32x3_expand(y);
_I32x3_expand(z);
_I32x3_expand(w);

//Shuffling bytes

I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b);		//Shuffle bytes around. Useful for changing endianness for example
