#pragma once
#include "operations.h"

typedef struct StringList StringList;
typedef struct FileInfo FileInfo;

void CLI_showHelp(EOperationCategory category, EOperation op, EFormat f);

Bool _CLI_convertToDL(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]);
//Bool _CLI_convertFromDL(ParsedArgs args);
//Bool _CLI_convertToCA(ParsedArgs args, String input, FileInfo inputInfo, String output, U32 encryptionKey[8]);
//Bool _CLI_convertFromCA(ParsedArgs args);

Bool _CLI_convert(ParsedArgs args, Bool isTo);

Bool CLI_convertTo(ParsedArgs args);
Bool CLI_convertFrom(ParsedArgs args);

Bool CLI_execute(StringList arglist);