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

#include "formats/oiSB/sb_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/error.h"
#include "types/allocator.h"
#include "types/buffer.h"

Bool SBFile_write(SBFile sbFile, Allocator alloc, Buffer *result, Error *e_rr) {

	Bool s_uccess = true;

	if(!sbFile.vars.ptr)
		retError(clean, Error_invalidOperation(0, "SBFile_write()::sbFile is undefined"))

	if(!result)
		retError(clean, Error_nullPointer(2, "SBFile_write()::result is required"))

	if(result->ptr)
		retError(clean, Error_invalidOperation(0, "SBFile_write()::result wasn't empty, might indicate memleak"))

	//Get header size

	U64 headerSize = sizeof(SBHeader);

	if(!(sbFile.flags & ESBSettingsFlags_HideMagicNumber))		//Magic number (can be hidden by parent; such as oiSC)
		headerSize += sizeof(U32);

	headerSize += ListSBStruct_bytes(sbFile.structs);
	headerSize += ListSBVar_bytes(sbFile.vars);
	headerSize += sbFile.arrays.length;

	for(U64 i = 0; i < sbFile.arrays.length; ++i)
		headerSize += sbFile.arrays.ptr[i].length * sizeof(U32);

	Bool isUTF8 = sbFile.flags & ESBSettingsFlags_IsUTF8;

	DLSettings settings = (DLSettings) {
		.dataType = isUTF8 ? EDLDataType_UTF8 : EDLDataType_Ascii,
		.flags = EDLSettingsFlags_HideMagicNumber
	};

	DLFile dlFile = (DLFile) { 0 };
	Buffer dlFileBuf = Buffer_createNull();
	gotoIfError3(clean, DLFile_create(settings, alloc, &dlFile, e_rr))

	for (U64 i = 0; i < sbFile.structNames.length; ++i) {

		if(isUTF8)
			gotoIfError3(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(sbFile.structNames.ptr[i]), alloc, e_rr))

		else gotoIfError3(clean, DLFile_addEntryAscii(
			&dlFile, CharString_createRefStrConst(sbFile.structNames.ptr[i]), alloc, e_rr
		))
	}

	for (U64 i = 0; i < sbFile.varNames.length; ++i) {

		if(isUTF8)
			gotoIfError3(clean, DLFile_addEntryUTF8(&dlFile, CharString_bufferConst(sbFile.varNames.ptr[i]), alloc, e_rr))

		else gotoIfError3(clean, DLFile_addEntryAscii(
			&dlFile, CharString_createRefStrConst(sbFile.varNames.ptr[i]), alloc, e_rr
		))
	}

	gotoIfError3(clean, DLFile_write(dlFile, alloc, &dlFileBuf, e_rr))

	U64 len = Buffer_length(dlFileBuf) + headerSize;

	//Create buffer

	gotoIfError2(clean, Buffer_createUninitializedBytes(len, alloc, result))

	//Prepend header

	U8 *headerIt = (U8*)result->ptr;

	if (!(sbFile.flags & ESBSettingsFlags_HideMagicNumber)) {
		*(U32*)headerIt = SBHeader_MAGIC;
		headerIt += sizeof(U32);
	}

	SBHeader *sbHeader = (SBHeader*) headerIt;

	*sbHeader = (SBHeader) {

		.version = ESBVersion_V1_2,
		.flags = sbFile.flags & ESBSettingsFlags_IsTightlyPacked ? ESBFlag_IsTightlyPacked : ESBFlag_None,
		.arrays = (U16) sbFile.arrays.length,

		.structs = (U16) sbFile.structs.length,
		.vars = (U16) sbFile.vars.length,

		.bufferSize = sbFile.bufferSize
	};

	headerIt += sizeof(SBHeader);

	Buffer_copy(Buffer_createRef(headerIt, Buffer_length(dlFileBuf)), dlFileBuf);
	headerIt += Buffer_length(dlFileBuf);

	SBStruct *structs = (SBStruct*) headerIt;

	Buffer_copy(Buffer_createRef(structs, ListSBStruct_bytes(sbFile.structs)), ListSBStruct_bufferConst(sbFile.structs));
	SBVar *vars = (SBVar*) (structs + sbFile.structs.length);

	Buffer_copy(Buffer_createRef(vars, ListSBVar_bytes(sbFile.vars)), ListSBVar_bufferConst(sbFile.vars));
	U8 *arrayDimCount = (U8*) (vars + sbFile.vars.length);
	U32 *arrayDims = (U32*) (arrayDimCount + sbFile.arrays.length);

	for(U64 i = 0, j = 0; i < sbFile.arrays.length; ++i) {

		arrayDimCount[i] = (U8)sbFile.arrays.ptr[i].length;

		for(U64 k = 0; k < arrayDimCount[i]; ++k, ++j)
			arrayDims[j] = sbFile.arrays.ptr[i].ptr[k];
	}

clean:
	Buffer_free(&dlFileBuf, alloc);
	DLFile_free(&dlFile, alloc);
	return s_uccess;
}
