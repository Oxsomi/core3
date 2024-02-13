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
#include "types/types.h"

typedef struct Error Error;
typedef struct CharString CharString;

typedef struct CASettings CASettings;
typedef struct CAFile CAFile;
typedef struct ArchiveEntry ArchiveEntry;

typedef struct DLSettings DLSettings;
typedef struct DLFile DLFile;

typedef struct ListCharString ListCharString;

//oiCA

Bool CAFile_freex(CAFile *caFile);

Error CAFile_writex(CAFile caFile, Buffer *result);
Error CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile);

//oiDL

Error DLFile_createx(DLSettings settings, DLFile *dlFile);
Bool DLFile_freex(DLFile *dlFile);

Error DLFile_createListx(DLSettings settings, ListBuffer *buffers, DLFile *dlFile);
Error DLFile_createUTF8Listx(DLSettings settings, ListBuffer buffers, DLFile *dlFile);
Error DLFile_createBufferListx(DLSettings settings, ListBuffer buffers, DLFile *dlFile);
Error DLFile_createAsciiListx(DLSettings settings, ListCharString strings, DLFile *dlFile);

Error DLFile_addEntryx(DLFile *dlFile, Buffer entry);
Error DLFile_addEntryAsciix(DLFile *dlFile, CharString entry);
Error DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry);

Error DLFile_writex(DLFile dlFile, Buffer *result);
Error DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile);
