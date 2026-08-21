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

//Push descriptors: root descriptors rather than table entries, so both resources ride in the command stream
// and this shader needs no descriptor heap and no descriptor table at all.
//Buffer class only, which is all a root descriptor can hold: a raw GPU address has nowhere to carry a
// texture's format, mip or swizzle.
//Registers are left to auto assign for the same reason the push constant shader leaves its buffer alone.
//Deliberately includes NO bindless headers.

struct Params {
	U32 scale;
	U32 bias;
	U32 xorMask;
	U32 offset;
};

ConstantBuffer<Params> params;

RWByteAddressBuffer output;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	//The offset comes from the constants too, so two dispatches in one scope write disjoint ranges and
	// don't race each other for the same slots

	output.Store((id.x + params.offset) * 4, (id.x * params.scale + params.bias) ^ params.xorMask);
}
