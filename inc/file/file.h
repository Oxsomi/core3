#pragma once
#include "types/types.h"

struct Error File_write(struct Buffer buf, struct String loc);
struct Error File_read(struct String loc, struct Allocator alloc, struct Buffer *output);

//TODO: make it more like a DirectStorage-like api
