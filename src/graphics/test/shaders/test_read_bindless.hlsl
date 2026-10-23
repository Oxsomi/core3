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

//Copies words out of a buffer that the shader only reads through its bindless read handle (a HostMemory buffer in
// GraphicsDevice/hostMemory), from a byte offset, into a bindless write buffer.
//One entrypoint per file on purpose, see test_write.hlsl.

#include "@resources.hlsli"
#include "@buffer.hlsli"

//Scalars only, padded to 16 bytes, as every push block of the suite.

struct ReadPush {
	U32 input;         //Bindless read handle of the buffer read
	U32 output;        //Bindless write handle of the output buffer
	U32 offset;        //Byte offset of the first word read, a multiple of 4
	U32 padding0;
};

PUSH_CONSTANT ReadPush _push;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {
	setAtUniform(_push.output, i << 2, getAtUniform<U32>(_push.input, _push.offset + (i << 2)));
}
