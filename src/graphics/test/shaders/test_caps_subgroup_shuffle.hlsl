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

//Capability execution shader: subgroup shuffle.
//
//SubgroupShuffle is VK_SUBGROUP_FEATURE_SHUFFLE_BIT, which covers OpGroupNonUniformShuffle.
//WaveReadLaneAt with a non uniform lane index is the HLSL intrinsic DXC lowers to exactly that, so this
// stays clear of ShuffleRelative and Quad, which are separate Vulkan bits the engine doesn't track.
//Both backends compile it, since WaveReadLaneAt is plain HLSL; DXIL reflection just can't detect the
// extension (one generic wave ops flag), so on DXIL it is annotation-driven.

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

//128 threads rather than the 64 the sibling shaders use.
//WaveReadLaneAt may only name an ACTIVE lane, and a group of 64 leaves half of a 128 wide wave idle on the
// parts that report that width, which would make the partner lane undefined.
//128 is a multiple of every wave width the engine accepts, since it refuses anything outside 4 to 128 and
// the size is always a power of two, so a driver packing a 1D group linearly leaves every wave full and
// lane ^ 1 always active.
//That packing is an assumption rather than a guarantee: full subgroups are only promised under
// VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT, which the engine doesn't set.
//A result strictly between 0 and 128 should be read as a partly populated wave before it is read as a
// broken shuffle.

//Both extensions are required, not just the obvious one: WaveReadLaneAt emits GroupNonUniformShuffle
// (SubgroupShuffle), while WaveGetLaneIndex reads SubgroupLocalInvocationId, which needs GroupNonUniform and
// maps to SubgroupOperations.
//Declaring only one makes processSPIRV refuse the binary with "capability that wasn't enabled by oiSH file".
//App data: [0] = bindless write handle of the output buffer.

[[oxc::extension("SubgroupShuffle", "SubgroupOperations")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(128, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	const U32 lane = WaveGetLaneIndex();
	const U32 partner = lane ^ 1;

	//Multiplying by an odd constant is a bijection modulo 2^32, so no two lanes carry the same payload and
	// every bit of it depends on the lane.
	//A shuffle moving only part of the word is caught by that, not just one reading the wrong lane outright.

	const U32 mine = (lane * 0x9E3779B9u) ^ 0xA5A5A5A5u;
	const U32 expected = (partner * 0x9E3779B9u) ^ 0xA5A5A5A5u;

	//Each lane checks its own read rather than the wave summing what it received.
	//XOR 1 is a permutation of the wave, so a sum is identical whether the shuffle moved anything or not, and
	// a summing test would pass on a shuffle that is a plain no-op.

	rwBufferUniform(_push.output).InterlockedAdd(0, WaveReadLaneAt(mine, partner) == expected ? 1 : 0);
}
