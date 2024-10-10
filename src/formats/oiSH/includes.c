/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#ifdef ALLOW_SH_OXC3_PLATFORMS
	#include "platforms/ext/listx_impl.h"
#else
	#include "types/container/list_impl.h"
#endif

#include "formats/oiSH/sh_file.h"

TListImpl(SHInclude);

Bool SHFile_addInclude(SHFile *shFile, SHInclude *include, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString tmp = (CharString) { 0 };

	//Validate everything

	if(!shFile || !include)
		retError(clean, Error_nullPointer(!shFile ? 0 : 1, "SHFile_addInclude()::shFile and include are required"))

	if(!CharString_length(include->relativePath) || !include->crc32c)
		retError(clean, Error_nullPointer(1, "SHFile_addInclude()::include->relativePath and crc32c are required"))

	//Avoid duplicates.
	//Though if the CRC32C mismatches, then we have a big problem.

	for(U64 i = 0; i < shFile->includes.length; ++i)
		if(CharString_equalsStringSensitive(include->relativePath, shFile->includes.ptr[i].relativePath)) {

			if(include->crc32c != shFile->includes.ptr[i].crc32c)
				retError(clean, Error_alreadyDefined(
					0, "SHFile_addInclude()::include was already defined, but with different CRC32C"
				))

			SHInclude_free(include, alloc);		//include is assumed to be moved, if it's not then it'd leak
			goto clean;
		}

	if((shFile->includes.length + 1) >> 16)
		retError(clean, Error_overflow(
			0, shFile->includes.length, 1 << 16, "SHFile_addInclude()::shFile->includes is limited to 16-bit"
		))

	//Ensure we insert it sorted, otherwise it's not reproducible

	U64 i = 0;

	for(; i < shFile->includes.length; ++i)
		if(CharString_compareSensitive(shFile->includes.ptr[i].relativePath, include->relativePath) == ECompareResult_Gt)
			break;

	//Create copy and/or move

	if(CharString_isRef(include->relativePath))
		gotoIfError2(clean, CharString_createCopy(include->relativePath, alloc, &tmp))

	SHInclude tmpInclude = (SHInclude) {
		.relativePath = CharString_isRef(include->relativePath) ? tmp : include->relativePath,
		.crc32c = include->crc32c
	};

	gotoIfError2(clean, ListSHInclude_insert(&shFile->includes, i, tmpInclude, alloc))
	*include = (SHInclude) { 0 };
	tmp = CharString_createNull();

clean:
	CharString_free(&tmp, alloc);
	return s_uccess;
}

void SHInclude_free(SHInclude *include, Allocator alloc) {

	if(!include)
		return;

	CharString_free(&include->relativePath, alloc);
}

void ListSHInclude_freeUnderlying(ListSHInclude *includes, Allocator alloc) {

	if(!includes)
		return;

	for(U64 i = 0; i < includes->length; ++i)
		SHInclude_free(&includes->ptrNonConst[i], alloc);

	ListSHInclude_free(includes, alloc);
}