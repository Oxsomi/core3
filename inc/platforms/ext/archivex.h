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

#pragma once
#include "types/container/file.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Archive Archive;
typedef struct ArchiveCombineSettings ArchiveCombineSettings;

Bool Archive_createx(Archive *archive, Error *e_rr);
Bool Archive_freex(Archive *archive);

Bool Archive_createCopyx(Archive a, Archive *archive, Error *e_rr);
Bool Archive_combinex(Archive a, Archive b, ArchiveCombineSettings combineSettings, Archive *archive, Error *e_rr);

Bool Archive_hasFilex(Archive archive, CharString path);
Bool Archive_hasFolderx(Archive archive, CharString path);
Bool Archive_hasx(Archive archive, CharString path);

Bool Archive_addDirectoryx(Archive *archive, CharString path, Error *e_rr);
Bool Archive_addFilex(Archive *archive, CharString path, Buffer *data, Ns timestamp, Error *e_rr);

Bool Archive_updateFileDatax(Archive *archive, CharString path, Buffer data, Error *e_rr);

Bool Archive_getFileDatax(Archive archive, CharString path, Buffer *data, Error *e_rr);
Bool Archive_getFileDataConstx(Archive archive, CharString path, Buffer *data, Error *e_rr);

Bool Archive_removeFilex(Archive *archive, CharString path, Error *e_rr);
Bool Archive_removeFolderx(Archive *archive, CharString path, Error *e_rr);
Bool Archive_removex(Archive *archive, CharString path, Error *e_rr);

U64 Archive_getIndexx(Archive archive, CharString loc);

Bool Archive_renamex(Archive *archive, CharString loc, CharString newFileName, Error *e_rr);
Bool Archive_movex(Archive *archive, CharString loc, CharString directoryName, Error *e_rr);

Bool Archive_getInfox(Archive archive, CharString loc, FileInfo *info, Error *e_rr);

Bool Archive_queryFileEntryCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr);
Bool Archive_queryFileCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr);
Bool Archive_queryFolderCountx(Archive archive, CharString loc, Bool isRecursive, U64 *res, Error *e_rr);

Bool Archive_foreachx(
	Archive archive,
	CharString loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
