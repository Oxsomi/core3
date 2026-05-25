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

//formats/oiDL/dl_entry.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct DLFile DLFile;
typedef struct Buffer Buffer;
typedef struct Allocator Allocator;
typedef struct Error Error;
typedef struct CharString CharString;

typedef struct DLEntryStream DLEntryStream;

typedef struct RefPtr RefPtr;
typedef RefPtr StreamRef;

//Add

//Move entry to dlFile, so afterwards entry is empty
Bool DLFile_addEntry(DLFile *dlFile, Buffer *entry, const Allocator *alloc, Error *e_rr);

//Move entry to dlFile, so afterwards entry is empty
Bool DLFile_addEntryString(DLFile *dlFile, CharString *entry, const Allocator *alloc, Error *e_rr);

//Add lazy entry for large entries
Bool DLFile_addEntryStream(
	DLFile *dlFile,
	StreamRef **stream,		//Moves stream ref (so the ref count doesn't need to be incremented)
	U64 dataOff,
	U64 len,
	const Allocator *alloc,
	Error *e_rr
);

//Set

//Move entry into DLFile
Bool DLFile_setEntry(DLFile *dlFile, U64 id, Buffer *entry, const Allocator *alloc, Error *e_rr);

//Move entry into DLFile
Bool DLFile_setEntryString(DLFile *dlFile, U64 id, CharString *entry, const Allocator *alloc, Error *e_rr);

//Move stream into DLFile
Bool DLFile_setStream(DLFile *dlFile, U64 id, StreamRef **stream, U64 dataOff, U64 len, const Allocator *alloc, Error *e_rr);

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

Bool DLFile_combine(const DLFile *a, const DLFile *b, const Allocator *alloc, DLFile *combined, Error *e_rr);
//TODO: Bool DLFile_split(const DLFile *a, const Buffer *bitset, const Allocator *alloc, DLFile *split, Error *e_rr);

#ifdef __cplusplus
	}
#endif
