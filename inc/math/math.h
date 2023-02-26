/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

typedef struct Error Error;

extern const F32 F32_E;
extern const F32 F32_PI;
extern const F32 F32_RAD_TO_DEG;
extern const F32 F32_DEG_TO_RAD;

//Math errors assume inputs aren't nan or inf
//Ensure it's true with extra validation
//But math operations can't create them

//Uint
//TODO: Errors

U64 U64_min(U64 v0, U64 v1);
U64 U64_max(U64 v0, U64 v1);
U64 U64_clamp(U64 v, U64 mi, U64 ma);

U64 U64_pow2(U64 v);
U64 U64_pow3(U64 v);
U64 U64_pow4(U64 v);
U64 U64_pow5(U64 v);
U64 U64_10pow(U64 v);

//TODO: Int, uint %/^*+-

//Int

I64 I64_min(I64 v0, I64 v1);
I64 I64_max(I64 v0, I64 v1);
I64 I64_clamp(I64 v, I64 mi, I64 ma);

I64 I64_abs(I64 v);

//TODO: proper safety checks, because this doesn't properly check overflow!

I64 I64_pow2(I64 v);
I64 I64_pow3(I64 v);
I64 I64_pow4(I64 v);
I64 I64_pow5(I64 v);

//Float
//TODO: %/^+-
//		Should also check if the ++ and -- actually increased the float. If not, throw! +- etc can check on lost precision (e.g. 1% of value)
//TODO: Proper error checking!

F32 F32_min(F32 v0, F32 v1);
F32 F32_max(F32 v0, F32 v1);
F32 F32_clamp(F32 v, F32 mi, F32 ma);
F32 F32_saturate(F32 v);

//TODO: Lerp perc should be 0,1

F32 F32_lerp(F32 a, F32 b, F32 perc);
F32 F32_abs(F32 v);
F32 F32_sqrt(F32 v);

//Inputs should always return false for the runtime, since floats should be validated before they're made
//As such, these won't be exposed to any user, only natively

Bool F32_isNaN(F32 v);
Bool F32_isInf(F32 v);
Bool F32_isValid(F32 v);

//

Error F32_pow2(F32 v, F32 *res);
Error F32_pow3(F32 v, F32 *res);
Error F32_pow4(F32 v, F32 *res);
Error F32_pow5(F32 v, F32 *res);

Error F32_pow(F32 v, F32 exp, F32 *res);
Error F32_expe(F32 v, F32 *res);
Error F32_exp2(F32 v, F32 *res);
Error F32_exp10(F32 v, F32 *res);

F32 F32_log10(F32 v);
F32 F32_loge(F32 v);
F32 F32_log2(F32 v);

F32 F32_asin(F32 v);
F32 F32_sin(F32 v);
F32 F32_cos(F32 v);
F32 F32_acos(F32 v);
F32 F32_tan(F32 v);
F32 F32_atan(F32 v);
F32 F32_atan2(F32 y, F32 x);

F32 F32_round(F32 v);
F32 F32_ceil(F32 v);
F32 F32_floor(F32 v);
F32 F32_fract(F32 v);

Error F32_mod(F32 v, F32 mod, F32 *result);

F32 F32_sign(F32 v);
F32 F32_signInc(F32 v);
