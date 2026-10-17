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
#include "@pack.hlsli"

//A texture as a push descriptor, which is the case a root descriptor cannot express.
//A root descriptor is a raw GPU address with nowhere to carry a format, a mip or a swizzle, so D3D12 gives
// the texture a single entry descriptor table and fills it from the bound heap's push ring, while Vulkan
// pushes an image descriptor straight into the set.
//The buffer beside it stays an ordinary root descriptor, so one shader covers both push forms at once.
//F32x4 against an RGBA8 texture for the same reason the copy shader picks its texel type: a view's format is
// the image's own and the sampled type has to match it numerically.
//Deliberately includes NO bindless headers.

Texture2D<F32x4> _input;

RWByteAddressBuffer output;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x3 id : SV_DispatchThreadID) {
	F32x4 texel = _input[id.xy];
	output.Store((id.y * 8 + id.x) * 4, packUnorm4x8(texel));
}
