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

//platforms/linux/lplatform.c

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE64_SOURCE

#include "types/container/list_impl.h"
#include "platforms/platform.h"
#include "platforms/keyboard.h"
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/base/thread.h"
#include "types/base/error.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <elf.h>

Bool Platform_initUnixExt(Error *e_rr) {

	Bool s_uccess = true;
	CharString tmpStr = CharString_createNull();

	//Grab exe name first, so we can find all sections that exist

	C8 exeName[1024];
	I32 fd = -1;
	C8 *ptr = NULL;
	U64 fileSize = 0;
	I32 exeNameLen = readlink("/proc/self/exe", exeName, sizeof(exeName) - 1);

	if(exeNameLen < 0)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() couldn't find out executable name"));

	exeName[exeNameLen] = '\0';

	Bool containedSlash = false;

	for(U64 i = exeNameLen - 1; i != U64_MAX; --i)
		if(exeName[i] == '/') {
			containedSlash = true;
			exeNameLen = i + 1;
			break;
		}

	if(!containedSlash)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() couldn't find app base path"));

	CharString appDir = CharString_createRefSizedConst(exeName, (U64)exeNameLen, false);

	gotoIfError3(clean, CharString_createCopy(Platform_instance->alloc, appDir, &Platform_instance->appDirectory, e_rr));

	//Try to open the main executable within 1s, if it fails we can't init

	U64 i = 0;

	for(; i < 1000 && (fd = open(exeName, O_RDONLY)) < 0; ++i) {

		if(errno != EINTR)
			retError(clean, Error_stderr(0, "Platform_initUnixExt() open failed on executable"));

		Thread_sleep(MS);
	}

	if(i == 1000)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() executable couldn't be opened in time"));

	//Grab file data

	fileSize = lseek(fd, 0, SEEK_END);
	ptr = (C8*) mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);

	if(ptr == (const C8*) MAP_FAILED)
		retError(clean, Error_invalidState(0, "Platform_initUnixExt() executable couldn't be mapped"));

	//Read sections

	Bool anySection = false;

	const Elf64_Ehdr *elf = (const Elf64_Ehdr*) ptr;
	const Elf64_Shdr *shdr = (const Elf64_Shdr*) (ptr + elf->e_shoff);
	const C8 *strings = (const C8*) (ptr + shdr[elf->e_shstrndx].sh_offset);

	for(U64 i = 0; i < elf->e_shnum; ++i) {

		CharString sectionName = CharString_createRefCStrConst(&strings[shdr[i].sh_name]);

		if(!CharString_startsWithStringSensitive(sectionName, CharString_createRefCStrConst("packages/"), 0))
			continue;

		sectionName.ptr += sizeof("packages");        //sizeof includes null terminator so no need for packages/
		sectionName.lenAndNullTerminated -= sizeof("packages");

		gotoIfError3(clean, CharString_createCopy(Platform_instance->alloc, sectionName, &tmpStr, e_rr));

		VirtualSection section = (VirtualSection) { .path = tmpStr };
		section.lenExt = shdr[i].sh_size;
		section.dataExt = ptr + shdr[i].sh_offset;

		gotoIfError3(clean, ListVirtualSection_pushBack(
			Platform_instance->alloc, &Platform_instance->virtualSections, section, e_rr
		));

		tmpStr = CharString_createNull();
		anySection = true;
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

	CharString_free(Platform_instance->alloc, &tmpStr);
	return s_uccess;
}

void Platform_cleanupUnixExt() {
	if(Platform_instance->data1) {
		munmap(Platform_instance->data1, Platform_instance->size1);
		close((I32)(U64) Platform_instance->data);
	}
}

Bool Keyboard_remap(const Keyboard *keyboard, EKey key, const Allocator *alloc, CharString *result, Error *e_rr) {

	(void) key; (void) keyboard; (void) alloc; (void) result;

	Bool s_uccess = true;
	retError(clean, Error_unimplemented(0, "Keyboard_remap() unimplemented on linux for now"));        //TODO:

clean:
	return s_uccess;
}
