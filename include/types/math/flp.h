/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//types/math/flp.h

#pragma once
#include "types/base/types.h"

#define EFloatType_make(exponentBits, mantissaBits, bytes)            \
	(((bytes) << 16) | ((exponentBits) << 8) | ((mantissaBits) << 0))

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EFloatType {

	EFloatType_F8            = EFloatType_make( 4,   3,  1),
	EFloatType_F16            = EFloatType_make( 5,  10,  2),
	EFloatType_F32            = EFloatType_make( 8,  23,  4),
	EFloatType_F64            = EFloatType_make(11,  52,  8),
	//EFloatType_Quadruple    = EFloatType_make(15, 112, 16),
	//EFloatType_Octuple    = EFloatType_make(19, 236, 32),

	EFloatType_BF16            = EFloatType_make( 8,   7,  2),            //BFloat
	EFloatType_TF19            = EFloatType_make( 8,  10,  4),            //TensorFloat
	EFloatType_PXR24        = EFloatType_make( 8,  15,  4),
	EFloatType_FP24            = EFloatType_make( 7,  16,  4)

} EFloatType;

static inline U8 EFloatType_bytes(EFloatType type) {
	return (U8)(type >> 16);
}

static inline U8 EFloatType_exponentBits(EFloatType type) {
	return (U8)(type >> 8);
}

static inline U8 EFloatType_mantissaBits(EFloatType type) {
	return (U8) type;
}

static inline U64 EFloatType_mantissaShift(EFloatType type) {
	(void)type;
	return 0;
}

static inline U64 EFloatType_exponentShift(EFloatType type) {
	return EFloatType_mantissaShift(type) + EFloatType_mantissaBits(type);
}

static inline U64 EFloatType_signShift(EFloatType type) {
	return EFloatType_exponentShift(type) + EFloatType_exponentBits(type);
}

static inline U64 EFloatType_signMask(EFloatType type) {
	return (U64)1 << EFloatType_signShift(type);
}

static inline U64 EFloatType_exponentMask(EFloatType type) {            //Not shifted
	return ((U64)1 << EFloatType_exponentBits(type)) - 1;
}

static inline U64 EFloatType_mantissaMask(EFloatType type) {            //Not shifted
	return ((U64)1 << EFloatType_mantissaBits(type)) - 1;
}

static inline Bool EFloatType_sign(EFloatType type, U64 v) {
	return v & EFloatType_signMask(type);
}

static inline U64 EFloatType_abs(EFloatType type, U64 v) {
	return v &~ EFloatType_signMask(type);
}

static inline U64 EFloatType_negate(EFloatType type, U64 v) {
	return v ^ EFloatType_signMask(type);
}

static inline U64 EFloatType_exponent(EFloatType type, U64 v) {
	return (v >> EFloatType_exponentShift(type)) & EFloatType_exponentMask(type);
}

static inline U64 EFloatType_mantissa(EFloatType type, U64 v) {
	return (v >> EFloatType_mantissaShift(type)) & EFloatType_mantissaMask(type);
}

static inline U64 EFloatType_isFinite(EFloatType type, U64 v) {
	return EFloatType_exponent(type, v) != EFloatType_exponentMask(type);
}

static inline U64 EFloatType_isDeN(EFloatType type, U64 v) {
	return !EFloatType_exponent(type, v);
}

static inline U64 EFloatType_isNaN(EFloatType type, U64 v) {
	return !EFloatType_isFinite(type, v) && EFloatType_mantissa(type, v);
}

static inline U64 EFloatType_isInf(EFloatType type, U64 v) {
	return !EFloatType_isFinite(type, v) && !EFloatType_mantissa(type, v);
}

static inline U64 EFloatType_isZero(EFloatType type, U64 v) {
	return !EFloatType_abs(type, v);
}

U64 EFloatType_convert(EFloatType type, U64 v, EFloatType conversionType);

//Auto-generated software float casts

typedef U8 F8;
typedef U16 F16;
typedef U16 BF16;
typedef U32 TF19;
typedef U32 PXR24;
typedef U32 FP24;

#define EFloatType_cast1(a, b)                                        \
static inline a b##_cast##a(b v) {                                    \
																	\
	U64 v64;                                                        \
	const void *vptr = &v;                                            \
																	\
	switch (EFloatType_bytes(EFloatType_##b)) {                        \
		case 2:        v64 = *(const U16*) vptr;        break;            \
		case 4:        v64 = *(const U32*) vptr;        break;            \
		case 8:        v64 = *(const U64*) vptr;        break;            \
		default:    v64 = *(const U8*) vptr;        break;            \
	}                                                                \
																	\
	v64 = EFloatType_convert(EFloatType_##b, v64, EFloatType_##a);    \
																	\
	vptr = &v64;                                                    \
	return *(const a*)vptr;                                            \
}

#define EFloatType_cast(a)        \
EFloatType_cast1(F8, a);        \
EFloatType_cast1(F16, a);        \
EFloatType_cast1(F32, a);        \
EFloatType_cast1(F64, a);        \
EFloatType_cast1(BF16, a);        \
EFloatType_cast1(TF19, a);        \
EFloatType_cast1(PXR24, a);        \
EFloatType_cast1(FP24, a);

EFloatType_cast(F8);
EFloatType_cast(F16);
EFloatType_cast(F32);
EFloatType_cast(F64);
EFloatType_cast(BF16);
EFloatType_cast(TF19);
EFloatType_cast(PXR24);
EFloatType_cast(FP24);

#undef EFloatType_cast1
#undef EFloatType_cast

#ifdef __cplusplus
	}
#endif
