/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//These are Pseudo RNG (PRNG), don't use these for critical uses!
//More info:
//	https://cheatsheetseries.owasp.org/cheatsheets/Cryptographic_Storage_Cheat_Sheet.html#secure-random-number-generation
//	https://github.com/TeamWisp/WispRenderer/blob/master/resources/shaders/rand_util.hlsl

static inline U32 Random_seed(U32 val0, U32 val1) {

	U32 v0 = val0, v1 = val1, s0 = 0;

	for (U32 n = 0; n < 16; n++) {
		s0 += 0x9E3779B9;
		v0 += ((v1 << 4) + 0xA341316C) ^ (v1 + s0) ^ ((v1 >> 5) + 0xC8013EA4);
		v1 += ((v0 << 4) + 0xAD90777D) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7E95761E);
	}

	return v0;
}

//Generate 'random' value [0, 1>

static inline F32 Random_sample(U32 *seed) {

	if (!seed)
		return 0;

	*seed = (1664525 * *seed + 1013904223);
	return (F32)(*seed & 0x00FFFFFF) / (F32)(0x01000000);
}

#ifdef __cplusplus
	}
#endif
