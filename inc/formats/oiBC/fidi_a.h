/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//reg is a 2-bit register.

#define FidiA_unwrapReg(T, prefix)						\
	EFidiA_##T##_##prefix##0 = EFidiA_##T(0),			\
	EFidiA_##T##_##prefix##1 = EFidiA_##T(1),			\
	EFidiA_##T##_##prefix##2 = EFidiA_##T(2),			\
	EFidiA_##T##_##prefix##3 = EFidiA_##T(3)

#define EFidiA_add(reg) (0x00 + ((reg) & 3))
#define EFidiA_sub(reg) (0x04 + ((reg) & 3))
#define EFidiA_mul(reg) (0x08 + ((reg) & 3))

#define EFidiA_swap(reg) (0x10 + ((reg) & 3))
#define EFidiA_cmp(reg)  (0x14 + ((reg) & 3))
#define EFidiA_load(reg) (0x18 + ((reg) & 3))

#define EFidiA_addv(reg) (0x40 + ((reg) & 3))
#define EFidiA_subv(reg) (0x44 + ((reg) & 3))
#define EFidiA_mulv(reg) (0x48 + ((reg) & 3))

#define EFidiA_swapv(reg) (0x50 + ((reg) & 3))
#define EFidiA_cmpv(reg)  (0x54 + ((reg) & 3))
#define EFidiA_loadv(reg) (0x58 + ((reg) & 3))

//Constant opcodes

#define EFidiA_expandx40(T, a, b, c)		 \
											 \
	/* Line 0x00 - 0x0F */					 \
											 \
	FidiA_unwrapReg(add##T, f##T),			 \
	FidiA_unwrapReg(sub##T, f##T),			 \
	FidiA_unwrapReg(mul##T, f##T),			 \
											 \
	EFidiA_div##T,							 \
	EFidiA_mod##T,							 \
	EFidiA_isfinite##T,						 \
	EFidiA_isnan##T,						 \
											 \
	/* Line 0x10 - 0x1F */					 \
											 \
	FidiA_unwrapReg(swap##T, f),			 \
	FidiA_unwrapReg(cmp##T, f),				 \
	FidiA_unwrapReg(load##T, f),			 \
											 \
	EFidiA_max##T,							 \
	EFidiA_min##T,							 \
	EFidiA_##a,								 \
	EFidiA_##b,								 \
											 \
	/* Line 0x20 - 0x2F */					 \
											 \
	EFidiA_negate##T,						 \
	EFidiA_abs##T,							 \
	EFidiA_inverse##T,						 \
	EFidiA_sign##T,							 \
											 \
	EFidiA_ceil##T,							 \
	EFidiA_floor##T,						 \
	EFidiA_round##T,						 \
	EFidiA_fract##T,						 \
											 \
	EFidiA_sqrt##T,							 \
	EFidiA_pow2##T,							 \
											 \
	EFidiA_loge##T,							 \
	EFidiA_log2##T,							 \
	EFidiA_log10##T,						 \
	EFidiA_exp##T,							 \
	EFidiA_exp2##T,							 \
	EFidiA_exp10##T,						 \
											 \
	/* Line 0x30 - 0x3F */					 \
											 \
	EFidiA_sin##T,							 \
	EFidiA_cos##T,							 \
	EFidiA_tan##T,							 \
											 \
	EFidiA_asin##T,							 \
	EFidiA_acos##T,							 \
	EFidiA_atan##T,							 \
	EFidiA_atan2##T,						 \
											 \
	EFidiA_sat##T,							 \
	EFidiA_clamp##T,						 \
	EFidiA_lerp##T,							 \
	EFidiA_zero##T,							 \
	EFidiA_pow##T,							 \
											 \
	EFidiA_##c##etX##T,						 \
	EFidiA_##c##etY##T,						 \
	EFidiA_##c##etZ##T,						 \
	EFidiA_##c##etW##T

typedef enum EFidiA {						//U8
	EFidiA_expandx40(, anyFv, allFv, g),
	EFidiA_expandx40(v, or, and, s)
} EFidiA;

#ifdef __cplusplus
	}
#endif
