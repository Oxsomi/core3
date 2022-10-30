#pragma once
#include "types/error.h"

typedef enum ELogLevel ELogLevel;
typedef enum ELogOptions ELogOptions;

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options);

//Errors with stacktraces

Error Error_traced(EGenericError err, U32 subId, U32 paramId, U32 paramSubId, U64 paramValue0, U64 paramValue1);