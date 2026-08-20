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
#include "@extensions.hlsli"

//The 64 bit twin of test_bindful_atomic_f32: same reasoning, a typed double lvalue only a bindful layout
// can supply, since the bindless resource set has nothing but RWByteAddressBuffer.
//AtomicF64 has no DXIL intrinsic either (ESHExtension_NoDxilCompile), so this is SPIRV only.
//F64 is declared alongside it because a double typed buffer needs the Float64 capability of its own.
//Deliberately includes NO bindless headers.

RWStructuredBuffer<F64> buf : register(u0, space0);

[[oxc::extension("AtomicF64", "F64")]]
[[oxc::model("6.5")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint id : SV_DispatchThreadID) {

	//Every thread adds 1 to slot 0, so a lost update leaves the total short of 64

	oxc::AtomicAddF64(buf[0], oxc::MemoryScope_Device, oxc::MemorySemantics_Relaxed, (F64) 1.0);
}
