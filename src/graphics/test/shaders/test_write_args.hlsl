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
#include "@buffer.hlsli"
#include "@indirect.hlsli"

//Write indirect arguments on the GPU, so the next scope can consume them without CPU involvement.
//Layout: IndirectDraw at 0 (3 vertices, 2 instances), IndirectDispatch at 16 (2 groups).
//
//Shares its push constant block with test_write.hlsl, so one setPushConstants feeds whichever of the two is
//bound; this one only reads the argument handle.

struct WritePush {
	U32 output;
	U32 base;
	U32 args;          //Bindless write handle of the argument buffer
	U32 padding0;
};

PUSH_CONSTANT WritePush _push;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(U32 i : SV_DispatchThreadID) {

	IndirectDraw draw;
	draw.vertexCount = 3;
	draw.instanceCount = 2;
	draw.vertexOffset = 0;
	draw.instanceOffset = 0;

	IndirectDispatch dispatch;
	dispatch.x = 2;
	dispatch.y = 1;
	dispatch.z = 1;
	dispatch.pad = 0;

	U32 resourceId = _push.args;
	setAtUniform(resourceId, 0, draw);
	setAtUniform(resourceId, 16, dispatch);
}
