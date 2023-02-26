/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#include "types/archive.h"
#include "types/allocator.h"
#include "types/error.h"
#include "types/buffer.h"

Error Archive_create(Allocator alloc, Archive *archive) {

	if(!archive)
		return Error_nullPointer(1);

	if(archive->entries.ptr)
		return Error_invalidOperation(0);

	archive->entries = List_createEmpty(sizeof(ArchiveEntry));
	return List_reserve(&archive->entries, 100, alloc);
}

Bool Archive_free(Archive *archive, Allocator alloc) {

	if(!archive || !archive->entries.ptr)
		return true;

	for (U64 i = 0; i < archive->entries.length; ++i) {

		ArchiveEntry entry = ((ArchiveEntry*)archive->entries.ptr)[i];

		Buffer_free(&entry.data, alloc);
		String_free(&entry.path, alloc);
	}

	List_free(&archive->entries, alloc);
	*archive = (Archive) { 0 };
	return true;
}

Bool Archive_getPath(
	Archive archive, 
	String path, 
	ArchiveEntry *entryOut, 
	U64 *iPtr, 
	String *resolvedPathPtr, 
	Allocator alloc
) {

	if(!archive.entries.ptr || !String_length(path))
		return false;

	Bool isVirtual = false;
	String resolvedPath = String_createNull();
	
	Error err = File_resolve(path, &isVirtual, 128, String_createNull(), alloc, &resolvedPath);

	if(err.genericError)
		return false;

	if (isVirtual) {
		String_free(&resolvedPath, alloc);
		return false;
	}

	//TODO: Optimize this with a hashmap

	for(U64 i = 0; i < archive.entries.length; ++i)
		if (String_equalsString(
			((ArchiveEntry*)archive.entries.ptr)[i].path, resolvedPath, 
			EStringCase_Insensitive
		)) {

			if(entryOut && !String_length(entryOut->path))
				*entryOut = ((ArchiveEntry*)archive.entries.ptr)[i];

			if(iPtr)
				*iPtr = i;

			if(resolvedPathPtr && !resolvedPathPtr->ptr)
				*resolvedPathPtr = resolvedPath; 

			else String_free(&resolvedPath, alloc);

			return true;
		}

	String_free(&resolvedPath, alloc);
	return false;
}

Bool Archive_has(Archive archive, String path, Allocator alloc) {
	return Archive_getPath(archive, path, NULL, NULL, NULL, alloc);
}

Bool Archive_hasFile(Archive archive, String path, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc))
		return false;

	return entry.type == EFileType_File;
}

Bool Archive_hasFolder(Archive archive, String path, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc))
		return false;

	return entry.type == EFileType_Folder;
}

Error Archive_addInternal(Archive *archive, ArchiveEntry entry, Bool successIfExists, Allocator alloc);

Bool Archive_createOrFindParent(Archive *archive, String path, Allocator alloc) {

	//If it doesn't contain / then we are already at the root
	//So we don't need to create a parent

	String substr = String_createNull();
	if (!String_cutAfterLast(path, '/', EStringCase_Sensitive, &substr))
		return true;

	//Try to add parent (returns true if already exists)

	ArchiveEntry entry = (ArchiveEntry) {
		.path = substr,
		.type = EFileType_Folder
	};

	return !Archive_addInternal(archive, entry, true, alloc).genericError;
}

Error Archive_addInternal(Archive *archive, ArchiveEntry entry, Bool successIfExists, Allocator alloc) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0);

	//If folder already exists, we're done

	String resolved = String_createNull();
	ArchiveEntry out = (ArchiveEntry) { 0 };
	Error err = Error_none();
	String oldPath = String_createNull();

	if (Archive_getPath(*archive, entry.path, &out, NULL, NULL, alloc)) {

		if(out.type != entry.type || !successIfExists)
			err = Error_alreadyDefined(0);

		goto clean;
	}

	//Resolve

	Bool isVirtual = false;
	_gotoIfError(clean, File_resolve(entry.path, &isVirtual, 128, String_createNull(), alloc, &resolved));

	if (isVirtual)
		_gotoIfError(clean, Error_unsupportedOperation(0));

	oldPath = entry.path;
	entry.path = resolved;

	//Try to find a parent or make one

	if(!Archive_createOrFindParent(archive, entry.path, alloc))
		_gotoIfError(clean, Error_notFound(0, 0));

	_gotoIfError(clean, List_pushBack(&archive->entries, Buffer_createConstRef(&entry, sizeof(entry)), alloc));
	resolved = String_createNull();

	String_free(&oldPath, alloc);

clean:

	if (String_length(oldPath))
		entry.path = oldPath;

	String_free(&resolved, alloc);
	return err;
}

Error Archive_addDirectory(Archive *archive, String path, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) {
		.path = path,
		.type = EFileType_Folder
	};

	return Archive_addInternal(archive, entry, true, alloc);
}

Error Archive_addFile(Archive *archive, String path, Buffer data, Ns timestamp, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) {
		.path = path,
		.type = EFileType_File,
		.data = data,
		.timestamp = timestamp
	};

	return Archive_addInternal(archive, entry, false, alloc);
}

Error Archive_removeInternal(Archive *archive, String path, Allocator alloc, EFileType type) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0);

	ArchiveEntry entry = (ArchiveEntry) { 0 };
	U64 i = 0;
	String resolved = String_createNull();
	Error err = Error_none();

	if(!Archive_getPath(*archive, path, &entry, &i, &resolved, alloc))
		return Error_notFound(0, 1);

	if(type != EFileType_Any && entry.type != type)
		_gotoIfError(clean, Error_invalidOperation(0));

	//Remove children

	if (entry.type == EFileType_Folder) {

		//Get myFolder/*

		_gotoIfError(clean, String_append(&resolved, '/', alloc));

		//Remove

		for (U64 j = archive->entries.length - 1; j != U64_MAX; --j) {

			ArchiveEntry cai = ((ArchiveEntry*)archive->entries.ptr)[i];

			if(!String_startsWithString(cai.path, resolved, EStringCase_Insensitive))
				continue;

			//Free and remove from array

			Buffer_free(&entry.data, alloc);
			String_free(&entry.path, alloc);

			_gotoIfError(clean, List_popLocation(&archive->entries, j, Buffer_createNull()));

			//Ensure our *self* id still makes sense

			if(j < i)
				--i;
		}
	}

	//Remove

	Buffer_free(&entry.data, alloc);
	String_free(&entry.path, alloc);

	_gotoIfError(clean, List_popLocation(&archive->entries, i, Buffer_createNull()));
	
clean:
	String_free(&resolved, alloc);
	return err;
}

Error Archive_removeFile(Archive *archive, String path, Allocator alloc) {
	return Archive_removeInternal(archive, path, alloc, EFileType_File);
}

Error Archive_removeFolder(Archive *archive, String path, Allocator alloc) {
	return Archive_removeInternal(archive, path, alloc, EFileType_Folder);
}

Error Archive_remove(Archive *archive, String path, Allocator alloc) {
	return Archive_removeInternal(archive, path, alloc, EFileType_Any);
}

Error Archive_rename(
	Archive *archive, 
	String loc, 
	String newFileName,
	Allocator alloc
) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0);

	String resolvedLoc = String_createNull();
	Error err = Error_none();

	if (!String_isValidFileName(newFileName))
		return Error_invalidParameter(1, 0);

	U64 i = 0;
	if (!Archive_getPath(*archive, loc, NULL, &i, &resolvedLoc, alloc))
		return Error_notFound(0, 1);

	//Rename 

	String *prevPath = &((ArchiveEntry*)archive->entries.ptr)[i].path;
	String subStr = String_createNull();

	String_cutAfterLast(*prevPath, '/', EStringCase_Sensitive, &subStr);
	prevPath->len = String_length(subStr);

	_gotoIfError(clean, String_appendString(prevPath, newFileName, alloc));

clean:
	String_free(&resolvedLoc, alloc);
	return err;
}

Error Archive_move(
	Archive *archive,
	String loc,
	String directoryName,
	Allocator alloc
) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0);

	String resolved = String_createNull();
	U64 i = 0;
	ArchiveEntry parent = (ArchiveEntry) { 0 };

	if (!Archive_getPath(*archive, loc, NULL, &i, NULL, alloc))
		return Error_notFound(0, 1);

	if (!Archive_getPath(*archive, directoryName, &parent, NULL, &resolved, alloc))
		return Error_notFound(0, 2);

	Error err = Error_none();

	if (parent.type != EFileType_Folder)
		_gotoIfError(clean, Error_invalidOperation(0));

	String *filePath = &((ArchiveEntry*)archive->entries.ptr)[i].path;

	U64 v = String_findLast(*filePath, '/', EStringCase_Sensitive);

	if (v != U64_MAX)
		_gotoIfError(clean, String_popFrontCount(filePath, v + 1));

	if (String_length(directoryName)) {
		_gotoIfError(clean, String_insert(filePath, '/', 0, alloc));
		_gotoIfError(clean, String_insertString(filePath, directoryName, 0, alloc));
	}

clean:
	String_free(&resolved, alloc);
	return err;
}

Error Archive_getInfo(Archive archive, String path, FileInfo *info, Allocator alloc) {

	if(!archive.entries.ptr || !info)
		return Error_nullPointer(!info ? 2 : 0);

	ArchiveEntry entry = (ArchiveEntry) { 0 };
	String resolved = String_createNull();

	if(!Archive_getPath(archive, path, &entry, NULL, &resolved, alloc))
		return Error_notFound(0, 1);

	*info = (FileInfo) {
		.access = Buffer_isConstRef(entry.data) ? FileAccess_Read : FileAccess_ReadWrite,
		.fileSize = Buffer_length(entry.data),
		.timestamp = entry.timestamp,
		.type = entry.type,
		.path = resolved
	};

	return Error_none();
}

U64 Archive_getIndex(Archive archive, String path, Allocator alloc) {
	U64 v = U64_MAX;
	Archive_getPath(archive, path, NULL, &v, NULL, alloc);
	return v;
}

Error Archive_updateFileData(Archive *archive, String path, Buffer data, Allocator alloc) {

	if (!archive || !archive->entries.ptr)
		return Error_nullPointer(0);

	ArchiveEntry entry = (ArchiveEntry) { 0 };
	U64 i = 0;

	if(!Archive_getPath(*archive, path, &entry, &i, NULL, alloc))
		return Error_notFound(0, 1);

	Buffer_free(&entry.data, alloc);
	((ArchiveEntry*)archive->entries.ptr)[i].data = data;
	return Error_none();
}

Error Archive_getFileDataInternal(
	Archive archive,
	String path, 
	Buffer *data, 
	Allocator alloc, 
	Bool isConst
) {

	if (!archive.entries.ptr || !data)
		return Error_nullPointer(!data ? 2 : 0);

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc))
		return Error_notFound(0, 1);

	if (entry.type != EFileType_File)
		return Error_invalidOperation(0);

	if(isConst)
		*data = Buffer_createConstRef(entry.data.ptr, Buffer_length(entry.data));

	else if(Buffer_isConstRef(entry.data))
		return Error_constData(1, 0);

	else *data = Buffer_createRef((U8*)entry.data.ptr, Buffer_length(entry.data));

	return Error_none();
}

Error Archive_getFileData(Archive archive, String path, Buffer *data, Allocator alloc) {
	return Archive_getFileDataInternal(archive, path, data, alloc, false);
}

Error Archive_getFileDataConst(Archive archive, String path, Buffer *data, Allocator alloc) {
	return Archive_getFileDataInternal(archive, path, data, alloc, true);
}

Error Archive_foreach(
	Archive archive,
	String loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Allocator alloc
) {

	if(!archive.entries.ptr || !callback)
		return Error_nullPointer(!callback ? 3 : 0);

	if(type > EFileType_Any)
		return Error_invalidEnum(5, (U64)type, (U64)EFileType_Any);

	String resolved = String_createNull();
	Bool isVirtual = false;

	Error err = File_resolve(loc, &isVirtual, 128, String_createNull(), alloc, &resolved);
	_gotoIfError(clean, err);

	if(isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	//Append / (replace last \0)

	if(String_length(resolved))									//Ignore root
		_gotoIfError(clean, String_append(&resolved, '/', alloc));

	U64 baseSlash = isRecursive ? 0 : String_countAll(resolved, '/', EStringCase_Sensitive);

	//TODO: Have a map where it's easy to find child files/folders.
	//		For now we'll have to loop over every file.
	//		Because our files are dynamic, so we don't want to reorder those every time.
	//		Maybe we should have Archive_optimize which is called before writing or if this functionality should be used.

	for (U64 i = 0; i < archive.entries.length; ++i) {

		ArchiveEntry cai = ((ArchiveEntry*)archive.entries.ptr)[i];

		if(type != EFileType_Any && type != cai.type)
			continue;

		if(!String_startsWithString(cai.path, resolved, EStringCase_Insensitive))
			continue;

		//It contains at least one sub dir

		if(!isRecursive && baseSlash != String_countAll(cai.path, '/', EStringCase_Sensitive))
			continue;

		FileInfo info = (FileInfo) {
			.path = cai.path,
			.type = cai.type
		};

		if (cai.type == EFileType_File) {
			info.access = Buffer_isConstRef(cai.data) ? FileAccess_Read : FileAccess_ReadWrite,
			info.fileSize = Buffer_length(cai.data);
			info.timestamp = cai.timestamp;
		}

		_gotoIfError(clean, callback(info, userData));
	}

clean:
	String_free(&resolved, alloc);
	return err;
}

Error countFile(FileInfo info, U64 *res) {
	info;
	++*res;
	return Error_none();
}

Error Archive_queryFileObjectCount(
	Archive archive, 
	String loc, 
	EFileType type, 
	Bool isRecursive, 
	U64 *res,
	Allocator alloc
) {

	if(!res)
		return Error_nullPointer(4);

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

Error Archive_queryFileObjectCountAll(
	Archive archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
) {
	return Archive_queryFileObjectCount(archive, loc, EFileType_Any, isRecursive, res, alloc); 
}

Error Archive_queryFileCount(
	Archive archive,
	String loc,
	Bool isRecursive,
	U64 *res, 
	Allocator alloc
) { 
	return Archive_queryFileObjectCount(archive, loc, EFileType_File, isRecursive, res, alloc); 
}

Error Archive_queryFolderCount(
	Archive archive, 
	String loc, 
	Bool isRecursive, 
	U64 *res, 
	Allocator alloc
) { 
	return Archive_queryFileObjectCount(archive, loc, EFileType_Folder, isRecursive, res, alloc); 
}
