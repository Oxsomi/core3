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

Bool DLFile_loadedStringAtConst(const DLFile *dlFile, U64 i, CharString *string, Error *e_rr) {

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

	*string = CharString_createRefStrConst(dlFile->entryStrings.ptr[i]);

clean:
	return s_uccess;
}

Bool DLFile_loadedBufferAtConst(const DLFile *dlFile, U64 i, Buffer *buffer, Error *e_rr) {

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

	Buffer buf = dlFile->entryBuffers.ptr[i];
	*buffer = Buffer_createRefFromBuffer(buf, true);

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

	U64 siz = RefPtr_data(*stream, Stream)->size;
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

	if (DLFile_isFullyLoaded(dlFile, i))
		goto clean;

	gotoIfError3(clean, MemoryStream_create(
		DLFile_entrySize(dlFile, i), EMemoryStreamFlags_IsWritable, &type, &memStreamRef, e_rr
	));

	gotoIfError3(clean, StreamCursor_create(memStreamRef, 0, true, alloc, &streamCursor, e_rr));

	gotoIfError3(clean, DLFile_loadStream(
		dlFile,
		i,
		Buffer_createNull(),
		&streamCursor,
		0,
		alloc,
		e_rr
	));

	StreamCursor_close(&streamCursor, alloc);
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

		U64 siz = RefPtr_data(*stream, Stream)->size;
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
