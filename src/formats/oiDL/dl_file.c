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

#include "formats/oiDL/dl_file.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/container/buffer.h"

Bool DLFile_createIntern(DLSettings settings, Allocator alloc, U64 reserve, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile)
		retError(clean, Error_nullPointer(0, "DLFile_create()::dlFile is required"))

	if(DLFile_isAllocated(*dlFile))
		retError(clean, Error_invalidOperation(0, "DLFile_create()::dlFile isn't empty, might indicate memleak"))

	if(settings.compressionType >= EXXCompressionType_Count)
		retError(clean, Error_invalidParameter(0, 0, "DLFile_create()::settings.compressionType is invalid"))

	if(settings.compressionType > EXXCompressionType_None)
		retError(clean, Error_invalidOperation(0, "DLFile_create() compression not supported yet"))		//TODO:

	if(settings.encryptionType >= EXXEncryptionType_Count)
		retError(clean, Error_invalidParameter(0, 1, "DLFile_create()::settings.encryptionType is invalid"))

	if(settings.dataType >= EDLDataType_Count)
		retError(clean, Error_invalidParameter(0, 2, "DLFile_create()::settings.dataType is invalid"))

	if(settings.flags & EDLSettingsFlags_Invalid)
		retError(clean, Error_invalidParameter(0, 3, "DLFile_create()::settings.flags contained unsupported flag"))

	dlFile->entryBuffers = (ListBuffer) { 0 };		//ListBuffer and ListCharString are same size

	if(reserve) {

		if(settings.dataType != EDLDataType_Ascii)
			gotoIfError2(clean, ListBuffer_reserve(&dlFile->entryBuffers, reserve, alloc))

		else gotoIfError2(clean, ListCharString_reserve(&dlFile->entryStrings, reserve, alloc))
	}

	dlFile->settings = settings;

clean:
	return s_uccess;
}

U64 DLFile_entryCount(DLFile file) {
	return file.entryBuffers.length;		//Union of entryBuffers and entryStrings contains length at same position
}

Bool DLFile_isAllocated(DLFile file) {
	return file.entryBuffers.ptr;			//Union of entryBuffers and entryStrings contains length at same position
}

Bool DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile, Error *e_rr) {
	return DLFile_createIntern(settings, alloc, 64, dlFile, e_rr);
}

Bool DLFile_free(DLFile *dlFile, Allocator alloc) {

	if(!dlFile || !DLFile_isAllocated(*dlFile))
		return true;

	if (dlFile->settings.dataType != EDLDataType_Ascii)
		ListBuffer_freeUnderlying(&dlFile->entryBuffers, alloc);

	else ListCharString_freeUnderlying(&dlFile->entryStrings, alloc);

	*dlFile = (DLFile) { 0 };
	return true;
}

CharString DLFile_stringAt(DLFile dlFile, U64 i, Bool *success) {

	if(dlFile.settings.dataType == EDLDataType_Data || i >= dlFile.entryStrings.length) {

		if(success)
			*success = false;

		return CharString_createNull();
	}

	if(success)
		*success = true;

	if(dlFile.settings.dataType == EDLDataType_Ascii)
		return dlFile.entryStrings.ptr[i];

	Buffer buf = dlFile.entryBuffers.ptr[i];
	return CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false);
}

Bool DLFile_entryEqualsString(DLFile dlFile, U64 i, CharString str) {
	Bool success = false;
	return CharString_equalsStringSensitive(DLFile_stringAt(dlFile, i, &success), str) && success;
}

U64 DLFile_find(DLFile dlFile, U64 start, U64 end, CharString str) {

	for (U64 j = start; j < dlFile.entryBuffers.length && j < end; ++j)
		if (DLFile_entryEqualsString(dlFile, j, str))
			return j;

	return U64_MAX;
}

Bool DLFile_addEntry(DLFile *dlFile, Buffer entryBuf, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile || !DLFile_isAllocated(*dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntry()::dlFile is required"))

	if(dlFile->settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntry() is unsupported if type isn't Data"))

	gotoIfError2(clean, ListBuffer_pushBack(&dlFile->entryBuffers, entryBuf, alloc))

clean:
	return s_uccess;
}

Bool DLFile_createBufferList(DLSettings settings, ListBuffer buffers, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidOperation(0, "DLFile_createBufferList() is unsupported if settings.type isn't Data"))

	gotoIfError3(clean, DLFile_createIntern(settings, alloc, 0, dlFile, e_rr))
	dlFile->entryBuffers = buffers;

clean:
	return s_uccess;
}

Bool DLFile_addEntryAscii(DLFile *dlFile, CharString entryStr, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile || !dlFile->entryStrings.ptr)
		retError(clean, Error_nullPointer(0, "DLFile_addEntryAscii()::dlFile is required"))

	if(dlFile->settings.dataType != EDLDataType_Ascii)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntryAscii() is unsupported if type isn't Ascii"))

	if(!CharString_isValidAscii(entryStr))
		retError(clean, Error_invalidParameter(1, 0, "DLFile_addEntryAscii()::entryStr isn't valid Ascii"))

	gotoIfError2(clean, ListCharString_pushBack(&dlFile->entryStrings, entryStr, alloc))

clean:
	return s_uccess;
}

Bool DLFile_createAsciiList(DLSettings settings, ListCharString strings, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(settings.dataType != EDLDataType_Ascii)
		retError(clean, Error_invalidOperation(0, "DLFile_createAsciiList() is unsupported if settings.type isn't Ascii"))

	for(U64 i = 0; i < strings.length; ++i)
		if(!CharString_isValidAscii(strings.ptr[i]))
			retError(clean, Error_invalidParameter(1, 0, "DLFile_createAsciiList()::strings[i] isn't valid Ascii"))

	gotoIfError3(clean, DLFile_createIntern(settings, alloc, 0, dlFile, e_rr))

	dlFile->entryStrings = strings;

clean:
	return s_uccess;
}

Bool DLFile_createAsciiListIntern(DLSettings settings, ListBuffer *strings, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	ListCharString strs = (ListCharString) { 0 };
	gotoIfError2(clean, ListCharString_resize(&strs, strings->length, alloc))

	for(U64 i = 0; i < strings->length; ++i) {

		const Buffer buf = strings->ptr[i];
		CharString str;

		if(Buffer_isConstRef(buf))
			str = CharString_createRefSizedConst((const C8*)buf.ptr, Buffer_length(buf), false);

		else if(Buffer_isRef(buf))
			str = CharString_createRefSized((C8*)buf.ptr, Buffer_length(buf), false);

		else str = (CharString) {
			.ptr = (const C8*) buf.ptr,
			.capacityAndRefInfo = Buffer_length(buf),
			.lenAndNullTerminated = Buffer_length(buf)
		};

		strs.ptrNonConst[i] = str;
	}

	gotoIfError3(clean, DLFile_createAsciiList(settings, strs, alloc, dlFile, e_rr))

clean:

	if(!s_uccess)
		ListBuffer_free(strings, alloc);

	return s_uccess;
}

Bool DLFile_addEntryUTF8(DLFile *dlFile, Buffer entryBuf, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile || !DLFile_isAllocated(*dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntryUTF8()::dlFile is required"))

	if(dlFile->settings.dataType != EDLDataType_UTF8)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntryUTF8() is unsupported if type isn't UTF8"))

	if(!Buffer_isUTF8(entryBuf, 1))
		retError(clean, Error_invalidParameter(1, 0, "DLFile_addEntryAscii()::entryBuf isn't valid UTF8"))

	gotoIfError2(clean, ListBuffer_pushBack(&dlFile->entryBuffers, entryBuf, alloc))

clean:
	return s_uccess;
}

Bool DLFile_createUTF8List(DLSettings settings, ListBuffer buffers, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(settings.dataType != EDLDataType_UTF8)
		retError(clean, Error_invalidOperation(0, "DLFile_createUTF8List() is unsupported if settings.type isn't UTF8"))

	for(U64 i = 0; i < buffers.length; ++i)
		if(!Buffer_isUTF8(buffers.ptr[i], 1))
			retError(clean, Error_invalidParameter(
				1, (U32)i, "DLFile_createUTF8List()::buffers[i] isn't valid UTF8"
			))

	gotoIfError3(clean, DLFile_createIntern(settings, alloc, buffers.length, dlFile, e_rr))
	dlFile->entryBuffers = buffers;

clean:
	return s_uccess;
}

Bool DLFile_createList(DLSettings settings, ListBuffer *buffers, Allocator alloc, DLFile *dlFile, Error *e_rr) {

	Bool s_uccess = true;

	if(!buffers)
		retError(clean, Error_nullPointer(1, "DLFile_createList()::buffers is required"))

	switch (settings.dataType) {

		case EDLDataType_Ascii:
			gotoIfError3(clean, DLFile_createAsciiListIntern(settings, buffers, alloc, dlFile, e_rr))
			break;

		case EDLDataType_UTF8:
			gotoIfError3(clean, DLFile_createUTF8List(settings, *buffers, alloc, dlFile, e_rr))
			break;

		default:
			gotoIfError3(clean, DLFile_createBufferList(settings, *buffers, alloc, dlFile, e_rr))
			break;
	}

clean:

	if(s_uccess)
		*buffers = (ListBuffer) { 0 };		//Moved

	return s_uccess;
}
