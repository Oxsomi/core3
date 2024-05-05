/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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
#include "types/list.h"
#include "oiXX.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum EBCSettingsFlags {
	EBCSettingsFlags_None				= 0,
	EBCSettingsFlags_UseSHA256			= 1 << 0,
	EBCSettingsFlags_Invalid			= 0xFFFFFFFF << 1
} EBCSettingsFlags;

typedef struct BCSettings {
	EXXCompressionType compressionType;
	EXXEncryptionType encryptionType;
	EBCSettingsFlags flags;
	U32 encryptionKey[8];
} BCSettings;

//Check docs/oiBC.md for the file spec

typedef struct BCFile {
	ListU8 fidiA, fidiB;
	ListU16 gida;
	ListU32 leon;
	BCSettings settings;
} BCFile;

Error BCFile_create(BCSettings settings, ListU8 fidiA, ListU8 fidiB, ListU8 gida, ListU8 leon, BCFile *bcFile);
Bool BCFile_free(BCFile *bcFile, Allocator alloc);

//Serialize

Error BCFile_write(BCFile bcFile, Allocator alloc, Buffer *result);
Error BCFile_read(Buffer file, const U32 encryptionKey[8], Allocator alloc, BCFile *bcFile);

//File headers

//File spec (docs/oiCA.md)

typedef enum EBCFlags {

	EBCFlags_None 						= 0,

	//Whether SHA256 (1) or CRC32C (0) is used as hash
	//(Only if compression is on)

	EBCFlags_UseSHA256					= 1 << 0,

	//Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 100MiB, 3 = 500MiB

	EBCFlags_UseAESChunksA				= 1 << 1,
	EBCFlags_UseAESChunksB				= 1 << 2,

	//Indicates EXXDataSizeType. E.g. (EXXDataSizeType)((b0 << 1) | b1)
	//This indicates the type for compression size if available.

	EBCFlags_CompressedSizeType_Shift	= 3,
	EBCFlags_CompressedSizeType_Mask	= 3,

	EBCFlags_CompressedSizeType_MaskShifted	= EBCFlags_CompressedSizeType_Mask << EBCFlags_CompressedSizeType_Shift,

	//If it includes Fidi A,B, Gida or Leon binaries.
	//Must include at least one.

	EBCFlags_FidiA						= 1 << 5,
	EBCFlags_FidiB						= 1 << 6,
	EBCFlags_Gida						= 1 << 7,
	EBCFlags_Leon						= 1 << 8,

	//Helpers

	EBCFlags_AESChunkMask				= EBCFlags_UseAESChunksA | EBCFlags_UseAESChunksB,
	EBCFlags_AESChunkShift				= 1,

	EBCFlags_NonFlagTypes				= EBCFlags_AESChunkMask | EBCFlags_CompressedSizeType_MaskShifted

} EBCFlags;

typedef struct BCHeader {

	U32 magicNumber;				//oiXX format, magicNumber here is oiBC (0x4342696F)

	U16 versionId;					//major.minor (%10 = minor, /10 = major (+1 to get real major)
	U16 flags;						//EBCFlags

	U8 type;						//(compressionType << 4) | encryptionType
	U8 padding[3];

} BCHeader;

#define BCHeader_MAGIC 0x4342696F

#ifdef __cplusplus
	}
#endif
