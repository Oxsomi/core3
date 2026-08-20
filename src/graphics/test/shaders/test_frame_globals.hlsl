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
#include "@appdata.hlsli"
#include "@buffer.hlsli"

//The per frame globals other than app data had no execution coverage at all: every bindless test reads
// getAppData, but nothing ever read the frame id, the time or the delta the runtime writes alongside it.
//That is the half of the globals block a layout change can silently break, so one thread copies the raw
// fields out for the CPU to check against what it submitted.
//App data: [0] = bindless write handle of the output buffer.

[shader("compute")]
[numthreads(1, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	const U32 output = getAppData1u(0);

	setAtUniform<U32>(output, 0, _frameId);
	setAtUniform<U32>(output, 4, asuint(_time));
	setAtUniform<U32>(output, 8, asuint(_deltaTime));
	setAtUniform<U32>(output, 12, _swapchainCount);
}
