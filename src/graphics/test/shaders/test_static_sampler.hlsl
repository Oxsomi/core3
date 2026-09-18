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

//STATIC samplers beside the bindless set: the device's layout was created from
// GraphicsDevice_defaultBindlessLayout plus a sampler binding per sampler, each naming a by-value sampler, so the
// shader reaches them as plain bindings and needs no DynamicSamplers extension and no sampler index.
//SPIRV set 0 bindings 1 and up, beside the slot the dynamic array would take; DXIL s0 and up in space 0, outside
// the reserved space.
//Two of them, differing only in address mode, so a layout that resolved a binding to the wrong sampler shows up as
// a wrong texel rather than as a sampler that merely works.
//One entrypoint per file, as in the other test shaders.

#include "@resources.hlsli"
#include "@pack.hlsli"

struct StaticSamplerPush {
	U32 texture;         //Bindless read handle of the source texture
	U32 output;          //Bindless write handle of the output buffer
	U32 padding0;
	U32 padding1;
};

PUSH_CONSTANT StaticSamplerPush _push;

_vkBinding(1, 0) SamplerState _staticSampler : register(s0, space0);
_vkBinding(2, 0) SamplerState _staticSamplerRepeat : register(s1, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x3 id : SV_DispatchThreadID) {

	//Sampled at texel centres of an 8x8 target, so a point sampler returns each texel exactly and the
	// readback compares against the source rather than against an interpolation of it.

	F32x2 uv = (F32x2(id.xy) + 0.5f) / 8.0f;
	U32 i = id.y * 8 + id.x;

	rwBuffer(_push.output).Store(i * 4, packUnorm4x8(
		texture2D(_push.texture).SampleLevel(_staticSampler, uv, 0)
	));

	//A whole texture out of range through each sampler tells them apart: the clamping one holds the far corner
	// for every thread, the repeating one wraps back to the texel it started from.

	rwBuffer(_push.output).Store((64 + i) * 4, packUnorm4x8(
		texture2D(_push.texture).SampleLevel(_staticSampler, uv + 1.0f, 0)
	));

	rwBuffer(_push.output).Store((128 + i) * 4, packUnorm4x8(
		texture2D(_push.texture).SampleLevel(_staticSamplerRepeat, uv + 1.0f, 0)
	));
}
