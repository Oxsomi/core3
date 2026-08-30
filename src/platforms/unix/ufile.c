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

//platforms/unix/ufile.c

#include "platforms/file.h"
#include "platforms/platform.h"
#include "platforms/logx.h"
#include "types/container/string.h"
#include "types/container/memory_stream.h"
#include "types/base/string_base.h"
#include "types/base/string_read_helper.h"
#include "types/base/mathi.h"

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>

//Convert struct timespec (nanoseconds since Unix epoch) to Ns
static inline Ns Ns_fromTimespec(struct timespec ts) {
	return (Ns)ts.tv_sec * SECOND + (Ns)ts.tv_nsec;
}

//Helper: retry loop
#define FILE_RETRY_LOOP(maxTimeout, expr)                                           \
	{                                                                               \
		Ns _maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);             \
		while((expr) && maxTimeout) {                                               \
			Thread_sleep(_maxTimeoutTry);                                           \
			if(maxTimeout <= _maxTimeoutTry) { maxTimeout = 0; break; }             \
			maxTimeout -= _maxTimeoutTry;                                           \
		}                                                                           \
	}

Bool File_getInfoPhysical(const CharString *str, FileInfo *info, const Allocator *alloc, Error *e_rr) {
 
	Bool s_uccess = true;
 
	struct stat st;
	if(stat(str->ptr, &st) != 0) {
		
		if(errno == ENOENT || errno == ENOTDIR)
			retError(clean, Error_notFound(0, 0, "File_getInfoPhysical() path not found"));

		retError(clean, Error_platformError(0, errno, "File_getInfoPhysical() stat failed"));
	}
 
	EFileType type = S_ISDIR(st.st_mode) ? EFileType_Folder : EFileType_File;
	U64 fileSize   = type == EFileType_File ? (U64)st.st_size : 0;
 
	CharString path = CharString_createNull();
	gotoIfError3(clean, CharString_createCopy(*str, alloc, &path, e_rr));
 
	#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
		Ns timestamp = Ns_fromTimespec(st.st_mtimespec);
	#else
		Ns timestamp = Ns_fromTimespec(st.st_mtim);
	#endif
 
	*info = (FileInfo) {
		.type      = type,
		.path      = path,
		.timestamp = timestamp,
		.fileSize  = fileSize
	};
 
clean:
	return s_uccess;
}

Bool File_addPhysical(const CharString *str, Bool isFile, const Allocator *alloc, Error *e_rr) {
 
	(void)alloc;
	Bool s_uccess = true;
 
	if(!str)
		retError(clean, Error_nullPointer(0, "File_addPhysical() str is required"));
 
	if(isFile) {
 
		int fd = open(str->ptr, O_WRONLY | O_CREAT | O_EXCL, 0666);
 
		if(fd < 0) {

			if(errno == EEXIST)
				retError(clean, Error_alreadyDefined(0, "File_addPhysical() file already exists"));

			retError(clean, Error_platformError(0, errno, "File_addPhysical() couldn't create file"));
		}
 
		close(fd);
 
	} else if(mkdir(str->ptr, 0777) != 0) {

		if(errno == EEXIST)
			retError(clean, Error_alreadyDefined(0, "File_addPhysical() folder already exists"));

		retError(clean, Error_platformError(0, errno, "File_addPhysical() couldn't create directory"));
	}
 
clean:
	return s_uccess;
}

static Bool File_removeDirRecursivePhysical(const char *path, Ns *maxTimeout, Error *e_rr) {
 
	Bool s_uccess = true;
 
	DIR *dir = opendir(path);
	if(!dir)
		retError(clean, Error_platformError(0, errno, "File_removePhysical() opendir failed"));
 
	struct dirent *entry;
	while((entry = readdir(dir)) != NULL) {
 
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
 
		//Build child path
		C8 child[MAX_OXC_PATH + 1];
		if(snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= MAX_OXC_PATH) {
			Log_warnLnx("File_foreach()::path out of bounds. Skipping...");
			continue;
		}
 
		struct stat st;
		if(lstat(child, &st) != 0)
			retError(clean, Error_platformError(1, errno, "File_removePhysical() lstat failed"));
 
		if(S_ISDIR(st.st_mode)) {
 
			gotoIfError3(clean, File_removeDirRecursivePhysical(child, maxTimeout, e_rr));
 
			Bool res = false;
			FILE_RETRY_LOOP(*maxTimeout, (res = (rmdir(child) == 0)) == false);
 
			if(!res)
				retError(clean, Error_platformError(1, errno, "File_removePhysical() rmdir failed"));
 
		} else {
 
			Bool res = false;
			FILE_RETRY_LOOP(*maxTimeout, (res = (unlink(child) == 0)) == false);
 
			if(!res)
				retError(clean, Error_platformError(2, errno, "File_removePhysical() unlink failed"));
		}
	}
 
clean:
	if(dir) closedir(dir);
	return s_uccess;
}

Bool File_removePhysical(const CharString *str, Ns maxTimeout, const Allocator *alloc, Error *e_rr) {
 
	(void)alloc;
	Bool s_uccess = true;
 
	if(!str)
		retError(clean, Error_nullPointer(0, "File_removePhysical() str is required"));
 
	struct stat st;
	if(lstat(str->ptr, &st) != 0)
		retError(clean, Error_notFound(0, 0, "File_removePhysical() path not found"));
 
	if(S_ISDIR(st.st_mode)) {
 
		gotoIfError3(clean, File_removeDirRecursivePhysical(str->ptr, &maxTimeout, e_rr));
 
		Bool res = false;
		FILE_RETRY_LOOP(maxTimeout, (res = (rmdir(str->ptr) == 0)) == false);
 
		if(!res)
			retError(clean, Error_platformError(0, errno, "File_removePhysical() rmdir failed"));
 
	} else {
 
		Bool res = false;
		FILE_RETRY_LOOP(maxTimeout, (res = (unlink(str->ptr) == 0)) == false);
 
		if(!res)
			retError(clean, Error_platformError(0, errno, "File_removePhysical() unlink failed"));
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
 
	Bool res = false;
	FILE_RETRY_LOOP(maxTimeout, (res = (rename(loc->ptr, dest.ptr) == 0)) == false);
 
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
 
	Bool res = false;
	FILE_RETRY_LOOP(maxTimeout, (res = (rename(loc->ptr, dest->ptr) == 0)) == false);
 
	//rename(2) fails across file-system boundaries (EXDEV); fall back to copy + delete
	if(!res && errno == EXDEV) {
 
		int src = open(loc->ptr, O_RDONLY);
		if(src < 0)
			retError(clean, Error_platformError(0, errno, "File_movePhysical() open src failed"));
 
		struct stat st;
		if(fstat(src, &st) != 0) {
			close(src);
			retError(clean, Error_platformError(1, errno, "File_movePhysical() fstat failed"));
		}
 
		int dst = open(dest->ptr, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
		if(dst < 0) {
			close(src);
			retError(clean, Error_platformError(2, errno, "File_movePhysical() open dst failed"));
		}
 
		char buf[65536];
		ssize_t n;
		Bool ok = true;
		while((n = read(src, buf, sizeof(buf))) > 0) {
			if(write(dst, buf, (size_t)n) != n) { ok = false; break; }
		}
 
		close(src);
		close(dst);
 
		if(!ok || n < 0) {
			unlink(dest->ptr);
			retError(clean, Error_stderr(3, "File_movePhysical() cross-device copy failed"));
		}
 
		if(unlink(loc->ptr) != 0)
			retError(clean, Error_platformError(4, errno, "File_movePhysical() unlink src after copy failed"));
 
	} else if(!res)
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
 
	Bool isRead  = type == EFileOpenType_Read  || type == EFileOpenType_ReadWrite;
	Bool isWrite = type == EFileOpenType_Write || type == EFileOpenType_ReadWrite;
 
	int flags;
	if(isRead && isWrite)  flags = O_RDWR    | O_CREAT;
	else if(isWrite)       flags = O_WRONLY  | O_CREAT | O_TRUNC;
	else                   flags = O_RDONLY;
 
	int fd = open(resolved->ptr, flags, 0666);
 
	Ns maxTimeoutTry = U64_min((maxTimeout + 7) >> 2, 1 * SECOND);
	while(fd < 0 && maxTimeout) {
		Thread_sleep(maxTimeoutTry);
		fd = open(resolved->ptr, flags, 0666);
		if(maxTimeout <= maxTimeoutTry) { maxTimeout = 0; break; }
		maxTimeout -= maxTimeoutTry;
	}
 
	if(fd < 0)
		retError(clean, Error_stderr(0, "File_openPhysical() couldn't open file"));
 
	U64 fileSize = 0;
	if(isRead) {
		
		struct stat st;
		if(fstat(fd, &st) != 0) {
			close(fd);
			retError(clean, Error_stderr(1, "File_openPhysical() fstat failed"));
		}

		fileSize = (U64)st.st_size;
	}
 
	*fileHandle = (FileHandle) {
		.ext          = (void*)(intptr_t)fd,
		.fileSizeType = FileHandle_makeFileSizeType(fileSize, EFileOpenType_create(isRead, isWrite))
	};
 
clean:
	return s_uccess;
}

Bool FileHandle_writePhysical(FileHandle *handle, U64 offset, U64 length, const Buffer *buf, Error *e_rr) {
 
	Bool s_uccess = true;
 
	if(!handle || !buf)
		retError(clean, Error_nullPointer(!handle ? 0 : 3, "FileHandle_writePhysical() handle and buf are required"));
 
	int fd = (int)(intptr_t)handle->ext;
 
	const U8 *src     = buf->ptr;
	U64 remaining     = length;
	U64 currentOffset = offset;
 
	while(remaining) {
 
		U64 toWrite = U64_min(remaining, 64 * MIBI);        //64 MB chunks
		ssize_t wrote = pwrite(fd, src, (size_t)toWrite, (off_t)currentOffset);
 
		if(wrote < 0 || (U64)wrote != toWrite)
			retError(clean, Error_stderr(1, "FileHandle_writePhysical() pwrite failed"));
 
		src           += wrote;
		currentOffset += (U64)wrote;
		remaining     -= (U64)wrote;
	}
 
clean:
	return s_uccess;
}

Bool FileHandle_readPhysical(FileHandle *handle, U64 offset, U64 length, Buffer *buf, Error *e_rr) {
 
	Bool s_uccess = true;
 
	if(!handle || !buf)
		retError(clean, Error_nullPointer(!handle ? 0 : 3, "FileHandle_readPhysical() handle and buf are required"));
 
	int fd = (int)(intptr_t)handle->ext;
 
	U8 *dst           = buf->ptrNonConst;
	U64 remaining     = length;
	U64 currentOffset = offset;
 
	while(remaining) {
 
		U64 toRead = U64_min(remaining, 64 * MIBI);        //64 MB chunks
		ssize_t got = pread(fd, dst, (size_t)toRead, (off_t)currentOffset);
 
		if(got < 0 || (U64)got != toRead)
			retError(clean, Error_stderr(1, "FileHandle_readPhysical() pread failed"));
 
		dst           += got;
		currentOffset += (U64)got;
		remaining     -= (U64)got;
	}
 
clean:
	return s_uccess;
}

void FileHandle_closePhysical(const void *ext, const Allocator *alloc) {
 
	(void)alloc;
 
	if(!ext)
		return;
 
	int fd = (int)(intptr_t)ext;
	if(fd >= 0)
		close(fd);
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
 
	CharString resolved       = CharString_createNull();
	CharString resolvedNoStar = CharString_createNull();
	CharString tmp            = CharString_createNull();
	CharString tmp2           = CharString_createNull();
	DIR *dir                  = NULL;
 
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
 
	//Build resolvedNoStar = "path/"
	gotoIfError3(clean, CharString_append(&resolved, '/', alloc, e_rr));
	gotoIfError3(clean, CharString_createCopy(resolved, alloc, &resolvedNoStar, e_rr));
 
	dir = opendir(resolved.ptr);
	if(!dir)
		retError(clean, Error_notFound(0, 0, "File_foreach()::loc couldn't be found"));
 
	struct dirent *entry;
	while((entry = readdir(dir)) != NULL) {
 
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
 
		//Build full path: resolvedNoStar + filename
		CharString_free(&tmp, alloc);
		CharString_free(&tmp2, alloc);
 
		gotoIfError3(clean, CharString_createCopy(resolvedNoStar, alloc, &tmp, e_rr));
		tmp2 = CharString_createRefCStrConst(entry->d_name);
		gotoIfError3(clean, CharString_appendString(&tmp, &tmp2, alloc, e_rr));
 
		if(CharString_length(tmp) > MAX_OXC_PATH) {
 
			Log_warnLn(alloc, "File_foreach()::path out of bounds (%.*s). Skipping...",
				(int) CharString_length(tmp), tmp.ptr
			);
 
			continue;
		}
 
		CharString_free(&tmp2, alloc);
 
		struct stat st;
		if(lstat(tmp.ptr, &st) != 0)
			continue;        //best effort; skip entries we cannot stat
 
		#if _PLATFORM_TYPE == PLATFORM_OSX || _PLATFORM_TYPE == PLATFORM_IOS
			Ns timestamp = Ns_fromTimespec(st.st_mtimespec);
		#else
			Ns timestamp = Ns_fromTimespec(st.st_mtim);
		#endif
 
		EFileAccess access = (st.st_mode & S_IWUSR) ? EFileAccess_ReadWrite : EFileAccess_Read;
 
		if(S_ISDIR(st.st_mode)) {
 
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
 
			FileInfo info = (FileInfo) {
				.path      = tmp,
				.timestamp = timestamp,
				.access    = access,
				.type      = EFileType_File,
				.fileSize  = (U64)st.st_size
			};
 
			gotoIfError3(clean, callback(&info, userData, alloc, e_rr));
		}
	}
 
clean:
 
	if(dir) closedir(dir);
 
	CharString_free(&tmp, alloc);
	CharString_free(&tmp2, alloc);
	CharString_free(&resolvedNoStar, alloc);
	CharString_free(&resolved, alloc);
 
	return s_uccess;
}

//Virtual files

//Android and web provide their own (afile.c / webfile.c): this variant reads section bytes that
//Platform_initUnixExt mapped from the executable itself, which neither platform can do.

#if _PLATFORM_TYPE != PLATFORM_ANDROID && _PLATFORM_TYPE != PLATFORM_WEB

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

			//Not loading, just verify presence
			if(!userData->doLoad) {

				if(!section->loadedAndId)
					retError(clean, Error_notFound(1, 1, "File_loadVirtualInternal1()::loc not found"));

				foundAny = true;
				continue;
			}

			//Already loaded, nothing to do
			if(section->loadedAndId) {
				foundAny = true;
				continue;
			}

			if(!allowLoad)
				retError(clean, Error_notFound(0, 0, "File_loadVirtualInternal1() was queried but none was found"));

			//The ELF/MACH section is already mmap'd by Platform_initUnixExt into section->dataExt / section->lenExt.
			//Wrap in a read-only Buffer and parse the embedded CAFile, no FindResource needed unlike Windows.

			Buffer buf = Buffer_createRefConst(section->dataExt, section->lenExt);

			gotoIfError3(clean, MemoryStream_createFromBuffer(
				&buf, EMemoryStreamFlags_None, memoryStreamType, &memoryStream, e_rr
			));

			gotoIfError3(clean, CAFile_read(memoryStream, encStreamType, 0, userData->encryptionKey, alloc, &caFile, e_rr));
			RefPtr_dec(&memoryStream);

			//Push the parsed archive into the global list and record its index.
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

#endif
