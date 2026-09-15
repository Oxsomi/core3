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

//types/container/platforms/unix/ubuffer_random.c

#include "types/container/buffer.h"
#include "types/base/time.h"
#include "types/base/platform_types.h"

#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
	#include <Security/SecRandom.h>
#else
	#include <stddef.h> //Before sys/random.h: emscripten's compat header uses size_t without declaring it
	#include <sys/random.h>
#endif

Bool Buffer_csprng(const Buffer target) {

	if(!Buffer_length(target) || Buffer_isConstRef(target))
		return false;

	#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
		return !SecRandomCopyBytes(kSecRandomDefault, Buffer_length(target), target.ptrNonConst);
	#elif _PLATFORM_TYPE == PLATFORM_WEB

		//emscripten's libc implements getentropy (crypto.getRandomValues / node crypto) but not getrandom,
		// and a single getentropy request caps at 256 bytes, so fill in chunks.

		U64 len = Buffer_length(target);

		for(U64 off = 0; off < len; off += 256)
			if(getentropy(target.ptrNonConst + off, len - off > 256 ? 256 : len - off))
				return false;

		return true;
	#else
		size_t bytes = getrandom(target.ptrNonConst, Buffer_length(target), GRND_NONBLOCK);
		return bytes == Buffer_length(target);
	#endif
}
