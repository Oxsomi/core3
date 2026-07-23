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

[shader("vertex")]
VSOutput mainVS(F32x2 pos : TEXCOORD0, F32x2 uv : TEXCOORD1) {
	
	U32 orientation = getAppData1u(EResourceBinding_Orientation);

	VSOutput output = (VSOutput) 0;
	output.pos = mul(F32x4(pos, 0, 1), F32x4x4_rotateZ(orientation * F32_degToRad));
	output.uv = uv;
	return output;
}

[shader("pixel")]
F32x4 mainPS(VSOutput input): SV_TARGET {

	U32 resourceId = getAppData1u(EResourceBinding_ConstantColorBuffer);
	F32x3 col = getAtUniform<F32x3>(resourceId, 0);		//Randomly output from compute shader (0 at init)

	Texture2D tex = texture2DUniform(getAppData1u(EResourceBinding_CrabbageCompressed));
	SamplerState sampler = samplerUniform(getAppData1u(EResourceBinding_Sampler));

	F32x3 col2 = tex.Sample(sampler, input.uv).rgb;

	return F32x4(col2 + col, 1);
}
