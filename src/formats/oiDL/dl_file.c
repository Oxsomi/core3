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

#include "types/container/list_impl.h"
#include "formats/oiDL/dl_file.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/list_basic_types.h"

TListImpl(DLEntryStream);

Bool DLFile_createInternal(
	const DLSettings *settings,
	U64 cacheSize,
	Bool reserve,
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	U8 didAlloc = 0;

	if(!dlFile)
		retError(clean, Error_nullPointer(0, "DLFile_create()::dlFile is required"));

	if(DLFile_isAllocated(dlFile))
		retError(clean, Error_invalidOperation(0, "DLFile_create()::dlFile isn't empty, might indicate memleak"));

	if(settings->compressionType >= EXXCompressionType_Count)
		retError(clean, Error_invalidParameter(0, 0, "DLFile_create()::settings.compressionType is invalid"));

	if(settings->compressionType != EXXCompressionType_None)
		retError(clean, Error_invalidOperation(0, "DLFile_create() compression not supported yet"));		//TODO:

	if(settings->encryptionType >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 1, "DLFile_create()::settings.encryptionType is invalid"));

	if(settings->dataType >= EDLDataType_Count)
		retError(clean, Error_invalidParameter(0, 2, "DLFile_create()::settings.dataType is invalid"));

	if(settings->flags & EDLSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 3, "DLFile_create()::settings.flags contained unsupported flag"));

	dlFile->entryBuffers = (ListBuffer) { 0 };		//ListBuffer and ListCharString are same size

	if (cacheSize) {
		gotoIfError3(clean, Buffer_createUninitializedBytes(cacheSize, alloc, &dlFile->cache, e_rr));
		didAlloc |= 1;
	}

	if (reserve) {

		gotoIfError3(clean, ListDLEntryStream_reserve(&dlFile->entryStreams, 4, alloc, e_rr));
		didAlloc |= 2;

		if (settings->dataType == EDLDataType_Data) {
			gotoIfError3(clean, ListBuffer_reserve(&dlFile->entryBuffers, 4, alloc, e_rr));
		}

		else gotoIfError3(clean, ListCharString_reserve(&dlFile->entryStrings, 4, alloc, e_rr));
	}

	dlFile->settings = *settings;

clean:
	if (!s_uccess && (didAlloc & 1))
		Buffer_free(&dlFile->cache, alloc);

	if (!s_uccess && (didAlloc & 2))
		ListDLEntryStream_free(&dlFile->entryStreams, alloc);

	return s_uccess;
}

Bool DLFile_create(const DLSettings *settings, U64 cacheSize, const Allocator *alloc, DLFile *dlFile, Error *e_rr) {
	return DLFile_createInternal(settings, cacheSize, true, alloc, dlFile, e_rr);
}

void DLFile_free(DLFile *dlFile, const Allocator *alloc) {

	if(!DLFile_isAllocated(dlFile))
		return;

	for (U64 i = 0; i < dlFile->entryStreams.length; ++i)
		RefPtr_dec(&dlFile->entryStreams.ptrNonConst[i].stream);

	ListDLEntryStream_free(&dlFile->entryStreams, alloc);

	if (dlFile->settings.dataType == EDLDataType_Data)
		ListBuffer_freeUnderlying(&dlFile->entryBuffers, alloc);

	else ListCharString_freeUnderlying(&dlFile->entryStrings, alloc);

	Buffer_free(&dlFile->cache, alloc);

	*dlFile = (DLFile) { 0 };
}

Bool DLFile_createCopy(const DLFile *dlFile, const Allocator *alloc, DLFile *copy, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocated = false;

	if (!copy)
		retError(clean, Error_nullPointer(2, "DLFile_createCopy()::copy is required"));

	if (copy->cache.ptr || copy->entryStreams.ptr)
		retError(clean, Error_invalidParameter(2, 0, "DLFile_createCopy()::copy isn't empty, indicating possible memleak"));

	if (!dlFile)
		goto clean;

	copy->settings = dlFile->settings;
	gotoIfError3(clean, Buffer_createCopy(dlFile->cache, alloc, &copy->cache, e_rr));
	allocated = true;

	U64 len = dlFile->entryStreams.length;

	if (dlFile->settings.dataType == EDLDataType_Data) {

		if(len)
			gotoIfError3(clean, ListBuffer_resize(&copy->entryBuffers, len, alloc, e_rr));

		//Move addresses from cache to copied cache if applicable

		for (U64 i = 0; i < len; ++i) {

			Buffer buf = dlFile->entryBuffers.ptr[i];

			if (!Buffer_isRef(buf)) {		//Needs copy
				gotoIfError3(clean, Buffer_createCopy(buf, alloc, &copy->entryBuffers.ptrNonConst[i], e_rr));
				continue;
			}

			//Either a ref or a ref to the cache, needs move of address or just ref the same buffer

			if (buf.ptr < dlFile->cache.ptr || buf.ptr >= dlFile->cache.ptr + Buffer_length(dlFile->cache)) {
				copy->entryBuffers.ptrNonConst[i] = buf;
				continue;
			}

			Bool isConst = Buffer_isConstRef(buf);
			buf = Buffer_createRef(copy->cache.ptrNonConst + (buf.ptr - dlFile->cache.ptr), Buffer_length(buf));

			if (isConst)
				buf = Buffer_createRefFromBuffer(buf, true);

			copy->entryBuffers.ptrNonConst[i] = buf;
		}
	}

	else {

		if(len)
			gotoIfError3(clean, ListCharString_resize(&copy->entryStrings, len, alloc, e_rr));
		
		//Move addresses from cache to copied cache if applicable

		for (U64 i = 0; i < len; ++i) {

			CharString str = dlFile->entryStrings.ptr[i];

			if (!CharString_isRef(str)) {
				gotoIfError3(clean, CharString_createCopy(str, alloc, &copy->entryStrings.ptrNonConst[i], e_rr));
				continue;
			}

			//Either a ref or a ref to the cache, needs move of address or just ref the same str

			if (
				(const U8*)str.ptr < dlFile->cache.ptr ||
				(const U8*)str.ptr >= dlFile->cache.ptr + Buffer_length(dlFile->cache)
			) {
				copy->entryStrings.ptrNonConst[i] = str;
				continue;
			}

			if(CharString_isConstRef(str))
				str = CharString_createRefSizedConst(
					(const C8*)copy->cache.ptr + ((const U8*)str.ptr - dlFile->cache.ptr),
					CharString_length(str), CharString_isNullTerminated(str)
				);

			else str = CharString_createRefSized(
				(C8*)copy->cache.ptrNonConst + ((const U8*)str.ptr - dlFile->cache.ptr),
				CharString_length(str), CharString_isNullTerminated(str)
			);

			copy->entryStrings.ptrNonConst[i] = str;
		}
	}

	gotoIfError3(clean, ListDLEntryStream_createCopy(dlFile->entryStreams, alloc, &copy->entryStreams, e_rr));

	for (U64 i = 0; i < copy->entryStreams.length; ++i)
		RefPtr_inc(copy->entryStreams.ptr[i].stream);

clean:

	if (!s_uccess && allocated)
		DLFile_free(copy, alloc);

	return s_uccess;
}

Bool DLFile_reserve(DLFile *dlFile, U64 reserve, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile)
		retError(clean, Error_nullPointer(2, "DLFile_reserve()::dlFile is required"));
	
	gotoIfError3(clean, ListDLEntryStream_reserve(&dlFile->entryStreams, reserve, alloc, e_rr));

	if (dlFile->settings.dataType == EDLDataType_Data) {
		gotoIfError3(clean, ListBuffer_reserve(&dlFile->entryBuffers, reserve, alloc, e_rr));
	}

	else gotoIfError3(clean, ListCharString_reserve(&dlFile->entryStrings, reserve, alloc, e_rr));

clean:
	return s_uccess;
}

Bool DLFile_initCache(DLFile *dlFile, U64 size, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if (!dlFile)
		retError(clean, Error_nullPointer(2, "DLFile_initCache()::dlFile is required"));
	
	if(dlFile->cache.ptr)
		retError(clean, Error_invalidState(0, "DLFile_initCache() can't be called when cache is already initialized"));

	if (!size)
		size = 1 * MIBI;
	
	gotoIfError3(clean, Buffer_resize(&dlFile->cache, size, false, false, alloc, e_rr));

clean:
	return s_uccess;
}
