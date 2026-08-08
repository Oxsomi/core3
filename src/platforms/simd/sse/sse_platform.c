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

//platforms/simd/sse/sse_platform.c

#include "platforms/platform.h"

#include <stdio.h>

Bool Platform_checkCPUSupport() {

	U16 v = 1;

	if(!*(const U8*)&v)        //Little endian only
		return false;

	//We need to double check that our CPU supports
	//SSE4.2, SSE4.1, (S)SSE3, SSE2, SSE, AES, PCLMULQDQ, BMI1, AVX, FMA
	//AVX + FMA (Haswell 2013 / Zen) are required because we compile with -mavx -mfma and F32x4_fma is unconditional;
	//they're already implied by the -mbmi2 / -mf16c compile flags, so this excludes no CPU we didn't already exclude.
	//https://gist.github.com/hi2p-perim/7855506
	//https://en.wikipedia.org/wiki/CPUID

	U32 mask3 = (1 << 25) | (1 << 26);                                        //SSE, SSE2

	//SSE3, PCLMULQDQ, SSSE3, FMA, SSE4.1, SSE4.2, AES, OSXSAVE, AVX
	U32 mask2 =
		(1 << 0) | (1 << 1) | (1 << 9) | (1 << 12) | (1 << 19) | (1 << 20) | (1 << 25) | (1 << 27) | (1 << 28);

	U32 cpuInfo[4];
	Platform_getCPUId(1, cpuInfo);

	U32 cpuInfo1[4];
	Platform_getCPUId(7, cpuInfo1);

	U32 mask1_1 = 1 << 3;                //BMI1

	const Bool ok = (cpuInfo[3] & mask3) == mask3 && (cpuInfo[2] & mask2) == mask2 && (cpuInfo1[1] & mask1_1) == mask1_1;

	//Which bits are missing is the whole diagnosis on emulator/VM guests whose CPUID model masks features the host
	// happily executes anyway, and this runs before the platform (and therefore Log) exists, so raw printf is all there is.

	if(!ok)
		printf(
			"-- Fatal: CPU is missing required features: leaf1 edx %08X (need %08X), ecx %08X (need %08X), "
			"leaf7 ebx %08X (need %08X)\n",
			cpuInfo[3], mask3, cpuInfo[2], mask2, cpuInfo1[1], mask1_1
		);

	return ok;
}
