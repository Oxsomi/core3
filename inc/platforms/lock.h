#pragma once
#include "types/types.h"

struct Lock {
	void* data;
};

impl bool Lock_lock();
impl bool Lock_unlock();
