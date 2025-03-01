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

#include "platforms/ext/listx_impl.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "types/base/atomic.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/archivex.h"

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

void *Platform_allocate(void *allocator, U64 length) { (void)allocator; return malloc(length); }
void Platform_free(void *allocator, void *ptr, U64 length) { (void) allocator; (void)length; free(ptr); }

impl Bool Platform_initUnixExt(Error *e_rr);
impl void Platform_cleanupUnixExt();

void Platform_cleanupExt() {
	Platform_cleanupUnixExt();
}

void *Platform_getDataImpl(void *ptr) { (void) ptr; return NULL; }

U64 Platform_getThreads() { return sysconf(_SC_NPROCESSORS_ONLN); }

Bool Platform_initExt(Error *e_rr) {

	Bool s_uccess = true;

	//Get work directory

	#define PATH_MAX 256
	C8 cwd[PATH_MAX + 1];
	if (!getcwd(cwd, sizeof(cwd)))
		retError(clean, Error_stderr(0, "Platform_initExt() getcwd failed"))

	gotoIfError2(clean, CharString_createCopyx(CharString_createRefCStrConst(cwd), &Platform_instance->workDirectory))
	gotoIfError2(clean, CharString_appendx(&Platform_instance->workDirectory, '/'))

	//Initialize Linux/OSX dependent as well as app dir path

	gotoIfError3(clean, Platform_initUnixExt(e_rr))
	
	//Make default path

	Platform_instance->defaultDir = CharString_createRefStrConst(
		Platform_instance->useWorkingDir ? Platform_instance->workDirectory : Platform_instance->appDirectory
	);

clean:
	return s_uccess;
}
