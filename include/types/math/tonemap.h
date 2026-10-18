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

//types/math/tonemap.h

//Display transforms: scene referred linear Rec.709 radiance in, display referred sRGB encoded [0, 1] out, which is
// what an 8 bit sRGB target (a BMP, a swapchain) takes. The HLSL twins in @tonemap.hlsli use the same formulas
// and constants; a GPU's pow and log2 round differently, so the two agree to a few ulp rather than bit for bit.
//
//Exposure is the caller's: scale by 2^stops before the transform, since every curve here assumes 1 is a mid-bright
// diffuse white rather than knowing the scene.
//
//  None         a clamp
//  Reinhard     never clips, and desaturates brights toward white slowly
//  ACES         filmic contrast, with hue shifts in saturated brights
//  AgX          brights desaturate toward white without their hue skewing
//  PBRNeutral   base colors below its compression start pass untouched, which is what a material (or an architect's
//               sample board) is judged against
//
//Each curve's source is at its function.

#pragma once
#include "types/math/vec4f.h"
#include "types/math/vec4f_swizzle.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ETonemap {
	ETonemap_None,
	ETonemap_Reinhard,
	ETonemap_ACES,
	ETonemap_AgX,
	ETonemap_PBRNeutral,
	ETonemap_Count
} ETonemap;

//The sRGB transfer function, IEC 61966-2-1, on rgb, alpha passed through. Exact rather than approximated: an 8 bit
// output's shadows are where the cheap forms go wrong by a percent or two
// (http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html has the tradeoffs, should a shader ever
// need one).
//https://en.wikipedia.org/wiki/SRGB#Transfer_function_(%22gamma%22)

static inline F32x4 F32x4_srgbEncode(F32x4 v) {

	const F32x4 c = F32x4_clamp(v, F32x4_zero(), F32x4_one());
	const F32x4 curve = F32x4_sub(F32x4_mul(F32x4_xxxx4(1.055f), F32x4_pow(c, F32x4_xxxx4(1 / 2.4f))), F32x4_xxxx4(0.055f));

	const F32x4 e = F32x4_select(F32x4_leq(c, F32x4_xxxx4(0.0031308f)), F32x4_mul(c, F32x4_xxxx4(12.92f)), curve);
	return F32x4_setWCopy(e, F32x4_w(v));
}

static inline F32x4 F32x4_srgbDecode(F32x4 v) {

	const F32x4 c = F32x4_clamp(v, F32x4_zero(), F32x4_one());
	const F32x4 curve = F32x4_pow(F32x4_div(F32x4_add(c, F32x4_xxxx4(0.055f)), F32x4_xxxx4(1.055f)), F32x4_xxxx4(2.4f));

	const F32x4 d = F32x4_select(F32x4_leq(c, F32x4_xxxx4(0.04045f)), F32x4_div(c, F32x4_xxxx4(12.92f)), curve);
	return F32x4_setWCopy(d, F32x4_w(v));
}

//The curves below take non negative linear Rec.709 and return LINEAR display values, except AgX, whose curve
// already produces display encoded ones; F32x4_tonemap applies the encoding the rest need. Alpha passes through.
//Matrices are given as their three COLUMNS (F32x4_mul3x3), so a column vector transform reads as it's published.

//Reinhard et al. 2002, "Photographic Tone Reproduction for Digital Images", the simple global operator.
//https://www.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf

static inline F32x4 F32x4_tonemapReinhard(F32x4 v) {
	return F32x4_setWCopy(F32x4_div(v, F32x4_add(v, F32x4_one())), F32x4_w(v));
}

//Stephen Hill's fit of the ACES RRT and sRGB ODT (Baking Lab), with its input and output matrices, transposed here from
// the rows its mul(M, v) gives.
//https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl

static inline F32x4 F32x4_tonemapACESCurve(F32x4 v) {
	const F32x4 a = F32x4_sub(F32x4_mul(v, F32x4_add(v, F32x4_xxxx4(0.0245786f))), F32x4_xxxx4(0.000090537f));
	const F32x4 b = F32x4_fma(v, F32x4_fma(v, F32x4_xxxx4(0.983729f), F32x4_xxxx4(0.4329510f)), F32x4_xxxx4(0.238081f));
	return F32x4_div(a, b);
}

static inline F32x4 F32x4_tonemapACES(F32x4 v) {

	F32x4 input[3] = {
		F32x4_create3(0.59719f, 0.07600f, 0.02840f),
		F32x4_create3(0.35458f, 0.90834f, 0.13383f),
		F32x4_create3(0.04823f, 0.01566f, 0.83777f)
	};

	F32x4 output[3] = {
		F32x4_create3(1.60475f, -0.10208f, -0.00327f),
		F32x4_create3(-0.53108f, 1.10813f, -0.07276f),
		F32x4_create3(-0.07367f, -0.00605f, 1.07602f)
	};

	const F32x4 c = F32x4_mul3x3(F32x4_tonemapACESCurve(F32x4_mul3x3(v, input)), output);
	return F32x4_setWCopy(c, F32x4_w(v));
}

//Troy Sobotka's AgX, base look: Benjamin Wrensch's polynomial fit of its default contrast curve, with Blender's inset
// and outset matrices as the GLSL original's column major mat3s give them. The result is display encoded.
//https://iolite-engine.com/blog_posts/minimal_agx_implementation
//https://github.com/sobotka/AgX

static inline F32x4 F32x4_tonemapAgXCurve(F32x4 x) {

	const F32x4 x2 = F32x4_pow2(x), x4 = F32x4_pow2(x2);

	F32x4 r = F32x4_mul(F32x4_xxxx4(15.5f), F32x4_mul(x4, x2));
	r = F32x4_fma(F32x4_xxxx4(-40.14f), F32x4_mul(x4, x), r);
	r = F32x4_fma(F32x4_xxxx4(31.96f), x4, r);
	r = F32x4_fma(F32x4_xxxx4(-6.868f), F32x4_mul(x2, x), r);
	r = F32x4_fma(F32x4_xxxx4(0.4298f), x2, r);
	r = F32x4_fma(F32x4_xxxx4(0.1191f), x, r);
	return F32x4_sub(r, F32x4_xxxx4(0.00232f));
}

static inline F32x4 F32x4_tonemapAgX(F32x4 v) {

	F32x4 inset[3] = {
		F32x4_create3(0.842479062253094f, 0.0423282422610123f, 0.0423756549057051f),
		F32x4_create3(0.0784335999999992f, 0.878468636469772f, 0.0784336f),
		F32x4_create3(0.0792237451477643f, 0.0791661274605434f, 0.879142973793104f)
	};

	F32x4 outset[3] = {
		F32x4_create3(1.19687900512017f, -0.0528968517574562f, -0.0529716355144438f),
		F32x4_create3(-0.0980208811401368f, 1.15190312990417f, -0.0980434501171241f),
		F32x4_create3(-0.0990297440797205f, -0.0989611768448433f, 1.15107367264116f)
	};

	const F32x4 minEv = F32x4_xxxx4(-12.47393f), maxEv = F32x4_xxxx4(4.026069f);

	const F32x4 e = F32x4_mul3x3(v, inset);
	const F32x4 ev = F32x4_select(F32x4_gt(e, F32x4_zero()), F32x4_clamp(F32x4_log2(e), minEv, maxEv), minEv);
	const F32x4 t = F32x4_div(F32x4_sub(ev, minEv), F32x4_sub(maxEv, minEv));

	const F32x4 o = F32x4_clamp(F32x4_mul3x3(F32x4_tonemapAgXCurve(t), outset), F32x4_zero(), F32x4_one());
	return F32x4_setWCopy(o, F32x4_w(v));
}

//Khronos PBR Neutral, from its reference implementation.
//https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral

static inline F32x4 F32x4_tonemapPBRNeutral(F32x4 v) {

	const F32 start = 0.8f - 0.04f, desaturation = 0.15f;

	const F32 low = F32_min(F32x4_x(v), F32_min(F32x4_y(v), F32x4_z(v)));
	const F32 offset = low < 0.08f ? low - 6.25f * F32_pow2(low) : 0.04f;

	const F32x4 c = F32x4_setWCopy(F32x4_sub(v, F32x4_xxxx4(offset)), F32x4_w(v));
	const F32 peak = F32_max(F32x4_x(c), F32_max(F32x4_y(c), F32x4_z(c)));

	if (peak < start)
		return c;

	const F32 d = 1 - start;
	const F32 newPeak = 1 - F32_pow2(d) / (peak + d - start);
	const F32 blend = 1 - 1 / (desaturation * (peak - newPeak) + 1);

	const F32x4 compressed = F32x4_mul(c, F32x4_xxxx4(newPeak / peak));
	const F32x4 r = F32x4_add(F32x4_mul(compressed, F32x4_xxxx4(1 - blend)), F32x4_xxxx4(newPeak * blend));

	return F32x4_setWCopy(r, F32x4_w(v));
}

//Linear radiance to display referred sRGB encoded, alpha passed through.

static inline F32x4 F32x4_tonemap(F32x4 v, ETonemap op) {

	v = F32x4_setWCopy(F32x4_max(v, F32x4_zero()), F32x4_w(v));

	switch (op) {
		case ETonemap_Reinhard:      return F32x4_srgbEncode(F32x4_tonemapReinhard(v));
		case ETonemap_ACES:          return F32x4_srgbEncode(F32x4_tonemapACES(v));
		case ETonemap_AgX:           return F32x4_tonemapAgX(v);
		case ETonemap_PBRNeutral:    return F32x4_srgbEncode(F32x4_tonemapPBRNeutral(v));
		default:                     return F32x4_srgbEncode(v);
	}
}

#ifdef __cplusplus
	}
#endif
