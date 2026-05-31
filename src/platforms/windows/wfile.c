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

//platforms/windows/wfile.c

#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "formats/oiCA/ca_file.h"
#include "types/container/string_unicode.h"
#include "types/container/list_basic_types.h"
#include "types/container/memory_stream.h"
#include "types/base/thread.h"
#include "types/base/mathi.h"
#include "types/base/buffer.h"
#include "types/base/string_mut_helper.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/constants.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
#include <Windows.h>
#include <stdio.h>

#define WIN_PATH_MAX (1024 + 4)        // 1023 usable + null + \\?\
 
//UTF-8 -> UTF-16, / -> \, prepend \\?\ (assumes that buf is WIN_PATH_MAX)
Bool CharString_toLongPath(wchar_t *buf, const CharString *str, Error *e_rr) {

	Bool s_uccess = true;

	buf[0] = L'\\'; buf[1] = L'\\'; buf[2] = L'?'; buf[3] = L'\\';

	ListU16 utf16 = (ListU16) { 0 };
	gotoIfError3(clean, ListU16_createRef((U16*)buf + 4, WIN_PATH_MAX - 4, &utf16, e_rr));

	gotoIfError3(clean, CharString_toUTF16(*str, NULL, &utf16, e_rr));
	
	for(U64 i = 0; i < utf16.length; ++i)
		if(utf16.ptr[i] == L'/')
			utf16.ptrNonConst[i] = L'\\';

clean:
	return s_uccess;
}

//FILETIME is 100-nanosecond intervals since Jan 1 1601
//Convert to Unix epoch (Jan 1 1970) = 11644473600 seconds offset
static Ns Ns_fromFileTime(FILETIME ft) {
	U64 ticks = ((U64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
	ticks -= 116444736000000000ULL;
	return ticks * 100;
}

//Helper: retry loop
#define FILE_RETRY_LOOP(maxTimeout, expr)                                            \
	{                                                                                \
		Ns _maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);                \
		while((expr) && maxTimeout) {                                                \
			Thread_sleep(_maxTimeoutTry);                                            \
			if(maxTimeout <= _maxTimeoutTry) { maxTimeout = 0; break; }                \
			maxTimeout -= _maxTimeoutTry;                                            \
		}                                                                            \
	}

Bool File_getInfoPhysical(const CharString *str, FileInfo *info, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
 
	wchar_t buf[WIN_PATH_MAX];
	gotoIfError3(clean, CharString_toLongPath(buf, str, e_rr));
 
	WIN32_FILE_ATTRIBUTE_DATA data;
	if(!GetFileAttributesExW(buf, GetFileExInfoStandard, &data)) {

		DWORD err = GetLastError();
		if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
			retError(clean, Error_notFound(0, 0, "File_getInfoPhysical() path not found"));

		retError(clean, Error_platformError(0, err, "File_getInfoPhysical() GetFileAttributesExW failed"));
	}
 
	EFileType type = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? EFileType_Folder : EFileType_File;
 
	U64 fileSize = type == EFileType_File ? (((U64)data.nFileSizeHigh << 32) | data.nFileSizeLow) : 0;
 
	CharString path = CharString_createNull();
	gotoIfError3(clean, CharString_createCopy(*str, alloc, &path, e_rr));
 
	*info = (FileInfo) {
		.type         = type,
		.path         = path,
		.timestamp    = Ns_fromFileTime(data.ftLastWriteTime),
		.fileSize     = fileSize
	};
 
clean:
	return s_uccess;
}

Bool File_addPhysical(const CharString *str, Bool isFile, const Allocator *alloc, Error *e_rr) {

	(void)alloc;
	Bool s_uccess = true;
 
	if(!str)
		retError(clean, Error_nullPointer(0, "File_addPhysical() str is required"));
 
	wchar_t buf[WIN_PATH_MAX];
	gotoIfError3(clean, CharString_toLongPath(buf, str, e_rr));
 
	if(isFile) {

		HANDLE h = CreateFileW(
			buf, GENERIC_WRITE, 0, NULL,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL
		);

		if(h == INVALID_HANDLE_VALUE) {

			DWORD err = GetLastError();

			if(err == ERROR_FILE_EXISTS)
				retError(clean, Error_alreadyDefined(0, "File_addPhysical() file already exists"));

			retError(clean, Error_platformError(0, err, "File_addPhysical() couldn't create file"));
		}

		CloseHandle(h);

	} else if(!CreateDirectoryW(buf, NULL)) {

		DWORD err = GetLastError();

		if(err == ERROR_ALREADY_EXISTS)
			retError(clean, Error_alreadyDefined(0, "File_addPhysical() folder already exists"));

		retError(clean, Error_platformError(0, err, "File_addPhysical() couldn't create directory"));
	}
 
clean:
	return s_uccess;
}

//Recursively remove a directory and all its contents
static Bool File_removeDirRecursivePhysical(const wchar_t *path, Ns *maxTimeout, Error *e_rr) {

	Bool s_uccess = true;
 
	//Build glob: path\*
	wchar_t glob[WIN_PATH_MAX + 2];
	wcsncpy_s(glob, sizeof(glob) / sizeof(wchar_t), path, WIN_PATH_MAX);
	wcsncat(glob, L"\\*", 2);
 
	WIN32_FIND_DATAW ffd;
	HANDLE hFind = FindFirstFileW(glob, &ffd);
	if(hFind == INVALID_HANDLE_VALUE)
		retError(clean, Error_platformError(0, GetLastError(), "File_removePhysical() FindFirstFileW failed"));
 
	do {

		if(wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0)
			continue;
 
		//Build full child path
		wchar_t child[WIN_PATH_MAX];
		_snwprintf(child, WIN_PATH_MAX, L"%ls\\%ls", path, ffd.cFileName);
 
		if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

			gotoIfError3(clean, File_removeDirRecursivePhysical(child, maxTimeout, e_rr));
			Bool res = false;
			FILE_RETRY_LOOP(*maxTimeout, (res = RemoveDirectoryW(child)) == false);

			if(!res)
				retError(clean, Error_platformError(1, GetLastError(), "File_removePhysical() RemoveDirectoryW failed"));

		} else {

			Bool res = false;
			FILE_RETRY_LOOP(*maxTimeout, (res = DeleteFileW(child)) == false);

			if(!res)
				retError(clean, Error_platformError(2, GetLastError(), "File_removePhysical() DeleteFileW failed"));
		}
 
	} while(FindNextFileW(hFind, &ffd));
 
clean:
	if(hFind != INVALID_HANDLE_VALUE) FindClose(hFind);
	return s_uccess;
}

Bool File_removePhysical(const CharString *str, Ns maxTimeout, const Allocator *alloc, Error *e_rr) {

	(void)alloc;
	Bool s_uccess = true;
 
	if(!str)
		retError(clean, Error_nullPointer(0, "File_removePhysical() str is required"));
 
	wchar_t buf[WIN_PATH_MAX];
	gotoIfError3(clean, CharString_toLongPath(buf, str, e_rr));
 
	DWORD attrs = GetFileAttributesW(buf);
	if(attrs == INVALID_FILE_ATTRIBUTES)
		retError(clean, Error_notFound(0, 0, "File_removePhysical() path not found"));
 
	if(attrs & FILE_ATTRIBUTE_DIRECTORY) {

		gotoIfError3(clean, File_removeDirRecursivePhysical(buf, &maxTimeout, e_rr));

		Bool res = false;
		FILE_RETRY_LOOP(maxTimeout, (res = RemoveDirectoryW(buf)) == false);

		if(!res)
			retError(clean, Error_platformError(0, GetLastError(), "File_removePhysical() RemoveDirectoryW failed"));

	} else {

		Bool res = false;
		FILE_RETRY_LOOP(maxTimeout, (res = DeleteFileW(buf)) == false);

		if(!res)
			retError(clean, Error_platformError(0, GetLastError(), "File_removePhysical() DeleteFileW failed"));
	}
 
clean:
	return s_uccess;
}

Bool File_renamePhysical(
	const CharString *loc,
	const CharString *newFileName,
	Ns maxTimeout,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	CharString dest = CharString_createNull();
 
	CharString parent = CharString_createNull();
	if(!CharString_cutAfterLastSensitive(loc, '/', &parent))
		parent = CharString_createRefCStrConst(".");
 
	gotoIfError3(clean, CharString_format(alloc, &dest, e_rr, "%.*s/%.*s",
		CharString_length(parent), parent.ptr,
		CharString_length(*newFileName), newFileName->ptr
	));
 
	wchar_t wSrc[WIN_PATH_MAX], wDst[WIN_PATH_MAX];
	gotoIfError3(clean, CharString_toLongPath(wSrc, loc, e_rr));
	gotoIfError3(clean, CharString_toLongPath(wDst, &dest, e_rr));
 
	Bool res = false;
	FILE_RETRY_LOOP(maxTimeout, (res = MoveFileExW(wSrc, wDst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) == false);
 
	if(!res)
		retError(clean, Error_stderr(0, "File_renamePhysical() rename failed"));
 
clean:
	CharString_free(&dest, alloc);
	return s_uccess;
}

Bool File_movePhysical(
	const CharString *loc,
	const CharString *dest,
	Ns maxTimeout,
	const Allocator *alloc,
	Error *e_rr
) {
	(void)alloc;
	Bool s_uccess = true;
 
	wchar_t wSrc[WIN_PATH_MAX], wDst[WIN_PATH_MAX];
	gotoIfError3(clean, CharString_toLongPath(wSrc, loc, e_rr));
	gotoIfError3(clean, CharString_toLongPath(wDst, dest, e_rr));
 
	Bool res = false;

	FILE_RETRY_LOOP(maxTimeout, (res = MoveFileExW(
		wSrc, wDst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH
	)) == false);
 
	if(!res)
		retError(clean, Error_stderr(0, "File_movePhysical() move failed"));
 
clean:
	return s_uccess;
}

Bool File_openPhysical(
	const CharString *resolved,
	Ns maxTimeout,
	EFileOpenType type,
	FileHandle *fileHandle,
	const Allocator *alloc,
	Error *e_rr
) {
	(void)alloc;
	Bool s_uccess = true;
 
	wchar_t buf[WIN_PATH_MAX];
	gotoIfError3(clean, CharString_toLongPath(buf, resolved, e_rr));

	Bool isRead  = type == EFileOpenType_Read || type == EFileOpenType_ReadWrite;
	Bool isWrite = type == EFileOpenType_Write || type == EFileOpenType_ReadWrite;
 
	DWORD access   = (isRead  ? GENERIC_READ  : 0) | (isWrite ? GENERIC_WRITE : 0);
	DWORD share    = isRead && !isWrite ? FILE_SHARE_READ : 0;    //no sharing for writable
	DWORD creation = isWrite ? (isRead ? OPEN_ALWAYS : CREATE_ALWAYS) : OPEN_EXISTING;
 
	HANDLE h = CreateFileW(buf, access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
 
	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);
	while(h == INVALID_HANDLE_VALUE && maxTimeout) {
		Thread_sleep(maxTimeoutTry);
		h = CreateFileW(buf, access, share, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
		if(maxTimeout <= maxTimeoutTry) { maxTimeout = 0; break; }
		maxTimeout -= maxTimeoutTry;
	}
 
	if(h == INVALID_HANDLE_VALUE)
		retError(clean, Error_stderr(0, "File_openPhysical() couldn't open file"));
 
	U64 fileSize = 0;
	if(isRead) {
		LARGE_INTEGER li;
		if(!GetFileSizeEx(h, &li)) {
			CloseHandle(h);
			retError(clean, Error_stderr(1, "File_openPhysical() GetFileSizeEx failed"));
		}
		fileSize = (U64)li.QuadPart;
	}
 
	*fileHandle = (FileHandle) {
		.ext          = (void*)h,
		.fileSizeType = FileHandle_makeFileSizeType(fileSize, EFileOpenType_create(isRead, isWrite))
	};
 
clean:
	return s_uccess;
}

Bool FileHandle_writePhysical(FileHandle *handle, U64 offset, U64 length, const Buffer *buf, Error *e_rr) {

	Bool s_uccess = true;
 
	if(!handle || !buf)
		retError(clean, Error_nullPointer(!handle ? 0 : 3, "FileHandle_writePhysical() handle and buf are required"));
 
	HANDLE h = (HANDLE)handle->ext;
 
	const U8 *src = buf->ptr;
	U64 remaining = length;
	U64 currentOffset = offset;
 
	while(remaining) {

		DWORD toWrite = (DWORD)U64_min(remaining, 64 * MIBI);        //64MB chunks
		DWORD wrote = 0;
		OVERLAPPED ov = { 0 };
		ov.Offset     = (DWORD)(currentOffset & U32_MAX);
		ov.OffsetHigh = (DWORD)(currentOffset >> 32);
		if(!WriteFile(h, src, toWrite, &wrote, &ov) || wrote != toWrite)
			retError(clean, Error_stderr(1, "FileHandle_writePhysical() write failed"));

		src           += wrote;
		currentOffset += wrote;
		remaining     -= wrote;
	}
 
clean:
	return s_uccess;
}

Bool FileHandle_readPhysical(FileHandle *handle, U64 offset, U64 length, Buffer *buf, Error *e_rr) {

	Bool s_uccess = true;
 
	if(!handle || !buf)
		retError(clean, Error_nullPointer(!handle ? 0 : 3, "FileHandle_readPhysical() handle and buf are required"));
 
	HANDLE h = (HANDLE)handle->ext;
 
	U8 *dst = buf->ptrNonConst;
	U64 remaining = length;
	U64 currentOffset = offset;
 
	while(remaining) {

		DWORD toRead = (DWORD) U64_min(remaining, 64 * MIBI);        //64MB chunks
		DWORD got = 0;
		OVERLAPPED ov = { 0 };
		ov.Offset     = (DWORD)(currentOffset & U32_MAX);
		ov.OffsetHigh = (DWORD)(currentOffset >> 32);
		if(!ReadFile(h, dst, toRead, &got, &ov) || got != toRead)
			retError(clean, Error_stderr(1, "FileHandle_readPhysical() read failed"));

		dst           += got;
		currentOffset += got;
		remaining     -= got;
	}
 
clean:
	return s_uccess;
}

void FileHandle_closePhysical(const void *ext, const Allocator *alloc) {

	(void)alloc;

	if(!ext || ext == INVALID_HANDLE_VALUE)
		return;

	CloseHandle((HANDLE)ext);
}

Bool File_foreachVirtual(
	const CharString *loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	const Allocator *alloc,
	Error *e_rr
);

Bool File_foreach(
	const CharString *loc,
	Bool inAppDir,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;

	CharString resolved        = CharString_createNull();
	CharString resolvedNoStar  = CharString_createNull();
	CharString tmp             = CharString_createNull();
	CharString tmp2            = CharString_createNull();
	HANDLE file                = INVALID_HANDLE_VALUE;

	if(!callback)
		retError(clean, Error_nullPointer(1, "File_foreach()::callback is required"));

	if(!loc || !CharString_isValidFilePath(*loc))
		retError(clean, Error_invalidParameter(0, 0, "File_foreach()::loc must be a valid file path"));

	Bool isVirtual = File_isVirtual(*loc);

	if(isVirtual) {
		gotoIfError3(clean, File_foreachVirtual(loc, callback, userData, isRecursive, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, File_resolve(
		loc,
		&isVirtual,
		0,
		inAppDir ? &Platform_instance->appDirectory : &Platform_instance->workDirectory,
		alloc,
		&resolved,
		e_rr
	));

	if(isVirtual)
		retError(clean, Error_invalidOperation(0, "File_foreach()::loc can't resolve to virtual here"));

	//Build resolvedNoStar = "path/" and resolved = "path/*"
	gotoIfError3(clean, CharString_append(&resolved, '/', alloc, e_rr));
	gotoIfError3(clean, CharString_createCopy(resolved, alloc, &resolvedNoStar, e_rr));
	gotoIfError3(clean, CharString_append(&resolved, '*', alloc, e_rr));

	{
		wchar_t wbuf[WIN_PATH_MAX];
		gotoIfError3(clean, CharString_toLongPath(wbuf, &resolved, e_rr));

		WIN32_FIND_DATAW dat;
		file = FindFirstFileW(wbuf, &dat);

		if(file == INVALID_HANDLE_VALUE)
			retError(clean, Error_notFound(0, 0, "File_foreach()::loc couldn't be found"));

		do {

			//Skip . and ..
			if(wcscmp(dat.cFileName, L".") == 0 || wcscmp(dat.cFileName, L"..") == 0)
				continue;

			Ns timestamp = Ns_fromFileTime(dat.ftLastWriteTime);

			//Build full path: resolvedNoStar + filename
			CharString_free(&tmp, alloc);
			CharString_free(&tmp2, alloc);

			gotoIfError3(clean, CharString_createCopy(resolvedNoStar, alloc, &tmp, e_rr));
			gotoIfError3(clean, CharString_createFromUTF16(dat.cFileName, MAX_OXC_PATH, alloc, &tmp2, e_rr));
			gotoIfError3(clean, CharString_appendString(&tmp, &tmp2, alloc, e_rr));

			if(CharString_length(tmp) > MAX_OXC_PATH) {

				Log_warnLn(alloc, "File_foreach()::path out of bounds (%.*s). Skipping...",
					(int) CharString_length(tmp), tmp.ptr
				);

				continue;
			}

			CharString_free(&tmp2, alloc);

			EFileAccess access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? EFileAccess_Read : EFileAccess_ReadWrite;

			if(dat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

				FileInfo info = (FileInfo) {
					.path      = tmp,
					.timestamp = timestamp,
					.access    = access,
					.type      = EFileType_Folder
				};

				gotoIfError3(clean, callback(&info, userData, alloc, e_rr));

				if(isRecursive)
					gotoIfError3(clean, File_foreach(&tmp, false, callback, userData, true, alloc, e_rr));

			} else {

				LARGE_INTEGER size;
				size.LowPart  = dat.nFileSizeLow;
				size.HighPart = dat.nFileSizeHigh;

				FileInfo info = (FileInfo) {
					.path      = tmp,
					.timestamp = timestamp,
					.access    = access,
					.type      = EFileType_File,
					.fileSize  = (U64)size.QuadPart
				};

				gotoIfError3(clean, callback(&info, userData, alloc, e_rr));
			}

		} while(FindNextFileW(file, &dat));

		DWORD err = GetLastError();
		if(err != ERROR_NO_MORE_FILES)
			retError(clean, Error_platformError(0, err, "File_foreach() FindNextFile failed"));
	}

clean:

	if(file != INVALID_HANDLE_VALUE)
		FindClose(file);

	CharString_free(&tmp, alloc);
	CharString_free(&tmp2, alloc);
	CharString_free(&resolvedNoStar, alloc);
	CharString_free(&resolved, alloc);

	return s_uccess;
}

Bool File_loadVirtualInternal1(
	FileLoadVirtual *userData,
	const CharString *loc,
	Bool allowLoad,
	const RefPtrType *memoryStreamType,
	const RefPtrType *encStreamType,
	const Allocator *alloc,
	Error *e_rr
) {
	Bool s_uccess = true;
	CharString isChild = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;
	CAFile caFile = (CAFile) { 0 };
	MemoryStreamRef *memoryStream = NULL;

	gotoIfError3(clean, CharString_createCopy(*loc, alloc, &isChild, e_rr));

	if(CharString_length(isChild))
		gotoIfError3(clean, CharString_append(&isChild, '/', alloc, e_rr));

	acq = SpinLock_lock(&Platform_instance->virtualSectionsLock, U64_MAX);
	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_loadVirtualInternal1() couldn't lock virtualSectionsLock"));

	Bool foundAny = false;

	for(U64 i = 0; i < Platform_instance->virtualSections.length; ++i) {

		VirtualSection *section = Platform_instance->virtualSections.ptrNonConst + i;

		if(
			!CharString_equalsStringInsensitive(loc, &section->path) &&
			!CharString_startsWithStringInsensitive(&section->path, &isChild, 0)
		)
			continue;

		//Not loading, just check presence
		if(!userData->doLoad) {

			if(!section->loadedAndId)
				retError(clean, Error_notFound(1, 1, "File_loadVirtualInternal1()::loc not found"));

			foundAny = true;
			continue;
		}

		//Already loaded
		if(section->loadedAndId) {
			foundAny = true;
			continue;
		}

		if(!allowLoad)
			retError(clean, Error_notFound(0, 0, "File_loadVirtualInternal1() was queried but none was found"));

		//Load from Windows resource
		
		HRSRC hrsrc = FindResourceA(NULL, section->path.ptr, MAKEINTRESOURCEA(10));
		if(!hrsrc)
			retError(clean, Error_notFound(0, 1, "File_loadVirtualInternal1() FindResource failed"));

		U32 size = (U32)SizeofResource(NULL, hrsrc);

		HGLOBAL hglobal = LoadResource(NULL, hrsrc);
		if(!hglobal)
			retError(clean, Error_notFound(3, 1, "File_loadVirtualInternal1() LoadResource failed"));

		//LockResource returns a pointer into the memory-mapped resource, no cleanup needed
		const U8 *dat = (const U8*)LockResource(hglobal);

		if(!dat)
			retError(clean, Error_notFound(3, 1, "File_loadVirtualInternal1() LockResource failed"));

		Buffer buf = Buffer_createRefConst(dat, size);

		gotoIfError3(clean, MemoryStream_createFromBuffer(
			&buf, EMemoryStreamFlags_None, memoryStreamType, &memoryStream, e_rr
		));

		gotoIfError3(clean, CAFile_read(memoryStream, encStreamType, 0, userData->encryptionKey, alloc, &caFile, e_rr));
		RefPtr_dec(&memoryStream);

		//Store archive, push into platform archives list and record index
		U64 archiveIndex = Platform_instance->archives.length;
		gotoIfError3(clean, ListCAFile_pushBack(&Platform_instance->archives, caFile, alloc, e_rr));
		caFile = (CAFile) { 0 };

		section->loadedAndId = archiveIndex | ((U64)1 << 63);
		foundAny = true;
	}

	if(!foundAny)
		retError(clean, Error_notFound(2, 1, "File_loadVirtualInternal1()::loc not found"));

clean:

	RefPtr_dec(&memoryStream);
	CAFile_free(&caFile, alloc);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	CharString_free(&isChild, alloc);
	return s_uccess;
}
