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
#include "types/file.h"

typedef struct Error Error;
typedef struct Archive Archive;
typedef struct String String;

Error Archive_createx(Archive *archive);
Bool Archive_freex(Archive *archive);

Bool Archive_hasFilex(Archive archive, String path);
Bool Archive_hasFolderx(Archive archive, String path);
Bool Archive_hasx(Archive archive, String path);

Error Archive_addDirectoryx(Archive *archive, String path);
Error Archive_addFilex(Archive *archive, String path, Buffer data, Ns timestamp);

Error Archive_updateFileDatax(Archive *archive, String path, Buffer data);

Error Archive_getFileDatax(Archive archive, String path, Buffer *data);
Error Archive_getFileDataConstx(Archive archive, String path, Buffer *data);

Error Archive_removeFilex(Archive *archive, String path);
Error Archive_removeFolderx(Archive *archive, String path);
Error Archive_removex(Archive *archive, String path);

Error Archive_renamex(Archive *archive, String loc, String newFileName);
Error Archive_movex(Archive *archive, String loc, String directoryName);

Error Archive_getInfox(Archive archive, String loc, FileInfo *info);

Error Archive_queryFileObjectCountx(
	Archive archive, 
	String loc, 
	EFileType type, 
	Bool isRecursive, 
	U64 *res
);

Error Archive_queryFileObjectCountAllx(Archive archive, String loc, Bool isRecursive, U64 *res);
Error Archive_queryFileCountx(Archive archive, String loc, Bool isRecursive, U64 *res);
Error Archive_queryFolderCountx(Archive archive, String loc, Bool isRecursive, U64 *res);

Error Archive_foreachx(
	Archive archive,
	String loc,
	FileCallback callback,
	void *userData,
	Bool isRecursive,
	EFileType type
);
