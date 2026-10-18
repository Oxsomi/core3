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

//[WaveSize] in its ranged form (SM6.8): any size from 4 to 128 lanes, preferring 32.
//Every device runs one of them, D3D12 only with SM6.8.

#include "@resources.hlsli"
#include "@buffer.hlsli"

struct WavePush {
	U32 output;        //Bindless write handle of the output buffer
	U32 padding0, padding1, padding2;
};

PUSH_CONSTANT WavePush _push;

//As test_wave_size.hlsl: word 0 holds every lane count the dispatch ran at.

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.8")]]
[shader("compute")]
[numthreads(64, 1, 1)]
[WaveSize(4, 128, 32)]
void main() {
	if(WaveIsFirstLane())
		rwBufferUniform(_push.output).InterlockedOr(0, WaveGetLaneCount());
}
