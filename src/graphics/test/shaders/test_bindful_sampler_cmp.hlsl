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


//A comparison sampler against a depth texture: the only shape that exercises the SamplerComparisonState
// register type, which the table's descriptor write type checks separately from a plain sampler.
//Every thread compares its own reference against the same cleared depth, so the boundary between passing
// and failing lands at a known thread index rather than depending on filtering.
//Distinct register indices because t/s/u share one SPIRV binding namespace per space.
//Deliberately includes NO bindless headers.

Texture2D<float> depthTex : register(t0, space0);
SamplerComparisonState cmpSamp : register(s1, space0);
RWByteAddressBuffer output : register(u2, space0);

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {

	//SampleCmpLevelZero rather than SampleCmp: no derivatives exist in a compute shader

	const float reference = id.x / 64.0;
	const float passed = depthTex.SampleCmpLevelZero(cmpSamp, float2(0.5, 0.5), reference);

	output.Store(id.x * 4, (uint)(passed + 0.5));
}
