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

//RayTriPosition (SM6.10 ray triangle vertex position fetch) under one oxc API.
//DXIL has the native RayQuery::CommittedTriangleObjectPositions(); DXC's SPIR-V backend has no such intrinsic, so
//there it is a single inline SPIR-V instruction on the (native, valid) HLSL ray query.

namespace oxc {

	struct TrianglePositions {
		float3 p0;
		float3 p1;
		float3 p2;
	};

//The SPIR-V op takes a concrete RayQuery type; DXC's inline SPIR-V doesn't instantiate a RayQuery<flags> template,
//so this is fixed to RayQuery<RAY_FLAG_NONE> (position fetch is independent of the ray flags). A shader that needs
//different flags can add an overload here.

#ifdef __spirv__

	typedef float3 __TrianglePositionArray[3];

	[[vk::ext_capability(/* RayQueryPositionFetchKHR */ 5391)]]
	[[vk::ext_extension("SPV_KHR_ray_tracing_position_fetch")]]
	[[vk::ext_instruction(/* OpRayQueryGetIntersectionTriangleVertexPositionsKHR */ 5340)]]
	__TrianglePositionArray __rayQueryTrianglePositions([[vk::ext_reference]] RayQuery<RAY_FLAG_NONE> q, int intersection);

	TrianglePositions CommittedTriangleObjectPositions(inout RayQuery<RAY_FLAG_NONE> q) {
		__TrianglePositionArray p = __rayQueryTrianglePositions(q, /* committed intersection */ 1);
		TrianglePositions result;
		result.p0 = p[0]; result.p1 = p[1]; result.p2 = p[2];
		return result;
	}

#else

	TrianglePositions CommittedTriangleObjectPositions(inout RayQuery<RAY_FLAG_NONE> q) {
		BuiltInTrianglePositions p = q.CommittedTriangleObjectPositions();
		TrianglePositions result;
		result.p0 = p.p0; result.p1 = p.p1; result.p2 = p.p2;
		return result;
	}

#endif

	//Ray-pipeline (closesthit / anyhit) position fetch: the hit triangle's object-space vertices come from the
	//HitTriangleVertexPositionsKHR builtin input on SPIRV, or the global TriangleObjectPositions() intrinsic on
	//DXIL (unqualified - oxc deliberately has no same-named member so this resolves to the builtin). RT stage only.
#ifdef __OXC_EXT_RAYTRACING

	#ifdef __spirv__

		[[vk::ext_capability(/* RayTracingPositionFetchKHR */ 5336)]]
		[[vk::ext_extension("SPV_KHR_ray_tracing_position_fetch")]]
		[[vk::ext_builtin_input(/* HitTriangleVertexPositionsKHR */ 5335)]]
		static const float3 __hitTriangleVertexPositions[3];

		TrianglePositions HitTrianglePositions() {
			TrianglePositions result;
			result.p0 = __hitTriangleVertexPositions[0];
			result.p1 = __hitTriangleVertexPositions[1];
			result.p2 = __hitTriangleVertexPositions[2];
			return result;
		}

	#else

		TrianglePositions HitTrianglePositions() {
			BuiltInTrianglePositions p = TriangleObjectPositions();
			TrianglePositions result;
			result.p0 = p.p0; result.p1 = p.p1; result.p2 = p.p2;
			return result;
		}

	#endif

#endif

}
)"
