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

//Structured buffers address by ELEMENT, not by byte, so the descriptor carries a stride the table validates
// the range against. Nothing else in the suite binds one: every other bindful buffer is byte addressed.
//Each component is transformed differently, so a wrong stride or a swapped field shows up per component
// rather than as a single blurred mismatch.
//The element is a built in vector rather than a custom struct on purpose: a struct element makes the two
// backends describe the layout differently and SBFile_combine then refuses to build a dual backend oiSH.
//Deliberately includes NO bindless headers.

StructuredBuffer<U32x4> input : register(t0, space0);
RWStructuredBuffer<U32x4> output : register(u1, space0);

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {

	const U32x4 e = input[id.x];
	output[id.x] = U32x4(e.x * 2, e.y + 100, e.z ^ 0xFFu, e.w + id.x);
}
