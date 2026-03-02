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
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct DLFile DLFile;
typedef struct Error Error;
typedef struct CharString CharString;
typedef struct Buffer Buffer;
typedef struct Allocator Allocator;
typedef struct StreamCursor StreamCursor;

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

#ifdef __cplusplus
	}
#endif
