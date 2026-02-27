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

#pragma once
#include "formats/oiDL/dl_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

Bool DLFile_create(const DLSettings *settings, U64 cacheSize, const Allocator *alloc, DLFile *dlFile, Error *e_rr);
void DLFile_free(DLFile *dlFile, const Allocator *alloc);

Bool DLFile_loadedStringAtConst(const DLFile *dlFile, U64 i, CharString *string, Error *e_rr);
Bool DLFile_loadedBufferAtConst(const DLFile *dlFile, U64 i, Buffer *buffer, Error *e_rr);

//Move from stream into memory permanently and close stream.
Bool DLFile_loadEntry(const DLFile *dlFile, U64 i, const Allocator *alloc, Error *e_rr);

//Load contents into stream (using cache), also works fine for loaded entries
Bool DLFile_loadStream(
	const DLFile *dlFile,
	U64 i,
	Buffer cache,					//Pass empty buffer for default
	StreamCursor *writeCursor,
	U64 writeOffset,
	const Allocator *alloc,
	Error *e_rr
);

//Currently quite slow!
U64 DLFile_findLoadedString(const DLFile *dlFile, U64 start, U64 end, const CharString *string);

//Turn raw buffer list / char string list / stream list into a DLFile.

Bool DLFile_createBufferList(
	const DLSettings *settings,
	ListBuffer *buffers,			//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_createStringList(
	const DLSettings *settings,
	ListCharString *strings,		//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_createStreamList(
	const DLSettings *settings,
	ListDLEntryStream *streams,		//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

//Add

//Move entry to dlFile, so afterwards entry is empty
Bool DLFile_addEntry(DLFile *dlFile, Buffer *entry, const Allocator *alloc, Error *e_rr);

//Move entry to dlFile, so afterwards entry is empty
Bool DLFile_addEntryString(DLFile *dlFile, CharString *entry, const Allocator *alloc, Error *e_rr);

//Add lazy entry for large entries
Bool DLFile_addEntryStream(
	DLFile *dlFile,
	StreamRef *stream,
	U64 dataOff,
	U64 len,
	const Allocator *alloc,
	Error *e_rr
);

//Remove

Bool DLFile_remove(DLFile *dlFile, U64 id, const Allocator *alloc, Error *e_rr);

//Removes entry and moves buffer and/or stream
Bool DLFile_removeEntry(DLFile *dlFile, U64 id, Buffer *buf, DLEntryStream *stream, Error *e_rr);

//Removes entry and moves string and/or stream
Bool DLFile_removeEntryString(DLFile *dlFile, U64 id, CharString *str, DLEntryStream *stream, Error *e_rr);

//Insertion

//Moves stream into DLFile at id and moves everything that's behind it
Bool DLFile_insertStream(DLFile *dlFile, U64 id, DLEntryStream *stream, const Allocator *alloc, Error *e_rr);

//Moves buffer into DLFile at id and moves everything that's behind it
Bool DLFile_insertEntry(DLFile *dlFile, U64 id, Buffer *buf, const Allocator *alloc, Error *e_rr);

//Moves string into DLFile at id and moves everything that's behind it
Bool DLFile_insertEntryString(DLFile *dlFile, U64 id, CharString *str, const Allocator *alloc, Error *e_rr);

//Important note: StreamRef at startOffset shouldn't be contained in a StreamCursor at this moment,
// or you might risk overwriting it.
Bool DLFile_write(
	const DLFile *dlFile,
	const Allocator *alloc,
	StreamRef *result,

	//NULL if not encrypted, otherwise must be valid for the DLFile's stream lifetime
	// This can outlast the DLFile if it's the stream is referenced elsewhere.
	const RefPtrType *encryptionStreamType,

	U64 *startOffset,
	Error *e_rr
);

Bool DLFile_read(
	StreamRef *file,
	U64 startOffset,
	const U32 encryptionKey[8],		//Must be NULL if no encryption, else must be valid
	Bool isSubFile,					//Sets HideMagicNumber flag and allows leftover data after the oiDL
	const Allocator *alloc,

	//NULL if not encrypted, otherwise must be valid for the DLFile's stream lifetime
	// This can outlast the DLFile if it's the stream is referenced elsewhere.
	const RefPtrType *encryptionStreamType,	

	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_combine(const DLFile *a, const DLFile *b, const Allocator *alloc, DLFile *combined, Error *e_rr);
//TODO: Bool DLFile_split(const DLFile *a, const Buffer *bitset, const Allocator *alloc, DLFile *split, Error *e_rr);

#ifdef __cplusplus
	}
#endif
