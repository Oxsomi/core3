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

//One register read by BOTH stages, which is the only way to produce a binding whose visibility is a union
// of two shader stages (SHADER_VISIBILITY_ALL on D3D12, multi bit stageFlags on Vulkan).
//Both entrypoints live in one file on purpose: layout detection refuses to merge two files into one info,
// so a shared binding can only be described by a single binary carrying both stages.
//Deliberately includes NO bindless headers.

ByteAddressBuffer sharedParams : register(t0, space0);

[shader("vertex")]
F32x4 mainVertex(U32 id : SV_VertexID) : SV_POSITION {

	//The vertex stage reads the register too, so the layout has to make it visible here as well.
	//Slot 0 holds 1: multiplying the fullscreen triangle by it keeps the triangle exactly as is, so a
	//failed read (0) collapses it and no pixel survives.

	const F32 scale = (F32) sharedParams.Load(0);
	U32 corner = id % 3;
	return F32x4((corner == 1 ? 3 : -1) * scale, (corner == 2 ? 3 : -1) * scale, 0, 1);
}

[shader("pixel")]
F32x4 mainPixel(F32x4 pos : SV_POSITION) : SV_TARGET {
	return F32x4_fromU32x4(sharedParams.Load4(16));
}
