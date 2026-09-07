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

//formats/oiDL/dl_json.c

#include "formats/oiDL/dl_file.h"
#include "formats/json/json_writer.h"
#include "types/container/string.h"

//An entry the reader left as a stream is one it decided not to hold in memory, so a document never asks for it:
//the entry says what it is and how long, and `contents` says why it isn't there. That is the same rule
//DLFile_isFullyLoaded answers everywhere else, rather than a size this file picks for itself.

Bool DLFile_writeJsonMembers(const DLFile *file, Bool withContents, JsonWriter *w, Error *e_rr) {

	Bool s_uccess = true;

	if(!file || !w)
		retError(clean, Error_nullPointer(!file ? 0 : 2, "DLFile_writeJsonMembers()::file and w are required"));

	const Bool isString = file->settings.dataType == EDLDataType_String;
	const U64 count = DLFile_entryCount(file);

	gotoIfError3(clean, (
		JsonWriter_keyObject(w, "header", e_rr) &&
		JsonWriter_keyCstr(w, "type", EDLDataType_name(file->settings.dataType), e_rr) &&
		JsonWriter_keyCstr(
			w, "encryption", EXXEncryptionType_name((EXXEncryptionType) file->settings.encryptionType), e_rr
		) &&
		JsonWriter_keyCstr(
			w, "compression", EXXCompressionType_name((EXXCompressionType) file->settings.compressionType), e_rr
		) &&
		JsonWriter_keyBool(
			w, "hidesMagicNumber", (file->settings.flags & EDLSettingsFlags_HideMagicNumber) != 0, e_rr
		) &&
		JsonWriter_keyU64(w, "entries", count, e_rr) &&
		JsonWriter_endObject(w, e_rr)
	));

	gotoIfError3(clean, JsonWriter_keyArray(w, "entries", e_rr));

	for (U64 i = 0; i < count; ++i) {

		const Bool loaded = DLFile_isFullyLoaded(file, i);

		gotoIfError3(clean, (
			JsonWriter_beginObject(w, e_rr) &&
			JsonWriter_keyU64(w, "id", i, e_rr) &&
			JsonWriter_keyU64(w, "size", DLFile_entrySize(file, i), e_rr) &&
			JsonWriter_keyBool(w, "loaded", loaded, e_rr) &&
			JsonWriter_key(w, "contents", e_rr)
		));

		//A held entry carries itself: text as text, bytes as hex. A stream-backed one never does, since that
		// is one the reader decided not to hold, and `file data -entry` is what reads it.

		if(withContents && loaded) {

			if(isString) {
				gotoIfError3(clean, JsonWriter_str(w, file->entryStrings.ptr[i], e_rr));
			}

			else gotoIfError3(clean, JsonWriter_hex(w, file->entryBuffers.ptr[i], e_rr));
		}

		else gotoIfError3(clean, JsonWriter_null(w, e_rr));

		gotoIfError3(clean, JsonWriter_endObject(w, e_rr));
	}

	gotoIfError3(clean, JsonWriter_endArray(w, e_rr));

clean:
	return s_uccess;
}

Bool DLFile_writeJson(const DLFile *file, Bool withContents, JsonWriter *w, Error *e_rr) {
	return
		JsonWriter_beginObject(w, e_rr) &&
		DLFile_writeJsonMembers(file, withContents, w, e_rr) &&
		JsonWriter_endObject(w, e_rr);
}
