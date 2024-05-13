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
#include "oiXX.h"
#include "types/archive.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum ECASettingsFlags {
	ECASettingsFlags_None				= 0,
	ECASettingsFlags_IncludeDate		= 1 << 0,			//--date
	ECASettingsFlags_IncludeFullDate	= 1 << 1,			//--full-date (automatically sets --date)
	ECASettingsFlags_UseSHA256			= 1 << 2,			//--sha256
	ECASettingsFlags_Invalid			= 0xFFFFFFFF << 3
} ECASettingsFlags;

typedef struct CASettings {
	EXXCompressionType compressionType;
	EXXEncryptionType encryptionType;
	ECASettingsFlags flags;
	U32 encryptionKey[8];
} CASettings;

//Check docs/oiCA.md for the file spec

typedef struct CAFile {
	Archive archive;
	CASettings settings;
} CAFile;

Error CAFile_create(CASettings settings, Archive archive, CAFile *caFile);
Bool CAFile_free(CAFile *caFile, Allocator alloc);

//Serialize

Error CAFile_write(CAFile caFile, Allocator alloc, Buffer *result);
Error CAFile_read(Buffer file, const U32 encryptionKey[8], Allocator alloc, CAFile *caFile);

//File headers

//File spec (docs/oiCA.md)

typedef enum ECAFlags {

	ECAFlags_None 						= 0,

   	//Whether SHA256 (1) or CRC32C (0) is used as hash
    //(Only if compression is on)

	ECAFlags_UseSHA256					= 1 << 0,

	//See ECAFileObject

	ECAFlags_FilesHaveDate				= 1 << 1,
	ECAFlags_FilesHaveExtendedDate		= 1 << 2,

    //Indicates EXXDataSizeType. E.g. (EXXDataSizeType)((b0 << 1) | b1)
    //This indicates the type the biggest file size uses

	ECAFlags_FileSizeType_Shift			= 3,
	ECAFlags_FileSizeType_Mask			= 3,

	ECAFlags_FileSizeType_MaskShifted	= ECAFlags_FileSizeType_Mask << ECAFlags_FileSizeType_Shift,

    //Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 100MiB, 3 = 500MiB

    ECAFlags_UseAESChunksA				= 1 << 5,
    ECAFlags_UseAESChunksB				= 1 << 6,

    ECAFlags_HasExtendedData			= 1 << 7,		//CAExtraInfo

    //Indicates EXXDataSizeType. E.g. (EXXDataSizeType)((b0 << 1) | b1)
    //This indicates the type the type for compression type if available.

	ECAFlags_CompressedSizeType_Shift		= 8,
	ECAFlags_CompressedSizeType_Mask		= 3,

	ECAFlags_CompressedSizeType_MaskShifted	= ECAFlags_CompressedSizeType_Mask << ECAFlags_CompressedSizeType_Shift,

	//Determines how many bytes the counter for files takes up.
	//If DirectoriesCountLong is set, it will allow up to 254 dirs, otherwise 64Ki-1.
	//If FilesCountLong is set, it will allow up to 64Ki, otherwise 4Gi.

	ECAFlags_DirectoriesCountLong		= 1 << 10,
	ECAFlags_FilesCountLong				= 1 << 11,

	//Helpers

	ECAFlags_AESChunkMask				= ECAFlags_UseAESChunksA | ECAFlags_UseAESChunksB,
	ECAFlags_AESChunkShift				= 5,

	ECAFlags_NonFlagTypes				=
		ECAFlags_FileSizeType_MaskShifted | ECAFlags_AESChunkMask | ECAFlags_CompressedSizeType_MaskShifted

} ECAFlags;

typedef struct CAHeader {

	U32 magicNumber;			//oiCA (0x4143696F)

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major)
	U8 type;					//(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count (see oiXX.md).
	U16 flags;					//ECAFlags

} CAHeader;

typedef struct CAExtraInfo {

	//Identifier to ensure the extension is detected.
	//0x0 - 0x1FFFFFFF are version headers, others are extensions.

	U32 extendedMagicNumber;

	U16 headerExtensionSize;		//To skip extended data size.

	U8 directoryExtensionSize;		//To skip directory extended data.
	U8 fileExtensionSize;			//To skip file extended data

} CAExtraInfo;

//A directory points to the parent.
//Important is to verify if there are no wrong indices.
//Small is used if there are <254 folders, Large is used when there are more.

typedef U16 CADirectoryLarge;	//0xFFFF for root directory, else id of parent directory (can't >=self)
typedef U8 CADirectorySmall;	//0xFF for root directory, else id of parent directory (can't >=self)

//CADirectory is referred to as the type that fits folders (<=254 = Small else Large).

#define CAHeader_MAGIC 0x4143696F

#ifdef __cplusplus
	}
#endif
