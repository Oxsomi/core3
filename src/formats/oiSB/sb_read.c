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

//formats/oiSB/sb_read.c

#include "formats/oiSB/sb_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/types.h"
#include "types/container/stream.h"
#include "types/container/list_basic_types.h"
#include "types/base/allocator.h"
#include "types/base/error.h"
#include "types/base/constants.h"

Bool SBFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SBFile *sbFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool didAllocate = false;
	StreamCursor cursor = (StreamCursor) { 0 };

	//We can't directly apply this to sbFile because we'd lose validation from our add functions
	ListSBStruct structs = (ListSBStruct) { 0 };
	ListSBVar vars = (ListSBVar) { 0 };
	ListU8 arraySizes = (ListU8) { 0 };
	ListU32 arrayOffs = (ListU32) { 0 };
	ListU32 arrays = (ListU32) { 0 };
	DLFile dlFile = (DLFile) { 0 };

	if (!offset || !sbFile)
		retError(clean, Error_nullPointer(!offset ? 1 : 4, "SBFile_read()::sbFile and offset are required"));

	if (*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SBFile_read() at misaligned offset is unsupported (16-byte)"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursor, e_rr));

	if(sbFile->bufferSize)
		retError(clean, Error_invalidParameter(4, 0, "SBFile_read()::sbFile is already present, possible memleak"));

	OxStream *stream = RefPtr_data(streamRef, OxStream);

	if(!isSubFile) {

		U32 magic = 0;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, offset, &magic, alloc, e_rr));

		if (magic != SBHeader_MAGIC)
			retError(clean, Error_invalidState(0, "SBFile_read()::file didn't start with oiSB"));
	}

	SBHeader header;
	gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	if (header.version != ESBVersion_V1_2 || (header.flags & ESBFlag_Unsupported) || !header.vars || !header.bufferSize)
		retError(clean, Error_invalidState(0, "SBFile_read()::file didn't have right version, flags or was empty"));

	if(header.arrays > (U16)I16_MAX)
		retError(clean, Error_invalidState(0, "SBFile_read()::header.arrays out of bounds"));

	ESBSettingsFlags flags =
		(isSubFile ? ESBSettingsFlags_HideMagicNumber : ESBSettingsFlags_None) |
		((header.flags & ESBFlag_IsTightlyPacked) ? ESBSettingsFlags_IsTightlyPacked : ESBSettingsFlags_None);

	gotoIfError3(clean, SBFile_create(flags, header.bufferSize, alloc, sbFile, e_rr));
	didAllocate = true;

	gotoIfError3(clean, ListSBStruct_reserve(&sbFile->structs, header.structs, alloc, e_rr));
	gotoIfError3(clean, ListSBVar_reserve(&sbFile->vars, header.vars, alloc, e_rr));

	gotoIfError3(clean, ListSBStruct_resize(&structs, header.structs, alloc, e_rr));
	gotoIfError3(clean, ListSBVar_resize(&vars, header.vars, alloc, e_rr));
	gotoIfError3(clean, ListU8_resize(&arraySizes, header.arrays, alloc, e_rr));
	gotoIfError3(clean, ListU32_resize(&arrayOffs, header.arrays, alloc, e_rr));

	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSBStruct_buffer(structs), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSBVar_buffer(vars), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListU8_buffer(arraySizes), alloc, e_rr));

	U64 total = 0;

	for (U64 i = 0; i < header.arrays; ++i) {
		arrayOffs.ptrNonConst[i] = (U32) total;
		total += arraySizes.ptr[i];
	}

	gotoIfError3(clean, ListU32_resize(&arrays, total, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListU32_buffer(arrays), alloc, e_rr));

	*offset = (*offset + 15) & ~15;        //Align 16-byte

	gotoIfError3(clean, DLFile_read(streamRef, offset, NULL, I32x4_zero(), true, false, alloc, NULL, &dlFile, e_rr));

	U64 stringCount = (U64)header.structs + header.vars;

	if (
		dlFile.entryStrings.length != stringCount ||
		dlFile.settings.dataType != EDLDataType_String ||
		dlFile.settings.encryptionType ||
		dlFile.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SBFile_read() dlFile didn't match expectations"));

	for(U64 i = 0; i < stringCount; ++i)
		if(CharString_length(dlFile.entryStrings.ptr[i]) > 128 || !DLFile_isFullyLoaded(&dlFile, i))
			retError(clean, Error_invalidParameter(0, 1, "SBFile_read() one of the strings exceeded limits"));

	//Copy struct data, validate and move string from dlFile

	for(U64 i = 0; i < header.structs; ++i)
		gotoIfError3(clean, SBFile_addStruct(sbFile, &dlFile.entryStrings.ptrNonConst[i], structs.ptr[i], alloc, e_rr));

	for(U64 i = 0; i < header.vars; ++i) {

		SBVar vari = vars.ptr[i];
		U16 arrayIdAssumed = vari.arrayDimOrArrayId & (U16)I16_MAX;

		if ((vari.arrayDimOrArrayId >> 15) && arrayIdAssumed >= header.arrays)
			retError(clean, Error_outOfBounds(
				0, arrayIdAssumed, header.arrays, "SBFile_read() arrayIndex out of bounds"
			));

		U8 arrayCount = 0;
		U32 tmp[32];    //Worst case array

		if(vari.arrayDimOrArrayId >> 15) {

			U64 off = arrayOffs.ptr[arrayIdAssumed];
			arrayCount = arraySizes.ptr[arrayIdAssumed];

			if (!arrayCount || arrayCount > 32)
				retError(clean, Error_outOfBounds(
					0, arrayCount, 32, "SBFile_read() arrayCount out of bounds [1, 32]"
				));

			for(U8 j = 0; j < arrayCount; ++j)
				tmp[j] = arrays.ptr[off + j];
		}

		else if (arrayIdAssumed) {
			tmp[0] = arrayIdAssumed;
			arrayCount = 1;
		}

		ListU32 listArray = (ListU32) { 0 };

		if (arrayCount)
			gotoIfError3(clean, ListU32_createRefConst(tmp, arrayCount, &listArray, e_rr));

		if (vari.type && vari.structId != U16_MAX)
			retError(clean, Error_invalidState(0, "SBFile_read()::file had a variable with both a type and a structId"));

		if(!vari.type && vari.structId == U16_MAX)
			retError(clean, Error_invalidState(0, "SBFile_read()::file had a variable with neither a type and a structId"));

		CharString *name = &dlFile.entryStrings.ptrNonConst[i + header.structs];

		if (vari.type) {
			gotoIfError3(clean, SBFile_addVariableAsType(
				sbFile,
				name,
				vari.offset,
				vari.parentId,
				vari.type,
				vari.flags,
				arrayCount ? &listArray : NULL,
				alloc,
				e_rr
			));
		}

		else gotoIfError3(clean, SBFile_addVariableAsStruct(
			sbFile,
			name,
			vari.offset,
			vari.parentId,
			vari.structId,
			vari.flags,
			arrayCount ? &listArray : NULL,
			alloc,
			e_rr
		));
	}

	if (!isSubFile && *offset != stream->size)
		retError(clean, Error_invalidState(0, "SBFile_read() file had unrecognized data at the end"));

clean:

	if(didAllocate && !s_uccess)
		SBFile_free(sbFile, alloc);

	DLFile_free(&dlFile, alloc);
	ListU32_free(&arrayOffs, alloc);
	ListU32_free(&arrays, alloc);
	ListU8_free(&arraySizes, alloc);
	ListSBStruct_free(&structs, alloc);
	ListSBVar_free(&vars, alloc);
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
