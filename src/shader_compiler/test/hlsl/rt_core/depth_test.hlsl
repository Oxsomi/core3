/* OxC3/RT Core(Oxsomi core 3/RT Core), a general framework for raytracing applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "resource_bindings.hlsli"

//Generate triangle

struct VSOutput {
	F32x4 pos : SV_POSITION;
	F32x2 uv : TEXCOORD0;
};

static const U32 quadIndices[] = {
	00132,		//Bottom
	06754,		//Top
	00451,		//Back
	02376,		//Front
	03157,		//Right
	00264		//Left
};

[shader("vertex")]
VSOutput mainVS(U32 id : SV_VertexID, U32 instanceId : SV_InstanceID) {

	//Generate quad

	U32 side = id / 6;
	U32 tri = id % 6;
	U32 triVertId = tri % 3;
	U32 triId = tri / 3;

	U32 indexId = (triVertId + (triId << 1)) & 3;			//0, 1, 2,  2, 3, 0
	U32 index = (quadIndices[side] >> (indexId * 3)) & 07;

	//Generate positions of quads

	F32x3 mpos = ((index.xxx >> uint3(0, 2, 1)) & 1) - 0.5f;
	F32x2 uv = float2(indexId & 1, indexId >> 1);

	F32x3 rot = _time.xxx;
	F32x3 scale = 0.25.xxx;
	F32x3 pos = F32x3(0, (sin(_time) * 0.5 + 0.5) * 5, 0);

	pos += F32x3((instanceId.xxx >> uint3(0, 2, 4)) & 3) / 3.0 * 2 - 1;

	F32x4x4 m = F32x4x4_transform(pos, rot, scale);		//pos, rot, scale
	F32x3 wpos = mul(F32x4(mpos, 1), m).xyz;

	U32 viewProjMatBuf = getAppData1u(EResourceBinding_ViewProjMatrices);
	ViewProjMatrices viewProjMat = getAtUniform<ViewProjMatrices>(viewProjMatBuf, 0);

	F32x4 cpos = mul(F32x4(wpos, 1), viewProjMat.viewProj);

	VSOutput output = (VSOutput) 0;
	output.pos = cpos;
	output.uv = uv;
	return output;
}
