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

#include "platforms/platform.h"

Bool Platform_checkCPUSupport() {

	//We need to double check that our CPU supports
	//SSE4.2, SSE4.1, (S)SSE3, SSE2, SSE, AES, PCLMULQDQ, BMI1 and RDRAND
	//https://gist.github.com/hi2p-perim/7855506
	//https://en.wikipedia.org/wiki/CPUID

	U32 mask3 = (1 << 25) | (1 << 26);										//SSE, SSE2

	//SSE3, PCLMULQDQ, SSSE3, SSE4.1, SSE4.2, AES, RDRAND
	U32 mask2 = (1 << 0) | (1 << 1) | (1 << 9) | (1 << 19) | (1 << 20) | (1 << 25) | (1 << 30);

	U32 cpuInfo[4];
	Platform_getCPUId(1, cpuInfo);

	U32 cpuInfo1[4];
	Platform_getCPUId(7, cpuInfo1);

	U32 mask1_1 = 1 << 3;				//BMI1

	//Unsupported CPU

	return (cpuInfo[3] & mask3) == mask3 && (cpuInfo[2] & mask2) == mask2 && (cpuInfo1[1] & mask1_1) == mask1_1;
}
