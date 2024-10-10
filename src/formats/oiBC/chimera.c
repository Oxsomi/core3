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

#include "formats/oiBC/chimera.h"
#include "types/base/error.h"
#include "types/math/math.h"

void Chimera_swapF32(F32 *a, F32 *b) {
	const F32 tmp = *a;
	*a = *b;
	*b = tmp;
}

void Chimera_cmp(Chimera *chim, const F32 v) {
	const ECompareResult res = v < 0 ? ECompareResult_Lt : (v > 0 ? ECompareResult_Gt : ECompareResult_Eq);
	chim->effects &=~ 3;
	chim->effects |= res;
}

ECompareResult Chimera_getLastCompare(const Chimera *chim) {
	return !chim ? ECompareResult_Eq : (ECompareResult)(chim->effects & 3);
}

void Chimera_stepFidiA(Chimera *chim, const EFidiA op) {

	if(!chim)
		return;

	//NaN for invalid operations (all exponent bits are set, and first mantissa bit)

	const U32 NaNu =
		(((U32)EFloatType_exponentMask(EFloatType_F32) << 1) | 1) <<
		((U32)EFloatType_exponentShift(EFloatType_F32) - 1);

	const void *NaNuptr = &NaNu;
	const F32 NaN = *(const F32*) NaNuptr;
	const U8 reg = op & 3;					//Doesn't always operate on this register though

	//Simple float and vector operations

	switch(op) {

		default:
			return;

		//Float functions:

		//Arithmetic and swap functions

		case EFidiA_add_f0: case EFidiA_add_f1: case EFidiA_add_f2: case EFidiA_add_f3:
			chim->f[4] += chim->f[reg];
			return;

		case EFidiA_sub_f0: case EFidiA_sub_f1: case EFidiA_sub_f2: case EFidiA_sub_f3:
			chim->f[4] -= chim->f[reg];
			return;

		case EFidiA_mul_f0: case EFidiA_mul_f1: case EFidiA_mul_f2: case EFidiA_mul_f3:
			chim->f[4] *= chim->f[reg];
			return;

		case EFidiA_swap_f0: case EFidiA_swap_f1: case EFidiA_swap_f2: case EFidiA_swap_f3:
			Chimera_swapF32(chim->f + 4, chim->f + reg);
			return;

		case EFidiA_cmp_f0: case EFidiA_cmp_f1: case EFidiA_cmp_f2: case EFidiA_cmp_f3:
			Chimera_cmp(chim, chim->f[4] - chim->f[reg]);
			return;

		case EFidiA_load_f0: case EFidiA_load_f1: case EFidiA_load_f2: case EFidiA_load_f3:
			chim->f[4] = chim->f[reg];
			return;

		//Functions that don't operate on a specific register (Af, Afv and/or f0 implied)

		case EFidiA_div:
			chim->f[4] /= chim->f[0];
			return;

		case EFidiA_mod:

			if(F32_mod(chim->f[4], chim->f[0], &chim->f[4]).genericError)
				chim->f[4] = NaN;

			return;

		case EFidiA_max:
			chim->f[4] = F32_max(chim->f[4], chim->f[0]);
			return;

		case EFidiA_min:
			chim->f[4] = F32_min(chim->f[4], chim->f[0]);
			return;

		//Bool functions return into the compare register

		case EFidiA_isfinite:
			Chimera_cmp(chim, F32_isValid(chim->f[4]));
			return;

		case EFidiA_isnan:
			Chimera_cmp(chim, F32_isNaN(chim->f[4]));
			return;

		case EFidiA_anyFv:
			Chimera_cmp(chim, F32x4_any(chim->vf[4]));
			return;

		case EFidiA_allFv:
			Chimera_cmp(chim, F32x4_all(chim->vf[4]));
			return;

		//Trig, some arithmetic, rounding and some misc ops
		//All operating on hardcoded registers, such as Af, f0, f1 and Afv.


		//More advanced vector operations as well as control flow
	}
}
