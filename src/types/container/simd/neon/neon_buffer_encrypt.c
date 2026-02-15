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

#include "types/base/types.h"

#if _PLATFORM_TYPE == PLATFORM_WINDOWS
	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
	#define NOMINMAX
	#include <Windows.h>
#elif _PLATFORM_TYPE == PLATFORM_IOS ||  _PLATFORM_TYPE == PLATFORM_OSX
	#include <sys/sysctl.h>
#elif _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_ANDROID
	#include <asm/hwcap.h>
	#include <sys/auxv.h>
#else
	#error Unsupported platform!
#endif

void AES_checkSupport(I8 *hasAES256) {
	if(*hasAES256 < 0) {
		#if _PLATFORM_TYPE == PLATFORM_WINDOWS
			*hasAES256 = !!IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
		#elif _PLATFORM_TYPE == PLATFORM_IOS ||  _PLATFORM_TYPE == PLATFORM_OSX
			*hasAES256 = 1;		//Apple says they support this always
		#elif defined(HWCAP_AES)
			*hasSHA256 = getauxval(AT_HWCAP) & HWCAP_AES;
		#else
			*hasSHA256 = 0;
		#endif
	}
}
