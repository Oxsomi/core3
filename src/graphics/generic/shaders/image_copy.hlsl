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

//A view's format has to be the image's own, and Vulkan requires the view's numeric type to match the sampled
//type the shader declares.
//So the texel type is a DEFINE rather than a uniform (a uniform is a spec constant
//and cannot type a declaration) and the copy is compiled once per numeric class: TEXEL is U32x4 for integer
//formats and F32x4 for the UNORM/SNORM/float ones, which is every color format a rotation is used on.
//Both are bit exact for their class: a UNORM texel round trips through fp32 losslessly at these widths, and
//the integer path never converts at all.

//The precompile pass that discovers the annotations below runs before any define is set, so the texel type
//needs a value to parse with; the permutations override it.

#ifndef $TEXEL
	#define $TEXEL U32x4
#endif

Texture2DArray<$TEXEL> _input;
UNKNOWN_FORMAT RWTexture2DArray<$TEXEL> _output;

//Simplest variant, only 1 dispatch, allows us to use root constants and works everywhere.
//Only turn on rotate if sizRot.w != 0

[[oxc::uniforms(B1 ROTATE = false)]]
[[oxc::uniforms(B1 ROTATE = true)]]
[[oxc::defines("TEXEL" = "U32x4")]]
[[oxc::defines("TEXEL" = "F32x4")]]
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

/* TODO:
//Hardest variant; used when lots of regions are copied at once (to avoid dispatching small batches of mainSingle).
//This is used to determine which region this warp is working on right now.
//This and group size is why the limit of regions is 128 (efficient to avoid groupshared on mobile).

#if $THREAD_COUNT != 128 && $THREAD_COUNT != 1
	groupshared U32x3 _regionPixelCount[128 / $THREAD_COUNT];
	groupshared U32x3 _regionPixelOffset[128 / $THREAD_COUNT];
#endif

#if $THREAD_COUNT == 1
	groupshared U32 _reduction0[128];
	groupshared U32 _reduction1[64];
	groupshared U32 _reduction2[32];
	groupshared U32 _reduction3[16];
	groupshared U32 _reduction4[8];
	groupshared U32 _reduction5[4];
	groupshared U32 _reduction6[2];
#endif

[[oxc::stage("compute")]]
[[oxc::extension("SubgroupOperations")]]
[[oxc::defines("THREAD_COUNT" = "4")]]		//Warp
[[oxc::defines("THREAD_COUNT" = "8")]]		//Samsung (sometimes)
[[oxc::defines("THREAD_COUNT" = "16")]]	//Intel (sometimes)
[[oxc::defines("THREAD_COUNT" = "32")]]	//NV/AMD/Intel
[[oxc::defines("THREAD_COUNT" = "64")]]	//AMD & Samsung
[[oxc::defines("THREAD_COUNT" = "128")]]	//QCOM, ARM
[numthreads(16, 8, 1)]
void mainMultiple(U32x3 id : SV_DispatchThreadID, U32 threadId : SV_GroupIndex) {

	//Calculate how many dispatches each copy needs

	U32x3 regionPixelCountI;
	U32x3 regionPixelOffsetI;
	
	U32 copyFootprint = 0;

	if(threadId < _regionCount) {

		U32x4 src = _regions[threadId].getSrc();
		U32x4 dst = _regions[threadId].getDst();
		U32x4 sizRot = _regions[threadId].getSizRot();

		U32x2 aligned = (sizRot.xy + U32x2(15, 7)) >> U32x2(4, 3);

		copyFootprint = aligned.x * aligned.y * sizRot.z;
	}

	//Grab the global footprint to see where each copy starts

	#if $THREAD_COUNT != 128
		GroupMemoryBarrierWithGroupSync();
	#endif
}*/
