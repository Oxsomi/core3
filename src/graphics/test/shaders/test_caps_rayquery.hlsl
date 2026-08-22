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

//Capability execution shader: inline raytracing (RayQuery) from a compute shader.
//
//This is the inline counterpart to test_rays.hlsl, which uses a real raytracing pipeline.
//RayQuery is a separate capability from RayPipeline, and a device can have either without the other,
// so the two need separate coverage rather than one standing in for the other.

#include "@buffer.hlsli"
#include "@resources.hlsli"

//Per dispatch data this shader reads, declared as a push constant.
//Scalars rather than an array: on DXIL each array element takes its own 16 byte cbuffer row, so the size the
//work op checks would not match what the shader declares.

struct CapsPush {
	U32 output;        //Bindless write handle of the output buffer
	U32 aux;           //Second handle, where the test needs one (a TLAS for the ray query cases)
	U32 padding0, padding1;
};

PUSH_CONSTANT CapsPush _push;

//Three rays against a TLAS holding TWO instances of the same triangle, the second translated +2 along X.
//Thread 0 aims into instance 0 and must hit, thread 1 aims into instance 1 and must hit, thread 2 aims away
// from both and must miss.
//Writing all three outcomes means a shader that always reports a hit, or always a miss, fails rather than
// passing part of it by accident.
//Thread 1 is the one that matters most: it only hits if the SECOND instance received its acceleration
// structure reference, which is exactly what vk_tlas.c used to get wrong by writing every instance's
// reference into instance 0's slot.
//App data: [0] = bindless write handle of the output buffer, [1] = bindless handle of the TLAS.

[[oxc::extension("RayQuery")]]
[[oxc::model("6.5")]]
[shader("compute")]
[numthreads(3, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	if(i >= 3)
		return;

	RayDesc ray;
	ray.TMin = 0;
	ray.TMax = 10;

	//Instance 0 covers x in [0, 1].
	//Instance 1 is the same triangle translated to x in [2, 3].
	//Both are aimed at from z = -1 along +Z, so they differ only in which instance can answer.

	ray.Origin = F32x3(i == 1 ? 2.25f : 0.25f, 0.25f, -1);

	//Thread 2 fires along -Z, away from both instances.

	ray.Direction = i == 2 ? F32x3(0, 0, -1) : F32x3(0, 0, 1);

	RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
	query.TraceRayInline(tlasExtUniform(_push.aux), RAY_FLAG_NONE, 0xFF, ray);
	query.Proceed();

	const U32 hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 1 : 0;

	setAtUniform<U32>(_push.output, i << 2, hit);
}
