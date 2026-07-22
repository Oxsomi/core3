/* OxC3/RT Core(Oxsomi core 3/RT Core), a general framework for raytracing applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "resource_bindings.hlsli"
#include "camera.hlsli"
#include "atmosphere.hlsli"

struct ColorPayload {
	F32x3 color;
	F32 hitT;
};

F32x3 expandBary(F32x2 bary) {
	return F32x3(1 - bary.x - bary.y, bary);
}

[shader("miss")]
void mainMiss(inout ColorPayload payload) {

	RayDesc ray = createRay(WorldRayOrigin(), 0, WorldRayDirection(), 1e38);

	F32x3 sunDir = getAppData3f(EResourceBinding_SunDirXYZ);

	F32x3 color = Atmosphere::earth(sunDir).getContribution(ray);

	payload.color = color;
	payload.hitT = -1;
}

[shader("closesthit")]
void mainClosestHit(inout ColorPayload payload, BuiltInTriangleIntersectionAttributes attr) {

	F32x3 sunDir = getAppData3f(EResourceBinding_SunDirXYZ);

	F32x3 diffuse = Atmosphere::earth(sunDir).getSunContribution(F32x3(0, 0, 1)) * F32x3(expandBary(attr.barycentrics));

	F32x3 emissive = 100000 * F32x3(0, 0, 1);

	payload.color = diffuse + emissive;

	payload.hitT = RayTCurrent();
}

[shader("raygeneration")]
void mainRaygen() {

	RWTexture2D<unorm F32x4> tex = rwTexture2DUniform(getAppData1u(EResourceBinding_RenderTargetRW));

	U32x2 id = DispatchRaysIndex().xy;
	U32x2 dims = DispatchRaysDimensions().xy;
	
	U32 orientation = getAppData1u(EResourceBinding_Orientation);

	U32x2 ogId = id;

	switch(orientation) {

		case 90:
			dims = dims.yx;
			id = id.yx;
			id.y = dims.y - 1 - id.y;
			break;

		case 180:
			id = dims - 1 - id;
			break;

		case 270:
			dims = dims.yx;
			id = id.yx;
			id.x = dims.x - 1 - id.x;
			break;
	}

	//Generate matrices

	F32 aspect = (F32) dims.x / dims.y;

	F32 localTime = _time * 0.5;

	F32x3 camPos = getAppData3f(EResourceBinding_CamPosXYZ);

	Camera cam;
	cam.v = F32x4x4_lookDir(camPos, F32x3(0, 0, -1), F32x3(0, 1, 0));
	cam.p = F32x4x4_perspective(45 * F32_degToRad, aspect, 0.1, 100);

	cam.vp = mul(cam.v, cam.p);

	cam.vInv = inverseSlow(cam.v);
	cam.pInv = inverseSlow(cam.p);
	cam.vpInv = inverseSlow(cam.vp);

	//RayDesc ray = cam.getRay(id, dims);

	F32x2 uv = (id + 0.5) / dims;
    uv.y = 1 - uv.y;		//Flip to avoid overlap with inline RT

	/*F32 scale = tan(45 * F32_degToRad);
	uv.x = (2 * uv.x - 1) * aspect * scale;
	uv.y = (1 - 2 * uv.y) * scale;*/

	//ray.Origin = ray.Origin;
	//ray.Direction = normalize(F32x3(uv, -1));


	//Trace against

	U32 tlasId = getAppData1u(EResourceBinding_TLAS);
	RayDesc ray = { F32x3(uv * 10 - 5, 5), 0, F32x3(0, 0, -1), 1e6 };

	if(!tlasId)
		ray.TMax = 0;		//Deactivate ray

	U32 flags = RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

	ColorPayload payload;
	TraceRay(tlasExtUniform(tlasId), flags, 0xFF, 0, 0, 0, ray, payload);

	/*Sphere sphere = (Sphere) 0;
	sphere.pos = F32x3(0, 0, -1);		//1 unit behind our traced object
	sphere.rad = 1;

	F32x3 tmp = F32x3(payload.hitT, 0.xx);
	Bool isBackSide;
	if(sphere.intersects(ray, tmp, isBackSide)) {

		F32x3 norm = normalize(posOnRay(ray, tmp.x) - sphere.pos);

		F32x3 sunDir = getAppData3f(EResourceBinding_SunDirXYZ);

		payload.color = Atmosphere::earth(sunDir).getSunContribution(norm) * F32x3(1, 0.5, 0.25);
	}*/

	F32 exposure = exp2(-14);

	if (payload.hitT >= 0)
		tex[ogId] = F32x4(payload.color * exposure, 1);
}
