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

//types/math/vec_cvt.hpp

#pragma once
#include <type_traits>

//Pre-include system headers used by the C headers below at global scope;
//they must not be pulled in for the first time inside a namespace.

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef __APPLE__
	#include <TargetConditionals.h>            //platform_types.h reaches for this on apple
#endif

//platform_types.h is what decides _SIMD,
// so pull it into oxc::c first, its *types* belong there like every other C header's.
//The macros it defines are global regardless (macros don't see namespaces),
// which is what lets the intrinsic header be picked below, at the global scope intrinsics require.

namespace oxc { namespace c {
	#include "types/base/platform_types.h"
} }

#if _SIMD == SIMD_SSE
	#include <emmintrin.h>
	#include <smmintrin.h>
	#include <immintrin.h>
#elif _SIMD == SIMD_NEON
	#include <arm_neon.h>
#endif

#include "types/math/vec4f.hpp"
#include "types/math/vec4i.hpp"

//Conversions between oxc::F32x4 and oxc::I32x4, mirroring the C vec_cvt.h.

namespace oxc {

	namespace c {
		#include "types/math/vec_cvt.h"
	}

	//Conversions between the two vector flavours.
	//Free functions rather than members, so neither vec4f.hpp nor vec4i.hpp has to know about the other -
	//the same reason the C keeps these in vec_cvt.h instead of vec4f.h / vec4i.h.
	//
	//  cast*  reinterprets the bits (F32 1.0f <-> I32 0x3F800000)
	//  to*    converts the value    (F32 1.9f -> I32 1)

	[[nodiscard]] inline I32x4 castI32x4(const F32x4 &v) noexcept { return c::I32x4_bitsF32x4(v.handle()); }
	[[nodiscard]] inline F32x4 castF32x4(const I32x4 &v) noexcept { return c::F32x4_bitsI32x4(v.handle()); }

	[[nodiscard]] inline I32x4 toI32x4(const F32x4 &v) noexcept { return c::I32x4_fromF32x4(v.handle()); }
	[[nodiscard]] inline F32x4 toF32x4(const I32x4 &v) noexcept { return c::F32x4_fromI32x4(v.handle()); }
}
