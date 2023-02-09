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
#include "types/types.h"

typedef struct Error Error;
typedef struct String String;

typedef struct CASettings CASettings;
typedef struct CAFile CAFile;
typedef struct ArchiveEntry ArchiveEntry;

typedef struct DLSettings DLSettings;
typedef struct DLFile DLFile;

//oiCA

Bool CAFile_freex(CAFile *caFile);
 
Error CAFile_writex(CAFile caFile, Buffer *result);
Error CAFile_readx(Buffer file, const U32 encryptionKey[8], CAFile *caFile);

//oiDL

Error DLFile_createx(DLSettings settings, DLFile *dlFile);
Bool DLFile_freex(DLFile *dlFile);

Error DLFile_addEntryx(DLFile *dlFile, Buffer entry);
Error DLFile_addEntryAsciix(DLFile *dlFile, String entry);
Error DLFile_addEntryUTF8x(DLFile *dlFile, Buffer entry);

Error DLFile_writex(DLFile dlFile, Buffer *result);
Error DLFile_readx(Buffer file, const U32 encryptionKey[8], Bool allowLeftOverData, DLFile *dlFile);
