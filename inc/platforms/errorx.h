#pragma once
#include "types/error.h"

enum LogLevel;
enum LogOptions;

void Error_printx(struct Error err, enum LogLevel logLevel, enum LogOptions options);

//Errors with stacktraces

struct Error Error_traced(enum GenericError err, U32 subId, U32 paramId, U32 paramSubId, U64 paramValue0, U64 paramValue1);