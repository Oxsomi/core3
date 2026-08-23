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

#include "@resources.hlsli"

//Per dispatch data this shader reads, declared as a push constant.
//The colour occupied U32 slots 4..7 and the two logic op sources 8..11 and 12..15, which is why this block
//keeps all three: one setPushConstants feeds whichever pixel shader is bound.

struct PixelPush {
	U32 padding0, padding1, padding2, padding3;
	F32 colorR, colorG, colorB, colorA;
	U32 logicSrc0R, logicSrc0G, logicSrc0B, logicSrc0A;
	U32 logicSrc1R, logicSrc1G, logicSrc1B, logicSrc1A;
};

PUSH_CONSTANT PixelPush _push;

//The pixel color comes from the push constant, so the readback can prove this exact submit produced it.
//App data: [4..7] = color as F32x4.

[shader("pixel")]
F32x4 main(F32x4 pos : SV_POSITION) : SV_TARGET {
	return F32x4(_push.colorR, _push.colorG, _push.colorB, _push.colorA);
}
