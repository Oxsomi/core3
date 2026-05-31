/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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

//platforms/unix/ufile.c

#include "platforms/ext/listx_impl.h"
#include "types/base/error.h"
#include "types/base/lock.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "formats/oiCA/ca_file.h"
#include "platforms/file.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/archivex.h"
#include "platforms/log.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef S_IWUSR
	#define S_IWUSR S_IWRITE
#endif

Bool File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr);

Bool File_foreach(CharString loc, Bool inAppDir, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr) {

	CharString resolved = CharString_createNull();
	CharString resolvedChild = CharString_createNull();
	DIR *d = NULL;
	Bool s_uccess = true;

	if(!callback)
		retError(clean, Error_nullPointer(1, "File_foreach()::callback is required"));

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_foreach()::loc must be a valid file path"));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_foreachVirtual(loc, callback, userData, isRecursive, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, inAppDir, 0, &resolved, e_rr));

	if(isVirtual)
		retError(clean, Error_invalidOperation(0, "File_foreach()::loc can't resolve to virtual here"));

	d = opendir(resolved.ptr);

	if(!d)
		retError(clean, Error_notFound(0, 0, "File_foreach()::loc not found"));

	struct dirent *dir = NULL;

	while ((dir = readdir(d)) != NULL) {

		CharString dirName = CharString_createRefCStrConst(dir->d_name);
		U64 dirLen = CharString_length(dirName);

		//. or ..

		if((dirLen == 1 || dirLen == 2) && dirName.ptr[0] == '.') {
			if(dirLen == 1 || dirName.ptr[1] == '.')
				continue;
		}

		gotoIfError2(clean, CharString_createCopyx(resolved, &resolvedChild));
		gotoIfError2(clean, CharString_appendx(&resolvedChild, '/'))
		gotoIfError2(clean, CharString_appendStringx(&resolvedChild, dirName))

		struct stat s = (struct stat) { 0 };
		if(stat(resolvedChild.ptr, &s))
			retError(clean, Error_stderr(errno, "File_foreach() failed to query file properties"));

		CharString tmp = CharString_createRefSizedConst(resolvedChild.ptr, CharString_length(resolvedChild), true);

		//Folder parsing

		if(S_ISDIR(s.st_mode)) {

			FileInfo info = (FileInfo) {
				.path = tmp,
				.timestamp = s.st_mtime,
				.access = s.st_mode & S_IWUSR ? EFileAccess_ReadWrite : EFileAccess_Read,
				.type = EFileType_Folder
			};

			gotoIfError3(clean, callback(info, userData, e_rr));

			if(isRecursive)
				gotoIfError3(clean, File_foreach(info.path, inAppDir, callback, userData, true, e_rr));

			CharString_freex(&resolvedChild);
			continue;
		}

		//File parsing

		FileInfo info = (FileInfo) {
			.path = tmp,
			.timestamp = s.st_mtime,
			.access = s.st_mode & S_IWUSR ? EFileAccess_ReadWrite : EFileAccess_Read,
			.type = EFileType_File,
			.fileSize = s.st_size
		};

		gotoIfError3(clean, callback(info, userData, e_rr));
		CharString_freex(&resolvedChild);
	}

clean:
	closedir(d);
	CharString_freex(&resolvedChild);
	CharString_freex(&resolved);
	return s_uccess;
}

#if _PLATFORM_TYPE != PLATFORM_ANDROID

	Bool File_loadVirtualInternal1(FileLoadVirtual *userData, CharString loc, Bool allowLoad, Error *e_rr) {

		CharString isChild = CharString_createNull();
		Bool s_uccess = true;
		ELockAcquire acq = ELockAcquire_Invalid;

		gotoIfError2(clean, CharString_createCopyx(loc, &isChild))

		if(CharString_length(isChild))
			gotoIfError2(clean, CharString_appendx(&isChild, '/'))        //Don't append to root

		acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			retError(clean, Error_invalidState(0, "File_loadVirtualInternal1() couldn't lock virtualSectionsLock"))

		Bool foundAny = false;

		for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {

			VirtualSection *section = Platform_instance->virtualSections.ptrNonConst + i;

			if(
				!CharString_equalsStringInsensitive(loc, section->path) &&
				!CharString_startsWithStringInsensitive(section->path, isChild, 0)
			)
				continue;

			//Load

			if (userData->doLoad) {

				if (!section->loaded) {

					if (!allowLoad)
						retError(clean, Error_notFound(0, 0, "File_loadVirtualInternal1() was queried but none was found"));

					CAFile file = (CAFile) { 0 };
					Buffer buf = Buffer_createRefConst(section->dataExt, section->lenExt);

					gotoIfError3(clean, CAFile_readx(buf, userData->encryptionKey, &file, e_rr))

					section->loadedData = file.archive;
					section->loaded = true;
					foundAny = true;
				}

				else foundAny = true;
			}

			//Otherwise we want to use error to determine if it's present or not

			else if(!section->loaded)
				retError(clean, Error_notFound(1, 1, "File_loadVirtualInternal1()::loc not found (1)"))
		}

		if(!foundAny)
			retError(clean, Error_notFound(2, 1, "File_loadVirtualInternal1()::loc not found (2)"))

	clean:

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&Platform_instance->virtualSectionsLock);

		CharString_freex(&isChild);
		return s_uccess;
	}

#endif
