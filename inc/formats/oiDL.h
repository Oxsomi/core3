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
#include "oiXX.h"
#include "types/list.h"

typedef enum EDLDataType {
	EDLDataType_Data,									//(default, Buffer)
	EDLDataType_Ascii,									//--ascii (CharString)
	EDLDataType_UTF8,									//--utf8 (Buffer)
	EDLDataType_Count
} EDLDataType;

typedef enum EDLSettingsFlags {
	EDLSettingsFlags_None				= 0,
	EDLSettingsFlags_UseSHA256			= 1 << 0,		//--sha256
	EDLSettingsFlags_HideMagicNumber	= 1 << 1,		//Only valid if the oiDL can be 100% confidently detected otherwise
	EDLSettingsFlags_Invalid			= 0xFFFFFFFF << 2
} EDLSettingsFlags;

typedef struct DLSettings {

	EXXCompressionType compressionType;
	EXXEncryptionType encryptionType;
	EDLDataType dataType;
	EDLSettingsFlags flags;

	U32 encryptionKey[8];

} DLSettings;

//Check docs/oiDL.md for the file spec

typedef struct DLFile {

	union {
		ListBuffer entryBuffers;
		ListCharString entryStrings;
	};

	DLSettings settings;
	U64 readLength;				//How many bytes were read for this file

} DLFile;

U64 DLFile_entryCount(DLFile file);
Bool DLFile_isAllocated(DLFile file);

Error DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile);
Bool DLFile_free(DLFile *dlFile, Allocator alloc);

Error DLFile_createBufferList(DLSettings settings, ListBuffer buffers, Allocator alloc, DLFile *dlFile);
Error DLFile_createAsciiList(DLSettings settings, ListCharString strings, Allocator alloc, DLFile *dlFile);
Error DLFile_createUtf8List(DLSettings settings, ListBuffer strings, Allocator alloc, DLFile *dlFile);

//Determine what type of list is made with settings.dataType

Error DLFile_createList(DLSettings settings, ListBuffer *buffers, Allocator alloc, DLFile *dlFile);

//DLEntry will belong to DLFile.
//This means that freeing it will free the CharString + Buffer if they're not a ref
//So be sure to make them a ref if needed.

Error DLFile_addEntry(DLFile *dlFile, Buffer entry, Allocator alloc);
Error DLFile_addEntryAscii(DLFile *dlFile, CharString entry, Allocator alloc);
Error DLFile_addEntryUTF8(DLFile *dlFile, Buffer entry, Allocator alloc);

Error DLFile_write(DLFile dlFile, Allocator alloc, Buffer *result);

Error DLFile_read(
	Buffer file,
	const U32 encryptionKey[8],
	Bool isSubFile,					//Sets HideMagicNumber flag and allows leftover data after the oiDL
	Allocator alloc,
	DLFile *dlFile
);

//File headers

//File spec (docs/oiDL.md)

typedef enum EDLFlags {

	EDLFlags_None 					= 0,

	EDLFlags_UseSHA256				= 1 << 0,		//Whether SHA256 (1) or CRC32C (0) is used as hash

	EDLFlags_IsString				= 1 << 1,		//If true; must be a valid string (!UTF8 ? Ascii : UTF8)
	EDLFlags_UTF8					= 1 << 2,		//ASCII (if off), otherwise UTF-8

    //Chunk size of AES for multi threading. 0 = none, 1 = 10MiB, 2 = 50MiB, 3 = 100MiB

	EDLFlags_UseAESChunksA			= 1 << 3,
	EDLFlags_UseAESChunksB			= 1 << 4,

	EDLFlags_HasExtendedData		= 1 << 5,		//Extended data

	EDLFlags_AESChunkMask			= EDLFlags_UseAESChunksA | EDLFlags_UseAESChunksA,
	EDLFlags_AESChunkShift			= 3

} EDLFlags;

typedef struct DLHeader {

	U8 version;					//major.minor (%10 = minor, /10 = major (+1 to get real major))
	U8 flags;					//EDLFlags
	U8 type;					//(EXXCompressionType << 4) | EXXEncryptionType. Each enum should be <Count (see oiXX.md).
	U8 sizeTypes;				//EXXDataSizeTypes: entryType | (uncompressedSizType << 2) | (dataType << 4) (must be < (1 << 6)).

} DLHeader;

typedef struct DLExtraInfo {

	//Identifier to ensure the extension is detected.
	//0x0 - 0x1FFFFFFF are version headers, others are extensions.
	U32 extendedMagicNumber;

	U16 extendedHeader;			//If extensions want to add extra data to the header
	U16 perEntryExtendedData;	//What to store per entry besides a DataSizeType

} DLExtraInfo;

#define DLHeader_MAGIC 0x4C44696F
