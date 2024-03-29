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

#include "types/error.h"
#include "types/buffer.h"
#include "formats/oiCA.h"
#include "platforms/file.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/archivex.h"
#include "platforms/ext/listx.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

Error File_foreach(CharString loc, FileCallback callback, void *userData, Bool isRecursive) {

	CharString resolved = CharString_createNull();
	CharString resolvedNoStar = CharString_createNull();
	CharString tmp = CharString_createNull();
	Error err = Error_none();
	HANDLE file = NULL;

	if(!callback) 
		_gotoIfError(clean, Error_nullPointer(1));

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_foreachVirtual(loc, callback, userData, isRecursive));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	if(isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	//Append /*

	_gotoIfError(clean, CharString_appendx(&resolved, '/'));

	_gotoIfError(clean, CharString_createCopyx(resolved, &resolvedNoStar));

	_gotoIfError(clean, CharString_appendx(&resolved, '*'));

	if(CharString_length(resolved) > MAX_PATH)
		_gotoIfError(clean, Error_outOfBounds(0, CharString_length(resolved), MAX_PATH));

	//Skip .

	WIN32_FIND_DATA dat;
	file = FindFirstFileA(resolved.ptr, &dat);

	if(file == INVALID_HANDLE_VALUE)
		_gotoIfError(clean, Error_notFound(0, 0));

	if(!FindNextFileA(file, &dat))
		_gotoIfError(clean, Error_notFound(0, 0));

	//Loop through real files (while instead of do while because we wanna skip ..)

	while(FindNextFileA(file, &dat)) {

		//Folders can also have timestamps, so parse timestamp here

		LARGE_INTEGER time;
		time.LowPart = dat.ftLastWriteTime.dwLowDateTime;
		time.HighPart = dat.ftLastWriteTime.dwHighDateTime;

		//Before 1970, unsupported. Default to 1970

		Ns timestamp = 0;
		static const U64 UNIX_START_WIN = (Ns)11644473600 * (1'000'000'000 / 100);	//ns to 100s of ns

		if ((U64)time.QuadPart >= UNIX_START_WIN)
			timestamp = (time.QuadPart - UNIX_START_WIN) * 100;	//Convert to Oxsomi time

		//Grab local file name

		_gotoIfError(clean, CharString_createCopyx(resolvedNoStar, &tmp));
		_gotoIfError(clean, CharString_appendStringx(&tmp, CharString_createConstRefAuto(dat.cFileName, MAX_PATH)));

		//Folder parsing

		if(dat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

			FileInfo info = (FileInfo) {
				.path = tmp,
				.timestamp = timestamp,
				.access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? EFileAccess_Read : EFileAccess_ReadWrite,
				.type = EFileType_Folder
			};

			_gotoIfError(clean, callback(info, userData));

			if(isRecursive)
				_gotoIfError(clean, File_foreach(info.path, callback, userData, true));

			CharString_freex(&tmp);
			continue;
		}

		LARGE_INTEGER size;
		size.LowPart = dat.nFileSizeLow;
		size.HighPart = dat.nFileSizeHigh;

		//File parsing

		FileInfo info = (FileInfo) {
			.path = tmp,
			.timestamp = timestamp,
			.access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? EFileAccess_Read : EFileAccess_ReadWrite,
			.type = EFileType_File,
			.fileSize = size.QuadPart
		};

		_gotoIfError(clean, callback(info, userData));
		CharString_freex(&tmp);
	}

	DWORD hr = GetLastError();

	if(hr != ERROR_NO_MORE_FILES)
		_gotoIfError(clean, Error_platformError(0, hr));

clean:

	if(file)
		FindClose(file);

	CharString_freex(&tmp);
	CharString_freex(&resolvedNoStar);
	CharString_freex(&resolved);
	return err;
}

//Virtual file support

typedef Error (*VirtualFileFunc)(void *userData, CharString resolved);

Error File_virtualOp(CharString loc, Ns maxTimeout, VirtualFileFunc f, void *userData, Bool isWrite) {

	maxTimeout;

	Bool isVirtual = false;
	CharString resolved = CharString_createNull();
	Error err = Error_none();

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 128, &resolved));

	if(!isVirtual)
		_gotoIfError(clean, Error_unsupportedOperation(0));

	CharString access = CharString_createConstRefCStr("//access/");
	CharString function = CharString_createConstRefCStr("//function/");

	if (CharString_startsWithString(loc, access, EStringCase_Insensitive)) {
		//TODO: Allow //access folder
		return Error_unimplemented(0);
	}

	if (CharString_startsWithString(loc, function, EStringCase_Insensitive)) {
		//TODO: Allow //function folder (user callbacks)
		return Error_unimplemented(1);
	}

	err = isWrite ? Error_constData(1, 0) : (!f ? Error_unimplemented(2) : f(userData, resolved));

clean:
	CharString_freex(&resolved);
	return err;
}

//These are write operations, we don't support them yet.

Error File_removeVirtual(CharString loc, Ns maxTimeout) {
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_addVirtual(CharString loc, EFileType type, Ns maxTimeout) { 
	type;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_renameVirtual(CharString loc, CharString newFileName, Ns maxTimeout) { 
	newFileName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_moveVirtual(CharString loc, CharString directoryName, Ns maxTimeout) { 
	directoryName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_writeVirtual(Buffer buf, CharString loc, Ns maxTimeout) {
	buf;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

//Read operations

Error File_resolveVirtual(CharString loc, CharString *subPath, VirtualSection **section) {

	CharString copy = CharString_createNull();
	CharString copy1 = CharString_createNull();
	CharString copy2 = CharString_createNull();
	Error err = Error_none();

	CharString_toLower(loc);
	_gotoIfError(clean, CharString_createCopyx(loc, &copy));
	_gotoIfError(clean, CharString_createCopyx(loc, &copy2));

	if(CharString_length(copy2))
		_gotoIfError(clean, CharString_appendx(&copy2, '/'))		//Don't append to root

	//Root is always present

	else goto clean;

	//Check sections

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *sectioni = (VirtualSection*)Platform_instance.virtualSections.ptr + i;

		//If folder is the same, we found a section.
		//This section won't return any subPath or section,
		//This will force it to be identified as a folder.

		if (CharString_equalsString(loc, sectioni->path, EStringCase_Insensitive))
			goto clean;

		//Parent folder.

		if(CharString_startsWithString(sectioni->path, copy2, EStringCase_Insensitive))
			goto clean;

		//Check if the section includes the referenced file/folder

		CharString_freex(&copy1);
		_gotoIfError(clean, CharString_createCopyx(sectioni->path, &copy1));
		_gotoIfError(clean, CharString_appendx(&copy1, '/'));

		if(CharString_startsWithString(copy, copy1, EStringCase_Insensitive)) {
			CharString_cut(copy, CharString_length(copy1), 0, subPath);
			*section = sectioni;
			goto clean;
		}
	}

	_gotoIfError(clean, Error_notFound(0, 0));

clean:

	CharString_freex(&copy);

	if (CharString_length(*subPath) && !err.genericError) {
		CharString_createCopyx(*subPath, &copy);
		*subPath = copy;
		copy = CharString_createNull();
	}

	CharString_freex(&copy1);
	CharString_freex(&copy2);
	return err;
}

Error File_readVirtualInternal(Buffer *output, CharString loc) {

	CharString subPath = CharString_createNull();
	VirtualSection *section = NULL;
	Error err = Error_none();

	_gotoIfError(clean, File_resolveVirtual(loc, &subPath, &section));

	if(!section)
		_gotoIfError(clean, Error_invalidOperation(0));

	_gotoIfError(clean, Archive_getFileDataConstx(section->loadedData, subPath, output));

clean:
	CharString_freex(&subPath);
	return err;
}

Error File_readVirtual(CharString loc, Buffer *output, Ns maxTimeout) { 

	if(!output)
		return Error_nullPointer(1);

	return File_virtualOp(
		loc, maxTimeout, 
		(VirtualFileFunc) File_readVirtualInternal, 
		output, 
		false
	);
}

Error File_getInfoVirtualInternal(FileInfo *info, CharString loc) {

	CharString subPath = CharString_createNull();
	VirtualSection *section = NULL;
	Error err = Error_none();

	_gotoIfError(clean, File_resolveVirtual(loc, &subPath, &section));

	if(!section) {	//Parent dir

		if(!CharString_length(loc))
			loc = CharString_createConstRefCStr(".");

		CharString copy = CharString_createNull();
		_gotoIfError(clean, CharString_createCopyx(loc, &copy));

		*info = (FileInfo) {
			.path = copy,
			.type = EFileType_Folder,
			.access = EFileAccess_Read
		};
	}

	else {

		_gotoIfError(clean, Archive_getInfox(section->loadedData, subPath, info));

		CharString tmp = CharString_createNull();
		CharString_cut(loc, 0, CharString_length(loc) - CharString_length(subPath), &tmp);

		_gotoIfError(clean, CharString_insertStringx(&info->path, tmp, 0));
	}

	_gotoIfError(clean, CharString_insertStringx(&info->path, CharString_createConstRefCStr("//"), 0));

clean:

	if(err.genericError)
		FileInfo_freex(info);

	CharString_freex(&subPath);
	return err;
}

Error File_getInfoVirtual(CharString loc, FileInfo *info) { 

	if(!info)
		return Error_nullPointer(1);

	if(info->access)
		return Error_invalidOperation(0);

	return File_virtualOp(
		loc, 1 * SECOND, 
		(VirtualFileFunc) File_getInfoVirtualInternal, 
		info, 
		false
	);
}

typedef struct ForeachFile {
	FileCallback callback;
	void *userData;
	Bool isRecursive;
	CharString currentPath;
} ForeachFile;

Error File_virtualCallback(FileInfo info, ForeachFile *userData) {

	CharString fullPath = CharString_createNull();
	Error err = Error_none();

	_gotoIfError(clean, CharString_createCopyx(userData->currentPath, &fullPath));
	_gotoIfError(clean, CharString_appendStringx(&fullPath, info.path));
	_gotoIfError(clean, CharString_insertStringx(&fullPath, CharString_createConstRefCStr("//"), 0));

	info.path = fullPath;

	_gotoIfError(clean, userData->callback(info, userData->userData));

clean:
	CharString_freex(&fullPath);
	return err;
}

Error File_foreachVirtualInternal(ForeachFile *userData, CharString resolved) {

	Error err = Error_none();
	CharString copy = CharString_createNull();
	CharString copy1 = CharString_createNull();
	CharString copy2 = CharString_createNull();
	CharString copy3 = CharString_createNull();
	CharString root = CharString_createConstRefCStr(".");
	List visited = List_createEmpty(sizeof(CharString));

	CharString_toLower(resolved);
	_gotoIfError(clean, CharString_createCopyx(resolved, &copy));

	if(CharString_length(copy))
		_gotoIfError(clean, CharString_appendx(&copy, '/'));		//Don't append to root

	U64 baseCount = CharString_countAll(copy, '/', EStringCase_Sensitive);

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *section = (VirtualSection*)Platform_instance.virtualSections.ptr + i;

		CharString_freex(&copy1);
		CharString_freex(&copy2);

		_gotoIfError(clean, CharString_createCopyx(section->path, &copy1));
		CharString_toLower(copy1);

		_gotoIfError(clean, CharString_createCopyx(copy1, &copy2));
		_gotoIfError(clean, CharString_appendx(&copy2, '/'));

		if(
			!CharString_startsWithString(copy1, copy, EStringCase_Sensitive) &&
			!CharString_equalsString(copy1, resolved, EStringCase_Sensitive) &&
			!CharString_startsWithString(copy, copy2, EStringCase_Sensitive)
		)
			continue;

		//Root directories don't exist, so we have to pretend they do

		if (!CharString_length(resolved)) {

			CharString parent = CharString_createNull();
			if(!CharString_cutAfterFirst(copy1, '/', EStringCase_Sensitive, &parent))
				_gotoIfError(clean, Error_invalidState(0));

			Bool contains = false;

			for(U64 j = 0; j < visited.length; ++j)
				if (CharString_equalsString(parent, ((CharString*)(visited.ptr))[j], EStringCase_Sensitive)) {
					contains = true;
					break;
				}

			if (!contains) {

				//Avoid duplicates

				_gotoIfError(clean, List_pushBackx(&visited, Buffer_createConstRef(&parent, sizeof(parent))));

				CharString_freex(&copy3);
				_gotoIfError(clean, CharString_createCopyx(parent, &copy3));
				_gotoIfError(clean, CharString_insertStringx(&copy3, CharString_createConstRefCStr("//"), 0));

				FileInfo info = (FileInfo) {
					.path = copy3,
					.type = EFileType_Folder,
					.access = EFileAccess_Read
				};

				_gotoIfError(clean, userData->callback(info, userData->userData));
			}
		}

		//All folders 
		
		if (
			(!userData->isRecursive && baseCount == CharString_countAll(section->path, '/', EStringCase_Sensitive)) ||
			(userData->isRecursive && baseCount <= CharString_countAll(section->path, '/', EStringCase_Sensitive))
		) {

			CharString_freex(&copy3);
			_gotoIfError(clean, CharString_createCopyx(copy1, &copy3));
			_gotoIfError(clean, CharString_insertStringx(&copy3, CharString_createConstRefCStr("//"), 0));

			FileInfo info = (FileInfo) {
				.path = copy3,
				.type = EFileType_Folder,
				.access = EFileAccess_Read
			};

			_gotoIfError(clean, userData->callback(info, userData->userData));
		}

		//We can only loop through the folders if they're loaded. Otherwise we consider them as empty.

		if(section->loaded) {

			userData->currentPath = copy2;

			if(userData->isRecursive || baseCount >= 2) {

				CharString child = CharString_createNull();

				if(baseCount > 2 && !CharString_cut(resolved, CharString_length(copy2), 0, &child))
					_gotoIfError(clean, Error_invalidState(1))

				if(!CharString_length(child))
					child = root;

				_gotoIfError(clean, Archive_foreachx(
					section->loadedData, 
					child, 
					(FileCallback) File_virtualCallback, userData, 
					userData->isRecursive,
					EFileType_Any
				));
			}
		}
	}

clean:
	List_freex(&visited);
	CharString_freex(&copy);
	CharString_freex(&copy1);
	CharString_freex(&copy2);
	CharString_freex(&copy3);
	return Error_unimplemented(0); 
}

Error File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive) { 

	if(!callback)
		return Error_nullPointer(1);

	ForeachFile foreachFile = (ForeachFile) { 
		.callback = callback, 
		.userData = userData, 
		.isRecursive = isRecursive
	};

	return File_virtualOp(
		loc, 1 * SECOND, 
		(VirtualFileFunc) File_foreachVirtualInternal, 
		&foreachFile, 
		false
	);
}

typedef struct FileCounter {
	EFileType type;
	Bool useType;
	U64 counter;
} FileCounter;

Error countFileType(FileInfo info, FileCounter *counter) {

	if (!counter->useType) {
		++counter->counter;
		return Error_none();
	}

	if(info.type == counter->type)
		++counter->counter;

	return Error_none();
}

Error File_queryFileObjectCountVirtual(CharString loc, EFileType type, Bool isRecursive, U64 *res) { 

	Error err = Error_none();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(3));

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	_gotoIfError(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive));
	*res = counter.counter;

clean:
	return err;
}

Error File_queryFileObjectCountAllVirtual(CharString loc, Bool isRecursive, U64 *res) { 

	Error err = Error_none();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(2));

	FileCounter counter = (FileCounter) { 0 };
	_gotoIfError(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive));
	*res = counter.counter;

clean:
	return err;
}

typedef struct FileLoadVirtual {
	Bool doLoad;
	const U32 *encryptionKey;
} FileLoadVirtual;

inline Error File_loadVirtualInternal(FileLoadVirtual *userData, CharString loc) {

	CharString isChild = CharString_createNull();
	Error err = Error_none();

	_gotoIfError(clean, CharString_createCopyx(loc, &isChild));

	if(CharString_length(isChild))
		_gotoIfError(clean, CharString_appendx(&isChild, '/'));		//Don't append to root

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *section = (VirtualSection*)Platform_instance.virtualSections.ptr + i;

		if(
			!CharString_equalsString(loc, section->path, EStringCase_Insensitive) &&
			!CharString_startsWithString(section->path, isChild, EStringCase_Insensitive)
		)
			continue;

		//Load

		if (userData->doLoad) {

			if (!section->loaded) {

				HRSRC data = FindResourceA(NULL, section->path.ptr, RT_RCDATA);
				HGLOBAL handle = NULL;
				CAFile file = (CAFile) { 0 };
				Buffer copy = Buffer_createNull();

				if(!data)
					_gotoIfError(clean0, Error_notFound(0, 1));

				U32 size = (U32) SizeofResource(NULL, data);
				handle = LoadResource(NULL, data);

				if(!handle)
					_gotoIfError(clean0, Error_notFound(3, 1));

				const U8 *dat = (const U8*) LockResource(handle);
				_gotoIfError(clean0, Buffer_createCopyx(Buffer_createConstRef(dat, size), &copy));

				_gotoIfError(clean0, CAFile_readx(copy, userData->encryptionKey, &file));

				section->loadedData = file.archive;
				file.archive = (Archive) { 0 };

				section->loaded = true;

			clean0:

				CAFile_freex(&file);
				Buffer_freex(&copy);

				if (handle) {
					UnlockResource(handle);
					FreeResource(handle);
				}
			}
		}

		//Otherwise we want to use error to determine if it's present or not

		else err = section->loaded ? Error_none() : Error_notFound(1, 1);

		goto clean;
	}

	err = Error_notFound(2, 1);

clean:
	CharString_freex(&isChild);
	return err;
}

Error File_unloadVirtualInternal(void *userData, CharString loc) {

	userData;

	CharString isChild = CharString_createNull();
	Error err = Error_none();

	_gotoIfError(clean, CharString_createCopyx(loc, &isChild));

	if(CharString_length(isChild))
		_gotoIfError(clean, CharString_appendx(&isChild, '/'));		//Don't append to root

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *section = (VirtualSection*)Platform_instance.virtualSections.ptr + i;

		if(
			!CharString_equalsString(loc, section->path, EStringCase_Insensitive) &&
			!CharString_startsWithString(section->path, isChild, EStringCase_Insensitive)
		)
			continue;

		if(section->loaded)
			Archive_freex(&section->loadedData);
	}

clean:
	CharString_freex(&isChild);
	return err;
}

Bool File_isVirtualLoaded(CharString loc) {
	FileLoadVirtual virt = (FileLoadVirtual) { 0 };
	return !File_loadVirtualInternal(&virt, loc).genericError;
}

Error File_loadVirtual(CharString loc, const U32 encryptionKey[8]) {
	FileLoadVirtual virt = (FileLoadVirtual) { .doLoad = true, .encryptionKey = encryptionKey };
	return File_virtualOp(loc, 1 * SECOND, (VirtualFileFunc) File_loadVirtualInternal, &virt, false);
}

Error File_unloadVirtual(CharString loc) {
	return File_virtualOp(loc, 1 * SECOND, File_unloadVirtualInternal, NULL, false);
}

