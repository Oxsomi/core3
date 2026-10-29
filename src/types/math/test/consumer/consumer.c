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

//types/math/test/consumer/consumer.c

//The consumer requests debug headers even when linking a release library.
//The library's exported frame count must determine the Error layout.

#ifdef NDEBUG
	#undef NDEBUG
#endif

#include "types/base/error.h"
#include "types/math/mat.h"
#include "types/math/vec4i.h"

static_assert(_ENABLE_SIMD == OXC3_EXPECTED_SIMD, "Consumer SIMD ABI differs from the library");
static_assert(_OXC3_ERROR_STACKTRACE == OXC3_EXPECTED_ERROR_FRAMES, "Consumer Error ABI differs from the library");

int main() {

	struct {
		Error error;
		U64 canary;
	} guard = { .canary = 0x123456789ABCDEF0ULL };

	guard.error = Error_invalidParameter(17, 29, "consumer ABI test");

	if(guard.error.paramId != 17 || guard.error.errorSubId != 29 || guard.canary != 0x123456789ABCDEF0ULL)
		return 1;

	const F32x4 vector = F32x4_create4(1, 2, 3, 4);

	if(F32x4_dot4(vector, vector) != 30)
		return 2;

	const I32x4 shifted = I32x4_lsh32(I32x4_create4(1, 2, 3, 4), 2);

	if(!I32x4_eq4(shifted, I32x4_create4(4, 8, 12, 16)))
		return 3;

	F32x4x4 inverse;

	if(!F32x4x4_inverse(F32x4x4_scale3(2, 4, 8), &inverse))
		return 4;

	const F32x4 scaled = F32x4x4_transformPoint(inverse, F32x4_create3(2, 4, 8));

	if(!F32x4_eqApprox4(scaled, F32x4_create4(1, 1, 1, 1)))
		return 5;

	volatile F32 inputs[] = { 0x1.000002p0f, 0x1.fffffcp-1f, -1 };
	const F32 a = inputs[0], b = inputs[1], c = inputs[2];
	const F32x4 fused = F32x4_fma(F32x4_xxxx4(a), F32x4_xxxx4(b), F32x4_xxxx4(c));

	return F32x4_x(fused) == -0x1p-46f ? 0 : 6;
}
