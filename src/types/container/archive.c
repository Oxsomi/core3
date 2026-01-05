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

#include "types/container/archive.h"
#include "types/base/allocator.h"
#include "types/base/c8.h"
#include "types/base/constants.h"
#include "types/container/list_impl.h"
#include "types/base/math.h"
#include "types/container/log.h"

TListImpl(ArchiveEntry);

Bool Archive_create(const Allocator *alloc, Archive *archive, Error *e_rr) {

	Bool s_uccess = true;

	if(!archive)
		retError(clean, Error_nullPointer(1, "Archive_create()::archive is required"));

	if(archive->entries.ptr)
		retError(clean, Error_invalidOperation(
			0, "Archive_create()::archive is already initialized, indicates possible memleak"
		));

	gotoIfError3(clean, ListArchiveEntry_reserve(&archive->entries, 100, alloc, e_rr));

clean:
	return s_uccess;
}

void Archive_free(Archive *archive, const Allocator *alloc) {

	if(!archive || !archive->entries.ptr)
		return;

	for (U64 i = 0; i < archive->entries.length; ++i) {
		ArchiveEntry entry = archive->entries.ptr[i];
		Buffer_free(&entry.data, alloc);
		CharString_free(&entry.path, alloc);
	}

	ListArchiveEntry_free(&archive->entries, alloc);
	*archive = (Archive) { 0 };
}

Bool Archive_createCopy(const Archive *a, const Allocator *alloc, Archive *archive, Error *e_rr) {

	Bool s_uccess = true;
	Bool allocate = false;

	if(!a || !archive)
		retError(clean, Error_nullPointer(!a ? 0 : 4, "Archive_combine()::a and combined are required"));

	if(archive->entries.ptr)
		retError(clean, Error_invalidParameter(
			4, 0, "Archive_combine()::combined contains data, which could indicate a memleak"
		));

	ListArchiveEntry aEntries = a->entries;
	ListArchiveEntry *archiveEntries = &archive->entries;

	gotoIfError3(clean, ListArchiveEntry_createCopy(aEntries, alloc, archiveEntries, e_rr));
	allocate = true;

	for(U64 i = 0; i < archiveEntries->length; ++i) {						//Reset state, before creating copy of buffers and strings
		archiveEntries->ptrNonConst[i].data = Buffer_createNull();
		archiveEntries->ptrNonConst[i].path = CharString_createNull();
	}

	for(U64 i = 0; i < archive->entries.length; ++i) {

		ArchiveEntry *dst = &archiveEntries->ptrNonConst[i];
		ArchiveEntry src = aEntries.ptr[i];

		gotoIfError3(clean, CharString_createCopy(src.path, alloc, &dst->path, e_rr));
		gotoIfError3(clean, Buffer_createCopy(src.data, alloc, &dst->data, e_rr));
	}

clean:

	if(allocate && !s_uccess)
		Archive_free(archive, alloc);

	return s_uccess;
}

static inline Bool Archive_getPath(
	const ArchiveOptionsConst *archive,
	ArchiveEntry *entryOut,
	U64 *iPtr,
	CharString *resolvedPathPtr,
	Error *e_rr
) {

	Bool isVirtual = false;
	Bool s_uccess = true;
	CharString resolvedPath = CharString_createNull();

	Bool noArchive = !archive || !archive->archive || !archive->archive->entries.ptr;

	if (noArchive || !archive->path || !CharString_length(*archive->path))
		retError(clean, Error_nullPointer(
			noArchive ? 0 : 1, "Archive_getPath()::archive, archive->archive and archive->path are required"
		));

	ListArchiveEntry entries = archive->archive->entries;

	gotoIfError3(clean, File_resolve(
		*archive->path, &isVirtual, 128, CharString_createNull(), archive->alloc, &resolvedPath, e_rr
	));

	if (isVirtual)
		retError(clean, Error_invalidState(0, "Archive_getPath()::path was virtual, not allowed in an archive"));

	//TODO: Optimize this with a hashmap

	for(U64 i = 0; i < entries.length; ++i)
		if (CharString_equalsStringInsensitive(entries.ptr[i].path, resolvedPath)) {

			if(entryOut && !CharString_length(entryOut->path))
				*entryOut = entries.ptr[i];

			if(iPtr)
				*iPtr = i;

			if(resolvedPathPtr && !resolvedPathPtr->ptr) {
				*resolvedPathPtr = resolvedPath;
				resolvedPath = CharString_createNull();		//Moved
			}

			goto clean;
		}

	retError(clean, Error_notFound(0, 0, "Archive_getPath() path was not found"));

clean:
	if(resolvedPath.ptr)
		CharString_free(&resolvedPath, archive->alloc);

	return s_uccess;
}

Bool Archive_combine(
	const Archive *a,
	const Archive *b,
	ArchiveCombineSettings settings,
	const Allocator *alloc,
	Archive *combined,
	Error *e_rr
) {

	Bool s_uccess = true;
	Bool allocate = false;

	ListU64 movedBEntries = (ListU64) { 0 };
	CharString renamed = CharString_createNull();

	if (!a || !b)
		retError(clean, Error_nullPointer(!a ? 0 : 1, "Archive_combine()::a and b are required"));

	const ListArchiveEntry aEntries = a->entries;
	const ListArchiveEntry bEntries = b->entries;

	if(aEntries.length >> 63 || bEntries.length >> 63)
		retError(clean, Error_invalidState(0, "Archive_combine()::a and b should be 63 bit"));

	if (settings.mode == EArchiveCombineMode_Rename)
		gotoIfError3(clean, ListU64_create(bEntries.length, alloc, &movedBEntries, e_rr));

	gotoIfError3(clean, Archive_createCopy(a, alloc, combined, e_rr));

	for (U64 i = 0; i < bEntries.length; ++i) {

		ArchiveEntry bi = bEntries.ptr[i];

		ArchiveEntry ai = (ArchiveEntry) { 0 };
		U64 aIndex = 0;

		ArchiveOptionsConst getPath = (ArchiveOptionsConst) {
			.archive = a,
			.path = &bi.path,
			.alloc = alloc
		};

		if(!Archive_getPath(&getPath, &ai, &aIndex, NULL, NULL))
			goto insert;

		ArchiveEntry *finalDst = &combined->entries.ptrNonConst[aIndex];

		//Conflict file type has no solution that isn't defined by combine mode

		if (ai.type != bi.type)
			retError(clean, Error_invalidState(
				0, "Archive_combine()::a and b had file that was mismatching in file type"
			));

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
						gotoIfError3(clean, Buffer_createCopy(bi.data, alloc, &finalDst->data, e_rr));
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
					));

				case EArchiveCombineMode_AcceptA:		//No-op
					break;

				case EArchiveCombineMode_AcceptB:
					finalDst->timestamp = bi.timestamp;
					Buffer_free(&finalDst->data, alloc);
					gotoIfError3(clean, Buffer_createCopy(bi.data, alloc, &finalDst->data, e_rr));
					break;

				case EArchiveCombineMode_RequireSame:
					retError(clean, Error_invalidState(
						0, "Archive_combine()::a and b had matching file paths, but mismatching contents"
					));

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

							if (!CharString_parseU64(num, &counter))
								retError(clean, Error_invalidState(0, "Archive_combine() parse U64 failed"));
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

					getPath.archive = combined;

					do {

						CharString_free(&renamed, alloc);

						++counter;

						gotoIfError3(clean, CharString_format(
							alloc, &renamed, "%.*s-%"PRIu64"%.*s",
							CharString_length(basePath), basePath.ptr,
							counter,
							CharString_length(extension), extension.ptr,
							e_rr
						));

						getPath.path = &renamed;
					}
					while(Archive_getPath(&getPath, NULL, NULL, NULL, NULL));

					goto insert;
				}
			}

		else if(settings.mode == EArchiveCombineMode_Rename)
			movedBEntries.ptrNonConst[i] = aIndex;

		continue;

	insert:

		ArchiveEntry entryCopy = bEntries.ptr[i];
		entryCopy.data = Buffer_createNull();
		entryCopy.path = CharString_createNull();

		if((combined->entries.length + 1) >> 63)
			retError(clean, Error_outOfBounds(
				0, combined->entries.length + 1, (U64)1 << 63, "Archive_combine() final combined archive size should be 63 bit"
			));

		gotoIfError3(clean, ListArchiveEntry_pushBack(&combined->entries, entryCopy, alloc, e_rr));

		ArchiveEntry *entryLast = ListArchiveEntry_last(combined->entries);

		if (renamed.ptr) {
			entryLast->path = renamed;
			renamed = CharString_createNull();
		}

		else gotoIfError3(clean, CharString_createCopy(bi.path, alloc, &entryLast->path, e_rr));

		gotoIfError3(clean, Buffer_createCopy(bi.data, alloc, &entryLast->data, e_rr));

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

Bool Archive_has(const ArchiveOptionsConst *archive) {
	return Archive_getPath(archive, NULL, NULL, NULL, NULL);
}

Bool Archive_hasFile(const ArchiveOptionsConst *archive) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, &entry, NULL, NULL, NULL))
		return false;

	return entry.type == EFileType_File;
}

Bool Archive_hasFolder(const ArchiveOptionsConst *archive) {

	ArchiveEntry entry = (ArchiveEntry) { 0 };

	if(!Archive_getPath(archive, &entry, NULL, NULL, NULL))
		return false;

	return entry.type == EFileType_Folder;
}

Bool Archive_addInternal(const ArchiveOptions *archive, const ArchiveEntry *entry, Bool successIfExists, Error *e_rr);

static inline Bool Archive_createOrFindParent(const ArchiveOptions *archive) {

	//If it doesn't contain / then we are already at the root
	//So we don't need to create a parent

	CharString substr = CharString_createNull();
	if (!CharString_cutAfterLastSensitive(archive->path, '/', &substr))
		return true;

	//Try to add parent (returns true if already exists)

	const ArchiveEntry entry = (ArchiveEntry) {
		.path = substr,
		.type = EFileType_Folder
	};

	return Archive_addInternal(archive, &entry, true, NULL);
}

Bool Archive_addInternal(const ArchiveOptions *archive, const ArchiveEntry *entry, Bool successIfExists, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	ArchiveEntry out = (ArchiveEntry) { 0 };
	const Allocator *alloc = NULL;

	if (!archive || !archive->archive || !archive->archive->entries.ptr)
		retError(clean, Error_nullPointer(0, "Archive_addInternal()::archive is required"));

	if(!entry)
		retError(clean, Error_nullPointer(1, "Archive_addInternal()::entry is required"));

	//If folder already exists, we're done

	if (Archive_getPath((const ArchiveOptionsConst*)archive, &out, NULL, NULL, NULL)) {

		if (out.type != entry->type || !successIfExists)
			retError(clean, Error_alreadyDefined(0, "Archive_addInternal() path already exists"));

		goto clean;
	}

	//Resolve

	Bool isVirtual = false;
	gotoIfError3(clean, File_resolve(&entry->path, &isVirtual, 128, CharString_createNull(), archive->alloc, &resolved, e_rr));
	alloc = archive->alloc;

	if (isVirtual)
		retError(clean, Error_unsupportedOperation(0, "Archive_addInternal()::entry.path was virtual (//)"))

	entry->path = resolved;

	//Try to find a parent or make one

	if(!Archive_createOrFindParent(archive))
		retError(clean, Error_notFound(0, 0, "Archive_addInternal()::entry.path parent couldn't be created"))

	gotoIfError3(clean, ListArchiveEntry_pushBack(&archive->entries, entry, alloc, e_rr));
	alloc = NULL;

clean:

	if(alloc)
		CharString_free(&resolved, alloc);

	return s_uccess;
}

Bool Archive_addDirectory(const ArchiveOptions *archive, Error *e_rr) {

	ArchiveEntry entry = (ArchiveEntry) { .type = EFileType_Folder };

	if (archive && archive->path)
		entry.path = *archive->path;

	return Archive_addInternal(archive, &entry, true, e_rr);
}

Bool Archive_addFile(const ArchiveOptions *archive, Buffer *data, Ns timestamp, Error *e_rr) {

	Bool s_uccess = true;

	if (!data)
		retError(clean, Error_nullPointer(2, "Archive_addFile()::data is required"));

	ArchiveEntry entry = (ArchiveEntry) { .type = EFileType_File, .data = *data, .timestamp = timestamp };

	if (archive && archive->path)
		entry.path = *archive->path;

	gotoIfError3(clean, Archive_addInternal(archive, &entry, false, e_rr));
	*data = Buffer_createNull();		//Moved

clean:
	return s_uccess;
}

//TODO:

static inline Bool Archive_removeInternal(const ArchiveOptions *archive, EFileType type, Error *e_rr) {

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

			const ArchiveEntry caj = archive->entries.ptr[j];

			if(!CharString_startsWithStringInsensitive(caj.path, resolved, 0))
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

Bool Archive_removeFile(const ArchiveOptions *archive, Error *e_rr) {
	return Archive_removeInternal(archive, EFileType_File, e_rr);
}

Bool Archive_removeFolder(const ArchiveOptions *archive, Error *e_rr) {
	return Archive_removeInternal(archive, EFileType_Folder, e_rr);
}

Bool Archive_remove(const ArchiveOptions *archive, Error *e_rr) {
	return Archive_removeInternal(archive, EFileType_Any, e_rr);
}

Bool Archive_rename(const ArchiveOptions *archive, const CharString *newFileName, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolvedLoc = CharString_createNull();
	const Allocator *alloc = NULL;

	if (!newFileName)
		retError(clean, Error_nullPointer(1, "Archive_rename()::newFileName is required"));

	if (!CharString_isValidFileName(*newFileName))
		retError(clean, Error_invalidParameter(1, 0, "Archive_rename()::newFileName isn't a valid filename"));

	U64 i = 0;
	if (!Archive_getPath(archive, NULL, &i, &resolvedLoc, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_rename()::loc couldn't be resolved to path"));

	alloc = archive->alloc;

	//Rename

	CharString *prevPath = &archive->archive->entries.ptrNonConst[i].path;
	CharString subStr = CharString_createNull();

	CharString_cutAfterLastSensitive(*prevPath, '/', &subStr);
	prevPath->lenAndNullTerminated = CharString_length(subStr);

	gotoIfError3(clean, CharString_appendString(prevPath, newFileName, alloc, e_rr));

clean:

	if(alloc)
		CharString_free(&resolvedLoc, alloc);

	return s_uccess;
}

Bool Archive_move(const ArchiveOptions *archive, const CharString *directoryName, Error *e_rr) {

	Bool s_uccess = true;
	CharString resolved = CharString_createNull();
	U64 i = 0;
	ArchiveEntry parent = (ArchiveEntry) { 0 };
	const Allocator *alloc = NULL;

	if (!Archive_getPath((const ArchiveOptionsConst*)archive, NULL, &i, NULL, e_rr))
		retError(clean, Error_notFound(0, 1, "Archive_move()::loc couldn't be resolved to path"));

	ArchiveOptionsConst directoryNameArchive = (ArchiveOptionsConst) {
		.archive = archive->archive, .path = directoryName, .alloc = archive->alloc
	};

	if (!Archive_getPath(&directoryNameArchive, &parent, NULL, &resolved, e_rr))
		retError(clean, Error_notFound(0, 2, "Archive_move()::directoryName couldn't be resolved to path"));

	alloc = archive->alloc;

	if (parent.type != EFileType_Folder)
		retError(clean, Error_invalidOperation(0, "Archive_move()::directoryName should resolve to folder file"));

	CharString *filePath = &archive->archive->entries.ptrNonConst[i].path;

	const U64 v = CharString_findLastSensitive(*filePath, '/', 0, 0);

	if (v != U64_MAX)
		gotoIfError3(clean, CharString_popFrontCount(filePath, v + 1, e_rr))

	if (CharString_length(*directoryName)) {
		gotoIfError3(clean, CharString_insert(filePath, '/', 0, alloc, e_rr));
		gotoIfError3(clean, CharString_insertString(filePath, directoryName, 0, alloc, e_rr));
	}

clean:

	if(alloc)
		CharString_free(&resolved, alloc);

	return s_uccess;
}

//TODO:

Bool Archive_getInfo(const ArchiveOptions *archive, FileInfo *info, Error *e_rr) {

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

U64 Archive_getIndex(const ArchiveOptions *archive) {
	U64 v = U64_MAX;
	Archive_getPath(archive, NULL, &v, NULL, NULL);
	return v;
}

//TODO:

Bool Archive_updateFileData(const ArchiveOptions *archive, const Buffer *data, Error *e_rr) {

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

//TODO:

Bool Archive_getFileDataInternal(const ArchiveOptions *archive, Buffer *data, Bool isConst, Error *e_rr) {

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

Bool Archive_getFileData(const ArchiveOptions *archive, Buffer *data, Error *e_rr) {
	return Archive_getFileDataInternal(archive, data, false, e_rr);
}

Bool Archive_getFileDataConst(const ArchiveOptions *archive, Buffer *data, Error *e_rr) {
	return Archive_getFileDataInternal(archive, data, true, e_rr);
}

//TODO:

Bool Archive_foreach(const ArchiveQuery *query, FileCallback callback, void *userData, EFileType type, Error *e_rr) {

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

static Bool countFile(FileInfo info, U64 *res, Error *e_rr) {
	(void)info; (void) e_rr;
	++*res;
	return true;
}

static inline Bool Archive_queryFileObjectCount(const ArchiveQuery *query, EFileType type, U64 *res, Error *e_rr) {

	Bool s_uccess = true;

	if (!res)
		retError(clean, Error_nullPointer(2, "Archive_queryFileObjectCount()::res is required"));

	gotoIfError3(clean, Archive_foreach(query, (FileCallback) countFile, res, type, e_rr));

clean:
	return s_uccess;
}

Bool Archive_queryFileEntryCount(const ArchiveQuery *query, U64 *res, Error *e_rr) {
	return Archive_queryFileObjectCount(query, EFileType_Any, res, e_rr);
}

Bool Archive_queryFileCount(const ArchiveQuery *query, U64 *res, Error *e_rr) {
	return Archive_queryFileObjectCount(query, EFileType_File, res, e_rr);
}

Bool Archive_queryFolderCount(const ArchiveQuery *query, U64 *res, Error *e_rr) {
	return Archive_queryFileObjectCount(query, EFileType_Folder, res, e_rr);
}
