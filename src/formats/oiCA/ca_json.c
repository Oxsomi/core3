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

//formats/oiCA/ca_json.c

#include "formats/oiCA/ca_file.h"
#include "formats/oiCA/ca_lookup.h"
#include "formats/oiCA/ca_props.h"
#include "formats/json/json_writer.h"
#include "types/container/string.h"
#include "types/container/file_base.h"

//The archive's table, which is what an archive is asked about: what is in it, how big, and when.
//A file's bytes travel only when they are asked for and only when the reader holds them, as hex; one it left
//as a stream is one it decided not to hold, so `file data -entry <path>` is what reads that.

typedef struct CAJsonWalk {
	JsonWriter *w;
	const CAFile *file;
	Error *e_rr;
	Bool withContents;
	Bool failed;
} CAJsonWalk;

static Bool CAFile_jsonEntry(const FileInfo *info, void *userData, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	CAJsonWalk *walk = (CAJsonWalk*) userData;

	(void) alloc;

	const Bool isFolder = info->type == EFileType_Folder;
	const CAHandle handle = CAFile_resolve(walk->file, info->path);

	gotoIfError3(clean, (
		JsonWriter_beginObject(walk->w, e_rr) &&
		JsonWriter_keyStr(walk->w, "path", info->path, e_rr) &&
		JsonWriter_keyCstr(walk->w, "type", isFolder ? "folder" : "file", e_rr)
	));

	//A folder has no size, no contents to be loaded and no timestamp of its own

	if (isFolder) {
		gotoIfError3(clean, (
			JsonWriter_keyNull(walk->w, "size", e_rr) &&
			JsonWriter_keyNull(walk->w, "loaded", e_rr) &&
			JsonWriter_keyNull(walk->w, "timestamp", e_rr) &&
			JsonWriter_keyNull(walk->w, "contents", e_rr)
		));
	}

	else {

		const Bool loaded = CAFile_isLoaded(walk->file, handle);

		gotoIfError3(clean, (
			JsonWriter_keyU64(walk->w, "size", info->fileSize, e_rr) &&
			JsonWriter_keyBool(walk->w, "loaded", loaded, e_rr) &&
			JsonWriter_key(walk->w, "timestamp", e_rr)
		));

		//The per file timestamp is optional, and zero is how the format spells its absence

		if(info->timestamp) {
			gotoIfError3(clean, JsonWriter_u64(walk->w, info->timestamp, e_rr));
		}

		else gotoIfError3(clean, JsonWriter_null(walk->w, e_rr));

		gotoIfError3(clean, JsonWriter_key(walk->w, "contents", e_rr));

		if (walk->withContents && loaded) {

			Bool isValid = false;
			const Buffer data = CAFile_getDataConst(walk->file, handle, &isValid);

			if(!isValid)
				retError(clean, Error_invalidState(0, "CAFile_jsonEntry() a held file wouldn't hand over its data"));

			gotoIfError3(clean, JsonWriter_hex(walk->w, data, e_rr));
		}

		else gotoIfError3(clean, JsonWriter_null(walk->w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endObject(walk->w, e_rr));

clean:

	if(!s_uccess)
		walk->failed = true;

	return s_uccess;
}

Bool CAFile_writeJsonMembers(const CAFile *file, Bool withContents, JsonWriter *w, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!file || !w)
		retError(clean, Error_nullPointer(!file ? 0 : 1, "CAFile_writeJsonMembers()::file and w are required"));

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "header", e_rr) &&
		JsonWriter_keyCstr(w, "encryption", EXXEncryptionType_name(file->settings.encryptionType), e_rr) &&
		JsonWriter_keyCstr(w, "compression", EXXCompressionType_name(file->settings.compressionType), e_rr) &&
		JsonWriter_keyBool(w, "hasDate", (file->settings.flags & ECASettingsFlags_IncludeDate) != 0, e_rr) &&
		JsonWriter_keyBool(w, "hasFullDate", (file->settings.flags & ECASettingsFlags_IncludeFullDate) != 0, e_rr) &&
		JsonWriter_keyObject(w, "counts", e_rr) &&
		JsonWriter_keyU64(w, "files", CAFile_fileCount(file, CAHandle_Root, true), e_rr) &&
		JsonWriter_keyU64(w, "folders", CAFile_dirCount(file, CAHandle_Root, true), e_rr) &&
		JsonWriter_endObject(w, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	//The whole tree flattened, each entry naming its full path, which is how a reader addresses one anyway

	gotoIfError3(clean, JsonWriter_keyArray(w, "entries", e_rr));

	CAJsonWalk walk = (CAJsonWalk) { .w = w, .file = file, .e_rr = e_rr, .withContents = withContents };

	gotoIfError3(clean, CAFile_foreach(file, CAHandle_Root, CAFile_jsonEntry, &walk, true, alloc, e_rr));

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	return s_uccess;
}

Bool CAFile_writeJson(const CAFile *file, Bool withContents, JsonWriter *w, const Allocator *alloc, Error *e_rr) {
	return
		JsonWriter_beginObject(w, e_rr) &&
		CAFile_writeJsonMembers(file, withContents, w, alloc, e_rr) &&
		JsonWriter_endObject(w, e_rr);
}
