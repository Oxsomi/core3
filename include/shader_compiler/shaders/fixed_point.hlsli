R"(
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

//shader_compiler/shaders/fixed_point.hlsli
//
//64-bit fixed point helpers, for world positions too large to keep in F32 without cracking.
//Needs 64-bit integers, so the whole file is behind __OXC_EXT_I64.

#pragma once
#include "@types.hlsli"

//Fixed point math

#ifdef __OXC_EXT_I64

	U64x3 fixedPointUnpack(U32x4 packed) {
		U32x3 hi = (packed.www >> U32x3(0, 10, 20)) & ((1 << 10) - 1);
		U64x3 hiShift = (U64x3) hi << 32;
		return hiShift | packed.xyz;
	}

	U32x4 fixedPointPack(U64x3 unpacked) {
	
		U32x3 lo = (U32x3) unpacked;

		unpacked >>= 32;
		U32x3 hi = ((U32x3) unpacked) << U32x3(0, 10, 20);

		return U32x4(lo, dot(hi, 1.xxx));
	}

	U64x3 fixedPointSub(U64x3 a, U64x3 b) { return a - b; }
	U64x3 fixedPointAdd(U64x3 a, U64x3 b) { return a + b; }

	static const U32 fixedPointFraction = 4;		//1/16th cm
	static const U32 fixedPointShift = 41;			//42 bits (sign is excluded here)

	static const F32 fixedPointMultiplier = 1.f / (1 << fixedPointFraction);
	static const U64 fixedPointMask = ((U64)1 << (fixedPointShift + 1)) - 1;

	F32x3 fixedPointToFloat(U64x3 value) {

		Boolx3 sign = (Boolx3)(value >> fixedPointShift);
		U64x3 correctedForSign = value ^ fixedPointMask.xxx;
		++correctedForSign;

		value = select(sign, correctedForSign, value);
		return (F32x3)value * select(sign, -fixedPointMultiplier, fixedPointMultiplier);
	}

	U64x3 fixedPointFromFloat(F32x3 v) {

		Boolx3 sign = v < 0;
		v = abs(v);

		v *= (1 << fixedPointFraction).xxx;

		U64x3 res = (U64x3) v;
		U64x3 correctedForSign = (res - 1) ^ fixedPointMask.xxx;
		return select(sign, correctedForSign, res);
	}

#endif

)"
