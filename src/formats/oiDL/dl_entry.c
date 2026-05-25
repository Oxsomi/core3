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

//formats/oiDL/dl_entry.c

#include "formats/oiDL/dl_file.h"
#include "types/container/ref_ptr.h"
#include "types/container/memory_stream.h"
#include "types/container/types.h"
#include "types/container/list_basic_types.h"
#include "types/base/error.h"

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
		retError(clean, Error_invalidParameter(1, 0, "DLFile_addEntryString()::entryBuf isn't a valid string"));

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
	StreamRef **stream,
	U64 dataOff,
	U64 len,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool pushed = false;

	if (!DLFile_isAllocated(dlFile))
		retError(clean, Error_nullPointer(0, "DLFile_addEntryStream()::dlFile is required"));

	if (!stream || !*stream || (*stream)->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_nullPointer(1, "DLFile_addEntryStream()::stream is required"));

	U64 siz = RefPtr_data(*stream, OxStream)->size;
	if (dataOff + len > siz)
		retError(clean, Error_outOfBounds(3, dataOff + len, siz, "DLFile_setStream()::off + len out of bounds"));

	if (dlFile->settings.dataType == EDLDataType_String) {
		gotoIfError3(clean, ListCharString_pushBack(&dlFile->entryStrings, CharString_createNull(), alloc, e_rr));
	}

	else gotoIfError3(clean, ListBuffer_pushBack(&dlFile->entryBuffers, Buffer_createNull(), alloc, e_rr));

	pushed = true;

	DLEntryStream entry = (DLEntryStream) { .stream = *stream, .dataOff = dataOff, .len = len };
	gotoIfError3(clean, ListDLEntryStream_pushBack(&dlFile->entryStreams, entry, alloc, e_rr));
	*stream = NULL;

clean:

	if (!s_uccess && pushed) {

		if (dlFile->settings.dataType == EDLDataType_String)
			ListCharString_popBack(&dlFile->entryStrings, NULL, e_rr);

		else ListBuffer_popBack(&dlFile->entryBuffers, NULL, e_rr);
	}

	return s_uccess;
}

Bool DLFile_remove(DLFile *dlFile, U64 id, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!dlFile)
		retError(clean, Error_nullPointer(0, "DLFile_remove()::dlFile is required"));

	if(id >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_remove()::id out of bounds"));

	DLEntryStream stream = dlFile->entryStreams.ptr[id];
	RefPtr_dec(&stream.stream);

	ListDLEntryStream_erase(&dlFile->entryStreams, id, NULL);

	if (dlFile->settings.dataType == EDLDataType_Data) {
		Buffer_free(&dlFile->entryBuffers.ptrNonConst[id], alloc);
		ListBuffer_erase(&dlFile->entryBuffers, id, NULL);
	} else {
		CharString_free(&dlFile->entryStrings.ptrNonConst[id], alloc);
		ListCharString_erase(&dlFile->entryStrings, id, NULL);
	}

clean:
	return s_uccess;
}

Bool DLFile_removeEntry(DLFile *dlFile, U64 id, Buffer *buf, DLEntryStream *stream, Error *e_rr) {
	
	Bool s_uccess = true;

	if (!dlFile || !buf || !stream)
		retError(clean, Error_nullPointer(
			!dlFile ? 0 : (!buf ? 2 : 3), "DLFile_removeEntry()::dlFile, buf and stream are required"
		));

	if (id >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_removeEntry()::id out of bounds"));

	if (dlFile->settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidState(0, "DLFile_removeEntry()::dlFile incompatible dataType"));

	*stream = dlFile->entryStreams.ptr[id];
	ListDLEntryStream_erase(&dlFile->entryStreams, id, NULL);

	*buf = dlFile->entryBuffers.ptrNonConst[id];
	ListBuffer_erase(&dlFile->entryBuffers, id, NULL);

clean:
	return s_uccess;
}

Bool DLFile_removeEntryString(DLFile *dlFile, U64 id, CharString *str, DLEntryStream *stream, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile || !str || !stream)
		retError(clean, Error_nullPointer(
			!dlFile ? 0 : (!str ? 2 : 3), "DLFile_removeEntryString()::dlFile, str and stream are required"
		));

	if (id >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_removeEntryString()::id out of bounds"));

	if (dlFile->settings.dataType != EDLDataType_String)
		retError(clean, Error_invalidState(0, "DLFile_removeEntryString()::dlFile incompatible dataType"));

	*stream = dlFile->entryStreams.ptr[id];
	ListDLEntryStream_erase(&dlFile->entryStreams, id, NULL);

	*str = dlFile->entryStrings.ptrNonConst[id];
	ListCharString_erase(&dlFile->entryStrings, id, NULL);

clean:
	return s_uccess;
}

Bool DLFile_insertStream(DLFile *dlFile, U64 id, DLEntryStream *stream, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile || !stream)
		retError(clean, Error_nullPointer(!dlFile ? 0 : 2, "DLFile_insertStream()::dlFile and stream are required"));

	if (id > dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_insertStream()::id out of bounds"));

	if (id == dlFile->entryStreams.length) {
		gotoIfError3(clean, DLFile_addEntryStream(dlFile, &stream->stream, stream->dataOff, stream->len, alloc, e_rr));
		*stream = (DLEntryStream) { 0 };
		goto clean;
	}

	gotoIfError3(clean, ListDLEntryStream_insert(&dlFile->entryStreams, id, *stream, alloc, e_rr));
	*stream = (DLEntryStream) { 0 };

	if (dlFile->settings.dataType == EDLDataType_String) {
		gotoIfError3(clean, ListCharString_insert(&dlFile->entryStrings, id, CharString_createNull(), alloc, e_rr));
	}

	else gotoIfError3(clean, ListBuffer_insert(&dlFile->entryBuffers, id, Buffer_createNull(), alloc, e_rr));

clean:
	return s_uccess;
}

Bool DLFile_insertEntry(DLFile *dlFile, U64 id, Buffer *buf, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile || !buf)
		retError(clean, Error_nullPointer(!dlFile ? 0 : 2, "DLFile_insertEntry()::dlFile and buf are required"));

	if (id > dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_insertEntry()::id out of bounds"));

	if (dlFile->settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidState(0, "DLFile_insertEntry()::dlFile incompatible dataType"));

	if (id == dlFile->entryStreams.length) {
		gotoIfError3(clean, DLFile_addEntry(dlFile, buf, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, ListDLEntryStream_insert(&dlFile->entryStreams, id, (DLEntryStream) { 0 }, alloc, e_rr));
	gotoIfError3(clean, ListBuffer_insert(&dlFile->entryBuffers, id, *buf, alloc, e_rr));
	*buf = Buffer_createNull();

clean:
	return s_uccess;
}

Bool DLFile_insertEntryString(DLFile *dlFile, U64 id, CharString *str, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile || !str)
		retError(clean, Error_nullPointer(!dlFile ? 0 : 2, "DLFile_insertEntryString()::dlFile and str are required"));

	if (id > dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_insertEntryString()::id out of bounds"));

	if (dlFile->settings.dataType != EDLDataType_String)
		retError(clean, Error_invalidState(0, "DLFile_insertEntryString()::dlFile incompatible dataType"));

	if (id == dlFile->entryStreams.length) {
		gotoIfError3(clean, DLFile_addEntryString(dlFile, str, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, ListDLEntryStream_insert(&dlFile->entryStreams, id, (DLEntryStream) { 0 }, alloc, e_rr));
	gotoIfError3(clean, ListCharString_insert(&dlFile->entryStrings, id, *str, alloc, e_rr));
	*str = CharString_createNull();

clean:
	return s_uccess;
}

Bool DLFile_setEntry(DLFile *dlFile, U64 id, Buffer *entry, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile || !entry)
		retError(clean, Error_nullPointer(!dlFile ? 0 : 2, "DLFile_setEntry()::dlFile and entry are required"));

	if (id >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_setEntry()::id out of bounds"));

	if (dlFile->settings.dataType != EDLDataType_Data)
		retError(clean, Error_invalidState(0, "DLFile_setEntry()::dlFile incompatible dataType"));

	if (dlFile->entryStreams.ptr[id].stream)
		RefPtr_dec(&dlFile->entryStreams.ptrNonConst[id].stream);

	if (Buffer_length(dlFile->entryBuffers.ptr[id]))
		Buffer_free(&dlFile->entryBuffers.ptrNonConst[id], alloc);

	dlFile->entryBuffers.ptrNonConst[id] = *entry;
	*entry = Buffer_createNull();

clean:
	return s_uccess;
}

Bool DLFile_setEntryString(DLFile *dlFile, U64 id, CharString *entry, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile || !entry)
		retError(clean, Error_nullPointer(!dlFile ? 0 : 2, "DLFile_setEntryString()::dlFile and entry are required"));

	if (id >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_setEntryString()::id out of bounds"));

	if (dlFile->settings.dataType != EDLDataType_String)
		retError(clean, Error_invalidState(0, "DLFile_setEntryString()::dlFile incompatible dataType"));

	if (dlFile->entryStreams.ptr[id].stream)
		RefPtr_dec(&dlFile->entryStreams.ptrNonConst[id].stream);

	if (CharString_length(dlFile->entryStrings.ptr[id]))
		CharString_free(&dlFile->entryStrings.ptrNonConst[id], alloc);

	dlFile->entryStrings.ptrNonConst[id] = *entry;
	*entry = CharString_createNull();

clean:
	return s_uccess;
}

Bool DLFile_setStream(DLFile *dlFile, U64 id, StreamRef **stream, U64 dataOff, U64 len, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (!dlFile || !stream)
		retError(clean, Error_nullPointer(!dlFile ? 0 : 2, "DLFile_setStream()::dlFile and stream are required"));

	if (id >= dlFile->entryStreams.length)
		retError(clean, Error_outOfBounds(1, id, dlFile->entryStreams.length, "DLFile_setStream()::id out of bounds"));
	
	if (*stream) {

		if ((*stream)->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
			retError(clean, Error_nullPointer(1, "DLFile_setStream() type of stream is invalid"));

		U64 siz = RefPtr_data(*stream, OxStream)->size;
		if (dataOff + len > siz)
			retError(clean, Error_outOfBounds(3, dataOff + len, siz, "DLFile_setStream()::off + len out of bounds"));
	}

	if (dlFile->entryStreams.ptr[id].stream)
		RefPtr_dec(&dlFile->entryStreams.ptrNonConst[id].stream);

	if (dlFile->settings.dataType == EDLDataType_String) {
		if (CharString_length(dlFile->entryStrings.ptr[id]))
			CharString_free(&dlFile->entryStrings.ptrNonConst[id], alloc);
	} else {
		if (Buffer_length(dlFile->entryBuffers.ptr[id]))
			Buffer_free(&dlFile->entryBuffers.ptrNonConst[id], alloc);
	}

	dlFile->entryStreams.ptrNonConst[id] = (DLEntryStream) {
		.stream = *stream,
		.dataOff = dataOff,
		.len = len
	};

	*stream = NULL;

clean:
	return s_uccess;
}
