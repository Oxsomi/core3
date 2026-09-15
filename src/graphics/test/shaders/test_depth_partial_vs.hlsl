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

#include "@types.hlsli"

static const F32 layerDepth = 0.7;
static const F32x4 layerColor = F32x4(0, 1, 0, 1);

//A partial coverage companion to test_depth_vs, in its OWN file rather than a second entrypoint beside that
//file's main. Two entrypoints in one module is what makes Vulkan warn "Found 2 different OpVariable, can't
//determine which entrypoint": below SPIR-V 1.4 OpEntryPoint lists only Input/Output, so globals the two
//entries share cannot be attributed to either. One entry per module sidesteps it without moving the whole
//shader stack to a newer target.
//The depth and color match test_depth_vs's SURVIVING layer (0.7, green), so a resolve of this can be
//compared against that file's result directly.

//It covers only PART of each pixel it touches, which is what a resolve mode needs to be
// observable at all: a fullscreen triangle leaves every sample of a pixel holding the same value, so min and
// max agree and a backend that ignores the mode looks correct.
//A quad over the left of the target whose right edge is SLANTED, running from x 4.5 at the bottom of an 8
// wide target to x 5.5 at the top. Everything left of it keeps both samples at 0.7, everything right keeps
// the cleared 0, and the pixels the edge crosses hold one of each, which is the only thing min and max can
// disagree about.
//The slant is the point. A 45 degree edge is exactly the axis the standard 2x pattern places its two samples
// on ((0.25, 0.25) and (0.75, 0.75)), so both would land on the edge itself and no pixel would ever split; a
// purely vertical one relies on the two samples differing in x, which the standard pattern does but a device
// reporting standardSampleLocations false need not. Slanted, neither degeneracy applies.
//6 vertices, two triangles, wound the same way as test_depth_vs's fullscreen triangles.

struct DepthLayerOutput {
	F32x4 pos : SV_POSITION;
	F32x4 color : TEXCOORD0;
};

[shader("vertex")]
DepthLayerOutput main(U32 id : SV_VertexID) {

	static const F32x2 corners[6] = {
		F32x2(-1, -1), F32x2(0.125, -1), F32x2(-1, 1),
		F32x2(0.125, -1), F32x2(0.375, 1), F32x2(-1, 1)
	};

	DepthLayerOutput output;
	output.pos = F32x4(corners[id % 6].x, corners[id % 6].y, layerDepth, 1);
	output.color = layerColor;
	return output;
}
