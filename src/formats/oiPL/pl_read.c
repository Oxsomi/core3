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

//formats/oiPL/pl_read.c

#include "formats/oiPL/pl_file.h"
#include "types/container/stream.h"
#include "types/math/vec4.h"
#include "types/base/error.h"

Bool PLFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, PLFile *plFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool didAllocate = false;
	StreamCursor cursor = (StreamCursor) { 0 };

	//Read into locals first so a validation failure can't leave a half-built plFile

	ListPLDescriptorBinding bindings = (ListPLDescriptorBinding) { 0 };
	ListPLSamplerInfo samplers = (ListPLSamplerInfo) { 0 };
	DLFile names = (DLFile) { 0 };

	if(!offset || !plFile)
		retError(clean, Error_nullPointer(!offset ? 1 : 4, "PLFile_read()::plFile and offset are required"));

	if(plFile->bindings.length || plFile->names.entryStrings.length)
		retError(clean, Error_invalidOperation(0, "PLFile_read()::plFile isn't empty, might indicate memleak"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursor, e_rr));

	if (!isSubFile) {

		U32 magic = 0;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, offset, &magic, alloc, e_rr));

		if(magic != PLHeader_MAGIC)
			retError(clean, Error_invalidState(0, "PLFile_read()::file didn't start with oiPL"));
	}

	PLHeader header;
	gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	if(header.version != EPLVersion_V1_1 || (header.flags & EPLFlag_Unsupported))
		retError(clean, Error_invalidState(0, "PLFile_read()::file didn't have the right version or flags"));

	const Bool hasPushConstant = header.flags & EPLFlag_HasPushConstant;

	gotoIfError3(clean, ListPLDescriptorBinding_resize(&bindings, header.bindingCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListPLDescriptorBinding_buffer(bindings), alloc, e_rr));

	PLDescriptorBinding pushConstant = (PLDescriptorBinding) { 0 };

	if(hasPushConstant)
		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &pushConstant, sizeof(pushConstant), alloc, e_rr));

	gotoIfError3(clean, ListPLSamplerInfo_resize(&samplers, header.samplerCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListPLSamplerInfo_buffer(samplers), alloc, e_rr));

	//The names oiDL starts 16-byte aligned

	const U64 aligned = (*offset + 15) & ~15;

	if (aligned != *offset) {
		U8 padDiscard[16];
		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, padDiscard, aligned - *offset, alloc, e_rr));
	}

	gotoIfError3(clean, DLFile_read(streamRef, offset, NULL, I32x4_zero(), true, false, alloc, NULL, &names, e_rr));
	didAllocate = true;

	if(
		names.settings.dataType != EDLDataType_String ||
		names.settings.encryptionType ||
		names.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "PLFile_read() names didn't match expectations"));

	const U64 nameCount = names.entryStrings.length;

	for (U64 i = 0; i < nameCount; ++i) {

		if(!DLFile_isFullyLoaded(&names, i))
			retError(clean, Error_invalidParameter(0, 1, "PLFile_read() one of the strings wasn't fully loaded"));

		if(CharString_length(names.entryStrings.ptr[i]) >= 32768)
			retError(clean, Error_invalidParameter(0, 1, "PLFile_read() one of the strings exceeded the length limit"));
	}

	for(U64 i = 0; i < bindings.length; ++i)
		gotoIfError3(clean, PLDescriptorBinding_validate(&bindings.ptr[i], nameCount, samplers.length, false, e_rr));

	if(hasPushConstant)
		gotoIfError3(clean, PLDescriptorBinding_validate(&pushConstant, nameCount, samplers.length, false, e_rr));

	for(U64 i = 0; i < samplers.length; ++i)
		gotoIfError3(clean, PLSamplerInfo_validate(&samplers.ptr[i], e_rr));

	*plFile = (PLFile) {
		.names = names,
		.bindings = bindings,
		.samplers = samplers,
		.pushConstant = pushConstant,
		.hasPushConstant = hasPushConstant,
		.flags = isSubFile ? EPLSettingsFlags_HideMagicNumber : EPLSettingsFlags_None
	};

	names = (DLFile) { 0 };
	bindings = (ListPLDescriptorBinding) { 0 };
	samplers = (ListPLSamplerInfo) { 0 };

	gotoIfError3(clean, PLFile_finalize(plFile, alloc, e_rr));

clean:

	if(didAllocate)
		DLFile_free(&names, alloc);

	ListPLDescriptorBinding_free(&bindings, alloc);
	ListPLSamplerInfo_free(&samplers, alloc);
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
