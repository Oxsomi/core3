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

#include "types/archive.h"
#include "types/allocator.h"
#include "types/list_impl.h"

TListImpl(ArchiveEntry);

Error Archive_create(Allocator alloc, Archive *archive) {

	if(!archive)
		return Error_nullPointer(1, "Archive_create()::archive is required");

	if(archive->entries.ptr)
		return Error_invalidOperation(0, "Archive_create()::archive is already initialized, indicates possible memleak");

	return ListArchiveEntry_reserve(&archive->entries, 100, alloc);
}

Bool Archive_free(Archive *archive, Allocator alloc) {

	if(!archive || !archive->entries.ptr)
		return true;

	for (U64 i = 0; i < archive->entries.length; ++i) {
		ArchiveEntry entry = archive->entries.ptr[i];
		Buffer_free(&entry.data, alloc);
		CharString_free(&entry.path, alloc);
	}

	ListArchiveEntry_free(&archive->entries, alloc);
	*archive = (Archive) { 0 };
	return true;
}

Bool Archive_getPath(
	Archive archive,
	CharString path,
	ArchiveEntry *entryOut,
	U64 *iPtr,
	CharString *resolvedPathPtr,
	Allocator alloc
) {

	if(!archive.entries.ptr || !CharString_length(path))
		return false;

	Bool isVirtual = false;
	CharString resolvedPath = CharString_createNull();

	const Error err = File_resolve(path, &isVirtual, 128, CharString_createNull(), alloc, &resolvedPath);

	if(err.genericError)
		return false;

	if (isVirtual) {
		CharString_free(&resolvedPath, alloc);
		return false;
	}

	//TODO: Optimize this with a hashmap

	for(U64 i = 0; i < archive.entries.length; ++i)
		if (CharString_equalsStringInsensitive(archive.entries.ptr[i].path, resolvedPath)) {

			if(entryOut && !CharString_length(entryOut->path))
				*entryOut = archive.entries.ptr[i];

			if(iPtr)
				*iPtr = i;

			if(resolvedPathPtr && !resolvedPathPtr->ptr)
				*resolvedPathPtr = resolvedPath;

			else CharString_free(&resolvedPath, alloc);

			return true;
		}

	CharString_free(&resolvedPath, alloc);
	return false;
}

Bool Archive_has(Archive archive, CharString path, Allocator alloc) {
	return Archive_getPath(archive, path, NULL, NULL, NULL, alloc);
}

Bool Archive_hasFile(Archive archive, CharString path, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc))
		return false;

	return entry.type == EFileType_File;
}

Bool Archive_hasFolder(Archive archive, CharString path, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc))
		return false;

	return entry.type == EFileType_Folder;
}

Error Archive_addInternal(Archive *archive, ArchiveEntry entry, Bool successIfExists, Allocator alloc);

Bool Archive_createOrFindParent(Archive *archive, CharString path, Allocator alloc) {

	//If it doesn't contain / then we are already at the root
	//So we don't need to create a parent

	CharString substr = CharString_createNull();
	if (!CharString_cutAfterLastSensitive(path, '/', &substr))
		return true;

	//Try to add parent (returns true if already exists)

	const ArchiveEntry entry = (ArchiveEntry) {
		.path = substr,
		.type = EFileType_Folder
	};

	return !Archive_addInternal(archive, entry, true, alloc).genericError;
}

Error Archive_addInternal(Archive *archive, ArchiveEntry entry, Bool successIfExists, Allocator alloc) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0, "Archive_addInternal()::archive is required");

	//If folder already exists, we're done

	CharString resolved = CharString_createNull();
	ArchiveEntry out = (ArchiveEntry) { 0 };
	Error err = Error_none();
	CharString oldPath = CharString_createNull();

	if (Archive_getPath(*archive, entry.path, &out, NULL, NULL, alloc)) {

		if(out.type != entry.type || !successIfExists)
			err = Error_alreadyDefined(0, "Archive_addInternal()::entry.path already exists");

		goto clean;
	}

	//Resolve

	Bool isVirtual = false;
	gotoIfError(clean, File_resolve(entry.path, &isVirtual, 128, CharString_createNull(), alloc, &resolved))

	if (isVirtual)
		gotoIfError(clean, Error_unsupportedOperation(0, "Archive_addInternal()::entry.path was virtual (//)"))

	oldPath = entry.path;
	entry.path = resolved;

	//Try to find a parent or make one

	if(!Archive_createOrFindParent(archive, entry.path, alloc))
		gotoIfError(clean, Error_notFound(0, 0, "Archive_addInternal()::entry.path parent couldn't be created"))

	gotoIfError(clean, ListArchiveEntry_pushBack(&archive->entries, entry, alloc))
	resolved = CharString_createNull();

	CharString_free(&oldPath, alloc);

clean:

	if (CharString_length(oldPath))
		entry.path = oldPath;

	CharString_free(&resolved, alloc);
	return err;
}

Error Archive_addDirectory(Archive *archive, CharString path, Allocator alloc) {

	const ArchiveEntry entry = (ArchiveEntry) {
		.path = path,
		.type = EFileType_Folder
	};

	return Archive_addInternal(archive, entry, true, alloc);
}

Error Archive_addFile(Archive *archive, CharString path, Buffer data, Ns timestamp, Allocator alloc) {

	const ArchiveEntry entry = (ArchiveEntry) {
		.path = path,
		.type = EFileType_File,
		.data = data,
		.timestamp = timestamp
	};

	return Archive_addInternal(archive, entry, false, alloc);
}

Error Archive_removeInternal(Archive *archive, CharString path, Allocator alloc, EFileType type) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0, "Archive_removeInternal()::archive is required");

	ArchiveEntry entry = (ArchiveEntry) { 0 };
	U64 i = 0;
	CharString resolved = CharString_createNull();
	Error err;

	if(!Archive_getPath(*archive, path, &entry, &i, &resolved, alloc))
		gotoIfError(clean, Error_notFound(0, 1, "Archive_removeInternal()::path doesn't exist"))

	if(type != EFileType_Any && entry.type != type)
		gotoIfError(clean, Error_invalidOperation(0, "Archive_removeInternal()::type doesn't match file type"))

	//Remove children

	if (entry.type == EFileType_Folder) {

		//Get myFolder/*

		gotoIfError(clean, CharString_append(&resolved, '/', alloc))

		//Remove

		for (U64 j = archive->entries.length - 1; j != U64_MAX; --j) {

			const ArchiveEntry cai = archive->entries.ptr[i];

			if(!CharString_startsWithStringInsensitive(cai.path, resolved, 0))
				continue;

			//Free and remove from array

			Buffer_free(&entry.data, alloc);
			CharString_free(&entry.path, alloc);

			gotoIfError(clean, ListArchiveEntry_popLocation(&archive->entries, j, NULL))

			//Ensure our *self* id still makes sense

			if(j < i)
				--i;
		}
	}

	//Remove

	Buffer_free(&entry.data, alloc);
	CharString_free(&entry.path, alloc);

	gotoIfError(clean, ListArchiveEntry_popLocation(&archive->entries, i, NULL))

clean:
	CharString_free(&resolved, alloc);
	return err;
}

Error Archive_removeFile(Archive *archive, CharString path, Allocator alloc) {
	return Archive_removeInternal(archive, path, alloc, EFileType_File);
}

Error Archive_removeFolder(Archive *archive, CharString path, Allocator alloc) {
	return Archive_removeInternal(archive, path, alloc, EFileType_Folder);
}

Error Archive_remove(Archive *archive, CharString path, Allocator alloc) {
	return Archive_removeInternal(archive, path, alloc, EFileType_Any);
}

Error Archive_rename(Archive *archive, CharString loc, CharString newFileName, Allocator alloc) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0, "Archive_rename()::archive is required");

	CharString resolvedLoc = CharString_createNull();
	Error err;

	if (!CharString_isValidFileName(newFileName))
		return Error_invalidParameter(1, 0, "Archive_rename()::newFileName isn't a valid filename");

	U64 i = 0;
	if (!Archive_getPath(*archive, loc, NULL, &i, &resolvedLoc, alloc))
		return Error_notFound(0, 1, "Archive_rename()::loc couldn't be resolved to path");

	//Rename

	CharString *prevPath = &archive->entries.ptrNonConst[i].path;
	CharString subStr = CharString_createNull();

	CharString_cutAfterLastSensitive(*prevPath, '/', &subStr);
	prevPath->lenAndNullTerminated = CharString_length(subStr);

	gotoIfError(clean, CharString_appendString(prevPath, newFileName, alloc))

clean:
	CharString_free(&resolvedLoc, alloc);
	return err;
}

Error Archive_move(Archive *archive, CharString loc, CharString directoryName, Allocator alloc) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0, "Archive_move()::archive is required");

	CharString resolved = CharString_createNull();
	U64 i = 0;
	ArchiveEntry parent = (ArchiveEntry) { 0 };

	if (!Archive_getPath(*archive, loc, NULL, &i, NULL, alloc))
		return Error_notFound(0, 1, "Archive_move()::loc couldn't be resolved to path");

	if (!Archive_getPath(*archive, directoryName, &parent, NULL, &resolved, alloc))
		return Error_notFound(0, 2, "Archive_move()::directoryName couldn't be resolved to path");

	Error err = Error_none();

	if (parent.type != EFileType_Folder)
		gotoIfError(clean, Error_invalidOperation(0, "Archive_move()::directoryName should resolve to folder file"))

	CharString *filePath = &archive->entries.ptrNonConst[i].path;

	const U64 v = CharString_findLastSensitive(*filePath, '/', 0);

	if (v != U64_MAX)
		gotoIfError(clean, CharString_popFrontCount(filePath, v + 1))

	if (CharString_length(directoryName)) {
		gotoIfError(clean, CharString_insert(filePath, '/', 0, alloc))
		gotoIfError(clean, CharString_insertString(filePath, directoryName, 0, alloc))
	}

clean:
	CharString_free(&resolved, alloc);
	return err;
}

Error Archive_getInfo(Archive archive, CharString path, FileInfo *info, Allocator alloc) {

	if(!archive.entries.ptr || !info)
		return Error_nullPointer(!info ? 2 : 0, "Archive_getInfo()::archive and info are required");

	ArchiveEntry entry = (ArchiveEntry) { 0 };
	CharString resolved = CharString_createNull();

	if(!Archive_getPath(archive, path, &entry, NULL, &resolved, alloc))
		return Error_notFound(0, 1, "Archive_getInfo()::path couldn't resolve to path");

	*info = (FileInfo) {
		.access = Buffer_isConstRef(entry.data) ? EFileAccess_Read : EFileAccess_ReadWrite,
		.fileSize = Buffer_length(entry.data),
		.timestamp = entry.timestamp,
		.type = entry.type,
		.path = resolved
	};

	return Error_none();
}

U64 Archive_getIndex(Archive archive, CharString path, Allocator alloc) {
	U64 v = U64_MAX;
	Archive_getPath(archive, path, NULL, &v, NULL, alloc);
	return v;
}

Error Archive_updateFileData(Archive *archive, CharString path, Buffer data, Allocator alloc) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0, "Archive_updateFileData()::archive is required");

	ArchiveEntry entry = (ArchiveEntry) { 0 };
	U64 i = 0;

	if(!Archive_getPath(*archive, path, &entry, &i, NULL, alloc))
		return Error_notFound(0, 1, "Archive_updateFileData()::path couldn't resolve to path");

	Buffer_free(&entry.data, alloc);
	archive->entries.ptrNonConst[i].data = data;
	return Error_none();
}

Error Archive_getFileDataInternal(
	Archive archive,
	CharString path,
	Buffer *data,
	Allocator alloc,
	Bool isConst
) {

	if (!archive.entries.ptr || !data)
		return Error_nullPointer(!data ? 2 : 0, "Archive_getFileDataInternal()::archive and data are required");

	if(data->ptr)
		return Error_invalidParameter(2, 0, "Archive_getFileDataInternal()::data wasn't empty, might indicate memleak");

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc))
		return Error_notFound(0, 1, "Archive_getFileDataInternal()::path couldn't be resolved to path");

	if (entry.type != EFileType_File)
		return Error_invalidOperation(0, "Archive_getFileDataInternal()::entry.type isn't file, can't get data of folder");

	if(isConst)
		*data = Buffer_createRefConst(entry.data.ptr, Buffer_length(entry.data));

	else if(Buffer_isConstRef(entry.data))
		return Error_constData(1, 0, "Archive_getFileDataInternal()::entry.data should be writable");

	else *data = Buffer_createRef((U8*)entry.data.ptr, Buffer_length(entry.data));

	return Error_none();
}

Error Archive_getFileData(Archive archive, CharString path, Buffer *data, Allocator alloc) {
	return Archive_getFileDataInternal(archive, path, data, alloc, false);
}

Error Archive_getFileDataConst(Archive archive, CharString path, Buffer *data, Allocator alloc) {
	return Archive_getFileDataInternal(archive, path, data, alloc, true);
}

Error Archive_foreach(
	Archive archive,
	CharString loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Allocator alloc
) {

	if(!archive.entries.ptr || !callback)
		return Error_nullPointer(!callback ? 3 : 0, "Archive_foreach()::archive and callback are required");

	if(type > EFileType_Any)
		return Error_invalidEnum(5, (U64)type, (U64)EFileType_Any, "Archive_foreach()::type should be file, folder or any");

	CharString resolved = CharString_createNull();
	Bool isVirtual = false;

	Error err = File_resolve(loc, &isVirtual, 128, CharString_createNull(), alloc, &resolved);
	gotoIfError(clean, err)

	if(isVirtual)
		gotoIfError(clean, Error_invalidOperation(0, "Archive_foreach()::path can't start with start with // (virtual)"))

	//Append / (replace last \0)

	if(CharString_length(resolved))									//Ignore root
		gotoIfError(clean, CharString_append(&resolved, '/', alloc))

	const U64 baseSlash = isRecursive ? 0 : CharString_countAllSensitive(resolved, '/', 0);

	//TODO: Have a map where it's easy to find child files/folders.
	//		For now we'll have to loop over every file.
	//		Because our files are dynamic, so we don't want to reorder those every time.
	//		Maybe we should have Archive_optimize which is called before writing or if this functionality should be used.

	for (U64 i = 0; i < archive.entries.length; ++i) {

		const ArchiveEntry cai = archive.entries.ptr[i];

		if(type != EFileType_Any && type != cai.type)
			continue;

		if(!CharString_startsWithStringInsensitive(cai.path, resolved, 0))
			continue;

		//It contains at least one sub dir

		if(!isRecursive && baseSlash != CharString_countAllSensitive(cai.path, '/', 0))
			continue;

		FileInfo info = (FileInfo) {
			.path = cai.path,
			.type = cai.type
		};

		if (cai.type == EFileType_File) {
			info.access = Buffer_isConstRef(cai.data) ? EFileAccess_Read : EFileAccess_ReadWrite,
			info.fileSize = Buffer_length(cai.data);
			info.timestamp = cai.timestamp;
		}

		gotoIfError(clean, callback(info, userData))
	}

clean:
	CharString_free(&resolved, alloc);
	return err;
}

Error countFile(FileInfo info, U64 *res) {
	(void)info;
	++*res;
	return Error_none();
}

Error Archive_queryFileObjectCount(
	Archive archive,
	CharString loc,
	EFileType type,
	Bool isRecursive,
	U64 *res,
	Allocator alloc
) {

	if(!res)
		return Error_nullPointer(4, "Archive_queryFileObjectCount()::res is required");

	return Archive_foreach(
		archive,
		loc,
		(FileCallback) countFile,
		res,
		isRecursive,
		type,
		alloc
	);
}

Error Archive_queryFileEntryCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc
) {
	return Archive_queryFileObjectCount(archive, loc, EFileType_Any, isRecursive, res, alloc);
}

Error Archive_queryFileCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc
) {
	return Archive_queryFileObjectCount(archive, loc, EFileType_File, isRecursive, res, alloc);
}

Error Archive_queryFolderCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc
) {
	return Archive_queryFileObjectCount(archive, loc, EFileType_Folder, isRecursive, res, alloc);
}
