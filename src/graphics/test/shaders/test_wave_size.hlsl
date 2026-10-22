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

//[WaveSize] in its first form (SM6.6), one entry per size: the device either runs the entry at that size or refuses it.

#include "@resources.hlsli"
#include "@buffer.hlsli"

struct WavePush {
	U32 output;        //Bindless write handle of the output buffer
	U32 padding0, padding1, padding2;
};

PUSH_CONSTANT WavePush _push;

//The first lane of every wave ORs its lane count into word 0, so the word holds every size the dispatch ran at.

void storeLanes() {
	if(WaveIsFirstLane())
		rwBufferUniform(_push.output).InterlockedOr(0, WaveGetLaneCount());
}

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(64, 1, 1)]
[WaveSize(16)]
void mainW16() {
	storeLanes();
}

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(64, 1, 1)]
[WaveSize(32)]
void mainW32() {
	storeLanes();
}

[[oxc::extension("SubgroupOperations")]]
[[oxc::model("6.6")]]
[shader("compute")]
[numthreads(64, 1, 1)]
[WaveSize(64)]
void mainW64() {
	storeLanes();
}
