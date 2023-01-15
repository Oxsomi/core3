#include "math/vec.h"
#include "types/type_cast.h"

#if _SIMD == SIMD_NONE

	//Cast

	F32x4 F32x4_fromI32x4(I32x4 a) { 
		return (F32x4) { .v = { 
			(F32) I32x4_x(a), (F32) I32x4_y(a), (F32) I32x4_z(a), (F32) I32x4_w(a) 
		} }; 
	}

	I32x4 I32x4_fromF32x4(F32x4 a) { 
		return (I32x4) { .v = { 
			(I32) F32x4_x(a), (I32) F32x4_y(a), (I32) F32x4_z(a), (I32) F32x4_w(a) 
		} }; 
	}

	F32x2 F32x2_fromI32x2(I32x2 a) { return (F32x2) { .v = { (F32) I32x2_x(a), (F32) I32x2_y(a) } }; }
	I32x2 I32x2_fromF32x2(F32x2 a) { return (I32x2) { .v = { (I32) F32x2_x(a), (I32) F32x2_y(a) } }; }

	//Arithmetic

	I32x4 I32x4_add(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] + b.v[i])
	F32x4 F32x4_add(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] + b.v[i])
	I32x2 I32x2_add(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] + b.v[i])
	F32x2 F32x2_add(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] + b.v[i])

	I32x4 I32x4_sub(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] - b.v[i])
	F32x4 F32x4_sub(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] - b.v[i])
	I32x2 I32x2_sub(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] - b.v[i])
	F32x2 F32x2_sub(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] - b.v[i])

	I32x4 I32x4_mul(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] * b.v[i])
	F32x4 F32x4_mul(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] * b.v[i])
	I32x2 I32x2_mul(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] * b.v[i])
	F32x2 F32x2_mul(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] * b.v[i])

	I32x4 I32x4_div(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] / b.v[i])
	F32x4 F32x4_div(F32x4 a, F32x4 b) _NONE_OP4F(a.v[i] / b.v[i])
	I32x2 I32x2_div(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] / b.v[i])
	F32x2 F32x2_div(F32x2 a, F32x2 b) _NONE_OP2F(a.v[i] / b.v[i])

	//Swizzle

	I32x4 I32x4_trunc2(I32x4 a) { return I32x4_create2(I32x4_x(a), I32x4_y(a)); }
	F32x4 F32x4_trunc2(F32x4 a) { return F32x4_create2(F32x4_x(a), F32x4_y(a)); }

	I32x4 I32x4_trunc3(I32x4 a) { return I32x4_create3(I32x4_x(a), I32x4_y(a), I32x4_z(a)); }
	F32x4 F32x4_trunc3(F32x4 a) { return F32x4_create3(F32x4_x(a), F32x4_y(a), F32x4_z(a)); }

	I32 I32x4_x(I32x4 a) { return a.v[0]; }
	F32 F32x4_x(F32x4 a) { return a.v[0]; }
	I32 I32x2_x(I32x2 a) { return a.v[0]; }
	F32 F32x2_x(F32x2 a) { return a.v[0]; }

	I32 I32x4_y(I32x4 a) { return a.v[1]; }
	F32 F32x4_y(F32x4 a) { return a.v[1]; }
	I32 I32x2_y(I32x2 a) { return a.v[1]; }
	F32 F32x2_y(F32x2 a) { return a.v[1]; }

	I32 I32x4_z(I32x4 a) { return a.v[2]; }
	F32 F32x4_z(F32x4 a) { return a.v[2]; }

	I32 I32x4_w(I32x4 a) { return a.v[3]; }
	F32 F32x4_w(F32x4 a) { return a.v[3]; }

	I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { return (I32x4) { .v = { x, y, z, w } }; }
	F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { return (F32x4) { .v = { x, y, z, w } }; }

	I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return I32x4_create4(x, y, z, 0); }
	F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return F32x4_create4(x, y, z, 0); }

	I32x4 I32x4_create2(I32 x, I32 y) { return I32x4_create4(x, y, 0, 0); }
	F32x4 F32x4_create2(F32 x, F32 y) { return F32x4_create4(x, y, 0, 0); }
	I32x2 I32x2_create2(I32 x, I32 y) { return (I32x2) { .v = { x, y } }; }
	F32x2 F32x2_create2(F32 x, F32 y) { return (F32x2) { .v = { x, y } }; }

	I32x4 I32x4_create1(I32 x) { return I32x4_create4(x, 0, 0, 0); }
	F32x4 F32x4_create1(F32 x) { return F32x4_create4(x, 0, 0, 0); }
	I32x2 I32x2_create1(I32 x) { return I32x2_create2(x, 0); }
	F32x2 F32x2_create1(F32 x) { return F32x2_create2(x, 0); }

	I32x4 I32x4_createFromU64x2(U64 i0, U64 i1) {
		I32x4 v;
		((U64*)&v)[0] = i0;
		((U64*)&v)[1] = i1;
		return v;
	}

	I32x4 I32x4_xxxx4(I32 x) { return I32x4_create4(x, x, x, x); }
	F32x4 F32x4_xxxx4(F32 x) { return F32x4_create4(x, x, x, x); }
	I32x2 I32x2_xx2(I32 x) { return I32x2_create2(x, x); }
	F32x2 F32x2_xx2(F32 x) { return F32x2_create2(x, x); }

	I32x4 I32x4_zero() { return I32x4_xxxx4(0); }
	I32x2 I32x2_zero() { return I32x2_xx2(0); }
	F32x4 F32x4_zero() { return F32x4_xxxx4(0); }
	F32x2 F32x2_zero() { return F32x2_xx2(0); }

	//Comparison

	I32x4 I32x4_eq(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] == b.v[i])
	I32x2 I32x2_eq(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] == b.v[i])
	F32x4 F32x4_eq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] == b.v[i]))
	F32x2 F32x2_eq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] == b.v[i]))

	I32x4 I32x4_neq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] != b.v[i])
	I32x2 I32x2_neq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] != b.v[i])
	F32x4 F32x4_neq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] != b.v[i]))
	F32x2 F32x2_neq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] != b.v[i]))

	I32x4 I32x4_geq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] >= b.v[i])
	I32x2 I32x2_geq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] >= b.v[i])
	F32x4 F32x4_geq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] >= b.v[i]))
	F32x2 F32x2_geq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] >= b.v[i]))

	I32x4 I32x4_gt(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] > b.v[i])
	I32x2 I32x2_gt(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] > b.v[i])
	F32x4 F32x4_gt(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] > b.v[i]))
	F32x2 F32x2_gt(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] > b.v[i]))

	I32x4 I32x4_leq(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] <= b.v[i])
	I32x2 I32x2_leq(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] <= b.v[i])
	F32x4 F32x4_leq(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] <= b.v[i]))
	F32x2 F32x2_leq(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] <= b.v[i]))

	I32x4 I32x4_lt(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] < b.v[i])
	I32x2 I32x2_lt(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] < b.v[i])
	F32x4 F32x4_lt(F32x4 a, F32x4 b)  _NONE_OP4F((F32)(a.v[i] < b.v[i]))
	F32x2 F32x2_lt(F32x2 a, F32x2 b)  _NONE_OP2F((F32)(a.v[i] < b.v[i]))

	//Bitwise

	I32x4 I32x4_or(I32x4 a, I32x4 b)  _NONE_OP4I(a.v[i] | b.v[i])
	I32x2 I32x2_or(I32x2 a, I32x2 b)  _NONE_OP2I(a.v[i] | b.v[i])

	I32x4 I32x4_and(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] & b.v[i])
	I32x2 I32x2_and(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] & b.v[i])

	I32x4 I32x4_xor(I32x4 a, I32x4 b) _NONE_OP4I(a.v[i] ^ b.v[i])
	I32x2 I32x2_xor(I32x2 a, I32x2 b) _NONE_OP2I(a.v[i] ^ b.v[i])

	//Min/max

	I32x4 I32x4_min(I32x4 a, I32x4 b) _NONE_OP4I((I32) I64_min(a.v[i], b.v[i]))
	I32x2 I32x2_min(I32x2 a, I32x2 b) _NONE_OP2I((I32) I64_min(a.v[i], b.v[i]))
	F32x4 F32x4_min(F32x4 a, F32x4 b) _NONE_OP4F(F32_min(a.v[i], b.v[i]))
	F32x2 F32x2_min(F32x2 a, F32x2 b) _NONE_OP2F(F32_min(a.v[i], b.v[i]))

	I32x4 I32x4_max(I32x4 a, I32x4 b) _NONE_OP4I((I32) I64_max(a.v[i], b.v[i]))
	I32x2 I32x2_max(I32x2 a, I32x2 b) _NONE_OP2I((I32) I64_max(a.v[i], b.v[i]))
	F32x4 F32x4_max(F32x4 a, F32x4 b) _NONE_OP4F(F32_max(a.v[i], b.v[i]))
	F32x2 F32x2_max(F32x2 a, F32x2 b) _NONE_OP2F(F32_max(a.v[i], b.v[i]))

	//Reduce

	I32 I32x4_reduce(I32x4 a) { return I32x4_x(a) + I32x4_y(a) + I32x4_z(a) + I32x4_w(a); }
	F32 F32x4_reduce(F32x4 a) { return F32x4_x(a) + F32x4_y(a) + F32x4_z(a) + F32x4_w(a); }

	I32 I32x2_reduce(I32x2 a) { return I32x2_x(a) + I32x2_y(a); }
	F32 F32x2_reduce(F32x2 a) { return F32x2_x(a) + F32x2_y(a); }

	//Simple swizzle

	I32x2 I32x2_xx(I32x2 a) { return I32x2_xx2(I32x2_x(a)); }
	F32x2 F32x2_xx(F32x2 a) { return F32x2_xx2(F32x2_x(a)); }

	I32x2 I32x2_yx(I32x2 a) { return I32x2_create2(I32x2_y(a), I32x2_x(a)); }
	F32x2 F32x2_yx(F32x2 a) { return F32x2_create2(F32x2_y(a), F32x2_x(a)); }

	F32x2 F32x2_yy(F32x2 a) { return F32x2_xx2(F32x2_y(a)); }
	I32x2 I32x2_yy(I32x2 a) { return I32x2_xx2(I32x2_y(a)); }

	//Float arithmetic

	F32x4 F32x4_ceil(F32x4 a) _NONE_OP4F(F32_ceil(a.v[i]))
	F32x2 F32x2_ceil(F32x2 a) _NONE_OP2F(F32_ceil(a.v[i]))

	F32x4 F32x4_floor(F32x4 a) _NONE_OP4F(F32_floor(a.v[i]))
	F32x2 F32x2_floor(F32x2 a) _NONE_OP2F(F32_floor(a.v[i]))

	F32x4 F32x4_round(F32x4 a) _NONE_OP4F(F32_round(a.v[i]))
	F32x2 F32x2_round(F32x2 a) _NONE_OP2F(F32_round(a.v[i]))

	F32x4 F32x4_sqrt(F32x4 a) _NONE_OP4F(F32_sqrt(a.v[i]))
	F32x2 F32x2_sqrt(F32x2 a) _NONE_OP2F(F32_sqrt(a.v[i]))

	F32x4 F32x4_rsqrt(F32x4 a) _NONE_OP4F(1 / F32_sqrt(a.v[i]))
	F32x2 F32x2_rsqrt(F32x2 a) _NONE_OP2F(1 / F32_sqrt(a.v[i]))

	F32x4 F32x4_acos(F32x4 a) _NONE_OP4F(F32_acos(a.v[i]))
	F32x2 F32x2_acos(F32x2 a) _NONE_OP2F(F32_acos(a.v[i]))

	F32x4 F32x4_cos(F32x4 a) _NONE_OP4F(F32_cos(a.v[i]))
	F32x2 F32x2_cos(F32x2 a) _NONE_OP2F(F32_cos(a.v[i]))

	F32x4 F32x4_asin(F32x4 a) _NONE_OP4F(F32_asin(a.v[i]))
	F32x2 F32x2_asin(F32x2 a) _NONE_OP2F(F32_asin(a.v[i]))

	F32x4 F32x4_sin(F32x4 a) _NONE_OP4F(F32_sin(a.v[i]))
	F32x2 F32x2_sin(F32x2 a) _NONE_OP2F(F32_sin(a.v[i]))

	F32x4 F32x4_atan(F32x4 a) _NONE_OP4F(F32_atan(a.v[i]))
	F32x2 F32x2_atan(F32x2 a) _NONE_OP2F(F32_atan(a.v[i]))

	F32x4 F32x4_atan2(F32x4 a, F32x4 x) _NONE_OP4F(F32_atan2(a.v[i], x.v[i]))
	F32x2 F32x2_atan2(F32x2 a, F32x2 x) _NONE_OP2F(F32_atan2(a.v[i], x.v[i]))

	F32x4 F32x4_tan(F32x4 a)  _NONE_OP4F(F32_tan(a.v[i]))
	F32x2 F32x2_tan(F32x2 a)  _NONE_OP2F(F32_tan(a.v[i]))

	F32x4 F32x4_loge(F32x4 a)  _NONE_OP4F(F32_loge(a.v[i]))
	F32x2 F32x2_loge(F32x2 a)  _NONE_OP2F(F32_loge(a.v[i]))

	F32x4 F32x4_log10(F32x4 a)  _NONE_OP4F(F32_log10(a.v[i]))
	F32x2 F32x2_log10(F32x2 a)  _NONE_OP2F(F32_log10(a.v[i]))

	F32x4 F32x4_log2(F32x4 a)  _NONE_OP4F(F32_log2(a.v[i]))
	F32x2 F32x2_log2(F32x2 a)  _NONE_OP2F(F32_log2(a.v[i]))

	//These return 0 if an invalid value was returned. TODO: Make this conform better!

	F32 F32_expeInternal(F32 v) { F32 v0 = 0; F32_expe(v, &v0); return v0; }
	F32 F32_exp10Internal(F32 v) { F32 v0 = 0; F32_exp10(v, &v0); return v0; }
	F32 F32_exp2Internal(F32 v) { F32 v0 = 0; F32_exp2(v, &v0); return v0; }

	//

	F32x4 F32x4_exp(F32x4 a) _NONE_OP4F(F32_expeInternal(a.v[i]))
	F32x2 F32x2_exp(F32x2 a) _NONE_OP2F(F32_expeInternal(a.v[i]))

	F32x4 F32x4_exp10(F32x4 a) _NONE_OP4F(F32_exp10Internal(a.v[i]))
	F32x2 F32x2_exp10(F32x2 a) _NONE_OP2F(F32_exp10Internal(a.v[i]))

	F32x2 F32x2_exp2(F32x2 a) _NONE_OP2F(F32_exp2Internal(a.v[i]))
	F32x4 F32x4_exp2(F32x4 a) _NONE_OP4F(F32_exp2Internal(a.v[i]))

	//Dot products

	F32 F32x4_dot2(F32x4 a, F32x4 b) { return F32x4_x(a) * F32x4_x(b) + F32x4_y(a) * F32x4_y(b); }
	F32 F32x4_dot3(F32x4 a, F32x4 b) { return F32x4_dot2(a, b) + F32x4_z(a) * F32x4_z(b); }
	F32 F32x4_dot4(F32x4 a, F32x4 b) { return F32x4_dot3(a, b) + F32x4_w(a) * F32x4_w(b); }

	F32 F32x2_dot(F32x2 a, F32x2 b) { return F32x4_dot2(F32x4_fromF32x2(a), F32x4_fromF32x2(b)); }

	//Encryption

	I32x4 I32x4_lsh32(I32x4 a) {
		return I32x4_create4(
			0,
			I32x4_x(a),
			I32x4_y(a),
			I32x4_z(a)
		);
	}

	I32x4 I32x4_lsh64(I32x4 a) {
		return I32x4_create4(
			0,
			0,
			I32x4_x(a),
			I32x4_y(a)
		);
	}

	I32x4 I32x4_lsh96(I32x4 a) {
		return I32x4_create4(
			0,
			0,
			0,
			I32x4_x(a)
		);
	}

	//Implemented from
	//https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf
	//https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197.pdf

	static const U8 AES_SBOX[256] = {
		0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
		0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
		0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
		0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
		0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
		0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
		0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
		0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
		0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
		0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
		0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
		0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
		0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
		0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
		0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
		0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
	};

	inline U32 AES_subWord(U32 a) {

		U32 res = 0;

		for(U8 i = 0; i < 4; ++i)
			res |= (U32)(AES_SBOX[(a >> (i * 8)) & 0xFF]) << (i * 8);
		
		return res;
	}

	inline U32 AES_rotWord(U32 a) { 
		return (a >> 8) | (a << 24);
	}
	
	I32x4 I32x4_aesKeyGenAssist(I32x4 a, U8 i) {

		if(i >= 8)
			return I32x4_zero();

		//https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aeskeygenassist_si128&ig_expand=6746,293

		U32 x1 = I32x4_y(a);
		U32 x3 = I32x4_w(a);
		U32 rcon = i ? 1 << (i - 1) : 0;

		x1 = AES_subWord(x1);
		x3 = AES_subWord(x3);

		return I32x4_create4(
			x1,
			AES_rotWord(x1) ^ rcon,
			x3,
			AES_rotWord(x3) ^ rcon
		);
	}

	typedef struct U8x4x4 {
		U8 v[4][4];
	} U8x4x4;

	inline U8x4x4 U8x4x4_transpose(const U8x4x4 *r) {

		U8x4x4 t = *r;

		for(U8 i = 0; i < 4; ++i)
			for(U8 j = 0; j < 4; ++j)
				t.v[i][j] = r->v[j][i];

		return t;
	}

	inline I32x4 AES_shiftRows(I32x4 a) {

		U8x4x4 *ap = (U8x4x4*) &a;

		U8x4x4 res = *ap;

		for(U64 j = 0; j < 4; ++j)
			for(U64 i = 1; i < 4; ++i)
				res.v[j][i] = ap->v[(i + j) & 3][i];

		return *(I32x4*)&res;
	}

	inline I32x4 AES_subBytes(I32x4 a) {

		I32x4 res = a;
		U8 *ptr = (U8*)&res;
	
		for(U8 i = 0; i < 16; ++i)
			ptr[i] = AES_SBOX[ptr[i]];

		return res;
	}

	inline U8 AES_g2_8(U8 v, U8 mul) {
		switch (mul) {
			case 2:		return (v << 1) ^ ((v >> 7) * 0x1B);
			case 3:		return v ^ AES_g2_8(v, 2);
			default:	return v;
		}
	}

	static U8 AES_MIX_COLUMN[4][4] = {
		{ 2, 3, 1, 1 },
		{ 1, 2, 3, 1 },
		{ 1, 1, 2, 3 },
		{ 3, 1, 1, 2 }
	};

	inline I32x4 AES_mixColumns(I32x4 vvv) { 

		U8x4x4 v = *(U8x4x4*)&vvv;

		v = U8x4x4_transpose(&v);

		U8x4x4 r = { 0 };

		for(U8 i = 0; i < 4; ++i) 
			for(U8 j = 0; j < 4; ++j) {

				for(U8 k = 0; k < 4; ++k)
					r.v[j][i] ^= AES_g2_8(v.v[k][i], AES_MIX_COLUMN[j][k]);
			}

		r = U8x4x4_transpose(&r);

		return *(const I32x4*)&r;
	}

	I32x4 I32x4_aesEnc(I32x4 a, I32x4 b, Bool isLast) {

		I32x4 t = AES_shiftRows(a);
		t = AES_subBytes(t);

		if(!isLast)
			t = AES_mixColumns(t);

		return I32x4_xor(t, b);
	}

	//SHA256 helper functions

	I32x4 I32x4_shuffleBytes(I32x4 a, I32x4 b) {
	
		U8 *ua = (U8*)&a;
		U8 *ub = (U8*)&b;
		U8 uc[16];

		for (U8 i = 0; i < 16; ++i) {

			if(ub[i] >> 7)
				uc[i] = 0;

			else uc[i] = ua[ub[i] & 0xF];
		}
	
		return *(const I32x4*)uc;
	}
	
	I32x4 I32x4_blend(I32x4 a, I32x4 b, U8 xyzw) {

		for(U8 i = 0; i < 4; ++i)
			if((xyzw >> i) & 1)
				I32x4_set(&a, i, I32x4_get(b, i));

		return a;
	}

	I32x4 I32x4_combineRightShift(I32x4 a, I32x4 b, U8 v) {

		switch (v) {

			case 0:		return b;
			case 1:		return I32x4_create4(I32x4_w(a), I32x4_x(b), I32x4_y(b), I32x4_z(b));
			case 2:		return I32x4_create4(I32x4_z(a), I32x4_w(a), I32x4_x(b), I32x4_y(b));
			case 3:		return I32x4_create4(I32x4_y(a), I32x4_z(a), I32x4_w(a), I32x4_x(b));

			case 4:		return a;
			case 5:		return I32x4_create4(0, I32x4_x(a), I32x4_y(a), I32x4_z(a));
			case 6:		return I32x4_create4(0, 0, I32x4_x(a), I32x4_y(a));
			case 7:		return I32x4_create4(0, 0, 0, I32x4_x(a));

			default:	return I32x4_zero();
		}
	}

#endif
