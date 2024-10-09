/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "formats/oiCA/ca_file.h"
#include "types/allocator.h"
#include "types/error.h"

Bool CAFile_create(CASettings settings, Archive *archive, CAFile *caFile, Error *e_rr) {

	Bool s_uccess = true;

	if(!caFile)
		retError(clean, Error_nullPointer(0, "CAFile_create()::caFile is required"))

	if(!archive || !archive->entries.ptr)
		retError(clean, Error_nullPointer(1, "CAFile_create()::archive is empty"))

	if(caFile->archive.entries.ptr)
		retError(clean, Error_invalidParameter(2, 0, "CAFile_create()::caFile wasn't empty, could indicate memleak"))

	if(settings.compressionType >= EXXCompressionType_Count)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_create()::settings.compressionType is invalid"))

	if(settings.compressionType > EXXCompressionType_None)
		retError(clean, Error_unsupportedOperation(0, "CAFile_create() compression not supported yet"))			//TODO:

	if(settings.encryptionType >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 1, "CAFile_create()::settings.encryptionType is invalid"))

	if(settings.flags & ECASettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 2, "CAFile_create()::flags is invalid"))

	caFile->archive = *archive;
	*archive = (Archive) { 0 };

	caFile->settings = settings;

clean:
	return s_uccess;
}

Bool CAFile_free(CAFile *caFile, Allocator alloc) {

	if(!caFile || !caFile->archive.entries.ptr)
		return true;

	const Bool b = Archive_free(&caFile->archive, alloc);
	*caFile = (CAFile) { 0 };
	return b;
}
