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

#include "platforms/ext/listx_impl.h"
#include "types/error.h"
#include "types/buffer.h"
#include "formats/oiCA.h"
#include "platforms/file.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/formatx.h"
#include "platforms/ext/bufferx.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

Bool File_foreachVirtual(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr);

Bool File_foreach(CharString loc, FileCallback callback, void *userData, Bool isRecursive, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	CharString resolvedNoStar = CharString_createNull();
	CharString tmp = CharString_createNull();
	CharString tmp2 = CharString_createNull();
	ListU16 tmpWStr = (ListU16) { 0 };
	HANDLE file = NULL;

	if(!callback)
		retError(clean, Error_nullPointer(1, "File_foreach()::callback is required"))

	if(!CharString_isValidFilePath(loc))
		retError(clean, Error_invalidParameter(0, 0, "File_foreach()::loc must be a valid file path"))

	Bool isVirtual = File_isVirtual(loc);

	if(isVirtual) {
		gotoIfError3(clean, File_foreachVirtual(loc, callback, userData, isRecursive, e_rr))
		goto clean;
	}

	gotoIfError3(clean, File_resolvex(loc, &isVirtual, 0, &resolved, e_rr))

	if(isVirtual)
		retError(clean, Error_invalidOperation(0, "File_foreach()::loc can't resolve to virtual here"))

	//Append /*

	gotoIfError2(clean, CharString_appendx(&resolved, '/'))

	gotoIfError2(clean, CharString_createCopyx(resolved, &resolvedNoStar))

	gotoIfError2(clean, CharString_appendx(&resolved, '*'))

	if(CharString_length(resolved) > MAX_PATH)
		retError(clean, Error_outOfBounds(
			0, CharString_length(resolved), MAX_PATH, "File_foreach()::loc file path is too big (>260 chars)"
		))

	gotoIfError2(clean, CharString_toUTF16x(resolved, &tmpWStr))

	//Skip .

	WIN32_FIND_DATAW dat;
	file = FindFirstFileW((const wchar_t*)tmpWStr.ptr, &dat);
	ListU16_freex(&tmpWStr);

	if(file == INVALID_HANDLE_VALUE)
		retError(clean, Error_notFound(0, 0, "File_foreach()::loc couldn't be found"))

	if(!FindNextFileW(file, &dat))
		retError(clean, Error_notFound(0, 0, "File_foreach() FindNextFile failed (1)"))

	//Loop through real files (while instead of do while because we want to skip ..)

	while(FindNextFileW(file, &dat)) {

		//Folders can also have timestamps, so parse timestamp here

		LARGE_INTEGER time;
		time.LowPart = dat.ftLastWriteTime.dwLowDateTime;
		time.HighPart = dat.ftLastWriteTime.dwHighDateTime;

		//Before 1970, unsupported. Default to 1970

		Ns timestamp = 0;
		static const U64 UNIX_START_WIN = (Ns)11644473600 * (1000000000 / 100);	//ns to 100s of ns

		if ((U64)time.QuadPart >= UNIX_START_WIN)
			timestamp = (time.QuadPart - UNIX_START_WIN) * 100;		//Convert to Oxsomi time

		//Grab local file name

		gotoIfError2(clean, CharString_createCopyx(resolvedNoStar, &tmp))
		gotoIfError2(clean, CharString_createFromUTF16x(dat.cFileName, MAX_PATH, &tmp2))
		gotoIfError2(clean, CharString_appendStringx(&tmp, tmp2))
		CharString_freex(&tmp2);

		//Folder parsing

		if(dat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

			FileInfo info = (FileInfo) {
				.path = tmp,
				.timestamp = timestamp,
				.access = dat.dwFileAttributes & FILE_ATTRIBUTE_READONLY ? EFileAccess_Read : EFileAccess_ReadWrite,
				.type = EFileType_Folder
			};

			gotoIfError3(clean, callback(info, userData, e_rr))

			if(isRecursive)
				gotoIfError3(clean, File_foreach(info.path, callback, userData, true, e_rr))

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

		gotoIfError3(clean, callback(info, userData, e_rr))
		CharString_freex(&tmp);
	}

	DWORD hr = GetLastError();

	if(hr != ERROR_NO_MORE_FILES)
		retError(clean, Error_platformError(0, hr, "File_foreach() FindNextFile failed (2)"))

clean:

	if(file)
		FindClose(file);

	CharString_freex(&tmp);
	CharString_freex(&tmp2);
	CharString_freex(&resolvedNoStar);
	CharString_freex(&resolved);
	ListU16_freex(&tmpWStr);
	return s_uccess;
}

Bool File_loadVirtualInternal1(FileLoadVirtual *userData, CharString loc, Bool allowLoad, Error *e_rr) {

	CharString isChild = CharString_createNull();
	ListU16 tmp = (ListU16) { 0 };
	Bool s_uccess = true;
	ELockAcquire acq = ELockAcquire_Invalid;

	gotoIfError2(clean, CharString_createCopyx(loc, &isChild))

	if(CharString_length(isChild))
		gotoIfError2(clean, CharString_appendx(&isChild, '/'))		//Don't append to root

	acq = SpinLock_lock(&Platform_instance.virtualSectionsLock, U64_MAX);

	if(acq < ELockAcquire_Success)
		retError(clean, Error_invalidState(0, "File_loadVirtualInternal1() couldn't lock virtualSectionsLock"))

	Bool foundAny = false;

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {

		VirtualSection *section = Platform_instance.virtualSections.ptrNonConst + i;

		if(
			!CharString_equalsStringInsensitive(loc, section->path) &&
			!CharString_startsWithStringInsensitive(section->path, isChild, 0)
		)
			continue;

		//Load

		if (userData->doLoad) {

			if (!section->loaded) {

				if (!allowLoad)
					retError(clean, Error_notFound(0, 0, "File_loadVirtualInternal1() was queried but none was found"));

				HGLOBAL handle = NULL;
				CAFile file = (CAFile) { 0 };
				Buffer copy = Buffer_createNull();

				gotoIfError2(clean0, CharString_toUTF16x(section->path, &tmp))
				HRSRC data = FindResourceW(NULL, tmp.ptr, RT_RCDATA);
				ListU16_freex(&tmp);

				if(!data)
					retError(clean0, Error_notFound(0, 1, "File_loadVirtualInternal1() FindResource failed"))

				U32 size = (U32) SizeofResource(NULL, data);
				handle = LoadResource(NULL, data);

				if(!handle)
					retError(clean0, Error_notFound(3, 1, "File_loadVirtualInternal1() LoadResource failed"))

				const U8 *dat = (const U8*) LockResource(handle);
				gotoIfError2(clean0, Buffer_createCopyx(Buffer_createRefConst(dat, size), &copy))

				gotoIfError3(clean0, CAFile_readx(copy, userData->encryptionKey, &file, e_rr))

				section->loadedData = file.archive;
				file.archive = (Archive) { 0 };

				section->loaded = true;
				foundAny = true;

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

		else if(!section->loaded)
			retError(clean, Error_notFound(1, 1, "File_loadVirtualInternal1()::loc not found (1)"))
	}

	if(!foundAny)
		retError(clean, Error_notFound(2, 1, "File_loadVirtualInternal1()::loc not found (2)"))

clean:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance.virtualSectionsLock);

	CharString_freex(&isChild);
	return s_uccess;
}
