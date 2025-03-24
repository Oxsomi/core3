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

//This is basically the same idea as AMD's single pass downsampler (SPD) except easier (albeit slower).
//The idea is to be able to avoid any stalls in between to allow mip maps in a single compute.
//To do this, it loads all pixel values that will be sampled by the 64x64 blur.
//Then, we remember the 64x64 region's value and use that in the last section to resolve the final mips.
//This is done by using an atomic to keep track of how many blocks were processed

#define THREAD_SIZE 8
#define PIXEL_PER_THREAD 16
#define PIXEL_PER_THREAD_02 (PIXEL_PER_THREAD / 2)
#define PIXEL_PER_THREAD_04 (PIXEL_PER_THREAD / 4)
#define PIXEL_PER_THREAD_08 (PIXEL_PER_THREAD / 8)
#define PIXEL_PER_THREAD_16 (PIXEL_PER_THREAD / 16)
#define DOWNSCALE 64			//One thread per 64x64

#ifdef __OXC_EXT_16BITTYPES
	typedef F16x4 Flp4;
#else
	typedef F32x4 Flp4;
#endif

groupshared Flp4 intermediate[PIXEL_PER_THREAD][PIXEL_PER_THREAD];
groupshared Flp4 reduc1[PIXEL_PER_THREAD_02][PIXEL_PER_THREAD_02];
groupshared Flp4 reduc2[PIXEL_PER_THREAD_04][PIXEL_PER_THREAD_04];
groupshared Flp4 reduc4[PIXEL_PER_THREAD_08][PIXEL_PER_THREAD_08];
groupshared Flp4 reduc8;

RWStructuredBuffer<U32> _atomic;

Texture2D<F32x4> _baseTexture;
RWTexture2DArray<F32x4> _writeSlices;						//Resource(s) excluding the first one
// [globallycoherent] RWTexture2DArray<Flp4> _writeSlices6;	//6 slice+

enum MipFilter {
	MipFilter_Avg,
	MipFilter_Min,
	MipFilter_Max
};

#if $FILTER == MipFilter_Min
	Flp4 filter(Flp4 i00, Flp4 i01, Flp4 i10, Flp4 i11) { return min(min(i00, i01), min(i10, i11)); }
#elif $FILTER == MipFilter_Max
	Flp4 filter(Flp4 i00, Flp4 i01, Flp4 i10, Flp4 i11) { return max(max(i00, i01), max(i10, i11)); }
#else
	Flp4 filter(Flp4 i00, Flp4 i01, Flp4 i10, Flp4 i11) { return ((i00 + i01) + (i10 + i11)) * 0.25; }
#endif

#ifdef __OXC_EXT_16BITTYPES		//Some operations still use F32x4 when not storing to groupshared
	#if $FILTER == MipFilter_Min
		F32x4 filterFull(F32x4 i00, F32x4 i01, F32x4 i10, F32x4 i11) { return min(min(i00, i01), min(i10, i11)); }
	#elif $FILTER == MipFilter_Max
		F32x4 filterFull(F32x4 i00, F32x4 i01, F32x4 i10, F32x4 i11) { return max(max(i00, i01), max(i10, i11)); }
	#else
		F32x4 filterFull(F32x4 i00, F32x4 i01, F32x4 i10, F32x4 i11) { return ((i00 + i01) + (i10 + i11)) * 0.25; }
	#endif
#else
	F32x4 filterFull(F32x4 i00, F32x4 i01, F32x4 i10, F32x4 i11) { return filter(i00, i01, i10, i11); }
#endif

Flp4 read(U32x2 xy, U32 it) {
	switch(it) {
		default:	return intermediate[xy.y][xy.x];
		case 3:		return reduc1[xy.y][xy.x];
		case 4:		return reduc2[xy.y][xy.x];
		case 5:		return reduc4[xy.y][xy.x];
	}
}

void write(U32x2 xy, U32 it, U32x2 og, Flp4 v) {

	_writeSlices[U32x3(og >> it, it)] = v;

	switch(it) {
		default:	reduc1[xy.y][xy.x] = v;			break;
		case 3:		reduc2[xy.y][xy.x] = v;			break;
		case 4:		reduc4[xy.y][xy.x] = v;			break;
		case 5:		reduc8 = v;						break;
	}
}

[[oxc::stage("compute")]]
[[oxc::extension("16BitTypes")]]
[[oxc::extension()]]
[[oxc::uniforms("FILTER" = "MipFilter_Avg")]]
[[oxc::uniforms("FILTER" = "MipFilter_Min")]]
[[oxc::uniforms("FILTER" = "MipFilter_Max")]]
[numthreads(8, 8, 1)]
void main(U32x2 id : SV_DispatchThreadID, U32x2 threadId : SV_GroupThreadID) {

	U32x2 dims; U32 mips;
	_writeSlices.GetDimensions(dims.x, dims.y, mips);

	U32x2 pixelLoc = id * DOWNSCALE;

	//Load the texture into LDS, but before doing so, run a 4x4 filter over it.

	[unroll]
	for(U32 i = 0; i < PIXEL_PER_THREAD * PIXEL_PER_THREAD; i += THREAD_SIZE * THREAD_SIZE) {
		
		U32x2 xy = U32x2(i % PIXEL_PER_THREAD, i / PIXEL_PER_THREAD);

		F32x4 intermediates[4];

		[unroll]
		for(U32 j = 0; j < 4; ++j) {

			U32x2 ij = pixelLoc + (xy << 2u) + (U32x2(j & 1u, j >> 1u) << 1u);

			intermediates[j] = filterFull(
				_baseTexture[ij + U32x2(0, 0)], _baseTexture[ij + U32x2(1, 0)],
				_baseTexture[ij + U32x2(0, 1)], _baseTexture[ij + U32x2(1, 1)]
			);

			_writeSlices[U32x3(ij >> 1, 0)] = intermediates[j];
		}

		if(mips == 1)
			return;

		F32x4 filtered = filterFull(intermediates[0], intermediates[1], intermediates[2], intermediates[3]);
		_writeSlices[U32x3(pixelLoc + xy, 1)] = filtered;
		intermediate[xy.y][xy.x] = Flp4(filtered);
	}

	if(mips == 2)
		return;

	GroupMemoryBarrierWithGroupSync();

	//Reduce every groupshared back to 

	U32x2 count = PIXEL_PER_THREAD.xx;

	[unroll]
	for(U32 i = 2; i < 6; ++i) {

		if(mips == i + 1)
			return;

		count >>= 1;

		if(all(threadId < count)) {

			Flp4 sampled0 = read((threadId << 1) | U32x2(0, 0), i);
			Flp4 sampled1 = read((threadId << 1) | U32x2(1, 0), i);
			Flp4 sampled2 = read((threadId << 1) | U32x2(0, 1), i);
			Flp4 sampled3 = read((threadId << 1) | U32x2(1, 1), i);
			
			Flp4 result = filter(sampled0, sampled1, sampled2, sampled3);
			write(threadId, i, pixelLoc, result);
		}

		GroupMemoryBarrierWithGroupSync();
	}
}