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

//Capability execution shader: subgroup arithmetic.
//Gated on SubgroupArithmetic so it is only emitted where the device claims it.

#include "@resources.hlsli"
#include "@buffer.hlsli"

//Per dispatch data this shader reads, declared as a push constant.
//Scalars rather than an array: on DXIL each array element takes its own 16 byte cbuffer row, so the size the
//work op checks would not match what the shader declares.

struct CapsPush {
	U32 output;        //Bindless write handle of the output buffer
	U32 aux;           //Second handle, where the test needs one (a TLAS for the ray query cases)
	U32 padding0, padding1;
};

PUSH_CONSTANT CapsPush _push;

//Each thread contributes its own index to a wave wide sum, then lane 0 of each wave adds that sum into the
// output with a plain atomic.
//Summing across the whole dispatch this way only lands on the expected total if WaveActiveSum really reduced
// across the wave rather than returning the lane's own value, which is the failure mode worth catching.
//App data: [0] = bindless write handle of the output buffer.

//Both extensions are required, not just the obvious one: WaveActiveSum emits GroupNonUniformArithmetic
// (SubgroupArithmetic), while WaveIsFirstLane emits GroupNonUniformBallot, which maps to SubgroupOperations.
//Declaring only one makes processSPIRV refuse the binary with "capability that wasn't enabled by oiSH file".

[[oxc::extension("SubgroupArithmetic", "SubgroupOperations")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	const U32 waveSum = WaveActiveSum(i + 1);

	if(WaveIsFirstLane())
		rwBufferUniform(_push.output).InterlockedAdd(0, waveSum);
}
