#pragma once
#include "types/types.h"

typedef struct Error Error;

Error Buffer_createCopyx(Buffer buf, Buffer *output);
Error Buffer_createZeroBitsx(U64 length, Buffer *output);
Error Buffer_createOneBitsx(U64 length, Buffer *output);
Error Buffer_createBitsx(U64 length, Bool value, Buffer *result);
Bool Buffer_freex(Buffer *buf);
Error Buffer_createEmptyBytesx(U64 length, Buffer *output);
Error Buffer_createUninitializedBytesx(U64 length, Buffer *output);
