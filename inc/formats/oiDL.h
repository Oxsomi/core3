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
