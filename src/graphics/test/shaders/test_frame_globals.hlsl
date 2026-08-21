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
#include "@buffer.hlsli"

//The per frame globals had no execution coverage at all: nothing ever read the frame id, the time or the
// delta the runtime writes alongside the swapchain ids.
//That is the half of the globals block a layout change can silently break, so one thread copies the raw
// fields out for the CPU to check against what it submitted.
//The output handle rides in a push constant, which is what replaced the app data block, so this also covers
// a custom pipeline layout receiving the runtime globals at all.
//A cbuffer rounds to 16 bytes on DXIL, so the block is spelled at 16: a 4 byte struct would be declared as 4
// on SPIRV and 16 on DXIL, and the work op requires the written size to match exactly.

struct FrameGlobalsPush {
	U32 output;        //Bindless write handle of the output buffer
	U32 padding0, padding1, padding2;
};

PUSH_CONSTANT FrameGlobalsPush _push;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	const U32 output = _push.output;

	setAtUniform<U32>(output, 0, _frameId);
	setAtUniform<U32>(output, 4, asuint(_time));
	setAtUniform<U32>(output, 8, asuint(_deltaTime));
	setAtUniform<U32>(output, 12, _swapchainCount);
}
