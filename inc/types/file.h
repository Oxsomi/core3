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
#include "types/string.h"

typedef enum EFileType {
	EFileType_Folder,
	EFileType_File,
	EFileType_Invalid
} EFileType;

typedef enum FileAccess {
	FileAccess_None,			//Invalid, never returned
	FileAccess_Read,
	FileAccess_Write,
	FileAccess_ReadWrite
} FileAccess;

typedef struct FileInfo {

	EFileType type;
	FileAccess access;
	Ns timestamp;		//In units that the file system supports. Normally that unit is seconds.
	String path;
	U64 fileSize;

} FileInfo;

typedef Error (*FileCallback)(FileInfo, void*);

typedef struct Allocator Allocator;
typedef struct String String;

Error File_resolve(
	String loc,
	Bool *isVirtual,
	U64 maxFilePathLimit,
	String absoluteDir,
	Allocator alloc,
	String *result
);

Bool File_isVirtual(String loc);

Bool FileInfo_free(FileInfo *info, Allocator alloc);
