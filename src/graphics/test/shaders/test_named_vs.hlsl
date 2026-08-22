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

//The entrypoint is deliberately NOT named "main".
//The compiler renames a non lib module's sole entrypoint to "main" in the SPIR-V while reflection keeps
// the original name, so a graphics pipeline built from this file has to look up "main", not "mainVS".
//A fullscreen triangle from the vertex id, so no vertex buffers are needed.

[shader("vertex")]
F32x4 mainVS(U32 id : SV_VertexID) : SV_POSITION {
	U32 corner = id % 3;
	return F32x4(corner == 1 ? 3 : -1, corner == 2 ? 3 : -1, 0, 1);
}
