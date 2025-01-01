/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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

Bool DLFile_combine(DLFile a, DLFile b, Allocator alloc, DLFile *combined, Error *e_rr) {

	Bool s_uccess = true;
	const void *aSettingsPtr = &a.settings;
	const void *bSettingsPtr = &b.settings;

	for(U64 i = 0; i < 6; ++i)
		if(((const U64*)aSettingsPtr)[i] != ((const U64*)bSettingsPtr)[i])
			retError(clean, Error_invalidParameter(1, 0, "DLFile_combine()::a is incompatible with b"))

	gotoIfError3(clean, DLFile_create(a.settings, alloc, combined, e_rr))

	if(a.settings.dataType == EDLDataType_Ascii) {

		U64 aj = a.entryStrings.length;
		U64 j = aj + b.entryStrings.length;
		gotoIfError2(clean, ListCharString_reserve(&combined->entryStrings, j, alloc))

		for(U64 i = 0; i < j; ++i)
			gotoIfError3(clean, DLFile_addEntryAscii(
				combined, i < aj ? a.entryStrings.ptr[i] : b.entryStrings.ptr[i - aj], alloc, e_rr
			))
	}

	else {

		U64 aj = a.entryBuffers.length;
		U64 j = aj + b.entryBuffers.length;
		gotoIfError2(clean, ListBuffer_reserve(&combined->entryBuffers, j, alloc))

		for(U64 i = 0; i < j; ++i)
			if(a.settings.dataType == EDLDataType_Data)
				gotoIfError3(clean, DLFile_addEntry(
					combined, i < aj ? a.entryBuffers.ptr[i] : b.entryBuffers.ptr[i - aj], alloc, e_rr
				))

			else gotoIfError3(clean, DLFile_addEntryUTF8(
				combined, i < aj ? a.entryBuffers.ptr[i] : b.entryBuffers.ptr[i - aj], alloc, e_rr
			))
	}

clean:
	return s_uccess;
}
