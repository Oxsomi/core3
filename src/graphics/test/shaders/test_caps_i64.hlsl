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

//Capability execution shader: 64 bit integer arithmetic.
//
//Separate from test_caps_atomics.hlsl, which needs AtomicI64 as well and therefore skips on any device that
// has plain I64 without the atomic. This one runs wherever I64 does.

#include "@appdata.hlsli"
#include "@buffer.hlsli"

//0xFFFFFFFF * v is (v << 32) - v, so the high word is v - 1 and the low word is -v.
//For every v above 1 the product needs more than 32 bits, which means a multiply that quietly happened at 32
// bit width keeps only the low word and cannot produce the high one.
//Checking both words per iteration is what makes that visible; checking the low word alone would pass on a
// truncating multiply, since the low word is the same either way.
//v is seeded with SV_DispatchThreadID so the whole loop can't be folded to a constant at compile time and
// pass without a single 64 bit instruction being emitted.
//App data: [0] = bindless write handle of the output buffer.

[[oxc::extension("I64")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	U32 total = 0;

	for (U32 k = 1; k <= 64; ++k) {

		const U32 v = 1 + ((k + i) & 63);

		const U64 wide = (U64)0xFFFFFFFFu * (U64)v;
		const U32 hi = (U32)(wide >> 32);
		const U32 lo = (U32)wide;

		total += (hi == v - 1 && lo == (U32)(0u - v)) ? k : 0;
	}

	setAtUniform<U32>(getAppData1u(0), 0, total);
}
