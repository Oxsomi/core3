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

//types/math/vec_cvt.h

#pragma once
#include "types/math/vec4f.h"
#include "types/math/vec4i.h"
#include "types/math/vec2f.h"
#include "types/math/vec2i.h"

#ifdef __cplusplus
	extern "C" {
#endif

static inline I32x2 I32x2_fromI32x4(I32x4 a) { return I32x2_load2(&a); }
static inline I32x4 I32x4_fromI32x2(I32x2 a) { return I32x4_load2(&a); }

//Cast from vec2f to vec4

static inline I32x4 I32x4_create2_2(I32x2 a, I32x2 b) { return I32x4_create4(I32x2_x(a), I32x2_y(a), I32x2_x(b), I32x2_y(b)); }

static inline I32x4 I32x4_create2_1_1(I32x2 a, I32 b, I32 c) { return I32x4_create4(I32x2_x(a), I32x2_y(a), b, c); }
static inline I32x4 I32x4_create1_2_1(I32 a, I32x2 b, I32 c) { return I32x4_create4(a, I32x2_x(b), I32x2_y(b), c); }
static inline I32x4 I32x4_create1_1_2(I32 a, I32 b, I32x2 c) { return I32x4_create4(a, b, I32x2_x(c), I32x2_y(c)); }

static inline I32x4 I32x4_create2_1(I32x2 a, I32 b) { return I32x4_create3(I32x2_x(a), I32x2_y(a), b); }
static inline I32x4 I32x4_create1_2(I32 a, I32x2 b) { return I32x4_create3(a, I32x2_x(b), I32x2_y(b)); }

//Casts from vec4f

static inline F32x2 F32x2_fromF32x4(F32x4 a) { return F32x2_load2(&a); }
static inline F32x4 F32x4_fromF32x2(F32x2 a) { return F32x4_load2(&a); }

//Cast from vec2f to vec4

static inline F32x4 F32x4_create2_2(F32x2 a, F32x2 b) { return F32x4_create4(F32x2_x(a), F32x2_y(a), F32x2_x(b), F32x2_y(b)); }

static inline F32x4 F32x4_create2_1_1(F32x2 a, F32 b, F32 c) { return F32x4_create4(F32x2_x(a), F32x2_y(a), b, c); }
static inline F32x4 F32x4_create1_2_1(F32 a, F32x2 b, F32 c) { return F32x4_create4(a, F32x2_x(b), F32x2_y(b), c); }
static inline F32x4 F32x4_create1_1_2(F32 a, F32 b, F32x2 c) { return F32x4_create4(a, b, F32x2_x(c), F32x2_y(c)); }

static inline F32x4 F32x4_create2_1(F32x2 a, F32 b) { return F32x4_create3(F32x2_x(a), F32x2_y(a), b); }
static inline F32x4 F32x4_create1_2(F32 a, F32x2 b) { return F32x4_create3(a, F32x2_x(b), F32x2_y(b)); }

static inline I32x2 I32x2_fromF32x2(F32x2 a) { return (I32x2) { .v = { (I32) F32x2_x(a), (I32) F32x2_y(a) } }; }
static inline F32x2 F32x2_fromI32x2(I32x2 a) { return (F32x2) { .v = { (F32) I32x2_x(a), (F32) I32x2_y(a) } }; }

#ifdef __cplusplus
		}
#endif
