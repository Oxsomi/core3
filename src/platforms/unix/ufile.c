/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/error.h"
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

Bool File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr);

Bool File_foreach(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr) {

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

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 0, &resolved, e_rr));

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
				.access = s.st_mode & S_IWRITE ? EFileAccess_ReadWrite : EFileAccess_Read,
				.type = EFileType_Folder
			};

			gotoIfError3(clean, callback(info, userData, e_rr));

			if(isRecursive)
				gotoIfError3(clean, File_foreach(info.path, callback, userData, true, e_rr));

			CharString_freex(&resolvedChild);
			continue;
		}

		//File parsing

		FileInfo info = (FileInfo) {
			.path = tmp,
			.timestamp = s.st_mtime,
			.access = s.st_mode & S_IWRITE ? EFileAccess_ReadWrite : EFileAccess_Read,
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
