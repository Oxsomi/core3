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

//cmake/linux/asan_no_deepbind.c

//RTLD_DEEPBIND gives a library its own symbol scope, which the sanitizer runtimes can't interpose through,
// so a driver loaded that way gets the real allocator while the process that calls it gets the poisoned one.
//NVIDIA's Vulkan driver loads its compiler that way, and the failure it produces is silent from the outside:
// vkCreateDevice returns VK_ERROR_INITIALIZATION_FAILED and the adapter looks broken rather than unsupported.
//A sanitizer can't be made to skip one library, since interception is per process, so the flag is dropped
// here instead. Dropping it only costs the driver its private symbol scope.
//Preloaded ahead of the test executables by build.py for sanitized builds, and inert everywhere else:
// no dlopen in the process asks for the flag unless a driver does.

#define _GNU_SOURCE
#include <dlfcn.h>

void *dlopen(const char *file, int mode) {

	static void *(*next)(const char*, int);

	if(!next)
		next = dlsym(RTLD_NEXT, "dlopen");

	return next(file, mode & ~RTLD_DEEPBIND);
}
