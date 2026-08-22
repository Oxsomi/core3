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

//RayReorder (SM6.9 Shader Execution Reordering) as a C style oxc:: API over one oxc::HitObject handle.
//DXIL: oxc::HitObject is the native dx::HitObject and every function is a thin wrapper over its methods.
//SPIRV: oxc::HitObject is the bare OpTypeHitObjectEXT opaque handle and every function is an inline SPIR-V
// instruction from SPV_EXT_shader_invocation_reorder.
//
//The API is DELIBERATELY free functions over a bare typedef rather than a struct with methods: wrapping the
// opaque hit object type in a struct makes DXC emit OpTypeStruct containing OpTypeHitObjectEXT plus access
// chains into it, which spirv-val accepts (the spec forbids loads/stores of hit objects but never composite
// containment) and which crashes the driver's compiler outright.
//A typedef is transparent and safe.
//A composite is not.
//
//Usage:
//	OXC_HITOBJECT(hit);
//	oxc::HitObject_TraceRay(hit, tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
//	oxc::MaybeReorderThread(hit);
//	oxc::HitObject_Invoke(hit, payload);
//On SPIR-V every function takes the handle by reference and never copies it, since the handle is opaque.
//On DXIL creators take it out, reads take it by value and the set macro below mutates it in place.

//TraceRay / FromRayQuery / Invoke / GetAttributes take a payload or attributes struct that SPIR-V requires
// to be an OpVariable of a specific storage class.
//Decorate that variable with these.
//On SPIRV they apply the storage class and on DXIL they're a no-op (a vk:: attribute), so the same
// declaration compiles on both backends.
#ifdef __spirv__
	#define OXC_RAYPAYLOAD           [[vk::ext_storage_class(/* RayPayloadKHR */ 5338)]]
	#define OXC_HITOBJECT_ATTRIBUTES [[vk::ext_storage_class(/* HitObjectAttributeEXT */ 5411)]]
#else
	#define OXC_RAYPAYLOAD
	#define OXC_HITOBJECT_ATTRIBUTES
#endif

//Declares a hit object local.
//The split exists because the two backends have OPPOSITE initialization rules.
//DXC's DXIL validation rejects any use of an uninitialized dx::HitObject, and its definedness tracking
// doesn't follow a wrapper's out parameter either, so DXIL initializes to a NOP hit object.
//On SPIR-V the handle is an opaque type that must never be stored to outside the SER instructions, so it
// stays bare.
#ifdef __spirv__
	#define OXC_HITOBJECT(name) oxc::HitObject name
#else
	#define OXC_HITOBJECT(name) oxc::HitObject name = dx::HitObject::MakeNop()
#endif

//Sets the shader table record index of an existing hit object.
//A macro out of necessity: the DXIL intrinsic takes the hit object by value and returns the new one, and
// DXC feeds an UNDEF hit object into it whenever the call goes through any inout wrapper, even one that
// copies to a local first (validator: "HitObject is undef"), so the method has to be called directly on the
// caller's own local.
//SPIRV has no such problem and goes through the normal function wrapper.
#ifdef __spirv__
	#define OXC_HITOBJECT_SET_SHADER_TABLE_INDEX(h, recordIndex) oxc::HitObject_SetShaderTableIndex(h, recordIndex)
#else
	#define OXC_HITOBJECT_SET_SHADER_TABLE_INDEX(h, recordIndex) (h).SetShaderTableIndex(recordIndex)
#endif

namespace oxc {

#ifdef __spirv__

	#define OXC_SER_OP(op) \
		[[vk::ext_capability(/* ShaderInvocationReorderEXT */ 5388)]] \
		[[vk::ext_extension("SPV_EXT_shader_invocation_reorder")]] \
		[[vk::ext_instruction(op)]]

	typedef vk::SpirvOpaqueType</* OpTypeHitObjectEXT */ 5313> HitObject;

	//The raw instructions.
	//The public API below wraps these so RayDesc packing and naming match DXIL.

	OXC_SER_OP(/* OpHitObjectRecordEmptyEXT */ 5318)
	void __hoRecordEmpty([[vk::ext_reference]] HitObject h);

	OXC_SER_OP(/* OpHitObjectRecordMissEXT */ 5305)
	void __hoRecordMiss(
		[[vk::ext_reference]] HitObject h, uint rayFlags, uint sbtRecordIndex,
		float3 origin, float tMin, float3 direction, float tMax
	);

	template<typename Payload>
	OXC_SER_OP(/* OpHitObjectTraceRayEXT */ 5316)
	void __hoTraceRay(
		[[vk::ext_reference]] HitObject h, RaytracingAccelerationStructure as, uint rayFlags, uint cullMask,
		uint sbtOffset, uint sbtStride, uint missIndex, float3 origin, float tMin, float3 direction, float tMax,
		[[vk::ext_reference]] Payload payload
	);

	OXC_SER_OP(/* OpHitObjectRecordFromQueryEXT */ 5304)
	void __hoRecordFromQuery(
		[[vk::ext_reference]] HitObject h, [[vk::ext_reference]] RayQuery<RAY_FLAG_NONE> rayQuery, uint sbtRecordIndex,
		[[vk::ext_reference]] BuiltInTriangleIntersectionAttributes attributes
	);

	template<typename Payload>
	OXC_SER_OP(/* OpHitObjectExecuteShaderEXT */ 5319)
	void __hoExecuteShader([[vk::ext_reference]] HitObject h, [[vk::ext_reference]] Payload payload);

	OXC_SER_OP(/* OpReorderThreadWithHitObjectEXT */ 5315)
	void __reorderWithHitObject([[vk::ext_reference]] HitObject h, uint hint, uint hintBits);

	OXC_SER_OP(/* OpReorderThreadWithHintEXT */ 5314)
	void __reorderWithHint(uint hint, uint hintBits);

	OXC_SER_OP(/* OpHitObjectIsHitEXT */ 5351)   bool  __hoIsHit  ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectIsMissEXT */ 5352)  bool  __hoIsMiss ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectIsEmptyEXT */ 5350) bool  __hoIsNop  ([[vk::ext_reference]] HitObject h);

	OXC_SER_OP(/* OpHitObjectGetRayFlagsEXT */ 5308)             uint __hoGetRayFlags       ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetRayTMinEXT */ 5347)              float __hoGetRayTMin        ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetRayTMaxEXT */ 5333)              float __hoGetRayTCurrent    ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetWorldRayOriginEXT */ 5330)      float3 __hoGetWorldRayOrigin([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetWorldRayDirectionEXT */ 5329)   float3 __hoGetWorldRayDir   ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetObjectRayOriginEXT */ 5327)     float3 __hoGetObjectRayOrigin([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetObjectRayDirectionEXT */ 5328)  float3 __hoGetObjectRayDir  ([[vk::ext_reference]] HitObject h);
	//These return a 4-column x 3-row matrix (SPIR-V mat4v3float, i.e. HLSL float4x3); the 3x4 accessors transpose.
	OXC_SER_OP(/* OpHitObjectGetObjectToWorldEXT */ 5331)       float4x3 __hoGetObjectToWorld([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetWorldToObjectEXT */ 5332)       float4x3 __hoGetWorldToObject([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetInstanceIdEXT */ 5325)          uint __hoGetInstanceIndex   ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetInstanceCustomIndexEXT */ 5326) uint __hoGetInstanceID      ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetGeometryIndexEXT */ 5324)       uint __hoGetGeometryIndex   ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetPrimitiveIndexEXT */ 5323)      uint __hoGetPrimitiveIndex  ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetHitKindEXT */ 5322)            uint __hoGetHitKind         ([[vk::ext_reference]] HitObject h);
	OXC_SER_OP(/* OpHitObjectGetShaderBindingTableRecordIndexEXT */ 5348) uint __hoGetShaderTableIndex([[vk::ext_reference]] HitObject h);

	OXC_SER_OP(/* OpHitObjectSetShaderBindingTableRecordIndexEXT */ 5309)
	void __hoSetShaderTableIndex([[vk::ext_reference]] HitObject h, uint index);

	template<typename Attr>
	OXC_SER_OP(/* OpHitObjectGetAttributesEXT */ 5321)
	void __hoGetAttributes([[vk::ext_reference]] HitObject h, [[vk::ext_reference]] Attr attributes);

	#undef OXC_SER_OP
	//Public API

	void HitObject_MakeNop(inout HitObject h) { __hoRecordEmpty(h); }

	void HitObject_MakeMiss(inout HitObject h, uint rayFlags, uint missShaderIndex, RayDesc ray) {
		__hoRecordMiss(h, rayFlags, missShaderIndex, ray.Origin, ray.TMin, ray.Direction, ray.TMax);
	}

	template<typename Payload>
	void HitObject_TraceRay(
		inout HitObject h,
		RaytracingAccelerationStructure as, uint rayFlags, uint instanceInclusionMask,
		uint rayContributionToHitGroupIndex, uint multiplierForGeometryContributionToHitGroupIndex,
		uint missShaderIndex, RayDesc ray, inout Payload payload
	) {
		__hoTraceRay(
			h, as, rayFlags, instanceInclusionMask, rayContributionToHitGroupIndex,
			multiplierForGeometryContributionToHitGroupIndex, missShaderIndex,
			ray.Origin, ray.TMin, ray.Direction, ray.TMax, payload
		);
	}

	//Only valid with a committed triangle hit in the query: the EXT form leaves a query without a committed
	// hit unspecified, so record a miss with HitObject_MakeMiss instead of converting one.
	//The attributes operand is ignored for triangle hits (the committed barycentrics are used), but it still
	// has to be a HitObjectAttributeEXT storage class variable, so declare it with OXC_HITOBJECT_ATTRIBUTES.
	//AABB hits would additionally need the optional hit kind operand, which isn't plumbed.
	//The record index is part of the record on SPIRV, which is why it's a parameter here unlike
	// dx::HitObject::FromRayQuery.
	//The DXIL side applies it with SetShaderTableIndex to match.
	//Nothing here is a template: DXC refuses a RayQuery parameter inside a template function even when the
	// type is spelled concretely, so the query is fixed to RayQuery<RAY_FLAG_NONE> (pass template flags
	// dynamically or add an overload) and the ignored-for-triangles attributes are the builtin type.

	void HitObject_FromRayQuery(
		inout HitObject h, inout RayQuery<RAY_FLAG_NONE> rayQuery, uint sbtRecordIndex,
		inout BuiltInTriangleIntersectionAttributes attributes
	) {
		__hoRecordFromQuery(h, rayQuery, sbtRecordIndex, attributes);
	}

	template<typename Payload>
	void HitObject_Invoke(inout HitObject h, inout Payload payload) { __hoExecuteShader(h, payload); }

	void MaybeReorderThread(inout HitObject h) { __reorderWithHitObject(h, 0, 0); }
	void MaybeReorderThread(inout HitObject h, uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		__reorderWithHitObject(h, coherenceHint, numCoherenceHintBitsFromLSB);
	}
	void MaybeReorderThread(uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		__reorderWithHint(coherenceHint, numCoherenceHintBitsFromLSB);
	}

	bool HitObject_IsHit (inout HitObject h) { return __hoIsHit(h); }
	bool HitObject_IsMiss(inout HitObject h) { return __hoIsMiss(h); }
	bool HitObject_IsNop (inout HitObject h) { return __hoIsNop(h); }

	uint HitObject_GetRayFlags           (inout HitObject h) { return __hoGetRayFlags(h); }
	float HitObject_GetRayTMin           (inout HitObject h) { return __hoGetRayTMin(h); }
	float HitObject_GetRayTCurrent       (inout HitObject h) { return __hoGetRayTCurrent(h); }
	float3 HitObject_GetWorldRayOrigin   (inout HitObject h) { return __hoGetWorldRayOrigin(h); }
	float3 HitObject_GetWorldRayDirection(inout HitObject h) { return __hoGetWorldRayDir(h); }
	float3 HitObject_GetObjectRayOrigin  (inout HitObject h) { return __hoGetObjectRayOrigin(h); }
	float3 HitObject_GetObjectRayDirection(inout HitObject h) { return __hoGetObjectRayDir(h); }
	float3x4 HitObject_GetObjectToWorld3x4(inout HitObject h) { return transpose(__hoGetObjectToWorld(h)); }
	float4x3 HitObject_GetObjectToWorld4x3(inout HitObject h) { return __hoGetObjectToWorld(h); }
	float3x4 HitObject_GetWorldToObject3x4(inout HitObject h) { return transpose(__hoGetWorldToObject(h)); }
	float4x3 HitObject_GetWorldToObject4x3(inout HitObject h) { return __hoGetWorldToObject(h); }
	uint HitObject_GetInstanceIndex      (inout HitObject h) { return __hoGetInstanceIndex(h); }
	uint HitObject_GetInstanceID         (inout HitObject h) { return __hoGetInstanceID(h); }
	uint HitObject_GetGeometryIndex     (inout HitObject h) { return __hoGetGeometryIndex(h); }
	uint HitObject_GetPrimitiveIndex     (inout HitObject h) { return __hoGetPrimitiveIndex(h); }
	uint HitObject_GetHitKind            (inout HitObject h) { return __hoGetHitKind(h); }
	uint HitObject_GetShaderTableIndex   (inout HitObject h) { return __hoGetShaderTableIndex(h); }

	void HitObject_SetShaderTableIndex(inout HitObject h, uint recordIndex) { __hoSetShaderTableIndex(h, recordIndex); }

	template<typename Attr>
	void HitObject_GetAttributes(inout HitObject h, inout Attr attributes) { __hoGetAttributes(h, attributes); }

#else

	typedef dx::HitObject HitObject;

	//Parameter directions differ from the SPIR-V branch on purpose, and call sites can't tell.
	//Creators take the handle OUT, since an inout would read the caller's uninitialized object in, which DXC
	// rejects as an undef use, and its tracking doesn't follow the copy back either.
	//Everything else takes it BY VALUE: dx::HitObject copies are legal on DXIL, while inout trips the same
	// undef analysis on the copy in.
	//On SPIR-V nothing may ever copy or initialize the opaque handle, so there everything is a reference.

	void HitObject_MakeNop(out HitObject h) { h = dx::HitObject::MakeNop(); }

	void HitObject_MakeMiss(out HitObject h, uint rayFlags, uint missShaderIndex, RayDesc ray) {
		h = dx::HitObject::MakeMiss(rayFlags, missShaderIndex, ray);
	}

	template<typename Payload>
	void HitObject_TraceRay(
		out HitObject h,
		RaytracingAccelerationStructure as, uint rayFlags, uint instanceInclusionMask,
		uint rayContributionToHitGroupIndex, uint multiplierForGeometryContributionToHitGroupIndex,
		uint missShaderIndex, RayDesc ray, inout Payload payload
	) {
		h = dx::HitObject::TraceRay(
			as, rayFlags, instanceInclusionMask, rayContributionToHitGroupIndex,
			multiplierForGeometryContributionToHitGroupIndex, missShaderIndex, ray, payload
		);
	}

	//The attributes are unused here: dx::HitObject::FromRayQuery takes them from the committed triangle hit,
	// exactly what the SPIRV form does with its ignored-for-triangles attributes operand.
	//Non-template with a concrete query type for the same DXC limitation the SPIRV side documents.

	void HitObject_FromRayQuery(
		out HitObject h, inout RayQuery<RAY_FLAG_NONE> rayQuery, uint sbtRecordIndex,
		inout BuiltInTriangleIntersectionAttributes attributes
	) {
		h = dx::HitObject::FromRayQuery(rayQuery);
		h.SetShaderTableIndex(sbtRecordIndex);
	}

	template<typename Payload>
	void HitObject_Invoke(HitObject h, inout Payload payload) { dx::HitObject::Invoke(h, payload); }

	void MaybeReorderThread(HitObject h) { dx::MaybeReorderThread(h); }
	void MaybeReorderThread(HitObject h, uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		dx::MaybeReorderThread(h, coherenceHint, numCoherenceHintBitsFromLSB);
	}
	void MaybeReorderThread(uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		dx::MaybeReorderThread(coherenceHint, numCoherenceHintBitsFromLSB);
	}

	bool HitObject_IsHit (HitObject h) { return h.IsHit(); }
	bool HitObject_IsMiss(HitObject h) { return h.IsMiss(); }
	bool HitObject_IsNop (HitObject h) { return h.IsNop(); }

	uint HitObject_GetRayFlags           (HitObject h) { return h.GetRayFlags(); }
	float HitObject_GetRayTMin           (HitObject h) { return h.GetRayTMin(); }
	float HitObject_GetRayTCurrent       (HitObject h) { return h.GetRayTCurrent(); }
	float3 HitObject_GetWorldRayOrigin   (HitObject h) { return h.GetWorldRayOrigin(); }
	float3 HitObject_GetWorldRayDirection(HitObject h) { return h.GetWorldRayDirection(); }
	float3 HitObject_GetObjectRayOrigin  (HitObject h) { return h.GetObjectRayOrigin(); }
	float3 HitObject_GetObjectRayDirection(HitObject h) { return h.GetObjectRayDirection(); }
	float3x4 HitObject_GetObjectToWorld3x4(HitObject h) { return h.GetObjectToWorld3x4(); }
	float4x3 HitObject_GetObjectToWorld4x3(HitObject h) { return h.GetObjectToWorld4x3(); }
	float3x4 HitObject_GetWorldToObject3x4(HitObject h) { return h.GetWorldToObject3x4(); }
	float4x3 HitObject_GetWorldToObject4x3(HitObject h) { return h.GetWorldToObject4x3(); }
	uint HitObject_GetInstanceIndex      (HitObject h) { return h.GetInstanceIndex(); }
	uint HitObject_GetInstanceID         (HitObject h) { return h.GetInstanceID(); }
	uint HitObject_GetGeometryIndex     (HitObject h) { return h.GetGeometryIndex(); }
	uint HitObject_GetPrimitiveIndex     (HitObject h) { return h.GetPrimitiveIndex(); }
	uint HitObject_GetHitKind            (HitObject h) { return h.GetHitKind(); }
	uint HitObject_GetShaderTableIndex   (HitObject h) { return h.GetShaderTableIndex(); }

	//HitObject_SetShaderTableIndex deliberately doesn't exist on DXIL: any inout wrapper hands the by-value
	// intrinsic an undef hit object, so use OXC_HITOBJECT_SET_SHADER_TABLE_INDEX (top of this file) instead.

	template<typename Attr>
	void HitObject_GetAttributes(HitObject h, inout Attr attributes) { h.GetAttributes(attributes); }

#endif

	//A hit object's triangle vertex positions need SER *and* SM6.10 position fetch, so they're only exposed when
	//the RayTriPosition extension is enabled too (on SPIRV the op requires both capabilities/extensions). Returns
	//the hit triangle's object-space vertices; oxc::TrianglePositions comes from @extension.RayTriPosition.hlsli.
#ifdef __OXC_EXT_RAYTRIPOSITION

	#ifdef __spirv__

		//__TrianglePositionArray comes from @extension.RayTriPosition.hlsli (included first when this block is on);
		//HLSL can't return a bare float3[3] from a function, so an array typedef is required either way.
		[[vk::ext_capability(/* ShaderInvocationReorderEXT */ 5388)]]
		[[vk::ext_extension("SPV_EXT_shader_invocation_reorder")]]
		[[vk::ext_capability(/* RayTracingPositionFetchKHR */ 5336)]]
		[[vk::ext_extension("SPV_KHR_ray_tracing_position_fetch")]]
		[[vk::ext_instruction(/* OpHitObjectGetIntersectionTriangleVertexPositionsEXT */ 5307)]]
		__TrianglePositionArray __hoTriangleObjectPositions([[vk::ext_reference]] HitObject h);

		TrianglePositions HitObject_TrianglePositions(inout HitObject h) {
			__TrianglePositionArray p = __hoTriangleObjectPositions(h);
			TrianglePositions result;
			result.p0 = p[0]; result.p1 = p[1]; result.p2 = p[2];
			return result;
		}

	#else

		TrianglePositions HitObject_TrianglePositions(HitObject h) {
			BuiltInTrianglePositions p = h.TriangleObjectPositions();
			TrianglePositions result;
			result.p0 = p.p0; result.p1 = p.p1; result.p2 = p.p2;
			return result;
		}

	#endif

#endif

}
