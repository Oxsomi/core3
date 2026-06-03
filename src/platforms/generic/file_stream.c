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

//platforms/generic/file_stream.c

#include "platforms/file.h"
#include "platforms/platform.h"
#include "types/base/buffer_base.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/constants.h"

static Bool FileStream_read(
	OxStream *stream,
	U64 offset,
	U64 length,
	Buffer buf,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "FileStream_read()::stream is required"));

	(void) alloc;

	gotoIfError3(clean, FileHandleRef_read(((FileStream*)stream)->handle, offset, length, &buf, e_rr));

clean:
	return s_uccess;
}

static inline Bool FileStream_write(OxStream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr) {
	
	Bool s_uccess = true;

	if(!stream)
		retError(clean, Error_nullPointer(0, "FileStream_write()::stream is required"));

	(void) alloc;

	gotoIfError3(clean, FileHandleRef_write(((FileStream*)stream)->handle, offset, length, &buf, e_rr));

clean:
	return s_uccess;
}

static void FileStream_close(OxStream *stream, const Allocator *alloc) {
	(void) alloc;
	RefPtr_dec(&((FileStream*)stream)->handle);
}

RefPtrType FileStream_makeType(const Allocator *alloc) {
	return Stream_inheritType(alloc, sizeof(FileStream) - sizeof(OxStream));
}

Bool File_openStream(
	const CharString *loc,
	Ns timeout,
	EFileOpenType type,
	Bool create,
	const RefPtrType *fileHandleType,
	const RefPtrType *streamType,
	StreamRef **stream,
	Error *e_rr
) {
	Bool s_uccess = true;
	FileHandleRef *handle = NULL;

	gotoIfError3(clean, File_open(loc, timeout, type, create, fileHandleType, &handle, e_rr));
	gotoIfError3(clean, FileHandle_openStream(&handle, streamType, stream, e_rr));

clean:
	RefPtr_dec(&handle);
	return s_uccess;
}

Bool FileHandle_openStream(
	FileHandleRef **handle,
	const RefPtrType *streamType,
	StreamRef **stream,
	Error *e_rr
) {
	Bool s_uccess = true;

	if(!handle || !*handle)
		retError(clean, Error_nullPointer(0, "FileHandle_openStream()::handle is required"));

	if(!stream)
		retError(clean, Error_nullPointer(3, "FileHandle_openStream()::stream is required"));

	if(*stream)
		retError(clean, Error_invalidOperation(0, "FileHandle_openStream()::stream already defined, might be a memleak"));

	const FileHandle *fh = RefPtr_data(*handle, FileHandle);
	EFileOpenType type = FileHandle_fileType(fh);
	Bool isRead  = EFileOpenType_isRead(type);
	Bool isWrite = EFileOpenType_isWrite(type);
	U64 fileSize = FileHandle_fileSize(fh);

	gotoIfError3(clean, Stream_create(
		isRead  ? FileStream_read  : NULL,
		isWrite ? FileStream_write : NULL,
		NULL,                        //reserve
		FileStream_close,
		fileSize,
		EStreamType_File | (isWrite ? EStreamType_Resizable : 0),
		streamType,
		stream,
		e_rr
	));

	//Transfer handle ownership into the FileStream, caller's ref becomes NULL
	FileStream *fs = RefPtr_data(*stream, FileStream);
	fs->handle = *handle;
	*handle = NULL;

clean:
	return s_uccess;
}
