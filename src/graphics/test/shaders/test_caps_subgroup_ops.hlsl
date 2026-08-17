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

//Capability execution shader: subgroup ballot operations.
//
//The companion to test_caps_subgroup.hlsl, which covers subgroup arithmetic.
//Both compile on both backends, since wave intrinsics are plain HLSL.
//The difference is detectability: DXIL reflection reports a single D3D_SHADER_REQUIRES_WAVE_OPS flag, so
// SubgroupArithmetic is annotation-driven on DXIL, while SubgroupOperations here is in both Native sets and
// reflection can confirm it on either backend.

#include "@appdata.hlsli"
#include "@buffer.hlsli"

//Every lane is active here, so each wave's ballot bit count is exactly its width,
// and the first lane of each wave contributes that count.
//Summed across the dispatch this lands on the thread count.
//The failure worth catching is a ballot that only saw the calling lane: that returns 1 per wave and totals
// the wave count instead of the thread count, which is a different number for every real wave width.
//App data: [0] = bindless write handle of the output buffer.

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	const U32 activeLanes = WaveActiveCountBits(true);

	if(WaveIsFirstLane())
		rwBufferUniform(getAppData1u(0)).InterlockedAdd(0, activeLanes);
}
