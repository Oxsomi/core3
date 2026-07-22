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

struct CopyImageRegion {

	U32x2 src;
	U32x2 dst;
	U32x2 sizRot;

	U32x4 getSrc() { return (src.xxyy >> U32x4(0, 16, 0, 16)) & 0xFFFF; }
	U32x4 getDst() { return (dst.xxyy >> U32x4(0, 16, 0, 16)) & 0xFFFF; }
	U32x4 getSizRot() { return (sizRot.xxyy >> U32x4(0, 16, 0, 16)) & 0xFFFF; }
};

//Command signature
//Note: mip dst and src are present, but only because they exist on the CPU.
//		these aren't really needed, since _input and _output are already set so only that mip is active.
//		In the case of mainMultiple they can be brought together to form the image dispatch offset instead.
//		This will allow each dispatch to quickly identify which group is responsible for which region.

struct CopyImageCommand {

	U32 regionCount;
	U32 pad3;

	U32x2 src;
	U32x2 dst;
	U32x2 sizRot;
};

PUSH_CONSTANT CopyImageCommand cmd;

Texture2DArray<U32x4> _input;
UNKNOWN_FORMAT RWTexture2DArray<U32x4> _output;

//Simplest variant, only 1 dispatch, allows us to use root constants and works everywhere.
//Only turn on rotate if sizRot.w != 0

[[oxc::uniforms(B1 ROTATE = false)]]
[[oxc::uniforms(B1 ROTATE = true)]]
[shader("compute")]
[numthreads(16, 8, 1)]
void mainSingle(U32x3 id : SV_DispatchThreadID) {

	CopyImageRegion region;
	region.src = cmd.src;
	region.dst = cmd.dst;
	region.sizRot = cmd.sizRot;

	U32x4 xyzRot = region.getSizRot();

	if(any(id >= xyzRot.xyz))
		return;

	U32x3 src = region.getSrc().xyz + id;
	U32x3 dst = region.getDst().xyz + id;

	#ifdef $$ROTATE
		if($$ROTATE) {

			if(xyzRot.w & 1) {
				xyzRot.xy = xyzRot.yx;
				dst.xy = dst.yx;
			}

			if(xyzRot.w < 3)
				dst.y = xyzRot.y - 1 - dst.y;

			if(xyzRot.w > 1)
				dst.x = xyzRot.x - 1 - dst.x;
		}
	#endif

	_output[dst] = _input[src];
}
