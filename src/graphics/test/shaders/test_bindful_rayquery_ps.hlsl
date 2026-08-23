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


#include "@types.hlsli"

//Inline raytracing from a GRAPHICS stage, which nothing else covers: every other RayQuery test traces from
// compute. That difference is not cosmetic on D3D12, where the acceleration structure access bits accept
// only a short list of sync scopes and the per stage graphics scopes are not on it, so a TLAS transitioned
// for a pixel shader is exactly the case a compute-only test can never reach.
//Deliberately includes NO bindless headers: the TLAS arrives through a classic register.

RaytracingAccelerationStructure scene : register(t0, space0);

[[oxc::extension("RayQuery")]]
[shader("pixel")]
F32x4 main(F32x4 pos : SV_POSITION) : SV_TARGET {

	//Every pixel fires the same ray straight at the triangle, so the whole target is one exact colour

	RayDesc ray;
	ray.Origin = F32x3(0.25f, 0.25f, -1);
	ray.Direction = F32x3(0, 0, 1);
	ray.TMin = 0;
	ray.TMax = 10;

	RayQuery<RAY_FLAG_FORCE_OPAQUE> query;
	query.TraceRayInline(scene, RAY_FLAG_NONE, 0xFF, ray);
	query.Proceed();

	//0xFF3366FF when the inline trace hit, which is what the pixel compare expects

	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? F32x4(1, 102.0 / 255, 51.0 / 255, 1) : F32x4(0, 0, 0, 1);
}
