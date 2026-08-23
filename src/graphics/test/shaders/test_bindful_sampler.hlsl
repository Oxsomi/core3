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


//A texture, a sampler and an output buffer in ONE bindful table: the sampler range lands in the separate
// sampler root table on D3D12, a path no other test exercises.
//Distinct register indices because t/s/u share one SPIRV binding namespace per space.
//Deliberately includes NO bindless headers.

Texture2D<float4> tex : register(t0, space0);
SamplerState samp : register(s1, space0);
RWByteAddressBuffer output : register(u2, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	float4 texel = tex.SampleLevel(samp, (float2(id.xy) + 0.5) / 8, 0);
	output.Store((id.y * 8 + id.x) * 4, (uint)(texel.x * 255 + 0.5));
}
