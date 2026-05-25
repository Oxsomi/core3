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

//formats/oiCA/ca_combine.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CAFile CAFile;
typedef struct Allocator Allocator;
typedef struct Error Error;

typedef enum EArchiveCombineMode {
	EArchiveCombineMode_RequireSame,							//Files are only allowed to merge if same contents
	EArchiveCombineMode_Rename,									//Try to rename the file on conflict
	EArchiveCombineMode_AcceptA,								//First archive is leading on conflict
	EArchiveCombineMode_AcceptB,								//Second archive is leading on conflict
	EArchiveCombineMode_Count
} EArchiveCombineMode;

typedef enum EArchiveCombineFlags {
	EArchiveCombineFlags_None = 0,
	EArchiveCombineFlags_ResolveLatestTimestamp = 1 << 0,	//Resolve timestamp with latest, as long as data matches
	EArchiveCombineFlags_ResolveAcceptLatest = 1 << 1		//Override file with latest file contents, otherwise conflict
} EArchiveCombineFlags;

Bool CAFile_combine(
	const CAFile *a,
	const CAFile *b,
	EArchiveCombineMode combineMode,
	EArchiveCombineFlags combineFlags,
	const Allocator *alloc,
	CAFile *combined,
	Error *e_rr
);

#ifdef __cplusplus
	}
#endif
