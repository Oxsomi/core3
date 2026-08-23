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

//The SER (shader execution reordering) twin of test_rays.hlsl: same scene, same 4 rays, same expected
// (1, 1, 0, 0) readback, but the trace is split into record / reorder hint / explicit invoke through
// oxc::HitObject rather than one TraceRay.
//Identical payloads out of both is the whole assertion, since the reorder itself only moves work between
// lanes and may not change any computed value.

#include "@resources.hlsli"
#include "@buffer.hlsli"
#include "@extensions.hlsli"

//Per dispatch data this shader reads, declared as a push constant.
//Scalars rather than an array: on DXIL each array element takes its own 16 byte cbuffer row, so the size the
//work op checks would not match what the shader declares.

struct RaysPush {
	U32 output;        //Bindless write handle of the output buffer
	U32 tlas;          //Bindless handle of the acceleration structure
	U32 padding0, padding1;
};

PUSH_CONSTANT RaysPush _push;

//SM6.9 requires payload access qualifiers, so unlike test_rays.hlsl the payload says exactly who reads and
// writes it: the caller seeds and reads it back, the hit and miss shaders overwrite it.

struct [raypayload] HitPayload {
	U32 hit : read(caller) : write(caller, closesthit, miss);
};

[[oxc::model("6.9")]]
[shader("miss")]
void mainMiss(inout HitPayload payload) {
	payload.hit = 0;
}

//The barycentrics have to be inside the triangle, so consuming them also proves the attributes are plumbed
// through the explicit invoke exactly as they are through a plain TraceRay.

[[oxc::model("6.9")]]
[shader("closesthit")]
void mainClosestHit(inout HitPayload payload, BuiltInTriangleIntersectionAttributes attr) {
	F32x2 bary = attr.barycentrics;
	payload.hit = all(bary >= 0) && bary.x + bary.y <= 1.001 ? 1 : 2;
}

//App data: [0] = bindless write handle of the result buffer, [1] = bindless TLAS id.

static const F32x2 rayOrigin[4] = {
	F32x2(0.25, 0.25),
	F32x2(0.1, 0.2),
	F32x2(0.9, 0.9),
	F32x2(-0.5, 0.5)
};

//RayReorder only exists on raygen, which is also why the annotation sits here and not on the whole file.
//The payload is declared with OXC_RAYPAYLOAD so SPIR-V gets the storage class the hit object ops require.
//The #ifdef matters in a mixed extension file: the miss and closest hit entries compile in a combination
// WITHOUT the RayReorder define, where oxc::HitObject doesn't exist, so the raygen has to vanish from that
// combination's parse. The annotation pass sees every extension defined, so the entry still registers.

#ifdef __OXC_EXT_RAYREORDER

[[oxc::extension("RayReorder", "RayQuery")]]
[[oxc::model("6.9")]]
[shader("raygeneration")]
void mainRaygen() {

	U32 i = min(DispatchRaysIndex().x, 3);

	RayDesc ray;
	ray.Origin = F32x3(rayOrigin[i], 5);
	ray.Direction = F32x3(0, 0, -1);
	ray.TMin = 0;
	ray.TMax = 1e6;

	OXC_RAYPAYLOAD HitPayload payload;
	payload.hit = 0xDEAD;

	//One path for both backends: oxc::HitObject is a bare handle typedef (dx::HitObject on DXIL, the opaque
	// hit object type on SPIR-V) and the C style oxc:: functions wrap each backend's form.
	//The hint is deliberately between record and invoke: on a device that actually reorders
	// (RayReorderActual) this is the point where lanes migrate, and the payload still has to survive it.

	OXC_HITOBJECT(hit);

	oxc::HitObject_TraceRay(hit, tlasExtUniform(_push.tlas), RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
	oxc::MaybeReorderThread(hit);
	oxc::HitObject_Invoke(hit, payload);

	U32 serHit = payload.hit;

	//Second source for the same answer: hits are converted from an inline ray query with FromRayQuery, and
	// misses are rebuilt with MakeMiss, since the EXT form of FromRayQuery leaves an uncommitted query
	// unspecified (DXR defines it as a miss, SPIRV doesn't).
	//Both aim at record 0, exactly where the TraceRay path landed (hit group 0 for hits, miss shader 0 for
	// misses), so every ray has to produce the identical payload again or the sentinel fails the readback.

	RayQuery<RAY_FLAG_NONE> query;
	query.TraceRayInline(tlasExtUniform(_push.tlas), RAY_FLAG_FORCE_OPAQUE, 0xFF, ray);
	query.Proceed();

	OXC_HITOBJECT_ATTRIBUTES BuiltInTriangleIntersectionAttributes attrs;
	attrs.barycentrics = F32x2(0, 0);

	OXC_HITOBJECT(hitRq);

	if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		oxc::HitObject_FromRayQuery(hitRq, query, 0u, attrs);

	else {
		oxc::HitObject_MakeMiss(hitRq, RAY_FLAG_NONE, 0, ray);
		OXC_HITOBJECT_SET_SHADER_TABLE_INDEX(hitRq, 0);
	}

	payload.hit = 0xDEAD;
	oxc::HitObject_Invoke(hitRq, payload);

	U32 ok = payload.hit == serHit && oxc::HitObject_GetShaderTableIndex(hitRq) == 0 ? 1 : 0;
	setAtUniform(_push.output, i << 2, ok ? serHit : 0xBADBAD);
}

#endif
