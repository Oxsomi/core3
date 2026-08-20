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


//A descriptor ARRAY in a non bindless layout: one binding, four elements, each element addressed.
//The loop is unrolled so the indices stay compile time constant and no dynamic indexing caps are implied.
//Deliberately includes NO bindless headers.

ByteAddressBuffer inputs[4] : register(t0, space0);
RWByteAddressBuffer output : register(u4, space0);

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {

	uint sum = 0;

	[unroll]
	for(uint j = 0; j < 4; ++j)
		sum += inputs[j].Load(id.x * 4);

	output.Store(id.x * 4, sum);
}
