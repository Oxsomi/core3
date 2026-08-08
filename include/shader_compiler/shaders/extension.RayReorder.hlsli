R"(
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

//RayReorder (SM6.9 Shader Execution Reordering) as one oxc::HitObject mirroring the DXR 1.2 dx::HitObject API.
//DXIL uses the native dx::HitObject; DXC's SPIR-V backend has no lowering for it, so on SPIRV oxc::HitObject wraps
//an opaque OpTypeHitObjectEXT handle and every method is an inline SPIR-V instruction from
//SPV_EXT_shader_invocation_reorder. Shaders use the same oxc:: calls regardless of backend.

//TraceRay / FromRayQuery / Invoke / GetAttributes take a payload or attributes struct that SPIR-V requires to be
//an OpVariable of a specific storage class. Decorate that variable with these; on SPIRV they apply the storage
//class, on DXIL they're a no-op (a vk:: attribute), so the same declaration compiles on both backends.
#ifdef __spirv__
	#define OXC_RAYPAYLOAD           [[vk::ext_storage_class(/* RayPayloadKHR */ 5338)]]
	#define OXC_HITOBJECT_ATTRIBUTES [[vk::ext_storage_class(/* HitObjectAttributeEXT */ 5411)]]
#else
	#define OXC_RAYPAYLOAD
	#define OXC_HITOBJECT_ATTRIBUTES
#endif

namespace oxc {

#ifdef __spirv__

	#define OXC_SER_OP(op) \
		[[vk::ext_capability(/* ShaderInvocationReorderEXT */ 5388)]] \
		[[vk::ext_extension("SPV_EXT_shader_invocation_reorder")]] \
		[[vk::ext_instruction(op)]]

	typedef vk::SpirvOpaqueType</* OpTypeHitObjectEXT */ 5313> __HitObjectHandle;

	//Construction / traversal

	OXC_SER_OP(/* OpHitObjectRecordEmptyEXT */ 5318)
	void __hoRecordEmpty([[vk::ext_reference]] __HitObjectHandle h);

	OXC_SER_OP(/* OpHitObjectRecordMissEXT */ 5305)
	void __hoRecordMiss(
		[[vk::ext_reference]] __HitObjectHandle h, uint rayFlags, uint sbtRecordIndex,
		float3 origin, float tMin, float3 direction, float tMax
	);

	template<typename Payload>
	OXC_SER_OP(/* OpHitObjectTraceRayEXT */ 5316)
	void __hoTraceRay(
		[[vk::ext_reference]] __HitObjectHandle h, RaytracingAccelerationStructure as, uint rayFlags, uint cullMask,
		uint sbtOffset, uint sbtStride, uint missIndex, float3 origin, float tMin, float3 direction, float tMax,
		[[vk::ext_reference]] Payload payload
	);

	template<typename RayQueryT>
	OXC_SER_OP(/* OpHitObjectRecordFromQueryEXT */ 5304)
	void __hoRecordFromQuery([[vk::ext_reference]] __HitObjectHandle h, [[vk::ext_reference]] RayQueryT rayQuery);

	template<typename Payload>
	OXC_SER_OP(/* OpHitObjectExecuteShaderEXT */ 5319)
	void __hoExecuteShader([[vk::ext_reference]] __HitObjectHandle h, [[vk::ext_reference]] Payload payload);

	//Reorder

	OXC_SER_OP(/* OpReorderThreadWithHitObjectEXT */ 5315)
	void __reorderWithHitObject([[vk::ext_reference]] __HitObjectHandle h, uint hint, uint hintBits);

	OXC_SER_OP(/* OpReorderThreadWithHintEXT */ 5314)
	void __reorderWithHint(uint hint, uint hintBits);

	//State queries

	OXC_SER_OP(/* OpHitObjectIsHitEXT */ 5351)   bool  __hoIsHit  ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectIsMissEXT */ 5352)  bool  __hoIsMiss ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectIsEmptyEXT */ 5350) bool  __hoIsNop  ([[vk::ext_reference]] __HitObjectHandle h);

	OXC_SER_OP(/* OpHitObjectGetRayFlagsEXT */ 5308)             uint __hoGetRayFlags       ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetRayTMinEXT */ 5347)              float __hoGetRayTMin        ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetRayTMaxEXT */ 5333)              float __hoGetRayTCurrent    ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetWorldRayOriginEXT */ 5330)      float3 __hoGetWorldRayOrigin([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetWorldRayDirectionEXT */ 5329)   float3 __hoGetWorldRayDir   ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetObjectRayOriginEXT */ 5327)     float3 __hoGetObjectRayOrigin([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetObjectRayDirectionEXT */ 5328)  float3 __hoGetObjectRayDir  ([[vk::ext_reference]] __HitObjectHandle h);
	//These return a 4-column x 3-row matrix (SPIR-V mat4v3float, i.e. HLSL float4x3); the 3x4 accessors transpose.
	OXC_SER_OP(/* OpHitObjectGetObjectToWorldEXT */ 5331)       float4x3 __hoGetObjectToWorld([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetWorldToObjectEXT */ 5332)       float4x3 __hoGetWorldToObject([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetInstanceIdEXT */ 5325)          uint __hoGetInstanceIndex   ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetInstanceCustomIndexEXT */ 5326) uint __hoGetInstanceID      ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetGeometryIndexEXT */ 5324)       uint __hoGetGeometryIndex   ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetPrimitiveIndexEXT */ 5323)      uint __hoGetPrimitiveIndex  ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetHitKindEXT */ 5322)            uint __hoGetHitKind         ([[vk::ext_reference]] __HitObjectHandle h);
	OXC_SER_OP(/* OpHitObjectGetShaderBindingTableRecordIndexEXT */ 5348) uint __hoGetShaderTableIndex([[vk::ext_reference]] __HitObjectHandle h);

	OXC_SER_OP(/* OpHitObjectSetShaderBindingTableRecordIndexEXT */ 5309)
	void __hoSetShaderTableIndex([[vk::ext_reference]] __HitObjectHandle h, uint index);

	template<typename Attr>
	OXC_SER_OP(/* OpHitObjectGetAttributesEXT */ 5321)
	void __hoGetAttributes([[vk::ext_reference]] __HitObjectHandle h, [[vk::ext_reference]] Attr attributes);

	#undef OXC_SER_OP

	struct HitObject {

		__HitObjectHandle _handle;

		//Construction

		static HitObject MakeNop() { HitObject o; __hoRecordEmpty(o._handle); return o; }

		static HitObject MakeMiss(uint rayFlags, uint missShaderIndex, RayDesc ray) {
			HitObject o;
			__hoRecordMiss(o._handle, rayFlags, missShaderIndex, ray.Origin, ray.TMin, ray.Direction, ray.TMax);
			return o;
		}

		//TraceRay / FromRayQuery / Invoke / GetAttributes take a payload or attributes struct; declare that
		//variable with OXC_RAYPAYLOAD / OXC_HITOBJECT_ATTRIBUTES (see top of file) so SPIRV gets the required
		//storage class while DXIL stays a plain local.

		template<typename Payload>
		static HitObject TraceRay(
			RaytracingAccelerationStructure as, uint rayFlags, uint instanceInclusionMask,
			uint rayContributionToHitGroupIndex, uint multiplierForGeometryContributionToHitGroupIndex,
			uint missShaderIndex, RayDesc ray, inout Payload payload
		) {
			HitObject o;
			__hoTraceRay(
				o._handle, as, rayFlags, instanceInclusionMask, rayContributionToHitGroupIndex,
				multiplierForGeometryContributionToHitGroupIndex, missShaderIndex,
				ray.Origin, ray.TMin, ray.Direction, ray.TMax, payload
			);
			return o;
		}

		template<typename RayQueryT>
		static HitObject FromRayQuery(RayQueryT rayQuery) {
			HitObject o; __hoRecordFromQuery(o._handle, rayQuery); return o;
		}

		template<typename Payload>
		static void Invoke(HitObject hitObject, inout Payload payload) {
			__hoExecuteShader(hitObject._handle, payload);
		}

		//State queries

		bool IsMiss() { return __hoIsMiss(_handle); }
		bool IsHit()  { return __hoIsHit(_handle); }
		bool IsNop()  { return __hoIsNop(_handle); }

		uint GetRayFlags()         { return __hoGetRayFlags(_handle); }
		float GetRayTMin()         { return __hoGetRayTMin(_handle); }
		float GetRayTCurrent()     { return __hoGetRayTCurrent(_handle); }
		float3 GetWorldRayOrigin() { return __hoGetWorldRayOrigin(_handle); }
		float3 GetWorldRayDirection() { return __hoGetWorldRayDir(_handle); }
		float3 GetObjectRayOrigin() { return __hoGetObjectRayOrigin(_handle); }
		float3 GetObjectRayDirection() { return __hoGetObjectRayDir(_handle); }
		float3x4 GetObjectToWorld3x4() { return transpose(__hoGetObjectToWorld(_handle)); }
		float4x3 GetObjectToWorld4x3() { return __hoGetObjectToWorld(_handle); }
		float3x4 GetWorldToObject3x4() { return transpose(__hoGetWorldToObject(_handle)); }
		float4x3 GetWorldToObject4x3() { return __hoGetWorldToObject(_handle); }
		uint GetInstanceIndex()    { return __hoGetInstanceIndex(_handle); }
		uint GetInstanceID()       { return __hoGetInstanceID(_handle); }
		uint GetGeometryIndex()    { return __hoGetGeometryIndex(_handle); }
		uint GetPrimitiveIndex()   { return __hoGetPrimitiveIndex(_handle); }
		uint GetHitKind()          { return __hoGetHitKind(_handle); }
		uint GetShaderTableIndex() { return __hoGetShaderTableIndex(_handle); }

		void SetShaderTableIndex(uint recordIndex) { __hoSetShaderTableIndex(_handle, recordIndex); }

		template<typename Attr>
		void GetAttributes(out Attr attributes) { __hoGetAttributes(_handle, attributes); }
	};

	void MaybeReorderThread(HitObject hitObject) { __reorderWithHitObject(hitObject._handle, 0, 0); }
	void MaybeReorderThread(HitObject hitObject, uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		__reorderWithHitObject(hitObject._handle, coherenceHint, numCoherenceHintBitsFromLSB);
	}
	void MaybeReorderThread(uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		__reorderWithHint(coherenceHint, numCoherenceHintBitsFromLSB);
	}

#else

	typedef dx::HitObject HitObject;

	void MaybeReorderThread(HitObject hitObject) { dx::MaybeReorderThread(hitObject); }
	void MaybeReorderThread(HitObject hitObject, uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		dx::MaybeReorderThread(hitObject, coherenceHint, numCoherenceHintBitsFromLSB);
	}
	void MaybeReorderThread(uint coherenceHint, uint numCoherenceHintBitsFromLSB) {
		dx::MaybeReorderThread(coherenceHint, numCoherenceHintBitsFromLSB);
	}

#endif

	//A hit object's triangle vertex positions need SER *and* SM6.10 position fetch, so they're only exposed when
	//the RayTriPosition extension is enabled too (on SPIRV the op requires both capabilities/extensions). Returns
	//the hit triangle's object-space vertices; oxc::TrianglePositions comes from @extension.RayTriPosition.hlsli.
	//On DXIL oxc::HitObject is dx::HitObject, so this is a free function (a method can't be added to that typedef).
#ifdef __OXC_EXT_RAYTRIPOSITION

	#ifdef __spirv__

		//__TrianglePositionArray comes from @extension.RayTriPosition.hlsli (included first when this block is on);
		//HLSL can't return a bare float3[3] from a function, so an array typedef is required either way.
		[[vk::ext_capability(/* ShaderInvocationReorderEXT */ 5388)]]
		[[vk::ext_extension("SPV_EXT_shader_invocation_reorder")]]
		[[vk::ext_capability(/* RayTracingPositionFetchKHR */ 5336)]]
		[[vk::ext_extension("SPV_KHR_ray_tracing_position_fetch")]]
		[[vk::ext_instruction(/* OpHitObjectGetIntersectionTriangleVertexPositionsEXT */ 5307)]]
		__TrianglePositionArray __hoTriangleObjectPositions([[vk::ext_reference]] __HitObjectHandle h);

		TrianglePositions HitTrianglePositions(HitObject hitObject) {
			__TrianglePositionArray p = __hoTriangleObjectPositions(hitObject._handle);
			TrianglePositions result;
			result.p0 = p[0]; result.p1 = p[1]; result.p2 = p[2];
			return result;
		}

	#else

		TrianglePositions HitTrianglePositions(HitObject hitObject) {
			BuiltInTrianglePositions p = hitObject.TriangleObjectPositions();
			TrianglePositions result;
			result.p0 = p.p0; result.p1 = p.p1; result.p2 = p.p2;
			return result;
		}

	#endif

#endif

}
)"
