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

		Error err = File_foreach(String_createConstRefUnsafe(ptr), recurseDelete, NULL, false);

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

Error File_resolvex(String loc, Bool *isVirtual, U64 maxFilePathLimit, String *result) {
	return File_resolve(
		loc, isVirtual, 
		maxFilePathLimit, 
		Platform_instance.workingDirectory, Platform_instance.alloc, 
		result
	);
}

Error File_getInfo(String loc, FileInfo *info) {

	String resolved = String_createNull();
	Error err = Error_none();

	if(!info) 
		_gotoIfError(clean, Error_nullPointer(0, 0));

	if(info->path.ptr) 
		_gotoIfError(clean, Error_invalidOperation(0));

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_getInfoVirtual(loc, info));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	struct stat inf = (struct stat) { 0 };

	if (stat(resolved.ptr, &inf))
		_gotoIfError(clean, Error_notFound(0, 0, 0));

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
			(inf.st_mode & S_IWRITE ? FileAccess_Write : FileAccess_None) | 
			(inf.st_mode & S_IREAD  ? FileAccess_Read  : FileAccess_None)
	};

	return Error_none();

clean:
	String_freex(&resolved);
	return err;
}

Bool File_hasFile(String loc) { return File_hasType(loc, EFileType_File); }
Bool File_hasFolder(String loc) { return File_hasType(loc, EFileType_Folder); }

Error File_queryFileCount(String loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, EFileType_File, isRecursive, res); 
}

Error File_queryFolderCount(String loc, Bool isRecursive, U64 *res) { 
	return File_queryFileObjectCount(loc, EFileType_Folder, isRecursive, res); 
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

Error File_queryFileObjectCount(String loc, EFileType type, Bool isRecursive, U64 *res) {

	Error err = Error_none();
	String resolved = String_createNull();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(3, 0));

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	if(!String_isValidFilePath(loc)) 
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_queryFileObjectCountVirtual(loc, type, isRecursive, res));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	//Normal counter for local files

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	_gotoIfError(clean, File_foreach(loc, (FileCallback) countFileType, &counter, isRecursive));
	*res = counter.counter;

clean:
	String_freex(&resolved);
	return err;
}

Error File_queryFileObjectCountAll(String loc, Bool isRecursive, U64 *res) {

	String resolved = String_createNull();
	Error err = Error_none();

	if(!res)
		_gotoIfError(clean, Error_nullPointer(2, 0));

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

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
	_gotoIfError(clean, File_foreach(loc, (FileCallback) countFileType, &counter, isRecursive));
	*res = counter.counter;

clean:
	String_freex(&resolved);
	return err;
}

Error File_add(String loc, EFileType type, Ns maxTimeout) {

	String resolved = String_createNull();
	StringList str = (StringList) { 0 };
	FileInfo info = (FileInfo) { 0 };
	Error err = Error_none();

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

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
		String_freex(&resolved);
		FileInfo_freex(&info);
		return Error_none();
	}

	//Already exists

	if(!err.genericError)
		_gotoIfError(clean, info.type != type ? Error_alreadyDefined(0) : Error_none());

	FileInfo_freex(&info);

	//Check parent directories until none left

	if(String_contains(resolved, '/', EStringCase_Sensitive)) {

		if(!String_eraseFirstString(&resolved, Platform_instance.workingDirectory, EStringCase_Insensitive))
			_gotoIfError(clean, Error_unauthorized(0));

		_gotoIfError(clean, String_splitx(resolved, '/', EStringCase_Sensitive, &str));

		for (U64 i = 0; i < str.length - 1; ++i) {

			String parent = String_createRefSized(resolved.ptr, String_end(str.ptr[i]) - resolved.ptr);

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

			if (String_end(parent) != String_end(resolved)) {

				prev = String_getAt(parent, parent.length - 1);
				if(!String_setAt(parent, parent.length - 1, '\0'))
					_gotoIfError(clean, Error_invalidOperation(2));
			}

			//Make parent

			if (mkdir(parent.ptr))
				_gotoIfError(clean, Error_invalidOperation(1));

			//Reset character that was replaced with \0

			if(prev)
				String_setAt(parent, parent.length - 1, prev);
		}

		StringList_freex(&str);
	}

	//Create folder

	if (type == EFileType_Folder && mkdir(resolved.ptr))
		_gotoIfError(clean, Error_invalidOperation(0));

	//Create file

	if(type == EFileType_File)
		_gotoIfError(clean, File_write(Buffer_createNull(), resolved, maxTimeout));

clean:
	FileInfo_freex(&info);
	String_freex(&resolved);
	StringList_freex(&str);
	return err;
}

Error File_remove(String loc, Ns maxTimeout) {

	String resolved = String_createNull();
	Error err = Error_none();

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

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
		_gotoIfError(clean, Error_notFound(0, 0, 0));

clean:
	String_freex(&resolved);
	return err;
}

Bool File_has(String loc) {
	FileInfo info = (FileInfo) { 0 };
	Error err = File_getInfo(loc, &info);
	FileInfo_freex(&info);
	return !err.genericError;
}

Bool File_hasType(String loc, EFileType type) {
	FileInfo info = (FileInfo) { 0 };
	Error err = File_getInfo(loc, &info);
	Bool sameType = info.type == type;
	FileInfo_freex(&info);
	return !err.genericError && sameType;
}

Error File_rename(String loc, String newFileName, Ns maxTimeout) {

	String resolved = String_createNull();
	Error err = Error_none();
	FileInfo info = (FileInfo) { 0 };

	if(!String_isValidFilePath(newFileName))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	if(!String_isValidFileName(newFileName))
		_gotoIfError(clean, Error_invalidParameter(1, 0, 0));

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		_gotoIfError(clean, File_renameVirtual(loc, newFileName, maxTimeout));
		return Error_none();
	}

	_gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved));

	//Check if file exists

	Bool fileExists = File_has(loc);

	if(!fileExists)
		_gotoIfError(clean, Error_notFound(0, 0, 0));

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
		_gotoIfError(clean, Error_invalidState(0));

clean:
	FileInfo_freex(&info);
	String_freex(&resolved);
	return err;
}

Error File_move(String loc, String directoryName, Ns maxTimeout) {

	String resolved = String_createNull(), resolvedFile = String_createNull();
	Error err = Error_none();
	FileInfo info = (FileInfo) { 0 };

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	if(!String_isValidFilePath(directoryName))
		_gotoIfError(clean, Error_invalidParameter(1, 0, 0));

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
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	if(!File_hasFolder(resolvedFile))
		_gotoIfError(clean, Error_notFound(0, 1, 0));

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	String fileName = String_createNull();

	if(!String_cutBeforeLast(fileName, '/', EStringCase_Sensitive, &fileName))
		fileName = resolved;

	String_setAt(resolvedFile, resolvedFile.length - 1, '/');

	_gotoIfError(clean, String_appendStringx(&resolvedFile, fileName));
	_gotoIfError(clean, String_appendx(&resolvedFile, '\0'));

	int ren = rename(resolved.ptr, resolvedFile.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, resolvedFile.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		_gotoIfError(clean, Error_invalidState(0));

clean:
	FileInfo_freex(&info);
	String_freex(&resolved);
	String_freex(&resolvedFile);
	return err;
}

Error File_write(Buffer buf, String loc, Ns maxTimeout) {

	String resolved = String_createNull();
	Error err = Error_none();
	FILE *f = NULL;

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

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
		_gotoIfError(clean, Error_notFound(1, 0, 0));

	U64 bufLen = Buffer_length(buf);

	if(bufLen && fwrite(buf.ptr, 1, bufLen, f) != bufLen)
		_gotoIfError(clean, Error_invalidState(0));

clean:
	if(f) fclose(f);
	String_freex(&resolved);
	return err;
}

Error File_read(String loc, Ns maxTimeout, Buffer *output) {

	String resolved = String_createNull();
	Error err = Error_none();
	FILE *f = NULL;

	if(!String_isValidFilePath(loc))
		_gotoIfError(clean, Error_invalidParameter(0, 0, 0));

	if(!output)
		_gotoIfError(clean, Error_nullPointer(2, 0));

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
		_gotoIfError(clean, Error_notFound(0, 0, 0));

	if(fseek(f, 0, SEEK_END))
		_gotoIfError(clean, Error_invalidState(0));

	U64 size = (U64)_ftelli64(f);

	if(!size)			//Empty files exist too
		goto success;

	_gotoIfError(clean, Buffer_createUninitializedBytesx(size, output));

	if(fseek(f, 0, SEEK_SET))
		_gotoIfError(clean, Error_invalidState(1));

	Buffer b = *output;
	U64 bufLen = Buffer_length(b);

	if (fread(b.ptr, 1, bufLen, f) != bufLen)
		_gotoIfError(clean, Error_invalidState(2));

	goto success;

clean:
	Buffer_freex(output);
success:
	if(f) fclose(f);
	String_freex(&resolved);
	return err;
}
