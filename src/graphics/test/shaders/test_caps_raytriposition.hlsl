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

//Capability execution shader: triangle vertex position fetch from an inline ray query.
//
//Position fetch hands back the OBJECT space vertices of the committed triangle, read straight out of the
// acceleration structure, so the values are the exact floats the BLAS was built from and a bit exact compare
// is legitimate here where it wouldn't be for a computed quantity like ray T.
//The BLAS has to be built with ERTASBuildFlags_AllowDataAccessExt or the data simply isn't kept; the harness
// sets that flag whenever the device claims the capability.

#include "@appdata.hlsli"
#include "@buffer.hlsli"
#include "@resources.hlsli"
#include "@extensions.hlsli"

//Thread 0 traces the instance at the origin, thread 1 the one translated +2 along X.
//Both must see the SAME object space positions, which is itself part of the test: a fetch returning world
// space positions gives thread 1 an X shifted by 2 and fails.
//App data: [0] = bindless write handle of the output buffer, [1] = bindless handle of the TLAS.

[[oxc::extension("RayQuery", "RayTriPosition")]]
[[oxc::model("6.10")]]
[shader("compute")]
[numthreads(2, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	if(i >= 2)
		return;

	RayDesc ray;
	ray.Origin = F32x3(i == 1 ? 2.25f : 0.25f, 0.25f, -1);
	ray.Direction = F32x3(0, 0, 1);
	ray.TMin = 0;
	ray.TMax = 10;

	RayQuery<RAY_FLAG_NONE> query;
	query.TraceRayInline(tlasExtUniform(getAppData1u(1)), RAY_FLAG_NONE, 0xFF, ray);
	query.Proceed();

	U32 ok = 0;

	if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {

		const oxc::TrianglePositions p = oxc::CommittedTriangleObjectPositions(query);

		//The exact vertices the BLAS was built from, in stored order.

		ok =
			all(p.p0 == F32x3(0, 0, 0)) &&
			all(p.p1 == F32x3(1, 0, 0)) &&
			all(p.p2 == F32x3(0, 1, 0)) ? 1 : 0;
	}

	setAtUniform<U32>(getAppData1u(0), i << 2, ok);
}
