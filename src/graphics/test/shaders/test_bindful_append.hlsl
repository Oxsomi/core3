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

//An append buffer carries a counter resource alongside the data, which is a descriptor shape of its own:
// the table validates that a counter is only ever attached to an Append/Consume register.
//Only threads passing a filter append, so the counter lands on a value the CPU can predict exactly.
//Vulkan has no support for these counters yet, so the test only executes this on D3D12 and pins the
// documented refusal on Vulkan instead.
//Deliberately includes NO bindless headers.

AppendStructuredBuffer<U32> appended : register(u0, space0);

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {

	//Exactly 16 of the 64 threads append, so the counter has to end on 16

	if(!(id.x & 3))
		appended.Append(id.x);
}
