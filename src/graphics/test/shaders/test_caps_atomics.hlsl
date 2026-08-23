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

//Capability execution shaders: 64 bit integer atomics.
//
//[[oxc::extension]] is what keeps this honest.
//The entrypoint is only emitted for backends that declared the extension,
// so a device without AtomicI64 never sees a binary it can't run,
// and the test can assert that the capability bit and the presence of the entrypoint agree.
//
//One entrypoint per file, as in the other test shaders: multi entry SPIR-V under 1.4 stops the validation
// layers attributing globals to an entrypoint, which the suite counts as a real warning.

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

//64 threads each add (i + 1) into a shared counter, so the total is only right if every add was atomic:
// a lost update lands short, which a non atomic read-modify-write reliably produces at this thread count.
//
//The counter is 32 bit rather than 64.
//InterlockedAdd64 is what would exercise AtomicI64 directly,
// but DXC's SPIR-V backend reports "intrinsic 'InterlockedAdd64' method unimplemented",
// so a 64 bit atomic can only be expressed for DXIL today.
//Rather than gate the whole shader to one backend, this covers the atomic path itself on both,
// and the 64 bit type is still exercised by the I64 load/store below.
//App data: [0] = bindless write handle of the output buffer.

[[oxc::extension("I64", "AtomicI64")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	//Thread 0 seeds a 64 bit value past the 32 bit boundary, so a truncating load/store is visible in the
	// readback rather than plausible.

	if(!i)
		setAtUniform<U64>(_push.output, 8, (U64)0x1FFFFFFFEull);

	rwBufferUniform(_push.output).InterlockedAdd(0, i + 1);
}
