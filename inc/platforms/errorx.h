#pragma once
#include "types/error.h"

typedef enum LogLevel LogLevel;
typedef enum LogOptions LogOptions;

void Error_printx(Error err, LogLevel logLevel, LogOptions options);

//Errors with stacktraces

Error Error_traced(GenericError err, U32 subId, U32 paramId, U32 paramSubId, U64 paramValue0, U64 paramValue1);