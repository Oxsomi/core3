/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/file.h"

typedef struct Error Error;
typedef struct Archive Archive;
typedef struct String String;

Error Archive_createx(Archive *archive);
Bool Archive_freex(Archive *archive);

Bool Archive_hasFilex(Archive archive, String path);
Bool Archive_hasFolderx(Archive archive, String path);
Bool Archive_hasx(Archive archive, String path);

Error Archive_addDirectoryx(Archive *archive, String path);
Error Archive_addFilex(Archive *archive, String path, Buffer data, Ns timestamp);

Error Archive_updateFileDatax(Archive *archive, String path, Buffer data);

Error Archive_getFileDatax(Archive archive, String path, Buffer *data);
Error Archive_getFileDataConstx(Archive archive, String path, Buffer *data);

Error Archive_removeFilex(Archive *archive, String path);
Error Archive_removeFolderx(Archive *archive, String path);
Error Archive_removex(Archive *archive, String path);

U64 Archive_getIndexx(Archive archive, String loc);

Error Archive_renamex(Archive *archive, String loc, String newFileName);
Error Archive_movex(Archive *archive, String loc, String directoryName);

Error Archive_getInfox(Archive archive, String loc, FileInfo *info);

Error Archive_queryFileObjectCountx(
	Archive archive, 
	String loc, 
	EFileType type, 
	Bool isRecursive, 
	U64 *res
);

Error Archive_queryFileObjectCountAllx(Archive archive, String loc, Bool isRecursive, U64 *res);
Error Archive_queryFileCountx(Archive archive, String loc, Bool isRecursive, U64 *res);
Error Archive_queryFolderCountx(Archive archive, String loc, Bool isRecursive, U64 *res);

Error Archive_foreachx(
	Archive archive,
	String loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type
);
