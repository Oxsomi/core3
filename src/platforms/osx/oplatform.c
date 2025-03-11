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
#include "types/base/error.h"
#include "types/base/thread.h"
#include "types/base/atomic.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <mach-o/loader.h>

Bool Platform_initUnixExt(Error *e_rr) {

	//Get exe name

	Bool s_uccess = true;
	C8 exeName[256];
	U32 exeNameLen = 255;
	CharString tmpStr = CharString_createNull();
	I32 fd = -1;
	C8 *ptr = NULL;
	U64 fileSize = 0;

  	if (_NSGetExecutablePath(exeName, &exeNameLen) != 0)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() exePath exceeds maximum"))

	exeName[exeNameLen] = '\0';

	Bool containedSlash = false;

	for(U64 i = exeNameLen - 1; i != U64_MAX; --i)
		if(exeName[i] == '/') {
			containedSlash = true;
			exeName[i + 1] = '\0';
			exeNameLen = i + 1;
			break;
		}

	if(!containedSlash)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() couldn't find app base path"))

	gotoIfError2(clean, CharString_createCopyx(
		CharString_createRefSizedConst(exeName, exeNameLen, true), &Platform_instance->appDirectory
	))

	//Try to open the main executable within 1s, if it fails we can't init

	U64 i = 0;

	for(; i < 1000 && (fd = open(exeName, O_RDONLY)) < 0; ++i) {

		if(errno != EINTR)
			retError(clean, Error_stderr(0, "Platform_initUnixExt() open failed on executable"))

		Thread_sleep(MS);
	}

	if(i == 1000)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() executable couldn't be opened in time"))

	//Grab file data

	fileSize = lseek(fd, 0, SEEK_END);
	ptr = (C8*) mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);

	if(ptr == (const C8*) MAP_FAILED)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() executable couldn't be mapped"))

	//Read sections

	Bool anySection = false;
	const struct mach_header_64 *header = (const struct mach_header_64*) ptr;
	const struct load_command *lc = (const struct load_command*)((const C8*) ptr + sizeof(struct mach_header_64));

	for (U32 i = 0; i < header->ncmds; i++) {

        if (lc->cmd == LC_SEGMENT_64) {

            const struct segment_command_64 *seg = (const struct segment_command_64*) lc;
            const struct section_64 *sec = (const struct section_64*)((const C8*)seg + sizeof(struct segment_command_64));

            for (U32 j = 0; j < seg->nsects; j++) {

				if(seg->segname[0] != '@')		//Packages are marked with @ in front
					continue;

				gotoIfError2(clean, CharString_formatx(&tmpStr, "%s/%s", seg->segname + 1, sec[j].sectname))

				VirtualSection section = (VirtualSection) { .path = tmpStr };
				section.lenExt = sec[j].size;
				section.dataExt = ptr + sec[j].offset;

				gotoIfError2(clean, ListVirtualSection_pushBackx(&Platform_instance->virtualSections, section))

				tmpStr = CharString_createNull();
				anySection = true;
			}
        }

        lc = (const struct load_command*)((const C8*)lc + lc->cmdsize);
    }

	//Keep file open until end of program.
	//Unless there's no need (when there's no sections present).
	//This doesn't keep anything in memory, until we actually load the sections.

	if(anySection) {
		Platform_instance->data = (void*) (U64) fd;
		Platform_instance->data1 = ptr;
		Platform_instance->size1 = fileSize;
		fd = -1;
		ptr = NULL;
	}

clean:

	if(fd >= 0)
		close(fd);

	if(ptr)
		munmap(ptr, fileSize);

	CharString_freex(&tmpStr);
	return s_uccess;
}

void Platform_cleanupUnixExt() {
	if(Platform_instance->data1) {
		munmap(Platform_instance->data1, Platform_instance->size1);
		close((I32)(U64) Platform_instance->data);
	}
}
