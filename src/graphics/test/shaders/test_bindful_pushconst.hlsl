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

//Push constants: a root constant range rather than a descriptor, so the values travel with the command
// rather than through the heap. 16 bytes, which is a legal size on both backends (4 to 128, multiple of 4).
//The buffer is left to auto assign its register: an explicit one changes where DXC puts the implicit
//$Globals cbuffer that a push constant becomes on DXIL, which stops reflection pairing the two backends.
//Deliberately includes NO bindless headers.

struct PushData {
	U32 scale;
	U32 bias;
	U32 xorMask;
	U32 offset;
};

PUSH_CONSTANT PushData constants;

RWByteAddressBuffer output;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	//The offset comes from the constants too, so two dispatches in one scope write disjoint ranges and
	// don't race each other for the same slots

	output.Store((id.x + constants.offset) * 4, (id.x * constants.scale + constants.bias) ^ constants.xorMask);
}
