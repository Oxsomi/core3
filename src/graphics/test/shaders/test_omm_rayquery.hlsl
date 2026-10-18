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

//Opacity micromaps through an inline ray query, from a compute entry and from a raygen.
//Each entry calls OXC_ENABLE_OPACITY_MICROMAP() in its own body (not from the shared helper).
//Compute and RT entries are split by __OXC_EXT_RAYTRACING, as in separate_rt_and_compute.hlsl.

#include "@extensions.hlsli"
#include "@buffer.hlsli"
#include "@resources.hlsli"

//Scalars, not an array: DXIL pads each array element to a 16 byte row

struct OmmPush {
	U32 output;
	U32 tlas;
	U32 instances;     //Four rays each
	U32 padding0;
};

PUSH_CONSTANT OmmPush _push;

//test_rays.hlsl's four rays (0 and 1 inside the triangle, 2 and 3 outside); instance k is shifted 2k along x

static const F32x2 rayOrigin[4] = {
	F32x2(0.25, 0.25),
	F32x2(0.1, 0.2),
	F32x2(0.9, 0.9),
	F32x2(-0.5, 0.5)
};

//0 = miss, 1 = hit on the aimed instance, 2 = hit on another. Non opaque candidates are committed (as without
// an anyHit), so an ignored micromap shows up as a hit.

void traceProbe(U32 i) {

	if(i >= _push.instances * 4)
		return;

	const U32 instance = i >> 2;

	RayDesc ray;
	ray.Origin = F32x3(rayOrigin[i & 3] + F32x2((F32) instance * 2, 0), 5);
	ray.Direction = F32x3(0, 0, -1);
	ray.TMin = 0;
	ray.TMax = 1e6;

	RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> query;
	query.TraceRayInline(tlasExtUniform(_push.tlas), RAY_FLAG_NONE, 0xFF, ray);

	while(query.Proceed())
		if(query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
			query.CommitNonOpaqueTriangleHit();

	U32 result = 0;

	if(query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		result = query.CommittedInstanceIndex() == instance ? 1 : 2;

	setAtUniform<U32>(_push.output, i << 2, result);
}

#if !defined(__OXC_EXT_RAYTRACING) || defined(__OXC_PREPROCESS)

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {
	OXC_ENABLE_OPACITY_MICROMAP();
	traceProbe(i);
}

#endif

#if defined(__OXC_EXT_RAYTRACING) || defined(__OXC_PREPROCESS)

struct [raypayload] OmmPayload {
	U32 hit : read(caller) : write(caller, miss, closesthit);
};

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("raygeneration")]
void mainRaygen() {
	OXC_ENABLE_OPACITY_MICROMAP();
	traceProbe(DispatchRaysIndex().x);
}

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("miss")]
void mainMiss(inout OmmPayload payload) {
	payload.hit = 0;
}

[[oxc::extension("RayQuery", "RayMicromapOpacity")]]
[[oxc::model("6.9")]]
[shader("closesthit")]
void mainClosestHit(inout OmmPayload payload, BuiltInTriangleIntersectionAttributes attr) {
	payload.hit = 1;
}

#endif
