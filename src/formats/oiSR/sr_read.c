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

//formats/oiSR/sr_read.c

#include "formats/oiSR/sr_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/container/stream.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

Bool SRFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SRFile *srFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool didAllocate = false;
	StreamCursor cursor = (StreamCursor) { 0 };

	//Read into locals first so a validation failure can't leave a half-built srFile

	ListSRNode nodes = (ListSRNode) { 0 };
	ListSRSymbol symbols = (ListSRSymbol) { 0 };
	ListSRAnnotation annotations = (ListSRAnnotation) { 0 };
	ListSRRegister registers = (ListSRRegister) { 0 };
	ListSREnumValue enumValues = (ListSREnumValue) { 0 };
	ListSRType types = (ListSRType) { 0 };
	ListU32 arrayDims = (ListU32) { 0 };
	ListSRInterface interfaces = (ListSRInterface) { 0 };
	DLFile names = (DLFile) { 0 };

	if(!offset || !srFile)
		retError(clean, Error_nullPointer(!offset ? 1 : 4, "SRFile_read()::srFile and offset are required"));

	if(srFile->nodes.length || srFile->names.entryStrings.length)
		retError(clean, Error_invalidParameter(4, 0, "SRFile_read()::srFile is already present, possible memleak"));

	if(*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SRFile_read() at misaligned offset is unsupported (16-byte)"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursor, e_rr));

	OxStream *stream = RefPtr_data(streamRef, OxStream);

	if(!isSubFile) {

		U32 magic = 0;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, offset, &magic, alloc, e_rr));

		if(magic != SRHeader_MAGIC)
			retError(clean, Error_invalidState(0, "SRFile_read()::file didn't start with oiSR"));
	}

	SRHeader header;
	gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	if(header.version != ESRVersion_V1_1 || (header.flags & ESRFlag_Unsupported))
		retError(clean, Error_invalidState(0, "SRFile_read()::file didn't have the right version or flags"));

	if(header.features & ~(U32)ESRFeature_All & ~(U32)ESRFeature_SymbolInfo)
		retError(clean, Error_invalidState(0, "SRFile_read()::header.features contained unsupported bits"));

	Bool hasSymbols = header.flags & ESRFlag_HasSymbols;

	//The SymbolInfo feature and the HasSymbols flag must agree

	if((Bool)(header.features & ESRFeature_SymbolInfo) != hasSymbols)
		retError(clean, Error_invalidState(0, "SRFile_read()::header symbol feature and flag disagree"));

	//Consume the fixed-size POD arrays

	gotoIfError3(clean, ListSRNode_resize(&nodes, header.nodeCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRNode_buffer(nodes), alloc, e_rr));

	if(hasSymbols) {
		gotoIfError3(clean, ListSRSymbol_resize(&symbols, header.nodeCount, alloc, e_rr));
		gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRSymbol_buffer(symbols), alloc, e_rr));
	}

	gotoIfError3(clean, ListSRAnnotation_resize(&annotations, header.annotationCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRAnnotation_buffer(annotations), alloc, e_rr));

	gotoIfError3(clean, ListSRRegister_resize(&registers, header.registerCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRRegister_buffer(registers), alloc, e_rr));

	gotoIfError3(clean, ListSREnumValue_resize(&enumValues, header.enumValueCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSREnumValue_buffer(enumValues), alloc, e_rr));

	gotoIfError3(clean, ListSRType_resize(&types, header.typeCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRType_buffer(types), alloc, e_rr));

	gotoIfError3(clean, ListU32_resize(&arrayDims, header.arrayDimCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListU32_buffer(arrayDims), alloc, e_rr));

	gotoIfError3(clean, ListSRInterface_resize(&interfaces, header.interfaceCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRInterface_buffer(interfaces), alloc, e_rr));

	//Align 16-byte then read the names oiDL.
	//Consume the pad bytes through the cursor rather than only advancing the offset, so a forward-only stream (file data
	// streaming straight off disk) moves past the padding too; otherwise DLFile_read would start reading inside the pad
	// and reject the names as a bad DLFile header.

	U64 aligned = (*offset + 15) & ~15;

	if(aligned != *offset) {
		U8 padDiscard[16];
		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, padDiscard, aligned - *offset, alloc, e_rr));
	}

	gotoIfError3(clean, DLFile_read(streamRef, offset, NULL, I32x4_zero(), true, false, alloc, NULL, &names, e_rr));

	if(
		names.settings.dataType != EDLDataType_String ||
		names.settings.encryptionType ||
		names.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SRFile_read() names didn't match expectations"));

	U64 nameCount = names.entryStrings.length;

	for(U64 i = 0; i < nameCount; ++i) {

		if(!DLFile_isFullyLoaded(&names, i))
			retError(clean, Error_invalidParameter(0, 1, "SRFile_read() one of the strings wasn't fully loaded"));

		//Cap string length, matching the producer's 32767 limit (and the oiSB/oiSH house style)

		if(CharString_length(names.entryStrings.ptr[i]) >= 32768)
			retError(clean, Error_invalidParameter(0, 1, "SRFile_read() one of the strings exceeded the length limit"));
	}

	if(!isSubFile && *offset != stream->size)
		retError(clean, Error_invalidState(0, "SRFile_read() file had unrecognized data at the end"));

	//Move everything into srFile

	ESRSettingsFlags flags =
		(isSubFile ? ESRSettingsFlags_HideMagicNumber : ESRSettingsFlags_None) |
		(hasSymbols ? ESRSettingsFlags_HasSymbols : ESRSettingsFlags_None);

	didAllocate = true;

	srFile->names = names;
	srFile->nodes = nodes;
	srFile->symbols = symbols;
	srFile->annotations = annotations;
	srFile->registers = registers;
	srFile->enumValues = enumValues;
	srFile->types = types;
	srFile->arrayDims = arrayDims;
	srFile->interfaces = interfaces;
	srFile->flags = flags;
	srFile->features = header.features;

	names = (DLFile) { 0 };
	nodes = (ListSRNode) { 0 };
	symbols = (ListSRSymbol) { 0 };
	annotations = (ListSRAnnotation) { 0 };
	registers = (ListSRRegister) { 0 };
	enumValues = (ListSREnumValue) { 0 };
	types = (ListSRType) { 0 };
	arrayDims = (ListU32) { 0 };
	interfaces = (ListSRInterface) { 0 };

	gotoIfError3(clean, SRFile_finalize(srFile, alloc, e_rr));

clean:

	if(didAllocate && !s_uccess)
		SRFile_free(srFile, alloc);

	DLFile_free(&names, alloc);
	ListSRNode_free(&nodes, alloc);
	ListSRSymbol_free(&symbols, alloc);
	ListSRAnnotation_free(&annotations, alloc);
	ListSRRegister_free(&registers, alloc);
	ListSREnumValue_free(&enumValues, alloc);
	ListSRType_free(&types, alloc);
	ListU32_free(&arrayDims, alloc);
	ListSRInterface_free(&interfaces, alloc);
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
