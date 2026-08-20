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


//A storage image (RW texture) in a bindful table; the only executor of the UAV texture descriptor path.
//k / 255 is exactly representable in 8 bit UNORM, so the readback can byte compare.
//unorm float4 matches DXIL reflection to the format; SPIRV needs the explicit vk::image_format, since
// DXC maps the plain template to Rgba32f, which would mismatch the RGBA8 view.
//Deliberately includes NO bindless headers.

#include "@types.hlsli"

UNKNOWN_FORMAT RWTexture2D<unorm float4> outTex : register(u0, space0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
	uint k = (id.y * 8 + id.x) * 3;
	outTex[id.xy] = float4(k / 255.0, 0, 0, 1);
}
