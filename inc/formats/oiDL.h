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
#include "types/list.h"

typedef enum EDLDataType {
	EDLDataType_Data,									//(default)
	EDLDataType_Ascii,									//--ascii
	EDLDataType_UTF8,									//--utf8
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

typedef union DLEntry {

	Buffer entryBuffer;
	String entryString;

} DLEntry;

//Check docs/oiDL.md for the file spec

typedef struct DLFile {

	List entries;				//<DLEntry>
	DLSettings settings;
	U64 readLength;				//How many bytes were read for this file

} DLFile;

Error DLFile_create(DLSettings settings, Allocator alloc, DLFile *dlFile);
Bool DLFile_free(DLFile *dlFile, Allocator alloc);

//DLEntry will belong to DLFile.
//This means that freeing it will free the String + Buffer if they're not a ref
//So be sure to make them a ref if needed.

Error DLFile_addEntry(DLFile *dlFile, Buffer entry, Allocator alloc);
Error DLFile_addEntryAscii(DLFile *dlFile, String entry, Allocator alloc);
Error DLFile_addEntryUTF8(DLFile *dlFile, Buffer entry, Allocator alloc);

Error DLFile_write(DLFile dlFile, Allocator alloc, Buffer *result);

Error DLFile_read(
	Buffer file, 
	const U32 encryptionKey[8],
	Bool isSubfile,					//Sets HideMagicNumber flag and allows leftover data after the oiDL
	Allocator alloc, 
	DLFile *dlFile
);

//File headers

//File spec (docs/oiDL.md)

typedef enum EDLFlags {

	EDLFlags_None 					= 0,

	EDLFlags_UseSHA256				= 1 << 0,		//Whether SHA256 (1) or CRC32C (0) is used as hash

	EDLFlags_IsString				= 1 << 1,		//If true; string must contain valid ASCII characters
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
