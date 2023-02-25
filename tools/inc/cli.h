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
#include "operations.h"

typedef struct StringList StringList;
typedef struct FileInfo FileInfo;

void CLI_showHelp(EOperationCategory category, EOperation op, EFormat f);

Error _CLI_convertToDL(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]);
Error _CLI_convertFromDL(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]);
Error _CLI_convertToCA(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]);
Error _CLI_convertFromCA(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]);

Bool CLI_convertTo(ParsedArgs args);
Bool CLI_convertFrom(ParsedArgs args);

Bool CLI_encryptDo(ParsedArgs args);
Bool CLI_encryptUndo(ParsedArgs args);

Bool CLI_hashFile(ParsedArgs args);
Bool CLI_hashString(ParsedArgs args);

Bool CLI_randKey(ParsedArgs args);
Bool CLI_randChar(ParsedArgs args);
Bool CLI_randData(ParsedArgs args);
Bool CLI_randNum(ParsedArgs args);

Bool CLI_inspectHeader(ParsedArgs args);
Bool CLI_inspectData(ParsedArgs args);

Bool CLI_package(ParsedArgs args);

Bool CLI_execute(StringList arglist);