#pragma once
#include "types/error.h"

typedef enum ELogLevel ELogLevel;
typedef enum ELogOptions ELogOptions;
typedef struct String String;

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options);

impl String Error_formatPlatformError(Error err);
