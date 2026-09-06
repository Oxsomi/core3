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

//Three fullscreen triangles at different depths in one draw, so a single call tells the whole depth story.
//Triangle 0 is far and writes, triangle 1 is nearer and passes, triangle 2 is farthest and must be rejected.
//Reverse Z is an app convention (fold 1 - z into the projection); the viewport is a plain 0..1 on every
// backend, so these z values are already reversed: near stores high, compare Greater, far clear is 0.
//The result has to be triangle 1's color and its 0.7 depth in the depth buffer.

static const F32 layerDepth[3] = { 0.4, 0.7, 0.1 };

static const F32x4 layerColor[3] = {
	F32x4(1, 0, 0, 1),
	F32x4(0, 1, 0, 1),
	F32x4(0, 0, 1, 1)
};

struct DepthLayerOutput {
	F32x4 pos : SV_POSITION;
	F32x4 color : TEXCOORD0;
};

[shader("vertex")]
DepthLayerOutput main(U32 id : SV_VertexID) {

	U32 triangleId = min(id / 3, 2);
	U32 corner = id % 3;

	DepthLayerOutput output;
	output.pos = F32x4(corner == 1 ? 3 : -1, corner == 2 ? 3 : -1, layerDepth[triangleId], 1);
	output.color = layerColor[triangleId];
	return output;
}
