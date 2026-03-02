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

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct DLFile DLFile;
typedef struct DLSettings DLSettings;
typedef struct Allocator Allocator;
typedef struct Error Error;
typedef struct ListBuffer ListBuffer;
typedef struct ListCharString ListCharString;
typedef struct ListDLEntryStream ListDLEntryStream;

//Turn raw buffer list / char string list / stream list into a DLFile.

Bool DLFile_createBufferList(
	const DLSettings *settings,
	ListBuffer *buffers,			//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_createStringList(
	const DLSettings *settings,
	ListCharString *strings,		//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

Bool DLFile_createStreamList(
	const DLSettings *settings,
	ListDLEntryStream *streams,		//Moves ListBuffer to DLFile, clears ListBuffer after.
	const Allocator *alloc,
	DLFile *dlFile,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
