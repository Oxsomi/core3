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

//File spec (docs/oiCA.md)

typedef enum ECAFlags {
	
	ECAFlags_None 						= 0,

	//See ECAFileObject

	ECAFlags_FilesHaveDate				= 1 << 0,
	ECAFlags_FilesHaveExtendedDate		= 1 << 1,

	//Indicates EXXDataSizeType. E.g. (EXXDataSizeType)((b0 << 1) | b1)
	//This indicates the type the biggest file size uses

	ECAFlags_FileSizeType_Shift			= 2,
	ECAFlags_FileSizeType_Mask			= 3,

	ECAFlags_HasExtendedData			= 1 << 4,		//CAExtraData

	//Determines how many bytes the counter for files takes up.
	//If DirectoriesCountLong is set, it will allow up to 254 dirs, otherwise 64Ki-1.
	//If FilesCountLong is set, it will allow up to 64Ki, otherwise 4Gi.

	ECAFlags_DirectoriesCountLong		= 1 << 5,
	ECAFlags_FilesCountLong				= 1 << 6

} ECAFlags;

typedef enum ECAVersion {
	ECAVersion_V1_0				//Current version
} ECAVersion;

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

typedef U16 CADirectoryIdLarge;	//0xFFFF for root directory, else id of parent directory (can't >=self)
typedef U8 CADirectoryIdSmall;	//0xFF for root directory, else id of parent directory (can't >=self)

typedef U32 CAFileIdLarge;		//Points to a file (not a directory) up to 0xFFFFFFFF
typedef U16 CAFileIdSmall;		//Points to a file (not a directory) up to 0xFFFF

#define CAHeader_MAGIC 0x4143696F

#ifdef __cplusplus
	}
#endif
