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

#include "platforms/ext/listx.h"
#include "types/base/thread.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/archivex.h"
#include "platforms/file.h"
#include "formats/oiCA/ca_file.h"
#include "types/base/error.h"
#include "types/container/buffer.h"
#include "types/container/string.h"

#include <stdio.h>
#include <sys/stat.h>

#include "types/math/math.h"

//This is fine in file.c instead of wfile.c because unix + windows systems all share very similar ideas about filesystems
//But windows is a bit stricter in some parts (like the characters you can include) and has some weird quirks
//Still, it's common enough here to not require separation

#ifndef _WIN32

	#define _ftelli64 ftell
	#define _fseeki64 fseek
	#define _mkdir(a) mkdir(a, ALLPERMS)

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
	#define NOMINMAX
	#include <Windows.h>
	#include <direct.h>
	#include <fileapi.h>

	#define stat _stat64
	#define S_ISREG(x) (x & _S_IFREG)
	#define S_ISDIR(x) (x & _S_IFDIR)

	//Can't rely on manifest.xml being present that allows use of RemoveDirectoryA with UTF8.
	I32 removeFolder(CharString str) {

		ListU16 utf8 = (ListU16) { 0 };

		if (CharString_toUTF16x(str, &utf8).genericError)
			return -1;

		const I32 res = RemoveDirectoryW((const wchar_t*)utf8.ptr) ? 0 : -1;
		ListU16_freex(&utf8);
		return res;
	}

#endif

//Private file functions

//Can only operate on //access, //function, //network

Bool File_removeVirtual(CharString loc, Ns maxTimeout, Error *e_rr);
Bool File_addVirtual(CharString loc, EFileType type, Ns maxTimeout, Error *e_rr);
Bool File_renameVirtual(CharString loc, CharString newFileName, Ns maxTimeout, Error *e_rr);
Bool File_moveVirtual(CharString loc, CharString directoryName, Ns maxTimeout, Error *e_rr);

Bool File_writeVirtual(Buffer buf, CharString loc, Ns maxTimeout, Error *e_rr);

//Works on almost all virtual files

Bool File_readVirtual(CharString loc, Buffer *output, Ns maxTimeout, Error *e_rr);

Bool File_getInfoVirtual(CharString loc, FileInfo *info, Error *e_rr);
Bool File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr);

//Inc files only
Bool File_queryFileObjectCountVirtual(CharString loc, EFileType type, Bool isRecursive, U64 *res, Error *e_rr);

//Inc folders + files
Bool File_queryFileObjectCountAllVirtual(CharString loc, Bool isRecursive, U64 *res, Error *e_rr);

//Both Linux and Windows require folder to be empty before removing.
//So we do it manually.

int removeFileOrFolder(CharString str);

Bool recurseDelete(FileInfo info, void *unused, Error *e_rr) {

	(void)unused;
	Bool s_uccess = true;

	if (removeFileOrFolder(info.path))
		retError(clean, Error_invalidOperation(0, "recurseDelete() removeFileOrFolder failed"))

clean:
	return s_uccess;
}

int removeFileOrFolder(CharString str) {

	struct stat inf = (struct stat) { 0 };
	const int r = stat(str.ptr, &inf);

	if(r)
		return r;

	if (S_ISDIR(inf.st_mode)) {

		//Delete every file, because RemoveDirectoryW requires it to be empty
		//We'll handle recursion ourselves

		if(!File_foreach(str, recurseDelete, NULL, false, NULL))
			return -1;

		//Now that our folder is empty, we can finally delete it

		return removeFolder(str);
	}

	return remove(str.ptr);
}

//

Bool FileInfo_freex(FileInfo *fileInfo) {
	return FileInfo_free(fileInfo, Platform_instance->alloc);
}

Bool File_resolvex(CharString loc, Bool *isVirtual, U64 maxFilePathLimit, CharString *result, Error *e_rr) {
	return File_resolve(
		loc, isVirtual,
		maxFilePathLimit,
		Platform_instance->workingDirectory, Platform_instance->alloc,
		result,
		e_rr
	);
}
Bool File_makeRelativex(CharString base, CharString subFile, U64 maxFilePathLimit, CharString *result, Error *e_rr) {
	return File_makeRelative(
		Platform_instance->workingDirectory, base, subFile,
		maxFilePathLimit,
		Platform_instance->alloc,
		result,
		e_rr
	);
}

Bool File_getInfo(CharString loc, FileInfo *info, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!info)
		retError(clean, Error_nullPointer(0, "File_getInfo()::info is required"))

	if(info->path.ptr)
		retError(clean, Error_invalidOperation(0, "File_getInfo()::info was already defined, may indicate memleak"))

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_getInfo()::loc wasn't a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_getInfoVirtual(loc, info, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, Platform_instance->workingDirectory, alloc, &resolved, e_rr))

	struct stat inf = (struct stat) { 0 };

	if (stat(resolved.ptr, &inf))
		retError(clean, Error_notFound(0, 0, "File_getInfo()::loc file not found"))

	if (!S_ISDIR(inf.st_mode) && !S_ISREG(inf.st_mode))
		retError(clean, Error_invalidOperation(
			2, "File_getInfo()::loc file type not supporterd (must be file or folder)"
		))

	if (!(inf.st_mode & (S_IREAD | S_IWRITE)))
		retError(clean, Error_unauthorized(0, "File_getInfo()::loc file must be read and/or write"))

	*info = (FileInfo) {

		.timestamp = (Ns)inf.st_mtime * SECOND,
		.path = resolved,

		.type = S_ISDIR(inf.st_mode) ? EFileType_Folder : EFileType_File,
		.fileSize = (U64) inf.st_size,

		.access =
			(inf.st_mode & S_IWRITE ? EFileAccess_Write : EFileAccess_None) |
			(inf.st_mode & S_IREAD  ? EFileAccess_Read  : EFileAccess_None)
	};

clean:

	if(!s_uccess)
		CharString_free(&resolved, alloc);

	return s_uccess;
}

Bool File_getInfox(CharString loc, FileInfo *info, Error *e_rr) {
	return File_getInfo(loc, info, Platform_instance->alloc, e_rr);
}

Bool File_hasFile(CharString loc) { return File_hasType(loc, EFileType_File); }
Bool File_hasFolder(CharString loc) { return File_hasType(loc, EFileType_Folder); }

Bool File_queryFileCount(CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return File_queryFileObjectCount(loc, EFileType_File, isRecursive, res, e_rr);
}

Bool File_queryFolderCount(CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {
	return File_queryFileObjectCount(loc, EFileType_Folder, isRecursive, res, e_rr);
}

typedef struct FileCounter {
	EFileType type;
	Bool useType;
	U64 counter;
} FileCounter;

Bool countFileTypeVirtual(FileInfo info, FileCounter *counter, Error *e_rr) {

	(void) e_rr;

	if (!counter->useType) {
		++counter->counter;
		return false;
	}

	if(info.type == counter->type)
		++counter->counter;

	return false;
}

Bool File_queryFileObjectCount(CharString loc, EFileType type, Bool isRecursive, U64 *res, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!res)
		retError(clean, Error_nullPointer(3, "File_queryFileObjectCount()::res is required"))

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_queryFileObjectCount()::res must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_queryFileObjectCountVirtual(loc, type, isRecursive, res, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 0, &resolved, e_rr))

	//Normal counter for local files

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	gotoIfError3(clean, File_foreach(loc, (FileCallback) countFileTypeVirtual, &counter, isRecursive, e_rr))
	*res = counter.counter;

clean:
	CharString_freex(&resolved);
	return s_uccess;
}

Bool File_queryFileObjectCountAll(CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!res)
		retError(clean, Error_nullPointer(2, "File_queryFileObjectCountAll()::res is required"))

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_queryFileObjectCountAll()::res must be a valid file path"))

	//Virtual files can supply a faster way of counting files
	//Such as caching it and updating it if something is changed

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_queryFileObjectCountAllVirtual(loc, isRecursive, res, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 0, &resolved, e_rr))

	//Normal counter for local files

	FileCounter counter = (FileCounter) { 0 };
	gotoIfError3(clean, File_foreach(loc, (FileCallback) countFileTypeVirtual, &counter, isRecursive, e_rr))
	*res = counter.counter;

clean:
	CharString_freex(&resolved);
	return s_uccess;
}

Bool File_add(CharString loc, EFileType type, Ns maxTimeout, Bool createParentOnly, Allocator alloc, Error *e_rr) {

	CharString resolved = CharString_createNull();
	ListCharString str = (ListCharString) { 0 };
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = true;

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_add()::loc must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_addVirtual(loc, type, maxTimeout, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, Platform_instance->workingDirectory, alloc, &resolved, e_rr))

	{
		Error errTmp = Error_none();
		Bool successTmp = File_getInfo(resolved, &info, alloc, &errTmp);

		if(!successTmp && errTmp.genericError != EGenericError_NotFound)
			retError(clean, errTmp)

		if(successTmp)
			goto clean;

		FileInfo_free(&info, alloc);
	}

	//Check parent directories until none left

	if(CharString_containsSensitive(resolved, '/', 0, 0)) {

		if(!CharString_eraseFirstStringInsensitive(&resolved, Platform_instance->workingDirectory, 0, 0))
			retError(clean, Error_unauthorized(0, "File_add() escaped working directory. This is not supported."))

		gotoIfError2(clean, CharString_splitSensitive(resolved, '/', alloc, &str))

		for (U64 i = 0; i < str.length - 1; ++i) {

			CharString parent = CharString_createRefSized((C8*)resolved.ptr, CharString_end(str.ptr[i]) - resolved.ptr, false);

			Error errTmp = Error_none();
			Bool successTmp = File_getInfo(parent, &info, alloc, &errTmp);

			if(!successTmp && errTmp.genericError != EGenericError_NotFound)
				retError(clean, errTmp)

			if(info.type != EFileType_Folder)
				retError(clean, Error_invalidOperation(2, "File_add() one of the parents wasn't a folder"))

			FileInfo_free(&info, alloc);

			if(successTmp)		//Already defined, continue to child
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
				retError(clean, Error_stderr(0, "File_add() couldn't mkdir parent"))

			//Reset character that was replaced with \0

			if(prev)
				CharString_setAt(resolved, CharString_length(parent) + 1, prev);
		}

		ListCharString_free(&str, alloc);
	}

	//Create folder

	if (type == EFileType_Folder && _mkdir(resolved.ptr))
		retError(clean, Error_stderr(1, "File_add() couldn't mkdir"))

	//Create file

	if(type == EFileType_File && !createParentOnly)
		gotoIfError3(clean, File_write(Buffer_createNull(), resolved, 0, 0, maxTimeout, false, alloc, e_rr))

clean:
	FileInfo_free(&info, alloc);
	CharString_free(&resolved, alloc);
	ListCharString_free(&str, alloc);
	return s_uccess;
}

Bool File_addx(CharString loc, EFileType type, Ns maxTimeout, Bool createParentOnly, Error *e_rr) {
	return File_add(loc, type, maxTimeout, createParentOnly, Platform_instance->alloc, e_rr);
}

Bool File_remove(CharString loc, Ns maxTimeout, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_remove()::loc must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_removeVirtual(resolved, maxTimeout, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 0, &resolved, e_rr))

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
		retError(clean, Error_stderr(0, "File_remove() couldn't remove file"))

clean:
	CharString_freex(&resolved);
	return s_uccess;
}

Bool File_has(CharString loc) {
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = File_getInfox(loc, &info, NULL);
	FileInfo_freex(&info);
	return s_uccess;
}

Bool File_hasType(CharString loc, EFileType type) {
	FileInfo info = (FileInfo) { 0 };
	Bool s_uccess = File_getInfox(loc, &info, NULL);
	const Bool sameType = info.type == type;
	FileInfo_freex(&info);
	return s_uccess && sameType;
}

Bool File_rename(CharString loc, CharString newFileName, Ns maxTimeout, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	FileInfo info = (FileInfo) { 0 };

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_rename()::loc must be a valid file path"))

	if(!CharString_isValidFileName(newFileName))
		retError(clean, Error_invalidParameter(1, 0, "File_rename()::newFileName must be a valid file name"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_renameVirtual(loc, newFileName, maxTimeout, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 0, &resolved, e_rr))

	//Check if file exists

	const Bool fileExists = File_has(loc);

	if(!fileExists)
		retError(clean, Error_notFound(0, 0, "File_rename()::loc doesn't exist"))

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
		retError(clean, Error_stderr(0, "File_rename() rename failed"))

clean:
	FileInfo_freex(&info);
	CharString_freex(&resolved);
	return s_uccess;
}

Bool File_move(CharString loc, CharString directoryName, Ns maxTimeout, Error *e_rr) {

	CharString resolved = CharString_createNull(), resolvedFile = CharString_createNull();
	Bool s_uccess = true;
	FileInfo info = (FileInfo) { 0 };

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_move()::loc must be a valid file path"))

	if(!CharString_isValidFilePath(directoryName))
		retError(clean, Error_invalidParameter(1, 0, "File_move()::directoryName must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_moveVirtual(loc, directoryName, maxTimeout, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 0, &resolved, e_rr))
	gotoIfError3(clean, File_resolvex(directoryName, &isVirtual, 0, &resolvedFile, e_rr))

	if(isVirtual)
		retError(clean, Error_invalidOperation(0, "File_move() can't resolve to virtual file here?"))

	//Check if file exists

	if(!File_has(resolved))
		retError(clean, Error_notFound(0, 0, "File_move()::loc can't be found"))

	if(!File_hasFolder(resolvedFile))
		retError(clean, Error_notFound(0, 1, "File_move()::directoryName can't be found"))

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	CharString fileName = CharString_createNull();

	if(!CharString_cutBeforeLastSensitive(fileName, '/', &fileName))
		fileName = resolved;

	CharString_setAt(resolvedFile, CharString_length(resolvedFile) - 1, '/');

	gotoIfError2(clean, CharString_appendStringx(&resolvedFile, fileName))
	gotoIfError2(clean, CharString_appendx(&resolvedFile, '\0'))

	int ren = rename(resolved.ptr, resolvedFile.ptr);

	while(ren && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		ren = rename(resolved.ptr, resolvedFile.ptr);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(ren)
		retError(clean, Error_stderr(0, "File_move() couldn't move file"))

clean:
	FileInfo_freex(&info);
	CharString_freex(&resolved);
	CharString_freex(&resolvedFile);
	return s_uccess;
}

Bool FileHandle_createRef(const FileHandle *input, FileHandle *output, Error *e_rr) {

	Bool s_uccess = true;

	if(!input || !input->filePath.ptr || !output)
		retError(clean, Error_nullPointer(!output ? 1 : 0, "FileHandle_createRef()::input and output are required"))

	if(output->filePath.ptr)
		retError(clean, Error_invalidParameter(1, 0, "FileHandle_createRef()::output wasn't empty"))

	*output = *input;
	output->ownsHandle = false;

clean:
	return s_uccess;
}

Bool File_open(CharString loc, Ns maxTimeout, EFileOpenType type, Bool create, Allocator alloc, FileHandle *handle, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	FILE *f = NULL;

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_read()::loc must be a valid file path"))

	if(!handle)
		retError(clean, Error_nullPointer(2, "File_read()::handle is required"))

	if(handle->filePath.ptr)
		retError(clean, Error_invalidOperation(0, "File_read()::handle was filled, may indicate memleak"))

	if(File_isVirtual(loc))
		retError(clean, Error_invalidOperation(0, "File_open() is not permitted on virtual files, only physical"))

	Bool isVirtual = false;
	gotoIfError3(clean, File_resolve(loc, &isVirtual, 0, Platform_instance->workingDirectory, alloc, &resolved, e_rr))
	
	if(type == EFileOpenType_Write && create)
		gotoIfError3(clean, File_add(loc, EFileType_File, maxTimeout, true, alloc, e_rr))

	const C8 *mode = type == EFileOpenType_Read ? "rb" : "wb";
	f = fopen(resolved.ptr, mode);

	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);		//Try ~4x+ up to 1s of wait

	while (!f && maxTimeout) {

		Thread_sleep(maxTimeoutTry);
		f = fopen(resolved.ptr, mode);

		if(maxTimeout <= maxTimeoutTry)
			break;

		maxTimeout -= maxTimeoutTry;
	}

	if(!f)
		retError(clean, Error_stderr(0, "File_open() couldn't open file for read"))

	U64 fileSize = 0;

	if(type == EFileOpenType_Read) {

		if(fseek(f, 0, SEEK_END))
			retError(clean, Error_stderr(1, "File_open() couldn't seek end"))

		fileSize = (U64)_ftelli64(f);
	}

	*handle = (FileHandle) {
	
		.ext = f,

		.filePath = resolved,

		.type = (FileOpenType) type,
		.ownsHandle = true,

		.fileSize = fileSize
	};

	resolved = CharString_createNull();
	f = NULL;

clean:
	if(f) fclose(f);
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool File_openx(CharString loc, Ns timeout, EFileOpenType type, Bool create, FileHandle *handle, Error *e_rr) {
	return File_open(loc, timeout, type, create, Platform_instance->alloc, handle, e_rr);
}

void FileHandle_close(FileHandle *handle, Allocator alloc) {

	if(!handle)
		return;

	CharString_free(&handle->filePath, alloc);
	if(handle->ext && handle->ownsHandle) fclose((FILE*)handle->ext);
	*handle = (FileHandle) { 0 };
}

void FileHandle_closex(FileHandle *handle) {
	FileHandle_close(handle, Platform_instance->alloc);
}

Bool FileHandle_write(const FileHandle *handle, Buffer buf, U64 offset, U64 length, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!handle)
		retError(clean, Error_nullPointer(0, "FileHandle_write()::handle is required"))

	if(_fseeki64((FILE*)handle->ext, offset, SEEK_SET))
		retError(clean, Error_stderr(2, "FileHandle_write() couldn't seek begin"))

	if(length > Buffer_length(buf))
		retError(clean, Error_outOfBounds(2, length, Buffer_length(buf), "FileHandle_write() out of bounds"))

	if(!length)
		length = Buffer_length(buf);

	if(length && fwrite(buf.ptr, 1, length, (FILE*)handle->ext) != length)
		retError(clean, Error_stderr(1, "FileHandle_write() couldn't write file"))

clean:
	return s_uccess;
}

Bool FileHandle_read(const FileHandle *handle, U64 off, U64 len, Buffer output, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!handle)
		retError(clean, Error_nullPointer(0, "FileHandle_read()::handle is required"))

	if(!handle->fileSize && !off && !len)			//Empty files exist too
		goto clean;

	if(off >= handle->fileSize)
		retError(clean, Error_invalidOperation(0, "FileHandle_read() offset out of bounds"))

	U64 size = !len ? handle->fileSize - off : len;

	if(off + size > handle->fileSize)
		retError(clean, Error_invalidOperation(0, "FileHandle_read() offset + length out of bounds"))

	if(Buffer_isConstRef(output))
		retError(clean, Error_invalidOperation(0, "FileHandle_read() output must be writable"))

	if(_fseeki64((FILE*)handle->ext, off, SEEK_SET))
		retError(clean, Error_stderr(2, "FileHandle_read() couldn't seek begin"))

	if(size > Buffer_length(output))
		retError(clean, Error_outOfBounds(2, size, Buffer_length(output), "FileHandle_read() out of bounds"))

	if (fread((U8*)output.ptr, 1, size, (FILE*)handle->ext) != size)
		retError(clean, Error_stderr(3, "FileHandle_read() couldn't read file"))

clean:
	return s_uccess;
}

Bool File_write(Buffer buf, CharString loc, U64 off, U64 len, Ns maxTimeout, Bool createParent, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	FileHandle handle = (FileHandle) { 0 };

	if(File_isVirtual(loc)) {

		if(!CharString_isValidFilePath(loc))
			retError(clean, Error_invalidParameter(0, 0, "File_write()::loc must be a valid file path"))

		gotoIfError3(clean, File_writeVirtual(buf, loc, maxTimeout, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_open(loc, maxTimeout, EFileOpenType_Write, createParent, alloc, &handle, e_rr))
	gotoIfError3(clean, FileHandle_write(&handle, buf, off, len, e_rr))

clean:
	FileHandle_close(&handle, alloc);
	return s_uccess;
}

Bool File_writex(Buffer buf, CharString loc, U64 off, U64 len, Ns maxTimeout, Bool createParent, Error *e_rr) {
	return File_write(buf, loc, off, len, maxTimeout, createParent, Platform_instance->alloc, e_rr);
}

Bool File_read(CharString loc, Ns maxTimeout, U64 off, U64 len, Allocator alloc, Buffer *output, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocate = false;
	FileHandle handle = (FileHandle) { 0 };

	if(File_isVirtual(loc)) {
	
		if(!CharString_isValidFilePath(loc))
			retError(clean, Error_invalidParameter(0, 0, "File_read()::loc must be a valid file path"))

		if(!output)
			retError(clean, Error_nullPointer(2, "File_read()::output is required"))

		if(output->ptr)
			retError(clean, Error_invalidOperation(0, "File_read()::output was filled, may indicate memleak"))

		gotoIfError3(clean, File_readVirtual(loc, output, maxTimeout, e_rr))
		allocate = true;
		goto clean;
	}

	gotoIfError3(clean, File_open(loc, maxTimeout, EFileOpenType_Read, false, alloc, &handle, e_rr))

	if(!handle.fileSize && !off && !len)			//Empty files exist too
		goto clean;

	if(off >= handle.fileSize)
		retError(clean, Error_invalidOperation(0, "File_read() offset out of bounds"))

	U64 size = !len ? handle.fileSize - off : len;
	gotoIfError2(clean, Buffer_createUninitializedBytes(size, alloc, output))
	allocate = true;

	gotoIfError3(clean, FileHandle_read(&handle, off, size, *output, e_rr))

clean:

	if(!s_uccess && allocate)
		Buffer_free(output, alloc);

	FileHandle_close(&handle, alloc);
	return s_uccess;
}

Bool File_readx(CharString loc, Ns maxTimeout, U64 off, U64 len, Buffer *output, Error *e_rr) {
	return File_read(loc, maxTimeout, off, len, Platform_instance->alloc, output, e_rr);
}

//Virtual file support

typedef Bool (*VirtualFileFunc)(void *userData, CharString resolved, Error *e_rr);

Bool File_virtualOp(CharString loc, Ns maxTimeout, VirtualFileFunc f, void *userData, Bool isWrite, Error *e_rr) {

	(void)maxTimeout;

	Bool s_uccess = true;
	Bool isVirtual = false;
	CharString resolved = CharString_createNull();

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 128, &resolved, e_rr))

	if(!isVirtual)
		retError(clean, Error_unsupportedOperation(0, "File_virtualOp()::loc should resolve to virtual path (//*)"))

	const CharString access = CharString_createRefCStrConst("//access/");
	const CharString function = CharString_createRefCStrConst("//function/");
	const CharString network = CharString_createRefCStrConst("//network/");

	if (CharString_startsWithStringInsensitive(loc, access, 0)) {
		//TODO: Allow //access folder
		retError(clean, Error_unimplemented(0, "File_virtualOp()::loc //access/ not supported yet"))
	}

	if (CharString_startsWithStringInsensitive(loc, function, 0)) {
		//TODO: Allow //function folder (user callbacks)
		retError(clean, Error_unimplemented(1, "File_virtualOp()::loc //function/ not supported yet"))
	}

	if (CharString_startsWithStringInsensitive(loc, network, 0)) {
		//TODO: Allow //network folder (access to \\ on windows)
		retError(clean, Error_unimplemented(1, "File_virtualOp()::loc //network/ not supported yet"))
	}

	if(isWrite)
		retError(clean, Error_constData(
			1, 0,
			"File_virtualOp()::isWrite is disallowed when acting on virtual files "
			"(except //access/*, //network/* or //function/*)"
		))

	if(!f)
		retError(clean, Error_unimplemented(2, "File_virtualOp()::f is required"))

	gotoIfError3(clean, f(userData, resolved, e_rr))

clean:
	CharString_freex(&resolved);
	return s_uccess;
}

//These are write operations, we don't support them yet.

Bool File_removeVirtual(CharString loc, Ns maxTimeout, Error *e_rr) {
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, e_rr);
}

Bool File_addVirtual(CharString loc, EFileType type, Ns maxTimeout, Error *e_rr) {
	(void)type;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, e_rr);
}

Bool File_renameVirtual(CharString loc, CharString newFileName, Ns maxTimeout, Error *e_rr) {
	(void)newFileName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, e_rr);
}

Bool File_moveVirtual(CharString loc, CharString directoryName, Ns maxTimeout, Error *e_rr) {
	(void)directoryName;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, e_rr);
}

Bool File_writeVirtual(Buffer buf, CharString loc, Ns maxTimeout, Error *e_rr) {
	(void)buf;
	return File_virtualOp(loc, maxTimeout, NULL, NULL, true, e_rr);
}

//Read operations

Bool File_resolveVirtual(CharString loc, CharString *subPath, const VirtualSection **section, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();
	CharString copy1 = CharString_createNull();
	CharString copy2 = CharString_createNull();

	CharString_toLower(loc);
	gotoIfError2(clean, CharString_createCopyx(loc, &copy))
	gotoIfError2(clean, CharString_createCopyx(loc, &copy2))

	if(CharString_length(copy2))
		gotoIfError2(clean, CharString_appendx(&copy2, '/'))		//Don't append to root

	//Root is always present

	else goto clean;

	if(!SpinLock_isLockedForThread(&Platform_instance->virtualSectionsLock))
		retError(clean, Error_invalidState(0, "File_resolveVirtual() requires virtualSectionsLock"))

	//Check sections

	for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {

		const VirtualSection *sectioni = Platform_instance->virtualSections.ptr + i;

		//If folder is the same, we found a section.
		//This section won't return any subPath or section,
		//This will force it to be identified as a folder.

		if (CharString_equalsStringInsensitive(loc, sectioni->path))
			goto clean;

		//Parent folder.

		if(CharString_startsWithStringInsensitive(sectioni->path, copy2, 0))
			goto clean;

		//Check if the section includes the referenced file/folder

		CharString_freex(&copy1);
		gotoIfError2(clean, CharString_createCopyx(sectioni->path, &copy1))
		gotoIfError2(clean, CharString_appendx(&copy1, '/'))

		if(CharString_startsWithStringInsensitive(copy, copy1, 0)) {
			CharString_cut(copy, CharString_length(copy1), 0, subPath);
			*section = sectioni;
			goto clean;
		}
	}

	retError(clean, Error_notFound(0, 0, "File_resolveVirtual() can't find section"))

clean:

	CharString_freex(&copy1);
	CharString_freex(&copy2);

	if (CharString_length(*subPath) && s_uccess) {
		CharString_createCopyx(*subPath, &copy1);
		*subPath = copy1;
		copy1 = CharString_createNull();
	}

	CharString_freex(&copy);
	return s_uccess;
}

Bool File_readVirtualInternal(Buffer *output, CharString loc, Error *e_rr) {

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();
	const VirtualSection *section = NULL;
	Buffer tmp = Buffer_createNull();
	const ELockAcquire acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_readVirtualInternal() couldn't lock virtualSectionsLock"))

	gotoIfError3(clean, File_resolveVirtual(loc, &subPath, &section, e_rr))

	if(!section)
		retError(clean, Error_invalidOperation(0, "File_readVirtualInternal() section couldn't be found"))

	//Create copy of data.
	//It's possible the file data is unloaded in parallel and then this buffer would point to invalid data.

	gotoIfError3(clean, Archive_getFileDataConstx(section->loadedData, subPath, &tmp, e_rr))
	gotoIfError2(clean, Buffer_createCopyx(tmp, output))

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	Buffer_freex(&tmp);
	CharString_freex(&subPath);
	return s_uccess;
}

Bool File_readVirtual(CharString loc, Buffer *output, Ns maxTimeout, Error *e_rr) {

	if(!output) {
		if(e_rr) *e_rr = Error_nullPointer(1, "File_readVirtual()::output is required");
		return false;
	}

	return File_virtualOp(
		loc, maxTimeout,
		(VirtualFileFunc) File_readVirtualInternal,
		output,
		false,
		e_rr
	);
}

Bool File_getInfoVirtualInternal(FileInfo *info, CharString loc, Error *e_rr) {

	Bool s_uccess = true;
	CharString subPath = CharString_createNull();
	const VirtualSection *section = NULL;
	const ELockAcquire acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_getInfoVirtualInternal() couldn't lock virtualSectionsLock"))

	gotoIfError3(clean, File_resolveVirtual(loc, &subPath, &section, e_rr))

	if(!section) {	//Parent dir

		if(!CharString_length(loc))
			loc = CharString_createRefCStrConst(".");

		CharString copy = CharString_createNull();
		gotoIfError2(clean, CharString_createCopyx(loc, &copy))

		*info = (FileInfo) {
			.path = copy,
			.type = EFileType_Folder,
			.access = EFileAccess_Read
		};
	}

	else {

		gotoIfError3(clean, Archive_getInfox(section->loadedData, subPath, info, e_rr))

		CharString tmp = CharString_createNull();
		CharString_cut(loc, 0, CharString_length(loc) - CharString_length(subPath), &tmp);

		gotoIfError2(clean, CharString_insertStringx(&info->path, tmp, 0))
	}

	gotoIfError2(clean, CharString_insertStringx(&info->path, CharString_createRefCStrConst("//"), 0))

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	if(!s_uccess)
		FileInfo_freex(info);

	CharString_freex(&subPath);
	return s_uccess;
}

Bool File_getInfoVirtual(CharString loc, FileInfo *info, Error *e_rr) {

	Bool s_uccess = true;

	if(!info)
		retError(clean, Error_nullPointer(1, "File_getInfoVirtual()::info is required"))

	if(info->access)
		retError(clean, Error_invalidOperation(0, "File_getInfoVirtual()::info isn't empty, might indicate memleak"))

	gotoIfError3(clean, File_virtualOp(
		loc, 1 * SECOND,
		(VirtualFileFunc) File_getInfoVirtualInternal,
		info,
		false,
		e_rr
	))

clean:
	return s_uccess;
}

typedef struct ForeachFile {
	FileCallback callback;
	void *userData;
	Bool isRecursive;
	CharString currentPath;
} ForeachFile;

Bool File_virtualCallback(FileInfo info, ForeachFile *userData, Error *e_rr) {

	Bool s_uccess = true;
	CharString fullPath = CharString_createNull();

	gotoIfError2(clean, CharString_createCopyx(userData->currentPath, &fullPath))
	gotoIfError2(clean, CharString_appendStringx(&fullPath, info.path))
	gotoIfError2(clean, CharString_insertStringx(&fullPath, CharString_createRefCStrConst("//"), 0))

	info.path = fullPath;

	gotoIfError3(clean, userData->callback(info, userData->userData, e_rr))

clean:
	CharString_freex(&fullPath);
	return s_uccess;
}

Bool File_foreachVirtualInternal(ForeachFile *userData, CharString resolved, Error *e_rr) {

	Bool s_uccess = true;
	CharString copy = CharString_createNull();
	CharString copy1 = CharString_createNull();
	CharString copy2 = CharString_createNull();
	CharString copy3 = CharString_createNull();
	CharString root = CharString_createRefCStrConst(".");
	ListCharString visited = (ListCharString) { 0 };
	ELockAcquire acq = ELockAcquire_Invalid;

	CharString_toLower(resolved);
	gotoIfError2(clean, CharString_createCopyx(resolved, &copy))

	if(CharString_length(copy))
		gotoIfError2(clean, CharString_appendx(&copy, '/'))		//Don't append to root

	acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_unloadVirtualInternal() couldn't lock virtualSectionsLock"))

	U64 baseCount = CharString_countAllSensitive(copy, '/', 0);

	for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {

		const VirtualSection *section = Platform_instance->virtualSections.ptr + i;

		CharString_freex(&copy1);
		CharString_freex(&copy2);

		gotoIfError2(clean, CharString_createCopyx(section->path, &copy1))
		CharString_toLower(copy1);

		gotoIfError2(clean, CharString_createCopyx(copy1, &copy2))
		gotoIfError2(clean, CharString_appendx(&copy2, '/'))

		if(
			!CharString_startsWithStringSensitive(copy1, copy, 0) &&
			!CharString_equalsStringSensitive(copy1, resolved) &&
			!CharString_startsWithStringSensitive(copy, copy2, 0)
		)
			continue;

		//Root directories don't exist, so we have to pretend they do

		if (!CharString_length(resolved)) {

			CharString parent = CharString_createNull();
			if(!CharString_cutAfterFirstSensitive(copy1, '/', &parent))
				retError(clean, Error_invalidState(0, "File_foreachVirtualInternal() cutAfterFirst failed"))

			Bool contains = false;

			for(U64 j = 0; j < visited.length; ++j)
				if (CharString_equalsStringSensitive(parent, visited.ptr[j])) {
					contains = true;
					break;
				}

			if (!contains) {

				//Avoid duplicates

				gotoIfError2(clean, ListCharString_pushBackx(&visited, parent))

				CharString_freex(&copy3);
				gotoIfError2(clean, CharString_createCopyx(parent, &copy3))
				gotoIfError2(clean, CharString_insertStringx(&copy3, CharString_createRefCStrConst("//"), 0))

				FileInfo info = (FileInfo) {
					.path = copy3,
					.type = EFileType_Folder,
					.access = EFileAccess_Read
				};

				gotoIfError3(clean, userData->callback(info, userData->userData, e_rr))
			}
		}

		//All folders

		if (
			(!userData->isRecursive && baseCount == CharString_countAllSensitive(section->path, '/', 0)) ||
			(userData->isRecursive && baseCount <= CharString_countAllSensitive(section->path, '/', 0))
		) {

			CharString_freex(&copy3);
			gotoIfError2(clean, CharString_createCopyx(copy1, &copy3))
			gotoIfError2(clean, CharString_insertStringx(&copy3, CharString_createRefCStrConst("//"), 0))

			FileInfo info = (FileInfo) {
				.path = copy3,
				.type = EFileType_Folder,
				.access = EFileAccess_Read
			};

			gotoIfError3(clean, userData->callback(info, userData->userData, e_rr))
		}

		//We can only loop through the folders if they're loaded. Otherwise, we consider them as empty.

		if(section->loaded) {

			userData->currentPath = copy2;

			if(userData->isRecursive || baseCount >= 2) {

				CharString child = CharString_createNull();

				if(baseCount > 2 && !CharString_cut(resolved, CharString_length(copy2), 0, &child))
					retError(clean, Error_invalidState(1, "File_foreachVirtualInternal() cut failed"))

				if(!CharString_length(child))
					child = root;

				gotoIfError3(clean, Archive_foreachx(
					section->loadedData,
					child,
					(FileCallback) File_virtualCallback, userData,
					userData->isRecursive,
					EFileType_Any,
					e_rr
				))
			}
		}
	}

	retError(clean, Error_unimplemented(0, "File_foreachVirtualInternal() couldn't find virtual section"))

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	ListCharString_freex(&visited);
	CharString_freex(&copy);
	CharString_freex(&copy1);
	CharString_freex(&copy2);
	CharString_freex(&copy3);
	return s_uccess;
}

Bool File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr) {

	if(!callback) {
		if(e_rr) *e_rr = Error_nullPointer(1, "File_foreachVirtual()::callback is required");
		return false;
	}

	ForeachFile foreachFile = (ForeachFile) {
		.callback = callback,
		.userData = userData,
		.isRecursive = isRecursive
	};

	return File_virtualOp(
		loc, 1 * SECOND,
		(VirtualFileFunc) File_foreachVirtualInternal,
		&foreachFile,
		false,
		e_rr
	);
}

Bool countFileType(FileInfo info, FileCounter *counter, Error *e_rr) {

	(void) e_rr;

	if (!counter->useType) {
		++counter->counter;
		return false;
	}

	if(info.type == counter->type)
		++counter->counter;

	return false;
}

Bool File_queryFileObjectCountVirtual(CharString loc, EFileType type, Bool isRecursive, U64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(3, "File_queryFileObjectCountVirtual()::res is required"))

	FileCounter counter = (FileCounter) { .type = type, .useType = true };
	gotoIfError3(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive, e_rr))
	*res = counter.counter;

clean:
	return s_uccess;
}

Bool File_queryFileObjectCountAllVirtual(CharString loc, Bool isRecursive, U64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if(!res)
		retError(clean, Error_nullPointer(2, "File_queryFileObjectCountAllVirtual()::res is required"))

	FileCounter counter = (FileCounter) { 0 };
	gotoIfError3(clean, File_foreachVirtual(loc, (FileCallback) countFileType, &counter, isRecursive, e_rr))
	*res = counter.counter;

clean:
	return s_uccess;
}

impl Bool File_loadVirtualInternal1(FileLoadVirtual *userData, CharString loc, Bool allowLoad, Error *e_rr);

Bool File_loadVirtualInternal(FileLoadVirtual *userData, CharString loc, Error *e_rr) {
	return File_loadVirtualInternal1(userData, loc, true, e_rr);
}

Bool File_unloadVirtualInternal(void *userData, CharString loc, Error *e_rr) {

	(void)userData;

	CharString isChild = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;
	Bool s_uccess = true;

	gotoIfError2(clean, CharString_createCopyx(loc, &isChild))

	if(CharString_length(isChild))
		gotoIfError2(clean, CharString_appendx(&isChild, '/'))		//Don't append to root

	acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_unloadVirtualInternal() couldn't lock virtualSectionsLock"))

	for (U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {

		VirtualSection *section = Platform_instance->virtualSections.ptrNonConst + i;

		if(
			!CharString_equalsStringInsensitive(loc, section->path) &&
			!CharString_startsWithStringInsensitive(section->path, isChild, 0)
		)
			continue;

		if(section->loaded)
			Archive_freex(&section->loadedData);
	}

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	CharString_freex(&isChild);
	return s_uccess;
}

Bool File_isVirtualLoaded(CharString loc, Error *e_rr) {
	FileLoadVirtual virt = (FileLoadVirtual) { 0 };
	return File_loadVirtualInternal1(&virt, loc, false, e_rr);
}

Bool File_loadVirtual(CharString loc, const U32 encryptionKey[8], Error *e_rr) {
	FileLoadVirtual virt = (FileLoadVirtual) { .doLoad = true, .encryptionKey = encryptionKey };
	return File_virtualOp(loc, 1 * SECOND, (VirtualFileFunc) File_loadVirtualInternal, &virt, false, e_rr);
}

Bool File_unloadVirtual(CharString loc, Error *e_rr) {
	return File_virtualOp(loc, 1 * SECOND, File_unloadVirtualInternal, NULL, false, e_rr);
}
