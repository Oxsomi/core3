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

//Display transforms, linear Rec.709 radiance to display referred sRGB encoded [0, 1].
//Mirrors types/math/tonemap.h, which documents each curve and where it came from; ETonemap is the same enum.

#pragma once
#include "@types.hlsli"
#include "@math.hlsli"

enum ETonemap {
	ETonemap_None,
	ETonemap_Reinhard,
	ETonemap_ACES,
	ETonemap_AgX,
	ETonemap_PBRNeutral,
	ETonemap_Count
};

//IEC 61966-2-1. https://en.wikipedia.org/wiki/SRGB#Transfer_function_(%22gamma%22)

F32x3 srgbEncode(F32x3 v) {
	v = saturate(v);
	return select(v <= 0.0031308, v * 12.92, 1.055 * pow(v, 1 / 2.4) - 0.055);
}

F32x3 srgbDecode(F32x3 v) {
	v = saturate(v);
	return select(v <= 0.04045, v / 12.92, pow((v + 0.055) / 1.055, 2.4));
}

//https://www.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf

F32x3 tonemapReinhard(F32x3 c) {
	return c / (1 + c);
}

//https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl

F32x3 tonemapACESCurve(F32x3 v) {
	const F32x3 a = v * (v + 0.0245786) - 0.000090537;
	const F32x3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
	return a / b;
}

F32x3 tonemapACES(F32x3 c) {

	static const F32x3x3 input = {
		0.59719, 0.35458, 0.04823,
		0.07600, 0.90834, 0.01566,
		0.02840, 0.13383, 0.83777
	};

	static const F32x3x3 output = {
		1.60475, -0.53108, -0.07367,
		-0.10208, 1.10813, -0.00605,
		-0.00327, -0.07276, 1.07602
	};

	return mul(output, tonemapACESCurve(mul(input, c)));
}

//https://iolite-engine.com/blog_posts/minimal_agx_implementation
//https://github.com/sobotka/AgX
//The result is already display encoded.

F32x3 tonemapAgXCurve(F32x3 x) {
	const F32x3 x2 = pow2(x), x4 = pow2(x2);
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

F32x3 tonemapAgX(F32x3 c) {

	static const F32x3x3 inset = {
		0.842479062253094, 0.0784335999999992, 0.0792237451477643,
		0.0423282422610123, 0.878468636469772, 0.0791661274605434,
		0.0423756549057051, 0.0784336, 0.879142973793104
	};

	static const F32x3x3 outset = {
		1.19687900512017, -0.0980208811401368, -0.0990297440797205,
		-0.0528968517574562, 1.15190312990417, -0.0989611768448433,
		-0.0529716355144438, -0.0980434501171241, 1.15107367264116
	};

	const F32 minEv = -12.47393, maxEv = 4.026069;

	const F32x3 e = mul(inset, c);
	const F32x3 ev = select(e > 0, clamp(log2(max(e, 1e-30)), minEv, maxEv), minEv);

	return saturate(mul(outset, tonemapAgXCurve((ev - minEv) / (maxEv - minEv))));
}

//https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral

F32x3 tonemapPBRNeutral(F32x3 c) {

	const F32 start = 0.8 - 0.04, desaturation = 0.15;

	const F32 low = min(c.r, min(c.g, c.b));
	const F32 offset = low < 0.08 ? low - 6.25 * pow2(low) : 0.04;

	c -= offset;
	const F32 peak = max(c.r, max(c.g, c.b));

	if(peak < start)
		return c;

	const F32 d = 1 - start;
	const F32 newPeak = 1 - pow2(d) / (peak + d - start);
	const F32 blend = 1 - 1 / (desaturation * (peak - newPeak) + 1);

	return lerp(c * (newPeak / peak), newPeak.xxx, blend);
}

F32x3 tonemap(F32x3 c, ETonemap op) {

	c = max(c, 0);

	switch(op) {
		case ETonemap_Reinhard:      return srgbEncode(tonemapReinhard(c));
		case ETonemap_ACES:          return srgbEncode(tonemapACES(c));
		case ETonemap_AgX:           return tonemapAgX(c);
		case ETonemap_PBRNeutral:    return srgbEncode(tonemapPBRNeutral(c));
		default:                     return srgbEncode(c);
	}
}
