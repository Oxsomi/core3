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

//types/math/type_cast.h

#pragma once
#include "types/base/mathf.h"
#include "types/base/error.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Conversions

//Bit casts go through a union, not a pointer cast.
//Reading a float out of storage that was written as an integer aliases two incompatible types, which C leaves undefined.
//gcc then sees no float was ever stored there and reports "'bits' is used uninitialized", an error under -Werror.
//Union punning is well defined in C99 and is already what the vector headers do, see I32x4_U64x2.

typedef union F32_U32 {
	F32 f;
	U32 u;
} F32_U32;

typedef union F64_U64 {
	F64 f;
	U64 u;
} F64_U64;

static inline U32 U32_fromF32Bits(F32 v) {
	F32_U32 bits;
	bits.f = v;
	return bits.u;
}

static inline U64 U64_fromF64Bits(F64 v) {
	F64_U64 bits;
	bits.f = v;
	return bits.u;
}

static inline F32 F32_fromU32Bits(U32 v) {
	F32_U32 bits;
	bits.u = v;
	return bits.f;
}

static inline F64 F64_fromU64Bits(U64 v) {
	F64_U64 bits;
	bits.u = v;
	return bits.f;
}

#define FLP_FROMBITS(T, TInt)                                                                               \
static inline Bool T##_from##TInt##BitsSafe(TInt v, T *res, Bool assertFinite, Error *e_rr) {               \
																											\
	Bool s_uccess = true;                                                                                   \
	const T r = T##_from##TInt##Bits(v);                                                                    \
																											\
	if(!res)                                                                                                \
		retError(clean, Error_nullPointer(1, #T "_from" #TInt "Bits()::res is required"));                  \
																											\
	if(assertFinite && !T##_isValid(r))                                                                     \
		retError(clean, Error_NaN(0, #T "_fromBits()::v generated NaN or Inf"));                            \
																											\
	*res = r;                                                                                               \
clean:                                                                                                      \
	return s_uccess;                                                                                        \
}

FLP_FROMBITS(F32, U32);
FLP_FROMBITS(F64, U64);

#undef FLP_FROMBITS

#ifdef __cplusplus
	}
#endif
