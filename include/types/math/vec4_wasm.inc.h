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

//types/math/vec4_wasm.inc.h

#pragma once
#ifndef VEC4_WASM_GUARD
	#error Vec4 wasm guard was undefined, this likely indicates include of vec4_wasm.h was attempted instead of vec4.h
#endif

#include <wasm_simd128.h>

//Native wasm SIMD128 rather than emscripten's SSE shims.
//The shims cannot be used here: they declare a named struct with attributes and no declarator inside a
// function, which -fms-extensions misparses, and OxC3's C++ TUs require that flag while including this header.
//SIMD128 is one 128 bit type (v128_t) for both int and float lanes, so I32x4 and F32x4 are the same
// underlying type and the lane interpretation lives in the operation rather than the value.

typedef v128_t I32x4;
typedef v128_t F32x4;

//wasm_i32x4_shuffle takes lane indices directly, where _mm_shuffle_epi32 packs them into an immediate.
//Selecting from one vector means naming it twice; lanes 0-3 are a, 4-7 would be b.

#define vecShufflei(a, x, y, z, w) wasm_i32x4_shuffle(a, a, x, y, z, w)
#define vecShufflef(a, x, y, z, w) wasm_i32x4_shuffle(a, a, x, y, z, w)

static inline F32x4 F32x4_zero() { return wasm_f32x4_splat(0); }
static inline F32x4 F32x4_xxxx4(F32 x) { return wasm_f32x4_splat(x); }

static inline F32x4 F32x4_create1(F32 x) { return wasm_f32x4_make(x, 0, 0, 0); }
static inline F32x4 F32x4_create2(F32 x, F32 y) { return wasm_f32x4_make(x, y, 0, 0); }
static inline F32x4 F32x4_create3(F32 x, F32 y, F32 z) { return wasm_f32x4_make(x, y, z, 0); }
static inline F32x4 F32x4_create4(F32 x, F32 y, F32 z, F32 w) { return wasm_f32x4_make(x, y, z, w); }

static inline I32x4 I32x4_zero() { return wasm_i32x4_splat(0); }
static inline I32x4 I32x4_xxxx4(I32 x) { return wasm_i32x4_splat(x); }

static inline I32x4 I32x4_create1(I32 x) { return wasm_i32x4_make(x, 0, 0, 0); }
static inline I32x4 I32x4_create2(I32 x, I32 y) { return wasm_i32x4_make(x, y, 0, 0); }
static inline I32x4 I32x4_create3(I32 x, I32 y, I32 z) { return wasm_i32x4_make(x, y, z, 0); }
static inline I32x4 I32x4_create4(I32 x, I32 y, I32 z, I32 w) { return wasm_i32x4_make(x, y, z, w); }
