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

Error File_foreach(String loc, FileCallback callback, void *userData, Bool isRecursive) {

	String resolved = String_createNull();
	String resolvedNoStar = String_createNull();
	String tmp = String_createNull();
	Error err = Error_none();
	HANDLE file = NULL;

	if(!callback) 
		_gotoIfError(clean, Error_nullPointer(1));

	if(!String_isValidFilePath(loc))
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

	_gotoIfError(clean, String_appendx(&resolved, '/'));

	_gotoIfError(clean, String_createCopyx(resolved, &resolvedNoStar));

	_gotoIfError(clean, String_appendx(&resolved, '*'));

	if(String_length(resolved) > MAX_PATH)
		_gotoIfError(clean, Error_outOfBounds(0, String_length(resolved), MAX_PATH));

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

		_gotoIfError(clean, String_createCopyx(resolvedNoStar, &tmp));
		_gotoIfError(clean, String_appendStringx(&tmp, String_createConstRefAuto(dat.cFileName, MAX_PATH)));

		//Folder parsing

		if(dat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

			FileInfo info = (FileInfo) {
				.path = tmp,
				.timestamp = timestamp,
				.access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? FileAccess_Read : FileAccess_ReadWrite,
				.type = EFileType_Folder
			};

			_gotoIfError(clean, callback(info, userData));

			if(isRecursive)
				_gotoIfError(clean, File_foreach(info.path, callback, userData, true));

			String_freex(&tmp);
			continue;
		}

		LARGE_INTEGER size;
		size.LowPart = dat.nFileSizeLow;
		size.HighPart = dat.nFileSizeHigh;

		//File parsing

		FileInfo info = (FileInfo) {
			.path = tmp,
			.timestamp = timestamp,
			.access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? FileAccess_Read : FileAccess_ReadWrite,
			.type = EFileType_File,
			.fileSize = size.QuadPart
		};

		_gotoIfError(clean, callback(info, userData));
		String_freex(&tmp);
	}

	DWORD hr = GetLastError();

	if(hr != ERROR_NO_MORE_FILES)
		_gotoIfError(clean, Error_platformError(0, hr));

clean:

	if(file)
		FindClose(file);

	String_freex(&tmp);
	String_freex(&resolvedNoStar);
	String_freex(&resolved);
	return err;
}

//Virtual file support

typedef Error (*VirtualFileFunc)(void *userData, String resolved);

Error File_virtualOp(String loc, Ns maxTimeout, VirtualFileFunc f, void *userData, Bool isWrite) {

	maxTimeout;

	Bool isVirtual = false;
	String resolved = String_createNull();
	Error err = Error_none();

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 128, &resolved));

	if(!isVirtual)
		_gotoIfError(clean, Error_unsupportedOperation(0));

	String access = String_createConstRefUnsafe("//access/");
	String function = String_createConstRefUnsafe("//function/");

	if (String_startsWithString(loc, access, EStringCase_Insensitive)) {
		//TODO: Allow //access folder
		return Error_unimplemented(0);
	}

	if (String_startsWithString(loc, function, EStringCase_Insensitive)) {
		//TODO: Allow //function folder (user callbacks)
		return Error_unimplemented(1);
	}

	err = isWrite ? Error_constData(1, 0) : (!f ? Error_unimplemented(2) : f(userData, resolved));

clean:
	String_freex(&resolved);
	return err;
}

//These are write operations, we don't support them yet.

Error File_removeVirtual(String loc, Ns maxTimeout) {
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_addVirtual(String loc, EFileType type, Ns maxTimeout) { 
	type;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_renameVirtual(String loc, String newFileName, Ns maxTimeout) { 
	newFileName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_moveVirtual(String loc, String directoryName, Ns maxTimeout) { 
	directoryName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_writeVirtual(Buffer buf, String loc, Ns maxTimeout) {
	buf;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

//Read operations

Error File_readVirtual(String loc, Buffer *output, Ns maxTimeout) { 
	maxTimeout; loc; output; 
	return Error_unimplemented(0); 
}

Error File_getInfoVirtual(String loc, FileInfo *info) { 
	loc; info; 
	return Error_unimplemented(0); 
}

typedef struct ForeachFile {
	FileCallback callback;
	void *userData;
	Bool isRecursive;
	String currentPath;
} ForeachFile;

Error File_virtualCallback(FileInfo info, ForeachFile *userData) {

	String fullPath = String_createNull();
	Error err = Error_none();

	_gotoIfError(clean, String_createCopyx(userData->currentPath, &fullPath));
	_gotoIfError(clean, String_appendStringx(&fullPath, info.path));

	info.path = fullPath;

	_gotoIfError(clean, userData->callback(info, userData->userData));

clean:
	String_freex(&fullPath);
	return err;
}

Error File_foreachVirtualInternal(ForeachFile *userData, String resolved) {

	Error err = Error_none();
	String copy = String_createNull();
	String copy1 = String_createNull();
	String copy2 = String_createNull();
	String root = String_createConstRefUnsafe(".");
	List visited = List_createEmpty(sizeof(String));

	String_toLower(resolved);
	_gotoIfError(clean, String_createCopyx(resolved, &copy));

	if(String_length(copy))
		_gotoIfError(clean, String_appendx(&copy, '/'));		//Don't append to root

	U64 baseCount = String_countAll(copy, '/', EStringCase_Sensitive);

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *section = (VirtualSection*)Platform_instance.virtualSections.ptr + i;

		String_freex(&copy1);
		String_freex(&copy2);

		_gotoIfError(clean, String_createCopyx(section->path, &copy1));
		String_toLower(copy1);

		_gotoIfError(clean, String_createCopyx(copy1, &copy2));
		_gotoIfError(clean, String_appendx(&copy2, '/'));

		if(
			!String_startsWithString(copy1, copy, EStringCase_Sensitive) &&
			!String_equalsString(copy1, resolved, EStringCase_Sensitive) &&
			!String_startsWithString(copy, copy2, EStringCase_Sensitive)
		)
			continue;

		//Root directories don't exist, so we have to pretend they do

		if (!String_length(resolved)) {

			String parent = String_createNull();
			if(!String_cutAfterFirst(copy1, '/', EStringCase_Sensitive, &parent))
				_gotoIfError(clean, Error_invalidState(0));

			Bool contains = false;

			for(U64 j = 0; j < visited.length; ++j)
				if (String_equalsString(parent, ((String*)(visited.ptr))[j], EStringCase_Sensitive)) {
					contains = true;
					break;
				}

			if (!contains) {

				//Avoid duplicates

				_gotoIfError(clean, List_pushBackx(&visited, Buffer_createConstRef(&parent, sizeof(parent))));

				FileInfo info = (FileInfo) {
					.path = parent,
					.type = EFileType_Folder,
					.access = FileAccess_Read
				};

				_gotoIfError(clean, userData->callback(info, userData->userData));
			}
		}

		//All folders 
		
		if (
			(!userData->isRecursive && baseCount == String_countAll(section->path, '/', EStringCase_Sensitive)) ||
			(userData->isRecursive && baseCount <= String_countAll(section->path, '/', EStringCase_Sensitive))
		) {

			FileInfo info = (FileInfo) {
				.path = copy1,
				.type = EFileType_Folder,
				.access = FileAccess_Read
			};

			_gotoIfError(clean, userData->callback(info, userData->userData));
		}

		//We can only loop through the folders if they're loaded. Otherwise we consider them as empty.

		if(section->loaded) {

			userData->currentPath = copy2;

			if(userData->isRecursive || baseCount >= 2) {

				String child = String_createNull();

				if(baseCount > 2 && !String_cut(resolved, String_length(copy2), 0, &child))
					_gotoIfError(clean, Error_invalidState(1))

				if(!String_length(child))
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
	String_freex(&copy);
	String_freex(&copy1);
	String_freex(&copy2);
	return Error_unimplemented(0); 
}

Error File_foreachVirtual(String loc, FileCallback callback, void *userData, Bool isRecursive) { 

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

Error File_queryFileObjectCountVirtual(String loc, EFileType type, Bool isRecursive, U64 *res) { 

	Error err = Error_none();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(3));

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	_gotoIfError(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive));
	*res = counter.counter;

clean:
	return err;
}

Error File_queryFileObjectCountAllVirtual(String loc, Bool isRecursive, U64 *res) { 

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

inline Error File_loadVirtualInternal(FileLoadVirtual *userData, String loc) {

	String isChild = String_createNull();
	Error err = Error_none();

	_gotoIfError(clean, String_createCopyx(loc, &isChild));

	if(String_length(isChild))
		_gotoIfError(clean, String_appendx(&isChild, '/'));		//Don't append to root

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *section = (VirtualSection*)Platform_instance.virtualSections.ptr + i;

		if(
			!String_equalsString(loc, section->path, EStringCase_Insensitive) &&
			!String_startsWithString(section->path, isChild, EStringCase_Insensitive)
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
	String_freex(&isChild);
	return err;
}

Error File_unloadVirtualInternal(void *userData, String loc) {

	userData;

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		//TODO: Parenting

		VirtualSection *section = (VirtualSection*)Platform_instance.virtualSections.ptr + i;

		if(!String_equalsString(loc, section->path, EStringCase_Insensitive))
			continue;

		if(section->loaded)
			Archive_freex(&section->loadedData);
	}

	return Error_none();
}

Bool File_isVirtualLoaded(String loc) {
	FileLoadVirtual virt = (FileLoadVirtual) { 0 };
	return !File_loadVirtualInternal(&virt, loc).genericError;
}

Error File_loadVirtual(String loc, const U32 encryptionKey[8]) {
	FileLoadVirtual virt = (FileLoadVirtual) { .doLoad = true, .encryptionKey = encryptionKey };
	return File_virtualOp(loc, 1 * SECOND, (VirtualFileFunc) File_loadVirtualInternal, &virt, false);
}

Error File_unloadVirtual(String loc) {
	return File_virtualOp(loc, 1 * SECOND, File_unloadVirtualInternal, NULL, false);
}

