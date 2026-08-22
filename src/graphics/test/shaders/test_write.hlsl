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

//One entrypoint per file on purpose: multi entry SPIR-V modules under 1.4 make the validation layers
// unable to attribute globals to an entrypoint, which the suite would count as a real warning.

#include "@resources.hlsli"
#include "@buffer.hlsli"

//Write a value derived from the thread id, so the test can prove every thread really ran.
//
//The handle and the base ride in push constants.
//Spelled as scalars rather than an array: on DXIL every array element takes a 16 byte cbuffer row, so the
//size the work op checks would not match what the shader declares.

struct WritePush {
	U32 output;        //Bindless write handle of the output buffer
	U32 base;          //Base value each thread adds its id to
	U32 args;          //Bindless write handle of the indirect argument buffer (test_write_args)
	U32 padding0;
};

PUSH_CONSTANT WritePush _push;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {
	setAtUniform(_push.output, i << 2, _push.base + i);
}
