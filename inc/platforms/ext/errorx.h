#pragma once
#include "types/error.h"

typedef enum ELogLevel ELogLevel;
typedef enum ELogOptions ELogOptions;

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options);
