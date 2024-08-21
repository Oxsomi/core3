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

#include "types/archive.h"
#include "types/allocator.h"
#include "types/list_impl.h"
#include "types/math.h"

TListImpl(ArchiveEntry);

Bool Archive_create(Allocator alloc, Archive *archive, Error *e_rr) {

	Bool s_uccess = true;

	if(!archive)
		retError(clean, Error_nullPointer(1, "Archive_create()::archive is required"))

	if(archive->entries.ptr)
		retError(clean, Error_invalidOperation(
			0, "Archive_create()::archive is already initialized, indicates possible memleak"
		))

	gotoIfError2(clean, ListArchiveEntry_reserve(&archive->entries, 100, alloc))

clean:
	return s_uccess;
}

Bool Archive_free(Archive *archive, Allocator alloc) {

	if(!archive || !archive->entries.ptr)
		return true;

	for (U64 i = 0; i < archive->entries.length; ++i) {
		ArchiveEntry entry = archive->entries.ptr[i];
		Buffer_free(&entry.data, alloc);
		CharString_free(&entry.path, alloc);
	}

	ListArchiveEntry_free(&archive->entries, alloc);
	*archive = (Archive) { 0 };
	return true;
}

Bool Archive_createCopy(Archive a, Allocator alloc, Archive *archive, Error *e_rr) {
	
	Bool s_uccess = true;
	Bool allocate = false;

	if(!archive)
		retError(clean, Error_nullPointer(4, "Archive_combine()::combined is required"))

	if(archive->entries.ptr)
		retError(clean, Error_invalidParameter(4, 0, "Archive_combine()::combined contains data, which could indicate a memleak"))

	gotoIfError2(clean, ListArchiveEntry_createCopy(a.entries, alloc, &archive->entries))
	allocate = true;

	for(U64 i = 0; i < archive->entries.length; ++i) {						//Reset state, before creating copy of buffers and strings
		archive->entries.ptrNonConst[i].data = Buffer_createNull();
		archive->entries.ptrNonConst[i].path = CharString_createNull();
	}

	for(U64 i = 0; i < archive->entries.length; ++i) {

		ArchiveEntry *dst = &archive->entries.ptrNonConst[i];
		ArchiveEntry src = a.entries.ptr[i];

		gotoIfError2(clean, CharString_createCopy(src.path, alloc, &dst->path))
		gotoIfError2(clean, Buffer_createCopy(src.data, alloc, &dst->data))
	}

clean:
	
	if(allocate && !s_uccess)
		Archive_free(archive, alloc);

	return s_uccess;
}

Bool Archive_getPath(
	Archive archive,
	CharString path,
	ArchiveEntry *entryOut,
	U64 *iPtr,
	CharString *resolvedPathPtr,
	Allocator alloc,
	Error *e_rr
) {

	Bool isVirtual = false;
	Bool s_uccess = true;
	CharString resolvedPath = CharString_createNull();

	if(!archive.entries.ptr || !CharString_length(path))
		retError(clean, Error_nullPointer(!archive.entries.ptr ? 0 : 1, "Archive_getPath()::archive and path are required"))

	gotoIfError3(clean, File_resolve(path, &isVirtual, 128, CharString_createNull(), alloc, &resolvedPath, e_rr))

	if (isVirtual)
		retError(clean, Error_invalidState(0, "Archive_getPath()::path was virtual, not allowed in an archive"))

	//TODO: Optimize this with a hashmap

	for(U64 i = 0; i < archive.entries.length; ++i)
		if (CharString_equalsStringInsensitive(archive.entries.ptr[i].path, resolvedPath)) {

			if(entryOut && !CharString_length(entryOut->path))
				*entryOut = archive.entries.ptr[i];

			if(iPtr)
				*iPtr = i;

			if(resolvedPathPtr && !resolvedPathPtr->ptr) {
				*resolvedPathPtr = resolvedPath;
				resolvedPath = CharString_createNull();		//Moved
			}

			else CharString_free(&resolvedPath, alloc);

			goto clean;
		}

	CharString_free(&resolvedPath, alloc);
	retError(clean, Error_notFound(0, 0, "Archive_getPath() path was not found"))

clean:
	CharString_free(&resolvedPath, alloc);
	return s_uccess;
}

Bool Archive_combine(Archive a, Archive b, ArchiveCombineSettings settings, Allocator alloc, Archive *combined, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocate = false;

	ListU64 movedBEntries = (ListU64) { 0 };
	CharString renamed = CharString_createNull();

	if(a.entries.length >> 63 || b.entries.length >> 63)
		retError(clean, Error_invalidState(0, "Archive_combine()::a and b should be 63 bit"))

	if(settings.mode == EArchiveCombineMode_Rename)
		gotoIfError2(clean, ListU64_create(b.entries.length, alloc, &movedBEntries))

	gotoIfError3(clean, Archive_createCopy(a, alloc, combined, e_rr))

	for (U64 i = 0; i < b.entries.length; ++i) {

		ArchiveEntry bi = b.entries.ptr[i];

		ArchiveEntry ai = (ArchiveEntry) { 0 };
		U64 aIndex = 0;

		if(!Archive_getPath(a, bi.path, &ai, &aIndex, NULL, alloc, NULL))
			goto insert;

		ArchiveEntry *finalDst = &combined->entries.ptrNonConst[aIndex];

		//Conflict file type has no solution that isn't defined by combine mode

		if(ai.type != bi.type)
			retError(clean, Error_invalidState(
				0, "Archive_combine()::a and b had file that was mismatching in file type"
			))

		Bool conflict = false;

		//Depending on mode, timestamp could indicate a conflict
			
		if (ai.timestamp != bi.timestamp) {

			//Folders can safely be merged

			if(ai.type == EFileType_Folder)
				finalDst->timestamp = U64_max(ai.timestamp, bi.timestamp);

			//If latest should be accepted, there won't be a problem

			else if (settings.flags & EArchiveCombineFlags_ResolveAcceptLatest) {

				if(Buffer_neq(ai.data, bi.data)) {

					//Two timestamps the same means there is no latest

					if(ai.timestamp == bi.timestamp)
						conflict = true;

					else if(ai.timestamp < bi.timestamp) {
						Buffer_free(&finalDst->data, alloc);
						gotoIfError2(clean, Buffer_createCopy(bi.data, alloc, &finalDst->data))
					}
				}

				finalDst->timestamp = U64_max(ai.timestamp, bi.timestamp);
			}

			//If timestamps should be maintained, there's no solution without mode

			else if (!(settings.flags & EArchiveCombineFlags_ResolveLatestTimestamp))
				conflict = true;

			//Conflict by data, not resolvable without mode

			else if(Buffer_neq(ai.data, bi.data))
				conflict = true;

			//No conflict, accept newest timestamp

			else finalDst->timestamp = U64_max(ai.timestamp, bi.timestamp);
		}

		//Otherwise we only have a conflict if buffer mismatches

		else if(Buffer_neq(ai.data, bi.data))
			conflict = true;

		//Resolve the conflict

		if (conflict)
			switch (settings.mode) {

				default:
					retError(clean, Error_invalidState(
						0, "Archive_combine()::settings.mode is invalid"
					))

				case EArchiveCombineMode_AcceptA:		//No-op
					break;

				case EArchiveCombineMode_AcceptB:
					finalDst->timestamp = bi.timestamp;
					Buffer_free(&finalDst->data, alloc);
					gotoIfError2(clean, Buffer_createCopy(bi.data, alloc, &finalDst->data))
					break;

				case EArchiveCombineMode_RequireSame:
					retError(clean, Error_invalidState(
						0, "Archive_combine()::a and b had matching file paths, but mismatching contents"
					))

				//TODO: Insert -1, -2, -3, -4 before the extension and try until it works
				//		If ends with - number, try to increment the number.

				case EArchiveCombineMode_Rename: {

					//-N with potentially .extension (e.g. -1.oiSH, -2.oiSH)
					//Remember N, so we can increment.

					U64 counter = 0;
					U64 startCounter = CharString_findLastSensitive(bi.path, '-', 0, 0);

					if (startCounter != U64_MAX && C8_isDec(CharString_getAt(bi.path, startCounter + 1))) {

						U64 j = startCounter + 2;
						Bool match = false;

						for (; j < CharString_length(bi.path); ++j) {

							if (bi.path.ptr[j] == '.') {
								match = true;
								break;
							}

							if(!C8_isDec(bi.path.ptr[j]))
								break;
						}

						match |= j == CharString_length(bi.path);

						//Everything from startCounter + 1 -> j contains a number. Parse it

						if (match) {

							CharString num = CharString_createRefSizedConst(
								bi.path.ptr + startCounter + 1, j - startCounter - 1, false
							);

							if(!CharString_parseU64(num, &counter))
								retError(clean, Error_invalidState(0, "Archive_combine() parse U64 failed"))
						}
					}

					//Try to find the next until there's no next.
					//Example:
					//We already have:
					//	test.png
					//	test-1.png
					//But we insert test.png
					//try:		test.png
					//try:		test-1.png
					//found:	test-2.png

					CharString basePath = bi.path;
					CharString extension = CharString_createRefCStrConst("");

					U64 lastSlash = CharString_findFirstSensitive(bi.path, '/', 0, 0);

					if(lastSlash == U64_MAX)
						lastSlash = 0;

					U64 lastDot = CharString_findLastSensitive(bi.path, '.', lastSlash, 0);

					if(lastDot != U64_MAX) {
						basePath = CharString_createRefSizedConst(bi.path.ptr, lastDot, false);
						extension = CharString_createRefSizedConst(
							bi.path.ptr + lastDot,
							CharString_length(bi.path) - lastDot,
							CharString_isNullTerminated(bi.path)
						);
					}

					do {

						CharString_free(&renamed, alloc);

						++counter;

						gotoIfError2(clean, CharString_format(
							alloc, &renamed, "%.*s-%"PRIu64"%.*s",
							CharString_length(basePath), basePath.ptr,
							counter,
							CharString_length(extension), extension.ptr
						))
					}
					while(Archive_getPath(*combined, renamed, NULL, NULL, NULL, alloc, NULL));

					goto insert;
				}
			}

		else if(settings.mode == EArchiveCombineMode_Rename)
			movedBEntries.ptrNonConst[i] = aIndex;

		continue;

	insert:

		ArchiveEntry entryCopy = b.entries.ptr[i];
		entryCopy.data = Buffer_createNull();
		entryCopy.path = CharString_createNull();

		if((combined->entries.length + 1) >> 63)
			retError(clean, Error_outOfBounds(
				0, combined->entries.length + 1, (U64)1 << 63, "Archive_combine() final combined archive size should be 63 bit"
			))

		gotoIfError2(clean, ListArchiveEntry_pushBack(&combined->entries, entryCopy, alloc))

		ArchiveEntry *entryLast = ListArchiveEntry_last(combined->entries);

		if (renamed.ptr) {
			entryLast->path = renamed;
			renamed = CharString_createNull();
		}
		
		else gotoIfError2(clean, CharString_createCopy(bi.path, alloc, &entryLast->path))

		gotoIfError2(clean, Buffer_createCopy(bi.data, alloc, &entryLast->data))
		
		if(settings.mode == EArchiveCombineMode_Rename)
			movedBEntries.ptrNonConst[i] = combined->entries.length - 1;
	}

clean:
	
	if(allocate && !s_uccess)
		Archive_free(combined, alloc);

	ListU64_free(&movedBEntries, alloc);
	CharString_free(&renamed, alloc);
	return s_uccess;
}

Bool Archive_has(Archive archive, CharString path, Allocator alloc) {
	return Archive_getPath(archive, path, NULL, NULL, NULL, alloc, NULL);
}

Bool Archive_hasFile(Archive archive, CharString path, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc, NULL))
		return false;

	return entry.type == EFileType_File;
}

Bool Archive_hasFolder(Archive archive, CharString path, Allocator alloc) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc, NULL))
		return false;

	return entry.type == EFileType_Folder;
}

Bool Archive_addInternal(Archive *archive, ArchiveEntry entry, Bool successIfExists, Allocator alloc, Error *e_rr);

Bool Archive_createOrFindParent(Archive *archive, CharString path, Allocator alloc) {

	//If it doesn't contain / then we are already at the root
	//So we don't need to create a parent

	CharString substr = CharString_createNull();
	if (!CharString_cutAfterLastSensitive(path, '/', &substr))
		return true;

	//Try to add parent (returns true if already exists)

	const ArchiveEntry entry = (ArchiveEntry) {
		.path = substr,
		.type = EFileType_Folder
	};

	return Archive_addInternal(archive, entry, true, alloc, NULL);
}

Bool Archive_addInternal(Archive *archive, ArchiveEntry entry, Bool successIfExists, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	ArchiveEntry out = (ArchiveEntry) { 0 };

	if (!archive || !archive->entries.ptr)
		retError(clean, Error_nullPointer(0, "Archive_addInternal()::archive is required"))

	//If folder already exists, we're done

	if (Archive_getPath(*archive, entry.path, &out, NULL, NULL, alloc, NULL)) {

		if(out.type != entry.type || !successIfExists)
			retError(clean, Error_alreadyDefined(0, "Archive_addInternal()::entry.path already exists"))

		goto clean;
	}

	//Resolve

	Bool isVirtual = false;
	gotoIfError3(clean, File_resolve(entry.path, &isVirtual, 128, CharString_createNull(), alloc, &resolved, e_rr))

	if (isVirtual)
		retError(clean, Error_unsupportedOperation(0, "Archive_addInternal()::entry.path was virtual (//)"))

	entry.path = resolved;

	//Try to find a parent or make one

	if(!Archive_createOrFindParent(archive, entry.path, alloc))
		retError(clean, Error_notFound(0, 0, "Archive_addInternal()::entry.path parent couldn't be created"))

	gotoIfError2(clean, ListArchiveEntry_pushBack(&archive->entries, entry, alloc))
	resolved = CharString_createNull();

clean:

	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool Archive_addDirectory(Archive *archive, CharString path, Allocator alloc, Error *e_rr) {

	const ArchiveEntry entry = (ArchiveEntry) {
		.path = path,
		.type = EFileType_Folder
	};

	return Archive_addInternal(archive, entry, true, alloc, e_rr);
}

Bool Archive_addFile(Archive *archive, CharString path, Buffer *data, Ns timestamp, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!data)
		retError(clean, Error_nullPointer(2, "Archive_addFile()::data is required"))

	const ArchiveEntry entry = (ArchiveEntry) {
		.path = path,
		.type = EFileType_File,
		.data = *data,
		.timestamp = timestamp
	};

	gotoIfError3(clean, Archive_addInternal(archive, entry, false, alloc, e_rr))
	*data = Buffer_createNull();		//Moved

clean:
	return s_uccess;
}

Bool Archive_removeInternal(Archive *archive, CharString path, Allocator alloc, EFileType type, Error *e_rr) {

	Bool s_uccess = true;
	ArchiveEntry entry = (ArchiveEntry) { 0 };
	U64 i = 0;
	CharString resolved = CharString_createNull();

	if (!archive || !archive->entries.ptr)
		retError(clean, Error_nullPointer(0, "Archive_removeInternal()::archive is required"))

	if(!Archive_getPath(*archive, path, &entry, &i, &resolved, alloc, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_removeInternal()::path doesn't exist"))

	if(type != EFileType_Any && entry.type != type)
		retError(clean, Error_invalidOperation(0, "Archive_removeInternal()::type doesn't match file type"))

	//Remove children

	if (entry.type == EFileType_Folder) {

		//Get myFolder/*

		gotoIfError2(clean, CharString_append(&resolved, '/', alloc))

		//Remove

		for (U64 j = archive->entries.length - 1; j != U64_MAX; --j) {

			const ArchiveEntry cai = archive->entries.ptr[i];

			if(!CharString_startsWithStringInsensitive(cai.path, resolved, 0))
				continue;

			//Free and remove from array

			Buffer_free(&entry.data, alloc);
			CharString_free(&entry.path, alloc);

			gotoIfError2(clean, ListArchiveEntry_popLocation(&archive->entries, j, NULL))

			//Ensure our *self* id still makes sense

			if(j < i)
				--i;
		}
	}

	//Remove

	Buffer_free(&entry.data, alloc);
	CharString_free(&entry.path, alloc);

	gotoIfError2(clean, ListArchiveEntry_popLocation(&archive->entries, i, NULL))

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool Archive_removeFile(Archive *archive, CharString path, Allocator alloc, Error *e_rr) {
	return Archive_removeInternal(archive, path, alloc, EFileType_File, e_rr);
}

Bool Archive_removeFolder(Archive *archive, CharString path, Allocator alloc, Error *e_rr) {
	return Archive_removeInternal(archive, path, alloc, EFileType_Folder, e_rr);
}

Bool Archive_remove(Archive *archive, CharString path, Allocator alloc, Error *e_rr) {
	return Archive_removeInternal(archive, path, alloc, EFileType_Any, e_rr);
}

Bool Archive_rename(Archive *archive, CharString loc, CharString newFileName, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolvedLoc = CharString_createNull();

	if (!archive || !archive->entries.ptr)
		retError(clean, Error_nullPointer(0, "Archive_rename()::archive is required"))

	if (!CharString_isValidFileName(newFileName))
		retError(clean, Error_invalidParameter(1, 0, "Archive_rename()::newFileName isn't a valid filename"))

	U64 i = 0;
	if (!Archive_getPath(*archive, loc, NULL, &i, &resolvedLoc, alloc, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_rename()::loc couldn't be resolved to path"))

	//Rename

	CharString *prevPath = &archive->entries.ptrNonConst[i].path;
	CharString subStr = CharString_createNull();

	CharString_cutAfterLastSensitive(*prevPath, '/', &subStr);
	prevPath->lenAndNullTerminated = CharString_length(subStr);

	gotoIfError2(clean, CharString_appendString(prevPath, newFileName, alloc))

clean:
	CharString_free(&resolvedLoc, alloc);
	return s_uccess;
}

Bool Archive_move(Archive *archive, CharString loc, CharString directoryName, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	U64 i = 0;
	ArchiveEntry parent = (ArchiveEntry) { 0 };

	if (!archive || !archive->entries.ptr)
		retError(clean, Error_nullPointer(0, "Archive_move()::archive is required"))

	if (!Archive_getPath(*archive, loc, NULL, &i, NULL, alloc, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_move()::loc couldn't be resolved to path"))

	if (!Archive_getPath(*archive, directoryName, &parent, NULL, &resolved, alloc, e_rr))
		retError(clean, Error_notFound(0, 2, "Archive_move()::directoryName couldn't be resolved to path"))

	if (parent.type != EFileType_Folder)
		retError(clean, Error_invalidOperation(0, "Archive_move()::directoryName should resolve to folder file"))

	CharString *filePath = &archive->entries.ptrNonConst[i].path;

	const U64 v = CharString_findLastSensitive(*filePath, '/', 0, 0);

	if (v != U64_MAX)
		gotoIfError2(clean, CharString_popFrontCount(filePath, v + 1))

	if (CharString_length(directoryName)) {
		gotoIfError2(clean, CharString_insert(filePath, '/', 0, alloc))
		gotoIfError2(clean, CharString_insertString(filePath, directoryName, 0, alloc))
	}

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool Archive_getInfo(Archive archive, CharString path, FileInfo *info, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	ArchiveEntry entry = (ArchiveEntry) { 0 };
	CharString resolved = CharString_createNull();

	if(!archive.entries.ptr || !info)
		retError(clean, Error_nullPointer(!info ? 2 : 0, "Archive_getInfo()::archive and info are required"))

	if(!Archive_getPath(archive, path, &entry, NULL, &resolved, alloc, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_getInfo()::path couldn't resolve to path"))

	*info = (FileInfo) {
		.access = Buffer_isConstRef(entry.data) ? EFileAccess_Read : EFileAccess_ReadWrite,
		.fileSize = Buffer_length(entry.data),
		.timestamp = entry.timestamp,
		.type = entry.type,
		.path = resolved
	};

clean:
	return s_uccess;
}

U64 Archive_getIndex(Archive archive, CharString path, Allocator alloc) {
	U64 v = U64_MAX;
	Archive_getPath(archive, path, NULL, &v, NULL, alloc, NULL);
	return v;
}

Bool Archive_updateFileData(Archive *archive, CharString path, Buffer data, Allocator alloc, Error *e_rr) {

	Bool s_uccess = true;
	ArchiveEntry entry = (ArchiveEntry) { 0 };
	U64 i = 0;

	if (!archive || !archive->entries.ptr)
		retError(clean, Error_nullPointer(0, "Archive_updateFileData()::archive is required"))

	if(!Archive_getPath(*archive, path, &entry, &i, NULL, alloc, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_updateFileData()::path couldn't resolve to path"))

	Buffer_free(&entry.data, alloc);
	archive->entries.ptrNonConst[i].data = data;

clean:
	return s_uccess;
}

Bool Archive_getFileDataInternal(
	Archive archive,
	CharString path,
	Buffer *data,
	Allocator alloc,
	Bool isConst,
	Error *e_rr
) {

	Bool s_uccess = true;
	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if (!archive.entries.ptr || !data)
		retError(clean, Error_nullPointer(!data ? 2 : 0, "Archive_getFileDataInternal()::archive and data are required"))

	if(data->ptr)
		retError(clean, Error_invalidParameter(
			2, 0, "Archive_getFileDataInternal()::data wasn't empty, might indicate memleak"
		))

	if(!Archive_getPath(archive, path, &entry, NULL, NULL, alloc, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_getFileDataInternal()::path couldn't be resolved to path"))

	if (entry.type != EFileType_File)
		retError(clean, Error_invalidOperation(
			0, "Archive_getFileDataInternal()::entry.type isn't file, can't get data of folder"
		))

	if(isConst)
		*data = Buffer_createRefConst(entry.data.ptr, Buffer_length(entry.data));

	else if(Buffer_isConstRef(entry.data))
		retError(clean, Error_constData(1, 0, "Archive_getFileDataInternal()::entry.data should be writable"))

	else *data = Buffer_createRef((U8*)entry.data.ptr, Buffer_length(entry.data));

clean:
	return s_uccess;
}

Bool Archive_getFileData(Archive archive, CharString path, Buffer *data, Allocator alloc, Error *e_rr) {
	return Archive_getFileDataInternal(archive, path, data, alloc, false, e_rr);
}

Bool Archive_getFileDataConst(Archive archive, CharString path, Buffer *data, Allocator alloc, Error *e_rr) {
	return Archive_getFileDataInternal(archive, path, data, alloc, true, e_rr);
}

Bool Archive_foreach(
	Archive archive,
	CharString loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Allocator alloc,
	Error *e_rr
) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	Bool isVirtual = false;

	if(!archive.entries.ptr || !callback)
		retError(clean, Error_nullPointer(!callback ? 3 : 0, "Archive_foreach()::archive and callback are required"))

	if(type > EFileType_Any)
		retError(clean, Error_invalidEnum(
			5, (U64)type, (U64)EFileType_Any, "Archive_foreach()::type should be file, folder or any"
		))

	gotoIfError3(clean, File_resolve(loc, &isVirtual, 128, CharString_createNull(), alloc, &resolved, e_rr))

	if(isVirtual)
		retError(clean, Error_invalidOperation(0, "Archive_foreach()::path can't start with start with // (virtual)"))

	//Append / (replace last \0)

	if(CharString_length(resolved))									//Ignore root
		gotoIfError2(clean, CharString_append(&resolved, '/', alloc))

	const U64 baseSlash = isRecursive ? 0 : CharString_countAllSensitive(resolved, '/', 0);

	//TODO: Have a map where it's easy to find child files/folders.
	//		For now we'll have to loop over every file.
	//		Because our files are dynamic, so we don't want to reorder those every time.
	//		Maybe we should have Archive_optimize which is called before writing or if this functionality should be used.

	for (U64 i = 0; i < archive.entries.length; ++i) {

		const ArchiveEntry cai = archive.entries.ptr[i];

		if(type != EFileType_Any && type != cai.type)
			continue;

		if(!CharString_startsWithStringInsensitive(cai.path, resolved, 0))
			continue;

		//It contains at least one sub dir

		if(!isRecursive && baseSlash != CharString_countAllSensitive(cai.path, '/', 0))
			continue;

		FileInfo info = (FileInfo) {
			.path = cai.path,
			.type = cai.type
		};

		if (cai.type == EFileType_File) {
			info.access = Buffer_isConstRef(cai.data) ? EFileAccess_Read : EFileAccess_ReadWrite,
			info.fileSize = Buffer_length(cai.data);
			info.timestamp = cai.timestamp;
		}

		gotoIfError3(clean, callback(info, userData, e_rr))
	}

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool countFile(FileInfo info, U64 *res, Error *e_rr) {
	(void)info; (void) e_rr;
	++*res;
	return true;
}

Bool Archive_queryFileObjectCount(
	Archive archive,
	CharString loc,
	EFileType type,
	Bool isRecursive,
	U64 *res,
	Allocator alloc,
	Error *e_rr
) {

	if(!res) {

		if(e_rr)
			*e_rr = Error_nullPointer(4, "Archive_queryFileObjectCount()::res is required");

		return false;
	}

	return Archive_foreach(
		archive,
		loc,
		(FileCallback) countFile,
		res,
		isRecursive,
		type,
		alloc,
		e_rr
	);
}

Bool Archive_queryFileEntryCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc,
	Error *e_rr
) {
	return Archive_queryFileObjectCount(archive, loc, EFileType_Any, isRecursive, res, alloc, e_rr);
}

Bool Archive_queryFileCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc,
	Error *e_rr
) {
	return Archive_queryFileObjectCount(archive, loc, EFileType_File, isRecursive, res, alloc, e_rr);
}

Bool Archive_queryFolderCount(
	Archive archive,
	CharString loc,
	Bool isRecursive,
	U64 *res,
	Allocator alloc,
	Error *e_rr
) {
	return Archive_queryFileObjectCount(archive, loc, EFileType_Folder, isRecursive, res, alloc, e_rr);
}
