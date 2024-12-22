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
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;

typedef struct CASettings CASettings;
typedef struct CAFile CAFile;
typedef struct ArchiveEntry ArchiveEntry;

typedef struct DLSettings DLSettings;
typedef struct DLFile DLFile;

typedef struct DDSInfo DDSInfo;

typedef struct ListCharString ListCharString;
typedef struct ListSubResourceData ListSubResourceData;

//oiCA

Bool CAFile_freex(CAFile *caFile);

Bool CAFile_writex(CAFile caFile, Buffer *result, Error *e_rr);
Bool CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile, Error *e_rr);
Bool CAFile_combinex(CAFile a, CAFile b, CAFile *combined, Error *e_rr);

//oiDL

Bool DLFile_createx(DLSettings settings, DLFile *dlFile, Error *e_rr);
Bool DLFile_freex(DLFile *dlFile);

Bool DLFile_createListx(DLSettings settings, ListBuffer *buffers, DLFile *dlFile, Error *e_rr);
Bool DLFile_createUTF8Listx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr);
Bool DLFile_createBufferListx(DLSettings settings, ListBuffer buffers, DLFile *dlFile, Error *e_rr);
Bool DLFile_createAsciiListx(DLSettings settings, ListCharString strings, DLFile *dlFile, Error *e_rr);

Bool DLFile_addEntryx(DLFile *dlFile, Buffer entry, Error *e_rr);
Bool DLFile_addEntryAsciix(DLFile *dlFile, CharString entry, Error *e_rr);
Bool DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry, Error *e_rr);

Bool DLFile_writex(DLFile dlFile, Buffer *result, Error *e_rr);
Bool DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile, Error *e_rr);
Bool DLFile_combinex(DLFile a, DLFile b, DLFile *combined, Error *e_rr);

//DDS

Error DDS_writex(ListSubResourceData buf, DDSInfo info, Buffer *result);
Error DDS_readx(Buffer buf, DDSInfo *info, ListSubResourceData *result);
Bool ListSubResourceData_freeAllx(ListSubResourceData *buf);

#ifdef __cplusplus
	}
#endif
