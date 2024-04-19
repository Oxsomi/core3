/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "platforms/ext/listx.h"
#include "types/thread.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/archivex.h"
#include "platforms/file.h"
#include "formats/oiCA.h"
#include "types/error.h"
#include "types/buffer.h"
#include "types/string.h"

#include <stdio.h>
#include <sys/stat.h>

#include "types/math.h"

//This is fine in file.c instead of wfile.c because unix + windows systems all share very similar ideas about filesystems
//But windows is a bit stricter in some parts (like the characters you can include) and has some weird quirks
//Still, it's common enough here to not require separation

#ifndef _WIN32

	#define _ftelli64 ftell
	#define _mkdir(a) mkdir(a, DEFFILEMODE)

	I32 removeFolder(CharString str) {

		CharString strCopy = CharString_createNull();
		Error err = Error_none();

		if(!CharString_isNullTerminated(str)) {
			gotoIfError(clean, CharString_createCopyx(str, &strCopy))
			remove(strCopy.ptr);
		}

		else remove(str.ptr);

	clean:
		CharString_freex(&strCopy);
		return err.genericError ? -1 : 0;
	}

#else

	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
	#include <Windows.h>
	#include <direct.h>
	#include <fileapi.h>

	#define stat _stat64
	#define S_ISREG(x) (x & _S_IFREG)
	#define S_ISDIR(x) (x & _S_IFDIR)

	I32 removeFolder(CharString str) {

		ListU16 utf8 = (ListU16) { 0 };

		if (CharString_toUtf16x(str, &utf8).genericError)
			return -1;

		const I32 res = RemoveDirectoryW((const wchar_t*)utf8.ptr) ? 0 : -1;
		ListU16_freex(&utf8);
		return res;
	}

#endif

//Private file functions

//Can only operate on //access, //function, //network

Error File_removeVirtual(CharString loc, Ns maxTimeout);
Error File_addVirtual(CharString loc, EFileType type, Ns maxTimeout);
Error File_renameVirtual(CharString loc, CharString newFileName, Ns maxTimeout);
Error File_moveVirtual(CharString loc, CharString directoryName, Ns maxTimeout);

Error File_writeVirtual(Buffer buf, CharString loc, Ns maxTimeout);

//Works on almost all virtual files

Error File_readVirtual(CharString loc, Buffer *output, Ns maxTimeout);

Error File_getInfoVirtual(CharString loc, FileInfo *info);
Error File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive);
Error File_queryFileObjectCountVirtual(CharString loc, EFileType type, Bool isRecursive, U64 *res);		//Inc files only
Error File_queryFileObjectCountAllVirtual(CharString loc, Bool isRecursive, U64 *res);					//Inc folders + files


//Both Linux and Windows require folder to be empty before removing.
//So we do it manually.

int removeFileOrFolder(CharString str);

Error recurseDelete(FileInfo info, void *unused) {

	(void)unused;

	if (removeFileOrFolder(info.path))
		return Error_invalidOperation(0, "recurseDelete() removeFileOrFolder failed");

	return Error_none();
}

int removeFileOrFolder(CharString str) {

	struct stat inf = (struct stat) { 0 };
	const int r = stat(str.ptr, &inf);

	if(r)
		return r;

	if (S_ISDIR(inf.st_mode)) {

		//Delete every file, because RemoveDirecotryA requires it to be empty
		//We'll handle recursion ourselves

		const Error err = File_foreach(str, recurseDelete, NULL, false);

		if(err.genericError)
			return -1;

		//Now that our folder is empty, we can finally delete it

		return removeFolder(str);
	}

	return remove(str.ptr);
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
	Error err;

	if(!info)
		gotoIfError(clean, Error_nullPointer(0, "File_getInfo()::info is required"))

	if(info->path.ptr)
		gotoIfError(clean, Error_invalidOperation(0, "File_getInfo()::info was already defined, may indicate memleak"))

	if(!CharString_isValidFilePath(loc))
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_getInfo()::loc wasn't a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_getInfoVirtual(loc, info))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

	struct stat inf = (struct stat) { 0 };

	if (stat(resolved.ptr, &inf))
		gotoIfError(clean, Error_notFound(0, 0, "File_getInfo()::loc file not found"))

	if (!S_ISDIR(inf.st_mode) && !S_ISREG(inf.st_mode))
		gotoIfError(clean, Error_invalidOperation(
			2, "File_getInfo()::loc file type not supporterd (must be file or folder)"
		))

	if (!(inf.st_mode & (S_IREAD | S_IWRITE)))
		gotoIfError(clean, Error_unauthorized(0, "File_getInfo()::loc file must be read and/or write"))

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

	Error err;
	CharString resolved = CharString_createNull();

	if(!res)
		gotoIfError(clean, Error_nullPointer(3, "File_queryFileObjectCount()::res is required"))

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	if(!CharString_isValidFilePath(loc))
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_queryFileObjectCount()::res must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_queryFileObjectCountVirtual(loc, type, isRecursive, res))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

	//Normal counter for local files

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	gotoIfError(clean, File_foreach(loc, (FileCallback) countFileTypeVirtual, &counter, isRecursive))
	*res = counter.counter;

clean:
	CharString_freex(&resolved);
	return err;
}

Error File_queryFileObjectCountAll(CharString loc, Bool isRecursive, U64 *res) {

	CharString resolved = CharString_createNull();
	Error err;

	if(!res)
		gotoIfError(clean, Error_nullPointer(2, "File_queryFileObjectCountAll()::res is required"))

	if(!CharString_isValidFilePath(loc))
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_queryFileObjectCountAll()::res must be a valid file path"))

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_queryFileObjectCountAllVirtual(loc, isRecursive, res))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

	//Normal counter for local files

	FileCounter counter = (FileCounter) { 0 };
	gotoIfError(clean, File_foreach(loc, (FileCallback) countFileTypeVirtual, &counter, isRecursive))
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
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_add()::loc must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_addVirtual(loc, type, maxTimeout))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

	err = File_getInfo(resolved, &info);

	if(err.genericError && err.genericError != EGenericError_NotFound)
		gotoIfError(clean, err)

	if (!err.genericError) {
		CharString_freex(&resolved);
		FileInfo_freex(&info);
		return Error_none();
	}

	//Already exists

	if(!err.genericError)
		gotoIfError(clean, info.type != type ? Error_alreadyDefined(0, "File_add()::loc already exists") : Error_none())

	FileInfo_freex(&info);

	//Check parent directories until none left

	if(CharString_containsSensitive(resolved, '/')) {

		if(!CharString_eraseFirstStringInsensitive(&resolved, Platform_instance.workingDirectory))
			gotoIfError(clean, Error_unauthorized(0, "File_add() escaped working directory. This is not supported."))

		gotoIfError(clean, CharString_splitSensitivex(resolved, '/', &str))

		for (U64 i = 0; i < str.length - 1; ++i) {

			CharString parent = CharString_createRefSized((C8*)resolved.ptr, CharString_end(str.ptr[i]) - resolved.ptr, false);

			err = File_getInfo(parent, &info);

			if(err.genericError && err.genericError != EGenericError_NotFound)
				gotoIfError(clean, err)

			if(info.type != EFileType_Folder)
				gotoIfError(clean, Error_invalidOperation(2, "File_add() one of the parents wasn't a folder"))

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

			if (_mkdir(parent.ptr))
				gotoIfError(clean, Error_stderr(0, "File_add() couldn't mkdir parent"))

			//Reset character that was replaced with \0

			if(prev)
				CharString_setAt(resolved, CharString_length(parent) + 1, prev);
		}

		CharStringList_freex(&str);
	}

	//Create folder

	if (type == EFileType_Folder && _mkdir(resolved.ptr))
		gotoIfError(clean, Error_stderr(1, "File_add() couldn't mkdir"))

	//Create file

	if(type == EFileType_File)
		gotoIfError(clean, File_write(Buffer_createNull(), resolved, maxTimeout))

clean:
	FileInfo_freex(&info);
	CharString_freex(&resolved);
	CharStringList_freex(&str);
	return err;
}

Error File_remove(CharString loc, Ns maxTimeout) {

	CharString resolved = CharString_createNull();
	Error err;

	if(!CharString_isValidFilePath(loc))
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_remove()::loc must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_removeVirtual(resolved, maxTimeout))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

	const Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	int res = removeFileOrFolder(resolved);

	while (res && maxTimeout) {

		Thread_sleep(maxTimeoutTry);

		res = removeFileOrFolder(resolved);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if (res)
		gotoIfError(clean, Error_stderr(0, "File_remove() couldn't remove file"))

clean:
	CharString_freex(&resolved);
	return err;
}

Bool File_has(CharString loc) {
	FileInfo info = (FileInfo) { 0 };
	const Error err = File_getInfo(loc, &info);
	FileInfo_freex(&info);
	return !err.genericError;
}

Bool File_hasType(CharString loc, EFileType type) {
	FileInfo info = (FileInfo) { 0 };
	const Error err = File_getInfo(loc, &info);
	const Bool sameType = info.type == type;
	FileInfo_freex(&info);
	return !err.genericError && sameType;
}

Error File_rename(CharString loc, CharString newFileName, Ns maxTimeout) {

	CharString resolved = CharString_createNull();
	Error err;
	FileInfo info = (FileInfo) { 0 };

	if(!CharString_isValidFilePath(loc))
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_rename()::loc must be a valid file path"))

	if(!CharString_isValidFileName(newFileName))
		gotoIfError(clean, Error_invalidParameter(1, 0, "File_rename()::newFileName must be a valid file name"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_renameVirtual(loc, newFileName, maxTimeout))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

	//Check if file exists

	const Bool fileExists = File_has(loc);

	if(!fileExists)
		gotoIfError(clean, Error_notFound(0, 0, "File_rename()::loc doesn't exist"))

	const Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	int ren = rename(resolved.ptr, newFileName.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, newFileName.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		gotoIfError(clean, Error_stderr(0, "File_rename() rename failed"))

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
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_move()::loc must be a valid file path"))

	if(!CharString_isValidFilePath(directoryName))
		gotoIfError(clean, Error_invalidParameter(1, 0, "File_move()::directoryName must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_moveVirtual(loc, directoryName, maxTimeout))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))
	gotoIfError(clean, File_resolvex(directoryName, &isVirtual, 0, &resolvedFile))

	if(isVirtual)
		gotoIfError(clean, Error_invalidOperation(0, "File_move() can't resolve to virtual file here?"))

	//Check if file exists

	if(!File_has(resolved))
		gotoIfError(clean, Error_notFound(0, 0, "File_move()::loc can't be found"))

	if(!File_hasFolder(resolvedFile))
		gotoIfError(clean, Error_notFound(0, 1, "File_move()::directoryName can't be found"))

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	CharString fileName = CharString_createNull();

	if(!CharString_cutBeforeLastSensitive(fileName, '/', &fileName))
		fileName = resolved;

	CharString_setAt(resolvedFile, CharString_length(resolvedFile) - 1, '/');

	gotoIfError(clean, CharString_appendStringx(&resolvedFile, fileName))
	gotoIfError(clean, CharString_appendx(&resolvedFile, '\0'))

	int ren = rename(resolved.ptr, resolvedFile.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, resolvedFile.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		gotoIfError(clean, Error_stderr(0, "File_move() couldn't move file"))

clean:
	FileInfo_freex(&info);
	CharString_freex(&resolved);
	CharString_freex(&resolvedFile);
	return err;
}

Error File_write(Buffer buf, CharString loc, Ns maxTimeout) {

	CharString resolved = CharString_createNull();
	Error err;
	FILE *f = NULL;

	if(!CharString_isValidFilePath(loc))
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_write()::loc must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_writeVirtual(buf, loc, maxTimeout))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

	f = fopen(resolved.ptr, "wb");

	const Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	while (!f && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		f = fopen(resolved.ptr, "wb");

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(!f)
		gotoIfError(clean, Error_stderr(0, "File_write() couldn't open file for write"))

	const U64 bufLen = Buffer_length(buf);

	if(bufLen && fwrite(buf.ptr, 1, bufLen, f) != bufLen)
		gotoIfError(clean, Error_stderr(1, "File_write() couldn't write file"))

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
		gotoIfError(clean, Error_invalidParameter(0, 0, "File_read()::loc must be a valid file path"))

	if(!output)
		gotoIfError(clean, Error_nullPointer(2, "File_read()::output is required"))

	if(output->ptr)
		gotoIfError(clean, Error_invalidOperation(0, "File_read()::output was filled, may indicate memleak"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError(clean, File_readVirtual(loc, output, maxTimeout))
		return Error_none();
	}

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 0, &resolved))

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
		gotoIfError(clean, Error_stderr(0, "File_read() couldn't open file for read"))

	if(fseek(f, 0, SEEK_END))
		gotoIfError(clean, Error_stderr(1, "File_read() couldn't seek end"))

	U64 size = (U64)_ftelli64(f);

	if(!size)			//Empty files exist too
		goto success;

	gotoIfError(clean, Buffer_createUninitializedBytesx(size, output))

	if(fseek(f, 0, SEEK_SET))
		gotoIfError(clean, Error_stderr(2, "File_read() couldn't seek begin"))

	Buffer b = *output;
	U64 bufLen = Buffer_length(b);

	if (fread((U8*)b.ptr, 1, bufLen, f) != bufLen)
		gotoIfError(clean, Error_stderr(3, "File_read() couldn't read file"))

	goto success;

clean:
	Buffer_freex(output);
success:
	if(f) fclose(f);
	CharString_freex(&resolved);
	return err;
}
//Virtual file support

typedef Error (*VirtualFileFunc)(void *userData, CharString resolved);

Error File_virtualOp(CharString loc, Ns maxTimeout, VirtualFileFunc f, void *userData, Bool isWrite) {

	(void)maxTimeout;

	Bool isVirtual = false;
	CharString resolved = CharString_createNull();
	Error err;

	gotoIfError(clean, File_resolvex(loc, &isVirtual, 128, &resolved))

	if(!isVirtual)
		gotoIfError(clean, Error_unsupportedOperation(0, "File_virtualOp()::loc should resolve to virtual path (//*)"))

	const CharString access = CharString_createRefCStrConst("//access/");
	const CharString function = CharString_createRefCStrConst("//function/");
	const CharString network = CharString_createRefCStrConst("//network/");

	if (CharString_startsWithStringInsensitive(loc, access)) {
		//TODO: Allow //access folder
		return Error_unimplemented(0, "File_virtualOp()::loc //access/ not supported yet");
	}

	if (CharString_startsWithStringInsensitive(loc, function)) {
		//TODO: Allow //function folder (user callbacks)
		return Error_unimplemented(1, "File_virtualOp()::loc //function/ not supported yet");
	}

	if (CharString_startsWithStringInsensitive(loc, network)) {
		//TODO: Allow //network folder (access to \\ on windows)
		return Error_unimplemented(1, "File_virtualOp()::loc //network/ not supported yet");
	}

	err =
		isWrite ? Error_constData(
			1, 0, 
			"File_virtualOp()::isWrite is disallowed when acting on virtual files "
			"(except //access/*, //network/* or //function/*)"
		) : (
			!f ? Error_unimplemented(2, "File_virtualOp()::f is required") : f(userData, resolved)
		);

clean:
	CharString_freex(&resolved);
	return err;
}

//These are write operations, we don't support them yet.

Error File_removeVirtual(CharString loc, Ns maxTimeout) {
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_addVirtual(CharString loc, EFileType type, Ns maxTimeout) {
	(void)type;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_renameVirtual(CharString loc, CharString newFileName, Ns maxTimeout) {
	(void)newFileName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_moveVirtual(CharString loc, CharString directoryName, Ns maxTimeout) {
	(void)directoryName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

Error File_writeVirtual(Buffer buf, CharString loc, Ns maxTimeout) {
	(void)buf;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true);
}

//Read operations

Error File_resolveVirtual(CharString loc, CharString *subPath, const VirtualSection **section) {

	CharString copy = CharString_createNull();
	CharString copy1 = CharString_createNull();
	CharString copy2 = CharString_createNull();
	Error err;

	CharString_toLower(loc);
	gotoIfError(clean, CharString_createCopyx(loc, &copy))
	gotoIfError(clean, CharString_createCopyx(loc, &copy2))

	if(CharString_length(copy2))
		gotoIfError(clean, CharString_appendx(&copy2, '/'))		//Don't append to root

	//Root is always present

	else goto clean;

	if(!Lock_isLockedForThread(&Platform_instance.virtualSectionsLock))
		gotoIfError(clean, Error_invalidState(0, "File_resolveVirtual() requires virtualSectionsLock"))

	//Check sections

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		const VirtualSection *sectioni = Platform_instance.virtualSections.ptr + i;

		//If folder is the same, we found a section.
		//This section won't return any subPath or section,
		//This will force it to be identified as a folder.

		if (CharString_equalsStringInsensitive(loc, sectioni->path))
			goto clean;

		//Parent folder.

		if(CharString_startsWithStringInsensitive(sectioni->path, copy2))
			goto clean;

		//Check if the section includes the referenced file/folder

		CharString_freex(&copy1);
		gotoIfError(clean, CharString_createCopyx(sectioni->path, &copy1))
		gotoIfError(clean, CharString_appendx(&copy1, '/'))

		if(CharString_startsWithStringInsensitive(copy, copy1)) {
			CharString_cut(copy, CharString_length(copy1), 0, subPath);
			*section = sectioni;
			goto clean;
		}
	}

	gotoIfError(clean, Error_notFound(0, 0, "File_resolveVirtual() can't find section"))

clean:

	CharString_freex(&copy1);
	CharString_freex(&copy2);

	if (CharString_length(*subPath) && !err.genericError) {
		CharString_createCopyx(*subPath, &copy1);
		*subPath = copy1;
		copy1 = CharString_createNull();
	}

	CharString_freex(&copy);
	return err;
}

Error File_readVirtualInternal(Buffer *output, CharString loc) {

	CharString subPath = CharString_createNull();
	const VirtualSection *section = NULL;
	Error err;
	Buffer tmp = Buffer_createNull();
	const ELockAcquire acq = Lock_lock(&Platform_instance.virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(0, "File_readVirtualInternal() couldn't lock virtualSectionsLock"))

	gotoIfError(clean, File_resolveVirtual(loc, &subPath, &section))

	if(!section)
		gotoIfError(clean, Error_invalidOperation(0, "File_readVirtualInternal() section couldn't be found"))

	//Create copy of data.
	//It's possible the file data is unloaded in parallel and then this buffer would point to invalid data.

	gotoIfError(clean, Archive_getFileDataConstx(section->loadedData, subPath, &tmp))
	gotoIfError(clean, Buffer_createCopyx(tmp, output))

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&Platform_instance.virtualSectionsLock);

	Buffer_freex(&tmp);
	CharString_freex(&subPath);
	return err;
}

Error File_readVirtual(CharString loc, Buffer *output, Ns maxTimeout) {

	if(!output)
		return Error_nullPointer(1, "File_readVirtual()::output is required");

	return File_virtualOp(
		loc, maxTimeout,
		(VirtualFileFunc) File_readVirtualInternal,
		output,
		false
	);
}

Error File_getInfoVirtualInternal(FileInfo *info, CharString loc) {

	CharString subPath = CharString_createNull();
	const VirtualSection *section = NULL;
	Error err;
	const ELockAcquire acq = Lock_lock(&Platform_instance.virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(0, "File_getInfoVirtualInternal() couldn't lock virtualSectionsLock"))

	gotoIfError(clean, File_resolveVirtual(loc, &subPath, &section))

	if(!section) {	//Parent dir

		if(!CharString_length(loc))
			loc = CharString_createRefCStrConst(".");

		CharString copy = CharString_createNull();
		gotoIfError(clean, CharString_createCopyx(loc, &copy))

		*info = (FileInfo) {
			.path = copy,
			.type = EFileType_Folder,
			.access = EFileAccess_Read
		};
	}

	else {

		gotoIfError(clean, Archive_getInfox(section->loadedData, subPath, info))

		CharString tmp = CharString_createNull();
		CharString_cut(loc, 0, CharString_length(loc) - CharString_length(subPath), &tmp);

		gotoIfError(clean, CharString_insertStringx(&info->path, tmp, 0))
	}

	gotoIfError(clean, CharString_insertStringx(&info->path, CharString_createRefCStrConst("//"), 0))

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&Platform_instance.virtualSectionsLock);

	if(err.genericError)
		FileInfo_freex(info);

	CharString_freex(&subPath);
	return err;
}

Error File_getInfoVirtual(CharString loc, FileInfo *info) {

	if(!info)
		return Error_nullPointer(1, "File_getInfoVirtual()::info is required");

	if(info->access)
		return Error_invalidOperation(0, "File_getInfoVirtual()::info isn't empty, might indicate memleak");

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
	Error err;

	gotoIfError(clean, CharString_createCopyx(userData->currentPath, &fullPath))
	gotoIfError(clean, CharString_appendStringx(&fullPath, info.path))
	gotoIfError(clean, CharString_insertStringx(&fullPath, CharString_createRefCStrConst("//"), 0))

	info.path = fullPath;

	gotoIfError(clean, userData->callback(info, userData->userData))

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
	CharString root = CharString_createRefCStrConst(".");
	ListCharString visited = (ListCharString) { 0 };
	ELockAcquire acq = ELockAcquire_Invalid;

	CharString_toLower(resolved);
	gotoIfError(clean, CharString_createCopyx(resolved, &copy))

	if(CharString_length(copy))
		gotoIfError(clean, CharString_appendx(&copy, '/'))		//Don't append to root

	acq = Lock_lock(&Platform_instance.virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(0, "File_unloadVirtualInternal() couldn't lock virtualSectionsLock"))

	U64 baseCount = CharString_countAllSensitive(copy, '/');

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		const VirtualSection *section = Platform_instance.virtualSections.ptr + i;

		CharString_freex(&copy1);
		CharString_freex(&copy2);

		gotoIfError(clean, CharString_createCopyx(section->path, &copy1))
		CharString_toLower(copy1);

		gotoIfError(clean, CharString_createCopyx(copy1, &copy2))
		gotoIfError(clean, CharString_appendx(&copy2, '/'))

		if(
			!CharString_startsWithStringSensitive(copy1, copy) &&
			!CharString_equalsStringSensitive(copy1, resolved) &&
			!CharString_startsWithStringSensitive(copy, copy2)
		)
			continue;

		//Root directories don't exist, so we have to pretend they do

		if (!CharString_length(resolved)) {

			CharString parent = CharString_createNull();
			if(!CharString_cutAfterFirstSensitive(copy1, '/', &parent))
				gotoIfError(clean, Error_invalidState(0, "File_foreachVirtualInternal() cutAfterFirst failed"))

			Bool contains = false;

			for(U64 j = 0; j < visited.length; ++j)
				if (CharString_equalsStringSensitive(parent, visited.ptr[j])) {
					contains = true;
					break;
				}

			if (!contains) {

				//Avoid duplicates

				gotoIfError(clean, ListCharString_pushBackx(&visited, parent))

				CharString_freex(&copy3);
				gotoIfError(clean, CharString_createCopyx(parent, &copy3))
				gotoIfError(clean, CharString_insertStringx(&copy3, CharString_createRefCStrConst("//"), 0))

				FileInfo info = (FileInfo) {
					.path = copy3,
					.type = EFileType_Folder,
					.access = EFileAccess_Read
				};

				gotoIfError(clean, userData->callback(info, userData->userData))
			}
		}

		//All folders

		if (
			(!userData->isRecursive && baseCount == CharString_countAllSensitive(section->path, '/')) ||
			(userData->isRecursive && baseCount <= CharString_countAllSensitive(section->path, '/'))
		) {

			CharString_freex(&copy3);
			gotoIfError(clean, CharString_createCopyx(copy1, &copy3))
			gotoIfError(clean, CharString_insertStringx(&copy3, CharString_createRefCStrConst("//"), 0))

			FileInfo info = (FileInfo) {
				.path = copy3,
				.type = EFileType_Folder,
				.access = EFileAccess_Read
			};

			gotoIfError(clean, userData->callback(info, userData->userData))
		}

		//We can only loop through the folders if they're loaded. Otherwise, we consider them as empty.

		if(section->loaded) {

			userData->currentPath = copy2;

			if(userData->isRecursive || baseCount >= 2) {

				CharString child = CharString_createNull();

				if(baseCount > 2 && !CharString_cut(resolved, CharString_length(copy2), 0, &child))
					gotoIfError(clean, Error_invalidState(1, "File_foreachVirtualInternal() cut failed"))

				if(!CharString_length(child))
					child = root;

				gotoIfError(clean, Archive_foreachx(
					section->loadedData,
					child,
					(FileCallback) File_virtualCallback, userData,
					userData->isRecursive,
					EFileType_Any
				))
			}
		}
	}

	gotoIfError(clean, Error_unimplemented(0, "File_foreachVirtualInternal() couldn't find virtual section"))

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&Platform_instance.virtualSectionsLock);

	ListCharString_freex(&visited);
	CharString_freex(&copy);
	CharString_freex(&copy1);
	CharString_freex(&copy2);
	CharString_freex(&copy3);
	return err;
}

Error File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive) {

	if(!callback)
		return Error_nullPointer(1, "File_foreachVirtual()::callback is required");

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

	Error err;

	if(!res)
		gotoIfError(clean, Error_nullPointer(3, "File_queryFileObjectCountVirtual()::res is required"))

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	gotoIfError(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive))
	*res = counter.counter;

clean:
	return err;
}

Error File_queryFileObjectCountAllVirtual(CharString loc, Bool isRecursive, U64 *res) {

	Error err;

	if(!res)
		gotoIfError(clean, Error_nullPointer(2, "File_queryFileObjectCountAllVirtual()::res is required"))

	FileCounter counter = (FileCounter) { 0 };
	gotoIfError(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive))
	*res = counter.counter;

clean:
	return err;
}

impl Error File_loadVirtualInternal1(FileLoadVirtual *userData, CharString loc, Bool allowLoad);

Error File_loadVirtualInternal(FileLoadVirtual *userData, CharString loc) {
	return File_loadVirtualInternal1(userData, loc, true);
}

Error File_unloadVirtualInternal(void *userData, CharString loc) {

	(void)userData;

	CharString isChild = CharString_createNull();
	Error err = Error_none();
	ELockAcquire acq = ELockAcquire_Invalid;

	gotoIfError(clean, CharString_createCopyx(loc, &isChild))

	if(CharString_length(isChild))
		gotoIfError(clean, CharString_appendx(&isChild, '/'))		//Don't append to root

	acq = Lock_lock(&Platform_instance.virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		gotoIfError(clean, Error_invalidState(0, "File_unloadVirtualInternal() couldn't lock virtualSectionsLock"))

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *section = Platform_instance.virtualSections.ptrNonConst + i;

		if(
			!CharString_equalsStringInsensitive(loc, section->path) &&
			!CharString_startsWithStringInsensitive(section->path, isChild)
		)
			continue;

		if(section->loaded)
			Archive_freex(&section->loadedData);
	}

clean:

	if(acq == ELockAcquire_Acquired)
		Lock_unlock(&Platform_instance.virtualSectionsLock);

	CharString_freex(&isChild);
	return err;
}

Bool File_isVirtualLoaded(CharString loc) {
	FileLoadVirtual virt = (FileLoadVirtual) { 0 };
	return !File_loadVirtualInternal1(&virt, loc, false).genericError;
}

Error File_loadVirtual(CharString loc, const U32 encryptionKey[8]) {
	FileLoadVirtual virt = (FileLoadVirtual) { .doLoad = true, .encryptionKey = encryptionKey };
	return File_virtualOp(loc, 1 * SECOND, (VirtualFileFunc) File_loadVirtualInternal, &virt, false);
}

Error File_unloadVirtual(CharString loc) {
	return File_virtualOp(loc, 1 * SECOND, File_unloadVirtualInternal, NULL, false);
}
