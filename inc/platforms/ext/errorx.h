#pragma once
#include "types/types.h"

typedef enum ELogLevel ELogLevel;
typedef enum ELogOptions ELogOptions;
typedef struct String String;
typedef struct Error Error;

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options);

impl String Error_formatPlatformError(Error err);
