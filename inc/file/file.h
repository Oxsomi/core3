#pragma once
#include "types/types.h"

struct Error File_write(struct Buffer buf, const c8 *loc);
struct Error File_read(const c8 *loc, struct Allocator alloc, struct Buffer *output);

//TODO: make it more like a DirectStorage-like api