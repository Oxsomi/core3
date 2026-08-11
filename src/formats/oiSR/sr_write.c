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

//formats/oiSR/sr_write.c

#include "formats/oiSR/sr_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/container/stream.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/container/buffer.h"

Bool SRFile_write(const SRFile *srFile, const Allocator *alloc, StreamRef *streamRef, U64 *offset, Error *e_rr) {

	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };

	if(!srFile)
		retError(clean, Error_nullPointer(0, "SRFile_write()::srFile is undefined"));

	if(!offset)
		retError(clean, Error_nullPointer(!streamRef ? 2 : 3, "SRFile_write()::offset is required"));

	if(streamRef && streamRef->refPtrType->typeId != (TypeId)EContainerTypeId_Stream)
		retError(clean, Error_invalidOperation(0, "SRFile_write()::streamRef invalid type"));

	if(*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SRFile_write() at misaligned offset is unsupported (16-byte)"));

	Bool hasSymbols = srFile->flags & ESRSettingsFlags_HasSymbols;

	if(srFile->nodes.length >> 32)
		retError(clean, Error_invalidOperation(0, "SRFile_write()::nodes out of bounds"));

	if(srFile->annotations.length >> 32)
		retError(clean, Error_invalidOperation(0, "SRFile_write()::annotations out of bounds"));

	if(srFile->registers.length >> 32)
		retError(clean, Error_invalidOperation(0, "SRFile_write()::registers out of bounds"));

	if(srFile->enumValues.length >> 32)
		retError(clean, Error_invalidOperation(0, "SRFile_write()::enumValues out of bounds"));

	if(srFile->types.length >> 32)
		retError(clean, Error_invalidOperation(0, "SRFile_write()::types out of bounds"));

	if(srFile->arrayDims.length >> 32)
		retError(clean, Error_invalidOperation(0, "SRFile_write()::arrayDims out of bounds"));

	if(hasSymbols && srFile->symbols.length != srFile->nodes.length)
		retError(clean, Error_invalidState(0, "SRFile_write() symbols must be parallel to nodes when HasSymbols is set"));

	if(!hasSymbols && srFile->symbols.length)
		retError(clean, Error_invalidState(0, "SRFile_write() symbols present but HasSymbols isn't set"));

	OxStream *stream = streamRef ? RefPtr_data(streamRef, OxStream) : NULL;

	//Get header size

	U64 headerSize = sizeof(SRHeader);

	if(!(srFile->flags & ESRSettingsFlags_HideMagicNumber))        //Magic number (can be hidden by parent; such as oiSH)
		headerSize += sizeof(U32);

	headerSize += ListSRNode_bytes(srFile->nodes);

	if(hasSymbols)
		headerSize += ListSRSymbol_bytes(srFile->symbols);

	headerSize += ListSRAnnotation_bytes(srFile->annotations);
	headerSize += ListSRRegister_bytes(srFile->registers);
	headerSize += ListSREnumValue_bytes(srFile->enumValues);
	headerSize += ListSRType_bytes(srFile->types);
	headerSize += ListU32_bytes(srFile->arrayDims);

	headerSize = (headerSize + 15) & ~15;

	if(!stream) {
		*offset += headerSize;
		gotoIfError3(clean, DLFile_write(&srFile->names, alloc, NULL, NULL, I32x4_zero(), offset, e_rr));
		goto clean;
	}

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, true, alloc, &cursor, e_rr));

	if(stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, *offset + headerSize, alloc, e_rr));

	SRHeader header = (SRHeader) {
		.version = ESRVersion_V1_1,
		.flags = hasSymbols ? ESRFlag_HasSymbols : ESRFlag_None,
		.features = srFile->features,
		.nodeCount = (U32) srFile->nodes.length,
		.annotationCount = (U32) srFile->annotations.length,
		.registerCount = (U32) srFile->registers.length,
		.enumValueCount = (U32) srFile->enumValues.length,
		.typeCount = (U32) srFile->types.length,
		.arrayDimCount = (U32) srFile->arrayDims.length
	};

	if(!(srFile->flags & ESRSettingsFlags_HideMagicNumber))
		gotoIfError3(clean, StreamCursor_appendU32(&cursor, offset, SRHeader_MAGIC, alloc, e_rr));

	gotoIfError3(clean, StreamCursor_append(&cursor, offset, &header, sizeof(header), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSRNode_bufferConst(srFile->nodes), alloc, e_rr));

	if(hasSymbols)
		gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSRSymbol_bufferConst(srFile->symbols), alloc, e_rr));

	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSRAnnotation_bufferConst(srFile->annotations), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSRRegister_bufferConst(srFile->registers), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSREnumValue_bufferConst(srFile->enumValues), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListSRType_bufferConst(srFile->types), alloc, e_rr));
	gotoIfError3(clean, StreamCursor_appendBuffer(&cursor, offset, ListU32_bufferConst(srFile->arrayDims), alloc, e_rr));

	//Need to make sure the names oiDL is 16-byte aligned

	U8 pad[16] = { 0 };
	U64 utilized = *offset & 15;

	if(utilized)
		gotoIfError3(clean, StreamCursor_append(&cursor, offset, pad, 16 - utilized, alloc, e_rr));

	//Append names

	StreamCursor_close(&cursor, alloc);

	gotoIfError3(clean, DLFile_write(&srFile->names, alloc, streamRef, NULL, I32x4_zero(), offset, e_rr));

clean:
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
