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

//Inline raytracing against ONE bottom level structure holding SEVERAL geometries.
//
//A geometry is what GeometryIndex() names, and it is how a mesh with more than one material finds the right
// one without spending a word per triangle.
//The three triangles are spread along x so a ray picks exactly one of them, and each thread writes the index
// it committed. A structure that collapsed its geometries into one reports 0 three times and fails here
// rather than passing by accident, and one that reordered them reports the wrong index.

#include "@buffer.hlsli"
#include "@resources.hlsli"

//Per dispatch data this shader reads, declared as a push constant.
//Scalars rather than an array: on DXIL each array element takes its own 16 byte cbuffer row, so the size the
//work op checks would not match what the shader declares.

struct BlasGeometryPush {
	U32 output;        //Bindless write handle of the output buffer
	U32 tlas;          //Bindless handle of the acceleration structure
	U32 padding0, padding1;
};

PUSH_CONSTANT BlasGeometryPush _push;

//Eight threads, the same four rays traced twice.
//Threads 0..3 take the geometry flags as they are, so every triangle answers and the committed index names
// which geometry was hit.
//Threads 4..7 repeat them with RAY_FLAG_CULL_OPAQUE. Only geometry 1 is built with DisableAnyHit, which is
// the opaque bit, so only it is culled. A BLAS that still applied one flag set to all of its geometries
// would cull either all three or none, and both of those fail here.

[[oxc::extension("RayQuery")]]
[[oxc::model("6.5")]]
[shader("compute")]
[numthreads(8, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	if(i >= 8)
		return;

	const U32 lane = i & 3;
	const Bool cullOpaque = i >= 4;

	RayDesc ray;
	ray.TMin = 0;
	ray.TMax = 10;

	//Geometry g covers x in [g * 2, g * 2 + 1], so an origin at 0.25 + g * 2 lands inside geometry g.
	//Lane 3 aims past the last of them and has to miss, which is what separates a real index from a
	// structure that answers every ray.

	ray.Origin = F32x3(0.25f + lane * 2.0f, 0.25f, -1);
	ray.Direction = F32x3(0, 0, 1);

	//No FORCE_OPAQUE here, unlike the other ray query tests: forcing it would overwrite the very geometry
	// flag under test. A non opaque candidate is therefore committed by hand, which is what an anyHit that
	// accepts everything amounts to.

	RayQuery<RAY_FLAG_NONE> query;
	query.TraceRayInline(tlasExtUniform(_push.tlas), cullOpaque ? RAY_FLAG_CULL_OPAQUE : RAY_FLAG_NONE, 0xFF, ray);

	while(query.Proceed())
		if(query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
			query.CommitNonOpaqueTriangleHit();

	const U32 committed =
		query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? query.CommittedGeometryIndex() : 0xFFFFFFFFu;

	setAtUniform<U32>(_push.output, i << 2, committed);
}
