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

//platforms/simd/neon/neon_platform.c

#include "platforms/platform.h"
#include "platforms/logx.h"

Bool Platform_checkCPUSupport() {

	U16 v = 1;

	if(!*(const U8*)&v)        //Little endian only
		return false;

	//CMakeLists.txt compiles the whole arm64 build with -march=armv8-a+simd+crypto+crc, and the NEON encrypt
	// path calls vaeseq_u8 unconditionally because SIMD_createCryptoState only exists on SSE.
	//So unlike CRC32C and SHA-256, which pick a fallback at runtime, AES has no guard behind this one:
	// an ARMv8-A part without the crypto extension would fault the first time anything encrypts an archive.

	const ECPUFeatures features = Platform_detectCPUFeatures();
	const ECPUFeatures required = ECPUFeatures_HwAES | ECPUFeatures_PCLMULQDQ | ECPUFeatures_HwCRC32C;

	if((features & required) == required)
		return true;

	//Same reasoning as the SSE backend: Log needs the platform allocator, so it is only reachable once
	// Platform_create has built the instance, and a standalone call just reports less.

	if(Platform_instance)
		Log_errorLnx(
			"Unsupported CPU, this build requires the ARMv8 crypto and CRC extensions (have %08X, need %08X)",
			(U32) features, (U32) required
		);

	return false;
}
