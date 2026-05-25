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

//formats/oiCA/ca_props.h

#pragma once
#include "formats/oiCA/ca_file.h"

#ifdef __cplusplus
	extern "C" {
#endif

//Getters

U64 CAFile_fileCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive);
U64 CAFile_dirCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive);

static inline U64 CAFile_fileObjectCount(const CAFile *caFile, CAHandle fileHandle, Bool recursive) {
	return CAFile_fileCount(caFile, fileHandle, recursive) + CAFile_dirCount(caFile, fileHandle, recursive);
}

U64 CAFile_fileSize(const CAFile *caFile, CAHandle fileHandle);
U64 CAFile_fileParent(const CAFile *caFile, CAHandle fileHandle);
Ns CAFile_fileTime(const CAFile *caFile, CAHandle fileHandle);

//Returns ref to existing data.
// isValid is false for invalid handles, folders or streams.
Buffer CAFile_getData(CAFile *caFile, CAHandle fileHandle, Bool *isValid);

//Returns ref to existing data.
// isValid is false for invalid handles, folders or streams.
Buffer CAFile_getDataConst(const CAFile *caFile, CAHandle fileHandle, Bool *isValid);

//Returns ref to existing data stream (increments stream ref).
// *streamOff is U64_MAX for invalid handles, folders or fully loaded data, otherwise is offset in the stream.
StreamRef *CAFile_getDataStream(const CAFile *caFile, CAHandle fileHandle, U64 *streamOff);

Bool CAFile_isLoaded(const CAFile *caFile, CAHandle fileHandle);			//Returns false for streams

//Setters

Bool CAFile_setTime(CAFile *caFile, CAHandle fileHandle, Ns time, Error *e_rr);

//Set data of a file, only valid if it's a file.
// Moves 'buf' if not a ref, otherwise copies.
Bool CAFile_setData(CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, Buffer *buf, Error *e_rr);

//Set data of a file to a stream, only valid if it's a file.
// Moves stream to content (moves ref)
Bool CAFile_setDataStream(
	CAFile *caFile, CAHandle fileHandle, const Allocator *alloc, StreamRef **stream, U64 off, U64 len, Error *e_rr
);

#ifdef __cplusplus
	}
#endif
