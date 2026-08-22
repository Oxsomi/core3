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

//Capability execution shader: derivatives in a compute shader.
//
//ddx/ddy are ordinarily pixel shader only, since they need the 2x2 quad the rasterizer supplies. This bit
// means the compute stage forms quads out of the thread group instead, which is what makes the call legal
// here at all.
//DXC emits the Quads derivative group for an even 2D thread group and the Linear one otherwise; both map to
// the same OxC3 extension, so a 2x2 group keeps this on the quad path.

#include "@resources.hlsli"
#include "@buffer.hlsli"

//Per dispatch data this shader reads, declared as a push constant.
//Scalars rather than an array: on DXIL each array element takes its own 16 byte cbuffer row, so the size the
//work op checks would not match what the shader declares.

struct CapsPush {
	U32 output;        //Bindless write handle of the output buffer
	U32 aux;           //Second handle, where the test needs one (a TLAS for the ray query cases)
	U32 padding0, padding1;
};

PUSH_CONSTANT CapsPush _push;

//The value varies by exactly 1 across x and exactly 10 across y, so within the quad the derivatives are
// those two constants, and every value involved is a small integer that F32 holds exactly.
//Each of the four threads checks both derivatives itself and contributes one, so the total is 4 only if the
// quad was formed and both directions were differenced correctly.
//A device that returned zero derivatives, or formed no quad, contributes nothing and lands on 0.
//Differencing only one direction lands on 0 as well, since each thread requires both to be right.
//App data: [0] = bindless write handle of the output buffer.

[[oxc::extension("ComputeDeriv")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(2, 2, 1)]
void main(U32x3 id : SV_DispatchThreadID) {

	const F32 v = (F32)id.x + (F32)id.y * 10.0f;

	const F32 dx = ddx(v);
	const F32 dy = ddy(v);

	rwBufferUniform(_push.output).InterlockedAdd(0, dx == 1.0f && dy == 10.0f ? 1 : 0);
}
