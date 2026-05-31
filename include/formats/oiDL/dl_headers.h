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

//formats/oiDL/dl_headers.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

//File spec (docs/oiDL.md)

typedef enum EDLVersion {
	EDLVersion_V1_0
} EDLVersion;

typedef U8 DLVersion;        //EDLVersion

typedef enum EDLFlags {

	EDLFlags_None                     = 0,

	EDLFlags_Reserved                = 1 << 0,        //Reserved: [CRC32C,SHA256][v] if compressed and not encrypted

	EDLFlags_IsString                = 1 << 1,        //If true; must be a valid UTF8 string

	//Chunk size of AES for multi threading. 0 = 128KiB, 1 = 1MiB, 2 = 8MiB, 3 = 64MiB

	EDLFlags_UseAESChunksA            = 1 << 2,
	EDLFlags_UseAESChunksB            = 1 << 3,

	EDLFlags_HasExtendedData        = 1 << 4,        //Extended data

	EDLFlags_AESChunkMask            = EDLFlags_UseAESChunksA | EDLFlags_UseAESChunksB,
	EDLFlags_AESChunkShift            = 2

} EDLFlags;

typedef U8 DLFlags;            //EDLFlags

typedef struct DLHeader {
	DLVersion version;        //major.minor (%10 = minor, /10 = major (+1 to get real major))
	DLFlags flags;
	U8 type;                //(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count (see oiXX.md).
	U8 sizeTypes;            //EXXDataSizeTypes: entryType | (uncompressedSizType << 2) | (dataType << 4) (must be < (1 << 6)).
} DLHeader;

static const U32 DLHeader_chunkSizes[] = { 131072, 1048576, 8388608, 67108864 };
static const U8 DLHeader_chunkSizesShifts[] = { 17, 20, 23, 26 };

typedef struct DLExtraInfo {

	//Identifier to ensure the extension is detected.
	//0x0 - 0x1FFFFFFF are version headers, others are extensions.
	U32 extendedMagicNumber;

	U16 extendedHeader;            //If extensions want to add extra data to the header
	U16 perEntryExtendedData;    //What to store per entry besides a DataSizeType

} DLExtraInfo;

#define DLHeader_MAGIC 0x4C44696F

#ifdef __cplusplus
	}
#endif
