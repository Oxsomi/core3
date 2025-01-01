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

#include "formats/oiSB/sb_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/container/buffer.h"

Bool SBFile_read(Buffer file, Bool isSubFile, Allocator alloc, SBFile *result, Error *e_rr) {

	Bool s_uccess = true;
	Bool didAllocate = false;
	DLFile strings = (DLFile) { 0 };

	if(!result)
		retError(clean, Error_nullPointer(0, "SBFile_read()::result is required"))

	if(result->vars.ptr)
		retError(clean, Error_invalidParameter(4, 0, "SBFile_read()::result is already present, possible memleak"))

	Buffer tmpFile = Buffer_createRefFromBuffer(file, true);

	if(!isSubFile) {

		U32 magic = 0;
		gotoIfError2(clean, Buffer_consumeU32(&tmpFile, &magic))

		if(magic != SBHeader_MAGIC)
			retError(clean, Error_invalidState(0, "SBFile_read()::file didn't start with oiSB"))
	}

	SBHeader header;
	gotoIfError2(clean, Buffer_consume(&tmpFile, &header, sizeof(header)))

	if(header.version != ESBVersion_V1_2 || (header.flags & ESBFlag_Unsupported) || !header.vars)
		retError(clean, Error_invalidState(0, "SBFile_read()::file didn't have right version, flags or was empty"))

	ESBSettingsFlags flags =
		(isSubFile ? ESBSettingsFlags_HideMagicNumber : ESBSettingsFlags_None) |
		((header.flags & ESBFlag_IsTightlyPacked) ? ESBSettingsFlags_IsTightlyPacked : ESBSettingsFlags_None);

	gotoIfError3(clean, SBFile_create(flags, header.bufferSize, alloc, result, e_rr))
	didAllocate = true;

	gotoIfError3(clean, DLFile_read(tmpFile, NULL, true, alloc, &strings, e_rr))

	U64 stringCount = (U64)header.structs + header.vars;

	if(
		strings.entryBuffers.length != stringCount ||
		strings.settings.flags & EDLSettingsFlags_UseSHA256 ||
		strings.settings.dataType == EDLDataType_Data ||
		strings.settings.encryptionType ||
		strings.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SBFile_read() dlFile didn't match expectations"))

	gotoIfError2(clean, Buffer_offset(&tmpFile, strings.readLength))

	const SBStruct *structs = (const SBStruct*) tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, sizeof(SBStruct) * header.structs))

	for(U64 i = 0; i < header.structs; ++i) {

		CharString tmpName = CharString_createNull();
		CharString *name = &tmpName;

		if(strings.settings.dataType == EDLDataType_Ascii)
			name = &strings.entryStrings.ptrNonConst[i];

		else {
			Buffer buf = strings.entryBuffers.ptr[i];
			tmpName = CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false);
		}

		gotoIfError3(clean, SBFile_addStruct(result, name, structs[i], alloc, e_rr))
	}

	const SBVar *vars = (const SBVar*) tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, sizeof(SBVar) * header.vars))

	const U8 *arraySizeCount = tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, header.arrays))

	U64 arrayCounters = 0;
	for(U64 i = 0; i < header.arrays; ++i)
		arrayCounters += arraySizeCount[i];

	const U32 *arraySizes = (const U32*) tmpFile.ptr;
	gotoIfError2(clean, Buffer_offset(&tmpFile, arrayCounters * sizeof(U32)))

	for(U64 i = 0; i < header.vars; ++i) {

		SBVar vari = vars[i];

		if(vari.arrayIndex != U16_MAX && vari.arrayIndex >= header.arrays)
			retError(clean, Error_outOfBounds(
				0, vari.arrayIndex, header.arrays, "SBFile_read() arrayIndex out of bounds"
			))

		U8 arrayCount = 0;
		U32 tmp[32];	//Worst case array

		if(vari.arrayIndex != U16_MAX) {

			U64 off = 0;

			for(U64 j = 0; j < vari.arrayIndex; ++j)
				off += arraySizeCount[j];

			arrayCount = arraySizeCount[vari.arrayIndex];

			if(!arrayCount || arrayCount > 32)
				retError(clean, Error_outOfBounds(
					0, arrayCount, 32, "SBFile_read() arrayCount out of bounds [1, 32]"
				))

			for(U8 j = 0; j < arrayCount; ++j)
				tmp[j] = arraySizes[off + j];
		}

		ListU32 listArray = (ListU32) { 0 };

		if(arrayCount)
			gotoIfError2(clean, ListU32_createRefConst(tmp, arrayCount, &listArray))

		if(vari.type && vari.structId != U16_MAX)
			retError(clean, Error_invalidState(0, "SBFile_read()::file had a variable with both a type and a structId"))

		if(!vari.type && vari.structId == U16_MAX)
			retError(clean, Error_invalidState(0, "SBFile_read()::file had a variable with neither a type and a structId"))

		CharString tmpName = CharString_createNull();
		CharString *name = &tmpName;

		if(strings.settings.dataType == EDLDataType_Ascii)
			name = &strings.entryStrings.ptrNonConst[i + header.structs];

		else {
			Buffer buf = strings.entryBuffers.ptr[i + header.structs];
			tmpName = CharString_createRefSizedConst((const C8*) buf.ptr, Buffer_length(buf), false);
		}

		if(vari.type)
			gotoIfError3(clean, SBFile_addVariableAsType(
				result,
				name,
				vari.offset, vari.parentId, vari.type, vari.flags,
				arrayCount ? &listArray : NULL,
				alloc, e_rr
			))

		else gotoIfError3(clean, SBFile_addVariableAsStruct(
			result,
			name,
			vari.offset, vari.parentId, vari.structId, vari.flags,
			arrayCount ? &listArray : NULL,
			alloc, e_rr
		))
	}

	if(!isSubFile && Buffer_length(tmpFile))
		retError(clean, Error_invalidState(0, "SBFile_read() file had unrecognized data at the end"))

clean:

	DLFile_free(&strings, alloc);

	if(didAllocate && !s_uccess)
		SBFile_free(result, alloc);

	return s_uccess;
}
