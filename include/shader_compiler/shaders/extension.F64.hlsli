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

//shader_compiler/shaders/extension.F64.hlsli

#pragma once

//F64: double and its vectors and matrices.
//Included by @types.hlsli and @extensions.hlsli behind __OXC_EXT_F64 or __OXC_EXT_ATOMICF64, so it carries
//no guard of its own. AtomicF64 implies these types because its intrinsic takes a double by reference: a
//shader enabling the atomic without the type could never call it, and the SPIRV capability check refuses
//that pairing anyway.

typedef double F64;
typedef double2 F64x2;
typedef double3 F64x3;
typedef double4 F64x4;

//Float64 matrices

typedef double4x4 F64x4x4;
typedef double3x4 F64x3x4;
typedef double2x4 F64x2x4;

typedef double4x3 F64x4x3;
typedef double3x3 F64x3x3;
typedef double2x3 F64x2x3;

typedef double4x2 F64x4x2;
typedef double3x2 F64x3x2;
typedef double2x2 F64x2x2;
