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

#include "platforms/thread.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/file.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"

#include <stdio.h>
#include <sys/stat.h>

//This is fine in file.c instead of wfile.c because unix + windows systems all share very similar ideas about filesystems
//But windows is a bit stricter in some parts (like the characters you can include) and has some weird quirks
//Still, it's common enough here to not require separation

#ifndef _WIN32
	#define _ftelli64 ftell
	#define removeFolder remove
#else

	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <direct.h>
	#include <fileapi.h>

	#define stat _stat64
	#define S_ISREG(x) (x & _S_IFREG)
	#define S_ISDIR(x) (x & _S_IFDIR)

	#define mkdir _mkdir
	#define removeFolder(ptr) (RemoveDirectoryA(ptr) ? 0 : -1)

#endif

//Both Linux and Windows require folder to be empty before removing.
//So we do it manually.

int removeFileOrFolder(const C8 *ptr);

Error recurseDelete(FileInfo info, void *unused) {

	unused;

	if (removeFileOrFolder(info.path.ptr))
		return Error_invalidOperation(0);

	return Error_none();
}

int removeFileOrFolder(const C8 *ptr) {

	struct stat inf = (struct stat) { 0 };
	int r = stat(ptr, &inf);

	if(r)
		return r;

	if (S_ISDIR(inf.st_mode)) {

		//Delete every file, because RemoveDirecotryA requires it to be empty
		//We'll handle recursion ourselves

		Error err = File_foreach(CharString_createConstRefCStr(ptr), recurseDelete, NULL, false);

		if(err.genericError)
			return -1;

		//Now that our folder is empty, we can finally delete it

		return removeFolder(ptr);
	}

	return remove(ptr);
}

//

Bool FileInfo_freex(FileInfo *fileInfo) {
	return FileInfo_free(fileInfo, Platform_instance.alloc);
}

Error File_resolvex(CharString loc, Bool *isVirtual, U64 maxFilePathLimit, CharString *result) {
	return File_resolve(
		loc, isVirtual, 
		maxFilePathLimit, 
		Platform_instance.workingDirectory, Platform_instance.alloc, 
		result
	);
}

Error File_getInfo(CharString loc, FileInfo *info) {

	CharString resolved = CharString_createNull();
	Error err = Error_none();

	if(!info) 
		_gotoIfError(clean, Error_nullPointer(0));

	if(info->path.ptr) 
		_gotoIfError(clean, Error_invalidOperation(0));

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_getInfoVirtual(loc, info));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	struct stat inf = (struct stat) { 0 };

	if (stat(resolved.ptr, &inf))
		_gotoIfError(clean, Error_notFound(0, 0));

	if (!S_ISDIR(inf.st_mode) && !S_ISREG(inf.st_mode))
		_gotoIfError(clean, Error_invalidOperation(2));

	if (!(inf.st_mode & (S_IREAD | S_IWRITE)))
		_gotoIfError(clean, Error_unauthorized(0));

	*info = (FileInfo) {

		.timestamp = (Ns)inf.st_mtime * SECOND,
		.path = resolved,

		.type = S_ISDIR(inf.st_mode) ? EFileType_Folder : EFileType_File,
		.fileSize = (U64) inf.st_size,

		.access = 
			(inf.st_mode & S_IWRITE ? EFileAccess_Write : EFileAccess_None) | 
			(inf.st_mode & S_IREAD  ? EFileAccess_Read  : EFileAccess_None)
	};

	return Error_none();

clean:
	CharString_freex(&resolved);
	return err;
}

Bool File_hasFile(CharString loc) { return File_hasType(loc, EFileType_File); }
Bool File_hasFolder(CharString loc) { return File_hasType(loc, EFileType_Folder); }

Error File_queryFileCount(CharString loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, EFileType_File, isRecursive, res); 
}

Error File_queryFolderCount(CharString loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, EFileType_Folder, isRecursive, res); 
}

typedef struct FileCounter {
	EFileType type;
	Bool useType;
	U64 counter;
} FileCounter;

Error countFileTypeVirtual(FileInfo info, FileCounter *counter) {

	if (!counter->useType) {
		++counter->counter;
		return Error_none();
	}

	if(info.type == counter->type)
		++counter->counter;

	return Error_none();
}

Error File_queryFileObjectCount(CharString loc, EFileType type, Bool isRecursive, U64 *res) {

	Error err = Error_none();
	CharString resolved = CharString_createNull();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(3));

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	if(!CharString_isValidFilePath(loc)) 
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_queryFileObjectCountVirtual(loc, type, isRecursive, res));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	//Normal counter for local files

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	_gotoIfError(clean, File_foreach(loc, (FileCallback) countFileTypeVirtual, &counter, isRecursive));
	*res = counter.counter;

clean:
	CharString_freex(&resolved);
	return err;
}

Error File_queryFileObjectCountAll(CharString loc, Bool isRecursive, U64 *res) {

	CharString resolved = CharString_createNull();
	Error err = Error_none();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(2));

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_queryFileObjectCountAllVirtual(loc, isRecursive, res));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	//Normal counter for local files

	FileCounter counter = (FileCounter) { 0 };
	_gotoIfError(clean, File_foreach(loc, (FileCallback) countFileTypeVirtual, &counter, isRecursive));
	*res = counter.counter;

clean:
	CharString_freex(&resolved);
	return err;
}

Error File_add(CharString loc, EFileType type, Ns maxTimeout) {

	CharString resolved = CharString_createNull();
	CharStringList str = (CharStringList) { 0 };
	FileInfo info = (FileInfo) { 0 };
	Error err = Error_none();

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_addVirtual(loc, type, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	err = File_getInfo(resolved, &info);

	if(err.genericError && err.genericError != EGenericError_NotFound)
		_gotoIfError(clean, err);

	if (!err.genericError) {
		CharString_freex(&resolved);
		FileInfo_freex(&info);
		return Error_none();
	}

	//Already exists

	if(!err.genericError)
		_gotoIfError(clean, info.type != type ? Error_alreadyDefined(0) : Error_none());

	FileInfo_freex(&info);

	//Check parent directories until none left

	if(CharString_contains(resolved, '/', EStringCase_Sensitive)) {

		if(!CharString_eraseFirstString(&resolved, Platform_instance.workingDirectory, EStringCase_Insensitive))
			_gotoIfError(clean, Error_unauthorized(0));

		_gotoIfError(clean, CharString_splitx(resolved, '/', EStringCase_Sensitive, &str));

		for (U64 i = 0; i < str.length - 1; ++i) {

			CharString parent = CharString_createRefSized((C8*)resolved.ptr, CharString_end(str.ptr[i]) - resolved.ptr, false);

			err = File_getInfo(parent, &info);

			if(err.genericError && err.genericError != EGenericError_NotFound)
				_gotoIfError(clean, err);

			if(info.type != EFileType_Folder)
				_gotoIfError(clean, Error_invalidOperation(2));

			FileInfo_freex(&info);

			if(!err.genericError)		//Already defined, continue to child
				continue;

			//Mkdir requires null terminated string
			//So we hack force in a null terminator

			C8 prev = '\0';

			if (CharString_end(parent) != CharString_end(resolved)) {
				prev = CharString_getAt(resolved, CharString_length(parent) + 1);
				*(C8*)(resolved.ptr + CharString_length(parent) + 1) = '\0';
			}

			//Make parent

			if (mkdir(parent.ptr))
				_gotoIfError(clean, Error_stderr(0));

			//Reset character that was replaced with \0

			if(prev)
				CharString_setAt(resolved, CharString_length(parent) + 1, prev);
		}

		CharStringList_freex(&str);
	}

	//Create folder

	if (type == EFileType_Folder && mkdir(resolved.ptr))
		_gotoIfError(clean, Error_stderr(1));

	//Create file

	if(type == EFileType_File)
		_gotoIfError(clean, File_write(Buffer_createNull(), resolved, maxTimeout));

clean:
	FileInfo_freex(&info);
	CharString_freex(&resolved);
	CharStringList_freex(&str);
	return err;
}

Error File_remove(CharString loc, Ns maxTimeout) {

	CharString resolved = CharString_createNull();
	Error err = Error_none();

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_removeVirtual(resolved, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	int res = removeFileOrFolder(resolved.ptr);

	while (res && maxTimeout) {

		Thread_sleep(maxTimeoutTry);

		res = removeFileOrFolder(resolved.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if (res)
		_gotoIfError(clean, Error_stderr(0));

clean:
	CharString_freex(&resolved);
	return err;
}

Bool File_has(CharString loc) {
	FileInfo info = (FileInfo) { 0 };
	Error err = File_getInfo(loc, &info);
	FileInfo_freex(&info);
	return !err.genericError;
}

Bool File_hasType(CharString loc, EFileType type) {
	FileInfo info = (FileInfo) { 0 };
	Error err = File_getInfo(loc, &info);
	Bool sameType = info.type == type;
	FileInfo_freex(&info);
	return !err.genericError && sameType;
}

Error File_rename(CharString loc, CharString newFileName, Ns maxTimeout) {

	CharString resolved = CharString_createNull();
	Error err = Error_none();
	FileInfo info = (FileInfo) { 0 };

	if(!CharString_isValidFilePath(newFileName))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	if(!CharString_isValidFileName(newFileName))
		_gotoIfError(clean, Error_invalidParameter(1, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_renameVirtual(loc, newFileName, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	//Check if file exists

	Bool fileExists = File_has(loc);

	if(!fileExists)
		_gotoIfError(clean, Error_notFound(0, 0));

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	int ren = rename(resolved.ptr, newFileName.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, newFileName.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		_gotoIfError(clean, Error_stderr(0));

clean:
	FileInfo_freex(&info);
	CharString_freex(&resolved);
	return err;
}

Error File_move(CharString loc, CharString directoryName, Ns maxTimeout) {

	CharString resolved = CharString_createNull(), resolvedFile = CharString_createNull();
	Error err = Error_none();
	FileInfo info = (FileInfo) { 0 };

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	if(!CharString_isValidFilePath(directoryName))
		_gotoIfError(clean, Error_invalidParameter(1, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_moveVirtual(loc, directoryName, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));
	_gotoIfError(clean, File_resolvex(directoryName, &isVirtual, 0, &resolvedFile));

	if(isVirtual)
		_gotoIfError(clean, Error_invalidOperation(0));

	//Check if file exists

	if(!File_has(resolved))
		_gotoIfError(clean, Error_notFound(0, 0));

	if(!File_hasFolder(resolvedFile))
		_gotoIfError(clean, Error_notFound(0, 1));

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	CharString fileName = CharString_createNull();

	if(!CharString_cutBeforeLast(fileName, '/', EStringCase_Sensitive, &fileName))
		fileName = resolved;

	CharString_setAt(resolvedFile, CharString_length(resolvedFile) - 1, '/');

	_gotoIfError(clean, CharString_appendStringx(&resolvedFile, fileName));
	_gotoIfError(clean, CharString_appendx(&resolvedFile, '\0'));

	int ren = rename(resolved.ptr, resolvedFile.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, resolvedFile.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		_gotoIfError(clean, Error_stderr(0));

clean:
	FileInfo_freex(&info);
	CharString_freex(&resolved);
	CharString_freex(&resolvedFile);
	return err;
}

Error File_write(Buffer buf, CharString loc, Ns maxTimeout) {

	CharString resolved = CharString_createNull();
	Error err = Error_none();
	FILE *f = NULL;

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_writeVirtual(buf, loc, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	f = fopen(resolved.ptr, "wb");

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	while (!f && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		f = fopen(resolved.ptr, "wb");

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(!f)
		_gotoIfError(clean, Error_stderr(0));

	U64 bufLen = Buffer_length(buf);

	if(bufLen && fwrite(buf.ptr, 1, bufLen, f) != bufLen)
		_gotoIfError(clean, Error_stderr(1));

clean:
	if(f) fclose(f);
	CharString_freex(&resolved);
	return err;
}

Error File_read(CharString loc, Ns maxTimeout, Buffer *output) {

	CharString resolved = CharString_createNull();
	Error err = Error_none();
	FILE *f = NULL;

	if(!CharString_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0));

	if(!output)
		_gotoIfError(clean, Error_nullPointer(2));

	if(output->ptr)
		_gotoIfError(clean, Error_invalidOperation(0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_readVirtual(loc, output, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	f = fopen(resolved.ptr, "rb");

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	while (!f && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		f = fopen(resolved.ptr, "rb");

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(!f)
		_gotoIfError(clean, Error_stderr(0));

	if(fseek(f, 0, SEEK_END))
		_gotoIfError(clean, Error_stderr(1));

	U64 size = (U64)_ftelli64(f);

	if(!size)			//Empty files exist too
		goto success;

	_gotoIfError(clean, Buffer_createUninitializedBytesx(size, output));

	if(fseek(f, 0, SEEK_SET))
		_gotoIfError(clean, Error_stderr(2));

	Buffer b = *output;
	U64 bufLen = Buffer_length(b);

	if (fread((U8*)b.ptr, 1, bufLen, f) != bufLen)
		_gotoIfError(clean, Error_stderr(3));

	goto success;

clean:
	Buffer_freex(output);
success:
	if(f) fclose(f);
	CharString_freex(&resolved);
	return err;
}
