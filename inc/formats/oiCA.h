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
