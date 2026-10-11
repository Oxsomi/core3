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

//shader_compiler/shaders/math.hlsli
//
//Scalar and vector helpers every shader wants, mirroring types/base/math_common.h where both sides have them.

#pragma once
#include "@types.hlsli"

static const F32 F32_pi = 3.1415926535;
static const F32 F32_twoPi = 2 * F32_pi;
static const F32 F32_invPi = 1 / F32_pi;
static const F32 F32_degToRad = F32_pi / 180;
static const F32 F32_radToDeg = 180 / F32_pi;

template<typename T>
T pow2(T v) { return v * v; }

//Barycentric interpolation of a per vertex attribute. bc is the pair a hit reports, DXR's hit attributes and a ray
// query's CommittedTriangleBarycentrics alike: the weights of the second and third vertex, the first's implied as
// 1 - u - v. Passing a triple around instead is how the two stop agreeing.

//1 / v kept finite: v clamped away from zero to a magnitude of at least 1e-5, keeping its own sign bit, so an exactly
// zero component (a ray along an axis has two) gives a large reciprocal of its sign instead of an infinity, and a slab
// test over it never computes inf - inf. sign(v) is 0 for a zero, which is what this exists to avoid.
//Twin of vec4f.h's F32_rcpSafe and F32x4_rcpSafe: the clamp is the same bits on both sides, the division as exact as
// each side makes it.

template<typename T>
T rcpSafe(T v) {
	return 1 / asfloat(asuint(max(abs(v), 1e-5f)) | (asuint(v) & 0x80000000u));
}

template<typename T>
T interpolateBary(T a0, T a1, T a2, F32x2 bc) {
	return a0 * (1 - bc.x - bc.y) + a1 * bc.x + a2 * bc.y;
}
