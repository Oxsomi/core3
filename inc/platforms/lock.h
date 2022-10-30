#pragma once
#include "types/types.h"

typedef struct Lock {
	void *data;
	U32 lockThread;
} Lock;

typedef struct Error Error;

impl Error Lock_create(Lock *res);
impl Error Lock_free(Lock *res);

//Even though maxTime is in Ns it may be interpreted 
//As a different unit by the runtime.
//Ex. Windows uses ms so it'll round up to ms

impl Bool Lock_lock(Lock *l, Ns maxTime);

impl Bool Lock_unlock(Lock *l);

inline Bool Lock_isLocked(Lock l) {
	return l.lockThread;
}

impl Bool Lock_isLockedForThread(Lock l);		//If it's locked for the current thread
