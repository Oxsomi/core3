#pragma once
#include "types/types.h"

typedef struct Error Error;
typedef struct String String;
typedef struct Allocator Allocator;

Error File_write(Buffer buf, String loc);
Error File_read(String loc, Allocator alloc, Buffer *output);

//TODO: make it more like a DirectStorage-like api
