#pragma once
#include "types/ref_ptr.h"

RefPtr RefPtr_createx(void *ptr, ObjectFreeFunc free);
