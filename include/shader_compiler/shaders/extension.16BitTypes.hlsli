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

//shader_compiler/shaders/extension.16BitTypes.hlsli

#pragma once

//16BitTypes: F16/I16/U16 and their vectors and matrices.
//Included by @types.hlsli and @extensions.hlsli behind __OXC_EXT_16BITTYPES, so it carries no guard of its
//own; including it directly declares the aliases whether or not the extension is on.

typedef float16_t F16;
typedef float16_t2 F16x2;
typedef float16_t3 F16x3;
typedef float16_t4 F16x4;

//Float16 matrices

typedef float16_t4x4 F16x4x4;
typedef float16_t3x4 F16x3x4;
typedef float16_t2x4 F16x2x4;

typedef float16_t4x3 F16x4x3;
typedef float16_t3x3 F16x3x3;
typedef float16_t2x3 F16x2x3;

typedef float16_t4x2 F16x4x2;
typedef float16_t3x2 F16x3x2;
typedef float16_t2x2 F16x2x2;

typedef int16_t I16;
typedef int16_t2 I16x2;
typedef int16_t3 I16x3;
typedef int16_t4 I16x4;

typedef uint16_t U16;
typedef uint16_t2 U16x2;
typedef uint16_t3 U16x3;
typedef uint16_t4 U16x4;

//Int16 matrices

typedef int16_t4x4 I16x4x4;
typedef int16_t3x4 I16x3x4;
typedef int16_t2x4 I16x2x4;

typedef int16_t4x3 I16x4x3;
typedef int16_t3x3 I16x3x3;
typedef int16_t2x3 I16x2x3;

typedef int16_t4x2 I16x4x2;
typedef int16_t3x2 I16x3x2;
typedef int16_t2x2 I16x2x2;

//Uint16 matrices

typedef uint16_t4x4 U16x4x4;
typedef uint16_t3x4 U16x3x4;
typedef uint16_t2x4 U16x2x4;

typedef uint16_t4x3 U16x4x3;
typedef uint16_t3x3 U16x3x3;
typedef uint16_t2x3 U16x2x3;

typedef uint16_t4x2 U16x4x2;
typedef uint16_t3x2 U16x3x2;
typedef uint16_t2x2 U16x2x2;
