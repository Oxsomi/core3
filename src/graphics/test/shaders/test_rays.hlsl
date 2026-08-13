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

#include "@appdata.hlsli"
#include "@buffer.hlsli"

//Each ray writes whether it hit, so the readback can check hits and misses ran through the real pipeline.

struct HitPayload {
	U32 hit;
};

[shader("miss")]
void mainMiss(inout HitPayload payload) {
	payload.hit = 0;
}

//The barycentrics have to be inside the triangle, so consuming them also proves the attributes are plumbed.
//A hit with broken attributes writes 2, which the readback distinguishes from both a hit and a miss.

[shader("closesthit")]
void mainClosestHit(inout HitPayload payload, BuiltInTriangleIntersectionAttributes attr) {
	F32x2 bary = attr.barycentrics;
	payload.hit = all(bary >= 0) && bary.x + bary.y <= 1.001 ? 1 : 2;
}

//The scene is one triangle at (0,0,0), (1,0,0), (0,1,0), so which origins hit is known exactly.
//Rays 0 and 1 start above the triangle's interior, rays 2 and 3 start outside of it.
//App data: [0] = bindless write handle of the result buffer, [1] = bindless TLAS id.

static const F32x2 rayOrigin[4] = {
	F32x2(0.25, 0.25),
	F32x2(0.1, 0.2),
	F32x2(0.9, 0.9),
	F32x2(-0.5, 0.5)
};

[shader("raygeneration")]
void mainRaygen() {

	U32 i = min(DispatchRaysIndex().x, 3);

	RayDesc ray;
	ray.Origin = F32x3(rayOrigin[i], 5);
	ray.Direction = F32x3(0, 0, -1);
	ray.TMin = 0;
	ray.TMax = 1e6;

	HitPayload payload;
	payload.hit = 0xDEAD;

	TraceRay(tlasExtUniform(getAppData1u(1)), RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

	setAtUniform(getAppData1u(0), i << 2, payload.hit);
}
