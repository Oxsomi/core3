/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
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
#include "operations.h"

typedef struct CharStringList CharStringList;
typedef struct FileInfo FileInfo;

void CLI_showHelp(EOperationCategory category, EOperation op, EFormat f);

Error _CLI_convertToDL(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]);
Error _CLI_convertFromDL(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]);
Error _CLI_convertToCA(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]);
Error _CLI_convertFromCA(ParsedArgs args, CharString input, FileInfo inputInfo, CharString output, U32 encryptionKey[8]);

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

Bool CLI_profileCast(ParsedArgs args);
Bool CLI_profileRNG(ParsedArgs args);
Bool CLI_profileCRC32C(ParsedArgs args);
Bool CLI_profileSHA256(ParsedArgs args);
Bool CLI_profileAES256(ParsedArgs args);

Bool CLI_inspectHeader(ParsedArgs args);
Bool CLI_inspectData(ParsedArgs args);

Bool CLI_package(ParsedArgs args);

Bool CLI_execute(CharStringList arglist);