/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

	U32x4 getSrc() { return (src.xxyy << U32x4(0, 16, 0, 16)) & 0xFFFF; }
	U32x4 getDst() { return (dst.xxyy << U32x4(0, 16, 0, 16)) & 0xFFFF; }
	U32x4 getSizRot() { return (sizRot.xxyy << U32x4(0, 16, 0, 16)) & 0xFFFF; }
};

//Command signature
//Note: mip dst and src are present, but only because they exist on the CPU.
//		these aren't really needed, since _input and _output are already set so only that mip is active.
//		In the case of mainMultiple they can be brought together to form the image dispatch offset instead.
//		This will allow each dispatch to quickly identify which group is responsible for which region.

U32 _regionCount;
U32 _padding;

#if $THREAD_COUNT != 1
	CopyImageRegion _regions[128];		//Likely not all allocated, avoid reading >= regionCount
#else
	CopyImageRegion _regions[1];		//Brings root constant size to 32 bytes
#endif

Texture2DArray<U32x4> _input;
RWTexture2DArray<U32x4> _output;

//Simplest variant, only 1 dispatch, allows us to use root constants and works everywhere.
//Only turn on rotate if sizRot.w != 0

[[oxc::stage("compute")]]
[[oxc::uniforms("THREAD_COUNT" = "1")]]
[[oxc::uniforms("THREAD_COUNT" = "1", "ROTATE" = "1")]]
[numthreads(16, 8, 1)]
void mainSingle(U32x3 id : SV_DispatchThreadID) {

	U32x4 xyzRot = _regions[0].getSizRot();

	if(any(id >= xyzRot.xyz))
		return;

	U32x3 src = _regions[0].getSrc().xyz + id;
	U32x3 dst = _regions[0].getDst().xyz + id;

	#if ROTATE

		if(xyzRot.w & 1) {
			xyzRot.xy = xyzRot.yx;
			dst.xy = dst.yx;
		}

		if(xyzRot.w < 3)
			dst.y = xyzRot.y - 1 - dst.y;

		if(xyzRot.w > 1)
			dst.x = xyzRot.x - 1 - dst.x;

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
[[oxc::uniforms("THREAD_COUNT" = "4")]]		//Warp
[[oxc::uniforms("THREAD_COUNT" = "8")]]		//Samsung (sometimes)
[[oxc::uniforms("THREAD_COUNT" = "16")]]	//Intel (sometimes)
[[oxc::uniforms("THREAD_COUNT" = "32")]]	//NV/AMD/Intel
[[oxc::uniforms("THREAD_COUNT" = "64")]]	//AMD & Samsung
[[oxc::uniforms("THREAD_COUNT" = "128")]]	//QCOM, ARM
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
