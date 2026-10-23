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

//Capability execution shader: packed 4 x 8 bit integer dot products with a 32 bit accumulate.
//
//Each thread dots two packed vectors of four signed bytes and adds its own index, and writes the result at its slot.
//The operands are hashes of the thread index, so their bytes differ per thread and per lane, signs included, and nothing
// folds to a constant.
//The test computes the same sums on the host.
//dot4add_i8packed is DXIL's Dot4AddI8Packed (SM6.4) and an OpSDot over a packed vector on SPIR-V, which needs
// VK_KHR_shader_integer_dot_product on Vulkan.

#include "@resources.hlsli"

struct DotPush {
	U32 output;          //Bindless write handle of the output buffer
	U32 padding0, padding1, padding2;
};

PUSH_CONSTANT DotPush _push;

[[oxc::extension("IntegerDot4x8")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32x3 id : SV_DispatchThreadID) {

	const U32 a = id.x * 0x9E3779B1u + 0x01234567u;
	const U32 b = id.x * 0x85EBCA6Bu + 0x76543210u;

	rwBuffer(_push.output).Store(id.x * 4, asuint(dot4add_i8packed(a, b, (int) id.x)));
}
