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

//Umbrella for the per-extension helpers that abstract the DXIL/SPIR-V split behind one oxc:: API.
//Each header is pulled in only when its extension is enabled (OxC3 defines __OXC_EXT_<NAME> per [[oxc::extension]]).

//RayTriPosition is included before RayReorder so the hit-object triangle-position accessor can reuse its types.
#ifdef __OXC_EXT_RAYTRIPOSITION
	#include "@extension.RayTriPosition.hlsli"
#endif

#ifdef __OXC_EXT_RAYMICROMAPOPACITY
	#include "@extension.RayMicromapOpacity.hlsli"
#endif

#ifdef __OXC_EXT_RAYREORDER
	#include "@extension.RayReorder.hlsli"
#endif

#ifdef __OXC_EXT_ATOMICF32
	#include "@extension.AtomicF32.hlsli"
#endif

#ifdef __OXC_EXT_ATOMICF64
	#include "@extension.AtomicF64.hlsli"
#endif

//dx/linalg.h (DXC's cooperative-vector/matrix DXIL header) has no include guard of its own and is needed by both
//CoopVec and CoopMat on the DXIL path; include it once here so they don't double-include it (which redefines its
//functions). SPIR-V uses inline ops instead, and the header #errors on __spirv__, so it's DXIL-only.
#ifndef __spirv__
	#if defined(__OXC_EXT_COOPVEC) || defined(__OXC_EXT_COOPMAT)
		#include <dx/linalg.h>
	#endif
#endif

#ifdef __OXC_EXT_COOPVEC
	#include "@extension.CoopVec.hlsli"
#endif

#ifdef __OXC_EXT_COOPMAT
	#include "@extension.CoopMat.hlsli"
#endif
