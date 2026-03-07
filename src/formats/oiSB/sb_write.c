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

#include "formats/oiSB/sb_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/ref_ptr.h"
#include "types/container/types.h"
#include "types/container/stream.h"
#include "types/container/list_basic_types.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/container/buffer.h"

Bool SBFile_write(const SBFile *sbFile, const Allocator *alloc, StreamRef *streamRef, U64 *offset, Error *e_rr) {

	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };

	if (!sbFile || !sbFile->bufferSize)
		retError(clean, Error_nullPointer(0, "SBFile_write()::sbFile is undefined"));

	if(!streamRef || !offset)
		retError(clean, Error_nullPointer(!streamRef ? 2 : 3, "SBFile_write()::streamRef and offset are required"));

	if(streamRef->refPtrType->typeId != (ETypeId)EContainerTypeId_Stream)
		retError(clean, Error_invalidOperation(0, "SBFile_write()::streamRef invalid type"));

	if(sbFile->arrays.length > (U16)I16_MAX)
		retError(clean, Error_invalidOperation(0, "SBFile_write()::arrays out of bounds"));

	Stream *stream = RefPtr_data(streamRef, Stream);
	gotoIfError3(clean, StreamCursor_create(streamRef, 0, true, alloc, &cursor, e_rr));

	//Get header size

	U64 headerSize = sizeof(SBHeader);

	if(!(sbFile->flags & ESBSettingsFlags_HideMagicNumber))		//Magic number (can be hidden by parent; such as oiSH)
		headerSize += sizeof(U32);

	headerSize += ListSBStruct_bytes(sbFile->structs);
	headerSize += ListSBVar_bytes(sbFile->vars);
	headerSize += sbFile->arrays.length * sizeof(U8);

	for(U64 i = 0; i < sbFile->arrays.length; ++i)
		headerSize += sbFile->arrays.ptr[i].length * sizeof(U32);

	headerSize = (headerSize + 15) & ~15;

	if (stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, *offset + headerSize, alloc, e_rr));

	SBHeader header = {

		.version = ESBVersion_V1_2,
		.flags = sbFile->flags & ESBSettingsFlags_IsTightlyPacked ? ESBFlag_IsTightlyPacked : ESBFlag_None,
		.arrays = (U16)sbFile->arrays.length,

		.structs = (U16)sbFile->structs.length,
		.vars = (U16)sbFile->vars.length,

		.bufferSize = sbFile->bufferSize
	};

	if (!(sbFile->flags & ESBSettingsFlags_HideMagicNumber))
		gotoIfError3(clean, StreamCursor_appendU32(&cursor, offset, SBHeader_MAGIC, alloc, e_rr));

	gotoIfError3(clean, StreamCursor_append(&cursor, offset, &header, sizeof(header), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSBStruct_bufferConst(sbFile->structs), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSBVar_bufferConst(sbFile->vars), alloc, e_rr));

	for (U64 i = 0; i < sbFile->arrays.length; ++i)
		gotoIfError3(clean, StreamCursor_appendU8(&cursor, offset, (U8)sbFile->arrays.ptr[i].length, alloc, e_rr));

	for(U64 i = 0; i < sbFile->arrays.length; ++i)
		gotoIfError3(clean, StreamCursor_appendBuffer(
			&cursor, offset, ListU32_bufferConst(sbFile->arrays.ptr[i]), alloc, e_rr
		));

	//Need to make sure oiDL is 16-byte aligned

	U8 pad[16] = { 0 };
	U64 utilized = *offset & 15;

	if (utilized)
		gotoIfError3(clean, StreamCursor_append(&cursor, offset, pad, 16 - utilized, alloc, e_rr));

	//Append names

	StreamCursor_close(&cursor, alloc);

	gotoIfError3(clean, DLFile_write(&sbFile->names, alloc, streamRef, NULL, I32x4_zero(), offset, e_rr));

clean:
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
