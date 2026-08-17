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

//Pixel shader emitting TWO colors from one draw, which is what dual source blending consumes.
//Both go to the same attachment rather than to two attachments, which is what makes this dual SOURCE rather
// than MRT: on SPIR-V both outputs sit at location 0 and the DUAL_SRC macros tell them apart with the Index
// decoration, while DXIL reads the plain SV_Target0/1 as src0/src1 whenever an Src1 blend factor is used.
//
//The entrypoint is deliberately not called main: SHFile_combine matches entries by name, so sharing one with
// the single target pixel shader in this directory would collide on target count instead.

struct DualSrcOutput {
	DUAL_SRC_TARGET0 F32x4 src0 : SV_Target0;
	DUAL_SRC_TARGET1 F32x4 src1 : SV_Target1;
};

//src0 is the app data color, src1 is a fixed half in every channel.
//The blend is configured src * Src1Color, so the result is the app data color scaled by exactly a half,
// which no single source blend factor could produce from these inputs alone.
//App data: [4..7] = color as F32x4.

[shader("pixel")]
DualSrcOutput mainDualSrc(F32x4 pos : SV_POSITION) {

	DualSrcOutput o;
	o.src0 = getAppData4f(4);
	o.src1 = F32x4(0.5f, 0.5f, 0.5f, 0.5f);
	return o;
}
