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

//platforms/android/afile.c

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

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

//Virtual files
//
//This mirrors unix/ufile.c, which is compiled for every other unix (it #if's this function out for Android).
//The only real difference is where the bytes come from: on Linux/OSX Platform_initUnixExt mmaps the executable
// and hands us a pointer per section, while an apk's assets aren't in our address space at all,
// so we pull the oiCA out of the AAssetManager here instead.

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
	CharString assetPath = CharString_createNull();
	ELockAcquire acq = ELockAcquire_Invalid;
	CAFile caFile = (CAFile) { 0 };
	MemoryStreamRef *memoryStream = NULL;
	Buffer buf = Buffer_createNull();
	AAsset *asset = NULL;
	const Allocator *streamAlloc = NULL;
	AAssetManager *assetManager = ((struct android_app*)Platform_instance->data)->activity->assetManager;

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

		//Only required from here on, where bytes are actually read.
		//File_isVirtualLoaded queries with a null memoryStreamType because it just reads loadedAndId,
		// so rejecting it up front (as this used to) made every isVirtualLoaded call
		// fail on android while working everywhere else.

		if(!memoryStreamType || !memoryStreamType->alloc)
			retError(clean, Error_nullPointer(3, "File_loadVirtualInternal1()::memoryStreamType is required"));

		//MemoryStream frees the buffer it took ownership of through the allocator its type was made with,
		// and the stream can outlive this call (CAFile_read keeps entries larger than DLFile_smallLen stream backed),
		// so the read buffer has to come from that same allocator rather than from alloc.

		streamAlloc = memoryStreamType->alloc;

		//Platform_initUnixExt only recorded the section name and length, so re-open the asset by name.
		//section->path is "<target>/<name>" (see aplatform.c), the asset is "packages/<target>/<name>.oiCA".

		CharString_free(&assetPath, alloc);

		gotoIfError3(clean, CharString_format(
			alloc, &assetPath, e_rr, "packages/%.*s.oiCA",
			(int) CharString_length(section->path), section->path.ptr
		));

		asset = AAssetManager_open(assetManager, assetPath.ptr, AASSET_MODE_STREAMING);

		if(!asset)
			retError(clean, Error_notFound(0, 1, "File_loadVirtualInternal1() wasn't found by the asset manager"));

		off64_t assetLen = AAsset_getLength64(asset);

		if(assetLen < 0 || (U64) assetLen != section->lenExt)
			retError(clean, Error_invalidState(1, "File_loadVirtualInternal1() asset length changed since init"));

		gotoIfError3(clean, Buffer_createUninitializedBytes(section->lenExt, streamAlloc, &buf, e_rr));

		//AAsset_read caps at int and returns short reads for compressed assets (they're inflated in chunks),
		// so keep going until the whole thing is in.

		for (U64 readOff = 0; readOff < section->lenExt; ) {

			U64 remaining = section->lenExt - readOff;
			int read = AAsset_read(asset, buf.ptrNonConst + readOff, (size_t) U64_min(remaining, (U64) I32_MAX));

			if(read <= 0)
				retError(clean, Error_invalidState(2, "File_loadVirtualInternal1() asset wasn't readable"));

			readOff += (U64) read;
		}

		AAsset_close(asset);
		asset = NULL;

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

	if(asset)
		AAsset_close(asset);

	RefPtr_dec(&memoryStream);
	CAFile_free(&caFile, alloc);
	Buffer_free(&buf, streamAlloc);

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&Platform_instance->virtualSectionsLock);

	CharString_free(&assetPath, alloc);
	CharString_free(&isChild, alloc);
	return s_uccess;
}
