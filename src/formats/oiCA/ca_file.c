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

//formats/oiCA/ca_file.c

#include "types/container/list_impl.h"
#include "formats/oiCA/ca_file.h"
#include "formats/oiDL/dl_file.h"
#include "formats/oiDL/dl_entry.h"

TListImpl(CAFolderInfo);
TListImpl(CAFileInfo);
TListImpl(CAFile);

Bool CAFile_create(
	const CASettings *settings,
	U64 reservedFiles,
	U64 reservedFolders,
	const Allocator *alloc,
	CAFile *caFile,
	Error *e_rr
) {
	Bool s_uccess = true;

	if (!caFile)
		retError(clean, Error_nullPointer(4, "CAFile_create()::caFile is required"));

	if (!settings)
		retError(clean, Error_nullPointer(0, "CAFile_create()::settings is required"));

	if (settings->compressionType >= EXXCompressionType_Count)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_create()::settings.compressionType is invalid"));

	if (settings->compressionType > EXXCompressionType_None)
		retError(clean, Error_unsupportedOperation(0, "CAFile_create() compression not supported yet"));        //TODO:

	if (settings->flags & ECASettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 0, "CAFile_create()::settings->flags contains invalid flags"));

	if(caFile->names.settings.dataType)
		retError(clean, Error_invalidParameter(3, 0, "CAFile_create()::caFile was already allocated, indicating memleak"));

	if (settings->encryptionType >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 1, "CAFile_create()::settings.encryptionType is invalid"));

	if(settings->flags & ECASettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 2, "CAFile_create()::flags is invalid"));

	caFile->settings = *settings;
	caFile->version = 0;        //Debug generation counter starts at 0; add/move/remove bump it (see CAHandle note)

	//Names

	DLSettings nameSettings = (DLSettings) {
		.compressionType = settings->compressionType,        //TODO: Maybe allow different compression type here later
		.encryptionType  = settings->encryptionType,
		.dataType        = EDLDataType_String,
		.flags           = EDLSettingsFlags_HideMagicNumber
	};

	if (settings->encryptionType)
		Buffer_memcpy(
			Buffer_createRef(nameSettings.encryptionKey, sizeof(nameSettings.encryptionKey)),
			Buffer_createRefConst(settings->encryptionKey, sizeof(settings->encryptionKey))
		);

	gotoIfError3(clean, DLFile_create(&nameSettings, 0, alloc, &caFile->names, e_rr));

	gotoIfError3(clean, DLFile_reserve(&caFile->names, 1 + reservedFiles + reservedFolders, alloc, e_rr));

	//Content

	DLSettings contentSettings = (DLSettings) {
		.compressionType = settings->compressionType,
		.encryptionType  = settings->encryptionType,
		.dataType        = EDLDataType_Data,
		.flags           = EDLSettingsFlags_HideMagicNumber
	};

	if (settings->encryptionType)
		Buffer_memcpy(
			Buffer_createRef(contentSettings.encryptionKey, sizeof(contentSettings.encryptionKey)),
			Buffer_createRefConst(settings->encryptionKey, sizeof(settings->encryptionKey))
		);

	gotoIfError3(clean, DLFile_create(&contentSettings, 0, alloc, &caFile->content, e_rr));

	gotoIfError3(clean, ListCAFolderInfo_reserve(&caFile->folders, 1 + reservedFolders, alloc, e_rr));

	if (reservedFiles) {
		gotoIfError3(clean, DLFile_reserve(&caFile->content, reservedFiles, alloc, e_rr));
		gotoIfError3(clean, ListCAFileInfo_reserve(&caFile->files, reservedFiles, alloc, e_rr));
	}

	//We push root at 0, we do this so we can easily parent/traverse even from the root's perspective

	CharString empty = CharString_createNull();
	gotoIfError3(clean, DLFile_addEntryString(&caFile->names, &empty, alloc, e_rr));

	CAFolderInfo root = (CAFolderInfo) { 0 };
	gotoIfError3(clean, ListCAFolderInfo_pushBack(&caFile->folders, root, alloc, e_rr));

clean:

	//These local copies hold the secret encryption key, so wipe them before returning
	Buffer_clearAllSecure(Buffer_createRef(nameSettings.encryptionKey, sizeof(nameSettings.encryptionKey)));
	Buffer_clearAllSecure(Buffer_createRef(contentSettings.encryptionKey, sizeof(contentSettings.encryptionKey)));

	if (!s_uccess)
		CAFile_free(caFile, alloc);

	return s_uccess;
}

Bool CAFile_createCopy(const CAFile *caFile, const Allocator *alloc, CAFile *result, Error *e_rr) {

	Bool s_uccess = true;

	if (!caFile || !result)
		retError(clean, Error_nullPointer(!caFile ? 0 : 2, "CAFile_createCopy()::caFile and result are required"));

	*result = (CAFile) { 0 };

	gotoIfError3(clean, DLFile_createCopy(&caFile->names, alloc, &result->names, e_rr));
	gotoIfError3(clean, DLFile_createCopy(&caFile->content, alloc, &result->content, e_rr));
	gotoIfError3(clean, ListCAFolderInfo_createCopy(caFile->folders, alloc, &result->folders, e_rr));
	gotoIfError3(clean, ListCAFileInfo_createCopy(caFile->files, alloc, &result->files, e_rr));

	result->settings = caFile->settings;

clean:

	if (!s_uccess)
		CAFile_free(result, alloc);

	return s_uccess;
}

void CAFile_free(CAFile *caFile, const Allocator *alloc) {

	if(!caFile)
		return;

	DLFile_free(&caFile->names, alloc);
	DLFile_free(&caFile->content, alloc);
	ListCAFolderInfo_free(&caFile->folders, alloc);
	ListCAFileInfo_free(&caFile->files, alloc);

	//The encryption key is secret, so wipe it before freeing the struct
	Buffer_clearAllSecure(Buffer_createRef(caFile->settings.encryptionKey, sizeof(caFile->settings.encryptionKey)));

	*caFile = (CAFile) { 0 };
}
