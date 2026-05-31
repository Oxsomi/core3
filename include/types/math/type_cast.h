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

static inline U32 U32_fromF32Bits(F32 v) {
	const void *vptr = &v;
	U32 u = *(const U32*)vptr;
	return u;
}

static inline U64 U64_fromF64Bits(F64 v) {
	const void *vptr = &v;
	return *(const U64*) vptr;
}

static inline F32 F32_fromU32Bits(U32 v) {
	const void *vptr = &v;
	return *(const F32*) vptr;
}

static inline F64 F64_fromU64Bits(U64 v) {
	const void *vptr = &v;
	return *(const F64*) vptr;
}

#define FLP_FROMBITS(T, TInt)                                                                            \
static inline Bool T##_from##TInt##BitsSafe(TInt v, T *res, Bool assertFinite, Error *e_rr) {            \
																										\
	Bool s_uccess = true;                                                                                \
																										\
	if(!res)                                                                                            \
		retError(clean, Error_nullPointer(1, #T "_from" #TInt "Bits()::res is required"));                \
																										\
	TInt bits = (TInt) v;                                                                                \
	const void *bitsPtr = &bits;                                                                        \
	T r = *(const T*) bitsPtr;                                                                            \
																										\
	if(assertFinite && !T##_isValid(r))                                                                    \
		retError(clean, Error_NaN(0, #T "_fromBits()::v generated NaN or Inf"));                        \
																										\
	*res = r;                                                                                            \
clean:                                                                                                    \
	return s_uccess;                                                                                    \
}

FLP_FROMBITS(F32, U32);
FLP_FROMBITS(F64, U64);

#undef FLP_FROMBITS

#ifdef __cplusplus
	}
#endif
