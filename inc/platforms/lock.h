#pragma once
#include "types/types.h"

struct Lock {
	void *data;
};

impl struct Error Lock_create(struct Lock *res);
impl struct Error Lock_free(struct Lock *res);

//Even though maxTime is in Ns it may be interpreted 
//As a different unit by the runtime.
//Ex. Windows uses ms so it'll round up to ms

impl Bool Lock_lock(struct Lock l, Ns maxTime);

impl Bool Lock_unlock(struct Lock l);
