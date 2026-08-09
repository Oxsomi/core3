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
#include "platforms/logx.h"

Bool Platform_checkCPUSupport() {

	U16 v = 1;

	if(!*(const U8*)&v)        //Little endian only
		return false;

	//This has to cover everything X64_SIMD_FLAGS in CMakeLists.txt compiles for, because the compiler emits those
	// instructions anywhere it likes and a missing one faults at an arbitrary point in an arbitrary file.
	//Leaf 1 EDX gives SSE and SSE2, leaf 1 ECX the rest of the SSE family plus AES, PCLMULQDQ, FMA, AVX and F16C,
	// and leaf 7 EBX the BMI pair.
	//-msha is deliberately absent: its only use is runtime gated in sse_buffer_hash.c, so requiring SHA-NI here
	// would reject every pre-Ice-Lake desktop for a path they never take.
	//https://gist.github.com/hi2p-perim/7855506
	//https://en.wikipedia.org/wiki/CPUID

	U32 mask3 = (1 << 25) | (1 << 26);                                        //SSE, SSE2

	//SSE3, PCLMULQDQ, SSSE3, FMA, SSE4.1, SSE4.2, AES, OSXSAVE, AVX, F16C
	U32 mask2 =
		(1 << 0) | (1 << 1) | (1 << 9) | (1 << 12) | (1 << 19) | (1 << 20) | (1 << 25) | (1 << 27) | (1 << 28) |
		(1 << 29);

	//Zeroed because cpuid leaves the array untouched when the leaf is above the CPU's maximum.
	//Leaf 7 can be above it on pre-2012 parts and on VM CPU models that report an older family.
	//Reading it uninitialized decides CPU support from stack garbage.

	U32 cpuInfo[4] = { 0 };
	Platform_getCPUId(1, cpuInfo);

	U32 cpuInfo1[4] = { 0 };
	Platform_getCPUId(7, cpuInfo1);

	U32 mask1_1 = (1 << 3) | (1 << 8);                //BMI1, BMI2

	Bool ok = (cpuInfo[3] & mask3) == mask3 && (cpuInfo[2] & mask2) == mask2 && (cpuInfo1[1] & mask1_1) == mask1_1;

	//OSXSAVE only says the OS is allowed to enable AVX state, not that it did, and -mavx makes every SSE
	// instruction in this binary VEX encoded.
	//So without XMM and YMM actually enabled in XCR0 it isn't one path that faults, it's all of them.
	//xgetbv itself raises #UD unless OSXSAVE is set, hence the ordering.

	const Bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
	const U64 xcr0 = osxsave ? Platform_getXCR0() : 0;
	const Bool osAVX = (xcr0 & 0x6) == 0x6;

	if(!osAVX)
		ok = false;

	//Which bits are missing is the whole diagnosis on emulator and VM guests,
	// whose CPUID model masks features the host happily executes anyway.
	//Log needs the platform allocator, so Platform_create only runs this check once Platform_instance exists.
	//The guard keeps the standalone pre-create use of this function safe, it just reports less then.

	if(!ok && Platform_instance)
		Log_errorLnx(
			"Unsupported CPU, missing features: leaf1 edx %08X (need %08X), ecx %08X (need %08X), "
			"leaf7 ebx %08X (need %08X), XCR0 %08X (need bits 1 and 2 for OS enabled AVX)",
			cpuInfo[3], mask3, cpuInfo[2], mask2, cpuInfo1[1], mask1_1, (U32) xcr0
		);

	return ok;
}
