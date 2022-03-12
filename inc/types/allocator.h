#pragma once
#include "types.h"

typedef void *(AllocFunc)(void* allocator, usz);
typedef void *(FreeFunc)(void* allocator, struct Buffer);