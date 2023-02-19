/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
*/

#pragma once
#include "oiXX.h"
#include "types/archive.h"

typedef enum EFileType EFileType;

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

    //Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 100MiB, 3 = 500MiB
        
    ECAFlags_UseAESChunksA				= 1 << 5,
    ECAFlags_UseAESChunksB				= 1 << 6,
    
    ECAFlags_HasExtendedData			= 1 << 7,		//CAExtraInfo
    
    //Indicates EXXDataSizeType. E.g. (EXXDataSizeType)((b0 << 1) | b1)
    //This indicates the type the type for compression type if available.
    
	ECAFlags_CompressedSizeType_Shift		= 8,
	ECAFlags_CompressedSizeType_Mask		= 3,

	//Determines how many bytes the counter for files takes up.
	//If DirectoriesCountLong is set, it will allow up to 254 dirs, otherwise 64Ki-1.
	//If FilesCountLong is set, it will allow up to 64Ki, otherwise 4Gi.

	ECAFlags_DirectoriesCountLong		= 1 << 10,
	ECAFlags_FilesCountLong				= 1 << 11,

	//Helpers

	ECSFlags_AESChunkMask				= ECAFlags_UseAESChunksA | ECAFlags_UseAESChunksB,
	ECSFlags_AESChunkShift				= 5

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
