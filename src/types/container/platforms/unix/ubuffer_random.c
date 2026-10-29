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

#if _PLATFORM_TYPE != PLATFORM_OSX && _PLATFORM_TYPE != PLATFORM_IOS && _PLATFORM_TYPE != PLATFORM_WEB

	#include <errno.h>
	#include <fcntl.h>
	#include <unistd.h>

	//The kernel's own pool through its device, for when the getrandom syscall can't be used.
	static Bool Buffer_csprngDevice(U8 *ptr, U64 left) {

		const int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);

		if (fd < 0)
			return false;

		while (left) {

			const ssize_t got = read(fd, ptr, left > ((U64) 1 << 30) ? (size_t) 1 << 30 : (size_t) left);

			if (got < 0 && errno == EINTR)
				continue;

			if (got <= 0) {
				close(fd);
				return false;
			}

			ptr += got;
			left -= (U64) got;
		}

		close(fd);
		return true;
	}

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

		//getrandom hands out at most 32 MiB - 1 per call, and a call can come back short or be interrupted, so loop.
		//Without flags it only ever waits once, until the kernel's pool is first seeded, rather than failing early in boot.
		//Without the syscall (ENOSYS on kernels before 3.17, EPERM in some seccomp sandboxes) the device is the same pool.

		U8 *ptr = target.ptrNonConst;
		U64 left = Buffer_length(target);
		const U64 maxCall = ((U64) 1 << 25) - 1;

		while (left) {

			const ssize_t got = getrandom(ptr, (size_t) (left > maxCall ? maxCall : left), 0);

			if (got < 0 && errno == EINTR)
				continue;

			if (got < 0 && (errno == ENOSYS || errno == EPERM))
				return Buffer_csprngDevice(ptr, left);

			if (got <= 0)
				return false;

			ptr += got;
			left -= (U64) got;
		}

		return true;
	#endif
}
