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
#include "types/base/string_read_helper.h"
#include "types/container/list_basic_types.h"
#include "types/container/types.h"
#include "types/container/memory_stream.h"
#include "types/base/constants.h"

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

Bool DLFile_loadedStringAt(const DLFile *dlFile, U64 i, CharString *string, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile)
		retError(clean, Error_nullPointer(0, "DLFile_loadedStringAt()::dlFile is required"));

	if (i >= dlFile->entryStrings.length)
		retError(clean, Error_outOfBounds(1, i, dlFile->entryStrings.length, "DLFile_loadedStringAt()::i out of bounds"));

	if(dlFile->settings.dataType != EDLDataType_String)
		retError(clean, Error_invalidState(0, "DLFile_loadedStringAt()::dlFile doesn't contain strings"));

	if(dlFile->entryStreams.ptr[i].stream)
		retError(clean, Error_invalidState(0, "DLFile_loadedStringAt()::dlFile entry[i] isn't loaded"));

	if (!string)		//Success, to allow valid checking of string ids
		goto clean;

	if(string->ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "DLFile_loadedStringAt()::string was already allocated, possible memleak"
		));

	*string = dlFile->entryStrings.ptr[i];

clean:
	return s_uccess;
}

Bool DLFile_loadedBufferAt(const DLFile *dlFile, U64 i, Buffer *buffer, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile)
		retError(clean, Error_nullPointer(0, "DLFile_loadedBufferAt()::dlFile is required"));

	if (i >= dlFile->entryBuffers.length)
		retError(clean, Error_outOfBounds(1, i, dlFile->entryBuffers.length, "DLFile_loadedBufferAt()::i out of bounds"));

	if(dlFile->settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidState(0, "DLFile_loadedBufferAt()::dlFile doesn't contain buffers but strings"));

	if (dlFile->entryStreams.ptr[i].stream)
		retError(clean, Error_invalidState(0, "DLFile_loadedBufferAt()::dlFile entry[i] isn't loaded"));

	if (!buffer)		//Success, to allow valid checking of buffer ids
		goto clean;

	if(buffer->ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "DLFile_loadedBufferAt()::buffer was already allocated, possible memleak"
		));

	*buffer = dlFile->entryBuffers.ptr[i];

clean:
	return s_uccess;
}

U64 DLFile_findLoadedString(const DLFile *dlFile, U64 start, U64 end, const CharString *string) {

	if (!dlFile || !string || dlFile->settings.dataType != EDLDataType_String)
		return U64_MAX;

	const CharString *ptr = dlFile->entryStrings.ptr;

	for (U64 i = start, j = dlFile->entryStrings.length; i < j && i < end; ++i)
		if (CharString_equalsStringSensitive(&ptr[i], string))
			return i;

	return U64_MAX;
}

Bool DLFile_addEntry(DLFile *dlFile, Buffer *entry, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	Bool pushed = false;

	if (!DLFile_isAllocated(dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntry()::dlFile is required"));

	if (!entry)
		retError(clean, Error_nullPointer(1, "DLFile_addEntry()::entry is required"));

	if (dlFile->settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntry() is unsupported if type isn't Data"));

	gotoIfError3(clean, ListBuffer_pushBack(&dlFile->entryBuffers, *entry, alloc, e_rr));
	pushed = true;
	gotoIfError3(clean, ListDLEntryStream_pushBack(&dlFile->entryStreams, (DLEntryStream) { 0 }, alloc, e_rr));
	*entry = Buffer_createNull();

clean:

	if (!s_uccess && pushed)
		ListBuffer_popBack(&dlFile->entryBuffers, NULL, e_rr);

	return s_uccess;
}

Bool DLFile_addEntryString(DLFile *dlFile, CharString *entry, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	Bool pushed = false;

	if(!DLFile_isAllocated(dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntryString()::dlFile is required"));

	if (!entry)
		retError(clean, Error_nullPointer(1, "DLFile_addEntryString()::entry is required"));

	if(dlFile->settings.dataType != EDLDataType_String)
		retError(clean, Error_invalidOperation(0, "DLFile_addEntryString() is unsupported if type isn't string"));

	if(!Buffer_isUTF8(CharString_bufferConst(*entry), 1))
		retError(clean, Error_invalidParameter(1, 0, "DLFile_addEntryString()::entryBuf isn't valid string"));

	gotoIfError3(clean, ListCharString_pushBack(&dlFile->entryStrings, *entry, alloc, e_rr));
	pushed = true;
	gotoIfError3(clean, ListDLEntryStream_pushBack(&dlFile->entryStreams, (DLEntryStream) { 0 }, alloc, e_rr));
	*entry = CharString_createNull();

clean:

	if (!s_uccess && pushed)
		ListCharString_popBack(&dlFile->entryStrings, NULL, e_rr);

	return s_uccess;
}

Bool DLFile_addEntryStream(
	DLFile *dlFile,
	StreamRef *stream,
	U64 dataOff,
	U64 len,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool pushed = false;
	Bool addRef = false;

	if (!DLFile_isAllocated(dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntryStream()::dlFile is required"));

	if (!stream || stream->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(1, "DLFile_addEntryStream()::stream is required"));

	if (dlFile->settings.dataType == EDLDataType_String) {
		gotoIfError3(clean, ListCharString_pushBack(&dlFile->entryStrings, CharString_createNull(), alloc, e_rr));
	}

	else gotoIfError3(clean, ListBuffer_pushBack(&dlFile->entryBuffers, Buffer_createNull(), alloc, e_rr));

	pushed = true;

	RefPtr_inc(stream);
	addRef = true;

	DLEntryStream entry = (DLEntryStream) { .stream = stream, .dataOff = dataOff, .len = len };
	gotoIfError3(clean, ListDLEntryStream_pushBack(&dlFile->entryStreams, entry, alloc, e_rr));

clean:

	if (!s_uccess) {

		if (addRef)
			RefPtr_dec(&stream);

		if (pushed) {

			if (dlFile->settings.dataType == EDLDataType_String)
				ListCharString_popBack(&dlFile->entryStrings, NULL, e_rr);

			else ListBuffer_popBack(&dlFile->entryBuffers, NULL, e_rr);
		}
	}

	return s_uccess;
}

Bool DLFile_loadStream(
	const DLFile *dlFile,
	U64 i,
	Buffer cache,
	StreamCursor *writeCursor,
	U64 writeOffset,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool keepCache = false;
	StreamCursor readCursor = (StreamCursor) { 0 };

	if (!dlFile || !writeCursor)
		retError(clean, Error_nullPointer(!dlFile ? 0 : 3, "DLFile_loadStream()::dlFile and writeCursor are required"));

	if(i >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(0, i, dlFile->entryStreams.length, "DLFile_loadStream()::i out of bounds"));

	DLEntryStream stream = dlFile->entryStreams.ptr[i];

	if (!stream.stream) {

		Buffer buf =
			dlFile->settings.dataType == EDLDataType_String ?
			CharString_bufferConst(dlFile->entryStrings.ptr[i]) :
			dlFile->entryBuffers.ptr[i];

		gotoIfError3(clean, StreamCursor_write(writeCursor, buf, 0, writeOffset, Buffer_length(buf), false, alloc, e_rr));
		goto clean;
	}

	if (Buffer_length(cache)) {
		gotoIfError3(clean, StreamCursor_createWithCache(stream.stream, &cache, false, &readCursor, e_rr));
		keepCache = true;
	}

	else gotoIfError3(clean, StreamCursor_create(stream.stream, 0, false, alloc, &readCursor, e_rr));

	gotoIfError3(clean, StreamCursor_copyStream(
		writeCursor,
		&readCursor,
		stream.dataOff,
		writeOffset,
		stream.len,
		alloc,
		e_rr
	));

clean:

	if (readCursor.cacheData.ptr) {

		if(keepCache)
			StreamCursor_closeAndKeepCache(&readCursor, alloc, &cache, NULL);

		else StreamCursor_close(&readCursor, alloc);
	}

	return s_uccess;
}

Bool DLFile_loadEntry(const DLFile *dlFile, U64 i, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	Buffer buf = Buffer_createNull();
	MemoryStreamRef *memStreamRef = NULL;
	const RefPtrType type = MemoryStream_makeType(alloc);		//This doesn't outlast this scope
	StreamCursor streamCursor = (StreamCursor){ 0 };
	
	if (!dlFile)
		retError(clean, Error_nullPointer(0, "DLFile_loadEntry()::dlFile is required"));

	if(i >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(0, i, dlFile->entryStreams.length, "DLFile_loadEntry()::i out of bounds"));

	gotoIfError3(clean, MemoryStream_create(
		DLFile_entrySize(dlFile, i), EMemoryStreamFlags_None, &type, &memStreamRef, e_rr
	));

	gotoIfError3(clean, StreamCursor_create(memStreamRef, 0, false, alloc, &streamCursor, e_rr));

	gotoIfError3(clean, DLFile_loadStream(
		dlFile,
		i,
		Buffer_createNull(),
		&streamCursor,
		0,
		alloc,
		e_rr
	));

	gotoIfError3(clean, MemoryStream_move(&memStreamRef, &buf, e_rr));

	//Move buf to entry and close the stream

	if (dlFile->settings.dataType == EDLDataType_Data) {
		dlFile->entryBuffers.ptrNonConst[i] = buf;
		buf = Buffer_createNull();
	} else {
		CharString str = CharString_createRefSizedConst((const C8*)buf.ptr, Buffer_length(buf), false);
		dlFile->entryStrings.ptrNonConst[i] = str;
		buf = Buffer_createNull();
	}

	RefPtr_dec(&dlFile->entryStreams.ptrNonConst[i].stream);
	dlFile->entryStreams.ptrNonConst[i] = (DLEntryStream) { 0 };

clean:
	RefPtr_dec(&memStreamRef);
	StreamCursor_close(&streamCursor, alloc);
	Buffer_free(&buf, alloc);
	return s_uccess;
}

Bool DLFile_createBufferList(
	const DLSettings *settings,
	ListBuffer *buffers,
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if (settings && settings->dataType != EDLDataType_Data)
		retError(clean, Error_invalidOperation(0, "DLFile_createBufferList() is unsupported if settings.type isn't Data"));

	if (!buffers)
		retError(clean, Error_nullPointer(0, "DLFile_createBufferList() buffers are required"));

	gotoIfError3(clean, DLFile_createInternal(settings, 0, false, alloc, dlFile, e_rr));
	allocated = true;

	if(buffers->length)
		gotoIfError3(clean, ListDLEntryStream_resize(&dlFile->entryStreams, buffers->length, alloc, e_rr));

	dlFile->entryBuffers = *buffers;
	*buffers = (ListBuffer) { 0 };

clean:

	if (allocated && !s_uccess)
		DLFile_free(dlFile, alloc);

	return s_uccess;
}

Bool DLFile_createStringList(
	const DLSettings *settings,
	ListCharString *strings,
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if (settings && settings->dataType != EDLDataType_String)
		retError(clean, Error_invalidOperation(0, "DLFile_createStringList() is unsupported if settings.type isn't String"));

	if (!strings)
		retError(clean, Error_nullPointer(0, "DLFile_createStringList() strings are required"));

	gotoIfError3(clean, DLFile_createInternal(settings, 0, false, alloc, dlFile, e_rr));
	allocated = true;

	if(strings->length)
		gotoIfError3(clean, ListDLEntryStream_resize(&dlFile->entryStreams, strings->length, alloc, e_rr));

	dlFile->entryStrings = *strings;
	*strings = (ListCharString){ 0 };

clean:

	if (allocated && !s_uccess)
		DLFile_free(dlFile, alloc);

	return s_uccess;
}

Bool DLFile_createStreamList(
	const DLSettings *settings,
	ListDLEntryStream *streams,
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if (!streams || !streams->length)
		retError(clean, Error_nullPointer(0, "DLFile_createStreamList() strings are required"));

	gotoIfError3(clean, DLFile_createInternal(settings, 0, false, alloc, dlFile, e_rr));
	allocated = true;

	if (settings->dataType == EDLDataType_String) {
		gotoIfError3(clean, ListCharString_resize(&dlFile->entryStrings, streams->length, alloc, e_rr));
	}

	else gotoIfError3(clean, ListBuffer_resize(&dlFile->entryBuffers, streams->length, alloc, e_rr));

	dlFile->entryStreams = *streams;
	*streams = (ListDLEntryStream){ 0 };

clean:

	if (allocated && !s_uccess)
		DLFile_free(dlFile, alloc);

	return s_uccess;
}
