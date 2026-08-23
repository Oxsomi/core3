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

//A pixel shader writing more than one render target.
//
//It is drawn with AND packaged, and both halves matter. Packaging compiles a shader for both backends and
// merges the results, and merging is where the two used to disagree.
//The SPIR-V path used to discard the index of a default semantic, recording SV_Target1 as index 0 while DXIL
// recorded 1, so outputSemanticNames differed and SHFile_combine refused the merge. Any shader using
// TEXCOORD1 or higher had the same fate.
//Nothing in the suite had more than one render target or a non zero default semantic index, which is why it
// stayed invisible.
//
//The entrypoint is deliberately not called main: SHFile_combine matches entries by name, so sharing one with
// the single target pixel shader in this directory would collide on target count instead.

//The two targets get DIFFERENT constants, so a backend that wrote one output to both attachments, or swapped
// them, fails rather than landing on a plausible picture.
//Values are exact in 8 bit UNORM, matching the rest of this module.

struct MRTOutput {
	float4 a : SV_Target0;
	float4 b : SV_Target1;
};

[[oxc::model("6.6")]]
[shader("pixel")]
MRTOutput mainMrt(float4 pos : SV_POSITION) {

	MRTOutput o;
	o.a = float4(1, 102.0 / 255, 51.0 / 255, 1);        //0xFF3366FF once packed
	o.b = float4(0, 204.0 / 255, 0, 1);                 //0xFF00CC00
	return o;
}
