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

//shader_compiler/shaders/extension.I64.hlsli

#pragma once

//I64: int64_t/uint64_t and their vectors and matrices.
//Included by @types.hlsli and @extensions.hlsli behind __OXC_EXT_I64, so it carries no guard of its own;
//including it directly declares the aliases whether or not the extension is on.

typedef uint64_t U64;
typedef uint64_t2 U64x2;
typedef uint64_t3 U64x3;
typedef uint64_t4 U64x4;

typedef int64_t I64;
typedef int64_t2 I64x2;
typedef int64_t3 I64x3;
typedef int64_t4 I64x4;

//Uint64 matrices

typedef uint64_t4x4 U64x4x4;
typedef uint64_t3x4 U64x3x4;
typedef uint64_t2x4 U64x2x4;

typedef uint64_t4x3 U64x4x3;
typedef uint64_t3x3 U64x3x3;
typedef uint64_t2x3 U64x2x3;

typedef uint64_t4x2 U64x4x2;
typedef uint64_t3x2 U64x3x2;
typedef uint64_t2x2 U64x2x2;

//Int64 matrices

typedef int64_t4x4 I64x4x4;
typedef int64_t3x4 I64x3x4;
typedef int64_t2x4 I64x2x4;

typedef int64_t4x3 I64x4x3;
typedef int64_t3x3 I64x3x3;
typedef int64_t2x3 I64x2x3;

typedef int64_t4x2 I64x4x2;
typedef int64_t3x2 I64x3x2;
typedef int64_t2x2 I64x2x2;
