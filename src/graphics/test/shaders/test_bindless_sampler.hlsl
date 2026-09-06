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

//The BINDLESS sampler array, which nothing else in the suite touches.
//_samplers is the one bindless array that owns a whole descriptor set to itself on Vulkan (set 0, while every
// resource array sits at set 1), and it is the array EGraphicsDeviceFlags_EnableDynamicSamplers turns off.
//Without a test that indexes it dynamically, dropping it by default would look like it cost nothing.
//One entrypoint per file, as in the other test shaders.

#include "@resources.hlsli"

struct SamplerPush {
	U32 texture;         //Bindless read handle of the source texture
	U32 samplerId;       //Sampler's samplerLocation, its index into _samplers
	U32 output;          //Bindless write handle of the output buffer
	U32 padding;
};

PUSH_CONSTANT SamplerPush _push;

//The annotation is what declares _samplers and the sampler() accessor at all: it sets
//__OXC_EXT_DYNAMICSAMPLERS, resources.hlsli declares the array behind it, and the binary carries
//ESHExtension_DynamicSamplers so a device without EnableDynamicSamplers refuses it by name.

[[oxc::extension("DynamicSamplers")]]
[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x3 id : SV_DispatchThreadID) {

	//Sampled at texel centres of an 8x8 target, so a point sampler returns each texel exactly and the
	// readback compares against the source rather than against an interpolation of it.

	F32x2 uv = (F32x2(id.xy) + 0.5f) / 8.0f;

	F32x4 texel = texture2D(_push.texture).SampleLevel(sampler(_push.samplerId), uv, 0);

	//Repacked into the 0xAABBGGRR the texture was uploaded as

	U32 packed =
		((U32)(texel.x * 255.0f + 0.5f)) |
		((U32)(texel.y * 255.0f + 0.5f) << 8) |
		((U32)(texel.z * 255.0f + 0.5f) << 16) |
		((U32)(texel.w * 255.0f + 0.5f) << 24);

	rwBuffer(_push.output).Store((id.y * 8 + id.x) * 4, packed);
}
