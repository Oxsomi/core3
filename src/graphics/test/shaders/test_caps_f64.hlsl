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

//Capability execution shader: 64 bit floating point.
//
//2^24 is the first integer F32 can no longer follow one at a time: above it consecutive F32 values sit two
// apart, so 2^24 + k is exact in F32 only when k is even.
//F64 represents every one of them exactly, 2^24 + 64 being far below the 2^53 where F64 in turn starts
// skipping integers.
//That gap is the whole test: round tripping 2^24 + k through a double returns k for all 64 values of k only
// if the arithmetic really ran at 64 bits.

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

//The addend is seeded with SV_DispatchThreadID so neither DXC nor the driver can fold the loop into a
// constant computed at host precision.
//The double is undone with an integer subtract rather than by comparing doubles, because a double compare
// needs DXIL's double-extensions flag and the entrypoint would then carry a capability it never declared.
//That conversion is why I64 is declared alongside F64: DXC's SPIR-V backend lowers the F64 to U32 convert
// through a 64 bit integer, so the module really does use Int64 and processSPIRV requires it to say so.
//A device computing this at F32 lands on 1056, the sum of the even k, rather than on a near miss.
//App data: [0] = bindless write handle of the output buffer.

[[oxc::extension("F64", "I64")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	const U32 base = 1u << 24;

	U32 total = 0;

	for (U32 k = 1; k <= 64; ++k) {

		const U32 want = k + i;
		const F64 sum = (F64) base + (F64) want;
		const U32 back = (U32) sum - base;

		total += back == want ? k : 0;
	}

	setAtUniform<U32>(_push.output, 0, total);
}
