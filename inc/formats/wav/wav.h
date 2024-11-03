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
#include "formats/wav/headers.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct Stream Stream;

//Only used through read/write, don't cast directly.
//To avoid loading the whole WAV, a stream is used to find all these sections.
typedef struct WAVFile {
	RIFFFmtHeader fmt;
	U32 dataLength;
	U64 dataStart;
} WAVFile;

//Error WAV_write(..., Allocator allocator, Buffer *result);		//TODO:
Bool WAV_read(Stream *stream, U64 off, U64 len, Allocator allocator, WAVFile *result, Error *e_rr);

#ifdef __cplusplus
	}
#endif
