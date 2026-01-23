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
#ifndef VEC4_NONE_GUARD
	#error Vec4 NONE guard was undefined, this likely indicates include of vec4_none.h was attempted instead of vec4.h
#endif

#include <stdalign.h>

typedef struct I32x4 {
	alignas(16) I32 v[4];
} I32x4;

typedef struct F32x4 {
	alignas(16) F32 v[4];
} F32x4;

#define vecShufflei(a, x, y, z, w) (I32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }
#define vecShufflef(a, x, y, z, w) (F32x4){ { a.v[x], a.v[y], a.v[z], a.v[w] } }

static inline F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { F32x4 v = { { x, y, z, w } }; return v; }
static inline F32x4 F32x4_xxxx4(F32 x) { return F32x4_create4(x, x, x, x); }
static inline F32x4 F32x4_zero() { return F32x4_xxxx4(0); }

static inline F32x4 F32x4_create1(F32 x) { return F32x4_create4(x, 0, 0, 0); }
static inline F32x4 F32x4_create2(F32 x, F32 y) { return F32x4_create4(x, y, 0, 0); }
static inline F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return F32x4_create4(x, y, z, 0); }

static inline I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { I32x4 v = { { x, y, z, w } }; return v; }
static inline I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return I32x4_create4(x, y, z, 0); }
static inline I32x4 I32x4_create2(I32 x, I32 y) { return I32x4_create4(x, y, 0, 0); }
static inline I32x4 I32x4_create1(I32 x) { return I32x4_create4(x, 0, 0, 0); }

static inline I32x4 I32x4_xxxx4(I32 x) { return I32x4_create4(x, x, x, x); }
static inline I32x4 I32x4_zero() { return I32x4_xxxx4(0); }
