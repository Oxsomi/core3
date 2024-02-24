/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/error.h"
#include "types/buffer.h"
#include "formats/oiCA.h"
#include "platforms/file.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/archivex.h"

#include <dirent.h>
#include <stdio.h>
#include <stat.h>
#include <errno.h>

Error File_foreach(CharString loc, FileCallback callback, void *userData, Bool isRecursive) {

	CharString resolved = CharString_createNull();
	Error err = Error_none();

	if(!callback)
		_gotoIfError(clean, Error_nullPointer(1, "File_foreach()::callback is required"));

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, "File_foreach()::loc must be a valid file path"));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_foreachVirtual(loc, callback, userData, isRecursive));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	if(isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0, "File_foreach()::loc can't resolve to virtual here"));

	DIR *d = opendir(resolved.ptr);

	if(!d)
		_gotoIfError(clean, Error_notFound(0, 0, "File_foreach()::loc not found"));

	while ((dir = readdir(d)) != NULL) {

		struct stat s = (struct stat) { 0 };
		if(stat(dir->d_name, &s))
			_gotoIfError(clean, Error_stderr(errno, "File_foreach() failed to query file properties"));

		//Folder parsing

		if(S_ISDIR(s.st_mode)) {

			CharString tmp = CharString_createRefCStrConst(dir->d_name);
			FileInfo info = (FileInfo) {
				.path = tmp,
				.timestamp = s.st_mtime,
				.access = !S_IWRITE(s.st_mode) ? EFileAccess_Read : EFileAccess_ReadWrite,
				.type = EFileType_Folder
			};

			_gotoIfError(clean, callback(info, userData));

			if(isRecursive)
				_gotoIfError(clean, File_foreach(info.path, callback, userData, true));

			continue;
		}

		//File parsing

		FileInfo info = (FileInfo) {
			.path = tmp,
			.timestamp = s.st_mtime,
			.access = !S_IWRITE(s.st_mode) ? EFileAccess_Read : EFileAccess_ReadWrite,
			.type = EFileType_File,
			.fileSize = s.st_size
		};

		_gotoIfError(clean, callback(info, userData));
	}

	closedir(d);
	return err;
}
