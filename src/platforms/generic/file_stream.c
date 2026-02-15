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

#include "platforms/ext/listx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/constants.h"

static inline Bool FileStream_read(Stream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "FileStream_read()::stream is required"));

	(void) alloc;

	gotoIfError3(clean, FileHandle_read(&((FileStream*)stream)->handle, offset, length, buf, e_rr));

clean:
	return s_uccess;
}

static inline Bool FileStream_write(Stream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "FileStream_write()::stream is required"));

	(void) alloc;

	gotoIfError3(clean, FileHandle_write(&((FileStream*)stream)->handle, offset, length, buf, e_rr));

clean:
	return s_uccess;
}

static inline void FileStream_close(Stream *stream, const Allocator *alloc) {
	FileHandle_close(&((FileStream*)stream)->handle, alloc);
}

Bool File_openStream(
	const CharString *loc,
	Ns timeout,
	EFileOpenType type,
	Bool create,
	U64 cache,
	const Allocator *alloc,
	FileStream *stream,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocated = false;

	if(!loc || !stream)
		retError(clean, Error_nullPointer(!loc ? 0 : 6, "File_openStream()::loc and stream are required"));

	gotoIfError3(clean, File_open(*loc, timeout, type, create, alloc, &stream->handle, e_rr));
	allocated = true;

	Bool read = type == EFileOpenType_Read;		//TODO: Readwrite
	Bool write = type == EFileOpenType_Write;

	gotoIfError3(clean, Stream_create(
		read ? FileStream_read : NULL,
		write ? FileStream_write : NULL,
		FileStream_close,
		cache,
		stream->handle.fileSize,
		alloc,
		&stream->parent,
		e_rr
	));

clean:

	if(allocated && !s_uccess)
		FileHandle_close(&stream->handle, alloc);

	return s_uccess;
}

Bool FileHandle_openStream(FileHandle *handle, U64 cache, const Allocator *alloc, FileStream *stream, Error *e_rr) {

	Bool s_uccess = true;

	if(!handle || !handle->ext || !stream)
		retError(clean, Error_nullPointer(
			!handle || !handle->ext ? 0 : 5, "FileHandle_openStream()::handle and stream are required"
		));

	if(stream->handle.ext)
		retError(clean, Error_invalidParameter(5, 0, "FileHandle_openStream()::stream already defined, might be a memleak"));

	FileOpenType access = handle->type;
	Bool read = access == EFileOpenType_Read;		//TODO: Readwrite
	Bool write = access == EFileOpenType_Write;

	gotoIfError3(clean, Stream_create(
		read ? FileStream_read : NULL,
		write ? FileStream_write : NULL,
		FileStream_close,
		cache,
		handle->fileSize,
		alloc,
		&stream->parent,
		e_rr
	));

	stream->handle = *handle;
	*handle = (FileHandle) { 0 };

clean:
	return s_uccess;
}

Bool File_openStreamx(
	const CharString *loc,
	Ns timeout,
	EFileOpenType type,
	Bool create,
	U64 cache,
	FileStream *output,
	Error *e_rr
) {
	return File_openStream(loc, timeout, type, create, cache, Platform_instance->alloc, output, e_rr);
}

Bool FileHandle_openStreamx(FileHandle *handle, U64 cache, FileStream *stream, Error *e_rr) {
	return FileHandle_openStream(handle, cache, Platform_instance->alloc, stream, e_rr);
}
