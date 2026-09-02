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

//types/math/vec4.h

#pragma once
#include "types/math/vec_base.h"
#include "types/base/buffer_base.h"
#include "types/base/platform_types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Helper function to expand switch case

#define FUNC_EXPAND2(offset, func, var)                        \
		case offset:        return func(var, offset);        \
		case offset + 1:    return func(var, offset + 1)

#define FUNC_EXPAND4(offset, func, var) FUNC_EXPAND2(offset, func, var); FUNC_EXPAND2((offset) + 2, func, var)
#define FUNC_EXPAND8(offset, func, var) FUNC_EXPAND4(offset, func, var); FUNC_EXPAND4((offset) + 4, func, var)
#define FUNC_EXPAND16(offset, func, var) FUNC_EXPAND8(offset, func, var); FUNC_EXPAND8((offset) + 8, func, var)
#define FUNC_EXPAND32(offset, func, var) FUNC_EXPAND16(offset, func, var); FUNC_EXPAND16((offset) + 16, func, var)
#define FUNC_EXPAND64(offset, func, var) FUNC_EXPAND32(offset, func, var); FUNC_EXPAND32((offset) + 32, func, var)

#if _SIMD == SIMD_SSE
	#define VEC4_SSE_GUARD
	#include "types/math/vec4_sse.inc.h"
#elif _SIMD == SIMD_WASM
	#define VEC4_WASM_GUARD
	#include "types/math/vec4_wasm.inc.h"
#elif _SIMD == SIMD_NEON
	#define VEC4_NEON_GUARD
	#include "types/math/vec4_neon.inc.h"
#else
	#define VEC4_NONE_GUARD
	#include "types/math/vec4_none.inc.h"
#endif

#ifdef __cplusplus
		}
#endif
