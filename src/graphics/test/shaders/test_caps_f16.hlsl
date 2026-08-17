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

//Capability execution shader: 16 bit floating point.
//
//Every half value stays in registers.
//The bindless layout's RWByteAddressBuffer is only ever written as U32, because the device enables
// shaderFloat16 but no 16 bit STORAGE feature, so a half crossing the buffer interface would declare
// StorageBuffer16BitAccess and the validation layers would reject the module at create time.

#include "@appdata.hlsli"
#include "@buffer.hlsli"

//The three predicates below bracket half's significand from both sides, which is as much as a portable
// shader can assert about it.
//Every value they touch is exactly representable in binary16, so none of them depends on a rounding
// decision. That matters because Vulkan leaves shaderRoundingModeRTEFloat16 optional, so an assertion
// needing a particular rounding mode would not be portable.
//What none of this can do is tell a native half multiply apart from one the backend widened to F32 and
// narrowed back. That is not a shortcoming of the shader: Vulkan permits evaluating at a higher precision
// than the declared type, so both are conforming and no shader can separate them portably.
//What it does prove is that a module carrying native 16 bit ops executes and keeps a 10 bit significand.
//App data: [0] = bindless write handle of the output buffer.

[[oxc::extension("16BitTypes")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	U32 total = 0;
	U32 narrowedCount = 0;
	U32 exactCount = 0;
	U32 tenBitsCount = 0;

	for (U32 k = 1; k <= 64; ++k) {

		//Seeded with SV_DispatchThreadID so nothing can be folded at host precision.
		//The mask keeps v in 1..64 whatever the thread id is, so the predicates stay true if the dispatch is
		// ever widened rather than only for the single thread this runs today.

		const U32 v = 1 + ((k + i) & 63);

		//Magnitude: 32 * (1..64) tops out at 2048 and scaling by a power of two is exact, so this is the half
		// ALU's own product with no rounding involved.

		const F16 scaled = (F16)v * (F16)32.0;
		const Bool exact = (U32)scaled == v * 32;

		//Lower bound: 1 + v * 2^-10 lands exactly on a representable half for every v in 1..64, the subtract
		// is exact, and scaling back recovers v only if the significand really carries 10 fractional bits.
		//A backend quietly substituting a same width but wider exponent format such as bfloat16 loses v here.

		const F16 stepped = (F16)1.0 + (F16)v * (F16)(1.0 / 1024.0);
		const Bool tenBits = (U32)((stepped - (F16)1.0) * (F16)1024.0) == v;

		total += exact && tenBits ? k : 0;

		//Each predicate is counted on its own as well, so a failure says which of them went wrong instead of
		// only that the total was short.

		exactCount += exact ? 1 : 0;
		tenBitsCount += tenBits ? 1 : 0;

		//Narrowing is reported rather than asserted.
		//1 + v * 2^-18 cannot survive a real narrowing, since the neighbouring halves are 1.0 and 1 + 2^-10 and
		// every rounding mode picks one of those, neither being the F32 it started from.
		//It is not asserted because Vulkan explicitly allows an implementation to evaluate at a higher
		// precision than the declared type, so keeping the value in F32 is conforming rather than broken.
		//Measured, not assumed: on one RTX 3080 this is 64 of 64 through D3D12 and 0 of 64 through Vulkan, so
		// it is the backend that differs and not the hardware.
		//Written out anyway so that difference stays visible rather than being quietly assumed either way.

		const F32 wide = 1.0f + (F32)v / 262144.0f;
		narrowedCount += (F32)((F16)wide) != wide ? 1 : 0;
	}

	setAtUniform<U32x4>(getAppData1u(0), 0, U32x4(total, exactCount, tenBitsCount, narrowedCount));
}
