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

//formats/oiPL/pl_write.c

#include "formats/oiPL/pl_file.h"
#include "types/container/stream.h"
#include "types/math/vec4i.h"
#include "types/base/error.h"

Bool PLFile_write(const PLFile *plFile, const Allocator *alloc, StreamRef *streamRef, U64 *offset, Error *e_rr) {

	Bool s_uccess = true;
	StreamCursor cursor = (StreamCursor) { 0 };

	if(!plFile || !offset)
		retError(clean, Error_nullPointer(!plFile ? 0 : 3, "PLFile_write()::plFile and offset are required"));

	if(plFile->bindings.length > 255 || plFile->samplers.length > 255)
		retError(clean, Error_invalidOperation(0, "PLFile_write()::a layout is limited to 255 bindings and samplers"));

	OxStream *stream = streamRef ? RefPtr_data(streamRef, OxStream) : NULL;

	U64 headerSize = sizeof(PLHeader);

	if(!(plFile->flags & EPLSettingsFlags_HideMagicNumber))
		headerSize += sizeof(U32);

	headerSize += ListPLDescriptorBinding_bytes(plFile->bindings);
	headerSize += plFile->hasPushConstant ? sizeof(PLDescriptorBinding) : 0;
	headerSize += ListPLSamplerInfo_bytes(plFile->samplers);

	if (!stream) {
		*offset += headerSize;
		*offset = (*offset + 15) & ~(U64)15;
		gotoIfError3(clean, DLFile_write(&plFile->names, alloc, NULL, NULL, I32x4_zero(), offset, e_rr));
		goto clean;
	}

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, true, alloc, &cursor, e_rr));

	if(stream->reserve)
		gotoIfError3(clean, stream->reserve(stream, *offset + headerSize + 15, alloc, e_rr));

	const PLHeader header = (PLHeader) {
		.version = EPLVersion_V1_1,
		.flags = plFile->hasPushConstant ? EPLFlag_HasPushConstant : EPLFlag_None,
		.bindingCount = (U8) plFile->bindings.length,
		.samplerCount = (U8) plFile->samplers.length
	};

	if(!(plFile->flags & EPLSettingsFlags_HideMagicNumber))
		gotoIfError3(clean, StreamCursor_appendU32(&cursor, offset, PLHeader_MAGIC, alloc, e_rr));

	gotoIfError3(clean, StreamCursor_append(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	gotoIfError3(clean, StreamCursor_appendBuffer(
		&cursor, offset, ListPLDescriptorBinding_bufferConst(plFile->bindings), alloc, e_rr
	));

	if(plFile->hasPushConstant)
		gotoIfError3(clean, StreamCursor_append(
			&cursor, offset, &plFile->pushConstant, sizeof(plFile->pushConstant), alloc, e_rr
		));

	gotoIfError3(clean, StreamCursor_appendBuffer(
		&cursor, offset, ListPLSamplerInfo_bufferConst(plFile->samplers), alloc, e_rr
	));

	//The names oiDL starts 16-byte aligned

	U8 pad[16] = { 0 };
	const U64 utilized = *offset & 15;

	if(utilized)
		gotoIfError3(clean, StreamCursor_append(&cursor, offset, pad, 16 - utilized, alloc, e_rr));

	StreamCursor_close(&cursor, alloc);

	gotoIfError3(clean, DLFile_write(&plFile->names, alloc, streamRef, NULL, I32x4_zero(), offset, e_rr));

clean:
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
