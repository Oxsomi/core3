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

//formats/oiDL/dl_load.c

#include "formats/oiDL/dl_file.h"
#include "types/container/container_types.h"
#include "types/container/memory_stream.h"
#include "types/base/error.h"
#include "types/base/string_read_helper.h"

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

	if (!string)        //Success, to allow valid checking of string ids
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

	if (!buffer)        //Success, to allow valid checking of buffer ids
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
	const RefPtrType type = MemoryStream_makeType(alloc);        //This doesn't outlast this scope
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
