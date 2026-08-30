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

//platforms/web/webfile.c
//
//Virtual files on the web target.
//Mirrors android/afile.c: Platform_initUnixExt (webplatform.c) recorded only name + length per section,
// so the bytes are pulled in here on demand: from packages/<path>.oiCA on the mounted filesystem
// (NODERAWFS under node, MEMFS/preloaded in a browser) instead of an AAssetManager.
//unix/ufile.c #if's its ELF-section variant out for this platform, same as it does for android.

#include "platforms/file.h"
#include "platforms/platform.h"
#include "formats/oiCA/ca_file.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/memory_stream.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/lock.h"
#include "types/base/mathi.h"

#include <unistd.h>
#include <fcntl.h>

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
	CharString filePath = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;
	CAFile caFile = (CAFile) { 0 };
	MemoryStreamRef *memoryStream = NULL;
	Buffer buf = Buffer_createNull();
	int fd = -1;
	const Allocator *streamAlloc = NULL;

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

		//Only required once bytes are actually read; File_isVirtualLoaded probes with a null type (see afile.c).

		if(!memoryStreamType || !memoryStreamType->alloc)
			retError(clean, Error_nullPointer(3, "File_loadVirtualInternal1()::memoryStreamType is required"));

		//The stream outlives this call and frees the buffer through its own allocator (see afile.c).

		streamAlloc = memoryStreamType->alloc;

		//section->path is "<target>/<name>" (see webplatform.c),
		// the file is "packages/<target>/<name>.oiCA", relative to cwd == app directory.

		CharString_free(&filePath, alloc);

		gotoIfError3(clean, CharString_format(
			alloc, &filePath, e_rr, "packages/%.*s.oiCA",
			(int) CharString_length(section->path), section->path.ptr
		));

		fd = open(filePath.ptr, O_RDONLY);

		if(fd < 0)
			retError(clean, Error_notFound(0, 1, "File_loadVirtualInternal1() package file wasn't found"));

		const off_t fileLen = lseek(fd, 0, SEEK_END);

		if(fileLen < 0 || (U64) fileLen != section->lenExt)
			retError(clean, Error_invalidState(1, "File_loadVirtualInternal1() package length changed since init"));

		gotoIfError3(clean, Buffer_createUninitializedBytes(section->lenExt, streamAlloc, &buf, e_rr));

		//Chunked pread, same as ufile.c's FileHandle_readPhysical: a single read isn't guaranteed to
		// return everything, and MEMFS in particular short reads.

		U8 *dst           = buf.ptrNonConst;
		U64 remaining     = section->lenExt;
		U64 currentOffset = 0;

		while(remaining) {

			const U64 toRead = U64_min(remaining, 64 * MIBI);        //64 MB chunks
			const ssize_t got = pread(fd, dst, (size_t)toRead, (off_t)currentOffset);

			if(got < 0 || (U64)got != toRead)
				retError(clean, Error_invalidState(2, "File_loadVirtualInternal1() package wasn't readable"));

			dst           += got;
			currentOffset += (U64)got;
			remaining     -= (U64)got;
		}

		close(fd);
		fd = -1;

		//Hands buf over to the stream; it's freed when the last reference to the stream goes away.

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

	if(fd >= 0)
		close(fd);

	RefPtr_dec(&memoryStream);
	CAFile_free(&caFile, alloc);
	Buffer_free(&buf, streamAlloc);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	CharString_free(&filePath, alloc);
	CharString_free(&isChild, alloc);
	return s_uccess;
}
