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

//The bindful twin of test_rays: the TLAS and output buffer come from classic registers instead of bindless
// handles, so ray tracing itself is proven independent of bindless.
//Same scene and rays as test_rays: two origins above the triangle's interior hit, two outside miss.

RaytracingAccelerationStructure scene : register(t0, space0);
RWByteAddressBuffer output : register(u1, space0);

struct HitPayload {
	uint hit;
};

[shader("miss")]
void mainMiss(inout HitPayload payload) {
	payload.hit = 0;
}

[shader("closesthit")]
void mainClosestHit(inout HitPayload payload, BuiltInTriangleIntersectionAttributes attr) {
	float2 bary = attr.barycentrics;
	payload.hit = all(bary >= 0) && bary.x + bary.y <= 1.001 ? 1 : 2;
}

static const float2 rayOrigin[4] = {
	float2(0.25, 0.25),
	float2(0.1, 0.2),
	float2(0.9, 0.9),
	float2(-0.5, 0.5)
};

[shader("raygeneration")]
void mainRaygen() {

	uint i = min(DispatchRaysIndex().x, 3);

	RayDesc ray;
	ray.Origin = float3(rayOrigin[i], 5);
	ray.Direction = float3(0, 0, -1);
	ray.TMin = 0;
	ray.TMax = 1e6;

	HitPayload payload;
	payload.hit = 0xDEAD;

	TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

	output.Store(i << 2, payload.hit);
}
