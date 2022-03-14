#pragma once
#include "types/allocator.h"

void File_write(struct Buffer buf, const c8 *loc);
struct Buffer File_read(const c8 *loc, AllocFunc alloc, void *allocator);