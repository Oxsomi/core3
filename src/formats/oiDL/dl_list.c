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

#include "formats/oiDL/dl_file.h"
#include "types/container/list_basic_types.h"
#include "types/base/error.h"

Bool DLFile_createInternal(
	const DLSettings *settings,
	U64 cacheSize,
	Bool reserve,
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

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
